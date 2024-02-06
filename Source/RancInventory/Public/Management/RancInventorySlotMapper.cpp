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
	for(int32 i = 0; i < NumSlots; i++)
	{
		SlotMappings.Add(FRancItemInfo());
	}

	// Retrieve all items from the linked inventory component
	TArray<FRancItemInfo> Items = LinkedInventoryComponent->GetAllItems();

	// Assuming a helper function exists that can get item data by ID
	for(FRancItemInfo Item : Items)
	{
		const URancItemData* ItemData = URancInventoryFunctions::GetSingleItemDataById(Item.ItemId, { "Data" });
		if(ItemData != nullptr)
		{
			int32 CurrentIndex = 0;
			while(Item.Quantity > 0 && CurrentIndex < NumSlots)
			{
				// Check if the slot is empty or contains the same item, and it's not full
				if(IsSlotEmpty(CurrentIndex) || 
				  (SlotMappings[CurrentIndex].ItemId == Item.ItemId && SlotMappings[CurrentIndex].Quantity < ItemData->MaxStackSize))
				{
					int32 AvailableSpace = ItemData->MaxStackSize - SlotMappings[CurrentIndex].Quantity;
					int32 TransferAmount = FMath::Min(AvailableSpace, Item.Quantity);
                    
					// Update SlotMapping and Item quantities
					SlotMappings[CurrentIndex].ItemId = Item.ItemId;
					SlotMappings[CurrentIndex].Quantity += TransferAmount;
					Item.Quantity -= TransferAmount;
				}
				CurrentIndex++;
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
		LinkedInventoryComponent->RemoveItems(ItemToRemove);

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
		// If we successfully removed items, update the linked inventory
		LinkedInventoryComponent->RemoveItems(FRancItemInfo(ItemToRemove.ItemId,
		                                                    ItemToRemove.Quantity - RemainingToRemove));
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

bool URancInventorySlotMapper::DropItem(int32 SlotIndex, int32 Count)
{
	if (!LinkedInventoryComponent || !SlotMappings.IsValidIndex(SlotIndex)) return false;

	auto& ItemInfo = SlotMappings[SlotIndex];
	Count = FMath::Min(Count, ItemInfo.Quantity); // Ensure we don't drop more than we have

	// Adjust the quantity or clear the slot if necessary
	if (ItemInfo.Quantity > Count)
	{
		ItemInfo.Quantity -= Count;
	}
	else
	{
		Count = ItemInfo.Quantity;
		ItemInfo = FRancItemInfo(); // Reset the slot if all items are dropped
	}

	// Spawn the world item
	if (UWorld* World = GetWorld())
	{
		auto* owner = LinkedInventoryComponent->GetOwner();
		if (AWorldItem* WorldItem = World->SpawnActor<AWorldItem>(AWorldItem::StaticClass(),
		                                                          owner->
		                                                          GetActorLocation() + owner->GetActorForwardVector() * DropDistance, FRotator::ZeroRotator))
		{
			WorldItem->Item = ItemInfo;
			WorldItem->SetReplicates(true);

			OnSlotUpdated.Broadcast(SlotIndex);
			return true;
		}
	}

	return false;
}

void URancInventorySlotMapper::MoveItem(int32 SourceSlotIndex, int32 TargetSlotIndex) {
	if (!LinkedInventoryComponent || !SlotMappings.IsValidIndex(SourceSlotIndex) || !SlotMappings.IsValidIndex(TargetSlotIndex) || SourceSlotIndex == TargetSlotIndex) {
		return; // Validate indices and ensure they are not the same
	}

	auto& SourceItem = SlotMappings[SourceSlotIndex];
	auto& TargetItem = SlotMappings[TargetSlotIndex];
	const URancItemData* SourceData = URancInventoryFunctions::GetSingleItemDataById(SourceItem.ItemId, { "Data" });
	const URancItemData* TargetData = URancInventoryFunctions::GetSingleItemDataById(TargetItem.ItemId, { "Data" });

	if (!SourceData) return; // Ensure source data is valid

	if (IsSlotEmpty(TargetSlotIndex))
	{
		SlotMappings[TargetSlotIndex] = SourceItem;
		SlotMappings[SourceSlotIndex] = FRancItemInfo(); // Clear source slot
	}
	else if (SourceItem.ItemId != TargetItem.ItemId ||  !TargetData->bIsStackable)
	{
		SlotMappings[TargetSlotIndex] = SourceItem;
		SlotMappings[SourceSlotIndex] = TargetItem;
	}
	else
	{
		// Stack items up to max capacity
		int32 AvailableSpace = TargetData->MaxStackSize - TargetItem.Quantity;
		int32 TransferAmount = FMath::Min(AvailableSpace, SourceItem.Quantity);
		TargetItem.Quantity += TransferAmount;
		SourceItem.Quantity -= TransferAmount;

		if (SourceItem.Quantity <= 0) {
			SlotMappings[SourceSlotIndex] = FRancItemInfo(); // Clear source slot if emptied
		}
	}

	OnSlotUpdated.Broadcast(SourceSlotIndex);
	OnSlotUpdated.Broadcast(TargetSlotIndex);
}

void URancInventorySlotMapper::AddItems(const FRancItemInfo& ItemInfo) {
	for (int32 Index = 0; Index < SlotMappings.Num(); ++Index) {
		if (CanAddItemToSlot(ItemInfo, Index)) {
			AddItemToSlot(ItemInfo, Index);
			return;
		}
	}
}

void URancInventorySlotMapper::AddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex) {
	if (!SlotMappings.IsValidIndex(SlotIndex)) return;

	const URancItemData* ItemData = URancInventoryFunctions::GetSingleItemDataById(ItemInfo.ItemId, { "Data" });
	if (!ItemData || !ItemData->bIsStackable) {
		SlotMappings[SlotIndex] = ItemInfo; // Non-stackable items or new items replace the slot content
	} else {
		// Stackable items logic
		auto& SlotItem = SlotMappings[SlotIndex];
		if (SlotItem.ItemId == ItemInfo.ItemId) {
			int32 TotalQuantity = SlotItem.Quantity + ItemInfo.Quantity;
			if (TotalQuantity <= ItemData->MaxStackSize) {
				SlotItem.Quantity = TotalQuantity;
			} else {
				SlotItem.Quantity = ItemData->MaxStackSize;
				// Handle overflow logic if needed
			}
		} else {
			SlotMappings[SlotIndex] = ItemInfo; // Replace if different items
		}
	}
	
	OnSlotUpdated.Broadcast(SlotIndex);
}

bool URancInventorySlotMapper::CanAddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex) const {
	if (!SlotMappings.IsValidIndex(SlotIndex)) {
		return false; // Slot index out of bounds
	}

	bool TargetSlotEmpty = IsSlotEmpty(SlotIndex);

	const FRancItemInfo& TargetSlotItem = SlotMappings[SlotIndex];
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemInfo.ItemId) {
		const URancItemData* ItemData = URancInventoryFunctions::GetSingleItemDataById(ItemInfo.ItemId, { "Data" });
		if (!ItemData) {
			return false; // Item data not found
		}

		int32 AvailableSpace = ItemData->bIsStackable ? ItemData->MaxStackSize - TargetSlotItem.Quantity : 0;
		return AvailableSpace >= ItemInfo.Quantity;
	}

	return false; // Different item types or slot not stackable
}