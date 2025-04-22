// Copyright Rancorous Games, 2024
#include "ViewModels/ContainerGridViewModel.h"
#include "Components/ItemContainerComponent.h"
#include "Core/RISSubsystem.h" // For GetItemDataById
#include "Data/ItemStaticData.h"
#include "Core/RISFunctions.h" // For MoveBetweenSlots
#include "Actors/WorldItem.h" // For PickupItem
#include "LogRancInventorySystem.h" // For logging

void UContainerGridViewModel::Initialize_Implementation(UItemContainerComponent* ContainerComponent, int32 NumSlots)
{
    if (bIsInitialized || !ContainerComponent)
    {
        if (!ContainerComponent) UE_LOG(LogRISInventory, Warning, TEXT("ContainerGridViewModel::Initialize failed: ContainerComponent is null."));
        return;
    }

    LinkedContainerComponent = ContainerComponent;
    NumberOfGridSlots = NumSlots;
    ViewableGridSlots.Empty();
    OperationsToConfirm.Empty();

    // Initialize grid slots
    ViewableGridSlots.Init(FItemBundle::EmptyItemInstance, NumberOfGridSlots);

    // Subscribe to container events
    if (LinkedContainerComponent)
    {
        LinkedContainerComponent->OnItemAddedToContainer.AddDynamic(this, &UContainerGridViewModel::HandleItemAdded);
        LinkedContainerComponent->OnItemRemovedFromContainer.AddDynamic(this, &UContainerGridViewModel::HandleItemRemoved);
    }
    else
    {
        UE_LOG(LogRISInventory, Error, TEXT("ContainerGridViewModel::Initialize: LinkedContainerComponent became null unexpectedly."));
        return; // Cannot proceed without component
    }


    bIsInitialized = true;

    // Initial population of the grid from the container
    ForceFullGridUpdate();
}

void UContainerGridViewModel::BeginDestroy()
{
    // Unsubscribe from events to prevent crashes
    if (LinkedContainerComponent)
    {
        LinkedContainerComponent->OnItemAddedToContainer.RemoveDynamic(this, &UContainerGridViewModel::HandleItemAdded);
        LinkedContainerComponent->OnItemRemovedFromContainer.RemoveDynamic(this, &UContainerGridViewModel::HandleItemRemoved);
    }
    Super::BeginDestroy();
}

bool UContainerGridViewModel::IsGridSlotEmpty(int32 SlotIndex) const
{
    return !ViewableGridSlots.IsValidIndex(SlotIndex) || !ViewableGridSlots[SlotIndex].IsValid();
}

FItemBundle UContainerGridViewModel::GetGridItem(int32 SlotIndex) const
{
    if (ViewableGridSlots.IsValidIndex(SlotIndex))
    {
        return ViewableGridSlots[SlotIndex];
    }
    return FItemBundle::EmptyItemInstance;
}

int32 UContainerGridViewModel::DropItemFromGrid(int32 SlotIndex, int32 Quantity)
{
	if (!LinkedContainerComponent || !ViewableGridSlots.IsValidIndex(SlotIndex) || Quantity <= 0)
        return 0;

    const FItemBundle& SourceSlotItem = ViewableGridSlots[SlotIndex];
    if (!SourceSlotItem.IsValid())
        return 0; // Cannot drop from empty slot

    int32 QuantityToDrop = FMath::Min(Quantity, SourceSlotItem.Quantity);
    if (QuantityToDrop <= 0) return 0;

    // Predict visual change
    OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceSlotItem.ItemId, QuantityToDrop));
    int32 DroppedCount = LinkedContainerComponent->DropItems(SourceSlotItem.ItemId, QuantityToDrop); // Request server action

    // Update visual state based on prediction (server response will correct later if needed via HandleItemRemoved)
    if (DroppedCount > 0) // Assume server *might* succeed based on client-side check
    {
        ViewableGridSlots[SlotIndex].Quantity -= DroppedCount; // Use DroppedCount as it's the client's best guess
        if (ViewableGridSlots[SlotIndex].Quantity <= 0)
        {
            ViewableGridSlots[SlotIndex] = FItemBundle::EmptyItemInstance; // Reset the slot
        }
        OnGridSlotUpdated.Broadcast(SlotIndex);
    }
    else
    {
         // If DropItems immediately returns 0 locally, remove the pending operation
         for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
         {
             if (OperationsToConfirm[i].Operation == ERISSlotOperation::Remove &&
                 OperationsToConfirm[i].ItemId == SourceSlotItem.ItemId &&
                 OperationsToConfirm[i].Quantity == QuantityToDrop &&
                 !OperationsToConfirm[i].TaggedSlot.IsValid())
             {
                 OperationsToConfirm.RemoveAt(i);
                 break;
             }
         }
    }

    return DroppedCount; // Return the predicted/requested drop count
}

int32 UContainerGridViewModel::UseItemFromGrid(int32 SlotIndex)
{
	if (!LinkedContainerComponent || !ViewableGridSlots.IsValidIndex(SlotIndex))
        return 0;

    const FItemBundle& SourceSlotItem = ViewableGridSlots[SlotIndex];
    if (!SourceSlotItem.IsValid())
        return 0; // Cannot use empty slot

    // Predict visual change (if item is consumable)
    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(SourceSlotItem.ItemId);
    int32 QuantityToConsume = 0;
    if (ItemData)
    {
         // Assuming UUsableItemDefinition exists and is accessible
         // const UUsableItemDefinition* UsableDef = ItemData->GetItemDefinition<UUsableItemDefinition>();
         // if(UsableDef) QuantityToConsume = UsableDef->QuantityPerUse;
         // Simplified: Assume quantity 1 for now if stackable > 1, or the whole item if not stackable
         QuantityToConsume = (ItemData->MaxStackSize > 1) ? 1 : SourceSlotItem.Quantity;
         QuantityToConsume = FMath::Min(QuantityToConsume, SourceSlotItem.Quantity); // Can't consume more than available
    }

    if (QuantityToConsume > 0)
    {
        OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceSlotItem.ItemId, QuantityToConsume));
        // Update visual state based on prediction
        ViewableGridSlots[SlotIndex].Quantity -= QuantityToConsume;
        if (ViewableGridSlots[SlotIndex].Quantity <= 0)
        {
             ViewableGridSlots[SlotIndex] = FItemBundle::EmptyItemInstance;
        }
         OnGridSlotUpdated.Broadcast(SlotIndex);
    }

    // Request server action
    LinkedContainerComponent->UseItem(SourceSlotItem.ItemId);

    return 0; // Base implementation doesn't know the exact result
}

bool UContainerGridViewModel::SplitItemInGrid_Implementation(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity)
{
     return MoveItemInGrid_Internal(SourceSlotIndex, TargetSlotIndex, Quantity, true);
}

bool UContainerGridViewModel::MoveItemInGrid_Implementation(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
     // For a full move, quantity is ignored (pass 0), IsSplit is false
     return MoveItemInGrid_Internal(SourceSlotIndex, TargetSlotIndex, 0, false);
}

// Internal implementation for both move and split within the grid
bool UContainerGridViewModel::MoveItemInGrid_Internal(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit)
{
    if (!LinkedContainerComponent ||
        !ViewableGridSlots.IsValidIndex(SourceSlotIndex) ||
        !ViewableGridSlots.IsValidIndex(TargetSlotIndex) ||
        SourceSlotIndex == TargetSlotIndex)
    {
        return false;
    }

    FItemBundle& SourceItemBundle = ViewableGridSlots[SourceSlotIndex];
    FItemBundle& TargetItemBundle = ViewableGridSlots[TargetSlotIndex];

    // Convert to generic bundles for MoveBetweenSlots function
    FGenericItemBundle SourceItem(&SourceItemBundle);
    FGenericItemBundle TargetItem(&TargetItemBundle);

    if (!SourceItem.IsValid()) return false; // Cannot move/split from empty

    int32 QuantityToMove = IsSplit ? InQuantity : SourceItem.GetQuantity();
    if (QuantityToMove <= 0) return false;

    if (IsSplit && QuantityToMove > SourceItem.GetQuantity())
    {
        UE_LOG(LogRISInventory, Warning, TEXT("Cannot split %d items, only %d available in source slot %d."), QuantityToMove, SourceItem.GetQuantity(), SourceSlotIndex);
        return false; // Cannot split more than available
    }

    // Perform the visual move/split using the helper function
    // For grid-to-grid, no server validation needed, just visual changes.
    FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem, false, QuantityToMove, !IsSplit); // Allow swap only if not splitting

    // If the move was visually successful, broadcast updates
    if (MoveResult.QuantityMoved > 0 || MoveResult.WereItemsSwapped)
    {
        OnGridSlotUpdated.Broadcast(SourceSlotIndex);
        OnGridSlotUpdated.Broadcast(TargetSlotIndex);
        return true;
    }

    return false;
}

bool UContainerGridViewModel::CanGridSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const
{
    if (!ViewableGridSlots.IsValidIndex(SlotIndex) || Quantity <= 0 || !ItemId.IsValid())
    {
        return false; // Invalid parameters
    }

    if (!LinkedContainerComponent->CanContainerReceiveItems(ItemId, Quantity)) return false;
    
    const FItemBundle& TargetSlotItem = ViewableGridSlots[SlotIndex];
    const bool TargetSlotEmpty = !TargetSlotItem.IsValid();

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

void UContainerGridViewModel::PickupItemToContainer(AWorldItem* WorldItem, bool DestroyAfterPickup)
{
	if (!WorldItem || !LinkedContainerComponent) return;

    const FItemBundleWithInstanceData& ItemToPickup = WorldItem->RepresentedItem;
    if (!ItemToPickup.IsValid()) return;

    // Check if the container *can* fundamentally receive the item (ignoring visual slots for now)
    // This check is important before requesting server action.
    int32 ReceivableQty = LinkedContainerComponent->GetReceivableQuantity(ItemToPickup.ItemId);
    int32 QuantityToPickup = FMath::Min(ItemToPickup.Quantity, ReceivableQty);

    if (QuantityToPickup <= 0)
    {
        UE_LOG(LogRISInventory, Log, TEXT("Container cannot receive item %s from WorldItem."), *ItemToPickup.ItemId.ToString());
        return;
    }

    // Predict Add Operation
    OperationsToConfirm.Emplace(FRISExpectedOperation(Add, ItemToPickup.ItemId, QuantityToPickup));

    // Request server pickup (AddItem_IfServer takes from the source)
    int32 AddedQty = LinkedContainerComponent->AddItem_IfServer(WorldItem, ItemToPickup.ItemId, QuantityToPickup, true); // Allow partial

    // --- Client-Side Visual Update (Prediction) ---
    // Note: This duplicates logic from HandleItemAdded. Refactor potential?
    if (AddedQty > 0)
    {
        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemToPickup.ItemId);
        if (ItemData)
        {
            int32 RemainingItems = AddedQty;
            while (RemainingItems > 0)
            {
                int32 SlotIndex = FindGridSlotIndexForItem(ItemData->ItemId, RemainingItems); // Find best slot
                if (SlotIndex == -1)
                {
                    UE_LOG(LogRISInventory, Warning, TEXT("PickupItemToContainer: No visual slot found for item %s after adding %d."), *ItemData->ItemId.ToString(), AddedQty);
                    // Potentially ForceFullGridUpdate() if prediction fails badly?
                    break;
                }

                FItemBundle& TargetSlot = ViewableGridSlots[SlotIndex];
                int32 CanAddToSlot = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;
                if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId)
                {
                    CanAddToSlot -= TargetSlot.Quantity;
                }
                else if (!TargetSlot.IsValid())
                {
                     TargetSlot.ItemId = ItemData->ItemId;
                     TargetSlot.Quantity = 0;
                }
                else {
                     // This case shouldn't happen if FindGridSlotIndexForItem is correct
                     UE_LOG(LogRISInventory, Error, TEXT("PickupItemToContainer: FindGridSlotIndexForItem returned incompatible slot %d."), SlotIndex);
                     break;
                }


                int32 ActuallyAddedToSlot = FMath::Min(RemainingItems, CanAddToSlot);
                 if (ActuallyAddedToSlot <= 0) {
                      // This could happen if FindGridSlotIndexForItem finds a full stack - indicates FindGridSlotIndexForItem needs refinement or container is visually full.
                      UE_LOG(LogRISInventory, Warning, TEXT("PickupItemToContainer: Could not add to found slot %d (already full?)."), SlotIndex);
                      // Attempt to find another slot (might lead to infinite loop if FindGridSlotIndexForItem isn't robust)
                      // For safety, we break here. A better FindGridSlotIndexForItem is needed.
                      break;
                 }

                TargetSlot.Quantity += ActuallyAddedToSlot;
                RemainingItems -= ActuallyAddedToSlot;
                OnGridSlotUpdated.Broadcast(SlotIndex);
            }
        }
         // If server interaction succeeded, and DestroyAfterPickup is true, destroy the world item.
         // Note: Server might destroy it anyway. Client destroy is mostly visual cleanup.
         if (DestroyAfterPickup && WorldItem && WorldItem->GetContainedQuantity(ItemToPickup.ItemId) <= 0) // Check if source is empty
         {
             WorldItem->Destroy();
         }
    }
     else // Pickup request failed immediately (e.g., server-side CanReceive check failed if called synchronously)
     {
         // Remove the pending operation if the AddItem call returned 0
         for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
         {
             if (OperationsToConfirm[i].Operation == ERISSlotOperation::Add &&
                 OperationsToConfirm[i].ItemId == ItemToPickup.ItemId &&
                 OperationsToConfirm[i].Quantity == QuantityToPickup &&
                 !OperationsToConfirm[i].TaggedSlot.IsValid())
             {
                 OperationsToConfirm.RemoveAt(i);
                 break;
             }
         }
     }


}

bool UContainerGridViewModel::AssertViewModelSettled() const
{
    bool bOpsSettled = OperationsToConfirm.IsEmpty();
    ensureMsgf(bOpsSettled, TEXT("ContainerViewModel is not settled. %d operations pending."), OperationsToConfirm.Num());
    if (!bOpsSettled) UE_LOG(LogRISInventory, Warning, TEXT("ContainerViewModel pending ops: %d"), OperationsToConfirm.Num());

    // Check grid slot quantities against the container component
    bool bQuantitiesMatch = true;
    if (LinkedContainerComponent)
    {
        TMap<FGameplayTag, int32> ContainerQuantities;
        for (const auto& Item : LinkedContainerComponent->GetAllContainerItems())
        {
            ContainerQuantities.Add(Item.ItemId, Item.Quantity);
        }

        TMap<FGameplayTag, int32> ViewModelQuantities;
        for (const FItemBundle& Slot : ViewableGridSlots)
        {
            if (Slot.IsValid())
            {
                ViewModelQuantities.FindOrAdd(Slot.ItemId) += Slot.Quantity;
            }
        }

        // Compare maps
        if (ContainerQuantities.Num() != ViewModelQuantities.Num())
        {
            bQuantitiesMatch = false;
        }
        else
        {
            for (const auto& Pair : ContainerQuantities)
            {
                const int32* VmQty = ViewModelQuantities.Find(Pair.Key);
                if (!VmQty || *VmQty != Pair.Value)
                {
                    bQuantitiesMatch = false;
                    ensureMsgf(false, TEXT("Quantity mismatch for %s. Container: %d, ViewModel: %d"), *Pair.Key.ToString(), Pair.Value, VmQty ? *VmQty : -1);
                    UE_LOG(LogRISInventory, Warning, TEXT("Quantity mismatch for %s. Container: %d, ViewModel: %d"), *Pair.Key.ToString(), Pair.Value, VmQty ? *VmQty : -1);
                    break;
                }
            }
        }
         ensureMsgf(bQuantitiesMatch, TEXT("ContainerViewModel grid quantities do not match LinkedContainerComponent."));
          if (!bQuantitiesMatch) UE_LOG(LogRISInventory, Warning, TEXT("ContainerViewModel grid quantities mismatch."));
    }
    else
    {
        UE_LOG(LogRISInventory, Warning, TEXT("AssertViewModelSettled: LinkedContainerComponent is null."));
        return false; // Cannot verify quantities
    }


    return bOpsSettled && bQuantitiesMatch;
}

// --- Event Handlers ---

void UContainerGridViewModel::HandleItemAdded_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
    if (!ItemData || Quantity <= 0) return;

    // Try to confirm a pending operation first
    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        // Only confirm Add operations for the grid (no TaggedSlot)
        if (OperationsToConfirm[i].Operation == ERISSlotOperation::Add &&
            !OperationsToConfirm[i].TaggedSlot.IsValid() &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
            // Assuming the predictive visual update was correct, we don't need to do anything else.
            // If prediction is complex, this is where you'd reconcile.
            return;
        }
    }

    // If no operation was confirmed, this is an unexpected add from the server.
    // Apply the change visually. This logic is similar to the predictive part of PickupItem.
    UE_LOG(LogRISInventory, Log, TEXT("HandleItemAdded: Received unpredicted add for %s x%d. Updating visuals."), *ItemData->ItemId.ToString(), Quantity);
    int32 RemainingItems = Quantity;
    while (RemainingItems > 0)
    {
        int32 SlotIndex = FindGridSlotIndexForItem(ItemData->ItemId, RemainingItems);
        if (SlotIndex == -1)
        {
            UE_LOG(LogRISInventory, Error, TEXT("HandleItemAdded: No available visual slot found for server-added item %s."), *ItemData->ItemId.ToString());
            // Consider ForceFullGridUpdate() as a fallback?
            break;
        }

        FItemBundle& TargetSlot = ViewableGridSlots[SlotIndex];
        int32 CanAddToSlot = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;
         if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId) {
             CanAddToSlot -= TargetSlot.Quantity;
         } else if (!TargetSlot.IsValid()) {
             TargetSlot.ItemId = ItemData->ItemId;
             TargetSlot.Quantity = 0;
         } else {
             UE_LOG(LogRISInventory, Error, TEXT("HandleItemAdded: FindGridSlotIndexForItem returned incompatible slot %d."), SlotIndex);
             break; // Should not happen
         }

        int32 ActuallyAddedToSlot = FMath::Min(RemainingItems, CanAddToSlot);
        if (ActuallyAddedToSlot <= 0) {
             // Indicates logic error or full visual grid
              UE_LOG(LogRISInventory, Warning, TEXT("HandleItemAdded: Could not add to found slot %d (already full?). Forcing full update."), SlotIndex);
              ForceFullGridUpdate(); // Force resync if we get stuck
              break;
        }

        TargetSlot.Quantity += ActuallyAddedToSlot;
        RemainingItems -= ActuallyAddedToSlot;
        OnGridSlotUpdated.Broadcast(SlotIndex);
    }
}

void UContainerGridViewModel::HandleItemRemoved_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
    if (!ItemData || Quantity <= 0) return;

    // Try to confirm a pending operation first
    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        // Only confirm Remove operations for the grid (no TaggedSlot)
        if (OperationsToConfirm[i].Operation == ERISSlotOperation::Remove &&
            !OperationsToConfirm[i].TaggedSlot.IsValid() &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
            // Assuming prediction was correct.
            return;
        }
    }

    // If no operation confirmed, handle unexpected server removal.
    UE_LOG(LogRISInventory, Log, TEXT("HandleItemRemoved: Received unpredicted remove for %s x%d. Updating visuals."), *ItemData->ItemId.ToString(), Quantity);
    int32 RemainingToRemove = Quantity;
    for (int32 SlotIndex = 0; SlotIndex < ViewableGridSlots.Num() && RemainingToRemove > 0; ++SlotIndex)
    {
        FItemBundle& CurrentSlot = ViewableGridSlots[SlotIndex];
        if (CurrentSlot.IsValid() && CurrentSlot.ItemId == ItemData->ItemId)
        {
            int32 CanRemoveFromSlot = FMath::Min(RemainingToRemove, CurrentSlot.Quantity);
            if (CanRemoveFromSlot > 0)
            {
                CurrentSlot.Quantity -= CanRemoveFromSlot;
                RemainingToRemove -= CanRemoveFromSlot;

                if (CurrentSlot.Quantity <= 0)
                {
                    CurrentSlot = FItemBundle::EmptyItemInstance; // Reset slot
                }
                OnGridSlotUpdated.Broadcast(SlotIndex);
            }
        }
    }

    if (RemainingToRemove > 0)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("HandleItemRemoved: Could not remove %d items of type %s visually. Forcing full update."), RemainingToRemove, *ItemData->ItemId.ToString());
        ForceFullGridUpdate(); // Resync if visual state is inconsistent
    }
}

int32 UContainerGridViewModel::FindGridSlotIndexForItem_Implementation(const FGameplayTag& ItemId, int32 Quantity)
{
    if (!ItemId.IsValid()) return -1;

    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
    if (!ItemData) return -1;

    int32 FirstEmptySlot = -1;
    int32 BestPartialSlot = -1;
    int32 SmallestPartialStack = ItemData->MaxStackSize + 1; // Sentinel value

    for (int32 Index = 0; Index < ViewableGridSlots.Num(); ++Index)
    {
        const FItemBundle& ExistingItem = ViewableGridSlots[Index];

        if (!ExistingItem.IsValid())
        {
            if (FirstEmptySlot == -1)
            {
                FirstEmptySlot = Index; // Record the first empty slot found
            }
        }
        else if (ExistingItem.ItemId == ItemId)
        {
            // Found a slot with the same item
            if (ItemData->MaxStackSize > 1 && ExistingItem.Quantity < ItemData->MaxStackSize)
            {
                 // This is a potential partial stack
                 // Prefer the stack with the *least* items already in it to fill up smaller stacks first? Or largest? Let's go with largest stack first.
                 // Update: Let's just return the first partial stack we find for simplicity.
                 return Index; // Found a suitable partial stack

                 /* Example: Find smallest partial stack
                 if (ExistingItem.Quantity < SmallestPartialStack) {
                     SmallestPartialStack = ExistingItem.Quantity;
                     BestPartialSlot = Index;
                 }
                 */
            }
             // If MaxStackSize is 1, and we found the item, the slot is full. Continue searching.
        }
    }

    // If we found a partial stack, return it (This part is currently redundant due to early return above, keep for alternative logic)
    // if (BestPartialSlot != -1)
    // {
    //     return BestPartialSlot;
    // }

    // If no partial stack was found, return the first empty slot
    return FirstEmptySlot;
}

void UContainerGridViewModel::ForceFullGridUpdate_Implementation()
{
    if (!LinkedContainerComponent)
    {
        UE_LOG(LogRISInventory, Error, TEXT("ForceFullGridUpdate: Cannot update, LinkedContainerComponent is null."));
        return;
    }

    UE_LOG(LogRISInventory, Log, TEXT("ForceFullGridUpdate: Resynchronizing visual grid slots."));

    // Clear current visual state
    ViewableGridSlots.Init(FItemBundle::EmptyItemInstance, NumberOfGridSlots);
    OperationsToConfirm.Empty(); // Clear pending operations as we are forcing state

    // Re-populate based on the actual container state
    TArray<FItemBundleWithInstanceData> ActualItems = LinkedContainerComponent->GetAllContainerItems();
    for (const FItemBundleWithInstanceData& BackingItem : ActualItems)
    {
        if (BackingItem.Quantity <= 0) continue; // Skip empty/invalid items

        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(BackingItem.ItemId);
        if (!ItemData)
        {
            UE_LOG(LogRISInventory, Warning, TEXT("ForceFullGridUpdate: Skipping item %s, ItemStaticData not found."), *BackingItem.ItemId.ToString());
            continue;
        }

        int32 RemainingQuantity = BackingItem.Quantity;
        while (RemainingQuantity > 0)
        {
            int32 SlotToAddTo = FindGridSlotIndexForItem(BackingItem.ItemId, RemainingQuantity);
            if (SlotToAddTo == -1)
            {
                UE_LOG(LogRISInventory, Error, TEXT("ForceFullGridUpdate: Failed to find visual slot for item %s during resync. Visual grid might be smaller than container capacity or FindGridSlotIndexForItem needs fixing."), *BackingItem.ItemId.ToString());
                // This suggests a potential issue: either the visual grid is full but the container isn't,
                // or FindGridSlotIndexForItem failed unexpectedly.
                break; // Avoid infinite loop
            }

            FItemBundle& TargetSlot = ViewableGridSlots[SlotToAddTo];
            int32 AddLimit = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;

            if (TargetSlot.IsValid() && TargetSlot.ItemId == BackingItem.ItemId) {
                 AddLimit -= TargetSlot.Quantity;
            } else if (!TargetSlot.IsValid()) {
                 TargetSlot.ItemId = BackingItem.ItemId;
                 TargetSlot.Quantity = 0;
            } else {
                 UE_LOG(LogRISInventory, Error, TEXT("ForceFullGridUpdate: FindGridSlotIndexForItem returned incompatible slot %d."), SlotToAddTo);
                 break; // Avoid overwriting different item
            }


            int32 AddedAmount = FMath::Min(RemainingQuantity, AddLimit);
            if(AddedAmount <= 0) {
                 // Should not happen if FindGridSlotIndexForItem is correct and returned a valid slot
                 UE_LOG(LogRISInventory, Error, TEXT("ForceFullGridUpdate: Calculated AddedAmount is zero for slot %d. FindGridSlotIndexForItem logic error?"), SlotToAddTo);
                 break;
            }

            TargetSlot.Quantity += AddedAmount;
            RemainingQuantity -= AddedAmount;
        }
    }

    // Broadcast update for all potentially changed slots
    for (int32 i = 0; i < NumberOfGridSlots; ++i)
    {
        OnGridSlotUpdated.Broadcast(i);
    }
}