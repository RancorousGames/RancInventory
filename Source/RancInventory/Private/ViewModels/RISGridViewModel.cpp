// Copyright Rancorous Games, 2024

#include "ViewModels/RISGridViewModel.h"

#include "LogRancInventorySystem.h"
#include "Components/InventoryComponent.h"
#include "Core/RISFunctions.h"

class UInventoryComponent;

void URISGridViewModel::Initialize_Implementation(UInventoryComponent* InventoryComponent, int32 NumSlots,  bool bPreferEmptyUniversalSlots)
{
	if (IsInitialized)
		return;
	NumberOfSlots = NumSlots;
	PreferEmptyUniversalSlots = bPreferEmptyUniversalSlots;
	LinkedInventoryComponent = InventoryComponent;
	ViewableGridSlots.Empty();
	ViewableTaggedSlots.Empty();
	OperationsToConfirm.Empty();

	if (!LinkedInventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory Component is null"));
		return;
	}

	IsInitialized = true;
	
	// Initialize DisplayedSlots with empty item info for the specified number of slots
	for (int32 i = 0; i < NumSlots; i++)
	{
		ViewableGridSlots.Add(FItemBundle());
	}

	LinkedInventoryComponent->OnItemAddedToContainer.AddDynamic(this, &URISGridViewModel::HandleItemAdded);
	LinkedInventoryComponent->OnItemRemovedFromContainer.AddDynamic(this, &URISGridViewModel::HandleItemRemoved);
	LinkedInventoryComponent->OnItemAddedToTaggedSlot.
	                          AddDynamic(this, &URISGridViewModel::HandleTaggedItemAdded);
	LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(
		this, &URISGridViewModel::HandleTaggedItemRemoved);


	TArray<FItemBundleWithInstanceData> Items = LinkedInventoryComponent->GetAllContainerItems();

	for (FItemBundleWithInstanceData BackingItem : Items)
	{
		if (const UItemStaticData* const ItemData = URISSubsystem::GetItemDataById(BackingItem.ItemId))
		{
			int32 RemainingQuantity = BackingItem.Quantity;
			while (RemainingQuantity > 0)
			{
				int32 SlotToAddTo = FindSlotIndexForItem(BackingItem.ItemId, RemainingQuantity);
				if (SlotToAddTo == -1)
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find a slot to add the item to"));
					LinkedInventoryComponent->DropItems(BackingItem.ItemId, RemainingQuantity);
					continue;
				}

				FItemBundle& ExistingItem = ViewableGridSlots[SlotToAddTo];

				int32 AddLimit = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;
				if (ExistingItem.ItemId.IsValid())
				{
					AddLimit -= ExistingItem.Quantity;
				}
				else
				{
					ExistingItem.ItemId = BackingItem.ItemId;
					ExistingItem.Quantity = 0;
				}

				const int32 AddedAmount = FMath::Min(RemainingQuantity, AddLimit);
				RemainingQuantity -= AddedAmount;
				ExistingItem.Quantity += AddedAmount;
			}
		}
	}

	for (FUniversalTaggedSlot& UniTag : LinkedInventoryComponent->UniversalTaggedSlots)
	{
		ViewableTaggedSlots.Add(UniTag.Slot, FTaggedItemBundle(UniTag.Slot, FGameplayTag(), 0));
	}
	for (FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots)
	{
		ViewableTaggedSlots.Add(Tag, FTaggedItemBundle(Tag, FGameplayTag(), 0));
	}

	const TArray<FTaggedItemBundle>& TaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
	for (const FTaggedItemBundle& TaggedItem : TaggedItems)
	{
		ViewableTaggedSlots[TaggedItem.Tag] = TaggedItem;
	}
}


bool URISGridViewModel::IsSlotEmpty(int32 SlotIndex) const
{
	return !ViewableGridSlots.IsValidIndex(SlotIndex) || !ViewableGridSlots[SlotIndex].ItemId.IsValid();
}


bool URISGridViewModel::IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const
{
	return !ViewableTaggedSlots.Contains(SlotTag) || !ViewableTaggedSlots[SlotTag].ItemId.IsValid();
}


FItemBundle URISGridViewModel::GetItem(int32 SlotIndex) const
{
	if (ViewableGridSlots.IsValidIndex(SlotIndex))
	{
		return ViewableGridSlots[SlotIndex];
	}

	// Return an empty item info if the slot index is invalid
	return FItemBundle();
}


int32 URISGridViewModel::DropItem(FGameplayTag TaggedSlot, int32 SlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent ||
		(TaggedSlot.IsValid() && !ViewableTaggedSlots.Contains(TaggedSlot)) ||
		(!TaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(SlotIndex)))
		return false;

	int32 DroppedCount;;
	if (TaggedSlot.IsValid())
	{
		Quantity = FMath::Min(
			Quantity, LinkedInventoryComponent->GetItemForTaggedSlot(TaggedSlot).Quantity);
		OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, Quantity));
		DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(TaggedSlot, Quantity);
		ViewableTaggedSlots[TaggedSlot].Quantity -= DroppedCount;
		if (ViewableTaggedSlots[TaggedSlot].Quantity <= 0)
		{
			ViewableTaggedSlots[TaggedSlot].ItemId = FGameplayTag(); // Reset the slot to empty
		}
		OnTaggedSlotUpdated.Broadcast(TaggedSlot);
	}
	else
	{
		Quantity = FMath::Min(Quantity, ViewableGridSlots[SlotIndex].Quantity);
		OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, ViewableGridSlots[SlotIndex].ItemId, Quantity));
		DroppedCount = LinkedInventoryComponent->DropItems(ViewableGridSlots[SlotIndex].ItemId, Quantity);
		if (DroppedCount > 0)
		{
			ViewableGridSlots[SlotIndex].Quantity -= DroppedCount;
			if (ViewableGridSlots[SlotIndex].Quantity <= 0)
			{
				ViewableGridSlots[SlotIndex] = FItemBundle(); // Reset the slot to empty
			}
			OnSlotUpdated.Broadcast(SlotIndex);
		}
	}

	return DroppedCount;
}

int32 URISGridViewModel::UseItem(FGameplayTag TaggedSlot, int32 SlotIndex)
{
	if (!LinkedInventoryComponent ||
		(TaggedSlot.IsValid() && !ViewableTaggedSlots.Contains(TaggedSlot)) ||
		(!TaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(SlotIndex)))
		return false;

	if (TaggedSlot.IsValid())
	{
		LinkedInventoryComponent->UseItemFromTaggedSlot(TaggedSlot);
	}
	else
	{
		LinkedInventoryComponent->UseItem(ViewableGridSlots[SlotIndex].ItemId);
	}
	
	return 0;
}

bool URISGridViewModel::MoveItem_Implementation(FGameplayTag SourceTaggedSlot,
												 int32 SourceSlotIndex,
												 FGameplayTag TargetTaggedSlot,
												 int32 TargetSlotIndex)
{
	// For a full move, ignore the InQuantity (it will use the full source quantity)
	return MoveItem_Impl(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, 0, false);
}

bool URISGridViewModel::SplitItem_Implementation(FGameplayTag SourceTaggedSlot,
												  int32 SourceSlotIndex,
												  FGameplayTag TargetTaggedSlot,
												  int32 TargetSlotIndex,
												  int32 Quantity)
{
	// For a split, pass in the desired quantity and set bIsSplit to true.
	return MoveItem_Impl(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, Quantity, true);
}

bool URISGridViewModel::TryUnblockingMove(FGameplayTag TargetTaggedSlot, FGameplayTag ItemId)
{
	if (auto* BlockingSlot = LinkedInventoryComponent->WouldItemMoveIndirectlyViolateBlocking(TargetTaggedSlot, URISSubsystem::GetItemDataById(ItemId)))
	{
		FTaggedItemBundle BlockingItem = GetItemForTaggedSlot(BlockingSlot->UniversalSlotToBlock);
		if (BlockingItem.IsValid())
		{
			// Move blocking item to generic slot with a recursive call
			return MoveItem_Impl(BlockingSlot->UniversalSlotToBlock, -1, FGameplayTag(), FindSlotIndexForItem(BlockingItem.ItemId, BlockingItem.Quantity), BlockingItem.Quantity, false);
		}
	}

	return false;
}

bool URISGridViewModel::MoveItem_Impl(FGameplayTag SourceTaggedSlot,
                                      int32 SourceSlotIndex,
                                      FGameplayTag TargetTaggedSlot,
                                      int32 TargetSlotIndex,
                                      int32 InQuantity,    // Only used for split moves
                                      bool IsSplit)       // false = full move, true = split move
{
    if (!LinkedInventoryComponent ||
        (!SourceTaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(SourceSlotIndex)) ||
        (!TargetTaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(TargetSlotIndex)) ||
        (SourceSlotIndex != -1 && SourceSlotIndex == TargetSlotIndex) ||
        (SourceTaggedSlot == TargetTaggedSlot && SourceTaggedSlot.IsValid()))
    {
        return false;
    }

    // Determine whether we’re working with tagged slots
    bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid();
    bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid();

    // Get the source bundle
    FGenericItemBundle SourceItem;
    if (SourceIsTaggedSlot)
    {
        SourceItem = ViewableTaggedSlots.Find(SourceTaggedSlot);
        if (!SourceItem.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("Source tagged slot does not exist"));
            return false;
        }
    }
    else
    {
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }

    // Get the target bundle
    FGenericItemBundle TargetItem;
    if (TargetIsTaggedSlot)
    {
        if (!LinkedInventoryComponent->IsTaggedSlotCompatible(SourceItem.GetItemId(), TargetTaggedSlot))
        {
            UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
            return false;
        }

        TargetItem = ViewableTaggedSlots.Find(TargetTaggedSlot);
    }
    else
    {
        TargetItem = &ViewableGridSlots[TargetSlotIndex];
    }

    const FGameplayTag& ItemId = SourceItem.GetItemId();

	if (IsSplit && InQuantity > SourceItem.GetQuantity())
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't split more items than are contained in the source slot"));
		return false;
	}
	
    int32 RequestedQuantity = IsSplit ? InQuantity : SourceItem.GetQuantity();

	FGameplayTag SwapItemId = FGameplayTag();
	int32 SwapQuantity = -1;
	if (!IsSplit && TargetItem.IsValid() && TargetItem.GetItemId() != ItemId)
	{
		SwapItemId = TargetItem.GetItemId();
		SwapQuantity = TargetItem.GetQuantity();
	}
	
	int32 QuantityToMove = RequestedQuantity;
	if (TargetIsTaggedSlot || SourceIsTaggedSlot)
	{
		QuantityToMove = LinkedInventoryComponent->ValidateMoveItem(ItemId, RequestedQuantity, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);

		if (QuantityToMove <= 0 && TargetIsTaggedSlot && TryUnblockingMove(TargetTaggedSlot, ItemId))
		{
			QuantityToMove = LinkedInventoryComponent->ValidateMoveItem(ItemId, RequestedQuantity, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
		}

		if (QuantityToMove <= 0)
			return false;
	}
	
    // Call MoveBetweenSlots with different parameters depending on whether this is a split
    FRISMoveResult MoveResult;
    if (IsSplit)
    {
        MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem, false,
                                                     QuantityToMove, false, false);
    }
    else
    {
        MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem, false,
                                                     QuantityToMove, true);
    }

    // Process the result (identical for both operations)
    if (MoveResult.QuantityMoved > 0)
    {
        if (SourceIsTaggedSlot)
        {
            OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, SourceTaggedSlot, ItemId, MoveResult.QuantityMoved));
            OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
            if (TargetIsTaggedSlot)
            {
                OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot, ItemId, MoveResult.QuantityMoved));
                OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
            }
            else
            {
                OperationsToConfirm.Emplace(FRISExpectedOperation(Add, ItemId, MoveResult.QuantityMoved));
                OnSlotUpdated.Broadcast(TargetSlotIndex);
            }

            if (MoveResult.WereItemsSwapped)
            {
                if (TargetIsTaggedSlot)
                    OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
                else
                    OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceItem.GetItemId(), SourceItem.GetQuantity()));
                OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, SourceTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
            }
        }
        else
        {
            if (TargetIsTaggedSlot)
            {
                OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, ItemId, MoveResult.QuantityMoved));
                OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot, ItemId, MoveResult.QuantityMoved));
                OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);

            	if (MoveResult.WereItemsSwapped)
            	{
            		OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
            		OperationsToConfirm.Emplace(FRISExpectedOperation(Add, SourceItem.GetItemId(), SourceItem.GetQuantity()));
            	}
            }
            else // purely visual move
            {
                OnSlotUpdated.Broadcast(TargetSlotIndex);
            }
            OnSlotUpdated.Broadcast(SourceSlotIndex);

        	
        }
    }

    // For moves involving tagged slots, send the move request to the server
    if (TargetIsTaggedSlot || SourceIsTaggedSlot)
    {
        LinkedInventoryComponent->MoveItem(ItemId, QuantityToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
    }

    return true;
}

bool URISGridViewModel::CanSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const
{
	if (!ViewableGridSlots.IsValidIndex(SlotIndex))
	{
		return false; // Slot index out of bounds
	}

	if (!LinkedInventoryComponent->CanContainerReceiveItems(ItemId, Quantity)) return false;

	const bool TargetSlotEmpty = IsSlotEmpty(SlotIndex);

	const FItemBundle& TargetSlotItem = ViewableGridSlots[SlotIndex];
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemId)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		const int32 AvailableSpace = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize - TargetSlotItem.Quantity : TargetSlotEmpty ? 1 : 0;
		return AvailableSpace >= Quantity;
	}

	return false; // Different item types or slot not stackable
}

bool URISGridViewModel::CanTaggedSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool CheckContainerLimits) const
{
	bool BasicCheck = LinkedInventoryComponent->IsTaggedSlotCompatible(ItemId, SlotTag) && (!CheckContainerLimits || LinkedInventoryComponent->
		CanContainerReceiveItems(ItemId, Quantity));

	if (!BasicCheck) return false;

	const bool TargetSlotEmpty = IsTaggedSlotEmpty(SlotTag);
	const FTaggedItemBundle& TargetSlotItem = ViewableTaggedSlots.FindChecked(SlotTag);
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemId)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		const int32 AvailableSpace = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize - TargetSlotItem.Quantity : TargetSlotEmpty ? 1 : 0;
		return AvailableSpace >= Quantity;
	}

	return false;
}

void URISGridViewModel::HandleItemAdded_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
	// Iterate in reverse to safely remove items
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == RISSlotOperation::Add &&
			OperationsToConfirm[i].Quantity == Quantity &&
			!OperationsToConfirm[i].TaggedSlot.IsValid() && // Assuming non-tagged operations have a "none" tag
			OperationsToConfirm[i].ItemId == ItemData->ItemId) // Assuming you have ItemID or similar property
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	int32 RemainingItems = Quantity;

	while (RemainingItems > 0)
	{
		int32 SlotIndex = FindSlotIndexForItem(ItemData->ItemId, Quantity);
		if (SlotIndex == -1)
		{
			UE_LOG(LogTemp, Error, TEXT("No available slot found for item."));
			break; // Exit loop if no slot is found
		}

		if (ItemData == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Item data not found for item %s"), *ItemData->ItemId.ToString());
			break; // Exit loop if no item data is found
		}

		// Calculate how many items can be added to this slot
		int32 ItemsToAdd = FMath::Min(RemainingItems, ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1);
		FItemBundle& ExistingItem = ViewableGridSlots[SlotIndex];
		if (ExistingItem.IsValid())
		{
			ItemsToAdd = FMath::Min(RemainingItems, ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize - ExistingItem.Quantity : 1);
			ExistingItem.Quantity += ItemsToAdd;
		}
		else
		{
			ExistingItem = FItemBundle(ItemData->ItemId, ItemsToAdd);
		}

		RemainingItems -= ItemsToAdd;

		OnSlotUpdated.Broadcast(SlotIndex);

		if (ItemsToAdd == 0) break; // Prevent infinite loop if no items can be added
	}
}

void URISGridViewModel::HandleTaggedItemAdded_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, FTaggedItemBundle PreviousItem, EItemChangeReason Reason)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == RISSlotOperation::AddTagged &&
			OperationsToConfirm[i].Quantity == Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemData->ItemId)
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	// Directly add the item to the tagged slot without overflow check
	if (ViewableTaggedSlots[SlotTag].ItemId == ItemData->ItemId)
		ViewableTaggedSlots[SlotTag].Quantity += Quantity;
	else
		ViewableTaggedSlots[SlotTag] = FTaggedItemBundle(SlotTag, ItemData->ItemId, Quantity);
	OnTaggedSlotUpdated.Broadcast(SlotTag);
}

void URISGridViewModel::HandleItemRemoved_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == RISSlotOperation::Remove &&
			OperationsToConfirm[i].Quantity == Quantity &&
			OperationsToConfirm[i].ItemId == ItemData->ItemId) // Corrected field names based on your updated struct
		{
			OperationsToConfirm.RemoveAt(i);
			return; // Early return if the operation is confirmed
		}
	}

	// RemainingItems variable to track how many items still need to be removed
	int32 RemainingItems = Quantity;

	// Iterate through all displayed slots to remove items
	for (int32 SlotIndex = 0; SlotIndex < ViewableGridSlots.Num() && RemainingItems > 0; ++SlotIndex)
	{
		// Check if the current slot contains the item we're looking to remove
		if (ViewableGridSlots[SlotIndex].ItemId == ItemData->ItemId)
		{
			// Determine how many items we can remove from this slot
			int32 ItemsToRemove = FMath::Min(RemainingItems, ViewableGridSlots[SlotIndex].Quantity);

			// Adjust the slot quantity and the remaining item count
			ViewableGridSlots[SlotIndex].Quantity -= ItemsToRemove;
			RemainingItems -= ItemsToRemove;

			// If the slot is now empty, reset it to the default empty item instance
			if (ViewableGridSlots[SlotIndex].Quantity <= 0)
			{
				ViewableGridSlots[SlotIndex] = FItemBundle::EmptyItemInstance;
			}

			// Broadcast the slot update
			OnSlotUpdated.Broadcast(SlotIndex);
		}
	}

	// If after iterating through all slots, RemainingItems is still greater than 0,
	// it indicates we were unable to find enough items to remove.
	// You might want to handle this case, possibly with an error message or additional logic.
	if (RemainingItems > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to remove all requested items. %d items could not be removed."), RemainingItems);
	}
}

void URISGridViewModel::HandleTaggedItemRemoved_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == RISSlotOperation::RemoveTagged &&
			OperationsToConfirm[i].Quantity == Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemData->ItemId) // Assuming ItemID is available
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	if (ViewableTaggedSlots.Contains(SlotTag))
	{
		if (!ViewableTaggedSlots[SlotTag].IsValid() || ViewableTaggedSlots[SlotTag].ItemId != ItemData->ItemId)
		{
			UE_LOG(LogTemp, Warning, TEXT("Client misprediction detected in tagged slot %s"), *SlotTag.ToString());
			ForceFullUpdate(); // Todo: we would want to wait until all other operations have cleared until we do this
			return;
		}

		// Update the quantity or remove the item if necessary
		ViewableTaggedSlots[SlotTag].Quantity -= Quantity;
		if (ViewableTaggedSlots[SlotTag].Quantity <= 0)
		{
			ViewableTaggedSlots[SlotTag].ItemId = FGameplayTag(); // Remove the item if quantity drops to 0 or below
			ViewableTaggedSlots[SlotTag].Quantity = 0; // Note we do not want to reset tag as we expect that to stay the same as the owning slot
		}

		OnTaggedSlotUpdated.Broadcast(SlotTag);
	}
}

void URISGridViewModel::ForceFullUpdate_Implementation()
{
	// TODO: Implement this, we prefer not to just call initialize as that will loose all slot mappings
}

FTaggedItemBundle& URISGridViewModel::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	// cast away constness to allow for modification
	return const_cast<FTaggedItemBundle&>(ViewableTaggedSlots.FindChecked(SlotTag));
}

bool URISGridViewModel::AssertViewModelSettled() const
{
	// Check operations to confirm
	bool bAllSlotsSettled = OperationsToConfirm.Num() == 0;
	ensureMsgf(bAllSlotsSettled, TEXT("ViewModel is not settled. %d operations are pending."), OperationsToConfirm.Num());
	if (!bAllSlotsSettled)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("ViewModel is not settled. %d operations are pending."), OperationsToConfirm.Num());
	}

	// Check slot quantities matches inventorycomponent
	bool bAllSlotsQuantitiesMatched = true;
	TArray<FItemBundleWithInstanceData> AllInventoryItems = LinkedInventoryComponent->GetAllContainerItems();
	
	for (FItemBundleWithInstanceData InventoryItem : AllInventoryItems)
	{
		if (InventoryItem.Quantity == 0) continue;

		int32 QuantityInViewModelSlots = 0;
		for (const FItemBundle& Slot : ViewableGridSlots)
		{
			if (Slot.ItemId == InventoryItem.ItemId)
			{
				QuantityInViewModelSlots += Slot.Quantity;
			}
		}
		for (const TTuple<FGameplayTag, FTaggedItemBundle>& Slot : ViewableTaggedSlots)
		{
			if (Slot.Value.ItemId == InventoryItem.ItemId)
			{
				QuantityInViewModelSlots += Slot.Value.Quantity;
			}
		}

		bAllSlotsQuantitiesMatched &= QuantityInViewModelSlots == InventoryItem.Quantity;

		ensureMsgf(bAllSlotsQuantitiesMatched, TEXT("Item %s has a quantity mismatch. Expected %d, got %d"), *InventoryItem.ItemId.ToString(), InventoryItem.Quantity, QuantityInViewModelSlots);
		if (!bAllSlotsQuantitiesMatched)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Item %s has a quantity mismatch. Expected %d, got %d"), *InventoryItem.ItemId.ToString(), InventoryItem.Quantity, QuantityInViewModelSlots);
		}
	}

	// Check that all ViewableTaggedSlots are either empty or their tag matches the tag of the slot
	bool bAllTaggedSlotsTagMatched = true;
	for (const TTuple<FGameplayTag, FTaggedItemBundle>& Slot : ViewableTaggedSlots)
	{
		if (Slot.Value.ItemId.IsValid())
		{
			bAllTaggedSlotsTagMatched &= Slot.Key == Slot.Value.Tag;
			ensureMsgf(bAllTaggedSlotsTagMatched, TEXT("Tagged slot %s has a tag mismatch. Expected %s, got %s"), *Slot.Key.ToString(), *Slot.Key.ToString(), *Slot.Value.Tag.ToString());
			if (!bAllTaggedSlotsTagMatched)
			{
				UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot %s has a tag mismatch. Expected %s, got %s"), *Slot.Key.ToString(), *Slot.Key.ToString(), *Slot.Value.Tag.ToString());
			}
		}
		bAllTaggedSlotsTagMatched &= Slot.Value.Tag.IsValid();
	}
	
	return bAllSlotsSettled && bAllSlotsQuantitiesMatched && bAllTaggedSlotsTagMatched;
}

void URISGridViewModel::PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots, bool DestroyAfterPickup)
{
	if (WorldItem == nullptr || !WorldItem->IsValidLowLevel())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WorldItem is not valid"));
		return;
	}

	auto PreferredSlot = FindTaggedSlotForItem(WorldItem->RepresentedItem.ItemId, WorldItem->RepresentedItem.Quantity);
	TryUnblockingMove(PreferredSlot, WorldItem->RepresentedItem.ItemId);
	
	LinkedInventoryComponent->PickupItem(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
}


int32 URISGridViewModel::FindSlotIndexForItem_Implementation(const FGameplayTag& ItemId, int32 Quantity)
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);

	int32 FirstEmptySlot = -1;
	for (int32 Index = 0; Index < ViewableGridSlots.Num(); ++Index)
	{
		const FItemBundle& ExistingItem = ViewableGridSlots[Index];

		if (!ExistingItem.ItemId.IsValid() && FirstEmptySlot == -1)
		{
			FirstEmptySlot = Index;
		}
		if (ExistingItem.ItemId == ItemId)
		{
			if (ItemData->MaxStackSize > 1 && ExistingItem.Quantity < ItemData->MaxStackSize)
			{
				return Index;
			}
		}
	}

	return FirstEmptySlot;
}

FGameplayTag URISGridViewModel::FindTaggedSlotForItem(const FGameplayTag& ItemId, int32 Quanitity) const
{
	// Validate
	if (!ItemId.IsValid()) return FGameplayTag::EmptyTag;

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData) return FGameplayTag::EmptyTag; // Ensure the item data is valid

	FGameplayTag FallbackSwapSlot = FGameplayTag::EmptyTag;
	
	// First try specialized slots
	for (const FGameplayTag& SlotTag : LinkedInventoryComponent->SpecializedTaggedSlots)
	{
		if (ItemData->ItemCategories.HasTag(SlotTag))
		{
			if (IsTaggedSlotEmpty(SlotTag))
			{
				return SlotTag;
			}
			
			FallbackSwapSlot = SlotTag;
		}
	}

	// rather swap to a non-empty specialized slot than to an empty universal slot
	if (!PreferEmptyUniversalSlots && FallbackSwapSlot.IsValid()) return FallbackSwapSlot;

	// Then try universal slots, but prefer slots that are matched by a category
	for (const FUniversalTaggedSlot& UniSlotTag : LinkedInventoryComponent->UniversalTaggedSlots)
	{
		// this needs to account for empty blocking and universal exclusive slot
		if (IsTaggedSlotEmpty(UniSlotTag.Slot) && LinkedInventoryComponent->CanItemBeEquippedInUniversalSlot(ItemId, UniSlotTag.Slot, true))
		{
			if (!FallbackSwapSlot.IsValid()) FallbackSwapSlot = UniSlotTag.Slot;

			if (ItemData->ItemCategories.HasTag(UniSlotTag.Slot))
			{
				FTaggedItemBundle ExistingItem = GetItemForTaggedSlot(UniSlotTag.Slot);
				if (!ExistingItem.IsValid() || !URISSubsystem::GetItemDataById(ExistingItem.ItemId)->ItemCategories.HasTag(UniSlotTag.Slot))
				{
					// This is the right slot if it is empty or if we are a better fit
					return UniSlotTag.Slot;
				}
			}
		}
	}

	// Attempt to swap with the fallback slot if one was identified
	if (!FallbackSwapSlot.IsValid())
	{
		if (LinkedInventoryComponent->UniversalTaggedSlots.Num() == 0) return FGameplayTag::EmptyTag;

		FallbackSwapSlot = LinkedInventoryComponent->UniversalTaggedSlots[0].Slot;
	}

	return FallbackSwapSlot;
}

bool URISGridViewModel::MoveItemToAnyTaggedSlot_Implementation(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex)
{
	if (!LinkedInventoryComponent || (!SourceTaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(SourceSlotIndex)))
	{
		return false;
	}

	const bool SourceIsTagSlot = SourceTaggedSlot.IsValid();

	FGenericItemBundle SourceItem;
	if (SourceIsTagSlot)
	{
		SourceItem = &ViewableTaggedSlots[SourceTaggedSlot];
	}
	else
	{
		SourceItem = &ViewableGridSlots[SourceSlotIndex];
	}

	if (!SourceItem.IsValid()) return false;

	const auto TargetSlot = FindTaggedSlotForItem(SourceItem.GetItemId(), SourceItem.GetQuantity());

	return MoveItem(SourceTaggedSlot, SourceSlotIndex, TargetSlot, -1);
}
