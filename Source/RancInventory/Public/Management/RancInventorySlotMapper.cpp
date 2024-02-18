#include "RancInventorySlotMapper.h"

#include "RancInventoryFunctions.h"
#include "Actors/AWorldItem.h"
#include "RancInventory/public/Components/RancInventoryComponent.h"
#include "WarTribes/InventorySystem/AWTWorldItem.h"

URancInventorySlotMapper::URancInventorySlotMapper(): LinkedInventoryComponent(nullptr)
{
	// Initialize any necessary members if needed
}


void URancInventorySlotMapper::Initialize(URancInventoryComponent* InventoryComponent, int32 NumSlots,
                                          bool AutoEquipToSpecialSlots, bool PreferUniversalOverGenericSlots)
{
	NumberOfSlots = NumSlots;
	LinkedInventoryComponent = InventoryComponent;
	bAutoAddToSpecializedSlots = AutoEquipToSpecialSlots;
	bPreferUniversalOverGenericSlots = PreferUniversalOverGenericSlots;
	DisplayedSlots.Empty();
	DisplayedTaggedSlots.Empty();
	OperationsToConfirm.Empty();

	if (!LinkedInventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory Component is null"));
		return;
	}

	// Initialize DisplayedSlots with empty item info for the specified number of slots
	for (int32 i = 0; i < NumSlots; i++)
	{
		DisplayedSlots.Add(FRancItemInstance());
	}

	LinkedInventoryComponent->OnItemAdded.AddDynamic(this, &URancInventorySlotMapper::HandleItemAdded);
	LinkedInventoryComponent->OnItemRemoved.AddDynamic(this, &URancInventorySlotMapper::HandleItemRemoved);
	LinkedInventoryComponent->OnItemAddedToTaggedSlot.
	                          AddDynamic(this, &URancInventorySlotMapper::HandleTaggedItemAdded);
	LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(
		this, &URancInventorySlotMapper::HandleTaggedItemRemoved);


	TArray<FRancItemInstance> Items = LinkedInventoryComponent->GetAllItems();

	for (FRancItemInstance BackingItem : Items)
	{
		if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemDataById(BackingItem.ItemId))
		{
			int32 RemainingQuantity = BackingItem.Quantity;
			while (RemainingQuantity > 0)
			{
				int32 SlotToAddTo = FindSlotIndexForItem(BackingItem);
				if (SlotToAddTo == -1)
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find a slot to add the item to"));
					LinkedInventoryComponent->DropItems(FRancItemInstance(BackingItem.ItemId, RemainingQuantity));
					continue;
				}

				FRancItemInstance& ExistingItem = DisplayedSlots[SlotToAddTo];

				int32 AddLimit = ItemData->bIsStackable ? ItemData->MaxStackSize : 1;
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

	for (FGameplayTag& Tag : LinkedInventoryComponent->UniversalTaggedSlots)
	{
		DisplayedTaggedSlots.Add(Tag, FRancItemInstance());
	}
	for (FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots)
	{
		DisplayedTaggedSlots.Add(Tag, FRancItemInstance());
	}

	const TArray<FRancTaggedItemInstance>& TaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
	for (const FRancTaggedItemInstance& TaggedItem : TaggedItems)
	{
		DisplayedTaggedSlots[TaggedItem.Tag] = TaggedItem.ItemInstance;
	}
}


bool URancInventorySlotMapper::IsSlotEmpty(int32 SlotIndex) const
{
	return DisplayedSlots.IsValidIndex(SlotIndex) && !DisplayedSlots[SlotIndex].ItemId.IsValid();
}


bool URancInventorySlotMapper::IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const
{
	return !DisplayedTaggedSlots.Contains(SlotTag) || !DisplayedTaggedSlots[SlotTag].ItemId.IsValid();
}


FRancItemInstance URancInventorySlotMapper::GetItem(int32 SlotIndex) const
{
	if (DisplayedSlots.IsValidIndex(SlotIndex))
	{
		return DisplayedSlots[SlotIndex];
	}

	// Return an empty item info if the slot index is invalid
	return FRancItemInstance();
}

void URancInventorySlotMapper::SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent) return;
	if (!DisplayedSlots.IsValidIndex(SourceSlotIndex)) return;
	if (Quantity <= 0) return;

	FRancItemInstance& SourceItem = DisplayedSlots[SourceSlotIndex];
	if (SourceItem.Quantity < Quantity) return; // Not enough items in the source slot to split

	if (DisplayedSlots.IsValidIndex(TargetSlotIndex))
	{
		// If the target slot is valid and matches the item type, add to it
		FRancItemInstance& TargetItem = DisplayedSlots[TargetSlotIndex];
		if (TargetItem.ItemId == SourceItem.ItemId)
		{
			TargetItem.Quantity += Quantity;
		}
		else if (!TargetItem.ItemId.IsValid())
		{
			// If the target slot is empty, move the specified quantity
			TargetItem = FRancItemInstance(SourceItem.ItemId, Quantity);
		}
		else
		{
			// Can't split into a different, non-empty item type
			return;
		}
	}
	else
	{
		// If the target slot is beyond the current array, add a new slot with the item
		FRancItemInstance NewItem(SourceItem.ItemId, Quantity);
		DisplayedSlots.Add(NewItem);
	}

	// Update the source slot quantity
	SourceItem.Quantity -= Quantity;
	if (SourceItem.Quantity <= 0)
	{
		// If all items have been moved, reset the source slot
		SourceItem = FRancItemInstance();
	}

	OnSlotUpdated.Broadcast(SourceSlotIndex);
	OnSlotUpdated.Broadcast(TargetSlotIndex);
}

int32 URancInventorySlotMapper::DropItem(FGameplayTag TaggedSlot, int32 SlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent ||
		(TaggedSlot.IsValid() && !DisplayedTaggedSlots.Contains(TaggedSlot)) ||
		(!TaggedSlot.IsValid() && !DisplayedSlots.IsValidIndex(SlotIndex)))
		return false;

	int32 DroppedCount;;
	if (TaggedSlot.IsValid())
	{
		Quantity = FMath::Min(
			Quantity, LinkedInventoryComponent->GetItemForTaggedSlot(TaggedSlot).ItemInstance.Quantity);
		DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(TaggedSlot, Quantity);
		DisplayedTaggedSlots[TaggedSlot].Quantity -= DroppedCount;
		if (DisplayedTaggedSlots[TaggedSlot].Quantity <= 0)
		{
			DisplayedTaggedSlots[TaggedSlot] = FRancItemInstance::EmptyItemInstance; // Reset the slot to empty
		}
		OperationsToConfirm.Emplace(FExpectedOperation(RemoveTagged, TaggedSlot, DroppedCount));
		OnTaggedSlotUpdated.Broadcast(TaggedSlot);
	}
	else
	{
		Quantity = FMath::Min(Quantity, DisplayedSlots[SlotIndex].Quantity);
		DroppedCount = LinkedInventoryComponent->DropItems(
			FRancItemInstance(DisplayedSlots[SlotIndex].ItemId, Quantity));
		OperationsToConfirm.Emplace(FExpectedOperation(Remove, DisplayedSlots[SlotIndex].ItemId, DroppedCount));
		if (DroppedCount > 0)
		{
			DisplayedSlots[SlotIndex].Quantity -= DroppedCount;
			if (DisplayedSlots[SlotIndex].Quantity <= 0)
			{
				DisplayedSlots[SlotIndex] = FRancItemInstance(); // Reset the slot to empty
			}
			OnSlotUpdated.Broadcast(SlotIndex);
		}
	}

	return DroppedCount;
}

void SwapOrStack(FRancItemInstance* Source, FRancItemInstance* Target, int32 TransferAmount, bool ShouldStack)
{
	if (ShouldStack)
	{
		Target->Quantity += TransferAmount;
		Source->Quantity -= TransferAmount;
		if (Source->Quantity <= 0)
			*Source = FRancItemInstance::EmptyItemInstance;
	}
	else
	{
		const FRancItemInstance* Temp = Source;
		*Source = *Target; // Target might be invalid but thats fine
		*Target = *Temp;
	}
}

bool URancInventorySlotMapper::MoveItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex,
                                        FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex)
{
	if (!LinkedInventoryComponent || (!SourceTaggedSlot.IsValid() && !DisplayedSlots.IsValidIndex(SourceSlotIndex)) ||
		(!TargetTaggedSlot.IsValid() && !DisplayedSlots.IsValidIndex(TargetSlotIndex)) ||
		(SourceSlotIndex != -1 && SourceSlotIndex == TargetSlotIndex) ||
		(SourceTaggedSlot == TargetTaggedSlot && SourceTaggedSlot.IsValid()))
	{
		return false;
	}

	// Prepare
	bool SourceIsTagSlot = SourceTaggedSlot.IsValid();
	bool TargetIsTagSlot = TargetTaggedSlot.IsValid();

	FRancItemInstance SourceItem;
	if (SourceIsTagSlot)
	{
		SourceItem = LinkedInventoryComponent->GetItemForTaggedSlot(SourceTaggedSlot).ItemInstance;
	}
	else
	{
		SourceItem = DisplayedSlots[SourceSlotIndex];
	}

	if (!SourceItem.ItemId.IsValid()) return false;

	const URancItemData* SourceData = URancInventoryFunctions::GetItemDataById(SourceItem.ItemId);

	FRancItemInstance TargetItem;
	if (TargetIsTagSlot)
	{
		TargetItem = LinkedInventoryComponent->GetItemForTaggedSlot(TargetTaggedSlot).ItemInstance;
	}
	else
	{
		TargetItem = DisplayedSlots[TargetSlotIndex];
	}

	bool IsPureMove = !TargetIsTagSlot && !SourceIsTagSlot; // pure moves do not affect the linked inventory

	const URancItemData* TargetData = nullptr;
	const bool TargetSlotIsEmpty = !TargetItem.ItemId.IsValid();
	if (!TargetSlotIsEmpty)
	{
		TargetData = URancInventoryFunctions::GetItemDataById(TargetItem.ItemId);
	}

	if (!SourceData) return false; // Ensure source data is valid

	int32 TransferAmount = SourceItem.Quantity;
	const bool StackOnTarget = !TargetSlotIsEmpty && SourceItem.ItemId == TargetItem.ItemId && TargetData->bIsStackable;
	if (StackOnTarget)
	{
		const int32 AvailableSpace = TargetData->MaxStackSize - TargetItem.Quantity;
		TransferAmount = FMath::Min(AvailableSpace, SourceItem.Quantity);
	}

	const FRancItemInstance MoveItem = FRancItemInstance(SourceItem.ItemId, TransferAmount);

	// Move
	if (SourceIsTagSlot)
	{
		OperationsToConfirm.Emplace(FExpectedOperation(Remove, SourceTaggedSlot, TransferAmount));
		if (TargetIsTagSlot)
		{
			LinkedInventoryComponent->MoveItemsFromAndToTaggedSlot_Server(MoveItem, SourceTaggedSlot, TargetTaggedSlot);
			OperationsToConfirm.Emplace(FExpectedOperation(Add, TargetTaggedSlot, TransferAmount));

			SwapOrStack(&DisplayedTaggedSlots[SourceTaggedSlot], &DisplayedTaggedSlots[TargetTaggedSlot],
			            TransferAmount, StackOnTarget);

			OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
		}
		else
		{
			LinkedInventoryComponent->MoveItemsFromTaggedSlot_Server(MoveItem, SourceTaggedSlot);
			OperationsToConfirm.Emplace(FExpectedOperation(Add, MoveItem.ItemId, TransferAmount));

			SwapOrStack(&DisplayedTaggedSlots[SourceTaggedSlot], &DisplayedSlots[TargetSlotIndex], TransferAmount,
			            StackOnTarget);

			OnSlotUpdated.Broadcast(TargetSlotIndex);
		}

		OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
	}
	else // source is a generic slot
	{
		if (TargetIsTagSlot)
		{
			// Move from a generic slot to a tagged slot
			LinkedInventoryComponent->MoveItemsToTaggedSlot_Server(MoveItem, TargetTaggedSlot);
			OperationsToConfirm.Emplace(FExpectedOperation(Remove, MoveItem.ItemId, TransferAmount));
			OperationsToConfirm.Emplace(FExpectedOperation(Add, TargetTaggedSlot, TransferAmount));

			SwapOrStack(&DisplayedSlots[SourceSlotIndex], &DisplayedTaggedSlots[TargetTaggedSlot], TransferAmount,
			            StackOnTarget);
			OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
		}
		else
		{
			SwapOrStack(&DisplayedSlots[SourceSlotIndex], &DisplayedSlots[TargetSlotIndex], TransferAmount,
			            StackOnTarget);
			OnSlotUpdated.Broadcast(TargetSlotIndex);
		}

		OnSlotUpdated.Broadcast(SourceSlotIndex);
	}

	return true;
}


bool URancInventorySlotMapper::CanAddItemToSlot(const FRancItemInstance& ItemInfo, int32 SlotIndex) const
{
	if (!DisplayedSlots.IsValidIndex(SlotIndex))
	{
		return false; // Slot index out of bounds
	}

	bool TargetSlotEmpty = IsSlotEmpty(SlotIndex);

	const FRancItemInstance& TargetSlotItem = DisplayedSlots[SlotIndex];
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemInfo.ItemId)
	{
		const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemInfo.ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		const int32 AvailableSpace = ItemData->bIsStackable ? ItemData->MaxStackSize - TargetSlotItem.Quantity : 0;
		return AvailableSpace >= ItemInfo.Quantity;
	}

	return false; // Different item types or slot not stackable
}

void URancInventorySlotMapper::HandleItemAdded(const FRancItemInstance& Item)
{
	// Iterate in reverse to safely remove items
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::Add &&
			OperationsToConfirm[i].Quantity == Item.Quantity &&
			!OperationsToConfirm[i].TaggedSlot.IsValid() && // Assuming non-tagged operations have a "none" tag
			OperationsToConfirm[i].ItemId == Item.ItemId) // Assuming you have ItemID or similar property
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	int32 RemainingItems = Item.Quantity;

	while (RemainingItems > 0)
	{
		int32 SlotIndex = FindSlotIndexForItem(Item);
		if (SlotIndex == -1)
		{
			UE_LOG(LogTemp, Error, TEXT("No available slot found for item."));
			break; // Exit loop if no slot is found
		}

		// get item data
		const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(Item.ItemId);

		if (ItemData == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Item data not found for item %s"), *Item.ItemId.ToString());
			break; // Exit loop if no item data is found
		}

		// Calculate how many items can be added to this slot
		int32 ItemsToAdd = ItemData->bIsStackable ? ItemData->MaxStackSize : 1;
		FRancItemInstance& ExistingItem = DisplayedSlots[SlotIndex];
		if (ExistingItem.IsValid())
		{
			ItemsToAdd = FMath::Min(RemainingItems, ItemData->bIsStackable ? ItemData->MaxStackSize - ExistingItem.Quantity : 1);
			ExistingItem.Quantity += ItemsToAdd;
		}
		else
		{
			ExistingItem = FRancItemInstance(Item.ItemId, ItemsToAdd);
		}
		
		RemainingItems -= ItemsToAdd;

		OnSlotUpdated.Broadcast(SlotIndex);

		if (ItemsToAdd == 0) break; // Prevent infinite loop if no items can be added
	}
}

void URancInventorySlotMapper::HandleTaggedItemAdded(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInfo)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::AddTagged &&
			OperationsToConfirm[i].Quantity == ItemInfo.Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemInfo.ItemId)
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	// Directly add the item to the tagged slot without overflow check
	DisplayedTaggedSlots[SlotTag] = ItemInfo;
	OnTaggedSlotUpdated.Broadcast(SlotTag);
}

void URancInventorySlotMapper::HandleItemRemoved(const FRancItemInstance& ItemInfo)
{
    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == SlotOperation::Remove &&
            OperationsToConfirm[i].Quantity == ItemInfo.Quantity &&
            OperationsToConfirm[i].ItemId == ItemInfo.ItemId) // Corrected field names based on your updated struct
        {
            OperationsToConfirm.RemoveAt(i);
            return; // Early return if the operation is confirmed
        }
    }

    // RemainingItems variable to track how many items still need to be removed
    int32 RemainingItems = ItemInfo.Quantity;

    // Iterate through all displayed slots to remove items
    for (int32 SlotIndex = 0; SlotIndex < DisplayedSlots.Num() && RemainingItems > 0; ++SlotIndex)
    {
        // Check if the current slot contains the item we're looking to remove
        if (DisplayedSlots[SlotIndex].ItemId == ItemInfo.ItemId)
        {
            // Determine how many items we can remove from this slot
            int32 ItemsToRemove = FMath::Min(RemainingItems, DisplayedSlots[SlotIndex].Quantity);

            // Adjust the slot quantity and the remaining item count
            DisplayedSlots[SlotIndex].Quantity -= ItemsToRemove;
            RemainingItems -= ItemsToRemove;

            // If the slot is now empty, reset it to the default empty item instance
            if (DisplayedSlots[SlotIndex].Quantity <= 0)
            {
                DisplayedSlots[SlotIndex] = FRancItemInstance::EmptyItemInstance;
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

void URancInventorySlotMapper::HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInfo)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::RemoveTagged &&
			OperationsToConfirm[i].Quantity == ItemInfo.Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemInfo.ItemId) // Assuming ItemID is available
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	if (DisplayedTaggedSlots.Contains(SlotTag))
	{
		if (!DisplayedTaggedSlots[SlotTag].IsValid() || DisplayedTaggedSlots[SlotTag].ItemId != ItemInfo.ItemId)
		{
			UE_LOG(LogTemp, Warning, TEXT("Client misprediction detected in tagged slot %s"), *SlotTag.ToString());
			ForceFullUpdate();
			return;
		}
		
		// Update the quantity or remove the item if necessary
		DisplayedTaggedSlots[SlotTag].Quantity -= ItemInfo.Quantity;
		if (DisplayedTaggedSlots[SlotTag].Quantity <= 0)
		{
			DisplayedTaggedSlots[SlotTag] = FRancItemInstance::EmptyItemInstance; // Remove the item if quantity drops to 0 or below
		}

		OnTaggedSlotUpdated.Broadcast(SlotTag);
	}
}

void URancInventorySlotMapper::ForceFullUpdate()
{
	// TODO: Implement this, we prefer not to just call initialize as that will loose all slot mappings
}


int32 URancInventorySlotMapper::FindSlotIndexForItem(const FRancItemInstance& Item)
{
	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(Item.ItemId);
	for (int32 Index = 0; Index < DisplayedSlots.Num(); ++Index)
	{
		const FRancItemInstance& ExistingItem = DisplayedSlots[Index];

		if (!ExistingItem.ItemId.IsValid())
		{
			return Index;
		}
		if (ExistingItem.ItemId == Item.ItemId)
		{
			if (ItemData->bIsStackable && ExistingItem.Quantity < ItemData->MaxStackSize)
			{
				return Index;
			}
		}
	}

	return -1;
}

FGameplayTag URancInventorySlotMapper::FindTaggedSlotForItem(const FRancItemInstance& Item)
{
	// Validate
	if (!Item.IsValid()) return FGameplayTag::EmptyTag;

	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(Item.ItemId);
	if (!ItemData) return FGameplayTag::EmptyTag; // Ensure the item data is valid

	// Find which slot to move the item to based on item categories
	FGameplayTag FallbackSwapSlot = FGameplayTag::EmptyTag;
	TArray<FGameplayTag> ConsideredSlots = LinkedInventoryComponent->SpecializedTaggedSlots;
	ConsideredSlots.Append(LinkedInventoryComponent->UniversalTaggedSlots);

	for (const FGameplayTag& SlotTag : ConsideredSlots)
	{
		if (ItemData->ItemCategories.HasTag(SlotTag))
		{
			// Note: an item can be tagged to belong to both a specialized and/or a universal slot
			if (!FallbackSwapSlot.IsValid()) FallbackSwapSlot = SlotTag;

			if (!LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag).ItemInstance.IsValid())
			{
				return SlotTag;
			}
		}
	}

	// Attempt to swap with the fallback slot if one was identified
	if (!FallbackSwapSlot.IsValid())
	{
		if (LinkedInventoryComponent->UniversalTaggedSlots.Num() == 0) return FGameplayTag::EmptyTag;

		FallbackSwapSlot = LinkedInventoryComponent->UniversalTaggedSlots[0];
	}

	return FallbackSwapSlot;
}


const FRancItemInstance& URancInventorySlotMapper::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	return DisplayedTaggedSlots.FindChecked(SlotTag);
}

bool URancInventorySlotMapper::MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex)
{
	if (!LinkedInventoryComponent || (!SourceTaggedSlot.IsValid() && !DisplayedSlots.IsValidIndex(SourceSlotIndex)))
	{
		return false;
	}

	const bool SourceIsTagSlot = SourceTaggedSlot.IsValid();

	FRancItemInstance& SourceItem = const_cast<FRancItemInstance&>(FRancItemInstance::EmptyItemInstance);
	if (SourceIsTagSlot)
	{
		SourceItem = DisplayedTaggedSlots[SourceTaggedSlot];
	}
	else
	{
		SourceItem = DisplayedSlots[SourceSlotIndex];
	}

	if (!SourceItem.IsValid()) return false;

	const auto TargetSlot = FindTaggedSlotForItem(SourceItem);

	return MoveItem(SourceTaggedSlot, SourceSlotIndex, TargetSlot, -1);
}
