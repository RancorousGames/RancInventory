// Copyright Rancorous Games, 2024

#include "Components\InventoryComponent.h"

#include <ObjectArray.h>
#include <GameFramework/Actor.h>
#include <Engine/AssetManager.h>

#include "LogRancInventorySystem.h"
#include "Algo/AnyOf.h"
#include "Data/RecipeData.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Data/UsableItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"


UInventoryComponent::UInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	Subsystem(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	bReplicateUsingRegisteredSubObjectList = true;
}

void UInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Subscribe to base class inventory events
	OnItemAddedToContainer.AddDynamic(this, &UInventoryComponent::OnInventoryItemAddedHandler);
	OnItemRemovedFromContainer.AddDynamic(this, &UInventoryComponent::OnInventoryItemRemovedHandler);

	Subsystem = URISSubsystem::Get(this);

	// We sort to help with GetItemDistributionPlan calculations
	// 1. If we have one item that fits in one of 2+ slots but one slot blocks another then we want to make sure we pick the blocking slot before the to-be-blocked slot
	// 2. Sorting might have cycles if left hand can block right hand and right hand can block left hand making a perfect sorting impossible.
	// This can create some slight undesired behavior if we have both 1 and 2.
	SortUniversalTaggedSlots();

	// Initialize available recipes based on initial inventory and recipes
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UInventoryComponent, TaggedSlotItems, SharedParams);

	DOREPLIFETIME(UInventoryComponent, AllUnlockedRecipes);
}

bool UInventoryComponent::CanReceiveItemInTaggedSlot(const FGameplayTag& ItemId, int32 QuantityToReceive, const FGameplayTag& TargetTaggedSlot, bool SwapBackAllowed) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return false;

	int32 ReceivableQuantity = GetReceivableQuantityForTaggedSlot(ItemData, TargetTaggedSlot, QuantityToReceive, false, SwapBackAllowed);

	return ReceivableQuantity >= QuantityToReceive;
}

int32 UInventoryComponent::GetReceivableQuantityForTaggedSlot(const UItemStaticData* ItemData, const FGameplayTag& TargetTaggedSlot, int32 RequestedQuantity, bool AllowPartial, bool AllowSwapback) const
{
	if (BadItemData(ItemData)) return 0;

	if (ContainedInUniversalSlot(TargetTaggedSlot))
	{
		// loop over UniversalTaggedExclusiveCategoriesSlots and look for matching slot
		for (const FUniversalTaggedSlot& UniSlot : UniversalTaggedSlots)
		{
			// If the item has the exclusive category tag and the slot is not the universal slot, then it is exclusive to some other universal slot and is not compatible
			if (TargetTaggedSlot != UniSlot.Slot && UniSlot.ExclusiveToSlotCategory.IsValid() &&
				ItemData->ItemCategories.HasTag(UniSlot.ExclusiveToSlotCategory))
			{
				return 0;
			}
		}
		
		if (!AllowSwapback && WouldItemMoveIndirectlyViolateBlocking(TargetTaggedSlot, ItemData))
		{
			return 0;
		}
	
	}
	else // Specialized tagged slot
	{
		if (!ItemData->ItemCategories.HasTag(TargetTaggedSlot))
			return 0;
	}

	const FTaggedItemBundle& ItemInSlot = GetItemForTaggedSlot(TargetTaggedSlot);
	int32 ViableQuantity =  FMath::Min(ItemData->MaxStackSize, RequestedQuantity);

	if (ItemInSlot.ItemId == ItemData->ItemId)
	{
		if (!AllowSwapback || ItemData->MaxStackSize > 1)
			ViableQuantity = FMath::Min(ViableQuantity,ItemData->MaxStackSize - ItemInSlot.Quantity);
	}
	else if (ItemInSlot.IsValid() && !AllowSwapback)
	{
		return 0;
	}
	
	if (ItemInSlot.IsBlocked && !AllowSwapback)
		return 0;
	
	if (!ItemInSlot.IsValid())
		return ViableQuantity;

	if (!AllowPartial && ViableQuantity < RequestedQuantity)
		return 0;

	return ViableQuantity;
}

int32 UInventoryComponent::GetContainerOnlyItemQuantity(const FGameplayTag& ItemId) const
{
	int32 QuantityInContainer = GetQuantityTotal_Implementation(ItemId);

	for (const FTaggedItemBundle& TaggedSlot : TaggedSlotItems)
	{
		if (TaggedSlot.ItemId == ItemId)
		{
			QuantityInContainer -= TaggedSlot.Quantity;
		}
	}

	return QuantityInContainer;
}

// Override that also accounts for tagged slots
int32 UInventoryComponent::GetReceivableQuantity(const UItemStaticData* ItemData, int32 RequestedQuantity, bool AllowPartial, bool SwapBackAllowed) const
{
	if (BadItemData(ItemData)) return 0;

	const int32 ViableQuantityByWeight = GetQuantityContainerCanReceiveByWeight(ItemData);

	int32 ViableQuantityBySlots = GetQuantityContainerCanReceiveBySlots(ItemData);

	// Then add any available in the tagged slots
	for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
	{
		ViableQuantityBySlots += GetReceivableQuantityForTaggedSlot(ItemData, SlotTag);
	}

	TArray<FGameplayTag> WouldBeBlockedSlots = TArray<FGameplayTag>();
	for (const FUniversalTaggedSlot& UniversalSlot : UniversalTaggedSlots)
	{
		if (WouldBeBlockedSlots.Contains(UniversalSlot.Slot))
			continue;

		int32 QuantityThisSlotCanTake = GetReceivableQuantityForTaggedSlot(ItemData, UniversalSlot.Slot);
		ViableQuantityBySlots += QuantityThisSlotCanTake;
		if (QuantityThisSlotCanTake > 0 && UniversalSlot.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.
			HasTag(UniversalSlot.RequiredItemCategoryToActivateBlocking))
		{
			WouldBeBlockedSlots.Add(UniversalSlot.UniversalSlotToBlock);
		}
	}

	if (SwapBackAllowed && ViableQuantityBySlots == 0) ViableQuantityBySlots = ItemData->MaxStackSize; // If we are allowed to swap back (max 1 item) then we will have the slots at least one stack
	
	int32 FinalViableQuantity =  FMath::Min(RequestedQuantity, FMath::Min(ViableQuantityByWeight, ViableQuantityBySlots));

	return FinalViableQuantity;
}

int32 UInventoryComponent::GetReceivableQuantityContainerOnly(const UItemStaticData* ItemData, int32 RequestedQuantity,
	bool AllowPartial, bool SwapBackAllowed) const
{
	if (BadItemData(ItemData)) return 0;

	const int32 ViableQuantityByWeight = GetQuantityContainerCanReceiveByWeight(ItemData);
	int32 ViableQuantityBySlotCount = GetQuantityContainerCanReceiveBySlots(ItemData);

	if (SwapBackAllowed && ViableQuantityBySlotCount == 0) ViableQuantityBySlotCount = ItemData->MaxStackSize; // If we are allowed to swap back (max 1 item) then we will have the slots at least one stack

	int32 FinalViableQuantity = FMath::Min(ViableQuantityBySlotCount, ViableQuantityByWeight);

	if (!AllowPartial && FinalViableQuantity < RequestedQuantity) return 0;

	return FMath::Min(FinalViableQuantity, RequestedQuantity);
}

int32 UInventoryComponent::ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity,
                                                    const TArray<UItemInstanceData*>& InstancesToExtract,
                                                    EItemChangeReason Reason,
                                                    TArray<UItemInstanceData*>& InstanceArrayToAppendTo,
                                                    bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	// TODO: Ignore quantity if InstancesToExtract is not empty
	// TODO: Check if SuppressUpdate makes sense here. Anyone uses false? Ensure we broadcast itemsaddedtocontainer

	if (AllowPartial && !InstancesToExtract.IsEmpty())
	{
		UE_LOG(LogRISInventory, Error, TEXT("ExtractItemImpl_IfServer: AllowPartial with InstancesToDestroy is not currently supported."));
		return 0;
	}

	
	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(
		ItemId, Quantity, InstancesToExtract, Reason, InstanceArrayToAppendTo, AllowPartial, true);

	if (ExtractedFromContainer <= 0) return 0;
	
	if (!InstancesToExtract.IsEmpty())
	{
		// Remove specific instances
		RemoveItemFromAnyTaggedSlots_IfServer(ItemId, InstancesToExtract.Num(), InstancesToExtract, Reason, false, SuppressUpdate);
	}
	
	const int32 QuantityUnderflow = GetContainerOnlyItemQuantity(ItemId); // Can return negative if we have more tagged slots than container slots
	if (QuantityUnderflow < 0)
	{
		// Remove any non instance specific items
		RemoveItemFromAnyTaggedSlots_IfServer(ItemId, -QuantityUnderflow, NoInstances, Reason, false, SuppressUpdate);
	}

	if (!SuppressEvents)
	{
		if (InstanceArrayToAppendTo.Num() > 0)
		{
			TArray<UItemInstanceData*> ExtractedInstances;
			for (int32 i = 0; i < ExtractedFromContainer; ++i)
				if (UItemInstanceData* InstanceData = InstanceArrayToAppendTo[InstanceArrayToAppendTo.Num() - 1 - i])
					ExtractedInstances.Add(InstanceData);

			OnItemRemovedFromContainer.Broadcast(URISSubsystem::GetItemDataById(ItemId), ExtractedFromContainer,
			                                     ExtractedInstances, Reason);
		}
		else
		{
			OnItemRemovedFromContainer.Broadcast(URISSubsystem::GetItemDataById(ItemId), ExtractedFromContainer,
			                                     NoInstances, Reason);
		}
	}

	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	return ExtractedFromContainer;
}

int32 UInventoryComponent::ExtractItemFromTaggedSlot_IfServer(const FGameplayTag& TaggedSlot,
                                                              const FGameplayTag& ItemId, int32 Quantity,
                                                              const TArray<UItemInstanceData*>& InstancesToExtract,
                                                              EItemChangeReason Reason,
                                                              TArray<UItemInstanceData*>& InstanceArrayToAppendTo)
{
	if (IsClient("ExtractItemFromTaggedSlot_IfServer called on non-authority!")) return 0;

	int32 Index = GetIndexForTaggedSlot(TaggedSlot);
	if (Index == INDEX_NONE || !TaggedSlotItems[Index].Contains(Quantity, InstancesToExtract))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot %s does not contain item %s"), *TaggedSlot.ToString(),
		       *ItemId.ToString());
		return 0;
	}
	
	const int32 ExtractedFromContainer = Super::ExtractItemImpl_IfServer(
		ItemId, Quantity, InstancesToExtract, Reason, InstanceArrayToAppendTo, false, true, true);


	if (ExtractedFromContainer > 0)
		RemoveQuantityFromTaggedSlot_IfServer(TaggedSlot, ExtractedFromContainer, InstancesToExtract, Reason, false, false); // publishes events

	UpdateWeightAndSlots();
	
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
			UniversalSlotDefinition->RequiredItemCategoryToActivateBlocking))
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
		                                     HasTag(UniversalSlotDefinition->RequiredItemCategoryToActivateBlocking))
		{
			ShouldBlock = true;
		}
		SetTaggedSlotBlocked(UniversalSlotDefinition->UniversalSlotToBlock, ShouldBlock);
	}
}


////////////////////////////////////////////////////// TAGGED SLOTS ///////////////////////////////////////////////////////
int32 UInventoryComponent::AddItemToTaggedSlot_IfServer(TScriptInterface<IItemSource> ItemSource,
                                                        const FGameplayTag& SlotTag, const FGameplayTag& ItemId,
                                                        int32 RequestedQuantity, bool AllowPartial, bool PushOutExistingItem)
{
	// Reminder: Items in tagged slots are duplicated in container

	if (IsClient("AddItemsToTaggedSlot_IfServer called on non-authority!")) return 0;
	
	UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return 0;
	
	int32 ViableQuantity = GetReceivableQuantityForTaggedSlot(ItemData, SlotTag, RequestedQuantity, AllowPartial, PushOutExistingItem);

	if (ViableQuantity == 0 || (ViableQuantity < RequestedQuantity && !AllowPartial))
		return 0;
	
	// Attempt to automatically unblock if needed (original logic preserved here before main validation)
	if (WouldItemMoveIndirectlyViolateBlocking(SlotTag, ItemData))
	{
		if (!PushOutExistingItem) return 0;
		
		const FUniversalTaggedSlot* UniversalSlotDefinition = UniversalTaggedSlots.FindByPredicate(
			[&SlotTag](const FUniversalTaggedSlot& UniSlot) { return UniSlot.Slot == SlotTag; });

		if (UniversalSlotDefinition) // Should be non-null if WouldItem... returned true
		{
			auto ExistingItem = GetItemForTaggedSlot(UniversalSlotDefinition->UniversalSlotToBlock);
			if (MoveItem_ServerImpl(ExistingItem.ItemId, ExistingItem.Quantity, NoInstances,
			                        UniversalSlotDefinition->UniversalSlotToBlock,
			                        FGameplayTag::EmptyTag) != ExistingItem.Quantity)
			{
				// We could not unequip the blocking item, so we can't add this item
				UE_LOG(LogRISInventory, Log, TEXT("AddItemsToTaggedSlot_IfServer: Failed to auto-unequip blocking item from %s to allow adding %s to %s."),
					*UniversalSlotDefinition->UniversalSlotToBlock.ToString(), *ItemId.ToString(), *SlotTag.ToString());
				return 0;
			}
			UE_LOG(LogRISInventory, Log, TEXT("AddItemsToTaggedSlot_IfServer: Auto-unequipped item from %s to allow adding %s to %s."),
				*UniversalSlotDefinition->UniversalSlotToBlock.ToString(), *ItemId.ToString(), *SlotTag.ToString());
		}
	}

	// Get existing item in the slot and attempt to move it out
	FTaggedItemBundle* SlotItem = nullptr;
	int32 Index = GetIndexForTaggedSlot(SlotTag);
	if (Index != INDEX_NONE)
	{
		SlotItem = &TaggedSlotItems[Index];
	}
	if (SlotItem && SlotItem->IsValid() && SlotItem->ItemId != ItemId)
	{
		// Move the existing item out of the tagged slot
		int32 PreMoveQuantity = SlotItem->Quantity;
		int32 MovedQuantity = MoveItem_ServerImpl(SlotItem->ItemId, SlotItem->Quantity, NoInstances, SlotTag, FGameplayTag::EmptyTag);
		if (MovedQuantity != PreMoveQuantity)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("AddItemsToTaggedSlot_IfServer: Failed to move existing item %s from %s to container. Aborting tagged slot add."),
				*SlotItem->ItemId.ToString(), *SlotTag.ToString());
			return 0;
		}

		SlotItem = nullptr;
	}

	// Must succeed
	int32 ActualAddedToContainer = Super::AddItemWithInstances_IfServer(ItemSource, ItemId, ViableQuantity, NoInstances, false, true, true);

	// IMPORTANT: If adding to the container failed or added less than expected (e.g., due to weight limit hit JUST before adding to container),
	// we should not proceed to add to the tagged slot visually/logically.
	if (ActualAddedToContainer < ViableQuantity)
		return 0;

	FTaggedItemBundle PreviousItem = SlotItem ? *SlotItem : FTaggedItemBundle();

	if (!SlotItem)
	{
		TaggedSlotItems.Add(FTaggedItemBundle());
		SlotItem = &TaggedSlotItems[TaggedSlotItems.Num() - 1];
		SlotItem->Tag = SlotTag;
		SlotItem->Quantity = 0;
	}
	else if (SlotItem->IsBlocked)
	{
		if (!PushOutExistingItem) return 0;

		// Loop through UniversalTaggedSlots and find the one thats blocking us and try to push it out
		for (const FUniversalTaggedSlot& UniSlot : UniversalTaggedSlots)
		{
			if (UniSlot.UniversalSlotToBlock == SlotTag)
			{
				auto BlockingItem = GetItemForTaggedSlot(UniSlot.Slot);
				bool IsBlockCauser = URISSubsystem::GetItemDataById(BlockingItem.ItemId)->ItemCategories.HasTag(UniSlot.RequiredItemCategoryToActivateBlocking);
				if (!BlockingItem.IsValid() || !IsBlockCauser)
					continue;

				int32 PreMoveQuantity = BlockingItem.Quantity;
				// We have found the item causing the blocking
				int32 QuantityMoved = MoveItem_ServerImpl(BlockingItem.ItemId, BlockingItem.Quantity, NoInstances,
					UniSlot.Slot, FGameplayTag::EmptyTag, false, FGameplayTag::EmptyTag, 0, false, true);

				Index = GetIndexForTaggedSlot(SlotTag);
				if (Index != INDEX_NONE)
					SlotItem = &TaggedSlotItems[Index];
				
				// For some INSANE reason, this fails when both values are 1........
				if (QuantityMoved < PreMoveQuantity)
				{
					// We couldn't kick out the existing item so we have to give up
					return 0;
				}
				break;
			}
		}

		SlotItem->Quantity = 0;
		SlotItem->InstanceData.Empty();
		UpdateBlockingState(SlotTag, ItemData, false);

		ensureMsgf(!SlotItem->IsBlocked,
		           TEXT("AddItemsToTaggedSlot_IfServer: Slot %s remained block after clearing! Multiple items blocking the same slot is not supported"), *SlotTag.ToString());
	}
	// Ensure ItemId is set, especially if the slot was newly created or previously held a different item
	SlotItem->ItemId = ItemId;

	TArray<UItemInstanceData*> AddedInstances;
	if (ItemData->UsesInstances())
	{
		// Fetch the instances that were *actually* added to the container in the Super::AddItem_ServerImpl call
		if (auto* ContainerInstance = FindItemInstance(ItemId)) {
			// Get the last 'ActualAddedToContainer' instances from the container bundle.
			// This assumes AddItem_ServerImpl adds instances to the end.
			int32 NumInContainer = ContainerInstance->InstanceData.Num();
			int32 StartIndex = FMath::Max(0, NumInContainer - ActualAddedToContainer);
			for (int32 i = StartIndex; i < NumInContainer; ++i) {
				if(ContainerInstance->InstanceData.IsValidIndex(i)) { // Extra safety check
					AddedInstances.Add(ContainerInstance->InstanceData[i]);
				} else {
					UE_LOG(LogRISInventory, Error, TEXT("AddItemsToTaggedSlot_IfServer: Invalid instance index %d accessed in container for %s."), i, *ItemId.ToString());
				}
			}
			// Sanity check
			if(AddedInstances.Num() != ActualAddedToContainer && ActualAddedToContainer > 0) {
				 UE_LOG(LogRISInventory, Error, TEXT("AddItemsToTaggedSlot_IfServer: Instance count mismatch. Expected %d, Got %d for %s."), ActualAddedToContainer, AddedInstances.Num(), *ItemId.ToString());
				 // Correct the count based on actual instances fetched
				 ActualAddedToContainer = AddedInstances.Num();
			}
			SlotItem->InstanceData.Append(AddedInstances);
		} else {
			 UE_LOG(LogRISInventory, Error, TEXT("AddItemsToTaggedSlot_IfServer: ContainerInstance for %s not found after Super::AddItem_ServerImpl! Tagged slot instance data will be missing."), *ItemId.ToString());
			 ActualAddedToContainer = 0; // Cannot proceed without container instance
		}
	}

	// If instance fetching failed or adjusted the count, ensure we only add that many
	if (ActualAddedToContainer <= 0) {
		// If the slot was newly created, remove it again.
		if (!PreviousItem.IsValid() && SlotItem) {
			TaggedSlotItems.RemoveSingleSwap(*SlotItem); // Assuming Add creates at end or find index again
		}
		return 0; // Nothing was actually added visually/logically to the tagged slot
	}
	
	SlotItem->Quantity += ActualAddedToContainer;

	UpdateBlockingState(SlotTag, ItemData, true);
	UpdateWeightAndSlots();

	OnItemAddedToTaggedSlot.Broadcast(SlotTag, ItemData, ActualAddedToContainer, AddedInstances, PreviousItem, EItemChangeReason::Added);
	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItems, this);

	return ActualAddedToContainer;
}

int32 UInventoryComponent::AddItemToAnySlot(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId,
                                            int32 RequestedQuantity, EPreferredSlotPolicy PreferTaggedSlots,
                                            bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData || RequestedQuantity <= 0)
	{
		return 0;
	}

	// Use the inventory-specific implementation which considers tagged slots
	int32 ViableQuantity = GetReceivableQuantity(ItemData);

	ViableQuantity = AllowPartial ? FMath::Min(ViableQuantity, RequestedQuantity) : (ViableQuantity >= RequestedQuantity ? RequestedQuantity : 0) ;

	if (ViableQuantity <= 0)
		return 0;

	// Get the distribution plan based on the *actual* quantity we are trying to add.
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan = GetItemDistributionPlan(
		ItemData, ViableQuantity, PreferTaggedSlots);

	// First, add the *total* amount to the base container conceptually. This extracts from the source.
	// Dont allow partial as we already validated any partial amounts so it must succeed
	const int32 ActualAddedToContainer = Super::AddItemWithInstances_IfServer(ItemSource, ItemId, ViableQuantity, NoInstances, false, true, true);

	// If the base container couldn't accept the items (e.g., source dried up unexpectedly), abort.
	if (ActualAddedToContainer < ViableQuantity)
	{
		UE_LOG(LogRISInventory, Error, TEXT("AddItemToAnySlot: AddItem_ServerImpl failed to add %s (Calculated Viable: %d, Actual: %d). Aborting."), *ItemId.ToString(), ViableQuantity, ActualAddedToContainer);
		return 0;
	}
	
	auto* ContainerInstance = FindItemInstanceMutable(ItemId); // Get mutable pointer to the container item bundle
	if (!ContainerInstance && ActualAddedToContainer > 0)
	{
		 UE_LOG(LogRISInventory, Error, TEXT("AddItemToAnySlot: Failed to find container instance for %s after adding %d items! Aborting distribution."), *ItemId.ToString(), ActualAddedToContainer);
		 // Consider how to handle this - maybe just return ActualAddedToContainer as added to generic?
		 return 0; // Abort distribution if container state is inconsistent
	}

	int32 QuantityDistributed = 0;
	int32 QuantityAddedToGenericSlot = 0;
    TArray<UItemInstanceData*> InstancesForGenericSlotsEvent;

	TArray<TTuple<FGameplayTag, int32, TArray<UItemInstanceData*>, FTaggedItemBundle>> TaggedSlotAdditions; // First entry is always generic slot

    int32 InstanceSourceIndex = ContainerInstance ? FMath::Max(0, ContainerInstance->InstanceData.Num() - ActualAddedToContainer) : 0; // Start index for instances added
	
	for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
	{
		const FGameplayTag& SlotTag = std::get<0>(Plan);
		const int32 ViableQuantitySlot = std::get<1>(Plan);
		int32 QuantityRemainingInPlan = ActualAddedToContainer - QuantityDistributed;

		// Defensive check: Don't try to distribute more than what was actually added to the container
		if (ViableQuantitySlot > QuantityRemainingInPlan) {
			 UE_LOG(LogRISInventory, Error, TEXT("AddItemToAnySlot: Distribution plan requests %d for %s, but only %d remain available from container add. Adjusting."), ViableQuantitySlot, SlotTag.IsValid() ? *SlotTag.ToString() : TEXT("Generic"), QuantityRemainingInPlan);
			 // Adjust ViableQuantitySlot if needed, though ideally the plan matches ActualAddedToContainer
			 const_cast<int32&>(ViableQuantitySlot) = QuantityRemainingInPlan; // Modifying tuple element - use with caution or rebuild plan
			 if (ViableQuantitySlot <= 0) continue; // Skip if nothing left to distribute here
		}

		TArray<UItemInstanceData*> InstancesForThisSlot;
		if (ContainerInstance && ItemData->UsesInstances())
		{
			InstancesForThisSlot.Reserve(ViableQuantitySlot);
			int32 EndIndex = FMath::Min(InstanceSourceIndex + ViableQuantitySlot, ContainerInstance->InstanceData.Num());
			for (int32 i = InstanceSourceIndex; i < EndIndex; ++i)
			{
				if(ContainerInstance->InstanceData.IsValidIndex(i)) // Safety check
					InstancesForThisSlot.Add(ContainerInstance->InstanceData[i]);
			}
			InstanceSourceIndex = EndIndex; // Move the index forward

            // Verify fetched instance count matches expected quantity for this slot
			if (InstancesForThisSlot.Num() != ViableQuantitySlot) {
                 UE_LOG(LogRISInventory, Error, TEXT("AddItemToAnySlot: Instance count mismatch for distribution step. Slot: %s, Expected: %d, Got: %d"), SlotTag.IsValid() ? *SlotTag.ToString() : TEXT("Generic"), ViableQuantitySlot, InstancesForThisSlot.Num());
                 // Correct quantity based on actual instances for this step
                 const_cast<int32&>(ViableQuantitySlot) = InstancesForThisSlot.Num();
                 if (ViableQuantitySlot <= 0) continue;
            }
		}

		if (SlotTag.IsValid()) // Target is a tagged slot
		{
			// Suppress events/updates for the internal move, we broadcast consolidated events later
			FTaggedItemBundle PrevItemState = GetItemForTaggedSlot(SlotTag); // Get state *before* move

			// Use MoveItem_ServerImpl to transfer from generic container (SourceTag empty) to the target tagged slot
			const int32 ActualMovedQuantity = MoveItem_ServerImpl(ItemId, ViableQuantitySlot, InstancesForThisSlot, FGameplayTag::EmptyTag,
			                                                      SlotTag, false, FGameplayTag(), 0, true, true, false);

			if (ActualMovedQuantity != ViableQuantitySlot)
			{
				UE_LOG(LogRISInventory, Warning, TEXT("AddItemToAnySlot: Failed internal move of %d/%d %s to tagged slot %s. Items remain in generic."),
					ActualMovedQuantity, ViableQuantitySlot, *ItemId.ToString(), *SlotTag.ToString());
				// Items that failed to move stay in generic. Add them to the generic count.
				QuantityAddedToGenericSlot += ViableQuantitySlot - ActualMovedQuantity;
				// Also collect instances that failed to move for the generic event
				if (!InstancesForThisSlot.IsEmpty()) {
					 // This is tricky - need to know *which* instances failed.
					 // For now, assume all instances intended for this slot failed if ActualMovedQuantity == 0
					 // A more robust way would be needed if partial internal moves were common/expected.
					 if(ActualMovedQuantity == 0) InstancesForGenericSlotsEvent.Append(InstancesForThisSlot);
                     // If partial move, log warning, instances remain associated with generic
                     else UE_LOG(LogRISInventory, Warning, TEXT("AddItemToAnySlot: Partial internal move occurred. Instance tracking for events might be imprecise."));
				}
			} else {
                // Successful move, collect instances for the specific tagged slot event
				TaggedSlotAdditions.Add(TTuple<FGameplayTag, int32, TArray<UItemInstanceData*>, FTaggedItemBundle>(
					SlotTag, ActualMovedQuantity, InstancesForThisSlot, PrevItemState));
            }
			QuantityDistributed += ActualMovedQuantity; // Only count what was actually moved internally
		}
		else // Target is generic slot
		{
			QuantityAddedToGenericSlot += ViableQuantitySlot;
			InstancesForGenericSlotsEvent.Append(InstancesForThisSlot); // Collect instances staying in generic
			QuantityDistributed += ViableQuantitySlot;
		}
	}

	// --- Final Event Broadcasting and Updates ---
	if (!SuppressEvents)
	{
		// Broadcast for items added/remaining in the generic container
		if (QuantityAddedToGenericSlot > 0)
			OnItemAddedToContainer.Broadcast(ItemData, QuantityAddedToGenericSlot, InstancesForGenericSlotsEvent, EItemChangeReason::Added);
		
        // Broadcast for items added to each tagged slot
        for (const auto& Tuple : TaggedSlotAdditions)
        {
             const FGameplayTag& Slot = Tuple.Get<0>();
             int32 Quantity = Tuple.Get<1>();
             const TArray<UItemInstanceData*>& Instances = Tuple.Get<2>();
             const FTaggedItemBundle PrevItem = Tuple.Get<3>();
             OnItemAddedToTaggedSlot.Broadcast(Slot, ItemData, Quantity, Instances, PrevItem, EItemChangeReason::Added);
        }
	}

	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	// Return the total quantity successfully added to the component initially
	return ActualAddedToContainer;
}



void UInventoryComponent::PickupItem_Server_Implementation(AWorldItem* WorldItem,
														   EPreferredSlotPolicy PreferTaggedSlots,
														   bool DestroyAfterPickup)
{
	if (!WorldItem || !WorldItem->RepresentedItem.IsValid()) return;

	FGameplayTag ItemId = WorldItem->RepresentedItem.ItemId;
	int32 QuantityToPickup = WorldItem->RepresentedItem.Quantity;

	// Use AddItemToAnySlot to handle adding the item from the WorldItem source
	// AddItemToAnySlot internally calls AddItem_ServerImpl which calls WorldItem->ExtractItem_IfServer
	/*const int32 QuantityAdded =*/ AddItemToAnySlot(WorldItem, ItemId, QuantityToPickup, PreferTaggedSlots, true);

	// Check the WorldItem's state *after* AddItemToAnySlot has potentially extracted from it
	if (DestroyAfterPickup && WorldItem && !WorldItem->IsGarbageEliminationEnabled()) // Check if already being destroyed
	{
		// Re-check quantity on world item as ExtractItem should have modified it
		int32 RemainingQuantity = WorldItem->GetQuantityTotal_Implementation(ItemId);
		if (RemainingQuantity <= 0)
			WorldItem->Destroy();
	}
}

int32 UInventoryComponent::RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag SlotTag, int32 QuantityToRemove,
                                                                 const TArray<UItemInstanceData*>& InstancesToRemove,
                                                                 EItemChangeReason Reason,
                                                                 bool AllowPartial, bool DestroyFromContainer,
                                                                 bool SuppressEvents, bool SuppressUpdate)
{
	if (IsClient("RemoveQuantityFromTaggedSlot_IfServer")) return 0;

	if (AllowPartial && InstancesToRemove.Num() > 0)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Removing specific instances from tagged slot %s when AllowPartial is true is currently not supported"), *SlotTag.ToString());
		return 0;
	}

	int32 IndexToRemoveAt = GetIndexForTaggedSlot(SlotTag);

	if (IndexToRemoveAt < 0)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("RemoveQuantityFromTaggedSlot_IfServer: Tagged slot %s not found"), *SlotTag.ToString());
		return 0;
	}

	FTaggedItemBundle* TaggedBundleToRemoveFrom = &TaggedSlotItems[IndexToRemoveAt];

	if (!TaggedBundleToRemoveFrom || !TaggedBundleToRemoveFrom->IsValid())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("RemoveQuantityFromTaggedSlot_IfServer: Tagged slot %s is empty"), *SlotTag.ToString());
		return 0;
	}

	if ((!AllowPartial || !InstancesToRemove.IsEmpty()) && !TaggedBundleToRemoveFrom->Contains(QuantityToRemove, InstancesToRemove))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot %s does not contain specified items"), *SlotTag.ToString());
		return 0;
	}

	const int32 ViableQuantity = FMath::Min(QuantityToRemove, TaggedBundleToRemoveFrom->Quantity);

	if (ViableQuantity <= 0)
	{
		return 0;
	}

	const FGameplayTag RemovedId = TaggedBundleToRemoveFrom->ItemId;
	const FGameplayTag RemovedFromTag = TaggedBundleToRemoveFrom->Tag;
	const auto* ItemData = URISSubsystem::GetItemDataById(RemovedId);

	int32 ActualRemovedQuantity;
    const bool bSpecificInstancesTargeted = InstancesToRemove.Num() > 0;

	if (DestroyFromContainer)
	{
		auto* ContainerInstance = FindItemInstance(RemovedId);
		if (!InstancesToRemove.IsEmpty() && !ContainerInstance->Contains(ViableQuantity, InstancesToRemove))
		{
			ensureMsgf(false, TEXT("Container does not contain specified items even though slot %s has it"), *SlotTag.ToString());
			return 0;
		}

		ActualRemovedQuantity = Super::DestroyItemImpl(RemovedId, ViableQuantity, InstancesToRemove, Reason, true, true, true);

		ensureMsgf(InstancesToRemove.IsEmpty() || ActualRemovedQuantity == ViableQuantity,
		           TEXT("Failed to remove all items from tagged slot despite quantity calculated"));
		
		if (ActualRemovedQuantity <= 0)
		{
			return 0;
		}
	}
	else
	{
		// If specific instances are provided then they are contained 
		ActualRemovedQuantity = ViableQuantity;
	}

    // Only modify InstanceData array if it's supposed to have data
	if (TaggedBundleToRemoveFrom->InstanceData.Num() > 0)
	{
		if (bSpecificInstancesTargeted)
		{
            for (UItemInstanceData* Instance : InstancesToRemove) {
                TaggedBundleToRemoveFrom->InstanceData.RemoveSingleSwap(Instance);
            }
            // Ensure quantity matches remaining instances
            TaggedBundleToRemoveFrom->Quantity = FMath::Max(0, TaggedBundleToRemoveFrom->InstanceData.Num());
		}
		// If removing by quantity from an item that uses instance data but we didnt specify which instances
		else
		{
			int32 InstanceToRemoveQuantity = FMath::Min(ActualRemovedQuantity, TaggedBundleToRemoveFrom->InstanceData.Num());
			int32 StartIndex = TaggedBundleToRemoveFrom->InstanceData.Num() - InstanceToRemoveQuantity;
			TaggedBundleToRemoveFrom->InstanceData.RemoveAt(StartIndex, InstanceToRemoveQuantity);
             // Ensure quantity matches remaining instances
            TaggedBundleToRemoveFrom->Quantity = FMath::Max(0, TaggedBundleToRemoveFrom->InstanceData.Num());
		}
	}
	else
	{
		TaggedBundleToRemoveFrom->Quantity -= ActualRemovedQuantity;
	}
	
	if (TaggedBundleToRemoveFrom->Quantity <= 0 && !TaggedBundleToRemoveFrom->IsBlocked)
	{
		TaggedSlotItems.RemoveAt(IndexToRemoveAt);
	}


	if (ItemData)
		UpdateBlockingState(SlotTag, ItemData, false);

	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	if (!SuppressEvents)
		OnItemRemovedFromTaggedSlot.Broadcast(RemovedFromTag, ItemData, ActualRemovedQuantity, InstancesToRemove, Reason);

	MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItems, this);
	return ActualRemovedQuantity;
}

int32 UInventoryComponent::RemoveItemFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove, TArray<UItemInstanceData*> InstancesToRemove,
                                                                 EItemChangeReason Reason, bool DestroyFromContainer, bool SuppressEvents, bool SuppressUpdate)
{
	int32 RemovedCount = 0;
	for (int i = TaggedSlotItems.Num() - 1; i >= 0; i--)
	{
		if (TaggedSlotItems[i].ItemId == ItemId)
		{
			RemovedCount += RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItems[i].Tag,
			                                                      QuantityToRemove - RemovedCount, InstancesToRemove,
			                                                      Reason, true,
			                                                      DestroyFromContainer, SuppressEvents, SuppressUpdate);
			
			if (RemovedCount >= QuantityToRemove || (!InstancesToRemove.IsEmpty() && RemovedCount == InstancesToRemove.Num()))
				break;
			
		}
	}

	return RemovedCount;
}

void UInventoryComponent::MoveItem_Server_Implementation(const FGameplayTag& ItemId, int32 Quantity,
											             const TArray<int32>& InstanceIdsToMove,
                                                         const FGameplayTag& SourceTaggedSlot,
                                                         const FGameplayTag& TargetTaggedSlot,
                                                         const FGameplayTag& SwapItemId,
                                                         int32 SwapQuantity)
{
	const FItemBundle* ItemBundle = FindItemInstance(ItemId);
	MoveItem_ServerImpl(ItemId, Quantity, ItemBundle->FromInstanceIds(InstanceIdsToMove), SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId, SwapQuantity);
}

int32 UInventoryComponent::MoveItem_ServerImpl(const FGameplayTag& ItemId, int32 RequestedQuantity,
                                               TArray<UItemInstanceData*> InstancesToMove,
                                               const FGameplayTag& SourceTaggedSlot,
                                               const FGameplayTag& TargetTaggedSlot, bool AllowAutomaticSwapping,
                                               const FGameplayTag& SwapItemId, int32 SwapQuantity,
                                               bool SuppressEvents,
                                               bool SuppressUpdate,
                                               bool SimulateMoveOnly)
{
	if (IsClient("MoveItemsToTaggedSlot_ServerImpl called on non-authority!")) return 0;
		
	const bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid(); // Keep for logic flow
	const bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid(); // Keep for logic flow

	FGenericItemBundle SourceItem;
	int32 SourceTaggedSlotIndex = -1;
	if (SourceIsTaggedSlot)
	{
		SourceTaggedSlotIndex = GetIndexForTaggedSlot(SourceTaggedSlot);
        // If validation passed, index should be valid. Add defensive check just in case.
        if (!TaggedSlotItems.IsValidIndex(SourceTaggedSlotIndex)) {
            UE_LOG(LogRISInventory, Warning, TEXT("MoveItem_ServerImpl: Tried to move from invalid tagged slot %s"), *SourceTaggedSlot.ToString());
            return 0;
        }
		SourceItem = &TaggedSlotItems[SourceTaggedSlotIndex];
	}
	else
	{
        // Find the item in the container (validation confirmed it exists)
        FItemBundle* FoundSourceInContainer = FindItemInstanceMutable(ItemId);
         if (!FoundSourceInContainer) {
             UE_LOG(LogRISInventory, Warning, TEXT("MoveItem_ServerImpl: Source item %s not found in container."), *ItemId.ToString());
             return 0;
         }
        SourceItem = FoundSourceInContainer;
	}

	bool SwapBackRequested = SwapItemId.IsValid() && SwapQuantity > 0; // Original check

	FGenericItemBundle TargetItem;
	const UItemStaticData* TargetItemData = nullptr; // Keep for logic below
	if (TargetIsTaggedSlot)
	{
		const int32 TargetIndex = GetIndexForTaggedSlot(TargetTaggedSlot); // Find existing or needs adding

		if (!TaggedSlotItems.IsValidIndex(TargetIndex))
		{
            // If validation passed, it means the slot is compatible and exists logically. Add it visually.
			if (SpecializedTaggedSlots.Contains(TargetTaggedSlot) || ContainedInUniversalSlot(TargetTaggedSlot))
			{
				TaggedSlotItems.Add(FTaggedItemBundle(TargetTaggedSlot, FItemBundle::EmptyItemInstance));
				TargetItem = &TaggedSlotItems.Last();
			} else {
                 UE_LOG(LogRISInventory, Error, TEXT("MoveItem_ServerImpl: Target tagged slot %s not configured post-validation."), *TargetTaggedSlot.ToString());
                 return 0;
            }
		}
		else
		{
			TargetItem = &TaggedSlotItems[TargetIndex];
		}

		if (TargetItem.IsValid()) // Check if the fetched/created bundle is valid
		{
			TargetItemData = URISSubsystem::GetItemDataById(TargetItem.GetItemId());
		}
	}
	else // Target is Container
	{
        // Original logic to find target in container
		if (SwapBackRequested)
        {
            FItemBundle* FoundSwapTarget = FindItemInstanceMutable(SwapItemId);
            if (!FoundSwapTarget) {
                 UE_LOG(LogRISInventory, Error, TEXT("MoveItem_ServerImpl: Target container swap item %s not found post-validation."), *SwapItemId.ToString());
                 return 0;
            }
			TargetItem = FoundSwapTarget;
        }
		else
        {
            // Find existing stack of the item being moved
            FItemBundle* FoundStackTarget = FindItemInstanceMutable(ItemId);
            if (!FoundStackTarget) {
                // Item doesn't exist in container yet. Represent conceptually.
                // Subsequent logic (ReceiveExtractedItems) handles actual addition.
                static FItemBundle TempEmptyContainerTarget;
                TempEmptyContainerTarget = FItemBundle(ItemId, 0);
                TargetItem = &TempEmptyContainerTarget;
            } else {
			    TargetItem = FoundStackTarget;
            }
        }

		if (TargetItem.IsValid())
		{
			TargetItemData = URISSubsystem::GetItemDataById(TargetItem.GetItemId());
		}
	}

	if (!SourceItem.IsValid()) return 0;
	auto* SourceItemData = URISSubsystem::GetItemDataById(SourceItem.GetItemId());

	int32 TargetQuantity = TargetItem.GetQuantity();     // Capture pre-move state for events
	if (SwapBackRequested && TargetQuantity < SwapQuantity)
		return 0;
	
	// Allow partial
	int32 ValidatedQuantity = FMath::Min(SourceItem.GetQuantity(), RequestedQuantity);

	if (TargetIsTaggedSlot)
	{
		ValidatedQuantity = FMath::Min(ValidatedQuantity, GetReceivableQuantityForTaggedSlot(SourceItemData, TargetTaggedSlot, RequestedQuantity, true, SwapBackRequested));
	}
	else
	{
		ValidatedQuantity = FMath::Min(ValidatedQuantity, GetReceivableQuantity(SourceItemData, ValidatedQuantity, true, SwapBackRequested));
	}

	// If we are swapping back into a tagged slot then we need to do some additional validation
	if (SwapBackRequested && SourceIsTaggedSlot && GetReceivableQuantityForTaggedSlot(URISSubsystem::GetItemDataById(SwapItemId), SourceTaggedSlot, SwapQuantity, false, true) < SwapQuantity)
	{
		return 0;
	}
	
	if (SimulateMoveOnly) return ValidatedQuantity;
	
	// Now execute the move - Use 'QuantityToMove' from validation result
	//int32 MovedQuantity = FMath::Min(RequestedQuantity, SourceItem.GetQuantity()); // Original calculation - replace RequestedQuantity with QuantityToMove
    // int32 MovedQuantity = QuantityToMove; // Directly use the validated quantity. Min check against source already done by validation.
                                          // Keeping original name `MovedQuantity` for minimal diff, assigned from `QuantityToMove`.
    int32 MovedQuantity = ValidatedQuantity;

	int32 SourceQuantity = SourceItem.GetQuantity();     // Capture pre-move state for events
	const FGameplayTag& SourceItemId = SourceItem.GetItemId(); // Capture pre-move state for events
	const FGameplayTag& TargetItemId = TargetItem.GetItemId(); // Capture pre-move state for events
    TArray<UItemInstanceData*> TargetInstancesBeforeMove = *TargetItem.GetInstances(); // Capture pre-move state

    // SimulateMoveOnly already handled by the validation block replacement

	if (MovedQuantity <= 0) // Should be caught by validation, but keep as safety.
		return 0;

	if (SourceIsTaggedSlot && TargetIsTaggedSlot)
	{
		const FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(
			SourceItem, TargetItem, false, RequestedQuantity, InstancesToMove, true);

		MovedQuantity = MoveResult.QuantityMoved;
		if (MovedQuantity <= 0)
			return 0;
		
		// Note SourceItem and TargetItem are now swapped in content for this code block

		if (!SourceItem.IsValid())
			TaggedSlotItems.RemoveAt(GetIndexForTaggedSlot(SourceTaggedSlot));

		UpdateBlockingState(SourceTaggedSlot, TargetItemData, false);
		UpdateBlockingState(TargetTaggedSlot, SourceItemData, true);
		if (!SuppressEvents)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity, MoveResult.InstancesMoved,
			                                      EItemChangeReason::Moved);
			if (MoveResult.WereItemsSwapped && IsValid(TargetItemData)) // might be null if swapping to empty slot
			{
				OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SourceItem.GetQuantity(), *SourceItem.GetInstances(),
				                                      EItemChangeReason::Moved);
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SourceItem.GetQuantity(), *SourceItem.GetInstances(),
				                                  FTaggedItemBundle(TargetTaggedSlot, SourceItemId,
				                                  SourceQuantity),
				                                  EItemChangeReason::Moved);
			}
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity, MoveResult.InstancesMoved,
			                                  FTaggedItemBundle(TargetTaggedSlot, TargetItemId,
			                                  TargetQuantity), EItemChangeReason::Moved);
		}
	}
	else if (SourceIsTaggedSlot)
	{
		TArray<UItemInstanceData*> InstancesMoved;
		TArray<UItemInstanceData*> SwapBackInstances;
		if (SwapBackRequested) // swap from container to source tagged slot
		{
			SourceItem.SetItemId(SwapItemId);
			SourceItem.SetQuantity(SwapQuantity);
			TArray<UItemInstanceData*>* ContainerInstances = SourceItem.GetInstances();

			if (!ContainerInstances->IsEmpty())
			{
				SwapBackInstances.Reserve(SwapQuantity);
				for (int32 i = ContainerInstances->Num() - 1; SwapBackInstances.Num() < SwapQuantity; --i)
				{
					SwapBackInstances.Add(ContainerInstances->Pop());
				}
				InstancesMoved = InstancesToMove;
				ensureMsgf(SourceItem.GetQuantity() == SwapBackInstances.Num(),
						   TEXT("MoveItem: Source quantity does not match instances new instances"));
			}
			
			if (!SuppressEvents)
				OnItemRemovedFromContainer.Broadcast(TargetItemData, SwapQuantity, SwapBackInstances, EItemChangeReason::Moved);
		}
		else
		{
			SourceItem.SetQuantity(SourceItem.GetQuantity() - MovedQuantity);
			
			if (SourceItem.GetQuantity() <= 0)
			{
				InstancesMoved = *SourceItem.GetInstances();
				TaggedSlotItems.RemoveAt(SourceTaggedSlotIndex);
			}
			else if (!InstancesToMove.IsEmpty())
			{
				TArray<UItemInstanceData*>* CurrentInstances = SourceItem.GetInstances();
				ensureMsgf(CurrentInstances->Num() >= InstancesToMove.Num(),
						   TEXT("MoveItem: Source item does not have enough instances to remove"));
				for (UItemInstanceData* Instance : InstancesToMove)
				{
					CurrentInstances->Remove(Instance);
				}
				ensureMsgf(SourceItem.GetQuantity() == InstancesToMove.Num(),
						   TEXT("MoveItem: new quantity does not match new instances"));
				InstancesMoved = InstancesToMove;
			}
			else if (!SourceItem.GetInstances()->IsEmpty())
			{
				ensureMsgf(SourceItem.GetInstances()->Num() >= MovedQuantity,
				           TEXT("MoveItem: Source item does not have enough instances to remove"));
				for (int32 i = 0; i < MovedQuantity; i++)
				{
					InstancesMoved.Add(SourceItem.GetInstances()->Pop());
				}
			}
		}

		UpdateBlockingState(SourceTaggedSlot, TargetItemData, SwapBackRequested);
		if (!SuppressEvents)
		{
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, SourceItemData, MovedQuantity, InstancesMoved,
			                                      EItemChangeReason::Moved);
			OnItemAddedToContainer.Broadcast(SourceItemData, MovedQuantity,InstancesMoved, EItemChangeReason::Moved);

			if (SwapBackRequested)
				OnItemAddedToTaggedSlot.Broadcast(SourceTaggedSlot, TargetItemData, SwapQuantity, SwapBackInstances,
				                                  FTaggedItemBundle(SourceTaggedSlot, SourceItemId,
				                                                    SourceQuantity, InstancesMoved), EItemChangeReason::Moved);
		}
	}
	else // TargetIsTaggedSlot, source is container
	{
		auto PreviousItem = FTaggedItemBundle(TargetTaggedSlot, TargetItemId, TargetQuantity, *TargetItem.GetInstances());
		auto* TargetInstances = TargetItem.GetInstances();
		if (TargetItem.GetItemId() != ItemId) // If we are swapping or filling a newly added tagged slot
		{
			TargetItem.SetItemId(ItemId);
			TargetItem.SetQuantity(0);
			TargetInstances->Empty();
		}
		TargetItem.SetQuantity(TargetItem.GetQuantity() + MovedQuantity);

		TArray<UItemInstanceData*> MovedInstances;
		if (!SourceItem.GetInstances()->IsEmpty())
		{
			if (InstancesToMove.IsEmpty())
			{
				TArray<UItemInstanceData*>* ContainerInstances = SourceItem.GetInstances();
				
				for (int32 i = ContainerInstances->Num() - 1; TargetInstances->Num() < MovedQuantity; --i)
				{
					TargetInstances->Add((*ContainerInstances)[i]);
					MovedInstances.Add((*ContainerInstances)[i]);
				}
			}
			else
			{
				TargetInstances->Append(InstancesToMove);
				MovedInstances = InstancesToMove;
			}
		}

		ensureMsgf(TargetInstances->IsEmpty() || TargetItem.GetQuantity() == TargetInstances->Num(),
		           TEXT("MoveItem: Target quantity does not match new instances"));
		
		UpdateBlockingState(TargetTaggedSlot, SourceItemData, true);
		
		if (!SuppressEvents)
		{
			if (SwapItemId.IsValid() && SwapQuantity > 0 && !SuppressEvents)
			{
				ensureMsgf(SwapQuantity == TargetQuantity,
				           TEXT("Requested swap did not swap all of target item"));
				ensureMsgf(!SourceIsTaggedSlot || MovedQuantity == SourceQuantity,
				           TEXT("Requested swap did not swap all of tagged source item"));
				// Notify of the first part of the swap (we dont actually need to do any moving as its going to get overwritten anyway)
				OnItemRemovedFromTaggedSlot.Broadcast(TargetTaggedSlot, TargetItemData, SwapQuantity, PreviousItem.InstanceData,
				                                      EItemChangeReason::Moved);
				OnItemAddedToContainer.Broadcast(TargetItemData, SwapQuantity, PreviousItem.InstanceData, EItemChangeReason::Moved);
			}
			
			OnItemRemovedFromContainer.Broadcast(SourceItemData, MovedQuantity, MovedInstances, EItemChangeReason::Moved);
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, SourceItemData, MovedQuantity, MovedInstances, PreviousItem,
			                                  EItemChangeReason::Moved);
		}
	}

	if (MovedQuantity > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UInventoryComponent, TaggedSlotItems, this);
	}

	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	return MovedQuantity;
}

void UInventoryComponent::MoveBetweenContainers_ServerImpl(UItemContainerComponent* SourceComponent,
												           UItemContainerComponent* TargetComponent,
												           const FGameplayTag& ItemId,
												           int32 Quantity,
												           const TArray<int32>& InstanceIdsToMove,
												           const FGameplayTag& SourceTaggedSlot,
												           const FGameplayTag& TargetTaggedSlot)
{
    if (!TargetComponent || !IsValid(TargetComponent) || Quantity <= 0 || !ItemId.IsValid()) return;
	
    FGenericItemBundle SourceBundleWrapper;
    FTaggedItemBundle* FoundTaggedBundlePtr = nullptr;

    if (SourceTaggedSlot.IsValid())
    {
        UInventoryComponent* SourceInventoryComp = Cast<UInventoryComponent>(SourceComponent);
        if (!SourceInventoryComp) return;
        int32 SourceIdx = SourceInventoryComp->GetIndexForTaggedSlot(SourceTaggedSlot);
        if (!SourceInventoryComp->TaggedSlotItems.IsValidIndex(SourceIdx)) return;
        FoundTaggedBundlePtr = &SourceInventoryComp->TaggedSlotItems[SourceIdx];
        SourceBundleWrapper = FGenericItemBundle(FoundTaggedBundlePtr);
    }
    else
    {
	    FItemBundle* FoundGridBundlePtr = nullptr;
	    FoundGridBundlePtr = SourceComponent->FindItemInstanceMutable(ItemId);
        if (!FoundGridBundlePtr) return;
        SourceBundleWrapper = FGenericItemBundle(FoundGridBundlePtr);
    }

    if (!SourceBundleWrapper.IsValid() || SourceBundleWrapper.GetItemId() != ItemId) return;

    TArray<UItemInstanceData*>* SourceInstanceDataArrayPtr = SourceBundleWrapper.GetInstances();
    if (!SourceInstanceDataArrayPtr) return;

    TArray<UItemInstanceData*> InstancesToMovePtrs;
    if (!InstanceIdsToMove.IsEmpty())
    {
        InstancesToMovePtrs = SourceBundleWrapper.FromInstanceIds(InstanceIdsToMove);
        if (InstancesToMovePtrs.Num() != InstanceIdsToMove.Num()) return;
    }

    int32 QuantityToExtract = InstancesToMovePtrs.IsEmpty() ? Quantity : InstancesToMovePtrs.Num();
    if (SourceBundleWrapper.GetQuantity() < QuantityToExtract) return; // Check quantity before extraction

    TArray<UItemInstanceData*> ExtractedInstances;
    int32 ExtractedQuantity = 0;
    EItemChangeReason ExtractReason = EItemChangeReason::Transferred;

	auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
    if (TargetTaggedSlot.IsValid())
	{
    	UInventoryComponent* TargetInventoryComp = Cast<UInventoryComponent>(TargetComponent);
    	ensureMsgf(TargetInventoryComp, TEXT("RequestMoveItemToOtherContainer_Server: TargetTaggedSlot specified, but TargetComponent is not a UInventoryComponent. Move failed."));
		// Target is an Inventory, use its more specific validation
		QuantityToExtract = TargetInventoryComp->GetReceivableQuantityForTaggedSlot(ItemData, TargetTaggedSlot, QuantityToExtract, true, true);
	}
	else
	{
		// Target is a basic Container
		if (TargetTaggedSlot.IsValid())
		{
			UE_LOG(LogRISInventory, Error, TEXT("RequestMoveItemToOtherContainer_Server: TargetTaggedSlot specified, but TargetComponent is not a UInventoryComponent. Move failed."));
			return; // Cannot move to tagged slot of basic container
		}
		// Validate against the generic container part using base validation
		QuantityToExtract = TargetComponent->GetReceivableQuantity(ItemData, QuantityToExtract, true, true);
	}
	
    if (FoundTaggedBundlePtr)
    {
        UInventoryComponent* SourceInventoryComp = CastChecked<UInventoryComponent>(SourceComponent);
        ExtractedQuantity = SourceInventoryComp->ExtractItemFromTaggedSlot_IfServer(
            SourceTaggedSlot, ItemId, QuantityToExtract, InstancesToMovePtrs, ExtractReason, ExtractedInstances);
    }
    else
    {
        ExtractedQuantity = SourceComponent->ExtractItemImpl_IfServer(
            ItemId, QuantityToExtract, InstancesToMovePtrs, ExtractReason, ExtractedInstances, false, false, false);
    }

    if (ExtractedQuantity > 0)
    {
        int32 ActuallyAdded = 0;
        if (TargetTaggedSlot.IsValid())
        {
            UInventoryComponent* TargetInventoryComp = Cast<UInventoryComponent>(TargetComponent);
            if (!TargetInventoryComp)
            {
                SourceComponent->SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, ExtractedQuantity, FVector(1e+300, 0,0), ExtractedInstances);
                return;
            }

            // Add extracted items to the target's generic container first
            int32 ReceivedByTargetContainer = TargetInventoryComp->ReceiveExtractedItems_IfServer(ItemId, ExtractedQuantity, ExtractedInstances, false);
            if(ReceivedByTargetContainer > 0)
            {
                 TArray<UItemInstanceData*> InstancesActuallyInTargetContainer;
                 if(const FItemBundle* TargetContainerBundle = TargetInventoryComp->FindItemInstance(ItemId)) {
                     TSet<UItemInstanceData*> ExtractedSet(ExtractedInstances);
                     for(UItemInstanceData* InstInTarget : TargetContainerBundle->InstanceData) {
                         if(ExtractedSet.Contains(InstInTarget)) InstancesActuallyInTargetContainer.Add(InstInTarget);
                     }
                 }

                 // Move the successfully received items internally to the target tagged slot
                 ActuallyAdded = TargetInventoryComp->MoveItem_ServerImpl(
                     ItemId, ReceivedByTargetContainer, InstancesActuallyInTargetContainer, FGameplayTag(), TargetTaggedSlot,
                     false, FGameplayTag(), 0, false, false );

                 if(ActuallyAdded != ReceivedByTargetContainer)
                 {
	                 UE_LOG(LogRISInventory, Error, TEXT("RequestMove: Failed internal move generic->tagged in target despite validation.."));

                 	// Remove ActuallyAdded from InstancesActuallyInTargetContainer
                 	for (int32 i = InstancesActuallyInTargetContainer.Num() - 1; ActuallyAdded > 0 && i >= 0; --i)
				 		if (InstancesActuallyInTargetContainer[i])
				 		{
				 			InstancesActuallyInTargetContainer.RemoveAt(i);
				 			--ActuallyAdded;
				 		}
                 	
                 	TargetInventoryComp->DestroyItemImpl(ItemId, ReceivedByTargetContainer - ActuallyAdded, InstancesActuallyInTargetContainer, EItemChangeReason::Moved, true, true, true);
                 }
            }
            else {
                 SourceComponent->SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, ExtractedQuantity, FVector(1e+300, 0,0), ExtractedInstances);
                 return;
            }
        }
        else
        {
             ActuallyAdded = TargetComponent->ReceiveExtractedItems_IfServer(ItemId, ExtractedQuantity, ExtractedInstances);
        }

        if (ActuallyAdded < ExtractedQuantity)
        {
            int32 QuantityToReturnOrDrop = ExtractedQuantity - ActuallyAdded;
            TArray<UItemInstanceData*> InstancesToReturnOrDrop;
            TSet<UItemInstanceData*> AddedInstanceSet; // Determine which instances were successfully added to target
             const FItemBundle* FinalTargetBundle = TargetComponent->FindItemInstance(ItemId);
             if(FinalTargetBundle) AddedInstanceSet.Append(FinalTargetBundle->InstanceData);
             if(TargetTaggedSlot.IsValid()) {
	             if(UInventoryComponent* TargetInv = Cast<UInventoryComponent>(TargetComponent)) {
                       const FTaggedItemBundle* FinalTargetTaggedBundle = TargetInv->TaggedSlotItems.FindByPredicate([&](const FTaggedItemBundle& B){ return B.Tag == TargetTaggedSlot; });
                       if(FinalTargetTaggedBundle && FinalTargetTaggedBundle->IsValid()) AddedInstanceSet.Append(FinalTargetTaggedBundle->InstanceData);
                  }
             }
             for(UItemInstanceData* ExtractedInst : ExtractedInstances) {
                 if(!AddedInstanceSet.Contains(ExtractedInst)) InstancesToReturnOrDrop.Add(ExtractedInst);
             }

            // Attempt to return the leftovers to the source component (this)
            int32 ReturnedToSource = SourceComponent->ReceiveExtractedItems_IfServer(ItemId, QuantityToReturnOrDrop, InstancesToReturnOrDrop);
            if (ReturnedToSource < QuantityToReturnOrDrop)
            {
                TArray<UItemInstanceData*> InstancesToDrop;
                TSet<UItemInstanceData*> ReturnedInstanceSet; // Determine which instances were successfully returned
                 const FItemBundle* FinalSourceBundleGeneric = SourceComponent->FindItemInstance(ItemId);
                 if(FinalSourceBundleGeneric) ReturnedInstanceSet.Append(FinalSourceBundleGeneric->InstanceData);
                 if(SourceTaggedSlot.IsValid()) {
	                 if(UInventoryComponent* SourceInv = Cast<UInventoryComponent>(SourceComponent)) {
                           const FTaggedItemBundle* FinalSourceTaggedBundle = SourceInv->TaggedSlotItems.FindByPredicate([&](const FTaggedItemBundle& B){ return B.Tag == SourceTaggedSlot; });
                           if(FinalSourceTaggedBundle && FinalSourceTaggedBundle->IsValid()) ReturnedInstanceSet.Append(FinalSourceTaggedBundle->InstanceData);
                      }
                 }
                 for(UItemInstanceData* InstToReturn : InstancesToReturnOrDrop) {
                     if(!ReturnedInstanceSet.Contains(InstToReturn)) InstancesToDrop.Add(InstToReturn);
                 }
                SourceComponent->SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, InstancesToDrop.Num(), FVector(1e+300, 0,0), InstancesToDrop);
            }
            else if (SourceTaggedSlot.IsValid() && ReturnedToSource > 0)
            {
                 UInventoryComponent* SourceInventoryComp = CastChecked<UInventoryComponent>(SourceComponent);
                 TArray<UItemInstanceData*> ActualReturnedInstances; // Determine which specific instances made it back to generic
                 if(const FItemBundle* ReturnedBundle = SourceComponent->FindItemInstance(ItemId)) {
                       TSet<UItemInstanceData*> InstToReturnSet(InstancesToReturnOrDrop);
                       for(UItemInstanceData* ReturnedInst : ReturnedBundle->InstanceData) {
                            if(InstToReturnSet.Contains(ReturnedInst)) ActualReturnedInstances.Add(ReturnedInst);
                       }
                  }
                 int32 MovedBackToTagged = SourceInventoryComp->MoveItem_ServerImpl(ItemId, ReturnedToSource, ActualReturnedInstances, FGameplayTag(), SourceTaggedSlot, false, FGameplayTag(), 0, false, false);
                 if (MovedBackToTagged != ReturnedToSource) UE_LOG(LogRISInventory, Warning, TEXT("RequestMove: Failed internal move generic->tagged in source after return."));
            }
        }
    }
}


void UInventoryComponent::PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots,
                                     bool DestroyAfterPickup)
{
	if (!IsValid(WorldItem))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("PickupItem called with null world item"));
		return;
	}

	if (IsClient())
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(WorldItem->RepresentedItem.ItemId);
		TArray<std::tuple<FGameplayTag, int32>> DistributionPlan = GetItemDistributionPlan(
			ItemData, WorldItem->RepresentedItem.Quantity, PreferTaggedSlots);
		for (const std::tuple<FGameplayTag, int32>& Plan : DistributionPlan)
		{
			const FGameplayTag& SlotTag = std::get<0>(Plan);
			const int32 ViableQuantitySlot = std::get<1>(Plan);

			if (SlotTag.IsValid())
			{
				RequestedOperationsToServer.Add(
					FRISExpectedOperation(AddTagged, SlotTag, WorldItem->RepresentedItem.ItemId, ViableQuantitySlot));
			}
			else
			{
				RequestedOperationsToServer.Add(
					FRISExpectedOperation(Add, WorldItem->RepresentedItem.ItemId, ViableQuantitySlot));
			}
		}
	}

	PickupItem_Server(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
}

int32 UInventoryComponent::MoveItem(const FGameplayTag& ItemId, int32 Quantity,
	                                TArray<UItemInstanceData*> InstancesToMove,
                                    const FGameplayTag& SourceTaggedSlot,
                                    const FGameplayTag& TargetTaggedSlot,
                                    const FGameplayTag& SwapItemId, int32 SwapQuantity)
{
	if (IsClient())
	{
		// TODO: Make sure tests dont rely on return value so we can set return type to void
		MoveItem_Server(ItemId, Quantity, FItemBundle::ToInstanceIds(InstancesToMove), SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
		return -1;
	}
	else
	{
		return MoveItem_ServerImpl(ItemId, Quantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId,
		                           SwapQuantity);
	}
}

int32 UInventoryComponent::ValidateMoveItem(const FGameplayTag& ItemId, int32 Quantity,
                                            const TArray<UItemInstanceData*>& InstancesToMove,
                                            const FGameplayTag& SourceTaggedSlot, const FGameplayTag& TargetTaggedSlot,
                                            const FGameplayTag& SwapItemId,
                                            int32 SwapQuantity)
{
	return MoveItem_ServerImpl(ItemId, Quantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, true, SwapItemId, SwapQuantity,
	                           true, true, true);
}

bool UInventoryComponent::ContainsInTaggedSlot_BP(const FGameplayTag& SlotTag, const FGameplayTag& ItemId, int32 Quantity) const
{
    // Calls the virtual function with an empty instance array for quantity check
    return ContainsInTaggedSlot(SlotTag, ItemId, Quantity, NoInstances);
}

bool UInventoryComponent::ContainsInTaggedSlot(const FGameplayTag& SlotTag, const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToLookFor) const
{
    if (!SlotTag.IsValid())
    {
        return false;
    }

    const FTaggedItemBundle& ItemInSlot = GetItemForTaggedSlot(SlotTag);

    if (!ItemInSlot.IsValid() || ItemInSlot.ItemId != ItemId)
    {
        // Slot is empty, invalid, or contains the wrong item
         return (Quantity <= 0 && InstancesToLookFor.Num() == 0); // Only true if asking for nothing
    }

    // Use the FTaggedItemBundle::Contains helper function
    return ItemInSlot.Contains(Quantity, InstancesToLookFor);
}

int32 UInventoryComponent::RemoveAnyItemFromTaggedSlot_IfServer(FGameplayTag SlotTag)
{
	if (IsClient("ClearTaggedSlot_IfServer called on client for %s. Request ignored.")) return 0;

	// Find the item in the specified slot
	const FTaggedItemBundle& ItemInSlot = GetItemForTaggedSlot(SlotTag);

	// If the slot is empty or the item is invalid, there's nothing to clear
	if (!ItemInSlot.IsValid())
	{
		// UE_LOG(LogRISInventory, Verbose, TEXT("ClearTaggedSlot_IfServer: Slot %s is already empty or invalid."), *SlotTag.ToString());
		return 0;
	}

	const FGameplayTag ItemIdToMove = ItemInSlot.ItemId;
	const int32 QuantityToMove = ItemInSlot.Quantity;

	// Use the existing MoveItem implementation to handle the logic.
	// Source is the tagged slot, Target is the generic container (empty tag).
	// We don't want automatic swapping.
	int32 QuantityActuallyMoved = MoveItem_ServerImpl(
		ItemIdToMove,
		QuantityToMove,
		NoInstances,
		SlotTag,          // Source Slot
		FGameplayTag(),   // Target Slot (Empty = Generic Container)
		false,            // AllowAutomaticSwapping 
		FGameplayTag(),   // SwapItemId 
		0                 // SwapQuantity 
	);

	if (QuantityActuallyMoved < QuantityToMove && QuantityActuallyMoved > 0)
	{
		UE_LOG(LogRISInventory, Log, TEXT("ClearTaggedSlot_IfServer: Partially cleared slot %s. Moved %d/%d of %s. Container likely full."),
			*SlotTag.ToString(), QuantityActuallyMoved, QuantityToMove, *ItemIdToMove.ToString());
	}
	else if (QuantityActuallyMoved == 0 && QuantityToMove > 0)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("ClearTaggedSlot_IfServer: Failed to clear slot %s containing %d of %s. Container likely full or move rejected."),
			*SlotTag.ToString(), QuantityToMove, *ItemIdToMove.ToString());
	}

	return QuantityActuallyMoved;
}

int32 UInventoryComponent::UseItemFromTaggedSlot(const FGameplayTag& SlotTag, int32 ItemToUseInstanceId)
{
	
	// On client the below is just a guess
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (!Item.IsValid()) return 0;

	if (ItemToUseInstanceId >= 0 &&	!Algo::AnyOf(Item.InstanceData, [=](const UItemInstanceData* Instance) { return Instance->UniqueInstanceId == ItemToUseInstanceId; }))
		return 0;
		
	auto ItemId = Item.ItemId;

	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return 0;

	const UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

	if (!UsableItem)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return 0;
	}

	int32 QuantityToRemove = UsableItem->QuantityPerUse;

	if (IsClient())
		RequestedOperationsToServer.Add(FRISExpectedOperation(RemoveTagged, SlotTag, QuantityToRemove));

	UseItemFromTaggedSlot_Server(SlotTag, ItemToUseInstanceId);

	return QuantityToRemove;
}


void UInventoryComponent::UseItemFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag, int32 ItemToUseInstanceId)
{
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (Item.Tag.IsValid())
	{
		UItemInstanceData* const* FoundPtr = (ItemToUseInstanceId >= 0)
			? Item.InstanceData.FindByPredicate([=](const UItemInstanceData* Instance) { return Instance->UniqueInstanceId == ItemToUseInstanceId; })
			: nullptr;
		UItemInstanceData* ItemInstance = FoundPtr ? *FoundPtr : nullptr;

		if (!ItemInstance) return;
		
		const auto ItemId = Item.ItemId;
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		
		if (BadItemData(ItemData, ItemId)) return;

		UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

		if (!UsableItem)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
			return;
		}

		const int32 QuantityToConsume = UsableItem->QuantityPerUse;

		int32 ConsumedCount = RemoveQuantityFromTaggedSlot_IfServer(SlotTag, QuantityToConsume, TArray{ItemInstance},
		                                                            EItemChangeReason::Consumed, false, true);
		if (ConsumedCount > 0 || UsableItem->QuantityPerUse == 0)
		{
			UsableItem->Use(GetOwner(), ItemData, ItemInstance);
		}
	}
}

const FTaggedItemBundle& UInventoryComponent::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	int32 Index = GetIndexForTaggedSlot(SlotTag);

	if (Index < 0 || Index >= TaggedSlotItems.Num())
	{
		return FTaggedItemBundle::EmptyItemInstance;
	}

	return TaggedSlotItems[Index];
}

void UInventoryComponent::SetTaggedSlotBlocked(FGameplayTag Slot, bool IsBlocked)
{
	int32 SlotIndex = GetIndexForTaggedSlot(Slot);
	if (SlotIndex >= 0)
	{
		TaggedSlotItems[SlotIndex].IsBlocked = IsBlocked;
	}
	else
	{
		// Add the slot with the blocked flag
		TaggedSlotItems.Add(FTaggedItemBundle(Slot, FGameplayTag(), 0));
		TaggedSlotItems.Last().IsBlocked = IsBlocked;
	}
}

bool UInventoryComponent::IsTaggedSlotBlocked(const FGameplayTag& Slot) const
{
	return GetItemForTaggedSlot(Slot).IsBlocked;
}

int32 UInventoryComponent::GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const
{
	// loop over SpecialSlotItems
	for (int i = 0; i < TaggedSlotItems.Num(); i++)
	{
		if (TaggedSlotItems[i].Tag == SlotTag)
			return i;
	}

	return -1;
}

int32 UInventoryComponent::AddItemWithInstances_IfServer(TScriptInterface<IItemSource> ItemSource,
		const FGameplayTag& ItemId,
		int32 RequestedQuantity,
		const TArray<UItemInstanceData*>& InstancesToExtract,
		bool AllowPartial,
		bool SuppressEvents,
		bool SuppressUpdate)
{
	return AddItemToAnySlot(ItemSource, ItemId, RequestedQuantity, EPreferredSlotPolicy::PreferSpecializedTaggedSlot,
	                        AllowPartial, SuppressEvents, SuppressUpdate);
}

TArray<FTaggedItemBundle> UInventoryComponent::GetAllTaggedItems() const
{
	return TaggedSlotItems;
}

void UInventoryComponent::DetectAndPublishContainerChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	/*for (FTaggedItemBundle& NewItem : TaggedSlotItems)
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

TArray<std::tuple<FGameplayTag, int32>> UInventoryComponent::GetItemDistributionPlan(
	const UItemStaticData* ItemData, int32 ViableQuantity, EPreferredSlotPolicy PreferTaggedSlots)
{
	TArray<std::tuple<FGameplayTag, int32>> DistributionPlan;

	if (ViableQuantity <= 0 || !ItemData)
		return DistributionPlan;

	const FGameplayTag& ItemId = ItemData->ItemId;

	// Now remember how many we can add to generic slots, we need to check this before increasing quantity

	int32 TotalQuantityDistributed = 0;
	int32 QuantityDistributedToGenericSlots = 0;

	TArray<FGameplayTag> TaggedSlotsToExclude = TArray<FGameplayTag>();
	// We dont want to add to the same tagged slot twice
	if (ItemData->MaxStackSize > 1)
	{
		// First we need to check for any partially filled slots that we can top off first
		for (FTaggedItemBundle& Item : TaggedSlotItems)
		{
			if (TotalQuantityDistributed >= ViableQuantity) break;
			if (Item.ItemId == ItemId && !Item.IsBlocked)
			{
				int32 ViableQuantityToSlot = FMath::Min(ViableQuantity, ItemData->MaxStackSize - Item.Quantity);
				if (ViableQuantityToSlot > 0 && ViableQuantityToSlot < ItemData->MaxStackSize)
				{
					TaggedSlotsToExclude.Add(Item.Tag);
					DistributionPlan.Add(std::make_tuple(Item.Tag, ViableQuantityToSlot));
					TotalQuantityDistributed += ViableQuantityToSlot;
				}
			}
		}

		for (FItemBundle& Item : ItemsVer.Items)
		{
			if (TotalQuantityDistributed >= ViableQuantity) break;
			if (Item.ItemId == ItemId)
			{
				int32 Remainder = Item.Quantity % ItemData->MaxStackSize;
				int32 NeededToFill = (Remainder == 0) ? 0 : (ItemData->MaxStackSize - Remainder);
				int32 ViableQuantityToGeneric = FMath::Min(NeededToFill, ViableQuantity - TotalQuantityDistributed);
				if (ViableQuantityToGeneric > 0)
				{
					DistributionPlan.Add(std::make_tuple(FGameplayTag::EmptyTag, ViableQuantityToGeneric));
					TotalQuantityDistributed += ViableQuantityToGeneric;
				}
			}
		}
	}
	
	const int32 QuantityContainersGenericSlotsCanReceive = Super::GetReceivableQuantity(ItemData); // this only considers slotcount=generic slots for slot limits

	if (PreferTaggedSlots == EPreferredSlotPolicy::PreferGenericInventory)
	{
		// Try adding to generic slots first if not preferring tagged slots
		QuantityDistributedToGenericSlots += FMath::Min(ViableQuantity - TotalQuantityDistributed,
		                                                QuantityContainersGenericSlotsCanReceive);
		TotalQuantityDistributed += QuantityDistributedToGenericSlots;
	}

	// Proceed to try adding to tagged slots if PreferTaggedSlots is true or if there's remaining quantity
	if (PreferTaggedSlots != EPreferredSlotPolicy::PreferGenericInventory || TotalQuantityDistributed < ViableQuantity)
	{
		// if ItemsToAdd is valid that means we haven't extracted the full quantity yet
		for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
		{
			if (TotalQuantityDistributed >= ViableQuantity) break;
			if (TaggedSlotsToExclude.Contains(SlotTag)) continue;

			int32 AddedToTaggedSlot = FMath::Min(ViableQuantity - TotalQuantityDistributed,
			                                     GetReceivableQuantityForTaggedSlot(ItemData, SlotTag));

			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;
			}
		}

		TArray<FGameplayTag> BlockedSlots = TArray<FGameplayTag>(); // Some items in universal slots can block others, e.g. two handed in mainhand blocks offhand

		// First check universal slots for slots that are strongly preferred by the item
		for (const FUniversalTaggedSlot& SlotTag : UniversalTaggedSlots)
		{
			if (TotalQuantityDistributed >= ViableQuantity) break;

			if (ItemData->ItemCategories.HasTag(SlotTag.Slot) && !BlockedSlots.Contains(SlotTag.Slot))
			{
				int32 AddedToTaggedSlot = FMath::Min(ViableQuantity - TotalQuantityDistributed,
				                                     GetReceivableQuantityForTaggedSlot(ItemData, SlotTag.Slot));
				if (AddedToTaggedSlot > 0)
				{
					DistributionPlan.Add(std::make_tuple(SlotTag.Slot, AddedToTaggedSlot));
					TotalQuantityDistributed += AddedToTaggedSlot;
					// Add any BlockedSlots
					if (SlotTag.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.HasTag(
						SlotTag.RequiredItemCategoryToActivateBlocking))
					{
						BlockedSlots.Add(SlotTag.UniversalSlotToBlock);
					}
				}
			}
		}

		if (PreferTaggedSlots == EPreferredSlotPolicy::PreferSpecializedTaggedSlot && TotalQuantityDistributed <
			ViableQuantity)
		{
			int32 AddedToDistributedSecondRound = FMath::Min(ViableQuantity - TotalQuantityDistributed,
			                                                 Super::GetReceivableQuantity(ItemData));
			QuantityDistributedToGenericSlots += AddedToDistributedSecondRound;
			TotalQuantityDistributed += AddedToDistributedSecondRound;
		}

		for (const FUniversalTaggedSlot& SlotTag : UniversalTaggedSlots)
		{
			if (TotalQuantityDistributed >= ViableQuantity) break;

			if (BlockedSlots.Contains(SlotTag.Slot)) continue;

			int32 AddedToTaggedSlot = FMath::Min(ViableQuantity - TotalQuantityDistributed,
			                                     GetReceivableQuantityForTaggedSlot(ItemData, SlotTag.Slot));
			if (AddedToTaggedSlot > 0)
			{
				DistributionPlan.Add(std::make_tuple(SlotTag.Slot, AddedToTaggedSlot));
				TotalQuantityDistributed += AddedToTaggedSlot;

				if (SlotTag.UniversalSlotToBlock.IsValid() && ItemData->ItemCategories.HasTag(
					SlotTag.RequiredItemCategoryToActivateBlocking))
				{
					BlockedSlots.Add(SlotTag.UniversalSlotToBlock);
				}
			}
		}
	}

	// Any remaining quantity must be added to generic slots
	int32 FinalAddedtoGenericSlots = ViableQuantity - TotalQuantityDistributed;
	QuantityDistributedToGenericSlots += FinalAddedtoGenericSlots;
	TotalQuantityDistributed += FinalAddedtoGenericSlots;

	if (QuantityDistributedToGenericSlots > 0)
		DistributionPlan.Add(std::make_tuple(FGameplayTag::EmptyTag, QuantityDistributedToGenericSlots));

	ensureMsgf(TotalQuantityDistributed == ViableQuantity,
	           TEXT("Quantity distributed does not match requested quantity"));

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

    // Kahn's algorithm with cycle handling
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
    TArray<int32> CyclicIndices;

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

    // Collect any remaining indices with non-zero in-degree (cyclic dependencies)
    for (int32 i = 0; i < NumSlots; ++i)
    {
       if (InDegree[i] > 0)
       {
           CyclicIndices.Add(i);
       }
    }

    // If cycles are detected, log a warning and partially sort
    if (!CyclicIndices.IsEmpty())
    {
       UE_LOG(LogRISInventory, Warning, TEXT("Cycle detected in UniversalTaggedSlots dependency graph! %d slots have cyclic dependencies."), CyclicIndices.Num());
       
       // Log the specific cyclic slots for debugging
       for (int32 CyclicIndex : CyclicIndices)
       {
           UE_LOG(LogRISInventory, Warning, TEXT("Cyclic Slot Index: %d, Slot: %s"), 
                  CyclicIndex, 
                  *UniversalTaggedSlots[CyclicIndex].Slot.ToString());
       }
    }

    // Build a new sorted array based on the available topological order
    TArray<FUniversalTaggedSlot> SortedSlots;
    SortedSlots.SetNum(NumSlots);

    // First, add slots without dependencies
    for (int32 Index : SortedIndices)
    {
       SortedSlots[SortedSlots.Num() - SortedIndices.Num() + SortedIndices.IndexOfByKey(Index)] = UniversalTaggedSlots[Index];
    }

    // Then, add remaining cyclic slots (if any)
    int32 SortedCount = SortedIndices.Num();
    for (int32 CyclicIndex : CyclicIndices)
    {
       SortedSlots[SortedCount++] = UniversalTaggedSlots[CyclicIndex];
    }

    // Replace the original array with the partially sorted one
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
	if (IsClient("CraftRecipe_IfServer")) return false;

	bool bSuccess = false;
	if (Recipe && CanCraftRecipe(Recipe))
	{
		for (const auto& Component : Recipe->Components)
		{
			const int32 Removed = DestroyItem_IfServer(Component.ItemId, Component.Quantity, NoInstances,
			                                           EItemChangeReason::Transformed);
			if (Removed < Component.Quantity)
			{
				UE_LOG(LogRISInventory, Error, TEXT("Failed to remove all items for crafting even though they were confirmed"));
				return false;
			}
		}
		bSuccess = true;

		if (const UItemRecipeData* ItemRecipe = Cast<UItemRecipeData>(Recipe))
		{
			const FItemBundle CraftedItem = FItemBundle(ItemRecipe->ResultingItemId, ItemRecipe->QuantityCreated);
			const int32 QuantityAdded = Super::AddItemWithInstances_IfServer(Subsystem, CraftedItem.ItemId, CraftedItem.Quantity, NoInstances, 
			                                                      true);
			if (QuantityAdded < ItemRecipe->QuantityCreated)
			{
				UE_LOG(LogRISInventory, Display, TEXT("Failed to add crafted item to inventory, dropping item instead"));

				if (!Subsystem)
				{
					UE_LOG(LogRISInventory, Error, TEXT("Subsystem is null, cannot drop item"));
					return false;
				}

				TArray<UItemInstanceData*> DroppingItemState;
				Subsystem->ExtractItem_IfServer_Implementation(CraftedItem.ItemId, CraftedItem.Quantity - QuantityAdded, NoInstances,
				                             EItemChangeReason::Transformed, DroppingItemState, false);

				SpawnItemIntoWorldFromContainer_ServerImpl(
					CraftedItem.ItemId, CraftedItem.Quantity - QuantityAdded, FVector(1e+300, 0, 0), DroppingItemState);
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


int32 UInventoryComponent::DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDrop, FVector RelativeDropLocation)
{
	// On client the below is just a guess
	const FTaggedItemBundle Item = GetItemForTaggedSlot(SlotTag);
	if (!Item.IsValid()) return 0;
	int32 QuantityToDrop = FMath::Min(Quantity, Item.Quantity);

	if (IsClient())
		RequestedOperationsToServer.Add(FRISExpectedOperation(RemoveTagged, SlotTag, QuantityToDrop));

	DropFromTaggedSlot_Server(SlotTag, Quantity, FItemBundle::ToInstanceIds(InstancesToDrop), RelativeDropLocation);

	return QuantityToDrop;
}

void UInventoryComponent::DropFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag, int32 Quantity, const TArray<int32>& InstanceIdsToDrop, 
                                                                   FVector RelativeDropLocation)
{
	TArray<UItemInstanceData*> InstancesToDrop;
	if (!InstanceIdsToDrop.IsEmpty())
	{
		int32 TagedSlotIndex = GetIndexForTaggedSlot(SlotTag);
		FTaggedItemBundle* TaggedSlot = &TaggedSlotItems[TagedSlotIndex];
		InstancesToDrop = TaggedSlot->FromInstanceIds(InstanceIdsToDrop);
	}
	DropFromTaggedSlot_ServerImpl(SlotTag, Quantity, InstancesToDrop, RelativeDropLocation);
}

void UInventoryComponent::DropFromTaggedSlot_ServerImpl(const FGameplayTag& SlotTag, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDrop, 
																   FVector RelativeDropLocation)
{
	const int32 Index = GetIndexForTaggedSlot(SlotTag);
	const FTaggedItemBundle& Item = TaggedSlotItems[Index];
	if (!Item.Tag.IsValid())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("DropFromTaggedSlot called with invalid slot tag"));
		return;
	}

	FGameplayTag ItemId = Item.ItemId;

	const FTaggedItemBundle ItemContained = GetItemForTaggedSlot(SlotTag);
	int32 QuantityToDrop = 0;
	if (ItemContained.IsValid() && ItemContained.ItemId == ItemId)
	{
		QuantityToDrop = FMath::Min(Quantity, ItemContained.Quantity);
	}

	TArray<UItemInstanceData*> InstanceArrayToAppendTo;
	ExtractItemFromTaggedSlot_IfServer(SlotTag, Item.ItemId, QuantityToDrop, InstancesToDrop, EItemChangeReason::Dropped,
	                                   InstanceArrayToAppendTo);

	// Spawn item in the world and update state
	SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, QuantityToDrop, RelativeDropLocation, InstanceArrayToAppendTo);
}

int32 UInventoryComponent::DropAllItems_ServerImpl()
{
	int32 DroppedCount = 0;

	for (int i = TaggedSlotItems.Num() - 1; i >= 0; i--)
	{
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(
			FMath::FRand() * 100, FMath::FRand() * 100, 100);
		DropFromTaggedSlot_ServerImpl(TaggedSlotItems[i].Tag, TaggedSlotItems[i].Quantity, NoInstances, DropLocation);
		DroppedCount++;
	}

	TaggedSlotItems.Empty();
	Super::DropAllItems_ServerImpl();

	return DroppedCount;
}

int32 UInventoryComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason,
                                           bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	TArray<UItemInstanceData*> ThrowAwayInstances;
	return ExtractItemImpl_IfServer(ItemId, Quantity, InstancesToDestroy, Reason,ThrowAwayInstances,
	                                AllowPartial, SuppressEvents, SuppressUpdate);
}

void UInventoryComponent::ClearServerImpl()
{
	if (IsClient("ClearInventory called on non-authority!")) return;

	int Num = TaggedSlotItems.Num();
	for (int i = Num - 1; i >= 0; i--)
	{
		// DestroyFromContainer false to ensure we dont create a bad recursion where the destruction from generic calls into removal from tagged
		RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItems[i].Tag, MAX_int32, NoInstances,
											  EItemChangeReason::ForceDestroyed, true, true, false, true);
	}

	
	// This loop is temporary until we have a proper server rollback system
	//  Loop reverse through  ItemsVer.Items
	for (int i = ItemsVer.Items.Num() - 1; i >= 0; i--)
	{
		const FItemBundle& Item = ItemsVer.Items[i];
		int32 QuantityBefore = Item.Quantity;
		int32 Destroyed = DestroyItemImpl(Item.ItemId, QuantityBefore, TArray<UItemInstanceData*>(), EItemChangeReason::ForceDestroyed, false, false, true);
		ensureMsgf(Destroyed == QuantityBefore,
			TEXT("DestroyItemImpl: Destroyed %d items, but expected to destroy %d items."), Destroyed, Item.Quantity);
	}
	
	ensureMsgf(ItemsVer.Items.IsEmpty(),
		TEXT("ClearInventory: ItemsVer.Items is not empty after clearing inventory."));

	UpdateWeightAndSlots();
}


void UInventoryComponent::UpdateWeightAndSlots()
{
	// First update weight and slots as if all items were in the generic slots
	Super::UpdateWeightAndSlots();

	// then subtract the slots of the tagged items
	for (const FTaggedItemBundle& TaggedInstance : TaggedSlotItems)
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

	ensureMsgf(UsedContainerSlotCount <= MaxSlotCount, TEXT("Used slot count is higher than max slot count!"));
}

void UInventoryComponent::OnInventoryItemAddedHandler(const UItemStaticData* ItemData, int32 Quantity,
                                                      const TArray<UItemInstanceData*>& InstancesAdded, EItemChangeReason Reason)
{
	CheckAndUpdateRecipeAvailability();
}

void UInventoryComponent::OnInventoryItemRemovedHandler(const UItemStaticData* ItemData, int32 Quantity,
														const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason)
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
