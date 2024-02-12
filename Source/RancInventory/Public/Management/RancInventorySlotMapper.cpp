#include "RancInventorySlotMapper.h"

#include "RancInventoryFunctions.h"
#include "Actors/AWorldItem.h"
#include "RancInventory/public/Components/RancInventoryComponent.h"
#include "WarTribes/InventorySystem/AWTWorldItem.h"

URancInventorySlotMapper::URancInventorySlotMapper(): LinkedInventoryComponent(nullptr)
{
	// Initialize any necessary members if needed
}


void URancInventorySlotMapper::Initialize(URancInventoryComponent* InventoryComponent, int32 NumSlots = 9)
{
	NumberOfSlots = NumSlots;
	LinkedInventoryComponent = InventoryComponent;
	SlotMappings.Empty();

	if (!LinkedInventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory Component is null"));
		return;
	}

	// Initialize SlotMappings with empty item info for the specified number of slots
	for (int32 i = 0; i < NumSlots; i++)
	{
		SlotMappings.Add(FRancItemInfo());
	}

	// Retrieve all items from the linked inventory component
	LinkedInventoryComponent->OnInventoryItemAdded.AddDynamic(this, &URancInventorySlotMapper::HandleItemAdded);
	LinkedInventoryComponent->OnInventoryItemRemoved.AddDynamic(this, &URancInventorySlotMapper::HandleItemRemoved);

	TArray<FRancItemInfo> Items = LinkedInventoryComponent->GetAllItems();

	for (FRancItemInfo ItemInfo : Items)
	{
		if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemById(ItemInfo.ItemId))
		{
			int32 Unadded = AddItems(ItemInfo);
			if (Unadded > 0)
			{
				// Drop the remaining items if we couldn't add them all
				SuppressCallback = true;
				LinkedInventoryComponent->DropItems(FRancItemInfo(ItemInfo.ItemId, Unadded));
				SuppressCallback = false;
				UE_LOG(LogTemp, Warning, TEXT("Dropped %d items as the slotmapper could not handle it"), Unadded);
			}
		}
	}
}


bool URancInventorySlotMapper::IsSlotEmpty(int32 SlotIndex) const
{
	return SlotMappings.IsValidIndex(SlotIndex) && !SlotMappings[SlotIndex].ItemId.IsValid();
}

FRancItemInfo URancInventorySlotMapper::GetItem(int32 SlotIndex) const
{
	if (SlotMappings.IsValidIndex(SlotIndex))
	{
		return SlotMappings[SlotIndex];
	}

	// Return an empty item info if the slot index is invalid
	return FRancItemInfo();
}

void URancInventorySlotMapper::RemoveItemsFromSlot(const FRancItemInfo& ItemToRemove, int32 SlotIndex)
{
	if (!LinkedInventoryComponent || !SlotMappings.IsValidIndex(SlotIndex)) return;

	FRancItemInfo& SlotItem = SlotMappings[SlotIndex];
	if (SlotItem.ItemId == ItemToRemove.ItemId && SlotItem.Quantity >= ItemToRemove.Quantity)
	{
		SlotItem.Quantity -= ItemToRemove.Quantity;

		SuppressCallback = true;
		LinkedInventoryComponent->RemoveItems(ItemToRemove);
		SuppressCallback = false;

		// If all items are removed, reset the slot
		if (SlotItem.Quantity <= 0)
		{
			SlotItem = FRancItemInfo(); // Reset the slot to empty
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Slot %d does not contain enough items to remove."), SlotIndex);
	}

	OnSlotUpdated.Broadcast(SlotIndex);
}

void URancInventorySlotMapper::RemoveItems(const FRancItemInfo& ItemToRemove)
{
	if (!LinkedInventoryComponent) return;

	int32 RemainingToRemove = ItemToRemove.Quantity;

	for (int32 i = 0; i < SlotMappings.Num(); ++i)
	{
		auto& SlotItem = SlotMappings[i];
		if (RemainingToRemove <= 0) break; // Stop if we've removed the required quantity

		if (SlotItem.ItemId == ItemToRemove.ItemId && SlotItem.Quantity > 0)
		{
			int32 RemoveCount = FMath::Min(SlotItem.Quantity, RemainingToRemove);
			SlotItem.Quantity -= RemoveCount;
			RemainingToRemove -= RemoveCount;

			if (SlotItem.Quantity <= 0)
			{
				SlotItem = FRancItemInfo(); // Reset the slot to empty if all items are removed
			}
		}
		OnSlotUpdated.Broadcast(i);
	}

	if (RemainingToRemove > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not enough items to remove. %d remaining."), RemainingToRemove);
	}
	else
	{
		SuppressCallback = true;
		// If we successfully removed items, update the linked inventory
		LinkedInventoryComponent->RemoveItems(FRancItemInfo(ItemToRemove.ItemId,
		                                                    ItemToRemove.Quantity - RemainingToRemove));
		SuppressCallback = false;
	}
}

void URancInventorySlotMapper::SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent) return;
	if (!SlotMappings.IsValidIndex(SourceSlotIndex)) return;
	if (Quantity <= 0) return;

	FRancItemInfo& SourceItem = SlotMappings[SourceSlotIndex];
	if (SourceItem.Quantity < Quantity) return; // Not enough items in the source slot to split

	if (SlotMappings.IsValidIndex(TargetSlotIndex))
	{
		// If the target slot is valid and matches the item type, add to it
		FRancItemInfo& TargetItem = SlotMappings[TargetSlotIndex];
		if (TargetItem.ItemId == SourceItem.ItemId)
		{
			TargetItem.Quantity += Quantity;
		}
		else if (!TargetItem.ItemId.IsValid())
		{
			// If the target slot is empty, move the specified quantity
			TargetItem = FRancItemInfo(SourceItem.ItemId, Quantity);
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
		FRancItemInfo NewItem(SourceItem.ItemId, Quantity);
		SlotMappings.Add(NewItem);
	}

	// Update the source slot quantity
	SourceItem.Quantity -= Quantity;
	if (SourceItem.Quantity <= 0)
	{
		// If all items have been moved, reset the source slot
		SourceItem = FRancItemInfo();
	}

	OnSlotUpdated.Broadcast(SourceSlotIndex);
	OnSlotUpdated.Broadcast(TargetSlotIndex);
}

int32 URancInventorySlotMapper::DropItem(int32 SlotIndex, int32 Count)
{
	if (!LinkedInventoryComponent || !SlotMappings.IsValidIndex(SlotIndex)) return false;

	Count = FMath::Min(Count, SlotMappings[SlotIndex].Quantity);

	SuppressCallback = true;
	int32 DroppedCount = LinkedInventoryComponent->DropItems(FRancItemInfo(SlotMappings[SlotIndex].ItemId, Count));
	SuppressCallback = false;

	if (DroppedCount > 0)
	{
		SlotMappings[SlotIndex].Quantity -= DroppedCount;
		if (SlotMappings[SlotIndex].Quantity <= 0)
		{
			SlotMappings[SlotIndex] = FRancItemInfo(); // Reset the slot to empty
		}
		OnSlotUpdated.Broadcast(SlotIndex);
	}

	return DroppedCount;
}

void URancInventorySlotMapper::MoveItem(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
	if (!LinkedInventoryComponent || !SlotMappings.IsValidIndex(SourceSlotIndex) || !SlotMappings.
		IsValidIndex(TargetSlotIndex) || SourceSlotIndex == TargetSlotIndex)
	{
		return; // Validate indices and ensure they are not the same
	}

	auto& SourceItem = SlotMappings[SourceSlotIndex];
	auto TargetItem = SlotMappings[TargetSlotIndex]; // not a ref
	const URancItemData* SourceData = URancInventoryFunctions::GetItemById(SourceItem.ItemId);
	const URancItemData* TargetData = URancInventoryFunctions::GetItemById(TargetItem.ItemId);

	if (!SourceData) return; // Ensure source data is valid

	if (IsSlotEmpty(TargetSlotIndex))
	{
		SlotMappings[TargetSlotIndex] = SourceItem;
		SlotMappings[SourceSlotIndex] = FRancItemInfo(); // Clear source slot
	}
	else if (SourceItem.ItemId != TargetItem.ItemId || !TargetData->bIsStackable)
	{
		SlotMappings[TargetSlotIndex] = SourceItem;
		SlotMappings[SourceSlotIndex] = TargetItem;
	}
	else
	{
		// Stack items up to max capacity
		int32 AvailableSpace = TargetData->MaxStackSize - TargetItem.Quantity;
		int32 TransferAmount = FMath::Min(AvailableSpace, SourceItem.Quantity);
		SlotMappings[TargetSlotIndex].Quantity += TransferAmount;
		SlotMappings[SourceSlotIndex].Quantity -= TransferAmount;

		if (SourceItem.Quantity <= 0)
		{
			SlotMappings[SourceSlotIndex] = FRancItemInfo(); // Clear source slot if emptied
		}
	}

	OnSlotUpdated.Broadcast(SourceSlotIndex);
	OnSlotUpdated.Broadcast(TargetSlotIndex);
}

int32 URancInventorySlotMapper::AddItems(const FRancItemInfo& ItemInfo)
{
	if (!LinkedInventoryComponent->CanReceiveItem(ItemInfo)) return ItemInfo.Quantity;

	int32 RemainingItems = ItemInfo.Quantity;
	for (int32 Index = 0; Index < SlotMappings.Num(); ++Index)
	{
		RemainingItems = AddItemToSlot(FRancItemInfo(ItemInfo.ItemId, RemainingItems), Index);
		if (RemainingItems <= 0)
		{
			break;
		}
	}

	return RemainingItems;
}


int32 URancInventorySlotMapper::AddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex)
{
	return AddItemToSlotImplementation(ItemInfo, SlotIndex, true);
}

int32 URancInventorySlotMapper::AddItemToSlotImplementation(const FRancItemInfo& ItemInfo, int32 SlotIndex,
                                                            bool PushUpdates = true)
{
	if (PushUpdates && !LinkedInventoryComponent->CanReceiveItem(ItemInfo)) return ItemInfo.Quantity;

	if (!ItemInfo.ItemId.IsValid() || !SlotMappings.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid item or slot index"));
		return ItemInfo.Quantity;
	}

	int RemainingItems = ItemInfo.Quantity;

	const URancItemData* ItemData = URancInventoryFunctions::GetItemById(ItemInfo.ItemId);
	if (!ItemData || !ItemData->bIsStackable)
	{
		// if slot not empty return all items
		if (!IsSlotEmpty(SlotIndex))
		{
			return ItemInfo.Quantity;
		}

		SlotMappings[SlotIndex] = FRancItemInfo(ItemInfo.ItemId, 1);
		RemainingItems = FMath::Max(0, ItemInfo.Quantity - 1);
	}
	else
	{
		auto& SlotItem = SlotMappings[SlotIndex];
		// Stackable items logic
		if (IsSlotEmpty(SlotIndex))
		{
			int32 ToAdd = FMath::Min(ItemInfo.Quantity, ItemData->MaxStackSize);
			SlotMappings[SlotIndex] = FRancItemInfo(ItemInfo.ItemId, ToAdd);
			RemainingItems = FMath::Max(0, ItemInfo.Quantity - ToAdd);
		}
		else if (SlotItem.ItemId == ItemInfo.ItemId)
		{
			int32 TotalQuantity = SlotItem.Quantity + ItemInfo.Quantity;
			if (TotalQuantity <= ItemData->MaxStackSize)
			{
				SlotItem.Quantity = TotalQuantity;
				RemainingItems = 0;
			}
			else
			{
				SlotItem.Quantity = ItemData->MaxStackSize;

				// returns remaining items that we could not add
				RemainingItems = TotalQuantity - ItemData->MaxStackSize;
			}
		}
		else
		{
			// Can't add
			return ItemInfo.Quantity;
		}
	}

	if (PushUpdates)
	{
		SuppressCallback = true;
		LinkedInventoryComponent->AddItems(FRancItemInfo(ItemInfo.ItemId, ItemInfo.Quantity - RemainingItems));
		SuppressCallback = false;
		OnSlotUpdated.Broadcast(SlotIndex);
	}

	return RemainingItems;
}

bool URancInventorySlotMapper::CanAddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex) const
{
	if (!SlotMappings.IsValidIndex(SlotIndex))
	{
		return false; // Slot index out of bounds
	}

	bool TargetSlotEmpty = IsSlotEmpty(SlotIndex);

	const FRancItemInfo& TargetSlotItem = SlotMappings[SlotIndex];
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemInfo.ItemId)
	{
		const URancItemData* ItemData = URancInventoryFunctions::GetItemById(ItemInfo.ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		int32 AvailableSpace = ItemData->bIsStackable ? ItemData->MaxStackSize - TargetSlotItem.Quantity : 0;
		return AvailableSpace >= ItemInfo.Quantity;
	}

	return false; // Different item types or slot not stackable
}

void URancInventorySlotMapper::HandleItemRemoved(const FRancItemInfo& ItemInfo)
{
	if (SuppressCallback) return;

	int QuantityToRemove = ItemInfo.Quantity;
	// Loop through each slot in reverse and remove as much as quantity as we can from the slot, call OnSlotUpdated and move on to the next slot if we havent removed enough yet
	for (int32 i = SlotMappings.Num() - 1; i >= 0; --i)
	{
		auto& SlotItem = SlotMappings[i];
		if (SlotItem.ItemId == ItemInfo.ItemId)
		{
			if (SlotItem.Quantity > ItemInfo.Quantity)
			{
				SlotItem.Quantity -= ItemInfo.Quantity;
				OnSlotUpdated.Broadcast(i);
				break;
			}

			QuantityToRemove -= SlotItem.Quantity;
			SlotItem = FRancItemInfo();
			OnSlotUpdated.Broadcast(i);

			if (QuantityToRemove == 0)
			{
				break;
			}
		}
	}
}

void URancInventorySlotMapper::HandleItemAdded(const FRancItemInfo& ItemInfo)
{
	if (SuppressCallback) return;

	int32 RemainingItems = ItemInfo.Quantity;
	for (int32 Index = 0; Index < SlotMappings.Num(); ++Index)
	{
		int32 oldRemaining = RemainingItems;
		RemainingItems = AddItemToSlotImplementation(FRancItemInfo(ItemInfo.ItemId, RemainingItems), Index, false);
		if (oldRemaining > RemainingItems)
		{
			OnSlotUpdated.Broadcast(Index);
		}
		if (RemainingItems <= 0)
		{
			break;
		}
	}

	if (RemainingItems > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not add all items to the slot mapper. %d remaining."), RemainingItems);
	}
}
