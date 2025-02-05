// Copyright Rancorous Games, 2024

#include "Components\InventoryComponent.h"

#include <ObjectArray.h>
#include <variant>
#include <GameFramework/Actor.h>
#include <Engine/AssetManager.h>

#include "LogRancInventorySystem.h"
#include "Data/RecipeData.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Data/UsableItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"


UInventoryComponent::UInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Subscribe to base class inventory events
	OnItemAddedToContainer.AddDynamic(this, &UInventoryComponent::OnInventoryItemAddedHandler);
	OnItemRemovedFromContainer.AddDynamic(this, &UInventoryComponent::OnInventoryItemRemovedHandler);

	Subsystem = URISSubsystem::Get(this);
	
	// Initialize available recipes based on initial inventory and recipes
	CheckAndUpdateRecipeAvailability();
}

int32 UInventoryComponent::GetItemQuantityTotal(const FGameplayTag& ItemId) const
{
	return Super::GetContainerOnlyItemQuantityImpl(ItemId);
}

void UInventoryComponent::UpdateWeightAndSlots()
{
	// First update weight and slots as if all items were in the generic slots
	Super::UpdateWeightAndSlots();

	// then subtract the slots of the tagged items
	for (const FTaggedItemBundle& TaggedInstance : TaggedSlotItemInstances)
	{
		if (const UItemStaticData* const ItemData = URISSubsystem::GetItemDataById(
			TaggedInstance.ItemId))
		{
			int32 SlotsTakenPerStack = 1;
			if (JigsawMode)
			{
				if (JigsawMode)
				{
					SlotsTakenPerStack = ItemData->JigsawSizeX * ItemData->JigsawSizeY;
				}
			}

			UsedContainerSlotCount -= FMath::CeilToInt(TaggedInstance.Quantity / static_cast<float>(ItemData->MaxStackSize)) * SlotsTakenPerStack;
		}
	}

	ensureMsgf(UsedContainerSlotCount <= MaxContainerSlotCount, TEXT("Used slot count is higher than max slot count!"));
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, TaggedSlotItemInstances, SharedParams);

	DOREPLIFETIME(UInventoryComponent, AllUnlockedRecipes);
}

int32 UInventoryComponent::ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool SuppressUpdate)
{
	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(ItemId, Quantity, Reason, StateArrayToAppendTo, false);

	// See DestroyItemsImpl for equivalent implementation and explanation
	const int32 QuantityUnderflow = GetContainerOnlyItemQuantity(ItemId);
	if (QuantityUnderflow < 0)
	{
		RemoveItemFromAnyTaggedSlots_IfServer(ItemId, -QuantityUnderflow, Reason, false);
	}

	return ExtractedFromContainer;
}

int32 UInventoryComponent::ExtractItemFromTaggedSlot_IfServer(const FGameplayTag& TaggedSlot, const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	if (!ContainsImpl(ItemId, Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item not found in container"));
		return 0;
	}
	
	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(ItemId, Quantity, Reason, StateArrayToAppendTo, true);
	
	RemoveQuantityFromTaggedSlot_IfServer(TaggedSlot, ExtractedFromContainer, Reason, false, false);

	return ExtractedFromContainer;
}


////////////////////////////////////////////////////// TAGGED SLOTS ///////////////////////////////////////////////////////
int32 UInventoryComponent::AddItemToTaggedSlot_IfServer(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& SlotTag, const FGameplayTag& ItemId,
                                                            int32 RequestedQuantity, bool AllowPartial)
{
	// Reminder: Items in tagged slots are duplicated in container

	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddItemsToTaggedSlot_IfServer called on non-authority!"));
		return 0;
	}

	// New check for slot item compatibility and weight capacity
	if (!IsTaggedSlotCompatible(ItemId, SlotTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item cannot be added to the tagged slot"));
		return 0;
	}

	// Locate the existing item in the tagged slot
	const int32 Index = GetIndexForTaggedSlot(SlotTag); // -1 if not found

	UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	FTaggedItemBundle* SlotItem = nullptr;
	int32 QuantityToAdd = RequestedQuantity;
	if (TaggedSlotItemInstances.IsValidIndex(Index))
	{
		SlotItem = &TaggedSlotItemInstances[Index];

		if (SlotItem && SlotItem->IsValid())
		{
			if ((SlotItem->ItemId != ItemId || ItemData->MaxStackSize == 1))
			{
				return 0; // Slot taken
			}

			QuantityToAdd = FMath::Min(RequestedQuantity, ItemData->MaxStackSize - SlotItem->Quantity);

			if ((!AllowPartial && QuantityToAdd < RequestedQuantity) || QuantityToAdd == 0)
			{
				return 0; // Slot can't accept all
			}
		}
	}
	
	// We also need to add to container as items are duplicated between the containers instances and the tagged slots, but we suppres
	QuantityToAdd = AddItem_IfServer(ItemSource, ItemId, QuantityToAdd, AllowPartial, true);

	if (QuantityToAdd == 0)
	{
		return 0;
	}

	FTaggedItemBundle PreviousItem = SlotItem ? *SlotItem : FTaggedItemBundle();
	
	if (!SlotItem)
	{
		TaggedSlotItemInstances.Add(FTaggedItemBundle());
		SlotItem = &TaggedSlotItemInstances[TaggedSlotItemInstances.Num() - 1];
		SlotItem->Tag = SlotTag;
		SlotItem->ItemId = ItemId;
		SlotItem->Quantity = 0;
	}
	
	
	SlotItem->Quantity += QuantityToAdd;
	

	UpdateWeightAndSlots();
	
	OnItemAddedToTaggedSlot.Broadcast(SlotTag, ItemData, QuantityToAdd, PreviousItem, EItemChangeReason::Added);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItemInstances, this);

	return QuantityToAdd;
}

int32 UInventoryComponent::AddItemToAnySlot(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool PreferTaggedSlots)
{
	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}
	
	const int32 AcceptableQuantity = GetReceivableQuantity(ItemId);
	
	const int32 QuantityToAdd = FMath::Min(AcceptableQuantity, RequestedQuantity);

	if (QuantityToAdd <= 0)
		return 0;

	// Distribution plan should not try to add to left hand as left hand already has sticks!
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan =  GetItemDistributionPlan(ItemId, QuantityToAdd, PreferTaggedSlots);
	
	const int32 ActualAdded = AddItem_IfServer(ItemSource, ItemId, QuantityToAdd, true, true);

	if (GetOwnerRole() >= ROLE_Authority || GetOwnerRole() == ROLE_None)
		ensureMsgf(ActualAdded == QuantityToAdd, TEXT("Failed to add all items to container despite quantity calculated"));

	bool AddedAnyToGenericSlots = false;
	for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
	{
		const FGameplayTag& SlotTag = std::get<0>(Plan);
		const int32 QuantityToAddSlot = std::get<1>(Plan);

		if (SlotTag.IsValid())
		{
			FTaggedItemBundle PreviousItem = GetItemForTaggedSlot(SlotTag);
			// We do a move because the item has already been added to the container
			const int32 ActualAddedQuantity = MoveItem_ServerImpl(ItemId, QuantityToAddSlot, FGameplayTag::EmptyTag, SlotTag, false, FGameplayTag(), 0, true);
			OnItemAddedToTaggedSlot.Broadcast(SlotTag, ItemData, ActualAddedQuantity, PreviousItem, EItemChangeReason::Added);

			ensureMsgf(ActualAddedQuantity == QuantityToAddSlot, TEXT("Failed to add all items to tagged slot despite plan calculated"));
		}
		else
		{
			AddedAnyToGenericSlots = true;
		}
	}

	if (AddedAnyToGenericSlots)
	{
		OnItemAddedToContainer.Broadcast(ItemData, QuantityToAdd, EItemChangeReason::Added);
	}

	UpdateWeightAndSlots();

	return QuantityToAdd; // Total quantity successfully added across slots
}


void UInventoryComponent::PickupItem_Server_Implementation(AWorldItem* WorldItem, bool PreferTaggedSlots, bool DestroyAfterPickup)
{
	FGameplayTag ItemId = WorldItem->RepresentedItem.ItemId;
	const int32 QuantityAdded = AddItemToAnySlot(WorldItem, ItemId, WorldItem->RepresentedItem.Quantity, PreferTaggedSlots);

	if (QuantityAdded == 0) return;

	if (!WorldItem->RepresentedItem.IsValid())
	{
		// destroy the actor
		if (DestroyAfterPickup)
			WorldItem->Destroy();
	}
	else
	{
		// update the item quantity
		WorldItem->RepresentedItem.Quantity -= QuantityAdded;
		if (WorldItem->RepresentedItem.Quantity <= 0 && DestroyAfterPickup)
		{
			// destroy the actor
			WorldItem->Destroy();
		}
	}
}

int32 UInventoryComponent::RemoveQuantityFromTaggedSlot_IfServer(const FGameplayTag& SlotTag, int32 QuantityToRemove, EItemChangeReason Reason,
                                                                    bool AllowPartial, bool DestroyFromContainer)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		return 0;
	}

	int32 IndexToRemoveAt = GetIndexForTaggedSlot(SlotTag);

	if (IndexToRemoveAt < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tagged slot does not exist"));
		return 0;
	}
	
	FTaggedItemBundle* InstanceToRemoveFrom = &TaggedSlotItemInstances[IndexToRemoveAt];
	
	if (!AllowPartial && InstanceToRemoveFrom->Quantity <
		QuantityToRemove)
		return 0;

	const FGameplayTag RemovedId = InstanceToRemoveFrom->ItemId;
	const FGameplayTag RemovedFromTag = InstanceToRemoveFrom->Tag;

	const int32 ActualRemovedQuantity = FMath::Min(QuantityToRemove, InstanceToRemoveFrom->Quantity);
	InstanceToRemoveFrom->Quantity -= ActualRemovedQuantity;
	if (InstanceToRemoveFrom->Quantity <= 0)
	{
		TaggedSlotItemInstances.RemoveAt(IndexToRemoveAt);
	}

	if (DestroyFromContainer)
		DestroyItemImpl(RemovedId, ActualRemovedQuantity, Reason, true, false, false);

	UpdateWeightAndSlots();

	const auto* ItemData = URISSubsystem::GetItemDataById(RemovedId);
	OnItemRemovedFromTaggedSlot.Broadcast(RemovedFromTag, ItemData, ActualRemovedQuantity, Reason);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItemInstances, this);
	return ActualRemovedQuantity;
}

int32 UInventoryComponent::RemoveItemFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove, EItemChangeReason Reason, bool DestroyFromContainer)
{
	int32 RemovedCount = 0;
	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		if (TaggedSlotItemInstances[i].ItemId == ItemId)
		{
			RemovedCount += RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag,
			                                                      QuantityToRemove - RemovedCount, Reason, true, DestroyFromContainer);
			if (RemovedCount >= QuantityToRemove)
			{
				break;
			}
		}
	}

	return RemovedCount;
}

void UInventoryComponent::MoveItem_Server_Implementation(const FGameplayTag& ItemId, int32 Quantity,
                                                             const FGameplayTag& SourceTaggedSlot,
                                                             const FGameplayTag& TargetTaggedSlot,
                                                             const FGameplayTag& SwapItemId,
                                                             int32 SwapQuantity)
{
	MoveItem_ServerImpl(ItemId, Quantity, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId, SwapQuantity);
}

int32 UInventoryComponent::MoveItem_ServerImpl(const FGameplayTag& ItemId, int32 RequestedQuantity,
                                                   const FGameplayTag& SourceTaggedSlot,
                                                   const FGameplayTag& TargetTaggedSlot, bool AllowAutomaticSwapping,
                                                   const FGameplayTag& SwapItemId, int32 SwapQuantity, bool SuppressUpdate)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveItemsToTaggedSlot_ServerImpl called on non-authority!"));
		return 0;
	}

	const bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid();
	const bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid();

	if (!SourceIsTaggedSlot && !TargetIsTaggedSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moving to and from container is not meaningful"));
		return 0;
	}
	
	FGenericItemBundle  SourceItem;
	int32 SourceItemContainerIndex = -1;
	int32 SourceTaggedSlotIndex = -1;
	if (SourceIsTaggedSlot)
	{
		SourceTaggedSlotIndex = GetIndexForTaggedSlot(SourceTaggedSlot);
		if (!TaggedSlotItemInstances.IsValidIndex(SourceTaggedSlotIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Source tagged slot does not exist"));
			return 0;
		}

		SourceItem =  &TaggedSlotItemInstances[SourceTaggedSlotIndex];
	}
	else
	{
		for (int i = 0; i < ItemsVer.Items.Num(); i++)
		{
			if (ItemsVer.Items[i].ItemId == ItemId)
			{
				SourceItemContainerIndex = i;
				SourceItem = &ItemsVer.Items[i];
				break;
			}
		}

		if (!SourceItem.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Source container item does not exist"));
			return 0;
		}
	}
	
	bool SwapBackRequested = SwapItemId.IsValid() && SwapQuantity > 0;

	FGenericItemBundle TargetItem;
	const UItemStaticData* TargetItemData = nullptr;
	if (TargetIsTaggedSlot)
	{
		if (!IsTaggedSlotCompatible(ItemId, TargetTaggedSlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
			return 0;
		}

		const int32 TargetIndex = GetIndexForTaggedSlot(TargetTaggedSlot);

		if (!TaggedSlotItemInstances.IsValidIndex(TargetIndex))
		{
			if (!UniversalTaggedSlots.Contains(TargetTaggedSlot) && !SpecializedTaggedSlots.Contains(TargetTaggedSlot))
			{
				UE_LOG(LogTemp, Warning, TEXT("Target tagged slot does not exist"));
				return 0;
			}

			TaggedSlotItemInstances.Add(FTaggedItemBundle(TargetTaggedSlot, FItemBundle::EmptyItemInstance));
			TargetItem = &TaggedSlotItemInstances.Last();
		}
		else
		{
			TargetItem = &TaggedSlotItemInstances[TargetIndex];
		}

		if (TargetIndex >= 0)
		{
			TargetItemData = URISSubsystem::GetItemDataById(TargetItem.GetItemId());
			if (!TargetItemData)
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
				return 0; // Item data not found
			}

			if (TargetItem.GetItemId() == ItemId)
			{
				RequestedQuantity = FMath::Min(RequestedQuantity, TargetItemData->MaxStackSize > 1 ? TargetItemData->MaxStackSize - TargetItem.GetQuantity() : 0);

				if (RequestedQuantity <= 0)
					return 0;
			}
		}
	}
	else
	{
		if (SwapBackRequested)
			TargetItem = FindItemInstance(SwapItemId);
		else
			TargetItem = FindItemInstance(ItemId);
		
		if (TargetItem.IsValid())
		{
			TargetItemData = URISSubsystem::GetItemDataById(TargetItem.GetItemId());
		}

		ensureMsgf(TargetItem.IsValid(), TEXT("Target item not found"));
	}

	if (TargetItem.IsValid() && SourceIsTaggedSlot && URISFunctions::ShouldItemsBeSwapped(SourceItem.GetItemId(), TargetItem.GetItemId()) && !
		IsTaggedSlotCompatible(TargetItem.GetItemId(), SourceTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the source slot"));
		return 0;
	}

	if (!AllowAutomaticSwapping && TargetItem.GetItemId().IsValid() && SourceItem.GetItemId() != TargetItem.GetItemId())
		return 0;

	if (SwapBackRequested &&
	(!TargetItem.GetItemId().IsValid() || (TargetIsTaggedSlot && TargetItem.GetItemId() != SwapItemId) || TargetItem.GetQuantity() < SwapQuantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Requested swap was invalid"));
		return 0;
	}

	const auto SourceItemData = URISSubsystem::GetItemDataById(SourceItem.GetItemId());

	// Now execute the move
	int32 MovedQuantity = RequestedQuantity;
	if (SourceIsTaggedSlot && TargetIsTaggedSlot)
	{
		const FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(
			SourceItem, TargetItem, false, RequestedQuantity, true);
		
		// SourceItem and TargetItem are now swapped in content
		
		if (!SourceItem.IsValid())
			TaggedSlotItemInstances.RemoveAt(GetIndexForTaggedSlot(SourceTaggedSlot));

		MovedQuantity = MoveResult.QuantityMoved;
		if (!SuppressUpdate)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity, EItemChangeReason::Moved);
			if (MoveResult.WereItemsSwapped && IsValid(TargetItemData)) // might be null if swapping to empty slot
			{
				OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SourceItem.GetQuantity(), EItemChangeReason::Moved);
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SourceItem.GetQuantity(), FTaggedItemBundle(TargetTaggedSlot, TargetItem.GetItemId(), SourceItem.GetQuantity()), EItemChangeReason::Moved);
			}
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity, FTaggedItemBundle(TargetTaggedSlot, SourceItem.GetItemId(), MovedQuantity), EItemChangeReason::Moved);
		}
	}
	else if (SourceIsTaggedSlot)
	{
		if (SwapBackRequested) // swap from container to source tagged slot
		{
			SourceItem.SetItemId(SwapItemId);
			SourceItem.SetQuantity(SwapQuantity);
			if (!SuppressUpdate)
				OnItemRemovedFromContainer.Broadcast(TargetItemData, SwapQuantity, EItemChangeReason::Moved);
		}
		else
		{
			SourceItem.SetQuantity(SourceItem.GetQuantity() - MovedQuantity);
			if (SourceItem.GetQuantity() <= 0)
			{
				TaggedSlotItemInstances.RemoveAt(SourceTaggedSlotIndex);
			}
		}

		if (!SuppressUpdate)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity, EItemChangeReason::Moved);
			OnItemAddedToContainer.Broadcast(SourceItemData, MovedQuantity, EItemChangeReason::Moved);
		
			if (SwapBackRequested)
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SwapQuantity, FTaggedItemBundle(SourceTaggedSlot, TargetItem.GetItemId(), SwapQuantity), EItemChangeReason::Moved);
		}
	}
	else // TargetIsTaggedSlot
	{

		if (SwapItemId.IsValid() && SwapQuantity > 0 && !SuppressUpdate) // first perform any requested swap from target tagged slot to container
		{
			ensureMsgf(SwapQuantity == TargetItem.GetQuantity(), TEXT("Requested swap did not swap all of target item"));
			// Notify of the first part of the swap (we dont actually need to do any moving as its going to get overwritten anyway)
			OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SwapQuantity, EItemChangeReason::Moved);
			OnItemAddedToContainer.Broadcast(TargetItemData, SwapQuantity, EItemChangeReason::Moved);
		}	
	
		if (TargetItem.GetItemId() != ItemId) // If we are swapping or filling a newly added tagged slot
		{
			TargetItem.SetItemId(ItemId);
			TargetItem.SetQuantity(0);
		}
		TargetItem.SetQuantity(TargetItem.GetQuantity() + MovedQuantity);
		if (!SuppressUpdate)
		{
			OnItemRemovedFromContainer.Broadcast(SourceItemData, MovedQuantity, EItemChangeReason::Moved);
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity, FTaggedItemBundle(TargetTaggedSlot, SourceItem.GetItemId(), MovedQuantity), EItemChangeReason::Moved);
		}
	}

	if (MovedQuantity > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItemInstances, this);
	}

	if (!SuppressUpdate)
		UpdateWeightAndSlots(); // Slots might have changed

	return MovedQuantity;
}

void UInventoryComponent::PickupItem(AWorldItem* WorldItem, bool PreferTaggedSlots, bool DestroyAfterPickup)
{
	if (!WorldItem)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("PickupItem called with null world item"));
		return;
	}

	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		TArray<std::tuple<FGameplayTag, int32>> DistributionPlan =  GetItemDistributionPlan(WorldItem->RepresentedItem.ItemId, WorldItem->RepresentedItem.Quantity, PreferTaggedSlots);
		for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
		{
			const FGameplayTag& SlotTag = std::get<0>(Plan);
			const int32 QuantityToAddSlot = std::get<1>(Plan);

			if (SlotTag.IsValid())
			{
				RequestedOperationsToServer.Add(FRISExpectedOperation(AddTagged, SlotTag, WorldItem->RepresentedItem.ItemId, QuantityToAddSlot));
			}
			else
			{
				RequestedOperationsToServer.Add(FRISExpectedOperation(Add, WorldItem->RepresentedItem.ItemId, QuantityToAddSlot));
			}
		}
	}

	PickupItem_Server(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
}

int32 UInventoryComponent::MoveItem(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SourceTaggedSlot, const FGameplayTag& TargetTaggedSlot,
	const FGameplayTag& SwapItemId, int32 SwapQuantity)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		// TODO: Make sure tests dont rely on return value so we can set return type to void
		MoveItem_Server(ItemId, Quantity, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
		return -1;
	}
	else
	{
		return MoveItem_ServerImpl(ItemId, Quantity, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId, SwapQuantity);
	}
	
}


bool UInventoryComponent::CanTaggedSlotReceiveItem(const FItemBundle& ItemInstance,
                                                      const FGameplayTag& SlotTag) const
{
	return IsTaggedSlotCompatible(ItemInstance.ItemId, SlotTag) && CanContainerReceiveItems(ItemInstance.ItemId, ItemInstance.Quantity);
}

int32 UInventoryComponent::GetQuantityOfItemTaggedSlotCanReceive(const FGameplayTag& ItemId,
                                                                    const FGameplayTag& SlotTag) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	int32 QuantityThatCanBeAddedToTaggedSlots = 0;

	if (IsTaggedSlotCompatible(ItemId, SlotTag))
	{
		const FTaggedItemBundle& ItemInSlot = GetItemForTaggedSlot(SlotTag);
		if (ItemInSlot.IsValid() && ItemInSlot.ItemId == ItemId)
		{
			QuantityThatCanBeAddedToTaggedSlots += ItemData->MaxStackSize > 1
				                                       ? ItemData->MaxStackSize - ItemInSlot.Quantity
				                                       : 0;
		}
		else if (!ItemInSlot.IsValid())
		{
			QuantityThatCanBeAddedToTaggedSlots += ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;
		}
	}

	return QuantityThatCanBeAddedToTaggedSlots;
}


int32 UInventoryComponent::DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, FVector RelativeDropLocation)
{
	// On client the below is just a guess
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (!Item.IsValid()) return 0;
	int32 QuantityToDrop = FMath::Min(Quantity, Item.Quantity);

	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
		RequestedOperationsToServer.Add(FRISExpectedOperation(RemoveTagged, SlotTag, QuantityToDrop));
			
	DropFromTaggedSlot_Server(SlotTag, Quantity, RelativeDropLocation);

	return QuantityToDrop;
}

void UInventoryComponent::DropFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag, int32 Quantity,
                                                                      FVector RelativeDropLocation)
{
	const int32 Index = GetIndexForTaggedSlot(SlotTag);
	const FTaggedItemBundle& Item = TaggedSlotItemInstances[Index];
	if (!Item.Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DropFromTaggedSlot called with invalid slot tag"));
		return;
	}

	FGameplayTag ItemId = Item.ItemId;

	const FTaggedItemBundle ItemContained = GetItemForTaggedSlot(SlotTag);
	int32 QuantityToDrop = 0;
	if (ItemContained.IsValid() && ItemContained.ItemId == ItemId)
	{
		QuantityToDrop = FMath::Min(Quantity, ItemContained.Quantity);
	}

	TArray<UItemInstanceData*> StateArrayToAppendTo;
	ExtractItemFromTaggedSlot_IfServer(SlotTag, Item.ItemId, QuantityToDrop, EItemChangeReason::Dropped, StateArrayToAppendTo);

	// Spawn item in the world and update state
	SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, QuantityToDrop, RelativeDropLocation, StateArrayToAppendTo);
}


int32 UInventoryComponent::UseItemFromTaggedSlot(const FGameplayTag& SlotTag)
{
	// On client the below is just a guess
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (!Item.IsValid()) return 0;

	auto ItemId = Item.ItemId;
	
	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0;
	}

	const UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>(UUsableItemDefinition::StaticClass());

	if (!UsableItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return 0;
	}

	int32 QuantityToRemove = UsableItem->QuantityPerUse;
	
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
		RequestedOperationsToServer.Add(FRISExpectedOperation(RemoveTagged, SlotTag, QuantityToRemove));
	
	UseItemFromTaggedSlot_Server(SlotTag);

	return QuantityToRemove;
}



void UInventoryComponent::UseItemFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag)
{
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (Item.Tag.IsValid())
	{
		const auto ItemId = Item.ItemId;
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		if (!ItemData)
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
			return;
		}
	
		UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>(UUsableItemDefinition::StaticClass());

		if (!UsableItem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
			return;
		}

		const int32 QuantityToConsume = UsableItem->QuantityPerUse;
		
		int32 ConsumedCount = RemoveQuantityFromTaggedSlot_IfServer(SlotTag, QuantityToConsume, EItemChangeReason::Consumed, false, true);
		if (ConsumedCount > 0 || UsableItem->QuantityPerUse == 0)
		{
			UsableItem->Use(GetOwner());
		}
	}
}

const FTaggedItemBundle& UInventoryComponent::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	int32 Index = GetIndexForTaggedSlot(SlotTag);

	if (Index < 0 || Index >= TaggedSlotItemInstances.Num())
	{
		return FTaggedItemBundle::EmptyItemInstance;
	}

	return TaggedSlotItemInstances[Index];
}

void UInventoryComponent::SetTaggedSlotBlocked(FGameplayTag Slot, bool IsBlocked)
{
	int32 SlotIndex = GetIndexForTaggedSlot(Slot);
	if (SlotIndex >= 0)
	{
		TaggedSlotItemInstances[SlotIndex].IsBlocked = true;
	}
	else
	{
		// Add the slot with the blocked flag
		TaggedSlotItemInstances.Add(FTaggedItemBundle(Slot, FGameplayTag(), 0));
		TaggedSlotItemInstances.Last().IsBlocked = true;
	}
}

int32 UInventoryComponent::GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const
{
	// loop over SpecialSlotItems
	for (int i = 0; i < TaggedSlotItemInstances.Num(); i++)
	{
		if (TaggedSlotItemInstances[i].Tag == SlotTag)
			return i;
	}

	return -1;
}

TArray<FTaggedItemBundle> UInventoryComponent::GetAllTaggedItems() const
{
	return TaggedSlotItemInstances;
}

void UInventoryComponent::DetectAndPublishContainerChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	/*for (FTaggedItemBundle& NewItem : TaggedSlotItemInstances)
	{
		FItemBundle* OldItem = TaggedItemsCache.Find(NewItem.Tag);

		if (OldItem)
		{
			// Item is the same, check for quantity change
			if (OldItem->ItemId != NewItem.ItemInstance.ItemId || OldItem->Quantity != NewItem.ItemInstance.Quantity)
			{
				if (OldItem->ItemId == NewItem.ItemInstance.ItemId)
				{
					if (OldItem->Quantity < NewItem.ItemInstance.Quantity)
					{
						OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag, NewItem.ItemInstance.ItemId,
							                                  NewItem.ItemInstance.Quantity - OldItem->Quantity);
					}
					else if (OldItem->Quantity > NewItem.ItemInstance.Quantity)
					{
						OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag, NewItem.ItemInstance.ItemId,
							                                      OldItem->Quantity - NewItem.ItemInstance.Quantity);
					}
				}
				else // Item has changed
				{
					OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag, OldItem->ItemId, OldItem->Quantity);
					OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,NewItem.ItemInstance.ItemId,
					                                                   NewItem.ItemInstance.Quantity);
				}
			}

			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -NewItem.ItemInstance.Quantity;
		}
		else // New slot has been added to
		{
			OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,NewItem.ItemInstance.ItemId,
			                                                   NewItem.ItemInstance.Quantity);
			TaggedItemsCache.Add(NewItem.Tag,
			                     FItemBundle(NewItem.ItemInstance.ItemId, -NewItem.ItemInstance.Quantity));
			// Mark as processed
		}
	}

	// Second pass: Remove unmarked items (those not set to negative) and revert marks for processed items
	_SlotsToRemove.Reset(TaggedItemsCache.Num());
	for (auto& SlotItemKV : TaggedItemsCache)
	{
		if (SlotItemKV.Value.Quantity >= 0)
		{
			// Item was not processed (not found in Items), so it has been removed
			OnItemRemovedFromTaggedSlot.Broadcast(SlotItemKV.Key, SlotItemKV.Value);
			_SlotsToRemove.Add(SlotItemKV.Key);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			SlotItemKV.Value.Quantity = -SlotItemKV.Value.Quantity;
		}
	}

	// Remove items that were not found in the current Items array
	for (const FGameplayTag& Key : _SlotsToRemove)
	{
		TaggedItemsCache.Remove(Key);
	}*/
}

bool UInventoryComponent::IsTaggedSlotCompatible(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		return false; // Item data not found, assume slot cannot receive item
	}

	if (UniversalTaggedSlots.Contains(SlotTag))
	{
		// loop over UniversalTaggedExclusiveCategoriesSlots and look for matching slot
		for (const FUniversalSlotExclusiveCategories& ExclusiveCategory : UniversalTaggedExclusiveCategoriesSlots)
		{
			// If the item has the exclusive category tag and the slot is not the universal slot, then it is not compatible
			if (ExclusiveCategory.UniversalTaggedSlot != SlotTag && ItemData->ItemCategories.HasTag(ExclusiveCategory.Category))
			{
				return false; 
			}
		}

		return true;
	}

	if (ItemData->ItemCategories.HasTag(SlotTag))
	{
		return true; // Item is not compatible with specialized tagged slot
	}
	
	return false;
}

bool UInventoryComponent::IsItemInTaggedSlotValid(const FGameplayTag SlotTag) const
{
	const FTaggedItemBundle* Item = TaggedSlotItemInstances.FindByPredicate([&SlotTag](const FTaggedItemBundle& Item) { return Item.Tag == SlotTag; });
	return Item && Item->IsValid();
}

TArray<std::tuple<FGameplayTag, int32>> UInventoryComponent::GetItemDistributionPlan(const FGameplayTag& ItemId, int32 QuantityToAdd, bool PreferTaggedSlots)
{
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan;
	
	if (QuantityToAdd <= 0)
		return DistributionPlan;

	// Now remember how many we can add to generic slots, we need to check this before increasing quantity

	int32 TotalQuantityDistributed = 0;
	int32 QuantityDistributedToGenericSlots = 0;

	// Adjust the flow based on PreferTaggedSlots flag
	if (!PreferTaggedSlots)
	{
		const int32 QuantityContainersGenericSlotsCanReceive = Super::GetReceivableQuantityImpl(ItemId);
		// Try adding to generic slots first if not preferring tagged slots
		QuantityDistributedToGenericSlots += FMath::Min(QuantityToAdd, QuantityContainersGenericSlotsCanReceive);
		TotalQuantityDistributed += QuantityDistributedToGenericSlots;
	}

	// Proceed to try adding to tagged slots if PreferTaggedSlots is true or if there's remaining quantity
	if (PreferTaggedSlots || TotalQuantityDistributed < QuantityToAdd)
	{
		// if ItemsToAdd is valid that means we haven't extracted the full quantity yet
		for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;

			int32 AddedToTaggedSlot = FMath::Min(QuantityToAdd - TotalQuantityDistributed, GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag));

			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;
			}
		}

		for (const FGameplayTag& SlotTag : UniversalTaggedSlots)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;

			int32 AddedToTaggedSlot = FMath::Min(QuantityToAdd - TotalQuantityDistributed, GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag));
			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;
			}
		}
	}

	// Any remaining quantity must be added to generic slots
	QuantityDistributedToGenericSlots  += QuantityToAdd - TotalQuantityDistributed;

	if (QuantityDistributedToGenericSlots > 0)
		DistributionPlan.Add(std::make_tuple(FGameplayTag::EmptyTag, QuantityDistributedToGenericSlots));

	return DistributionPlan;
}

void UInventoryComponent::OnRep_Slots()
{
	UpdateWeightAndSlots();
	DetectAndPublishContainerChanges();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// CRAFTING /////////////////////////////////////////////////////////////////

bool UInventoryComponent::CanCraftRecipeId(const FPrimaryRISRecipeId& RecipeId) const
{
	const UObjectRecipeData* Recipe = Cast<UObjectRecipeData>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(Recipe);
}

bool UInventoryComponent::CanCraftRecipe(const UObjectRecipeData* Recipe) const
{
	if (!Recipe) return false;

	for (const auto& Component : Recipe->Components)
	{
		if (!Contains(Component.ItemId, Component.Quantity))
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::CanCraftCraftingRecipe(const FPrimaryRISRecipeId& RecipeId) const
{
	UItemRecipeData* CraftingRecipe = Cast<UItemRecipeData>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(CraftingRecipe);
}

void UInventoryComponent::CraftRecipeId_Server_Implementation(const FPrimaryRISRecipeId& RecipeId)
{
	const UObjectRecipeData* Recipe = Cast<UObjectRecipeData>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	CraftRecipe_IfServer(Recipe);
}

bool UInventoryComponent::CraftRecipe_IfServer(const UObjectRecipeData* Recipe)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		return false;
	}

	bool bSuccess = false;
	if (Recipe && CanCraftRecipe(Recipe))
	{
		for (const auto& Component : Recipe->Components)
		{
			const int32 Removed = DestroyItem_IfServer(Component.ItemId, Component.Quantity, EItemChangeReason::Transformed);
			if (Removed < Component.Quantity)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to remove all items for crafting even though they were confirmed"));
				return false;
			}
		}
		bSuccess = true;

		if (const UItemRecipeData* ItemRecipe = Cast<UItemRecipeData>(Recipe))
		{
			const FItemBundle CraftedItem = FItemBundle(ItemRecipe->ResultingItemId, ItemRecipe->QuantityCreated);
			const int32 QuantityAdded = AddItemToAnySlot(Subsystem, CraftedItem.ItemId, CraftedItem.Quantity, false);
			if (QuantityAdded < ItemRecipe->QuantityCreated)
			{
				UE_LOG(LogTemp, Display, TEXT("Failed to add crafted item to inventory, dropping item instead"));

				if (!Subsystem)
				{
					UE_LOG(LogTemp, Error, TEXT("Subsystem is null, cannot drop item"));
					return false;
				}
				
				TArray<UItemInstanceData*> DroppingItemState;
				Execute_ExtractItem_IfServer(Subsystem, CraftedItem.ItemId, CraftedItem.Quantity - QuantityAdded,
				                              EItemChangeReason::Transformed, DroppingItemState);
				
				DropItemFromContainer_Server(CraftedItem.ItemId, CraftedItem.Quantity - QuantityAdded);
			}
		}
		else
		{
			OnCraftConfirmed.Broadcast(Recipe->ResultingObject, Recipe->QuantityCreated);
		}
	}
	return bSuccess;
}

void UInventoryComponent::SetRecipeLock_Server_Implementation(const FPrimaryRISRecipeId& RecipeId, bool LockState)
{
	if (AllUnlockedRecipes.Contains(RecipeId) != LockState)
	{
		if (LockState)
		{
			AllUnlockedRecipes.RemoveSingle(RecipeId);
		}
		else
		{
			AllUnlockedRecipes.Add(RecipeId);
		}

		if (GetNetMode() == NM_ListenServer)
		{
			CheckAndUpdateRecipeAvailability();
		}
	}
}

UObjectRecipeData* UInventoryComponent::GetRecipeById(const FPrimaryRISRecipeId& RecipeId)
{
	return Cast<UObjectRecipeData>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
}

TArray<UObjectRecipeData*> UInventoryComponent::GetAvailableRecipes(FGameplayTag TagFilter)
{
	return CurrentAvailableRecipes.Contains(TagFilter)
		       ? CurrentAvailableRecipes[TagFilter]
		       : TArray<UObjectRecipeData*>();
}

void UInventoryComponent::CheckAndUpdateRecipeAvailability()
{
	// Clear current available recipes
	CurrentAvailableRecipes.Empty();

	// Iterate through all available recipes and check if they can be crafted
	for (const FPrimaryRISRecipeId& RecipeId : AllUnlockedRecipes)
	{
		UObjectRecipeData* Recipe = GetRecipeById(RecipeId);
		if (CanCraftRecipe(Recipe))
		{
			for (const FGameplayTag& Category : RecipeTagFilters)
			{
				// If recipe matches a category, add it to the corresponding list
				if (Recipe->Tags.HasTag(Category))
				{
					if (!CurrentAvailableRecipes.Contains(Category))
					{
						CurrentAvailableRecipes.Add(Category, {}); // ensure the value is a valid array
					}
					CurrentAvailableRecipes[Category].Add(Recipe);
				}
			}
		}
	}

	OnAvailableRecipesUpdated.Broadcast();
}

int32 UInventoryComponent::DropAllItems_ServerImpl()
{
	int32 DroppedCount = 0;
	const float AngleStep = 360.f / ItemsVer.Items.Num();

	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(FMath::FRand() * 100, FMath::FRand() * 100 , 100);
		DropFromTaggedSlot_Server(TaggedSlotItemInstances[i].Tag, TaggedSlotItemInstances[i].Quantity, DropLocation);
		DroppedCount++;
	}
	
	TaggedSlotItemInstances.Empty();

	for (int i = ItemsVer.Items.Num() - 1; i >= 0; i--)
	{
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(FMath::FRand() * 100, FMath::FRand() * 100 , 100);
		DropItemFromContainer_Server(ItemsVer.Items[i].ItemId, ItemsVer.Items[i].Quantity, DropLocation);
		DroppedCount++;
	}
	
	return DroppedCount;
}

int32 UInventoryComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial, bool UpdateAfter, bool SendEventAfter)
{
	const int32 DestroyedCount = Super::DestroyItemImpl(ItemId, Quantity, Reason, AllowPartial, UpdateAfter, SendEventAfter);
	// Remember that tagged items are also in the container,
	// so if we had 5 in container and 3 of those in tagged slots we might have removed the 5 from container
	// without removing the 3 from tagged slots, resulting in negative 3 underflow
	const int32 QuantityUnderflow = GetContainerOnlyItemQuantity(ItemId);
	if (QuantityUnderflow < 0)
	{
		RemoveItemFromAnyTaggedSlots_IfServer(ItemId, -QuantityUnderflow, Reason, false);
	}

	return DestroyedCount;
}

int32 UInventoryComponent::GetContainerOnlyItemQuantityImpl(const FGameplayTag& ItemId) const
{
	int32 Quantity = GetItemQuantityTotal(ItemId);
	for (const FTaggedItemBundle& TaggedItem : TaggedSlotItemInstances)
	{
		if (TaggedItem.ItemId == ItemId)
		{
			Quantity -= TaggedItem.Quantity;
		}
	}
	return Quantity;
}

bool UInventoryComponent::ContainsImpl(const FGameplayTag& ItemId, int32 Quantity) const
{
	return GetItemQuantityTotal(ItemId) >= Quantity;
}

void UInventoryComponent::ClearImpl()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}

	// This loop is temporary until we have a proper server rollback system
	for (auto& Item : ItemsVer.Items)
	{
		const int32 ContainedQuantityWithoutTaggedSlots = GetContainerOnlyItemQuantityImpl(Item.ItemId);
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(Item.ItemId);
		if (ContainedQuantityWithoutTaggedSlots > 0)
			OnItemRemovedFromContainer.Broadcast(ItemData, ContainedQuantityWithoutTaggedSlots, EItemChangeReason::ForceDestroyed);
	}

	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag, MAX_int32, EItemChangeReason::ForceDestroyed, true, false);
	}

	ItemsVer.Items.Reset();

	UpdateWeightAndSlots();
	// DetectAndPublishContainerChanges(); part of old strategy
}

int32 UInventoryComponent::GetReceivableQuantityImpl(const FGameplayTag& ItemId) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	const int32 AcceptableQuantityByWeight = GetQuantityContainerCanReceiveByWeight(ItemData);
	
	int32 AcceptableQuantityBySlots = GetQuantityContainerCanReceiveBySlots(ItemData);
	
	// Then add any available in the tagged slots
	for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
	{
		AcceptableQuantityBySlots += GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag);
	}

	for (const FGameplayTag& SlotTag : UniversalTaggedSlots)
	{
		AcceptableQuantityBySlots += GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag);
	}
	
	int32 FinalAcceptableQuantity = FMath::Min(AcceptableQuantityByWeight, AcceptableQuantityBySlots);
	if (OnValidateAddItemToContainer.IsBound())
		FinalAcceptableQuantity = FMath::Min(FinalAcceptableQuantity, OnValidateAddItemToContainer.Execute(ItemId, FinalAcceptableQuantity));
	
	return FinalAcceptableQuantity;	
}

void UInventoryComponent::OnInventoryItemAddedHandler(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::OnInventoryItemRemovedHandler(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::OnRep_Recipes()
{
	CheckAndUpdateRecipeAvailability();
}