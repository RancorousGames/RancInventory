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

	for (FGameplayTag& Tag : LinkedInventoryComponent->UniversalTaggedSlots)
	{
		ViewableTaggedSlots.Add(Tag, FTaggedItemBundle());
	}
	for (FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots)
	{
		ViewableTaggedSlots.Add(Tag, FTaggedItemBundle());
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

bool URISGridViewModel::SplitItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity)
{
	if (!LinkedInventoryComponent) return false;

	FGenericItemBundle SourceItem;
	if (SourceTaggedSlot.IsValid())
	{
		SourceItem = &GetItemForTaggedSlot(SourceTaggedSlot);
	}
	else if (ViewableGridSlots.IsValidIndex(SourceSlotIndex))
	{
		SourceItem = &ViewableGridSlots[SourceSlotIndex];
	}
	else
	{
		return false; // Invalid source
	}

	if (SourceItem.GetQuantity() < Quantity) return false; // Not enough items in source

	FGenericItemBundle TargetItem;
	if (TargetTaggedSlot.IsValid())
	{
		if (ViewableTaggedSlots.Contains(TargetTaggedSlot))
			TargetItem = &GetItemForTaggedSlot(TargetTaggedSlot);
		else
			return false; // Invalid target
	}
	else if (ViewableGridSlots.IsValidIndex(TargetSlotIndex))
	{
		TargetItem = &ViewableGridSlots[TargetSlotIndex];
	}
	else
	{
		return false; // Invalid target
	}

	// If target slot is occupied by a different item, fail the operation
	if (TargetItem.IsValid() && TargetItem.GetItemId() != SourceItem.GetItemId())
	{
		return false;
	}

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(SourceItem.GetItemId());
	if (!ItemData) return false; // Item data not found

	// Calculate total quantity after split and check against max stack size
	int32 TotalQuantityAfterSplit = TargetItem.GetQuantity() + Quantity;
	if (TotalQuantityAfterSplit > ItemData->MaxStackSize) return false; // Exceeds max stack size

	// Perform the split
	if (SourceTaggedSlot.IsValid())
	{
		ViewableTaggedSlots[SourceTaggedSlot].Quantity -= Quantity;
		if (ViewableTaggedSlots[SourceTaggedSlot].Quantity <= 0)
			ViewableTaggedSlots[SourceTaggedSlot] = FTaggedItemBundle(); // Reset if empty
	}
	else
	{
		ViewableGridSlots[SourceSlotIndex].Quantity -= Quantity;
		if (ViewableGridSlots[SourceSlotIndex].Quantity <= 0)
			ViewableGridSlots[SourceSlotIndex] = FItemBundle(); // Reset if empty
	}

	if (TargetTaggedSlot.IsValid())
	{
		if (ViewableTaggedSlots[TargetTaggedSlot].IsValid())
		{
			ViewableTaggedSlots[TargetTaggedSlot].Quantity += Quantity;
		}
		else
		{
			ViewableTaggedSlots[TargetTaggedSlot] = FTaggedItemBundle(TargetTaggedSlot, SourceItem.GetItemId(), Quantity);
		}
	}
	else
	{
		if (ViewableGridSlots[TargetSlotIndex].IsValid())
		{
			ViewableGridSlots[TargetSlotIndex].Quantity += Quantity;
		}
		else
		{
			ViewableGridSlots[TargetSlotIndex] = FItemBundle(SourceItem.GetItemId(), Quantity);
		}
	}

	const bool IsPureSplit = !SourceTaggedSlot.IsValid() && !TargetTaggedSlot.IsValid();
	
	// Broadcast updates
	if (SourceTaggedSlot.IsValid())
	{
		OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, SourceTaggedSlot, SourceItem.GetItemId(), Quantity));
		OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
	}
	else
	{
		if (!IsPureSplit) OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceItem.GetItemId(), Quantity));
		OnSlotUpdated.Broadcast(SourceSlotIndex);
	}

	if (TargetTaggedSlot.IsValid())
	{
		OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot, SourceItem.GetItemId(), Quantity));
		if (SourceTaggedSlot.IsValid())
			LinkedInventoryComponent->MoveItem(SourceItem.GetItemId(), Quantity, SourceTaggedSlot, TargetTaggedSlot);
		else
			LinkedInventoryComponent->MoveItem(SourceItem.GetItemId(), Quantity, FGameplayTag::EmptyTag, TargetTaggedSlot);
		OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
	}
	else
	{
		if (!IsPureSplit)
		{
			OperationsToConfirm.Emplace(FRISExpectedOperation(Add, SourceItem.GetItemId(), Quantity));
			LinkedInventoryComponent->MoveItem(SourceItem.GetItemId(), Quantity, SourceTaggedSlot, FGameplayTag::EmptyTag);
		}
		OnSlotUpdated.Broadcast(TargetSlotIndex);
	}

	return true;
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
		DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(TaggedSlot, Quantity);
		ViewableTaggedSlots[TaggedSlot].Quantity -= DroppedCount;
		if (ViewableTaggedSlots[TaggedSlot].Quantity <= 0)
		{
			ViewableTaggedSlots[TaggedSlot] = FTaggedItemBundle::EmptyItemInstance; // Reset the slot to empty
		}
		OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, DroppedCount));
		OnTaggedSlotUpdated.Broadcast(TaggedSlot);
	}
	else
	{
		Quantity = FMath::Min(Quantity, ViewableGridSlots[SlotIndex].Quantity);
		DroppedCount = LinkedInventoryComponent->DropItems(ViewableGridSlots[SlotIndex].ItemId, Quantity);
		OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, ViewableGridSlots[SlotIndex].ItemId, DroppedCount));
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

bool URISGridViewModel::MoveItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex,
                                                 FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex)
{
	if (!LinkedInventoryComponent || (!SourceTaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(SourceSlotIndex)) ||
		(!TargetTaggedSlot.IsValid() && !ViewableGridSlots.IsValidIndex(TargetSlotIndex)) ||
		(SourceSlotIndex != -1 && SourceSlotIndex == TargetSlotIndex) ||
		(SourceTaggedSlot == TargetTaggedSlot && SourceTaggedSlot.IsValid()))
	{
		return false;
	}

	// Prepare
	bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid();
	bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid();

	FGenericItemBundle SourceItem;
	if (SourceIsTaggedSlot)
	{
		SourceItem = ViewableTaggedSlots.Find(SourceTaggedSlot);

		if (!SourceItem.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Source tagged slot does not exist"));
			return 0;
		}
	}
	else
	{
		SourceItem =  &ViewableGridSlots[SourceSlotIndex];
	}

	FGenericItemBundle TargetItem;
	if (TargetIsTaggedSlot)
	{
		if (!LinkedInventoryComponent->IsTaggedSlotCompatible(SourceItem.GetItemId(), TargetTaggedSlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
			return false;
		}

		TargetItem = ViewableTaggedSlots.Find(TargetTaggedSlot);
		if (!TargetItem.IsValid())
		{
			if (!LinkedInventoryComponent->UniversalTaggedSlots.Contains(TargetTaggedSlot) && !LinkedInventoryComponent->SpecializedTaggedSlots.Contains(TargetTaggedSlot))
			{
				UE_LOG(LogTemp, Warning, TEXT("Target tagged slot does not exist"));
				return false;
			}

			ViewableTaggedSlots.Add(TargetTaggedSlot, FTaggedItemBundle::EmptyItemInstance);
			TargetItem = ViewableTaggedSlots.Find(TargetTaggedSlot);
		}
	}
	else
	{
		TargetItem = &ViewableGridSlots[TargetSlotIndex];
	}

	if (TargetItem.IsValid() && SourceIsTaggedSlot && URISFunctions::ShouldItemsBeSwapped(SourceItem.GetItemId(), TargetItem.GetItemId()) && !LinkedInventoryComponent->IsTaggedSlotCompatible(
		TargetItem.GetItemId(), SourceTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the source slot"));
		return 0;
	}

	FItemBundle MoveItem = FItemBundle(SourceItem.GetItemId(), SourceItem.GetQuantity());

	FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem, !TargetIsTaggedSlot,
	                                                               SourceItem.GetQuantity(), true);

	MoveItem.Quantity = MoveResult.QuantityMoved;
	
	if (MoveResult.QuantityMoved > 0)
	{
		if (SourceIsTaggedSlot)
		{
			OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, SourceTaggedSlot, MoveItem.ItemId, MoveResult.QuantityMoved));
			OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
			if (TargetIsTaggedSlot)
			{
				OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot,  MoveItem.ItemId, MoveResult.QuantityMoved));
				OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
			}
			else
			{
				OperationsToConfirm.Emplace(FRISExpectedOperation(Add,  MoveItem.ItemId, MoveResult.QuantityMoved));
				OnSlotUpdated.Broadcast(TargetSlotIndex);
			}
		}
		else
		{
			if (TargetIsTaggedSlot)
			{
				OperationsToConfirm.Emplace(FRISExpectedOperation(Remove,  MoveItem.ItemId, MoveResult.QuantityMoved));
				OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot,  MoveItem.ItemId, MoveResult.QuantityMoved));
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
		LinkedInventoryComponent->MoveItem(MoveItem.ItemId, MoveItem.Quantity, SourceTaggedSlot, TargetTaggedSlot);
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
			ForceFullUpdate();
			return;
		}

		// Update the quantity or remove the item if necessary
		ViewableTaggedSlots[SlotTag].Quantity -= Quantity;
		if (ViewableTaggedSlots[SlotTag].Quantity <= 0)
		{
			ViewableTaggedSlots[SlotTag] = FTaggedItemBundle::EmptyItemInstance; // Remove the item if quantity drops to 0 or below
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

void URISGridViewModel::PickupItem(AWorldItem* WorldItem, bool PreferTaggedSlots, bool DestroyAfterPickup)
{
	if (WorldItem == nullptr || !WorldItem->IsValidLowLevel())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WorldItem is not valid"));
		return;
	}

	LinkedInventoryComponent->PickupItem(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
}


int32 URISGridViewModel::FindSlotIndexForItem_Implementation(const FGameplayTag& ItemId, int32 Quantity)
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	for (int32 Index = 0; Index < ViewableGridSlots.Num(); ++Index)
	{
		const FItemBundle& ExistingItem = ViewableGridSlots[Index];

		if (!ExistingItem.ItemId.IsValid())
		{
			return Index;
		}
		if (ExistingItem.ItemId == ItemId)
		{
			if (ItemData->MaxStackSize > 1 && ExistingItem.Quantity < ItemData->MaxStackSize)
			{
				return Index;
			}
		}
	}

	return -1;
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
