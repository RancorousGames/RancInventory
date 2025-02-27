// Copyright Rancorous Games, 2024

#include "Components\InventoryComponent.h"

#include <ObjectArray.h>
#include <variant>
#include <GameFramework/Actor.h>
#include <Engine/AssetManager.h>

#include "IDetailTreeNode.h"
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

	SortUniversalTaggedSlots();

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
		if (!TaggedInstance.IsValid()) continue;
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

			UsedContainerSlotCount -= FMath::CeilToInt(
				TaggedInstance.Quantity / static_cast<float>(ItemData->MaxStackSize)) * SlotsTakenPerStack;
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

int32 UInventoryComponent::ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity,
                                                    EItemChangeReason Reason,
                                                    TArray<UItemInstanceData*>& StateArrayToAppendTo,
                                                    bool SuppressUpdate)
{
	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(
		ItemId, Quantity, Reason, StateArrayToAppendTo, false);

	// See DestroyItemsImpl for equivalent implementation and explanation
	const int32 QuantityUnderflow = GetContainerOnlyItemQuantity(ItemId);
	if (QuantityUnderflow < 0)
	{
		RemoveItemFromAnyTaggedSlots_IfServer(ItemId, -QuantityUnderflow, Reason, false);
	}

	return ExtractedFromContainer;
}

int32 UInventoryComponent::ExtractItemFromTaggedSlot_IfServer(const FGameplayTag& TaggedSlot,
                                                              const FGameplayTag& ItemId, int32 Quantity,
                                                              EItemChangeReason Reason,
                                                              TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	if (!ContainsImpl(ItemId, Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item not found in container"));
		return 0;
	}

	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(
		ItemId, Quantity, Reason, StateArrayToAppendTo, true);

	RemoveQuantityFromTaggedSlot_IfServer(TaggedSlot, ExtractedFromContainer, Reason, false, false);

	return ExtractedFromContainer;
}

const FUniversalTaggedSlot* UInventoryComponent::WouldItemMoveIndirectlyViolateBlocking(
	const FGameplayTag& TaggedSlot, const UItemStaticData* ItemData) const
{
	const FUniversalTaggedSlot* UniversalSlotDefinition = UniversalTaggedSlots.FindByPredicate(
		[&TaggedSlot](const FUniversalTaggedSlot& UniSlot) { return UniSlot.Slot == TaggedSlot; });

	if (UniversalSlotDefinition && UniversalSlotDefinition->IsValid() && UniversalSlotDefinition->UniversalSlotToBlock.
		IsValid())
	{
		const FTaggedItemBundle& PotentiallyBlockedSlotItem = GetItemForTaggedSlot(
			UniversalSlotDefinition->UniversalSlotToBlock);
		if (PotentiallyBlockedSlotItem.IsValid() && ItemData->ItemCategories.HasTag(
			UniversalSlotDefinition->RequiredItemCategoryToBlock))
		{
			// If the slot we should be blocking if equipped is blocked, we can't add to this slot
			return UniversalSlotDefinition;
		}
	}

	return nullptr;
}

void UInventoryComponent::UpdateBlockingState(FGameplayTag SlotTag, const UItemStaticData* ItemData, bool IsEquip)
{
	FUniversalTaggedSlot* UniversalSlotDefinition = UniversalTaggedSlots.FindByPredicate(
		[&SlotTag](const FUniversalTaggedSlot& UniSlot) { return UniSlot.Slot == SlotTag; });

	if (UniversalSlotDefinition && UniversalSlotDefinition->UniversalSlotToBlock.IsValid())
	{
		bool ShouldBlock = false;
		if (IsEquip && ItemData && ItemData->ItemCategories.
		                                     HasTag(UniversalSlotDefinition->RequiredItemCategoryToBlock))
		{
			ShouldBlock = true;
		}
		SetTaggedSlotBlocked(UniversalSlotDefinition->UniversalSlotToBlock, ShouldBlock);
	}
}


////////////////////////////////////////////////////// TAGGED SLOTS ///////////////////////////////////////////////////////
int32 UInventoryComponent::AddItemToTaggedSlot_IfServer(TScriptInterface<IItemSource> ItemSource,
                                                        const FGameplayTag& SlotTag, const FGameplayTag& ItemId,
                                                        int32 RequestedQuantity, bool AllowPartial)
{
	// Reminder: Items in tagged slots are duplicated in container

	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddItemsToTaggedSlot_IfServer called on non-authority!"));
		return 0;
	}

	UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);

	// New check for slot item compatibility and weight capacity
	if (!IsTaggedSlotCompatible(ItemId, SlotTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item cannot be added to the tagged slot"));
		return 0;
	}

	if (WouldItemMoveIndirectlyViolateBlocking(SlotTag, ItemData))
	{
		// This means an item in a slot that would be blocked by this item is already equipped, so try to unequip it
		const FUniversalTaggedSlot* UniversalSlotDefinition = UniversalTaggedSlots.FindByPredicate(
			[&SlotTag](const FUniversalTaggedSlot& UniSlot) { return UniSlot.Slot == SlotTag; });

		auto ExistingItem = GetItemForTaggedSlot(UniversalSlotDefinition->UniversalSlotToBlock);
		if (MoveItem_ServerImpl(ExistingItem.ItemId, ExistingItem.Quantity,
		                        UniversalSlotDefinition->UniversalSlotToBlock,
		                        FGameplayTag::EmptyTag) != ExistingItem.Quantity)
		{
			// We could not unequip the blocking item, so we can't add this item
			return 0;
		}
	}

	// Locate the existing item in the tagged slot
	const int32 Index = GetIndexForTaggedSlot(SlotTag); // -1 if not found

	FTaggedItemBundle* SlotItem = nullptr;
	int32 QuantityToAdd = RequestedQuantity;
	if (TaggedSlotItemInstances.IsValidIndex(Index))
	{
		SlotItem = &TaggedSlotItemInstances[Index];

		if (SlotItem->IsBlocked)
		{
			return 0;
		}

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
	QuantityToAdd = Super::AddItem_ServerImpl(ItemSource, ItemId, QuantityToAdd, AllowPartial, true);

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
		SlotItem->Quantity = 0;
	}
	SlotItem->ItemId = ItemId;

	UpdateBlockingState(SlotTag, ItemData, true);

	SlotItem->Quantity += QuantityToAdd;

	UpdateWeightAndSlots();

	OnItemAddedToTaggedSlot.Broadcast(SlotTag, ItemData, QuantityToAdd, PreviousItem, EItemChangeReason::Added);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItemInstances, this);

	return QuantityToAdd;
}

int32 UInventoryComponent::AddItemToAnySlot(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId,
                                            int32 RequestedQuantity, EPreferredSlotPolicy PreferTaggedSlots, bool AllowPartial)
{
	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	const int32 AcceptableQuantity = GetReceivableQuantity(ItemId); // Counts only container

	const int32 QuantityToAdd = FMath::Min(AcceptableQuantity, RequestedQuantity);

	if (QuantityToAdd <= 0 || (!AllowPartial && QuantityToAdd < RequestedQuantity))
		return 0;

	// Distribution plan should not try to add to left hand as left hand already has sticks!
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan = GetItemDistributionPlan(
		ItemData, QuantityToAdd, PreferTaggedSlots);

	const int32 ActualAdded = Super::AddItem_ServerImpl(ItemSource, ItemId, QuantityToAdd, AllowPartial, true);

	if (GetOwnerRole() >= ROLE_Authority || GetOwnerRole() == ROLE_None)
		ensureMsgf(ActualAdded == QuantityToAdd, TEXT("Failed to add all items to container despite quantity calculated"));

	int32 QuantityAddedToGenericSlot = 0;
	for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
	{
		const FGameplayTag& SlotTag = std::get<0>(Plan);
		const int32 QuantityToAddSlot = std::get<1>(Plan);

		if (SlotTag.IsValid())
		{
			FTaggedItemBundle PreviousItem = GetItemForTaggedSlot(SlotTag);
			// We do a move because the item has already been added to the container
			const int32 ActualAddedQuantity = MoveItem_ServerImpl(ItemId, QuantityToAddSlot, FGameplayTag::EmptyTag,
			                                                      SlotTag, false, FGameplayTag(), 0, true);

			UpdateBlockingState(SlotTag, ItemData, true);

			OnItemAddedToTaggedSlot.Broadcast(SlotTag, ItemData, ActualAddedQuantity, PreviousItem,
			                                  EItemChangeReason::Added);

			ensureMsgf(ActualAddedQuantity == QuantityToAddSlot,
			           TEXT("Failed to add all items to tagged slot despite plan calculated"));
		}
		else
		{
			QuantityAddedToGenericSlot += QuantityToAddSlot;
		}
	}

	if (QuantityAddedToGenericSlot > 0)
	{
		OnItemAddedToContainer.Broadcast(ItemData, QuantityAddedToGenericSlot, EItemChangeReason::Added);
	}

	UpdateWeightAndSlots();

	return QuantityToAdd; // Total quantity successfully added across slots
}


void UInventoryComponent::PickupItem_Server_Implementation(AWorldItem* WorldItem,
                                                           EPreferredSlotPolicy PreferTaggedSlots,
                                                           bool DestroyAfterPickup)
{
	FGameplayTag ItemId = WorldItem->RepresentedItem.ItemId;
	const int32 QuantityAdded = AddItemToAnySlot(WorldItem, ItemId, WorldItem->RepresentedItem.Quantity,
	                                             PreferTaggedSlots);

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

int32 UInventoryComponent::RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag SlotTag, int32 QuantityToRemove,
                                                                 EItemChangeReason Reason,
                                                                 bool AllowPartial, bool DestroyFromContainer,
                                                                 bool SkipWeightUpdate)
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

	if (!InstanceToRemoveFrom->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Tagged slot is empty"));
		return 0;
	}

	if (!AllowPartial && InstanceToRemoveFrom->Quantity <
		QuantityToRemove)
		return 0;

	const FGameplayTag RemovedId = InstanceToRemoveFrom->ItemId;
	const FGameplayTag RemovedFromTag = InstanceToRemoveFrom->Tag;

	const int32 ActualRemovedQuantity = FMath::Min(QuantityToRemove, InstanceToRemoveFrom->Quantity);
	InstanceToRemoveFrom->Quantity -= ActualRemovedQuantity;
	if (InstanceToRemoveFrom->Quantity <= 0 && !InstanceToRemoveFrom->IsBlocked)
	{
		TaggedSlotItemInstances.RemoveAt(IndexToRemoveAt);
	}

	if (DestroyFromContainer)
		DestroyItemImpl(RemovedId, ActualRemovedQuantity, Reason, true, false, false);


	if (!SkipWeightUpdate)
		UpdateWeightAndSlots();

	const auto* ItemData = URISSubsystem::GetItemDataById(RemovedId);
	UpdateBlockingState(SlotTag, ItemData, false);
	OnItemRemovedFromTaggedSlot.Broadcast(RemovedFromTag, ItemData, ActualRemovedQuantity, Reason);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItemInstances, this);
	return ActualRemovedQuantity;
}

int32 UInventoryComponent::RemoveItemFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove,
                                                                 EItemChangeReason Reason, bool DestroyFromContainer)
{
	int32 RemovedCount = 0;
	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		if (TaggedSlotItemInstances[i].ItemId == ItemId)
		{
			RemovedCount += RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag,
			                                                      QuantityToRemove - RemovedCount, Reason, true,
			                                                      DestroyFromContainer);
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
                                               const FGameplayTag& SwapItemId, int32 SwapQuantity, bool SuppressUpdate,
                                               bool SimulateMoveOnly)
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

	FGenericItemBundle SourceItem;
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

		SourceItem = &TaggedSlotItemInstances[SourceTaggedSlotIndex];
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
			if (!SpecializedTaggedSlots.Contains(TargetTaggedSlot) &&
				!ContainedInUniversalSlot(TargetTaggedSlot))
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
			if (TargetItemData)
			// TargetIndex might be valid but targetitemdata invalid if the slot was added for locking purposes without an item
			{
				if (TargetItem.GetItemId() == ItemId)
				{
					RequestedQuantity = FMath::Min(RequestedQuantity,
					                               TargetItemData->MaxStackSize > 1
						                               ? TargetItemData->MaxStackSize - TargetItem.GetQuantity()
						                               : 0);

					if (RequestedQuantity <= 0)
						return 0;
				}
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

	if (TargetItem.IsBlocked())
		return 0;

	if (TargetItem.IsValid() && SourceIsTaggedSlot && URISFunctions::ShouldItemsBeSwapped(
			SourceItem.GetItemId(), TargetItem.GetItemId()) &&
		(!IsTaggedSlotCompatible(TargetItem.GetItemId(), SourceTaggedSlot) ||
			WouldItemMoveIndirectlyViolateBlocking(SourceTaggedSlot, TargetItemData)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Target Item is not compatible with the source slot"));
		return 0;
	}

	if (!AllowAutomaticSwapping && TargetItem.GetItemId().IsValid() && SourceItem.GetItemId() != TargetItem.GetItemId())
		return 0;


	const auto SourceItemData = URISSubsystem::GetItemDataById(SourceItem.GetItemId());
	if (!SourceItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Source item data not found"));
		return 0;	
	}
	
	if (TargetItem.GetItemId().IsValid())
	{
		const bool ShouldStack = SourceItemData->MaxStackSize > 1 && SourceItem.GetItemId() == TargetItem.GetItemId();
		if (!SwapBackRequested && !ShouldStack && SourceItem.GetQuantity() > RequestedQuantity)
		{
			return 0;
		}
	}
	else
	{
		if (SwapBackRequested && ((TargetIsTaggedSlot && TargetItem.GetItemId() != SwapItemId) || TargetItem.
			GetQuantity() < SwapQuantity))
		{
			UE_LOG(LogTemp, Warning, TEXT("Requested swap was invalid"));
			return 0;
		}
	}


	if (WouldItemMoveIndirectlyViolateBlocking(TargetTaggedSlot, SourceItemData))
	{
		return 0;
	}


	// Now execute the move
	int32 MovedQuantity = FMath::Min(RequestedQuantity, SourceItem.GetQuantity());
	int32 SourceQuantity = SourceItem.GetQuantity();
	const FGameplayTag& SourceItemId = SourceItem.GetItemId();
	int32 TargetQuantity = TargetItem.GetQuantity();
	const FGameplayTag& TargetItemId = TargetItem.GetItemId();

	if (SimulateMoveOnly) return MovedQuantity;

	if (MovedQuantity <= 0)
		return 0;

	if (SourceIsTaggedSlot && TargetIsTaggedSlot)
	{
		const FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem,  false, RequestedQuantity, true);

		// SourceItem and TargetItem are now swapped in content

		if (!SourceItem.IsValid())
			TaggedSlotItemInstances.RemoveAt(GetIndexForTaggedSlot(SourceTaggedSlot));

		MovedQuantity = MoveResult.QuantityMoved;
		UpdateBlockingState(SourceTaggedSlot, TargetItemData, false);
		UpdateBlockingState(TargetTaggedSlot, SourceItemData, true);
		if (!SuppressUpdate)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity,
			                                      EItemChangeReason::Moved);
			if (MoveResult.WereItemsSwapped && IsValid(TargetItemData)) // might be null if swapping to empty slot
			{
				OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SourceItem.GetQuantity(),
				                                      EItemChangeReason::Moved);
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SourceItem.GetQuantity(),
				                                  FTaggedItemBundle(TargetTaggedSlot, SourceItemId,
				                                                   SourceQuantity),
				                                  EItemChangeReason::Moved);
			}
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity,
			                                  FTaggedItemBundle(TargetTaggedSlot, TargetItemId,
			                                                    TargetQuantity), EItemChangeReason::Moved);
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

		UpdateBlockingState(SourceTaggedSlot, TargetItemData, SwapBackRequested);
		if (!SuppressUpdate)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity,
			                                      EItemChangeReason::Moved);
			OnItemAddedToContainer.Broadcast(SourceItemData, MovedQuantity, EItemChangeReason::Moved);

			if (SwapBackRequested)
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SwapQuantity,
				                                  FTaggedItemBundle(SourceTaggedSlot, SourceItemId,
				                                                    SourceQuantity), EItemChangeReason::Moved);
		}
	}
	else // TargetIsTaggedSlot
	{
		if (SwapItemId.IsValid() && SwapQuantity > 0 && !SuppressUpdate)
		// first perform any requested swap from target tagged slot to container
		{
			ensureMsgf(SwapQuantity == TargetItem.GetQuantity(),
			           TEXT("Requested swap did not swap all of target item"));
			ensureMsgf(!SourceIsTaggedSlot || MovedQuantity == SourceItem.GetQuantity(),
			           TEXT("Requested swap did not swap all of tagged source item"));
			// Notify of the first part of the swap (we dont actually need to do any moving as its going to get overwritten anyway)
			OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SwapQuantity,
			                                      EItemChangeReason::Moved);
			OnItemAddedToContainer.Broadcast(TargetItemData, SwapQuantity, EItemChangeReason::Moved);
		}

		auto PreviousItem = FTaggedItemBundle(TargetTaggedSlot, TargetItemId, TargetQuantity);
		if (TargetItem.GetItemId() != ItemId) // If we are swapping or filling a newly added tagged slot
		{
			TargetItem.SetItemId(ItemId);
			TargetItem.SetQuantity(0);
		}
		TargetItem.SetQuantity(TargetItem.GetQuantity() + MovedQuantity);
		UpdateBlockingState(TargetTaggedSlot, SourceItemData, true);
		if (!SuppressUpdate)
		{
			OnItemRemovedFromContainer.Broadcast(SourceItemData, MovedQuantity, EItemChangeReason::Moved);
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity, PreviousItem,
			                                  EItemChangeReason::Moved);
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

void UInventoryComponent::PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots,
                                     bool DestroyAfterPickup)
{
	if (!WorldItem)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("PickupItem called with null world item"));
		return;
	}

	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(WorldItem->RepresentedItem.ItemId);
		TArray<std::tuple<FGameplayTag, int32>> DistributionPlan = GetItemDistributionPlan(
			ItemData, WorldItem->RepresentedItem.Quantity, PreferTaggedSlots);
		for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
		{
			const FGameplayTag& SlotTag = std::get<0>(Plan);
			const int32 QuantityToAddSlot = std::get<1>(Plan);

			if (SlotTag.IsValid())
			{
				RequestedOperationsToServer.Add(
					FRISExpectedOperation(AddTagged, SlotTag, WorldItem->RepresentedItem.ItemId, QuantityToAddSlot));
			}
			else
			{
				RequestedOperationsToServer.Add(
					FRISExpectedOperation(Add, WorldItem->RepresentedItem.ItemId, QuantityToAddSlot));
			}
		}
	}

	PickupItem_Server(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
}

int32 UInventoryComponent::MoveItem(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SourceTaggedSlot,
                                    const FGameplayTag& TargetTaggedSlot,
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
		return MoveItem_ServerImpl(ItemId, Quantity, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId,
		                           SwapQuantity);
	}
}

int32 UInventoryComponent::ValidateMoveItem(const FGameplayTag& ItemId, int32 Quantity,
                                            const FGameplayTag& SourceTaggedSlot, const FGameplayTag& TargetTaggedSlot,
                                            const FGameplayTag& SwapItemId,
                                            int32 SwapQuantity)
{
	return MoveItem_ServerImpl(ItemId, Quantity, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId, SwapQuantity,
	                           true, true);
}


bool UInventoryComponent::CanTaggedSlotReceiveItem(const FItemBundle& ItemInstance,
                                                   const FGameplayTag& SlotTag) const
{
	return IsTaggedSlotCompatible(ItemInstance.ItemId, SlotTag);
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

	// check for compatibility and blocked status
	if (IsTaggedSlotCompatible(ItemId, SlotTag) && !GetItemForTaggedSlot(SlotTag).IsBlocked)
	{
		// Check if slot is blocked
		const FTaggedItemBundle& ItemInSlot = GetItemForTaggedSlot(SlotTag);
		if (ItemInSlot.IsValid() && ItemInSlot.IsBlocked)
		{
			return 0;
		}

		if (WouldItemMoveIndirectlyViolateBlocking(SlotTag, ItemData))
			return 0;

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
	ExtractItemFromTaggedSlot_IfServer(SlotTag, Item.ItemId, QuantityToDrop, EItemChangeReason::Dropped,
	                                   StateArrayToAppendTo);

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

	const UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>(
		UUsableItemDefinition::StaticClass());

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

		UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>(
			UUsableItemDefinition::StaticClass());

		if (!UsableItem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
			return;
		}

		const int32 QuantityToConsume = UsableItem->QuantityPerUse;

		int32 ConsumedCount = RemoveQuantityFromTaggedSlot_IfServer(SlotTag, QuantityToConsume,
		                                                            EItemChangeReason::Consumed, false, true);
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
		TaggedSlotItemInstances[SlotIndex].IsBlocked = IsBlocked;
	}
	else
	{
		// Add the slot with the blocked flag
		TaggedSlotItemInstances.Add(FTaggedItemBundle(Slot, FGameplayTag(), 0));
		TaggedSlotItemInstances.Last().IsBlocked = IsBlocked;
	}
}

bool UInventoryComponent::CanItemBeEquippedInUniversalSlot(const FGameplayTag& ItemId,
                                                           const FUniversalTaggedSlot& Slot, bool IgnoreBlocking) const
{
	UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	int32 Index = GetIndexForTaggedSlot(Slot.Slot);
	return IsTaggedSlotCompatible(ItemId, Slot.Slot) &&
		(IgnoreBlocking || !WouldItemMoveIndirectlyViolateBlocking(Slot.Slot, ItemData)) &&
		TaggedSlotItemInstances.IsValidIndex(Index) &&
		(IgnoreBlocking || !TaggedSlotItemInstances[Index].IsBlocked);
}

bool UInventoryComponent::IsTaggedSlotBlocked(const FGameplayTag& Slot) const
{
	return GetItemForTaggedSlot(Slot).IsBlocked;
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

int32 UInventoryComponent::AddItem_ServerImpl(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId,
                                              int32 RequestedQuantity, bool AllowPartial, bool SuppressUpdate)
{
	return AddItemToAnySlot(ItemSource, ItemId, RequestedQuantity, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, AllowPartial);
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

	if (ContainedInUniversalSlot(SlotTag))
	{
		// loop over UniversalTaggedExclusiveCategoriesSlots and look for matching slot
		for (const FUniversalTaggedSlot& UniSlot : UniversalTaggedSlots)
		{
			// If the item has the exclusive category tag and the slot is not the universal slot, then it is exclusive to some other universal aslot and is not compatible
			if (SlotTag != UniSlot.Slot && UniSlot.ExclusiveToSlotCategory.IsValid() &&
				ItemData->ItemCategories.HasTag(UniSlot.ExclusiveToSlotCategory))
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
	const FTaggedItemBundle* Item = TaggedSlotItemInstances.FindByPredicate([&SlotTag](const FTaggedItemBundle& Item)
	{
		return Item.Tag == SlotTag;
	});
	return Item && Item->IsValid();
}

TArray<std::tuple<FGameplayTag, int32>> UInventoryComponent::GetItemDistributionPlan(
	const UItemStaticData* ItemData, int32 QuantityToAdd, EPreferredSlotPolicy PreferTaggedSlots)
{
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan;

	if (QuantityToAdd <= 0 || !ItemData)
		return DistributionPlan;

	const FGameplayTag& ItemId = ItemData->ItemId;

	// Now remember how many we can add to generic slots, we need to check this before increasing quantity

	int32 TotalQuantityDistributed = 0;
	int32 QuantityDistributedToGenericSlots = 0;

	TArray<FGameplayTag> TaggedSlotsToExclude = TArray<FGameplayTag>(); // We dont want to add to the same tagged slot twice
	if (ItemData->MaxStackSize > 1)
	{
		// First we need to check for any partially filled slots that we can top off first
		for (FTaggedItemBundle& Item : TaggedSlotItemInstances)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;
			if (Item.ItemId == ItemId && !Item.IsBlocked)
			{
				int32 QuantityToAddToSlot = FMath::Min(QuantityToAdd, ItemData->MaxStackSize - Item.Quantity);
				if (QuantityToAddToSlot > 0 && QuantityToAddToSlot < ItemData->MaxStackSize)
				{
					TaggedSlotsToExclude.Add(Item.Tag);
					DistributionPlan.Add(std::make_tuple(Item.Tag, QuantityToAddToSlot));
					TotalQuantityDistributed += QuantityToAddToSlot;
				}
			}
		}

		for (FItemBundleWithInstanceData& Item : ItemsVer.Items)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;
			if (Item.ItemId == ItemId)
			{
				int32 Remainder = Item.Quantity % ItemData->MaxStackSize;
				int32 NeededToFill = (Remainder == 0) ? 0 : (ItemData->MaxStackSize - Remainder);
				int32 QuantityToAddToGeneric = FMath::Min(NeededToFill, QuantityToAdd - TotalQuantityDistributed);
				if (QuantityToAddToGeneric > 0)
				{
					DistributionPlan.Add(std::make_tuple(FGameplayTag::EmptyTag, QuantityToAddToGeneric));
					TotalQuantityDistributed += QuantityToAddToGeneric;
				}
			}
		}
	}
	
	if (PreferTaggedSlots == EPreferredSlotPolicy::PreferGenericInventory)
	{
		const int32 QuantityContainersGenericSlotsCanReceive = Super::GetReceivableQuantityImpl(ItemId);
		// Try adding to generic slots first if not preferring tagged slots
		QuantityDistributedToGenericSlots += FMath::Min(QuantityToAdd - TotalQuantityDistributed, QuantityContainersGenericSlotsCanReceive);
		TotalQuantityDistributed += QuantityDistributedToGenericSlots;
	}

	// Proceed to try adding to tagged slots if PreferTaggedSlots is true or if there's remaining quantity
	if (PreferTaggedSlots != EPreferredSlotPolicy::PreferGenericInventory || TotalQuantityDistributed < QuantityToAdd)
	{
		// if ItemsToAdd is valid that means we haven't extracted the full quantity yet
		for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;
			if (TaggedSlotsToExclude.Contains(SlotTag)) continue;

			int32 AddedToTaggedSlot = FMath::Min(QuantityToAdd - TotalQuantityDistributed,
			                                     GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag));

			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;
			}
		}

		TArray<FGameplayTag> BlockedSlots = TArray<FGameplayTag>();

		// First check universal slots for slots that are strongly preferred by the item
		for (const FUniversalTaggedSlot& SlotTag : UniversalTaggedSlots)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;

			if (ItemData->ItemCategories.HasTag(SlotTag.Slot) && !BlockedSlots.Contains(SlotTag.Slot))
			{
				int32 AddedToTaggedSlot = FMath::Min(QuantityToAdd - TotalQuantityDistributed,
				                                     GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag.Slot));
				if (AddedToTaggedSlot > 0)
				{
					DistributionPlan.Add(std::make_tuple(SlotTag.Slot, AddedToTaggedSlot));
					TotalQuantityDistributed += AddedToTaggedSlot;
					// Add any BlockedSlots
					if (SlotTag.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.HasTag(
						SlotTag.RequiredItemCategoryToBlock))
					{
						BlockedSlots.Add(SlotTag.UniversalSlotToBlock);
					}
				}
			}
		}

		if (PreferTaggedSlots == EPreferredSlotPolicy::PreferSpecializedTaggedSlot && TotalQuantityDistributed <
			QuantityToAdd)
		{
			int32 AddedToDistributedSecondRound = FMath::Min(QuantityToAdd - TotalQuantityDistributed,
			                                                 Super::GetReceivableQuantityImpl(ItemId));
			QuantityDistributedToGenericSlots += AddedToDistributedSecondRound;
			TotalQuantityDistributed += AddedToDistributedSecondRound;
		}

		for (const FUniversalTaggedSlot& SlotTag : UniversalTaggedSlots)
		{
			if (TotalQuantityDistributed >= QuantityToAdd) break;

			if (BlockedSlots.Contains(SlotTag.Slot)) continue;

			int32 AddedToTaggedSlot = FMath::Min(QuantityToAdd - TotalQuantityDistributed,
			                                     GetQuantityOfItemTaggedSlotCanReceive(ItemId, SlotTag.Slot));
			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag.Slot, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;

				if (SlotTag.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.HasTag(
					SlotTag.RequiredItemCategoryToBlock))
				{
					BlockedSlots.Add(SlotTag.UniversalSlotToBlock);
				}
			}
		}
	}

	// Any remaining quantity must be added to generic slots
	int32 FinalAddedtoGenericSlots = QuantityToAdd - TotalQuantityDistributed;
	QuantityDistributedToGenericSlots += FinalAddedtoGenericSlots;
	TotalQuantityDistributed += FinalAddedtoGenericSlots;
	
	if (QuantityDistributedToGenericSlots > 0)
		DistributionPlan.Add(std::make_tuple(FGameplayTag::EmptyTag, QuantityDistributedToGenericSlots));

	ensureMsgf(TotalQuantityDistributed == QuantityToAdd, TEXT("Quantity distributed does not match requested quantity"));
	
	return DistributionPlan;
}

void UInventoryComponent::SortUniversalTaggedSlots()
{
	const int32 NumSlots = UniversalTaggedSlots.Num();
	// Build an adjacency list representing our dependency graph:
	// If slot A can block slot B, then add an edge from A to B.
	TArray<TArray<int32>> Graph;
	Graph.SetNum(NumSlots);

	// Array to keep track of how many dependencies (incoming edges) each slot has.
	TArray<int32> InDegree;
	InDegree.Init(0, NumSlots);

	// Build the graph.
	for (int32 i = 0; i < NumSlots; ++i)
	{
		const FUniversalTaggedSlot& SlotA = UniversalTaggedSlots[i];
		// If this slot doesn't block anything, skip it.
		if (!SlotA.UniversalSlotToBlock.IsValid())
		{
			continue;
		}

		for (int32 j = 0; j < NumSlots; ++j)
		{
			if (i == j)
			{
				continue;
			}
			const FUniversalTaggedSlot& SlotB = UniversalTaggedSlots[j];
			// If SlotA's UniversalSlotToBlock matches SlotB's Slot, then A can block B.
			if (SlotA.UniversalSlotToBlock == SlotB.Slot)
			{
				Graph[i].Add(j);
				InDegree[j]++;
			}
		}
	}

	// Kahn's algorithm: Start with all nodes that have no incoming edges.
	TQueue<int32> Queue;
	for (int32 i = 0; i < NumSlots; ++i)
	{
		if (InDegree[i] == 0)
		{
			Queue.Enqueue(i);
		}
	}

	// This will hold the sorted order of indices.
	TArray<int32> SortedIndices;
	while (!Queue.IsEmpty())
	{
		int32 Index;
		Queue.Dequeue(Index);
		SortedIndices.Add(Index);

		// Remove this node from the graph.
		for (int32 Neighbor : Graph[Index])
		{
			InDegree[Neighbor]--;
			if (InDegree[Neighbor] == 0)
			{
				Queue.Enqueue(Neighbor);
			}
		}
	}

	// Check for cycles (if any, SortedIndices won't contain all indices).
	if (SortedIndices.Num() != NumSlots)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cycle detected in UniversalTaggedSlots dependency graph!"));
		// Handle cycles appropriately here if needed.
		return;
	}

	// Build a new sorted array based on the topological order.
	TArray<FUniversalTaggedSlot> SortedSlots;
	SortedSlots.SetNum(NumSlots);
	for (int32 i = 0; i < NumSlots; ++i)
	{
		SortedSlots[i] = UniversalTaggedSlots[SortedIndices[i]];
	}

	// Replace the original array with the sorted one.
	UniversalTaggedSlots = SortedSlots;
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
			const int32 Removed = DestroyItem_IfServer(Component.ItemId, Component.Quantity,
			                                           EItemChangeReason::Transformed);
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
			const int32 QuantityAdded = Super::AddItem_ServerImpl(Subsystem, CraftedItem.ItemId, CraftedItem.Quantity,
			                                                      false);
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
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(
			FMath::FRand() * 100, FMath::FRand() * 100, 100);
		DropFromTaggedSlot_Server(TaggedSlotItemInstances[i].Tag, TaggedSlotItemInstances[i].Quantity, DropLocation);
		DroppedCount++;
	}

	TaggedSlotItemInstances.Empty();

	for (int i = ItemsVer.Items.Num() - 1; i >= 0; i--)
	{
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(
			FMath::FRand() * 100, FMath::FRand() * 100, 100);
		DropItemFromContainer_Server(ItemsVer.Items[i].ItemId, ItemsVer.Items[i].Quantity, DropLocation);
		DroppedCount++;
	}

	return DroppedCount;
}

int32 UInventoryComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason,
                                           bool AllowPartial, bool UpdateAfter, bool SendEventAfter)
{
	const int32 DestroyedCount = Super::DestroyItemImpl(ItemId, Quantity, Reason, AllowPartial, UpdateAfter,
	                                                    SendEventAfter);
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
			OnItemRemovedFromContainer.Broadcast(ItemData, ContainedQuantityWithoutTaggedSlots,
			                                     EItemChangeReason::ForceDestroyed);
	}

	int Num = TaggedSlotItemInstances.Num();
	for (int i = Num - 1; i >= 0; i--)
	{
		// DestroyFromContainer false to ensure we dont create a bad recursion where the destruction from generic calls into removal from tagged
		RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag, MAX_int32,
		                                      EItemChangeReason::ForceDestroyed, true, false, true);
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

	TArray<FGameplayTag> BlockedSlots = TArray<FGameplayTag>();
	for (const FUniversalTaggedSlot& UniversalSlot : UniversalTaggedSlots)
	{
		if (BlockedSlots.Contains(UniversalSlot.Slot))
			continue;

		int32 QuantityThisSlotCanTake = GetQuantityOfItemTaggedSlotCanReceive(ItemId, UniversalSlot.Slot);
		AcceptableQuantityBySlots += QuantityThisSlotCanTake;
		if (QuantityThisSlotCanTake > 0 && UniversalSlot.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.
			HasTag(UniversalSlot.RequiredItemCategoryToBlock))
		{
			BlockedSlots.Add(UniversalSlot.UniversalSlotToBlock);
		}
	}

	int32 FinalAcceptableQuantity = FMath::Min(AcceptableQuantityByWeight, AcceptableQuantityBySlots);
	if (OnValidateAddItemToContainer.IsBound())
		FinalAcceptableQuantity = FMath::Min(FinalAcceptableQuantity,
		                                     OnValidateAddItemToContainer.Execute(ItemId, FinalAcceptableQuantity));

	return FinalAcceptableQuantity;
}

void UInventoryComponent::OnInventoryItemAddedHandler(const UItemStaticData* ItemData, int32 Quantity,
                                                      EItemChangeReason Reason)
{
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::OnInventoryItemRemovedHandler(const UItemStaticData* ItemData, int32 Quantity,
                                                        EItemChangeReason Reason)
{
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::OnRep_Recipes()
{
	CheckAndUpdateRecipeAvailability();
}

bool UInventoryComponent::ContainedInUniversalSlot(const FGameplayTag& TagToFind) const
{
	return UniversalTaggedSlots.ContainsByPredicate([&TagToFind](const FUniversalTaggedSlot& UniversalSlot)
	{
		return UniversalSlot.Slot == TagToFind;
	});
}
