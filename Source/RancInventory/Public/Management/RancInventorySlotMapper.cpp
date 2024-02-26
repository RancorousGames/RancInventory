#include "RancInventorySlotMapper.h"

#include "RancInventoryFunctions.h"
#include "Actors/AWorldItem.h"
#include "RancInventory/public/Components/RancInventoryComponent.h"
#include "WarTribes/InventorySystem/AWTWorldItem.h"

URancInventorySlotMapper::URancInventorySlotMapper(): LinkedInventoryComponent(nullptr)
{
	// Initialize any necessary members if needed
}


void URancInventorySlotMapper::Initialize(URancInventoryComponent* InventoryComponent, int32 NumSlots,  bool bPreferEmptyUniversalSlots)
{
	NumberOfSlots = NumSlots;
	PreferEmptyUniversalSlots = bPreferEmptyUniversalSlots;
	LinkedInventoryComponent = InventoryComponent;
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

	LinkedInventoryComponent->OnItemAddedToContainer.AddDynamic(this, &URancInventorySlotMapper::HandleItemAdded);
	LinkedInventoryComponent->OnItemRemovedFromContainer.AddDynamic(this, &URancInventorySlotMapper::HandleItemRemoved);
	LinkedInventoryComponent->OnItemAddedToTaggedSlot.
	                          AddDynamic(this, &URancInventorySlotMapper::HandleTaggedItemAdded);
	LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(
		this, &URancInventorySlotMapper::HandleTaggedItemRemoved);


	TArray<FRancItemInstance> Items = LinkedInventoryComponent->GetAllContainerItems();

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
	return !DisplayedSlots.IsValidIndex(SlotIndex) || !DisplayedSlots[SlotIndex].ItemId.IsValid();
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

bool URancInventorySlotMapper::SplitItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent) return false;

	FRancItemInstance SourceItem;
	if (SourceTaggedSlot.IsValid())
	{
		SourceItem = GetItemForTaggedSlot(SourceTaggedSlot);
	}
	else if (DisplayedSlots.IsValidIndex(SourceSlotIndex))
	{
		SourceItem = GetItem(SourceSlotIndex);
	}
	else
	{
		return false; // Invalid source
	}

	if (SourceItem.Quantity < Quantity) return false; // Not enough items in source

	FRancItemInstance TargetItem;
	if (TargetTaggedSlot.IsValid())
	{
		if (DisplayedTaggedSlots.Contains(TargetTaggedSlot))
			TargetItem = GetItemForTaggedSlot(TargetTaggedSlot);
		else
			return false; // Invalid target
	}
	else if (DisplayedSlots.IsValidIndex(TargetSlotIndex))
	{
		TargetItem = GetItem(TargetSlotIndex);
	}
	else
	{
		return false; // Invalid target
	}

	// If target slot is occupied by a different item, fail the operation
	if (TargetItem.ItemId.IsValid() && TargetItem.ItemId != SourceItem.ItemId)
	{
		return false;
	}

	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(SourceItem.ItemId);
	if (!ItemData) return false; // Item data not found

	// Calculate total quantity after split and check against max stack size
	int32 TotalQuantityAfterSplit = TargetItem.Quantity + Quantity;
	if (TotalQuantityAfterSplit > ItemData->MaxStackSize) return false; // Exceeds max stack size

	// Perform the split
	if (SourceTaggedSlot.IsValid())
	{
		DisplayedTaggedSlots[SourceTaggedSlot].Quantity -= Quantity;
		if (DisplayedTaggedSlots[SourceTaggedSlot].Quantity <= 0)
			DisplayedTaggedSlots[SourceTaggedSlot] = FRancItemInstance(); // Reset if empty
	}
	else
	{
		DisplayedSlots[SourceSlotIndex].Quantity -= Quantity;
		if (DisplayedSlots[SourceSlotIndex].Quantity <= 0)
			DisplayedSlots[SourceSlotIndex] = FRancItemInstance(); // Reset if empty
	}

	if (TargetTaggedSlot.IsValid())
	{
		if (DisplayedTaggedSlots[TargetTaggedSlot].IsValid())
		{
			DisplayedTaggedSlots[TargetTaggedSlot].Quantity += Quantity;
		}
		else
		{
			DisplayedTaggedSlots[TargetTaggedSlot] = FRancItemInstance(SourceItem.ItemId, Quantity);
		}
	}
	else
	{
		if (DisplayedSlots[TargetSlotIndex].IsValid())
		{
			DisplayedSlots[TargetSlotIndex].Quantity += Quantity;
		}
		else
		{
			DisplayedSlots[TargetSlotIndex] = FRancItemInstance(SourceItem.ItemId, Quantity);
		}
	}

	const bool IsPureSplit = !SourceTaggedSlot.IsValid() && !TargetTaggedSlot.IsValid();
	
	// Broadcast updates
	if (SourceTaggedSlot.IsValid())
	{
		OperationsToConfirm.Emplace(FExpectedOperation(RemoveTagged, SourceTaggedSlot, SourceItem.ItemId, Quantity));
		OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
	}
	else
	{
		if (!IsPureSplit) OperationsToConfirm.Emplace(FExpectedOperation(Remove, SourceItem.ItemId, Quantity));
		OnSlotUpdated.Broadcast(SourceSlotIndex);
	}

	if (TargetTaggedSlot.IsValid())
	{
		OperationsToConfirm.Emplace(FExpectedOperation(AddTagged, TargetTaggedSlot, SourceItem.ItemId, Quantity));
		if (SourceTaggedSlot.IsValid())
			LinkedInventoryComponent->MoveItems_Server(FRancItemInstance(SourceItem.ItemId, Quantity), SourceTaggedSlot, TargetTaggedSlot);
		else
			LinkedInventoryComponent->MoveItems_Server(FRancItemInstance(SourceItem.ItemId, Quantity), FGameplayTag::EmptyTag, TargetTaggedSlot);
		OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
	}
	else
	{
		if (!IsPureSplit)
		{
			OperationsToConfirm.Emplace(FExpectedOperation(Add, SourceItem.ItemId, Quantity));
			LinkedInventoryComponent->MoveItems_Server(FRancItemInstance(SourceItem.ItemId, Quantity), SourceTaggedSlot, FGameplayTag::EmptyTag);
		}
		OnSlotUpdated.Broadcast(TargetSlotIndex);
	}

	return true;
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

bool URancInventorySlotMapper::MoveItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex,
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
	bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid();
	bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid();

	FRancItemInstance* SourceItem = nullptr;
	if (SourceIsTaggedSlot)
	{
		SourceItem = DisplayedTaggedSlots.Find(SourceTaggedSlot);

		if (!SourceItem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Source tagged slot does not exist"));
			return 0;
		}
	}
	else
	{
		SourceItem = &DisplayedSlots[SourceSlotIndex];
	}

	FRancItemInstance* TargetItem;
	if (TargetIsTaggedSlot)
	{
		if (!LinkedInventoryComponent->IsTaggedSlotCompatible(SourceItem->ItemId, TargetTaggedSlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
			return false;
		}

		TargetItem = DisplayedTaggedSlots.Find(TargetTaggedSlot);
		if (!TargetItem)
		{
			if (!LinkedInventoryComponent->UniversalTaggedSlots.Contains(TargetTaggedSlot) && !LinkedInventoryComponent->SpecializedTaggedSlots.Contains(TargetTaggedSlot))
			{
				UE_LOG(LogTemp, Warning, TEXT("Target tagged slot does not exist"));
				return false;
			}

			DisplayedTaggedSlots.Add(TargetTaggedSlot, FRancItemInstance::EmptyItemInstance);
			TargetItem = DisplayedTaggedSlots.Find(TargetTaggedSlot);
		}
	}
	else
	{
		TargetItem = &DisplayedSlots[TargetSlotIndex];
	}

	if (TargetItem && SourceIsTaggedSlot && URancInventoryFunctions::ShouldItemsBeSwapped(SourceItem, TargetItem) && !LinkedInventoryComponent->IsTaggedSlotCompatible(
		TargetItem->ItemId, SourceTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the source slot"));
		return 0;
	}

	FRancItemInstance MoveItem = FRancItemInstance(SourceItem->ItemId, SourceItem->Quantity);

	const int32 MovedQuantity = URancInventoryFunctions::MoveBetweenSlots(SourceItem, TargetItem, !TargetIsTaggedSlot, SourceItem->Quantity, true);

	MoveItem.Quantity = MovedQuantity;
	
	if (MovedQuantity > 0)
	{
		if (SourceIsTaggedSlot)
		{
			OperationsToConfirm.Emplace(FExpectedOperation(RemoveTagged, SourceTaggedSlot, MoveItem.ItemId, MovedQuantity));
			OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
			if (TargetIsTaggedSlot)
			{
				OperationsToConfirm.Emplace(FExpectedOperation(AddTagged, TargetTaggedSlot,  MoveItem.ItemId, MovedQuantity));
				OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
			}
			else
			{
				OperationsToConfirm.Emplace(FExpectedOperation(Add,  MoveItem.ItemId, MovedQuantity));
				OnSlotUpdated.Broadcast(TargetSlotIndex);
			}
		}
		else
		{
			if (TargetIsTaggedSlot)
			{
				OperationsToConfirm.Emplace(FExpectedOperation(Remove,  MoveItem.ItemId, MovedQuantity));
				OperationsToConfirm.Emplace(FExpectedOperation(AddTagged, TargetTaggedSlot,  MoveItem.ItemId, MovedQuantity));
				OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
			}
			else // purely visual
			{
				OnSlotUpdated.Broadcast(TargetSlotIndex);
			}
			OnSlotUpdated.Broadcast(SourceSlotIndex);
		}
	}

	if (TargetIsTaggedSlot || SourceIsTaggedSlot) // If its not a purely visual move
	{
		// now request the move on the server
		LinkedInventoryComponent->MoveItems_Server(MoveItem, SourceTaggedSlot, TargetTaggedSlot);
	}

	return true;
}


bool URancInventorySlotMapper::CanSlotReceiveItem(const FRancItemInstance& ItemInstance, int32 SlotIndex) const
{
	if (!DisplayedSlots.IsValidIndex(SlotIndex))
	{
		return false; // Slot index out of bounds
	}

	if (!LinkedInventoryComponent->CanContainerReceiveItems(ItemInstance)) return false;

	const bool TargetSlotEmpty = IsSlotEmpty(SlotIndex);

	const FRancItemInstance& TargetSlotItem = DisplayedSlots[SlotIndex];
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemInstance.ItemId)
	{
		const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemInstance.ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		const int32 AvailableSpace = ItemData->bIsStackable ? ItemData->MaxStackSize - TargetSlotItem.Quantity : TargetSlotEmpty ? 1 : 0;
		return AvailableSpace >= ItemInstance.Quantity;
	}

	return false; // Different item types or slot not stackable
}

bool URancInventorySlotMapper::CanTaggedSlotReceiveItem(const FRancItemInstance& ItemInstance, const FGameplayTag& SlotTag, bool CheckContainerLimits) const
{
	bool BasicCheck = LinkedInventoryComponent->IsTaggedSlotCompatible(ItemInstance.ItemId, SlotTag) && (!CheckContainerLimits || LinkedInventoryComponent->
		CanContainerReceiveItems(ItemInstance));

	if (!BasicCheck) return false;

	const bool TargetSlotEmpty = IsTaggedSlotEmpty(SlotTag);
	const FRancItemInstance& TargetSlotItem = DisplayedTaggedSlots.FindChecked(SlotTag);
	if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemInstance.ItemId)
	{
		const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemInstance.ItemId);
		if (!ItemData)
		{
			return false; // Item data not found
		}

		const int32 AvailableSpace = ItemData->bIsStackable ? ItemData->MaxStackSize - TargetSlotItem.Quantity : TargetSlotEmpty ? 1 : 0;
		return AvailableSpace >= ItemInstance.Quantity;
	}

	return false;
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
		int32 ItemsToAdd = FMath::Min(RemainingItems, ItemData->bIsStackable ? ItemData->MaxStackSize : 1);
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

void URancInventorySlotMapper::HandleTaggedItemAdded(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInstance)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::AddTagged &&
			OperationsToConfirm[i].Quantity == ItemInstance.Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemInstance.ItemId)
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	// Directly add the item to the tagged slot without overflow check
	if (DisplayedTaggedSlots[SlotTag].ItemId == ItemInstance.ItemId)
		DisplayedTaggedSlots[SlotTag].Quantity += ItemInstance.Quantity;
	else
		DisplayedTaggedSlots[SlotTag] = ItemInstance;
	OnTaggedSlotUpdated.Broadcast(SlotTag);
}

void URancInventorySlotMapper::HandleItemRemoved(const FRancItemInstance& ItemInstance)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::Remove &&
			OperationsToConfirm[i].Quantity == ItemInstance.Quantity &&
			OperationsToConfirm[i].ItemId == ItemInstance.ItemId) // Corrected field names based on your updated struct
		{
			OperationsToConfirm.RemoveAt(i);
			return; // Early return if the operation is confirmed
		}
	}

	// RemainingItems variable to track how many items still need to be removed
	int32 RemainingItems = ItemInstance.Quantity;

	// Iterate through all displayed slots to remove items
	for (int32 SlotIndex = 0; SlotIndex < DisplayedSlots.Num() && RemainingItems > 0; ++SlotIndex)
	{
		// Check if the current slot contains the item we're looking to remove
		if (DisplayedSlots[SlotIndex].ItemId == ItemInstance.ItemId)
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

void URancInventorySlotMapper::HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInstance)
{
	for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
	{
		if (OperationsToConfirm[i].Operation == SlotOperation::RemoveTagged &&
			OperationsToConfirm[i].Quantity == ItemInstance.Quantity &&
			OperationsToConfirm[i].TaggedSlot == SlotTag &&
			OperationsToConfirm[i].ItemId == ItemInstance.ItemId) // Assuming ItemID is available
		{
			OperationsToConfirm.RemoveAt(i);
			return;
		}
	}

	if (DisplayedTaggedSlots.Contains(SlotTag))
	{
		if (!DisplayedTaggedSlots[SlotTag].IsValid() || DisplayedTaggedSlots[SlotTag].ItemId != ItemInstance.ItemId)
		{
			UE_LOG(LogTemp, Warning, TEXT("Client misprediction detected in tagged slot %s"), *SlotTag.ToString());
			ForceFullUpdate();
			return;
		}

		// Update the quantity or remove the item if necessary
		DisplayedTaggedSlots[SlotTag].Quantity -= ItemInstance.Quantity;
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

const FRancItemInstance& URancInventorySlotMapper::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	return DisplayedTaggedSlots.FindChecked(SlotTag);
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

FGameplayTag URancInventorySlotMapper::FindTaggedSlotForItem(const FRancItemInstance& Item) const
{
	// Validate
	if (!Item.IsValid()) return FGameplayTag::EmptyTag;

	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(Item.ItemId);
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
	for (const FGameplayTag& SlotTag : LinkedInventoryComponent->UniversalTaggedSlots)
	{
		if (IsTaggedSlotEmpty(SlotTag))
		{
			if (!FallbackSwapSlot.IsValid()) FallbackSwapSlot = SlotTag;

			if (ItemData->ItemCategories.HasTag(SlotTag))
			{
				if (IsTaggedSlotEmpty(SlotTag))
				{
					return SlotTag;
				}
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

bool URancInventorySlotMapper::MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex)
{
	if (!LinkedInventoryComponent || (!SourceTaggedSlot.IsValid() && !DisplayedSlots.IsValidIndex(SourceSlotIndex)))
	{
		return false;
	}

	const bool SourceIsTagSlot = SourceTaggedSlot.IsValid();

	const FRancItemInstance* SourceItem = nullptr;
	if (SourceIsTagSlot)
	{
		SourceItem = &DisplayedTaggedSlots[SourceTaggedSlot];
	}
	else
	{
		SourceItem = &DisplayedSlots[SourceSlotIndex];
	}

	if (!SourceItem || !SourceItem->IsValid()) return false;

	const auto TargetSlot = FindTaggedSlotForItem(*SourceItem);

	return MoveItems(SourceTaggedSlot, SourceSlotIndex, TargetSlot, -1);
}
