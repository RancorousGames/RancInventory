// Copyright Rancorous Games, 2024
#include "ViewModels/InventoryGridViewModel.h"
#include "Components/InventoryComponent.h" // Include specific type
#include "Core/RISSubsystem.h"
#include "Data/ItemStaticData.h"
#include "Core/RISFunctions.h"
#include "Actors/WorldItem.h"
#include "LogRancInventorySystem.h"


// Override Initialize_Implementation from base UContainerGridViewModel
void UInventoryGridViewModel::Initialize_Implementation(UItemContainerComponent* ContainerComponent, int32 NumSlots)
{
    // We expect an InventoryComponent for this derived class.
    UInventoryComponent* InvComp = Cast<UInventoryComponent>(ContainerComponent);
    if (InvComp)
    {
        InitializeInventory(InvComp, NumSlots, bPreferEmptyUniversalSlots); // Call our specific init
    }
    else
    {
         UE_LOG(LogRISInventory, Error, TEXT("InventoryGridViewModel requires an InventoryComponent, but received a different UItemContainerComponent type. Initialization failed."));
         // Optionally call Super::Initialize with the base ContainerComponent? Or just fail?
         // Super::Initialize_Implementation(ContainerComponent, NumSlots); // If you want basic grid init
    }
}

// Specific Initializer for InventoryViewModel
void UInventoryGridViewModel::InitializeInventory_Implementation(UInventoryComponent* InventoryComponent, int32 NumGridSlots, bool InPreferEmptyUniversalSlots)
{
    if (bIsInitialized) return; // Prevent re-initialization check

    // Call base class initialization FIRST for grid setup
    Super::Initialize_Implementation(InventoryComponent, NumGridSlots);

    // Now, bIsInitialized is true from the base call, but we continue *our* init.
    // If base init failed (e.g., null component), it should have returned early.
    if (!bIsInitialized || !InventoryComponent) {
         UE_LOG(LogRISInventory, Error, TEXT("InventoryGridViewModel::InitializeInventory failed after base initialization or with null InventoryComponent."));
         bIsInitialized = false; // Reset flag if something went wrong
         return;
    }


    LinkedInventoryComponent = InventoryComponent; // Store specific type
    bPreferEmptyUniversalSlots = InPreferEmptyUniversalSlots;
    ViewableTaggedSlots.Empty();

    // Subscribe to Inventory-specific events
    LinkedInventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemAdded);
    LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemRemoved);

    // Initialize Tagged Slots map structure
    for (const FUniversalTaggedSlot& UniTag : LinkedInventoryComponent->UniversalTaggedSlots)
    {
        // Ensure tag is valid before adding
        if(UniTag.Slot.IsValid()) {
            ViewableTaggedSlots.Add(UniTag.Slot, FTaggedItemBundle(UniTag.Slot, FGameplayTag(), 0));
        } else {
             UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Found invalid tag in UniversalTaggedSlots."));
        }
    }
    for (const FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots)
    {
         if(Tag.IsValid()) {
            ViewableTaggedSlots.Add(Tag, FTaggedItemBundle(Tag, FGameplayTag(), 0));
         } else {
             UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Found invalid tag in SpecializedTaggedSlots."));
         }
    }

    // Populate tagged slots AFTER the map structure is built
    const TArray<FTaggedItemBundle>& ActualTaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
    for (const FTaggedItemBundle& TaggedItem : ActualTaggedItems)
    {
        if (ViewableTaggedSlots.Contains(TaggedItem.Tag))
        {
            ViewableTaggedSlots[TaggedItem.Tag] = TaggedItem; // Overwrite initial empty entry
        }
        else
        {
            UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Tagged item %s found in component but tag %s is not registered in ViewableTaggedSlots."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
            // Optionally add it anyway? Depends on desired strictness.
             if(TaggedItem.Tag.IsValid()) {
                 ViewableTaggedSlots.Add(TaggedItem.Tag, TaggedItem);
             }
        }
    }

    // Note: Base Initialize already called ForceFullGridUpdate.
    // We might need a combined ForceFullInventoryUpdate later.
}

void UInventoryGridViewModel::BeginDestroy()
{
     // Unsubscribe inventory-specific events
     if (LinkedInventoryComponent)
     {
         LinkedInventoryComponent->OnItemAddedToTaggedSlot.RemoveDynamic(this, &UInventoryGridViewModel::HandleTaggedItemAdded);
         LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.RemoveDynamic(this, &UInventoryGridViewModel::HandleTaggedItemRemoved);
     }
     Super::BeginDestroy(); // Call base to unsubscribe container events
}


// --- Tagged Slot Specific Functions ---

bool UInventoryGridViewModel::IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const
{
    const FTaggedItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    return !Found || !Found->IsValid();
}

const FTaggedItemBundle& UInventoryGridViewModel::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
    const FTaggedItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    if (Found)
    {
        return *Found;
    }
    // Return a static empty instance to avoid crashes on lookup failure
    static const FTaggedItemBundle EmptyTaggedBundle = FTaggedItemBundle::EmptyItemInstance;
    UE_LOG(LogRISInventory, Warning, TEXT("GetItemForTaggedSlot: SlotTag %s not found."), *SlotTag.ToString());
    return EmptyTaggedBundle;
}

FTaggedItemBundle& UInventoryGridViewModel::GetMutableItemForTaggedSlot(const FGameplayTag& SlotTag)
{
     FTaggedItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    if (Found)
    {
        return *Found;
    }
     // This is dangerous, but required by original code. Consider alternatives.
     UE_LOG(LogRISInventory, Error, TEXT("GetMutableItemForTaggedSlot: SlotTag %s not found. Returning potentially unsafe reference."), *SlotTag.ToString());
     // Add default if not found to allow modification? Risky.
     return ViewableTaggedSlots.Add(SlotTag, FTaggedItemBundle(SlotTag, FGameplayTag(), 0));
}

int32 UInventoryGridViewModel::DropItemFromTaggedSlot(FGameplayTag TaggedSlot, int32 Quantity)
{
	 if (!LinkedInventoryComponent || !TaggedSlot.IsValid() || Quantity <= 0)
        return 0;

     const FTaggedItemBundle* SourceSlotItemPtr = ViewableTaggedSlots.Find(TaggedSlot);
    if (!SourceSlotItemPtr || !SourceSlotItemPtr->IsValid())
        return 0; // Slot is empty or doesn't exist

    const FTaggedItemBundle& SourceSlotItem = *SourceSlotItemPtr;
    int32 QuantityToDrop = FMath::Min(Quantity, SourceSlotItem.Quantity);
     if (QuantityToDrop <= 0) return 0;

    // Predict visual change
    OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, SourceSlotItem.ItemId, QuantityToDrop));
    int32 DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(TaggedSlot, QuantityToDrop); // Request server action

    // Update visual state based on prediction
    if (DroppedCount > 0) // Assume server might succeed
    {
        FTaggedItemBundle& MutableSlot = ViewableTaggedSlots[TaggedSlot]; // Get mutable reference
        MutableSlot.Quantity -= DroppedCount;
        if (MutableSlot.Quantity <= 0)
        {
             MutableSlot.ItemId = FGameplayTag(); // Keep Tag, clear ItemId and Quantity
             MutableSlot.Quantity = 0;
        }
        OnTaggedSlotUpdated.Broadcast(TaggedSlot);
    }
    else
    {
         // If DropFromTaggedSlot returns 0 immediately, remove pending op
         for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
         {
             if (OperationsToConfirm[i].Operation == ERISSlotOperation::RemoveTagged &&
                 OperationsToConfirm[i].TaggedSlot == TaggedSlot &&
                 OperationsToConfirm[i].ItemId == SourceSlotItem.ItemId &&
                 OperationsToConfirm[i].Quantity == QuantityToDrop)
             {
                 OperationsToConfirm.RemoveAt(i);
                 break;
             }
         }
    }

    return DroppedCount;
}

int32 UInventoryGridViewModel::UseItemFromTaggedSlot(FGameplayTag TaggedSlot)
{
	if (!LinkedInventoryComponent || !TaggedSlot.IsValid())
        return 0;

     const FTaggedItemBundle* SourceSlotItemPtr = ViewableTaggedSlots.Find(TaggedSlot);
    if (!SourceSlotItemPtr || !SourceSlotItemPtr->IsValid())
        return 0; // Slot is empty or doesn't exist

    const FTaggedItemBundle& SourceSlotItem = *SourceSlotItemPtr;

    // Predict visual change (if consumable) - similar logic to UseItemFromGrid
    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(SourceSlotItem.ItemId);
     int32 QuantityToConsume = 0;
     if(ItemData) {
         // Simplified consumption logic
          QuantityToConsume = (ItemData->MaxStackSize > 1) ? 1 : SourceSlotItem.Quantity;
          QuantityToConsume = FMath::Min(QuantityToConsume, SourceSlotItem.Quantity);
     }


     if (QuantityToConsume > 0) {
         OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, SourceSlotItem.ItemId, QuantityToConsume));
         // Update visual state based on prediction
         FTaggedItemBundle& MutableSlot = ViewableTaggedSlots[TaggedSlot];
         MutableSlot.Quantity -= QuantityToConsume;
         if (MutableSlot.Quantity <= 0) {
             MutableSlot.ItemId = FGameplayTag();
             MutableSlot.Quantity = 0;
         }
         OnTaggedSlotUpdated.Broadcast(TaggedSlot);
     }


    // Request server action
    LinkedInventoryComponent->UseItemFromTaggedSlot(TaggedSlot);

    return 0; // Base implementation doesn't know the exact result
}

// --- Move/Split Implementation (Handles all slot types) ---

bool UInventoryGridViewModel::SplitItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity)
{
    return MoveItem_Internal(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, Quantity, true);
}

bool UInventoryGridViewModel::MoveItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex)
{
    return MoveItem_Internal(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, 0, false);
}

bool UInventoryGridViewModel::MoveItem_Internal(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit)
{
    if (!LinkedInventoryComponent) return false;

    // Basic Validation: Ensure at least one source/target index/tag is valid
    // and prevent moving to the exact same slot.
    const bool bSourceIsGrid = !SourceTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(SourceSlotIndex);
    const bool bSourceIsTag = SourceTaggedSlot.IsValid() && ViewableTaggedSlots.Contains(SourceTaggedSlot);
    const bool bTargetIsGrid = !TargetTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(TargetSlotIndex);
    const bool bTargetIsTag = TargetTaggedSlot.IsValid() && ViewableTaggedSlots.Contains(TargetTaggedSlot);

    if ((!bSourceIsGrid && !bSourceIsTag) || (!bTargetIsGrid && !bTargetIsTag)) return false; // Invalid source or target
    if (bSourceIsGrid && bTargetIsGrid && SourceSlotIndex == TargetSlotIndex) return false; // Grid -> Same Grid
    if (bSourceIsTag && bTargetIsTag && SourceTaggedSlot == TargetTaggedSlot) return false; // Tag -> Same Tag

    // --- Get Source Item ---
    FGenericItemBundle SourceItem;
    if (bSourceIsTag) {
        SourceItem = &ViewableTaggedSlots[SourceTaggedSlot]; // Use mutable map directly
    } else { // Source is Grid
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }
    if (!SourceItem.IsValid()) return false; // Cannot move/split empty source

    const FGameplayTag ItemIdToMove = SourceItem.GetItemId();
    int32 MaxSourceQty = SourceItem.GetQuantity();
    int32 RequestedQuantity = IsSplit ? InQuantity : MaxSourceQty;

    if (RequestedQuantity <= 0) return false;
    if (IsSplit && RequestedQuantity > MaxSourceQty) {
        UE_LOG(LogRISInventory, Warning, TEXT("Cannot split more items than available."));
        return false;
    }

    // --- Get Target Item ---
    FGenericItemBundle TargetItem;
     if (bTargetIsTag) {
         TargetItem = &ViewableTaggedSlots[TargetTaggedSlot];
     } else { // Target is Grid
         TargetItem = &ViewableGridSlots[TargetSlotIndex];
     }

    // --- Check Compatibility (especially for tagged slots) ---
    if (bTargetIsTag) {
        if (!LinkedInventoryComponent->IsTaggedSlotCompatible(ItemIdToMove, TargetTaggedSlot)) {
            UE_LOG(LogRISInventory, Warning, TEXT("Item %s not compatible with tagged slot %s."), *ItemIdToMove.ToString(), *TargetTaggedSlot.ToString());
            return false;
        }
    }
     // CanGridSlotReceiveItem handles grid compatibility implicitly via MoveBetweenSlots logic

    // --- Prepare for Potential Swap ---
    FGameplayTag SwapItemId = FGameplayTag();
    int32 SwapQuantity = -1;
    if (!IsSplit && TargetItem.IsValid() && TargetItem.GetItemId() != ItemIdToMove) {
        SwapItemId = TargetItem.GetItemId();
        SwapQuantity = TargetItem.GetQuantity();
    }

    // --- Validate Move with Inventory Component (Handles Blocking, etc.) ---
    int32 QuantityValidatedByServer = RequestedQuantity; // Assume client is correct for grid->grid
    if (bSourceIsTag || bTargetIsTag) // If involves tagged slots, ask server component
    {
        QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);

        // If initial validation failed due to blocking, try unblocking
        if (QuantityValidatedByServer <= 0 && bTargetIsTag && !IsSplit && TryUnblockingMove(TargetTaggedSlot, ItemIdToMove))
        {
             // Try validation again after unblocking attempt
             QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
        }

        if (QuantityValidatedByServer <= 0) {
             UE_LOG(LogRISInventory, Log, TEXT("Move/Split failed server-side validation."));
             return false; // Server component prevents this move
        }
    }

    // Adjust quantity if server validation reduced it (e.g., partial stack in target tagged slot)
    int32 QuantityToActuallyMove = QuantityValidatedByServer;


    // --- Perform Visual Move/Split ---
    FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItem, TargetItem, false, QuantityToActuallyMove, !IsSplit, !IsSplit); // Allow swaps only if not splitting


    // --- Handle Results and Pending Operations ---
    if (MoveResult.QuantityMoved > 0 || MoveResult.WereItemsSwapped)
    {
        // --- Update Pending Operations ---
        
        if (MoveResult.QuantityMoved > 0)
        {
            if (bSourceIsTag)
            {
                OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, SourceTaggedSlot, ItemIdToMove, MoveResult.QuantityMoved));
                OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
                if (bTargetIsTag)
                {
                    OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot, ItemIdToMove, MoveResult.QuantityMoved));
                    OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
                }
                else
                {
                    OperationsToConfirm.Emplace(FRISExpectedOperation(Add, ItemIdToMove, MoveResult.QuantityMoved));
                    OnGridSlotUpdated.Broadcast(TargetSlotIndex);
                }

                if (MoveResult.WereItemsSwapped)
                {
                    if (bTargetIsTag)
                        OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
                    else
                        OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceItem.GetItemId(), SourceItem.GetQuantity()));
                    OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, SourceTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
                }
            }
            else
            {
                if (bTargetIsTag)
                {
                    OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, ItemIdToMove, MoveResult.QuantityMoved));
                    OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, TargetTaggedSlot, ItemIdToMove, MoveResult.QuantityMoved));
                    OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);

            	    if (MoveResult.WereItemsSwapped)
            	    {
            		    OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem.GetItemId(), SourceItem.GetQuantity()));
            		    OperationsToConfirm.Emplace(FRISExpectedOperation(Add, SourceItem.GetItemId(), SourceItem.GetQuantity()));
            	    }
                }
                else // purely visual move
                {
                    OnGridSlotUpdated.Broadcast(TargetSlotIndex);
                }
                OnGridSlotUpdated.Broadcast(SourceSlotIndex);
            }
        }
        
        // Request Server Action ONLY if tagged slots involved OR if the move requires actual item transfer (not just visual)
        // For Grid->Grid moves, the server doesn't care about visual slot positions.
        if (bSourceIsTag || bTargetIsTag) {
            LinkedInventoryComponent->MoveItem(ItemIdToMove, QuantityToActuallyMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
        }

        return true; // Visual move succeeded
    }

    return false; // Visual move failed
}

bool UInventoryGridViewModel::TryUnblockingMove(FGameplayTag TargetTaggedSlot, FGameplayTag ItemId)
{
    if (!LinkedInventoryComponent) return false;

     bool bUnblocked = false;
     // Check if the target slot itself *would* be blocked by equipping the item
     if (auto* BlockingInfo = LinkedInventoryComponent->WouldItemMoveIndirectlyViolateBlocking(TargetTaggedSlot, URISSubsystem::GetItemDataById(ItemId)))
     {
          // Find the item currently IN the slot that needs to be cleared
          FGameplayTag SlotToClear = BlockingInfo->UniversalSlotToBlock; // The slot that's causing the block
          FTaggedItemBundle BlockingItem = GetItemForTaggedSlot(SlotToClear);

          if (BlockingItem.IsValid())
          {
               // Find an empty grid slot for the blocking item
               int32 TargetGridIndex = FindGridSlotIndexForItem(BlockingItem.ItemId, BlockingItem.Quantity);
               if (TargetGridIndex != -1 && IsGridSlotEmpty(TargetGridIndex)) // Ensure target grid slot is actually empty
               {
                   UE_LOG(LogRISInventory, Log, TEXT("TryUnblockingMove: Attempting to move blocking item %s from slot %s to grid slot %d."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString(), TargetGridIndex);
                   // Recursively call MoveItem_Internal to move blocking item out
                   // Move the ENTIRE stack of the blocking item.
                   bUnblocked = MoveItem_Internal(SlotToClear, -1, FGameplayTag(), TargetGridIndex, BlockingItem.Quantity, false);
                   if (!bUnblocked) {
                        UE_LOG(LogRISInventory, Warning, TEXT("TryUnblockingMove: Failed to move blocking item %s from %s."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString());
                   }
               } else {
                    UE_LOG(LogRISInventory, Warning, TEXT("TryUnblockingMove: No empty grid slot found for blocking item %s from slot %s."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString());
               }
          }
     }
     return bUnblocked; // Return true if we successfully moved the blocking item
}


bool UInventoryGridViewModel::MoveItemToAnyTaggedSlot_Implementation(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex)
{
    if (!LinkedInventoryComponent) return false;

    const bool bSourceIsTag = SourceTaggedSlot.IsValid();
    const bool bSourceIsGrid = !bSourceIsTag && ViewableGridSlots.IsValidIndex(SourceSlotIndex);

    if (!bSourceIsTag && !bSourceIsGrid) return false; // Invalid source

    // Get source item
    FGenericItemBundle SourceItem;
    if (bSourceIsTag) {
        SourceItem = &ViewableTaggedSlots[SourceTaggedSlot];
    } else {
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }

    if (!SourceItem.IsValid()) return false; // Empty source

    // Find the best target tagged slot
    const FGameplayTag TargetSlotTag = FindTaggedSlotForItem(SourceItem.GetItemId(), SourceItem.GetQuantity());

    if (!TargetSlotTag.IsValid()) {
        UE_LOG(LogRISInventory, Log, TEXT("MoveItemToAnyTaggedSlot: No suitable tagged slot found for item %s."), *SourceItem.GetItemId().ToString());
        return false; // No suitable slot found
    }

    // Call the regular MoveItem logic
    return MoveItem(SourceTaggedSlot, SourceSlotIndex, TargetSlotTag, -1);
}


bool UInventoryGridViewModel::CanTaggedSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool CheckContainerLimits) const
{
    if (!LinkedInventoryComponent || !ItemId.IsValid() || !SlotTag.IsValid() || Quantity <= 0) return false;

    // Basic compatibility check
    if (!LinkedInventoryComponent->IsTaggedSlotCompatible(ItemId, SlotTag)) return false;
    // Check blocking (using component's logic is safer)
    // Simplified check: If the slot *is currently* blocked, can't receive.
    // A full check would involve WouldItemMoveIndirectlyViolateBlocking.
    if (LinkedInventoryComponent->IsTaggedSlotBlocked(SlotTag)) return false;

    // Optional: Check overall container limits (weight/slots)
    if (CheckContainerLimits && !LinkedInventoryComponent->CanContainerReceiveItems(ItemId, Quantity)) return false;

    // Check stacking within the specific tagged slot
    const FTaggedItemBundle* TargetSlotItemPtr = ViewableTaggedSlots.Find(SlotTag);
    if (!TargetSlotItemPtr) return false; // Slot doesn't exist visually

    const FTaggedItemBundle& TargetSlotItem = *TargetSlotItemPtr;
    const bool bTargetSlotEmpty = !TargetSlotItem.IsValid();

    if (bTargetSlotEmpty) {
        return true; // Empty compatible slot can receive
    }
    else if (TargetSlotItem.ItemId == ItemId) {
        // Same item, check stack size
        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
        if (!ItemData) return false;
        if (ItemData->MaxStackSize <= 1) return false; // Not stackable
        return (TargetSlotItem.Quantity + Quantity) <= ItemData->MaxStackSize;
    }
    else {
        // Different item in the slot
        return false; // Cannot stack different items
    }
}

void UInventoryGridViewModel::PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots, bool DestroyAfterPickup)
{
	 if (!WorldItem || !LinkedInventoryComponent) return;

     // Use the Inventory Component's PickupItem which handles the policy logic
     // It will internally call AddItem or AddItemToTaggedSlot, triggering our handlers.
     LinkedInventoryComponent->PickupItem(WorldItem, PreferTaggedSlots, DestroyAfterPickup);

     // Note: We don't need explicit prediction here because the component's call
     // will result in HandleItemAdded or HandleTaggedItemAdded being called,
     // which already contain prediction/confirmation logic.
}


// --- Override Assert ---
bool UInventoryGridViewModel::AssertViewModelSettled() const
{
    // --- Check Pending Operations ---
    // This check remains the same, ensuring no client-side actions are awaiting server confirmation.
    bool bOpsSettled = OperationsToConfirm.IsEmpty();
    ensureMsgf(bOpsSettled, TEXT("InventoryViewModel is not settled. %d operations pending."), OperationsToConfirm.Num());
    if (!bOpsSettled)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("InventoryViewModel pending ops: %d"), OperationsToConfirm.Num());
        // Log details of pending ops for easier debugging
        for(const auto& Op : OperationsToConfirm) {
             if(Op.TaggedSlot.IsValid()) {
                  UE_LOG(LogRISInventory, Warning, TEXT("  - Pending Tagged Op: %d for %s on %s (Qty: %d)"), (int)Op.Operation, *Op.ItemId.ToString(), *Op.TaggedSlot.ToString(), Op.Quantity);
             } else {
                  UE_LOG(LogRISInventory, Warning, TEXT("  - Pending Grid Op: %d for %s (Qty: %d)"), (int)Op.Operation, *Op.ItemId.ToString(), Op.Quantity);
             }
        }
    }

    // --- Check Total Item Quantities ---
    bool bQuantitiesMatch = true;
    if (LinkedInventoryComponent)
    {
        // 1. Get Ground Truth: Total quantities from the Inventory Component
        TMap<FGameplayTag, int32> ComponentTotalQuantities;
        // Use GetAllContainerItems() as it should represent the superset in InventoryComponent
        // If InventoryComponent separates storage, this might need adjustment. Assuming GetAllContainerItems is the total.
        for (const auto& Item : LinkedInventoryComponent->GetAllContainerItems())
        {
            if (Item.Quantity > 0 && Item.ItemId.IsValid()) {
                 ComponentTotalQuantities.FindOrAdd(Item.ItemId) += Item.Quantity;
            }
        }

        // 2. Get ViewModel Total: Sum quantities from BOTH grid and tagged slots visually
        TMap<FGameplayTag, int32> ViewModelTotalQuantities;
        for (const FItemBundle& Slot : ViewableGridSlots)
        {
            if (Slot.IsValid())
            {
                ViewModelTotalQuantities.FindOrAdd(Slot.ItemId) += Slot.Quantity;
            }
        }
        for (const auto& Pair : ViewableTaggedSlots)
        {
            if (Pair.Value.IsValid())
            {
                ViewModelTotalQuantities.FindOrAdd(Pair.Value.ItemId) += Pair.Value.Quantity;
                // While we are add it, double check tagged slot tag matching
                ensureMsgf(Pair.Value.Tag == Pair.Key, TEXT("Tagged slot %s has mismatched tag %s."), *Pair.Key.ToString(), *Pair.Value.Tag.ToString());
            }
        }

        // 3. Compare Component total vs ViewModel total
        // Check items present in Component but missing/wrong in ViewModel
        for (const auto& CompPair : ComponentTotalQuantities)
        {
            const int32* VmQtyPtr = ViewModelTotalQuantities.Find(CompPair.Key);
            int32 VmQty = VmQtyPtr ? *VmQtyPtr : 0;
            if (VmQty != CompPair.Value)
            {
                bQuantitiesMatch = false;
                ensureMsgf(false, TEXT("Total Quantity mismatch for %s. Component: %d, ViewModel(Grid+Tagged): %d"), *CompPair.Key.ToString(), CompPair.Value, VmQty);
                UE_LOG(LogRISInventory, Warning, TEXT("Total Quantity mismatch for %s. Component: %d, ViewModel(Grid+Tagged): %d"), *CompPair.Key.ToString(), CompPair.Value, VmQty);
                // Don't break, log all mismatches
            }
        }
        // Check items present in ViewModel but missing/wrong in Component
        for (const auto& VmPair : ViewModelTotalQuantities)
        {
             const int32* CompQtyPtr = ComponentTotalQuantities.Find(VmPair.Key);
             int32 CompQty = CompQtyPtr ? *CompQtyPtr : 0;
             if (CompQty != VmPair.Value)
             {
                  // This case should have been caught above, but double-check
                  if (bQuantitiesMatch) // Avoid duplicate ensure messages
                  {
                        bQuantitiesMatch = false;
                        ensureMsgf(false, TEXT("Total Quantity mismatch for %s. ViewModel(Grid+Tagged): %d, Component: %d"), *VmPair.Key.ToString(), VmPair.Value, CompQty);
                        UE_LOG(LogRISInventory, Warning, TEXT("Total Quantity mismatch for %s. ViewModel(Grid+Tagged): %d, Component: %d"), *VmPair.Key.ToString(), VmPair.Value, CompQty);
                  }
             }
        }
        ensureMsgf(bQuantitiesMatch, TEXT("InventoryViewModel total quantities (Grid+Tagged) do not match LinkedInventoryComponent totals."));
         if (!bQuantitiesMatch) UE_LOG(LogRISInventory, Warning, TEXT("InventoryViewModel total quantity mismatch."));

    }
    else
    {
        UE_LOG(LogRISInventory, Warning, TEXT("AssertViewModelSettled: LinkedInventoryComponent is null. Cannot verify quantities."));
        bQuantitiesMatch = false; // Cannot verify
    }


    // --- Check Tagged Slot Consistency ---
    bool bTaggedConsistency = true;
    if (LinkedInventoryComponent)
    {
         // This part compares the specific items/quantities/tags within each tagged slot view vs component state.
         TMap<FGameplayTag, FTaggedItemBundle> ActualTaggedItemsMap;
         for(const auto& Item : LinkedInventoryComponent->GetAllTaggedItems()) {
              if (Item.Tag.IsValid()) ActualTaggedItemsMap.Add(Item.Tag, Item);
         }

         for (const auto& VMPair : ViewableTaggedSlots)
         {
              const FTaggedItemBundle* ActualItem = ActualTaggedItemsMap.Find(VMPair.Key);

              // Compare VM slot state vs Actual component state for this specific tag
              if ((!ActualItem || !ActualItem->IsValid()) && VMPair.Value.IsValid()) {
                   bTaggedConsistency = false;
                   ensureMsgf(false, TEXT("Tagged slot state mismatch: VM has %s in %s, Component has none."), *VMPair.Value.ItemId.ToString(), *VMPair.Key.ToString());
                   UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot state mismatch: VM has %s in %s, Component has none."), *VMPair.Value.ItemId.ToString(), *VMPair.Key.ToString());
              } else if (ActualItem && ActualItem->IsValid() && !VMPair.Value.IsValid()) {
                   bTaggedConsistency = false;
                   ensureMsgf(false, TEXT("Tagged slot state mismatch: VM has empty in %s, Component has %s."), *VMPair.Key.ToString(), *ActualItem->ItemId.ToString());
                   UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot state mismatch: VM has empty in %s, Component has %s."), *VMPair.Key.ToString(), *ActualItem->ItemId.ToString());
              } else if (ActualItem && ActualItem->IsValid() && VMPair.Value.IsValid()) {
                   if (ActualItem->ItemId != VMPair.Value.ItemId || ActualItem->Quantity != VMPair.Value.Quantity) {
                        bTaggedConsistency = false;
                         ensureMsgf(false, TEXT("Tagged slot state mismatch in %s: VM has %s(x%d), Component has %s(x%d)."),
                                    *VMPair.Key.ToString(),
                                    *VMPair.Value.ItemId.ToString(), VMPair.Value.Quantity,
                                    *ActualItem->ItemId.ToString(), ActualItem->Quantity);
                         UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot state mismatch in %s: VM has %s(x%d), Component has %s(x%d)."),
                                    *VMPair.Key.ToString(),
                                    *VMPair.Value.ItemId.ToString(), VMPair.Value.Quantity,
                                    *ActualItem->ItemId.ToString(), ActualItem->Quantity);
                   }
                    // Check internal tag consistency
                    if (VMPair.Value.Tag != VMPair.Key) {
                         bTaggedConsistency = false;
                         ensureMsgf(false, TEXT("Tagged slot internal Tag property mismatch in %s: Key is %s, Item.Tag is %s."), *VMPair.Key.ToString(), *VMPair.Key.ToString(), *VMPair.Value.Tag.ToString());
                         UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot internal Tag property mismatch in %s: Key is %s, Item.Tag is %s."), *VMPair.Key.ToString(), *VMPair.Key.ToString(), *VMPair.Value.Tag.ToString());
                    }
              }
              // If both ActualItem is null and VMPair.Value is invalid, it's consistent (both empty).
         }
         // Also check if Component has a tagged item not present visually at all
         for (const auto& ActualPair : ActualTaggedItemsMap) {
             if (!ViewableTaggedSlots.Contains(ActualPair.Key)) {
                 bTaggedConsistency = false;
                 ensureMsgf(false, TEXT("Tagged slot state mismatch: Component has %s in %s, VM has no such tag."), *ActualPair.Value.ItemId.ToString(), *ActualPair.Key.ToString());
                 UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot state mismatch: Component has %s in %s, VM has no such tag."), *ActualPair.Value.ItemId.ToString(), *ActualPair.Key.ToString());
             }
         }

          ensureMsgf(bTaggedConsistency, TEXT("InventoryViewModel tagged slots do not match LinkedInventoryComponent state."));
           if (!bTaggedConsistency) UE_LOG(LogRISInventory, Warning, TEXT("InventoryViewModel tagged slot state mismatch."));
    }


    // Final Result
    return bOpsSettled && bQuantitiesMatch && bTaggedConsistency;
}

// --- Tagged Event Handlers ---

void UInventoryGridViewModel::HandleTaggedItemAdded_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, FTaggedItemBundle PreviousItem, EItemChangeReason Reason)
{
    if (!ItemData || Quantity <= 0 || !SlotTag.IsValid()) return;

    // Try to confirm pending operation
    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == ERISSlotOperation::AddTagged &&
            OperationsToConfirm[i].TaggedSlot == SlotTag &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
            // If prediction was complex, reconcile here. Often, it's assumed correct.
            // We might need to double-check the quantity if prediction was just partial.
             if (ViewableTaggedSlots.Contains(SlotTag) && ViewableTaggedSlots[SlotTag].ItemId == ItemData->ItemId) {
                 // Ensure quantity matches exactly if operation confirmed
                  if(ViewableTaggedSlots[SlotTag].Quantity != LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag).Quantity) {
                       UE_LOG(LogRISInventory, Log, TEXT("Correcting predicted quantity for tag %s after confirmed add."), *SlotTag.ToString());
                       ViewableTaggedSlots[SlotTag].Quantity = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag).Quantity;
                       // No broadcast here, assume prediction broadcasted earlier. Re-broadcast if needed.
                  }
             } else {
                  // Prediction was wrong item or slot didn't exist visually? Force update for this slot.
                  UE_LOG(LogRISInventory, Warning, TEXT("Mismatch after confirming AddTagged for tag %s. Forcing slot update."), *SlotTag.ToString());
                  FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
                   ViewableTaggedSlots.FindOrAdd(SlotTag) = ActualItem; // Force overwrite visual
                   OnTaggedSlotUpdated.Broadcast(SlotTag);
             }

            return;
        }
    }

    // Handle unpredicted server add
    UE_LOG(LogRISInventory, Log, TEXT("HandleTaggedItemAdded: Received unpredicted add for %s x%d to tag %s. Updating visuals."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FTaggedItemBundle& TargetSlot = ViewableTaggedSlots[SlotTag];
        if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId)
        {
            // Item matches, just add quantity (server state is truth)
            // Get actual current quantity from component to be safe
             FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
             TargetSlot.Quantity = ActualItem.Quantity;
        }
        else
        {
            // Different item or empty, overwrite with server state
             FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
             TargetSlot = ActualItem; // Assign directly
        }
        OnTaggedSlotUpdated.Broadcast(SlotTag);
    }
    else
    {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemAdded: Received add for unmanaged tag %s!"), *SlotTag.ToString());
          // Optionally add the tag visually now?
          // ViewableTaggedSlots.Add(SlotTag, LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag));
          // OnTaggedSlotUpdated.Broadcast(SlotTag);
    }
}

void UInventoryGridViewModel::HandleTaggedItemRemoved_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason)
{
    if (!ItemData || Quantity <= 0 || !SlotTag.IsValid()) return;

    // Try to confirm pending operation
    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == RemoveTagged &&
            OperationsToConfirm[i].TaggedSlot == SlotTag &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
             // Assuming prediction was correct. Verify quantity?
             if (ViewableTaggedSlots.Contains(SlotTag)) {
                 FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
                 if(!ActualItem.IsValid() && ViewableTaggedSlots[SlotTag].IsValid()) {
                      // Server says empty, VM has item. Correct VM.
                      ViewableTaggedSlots[SlotTag] = ActualItem; // Make VM empty
                       // Broadcast handled by prediction? Re-broadcast if needed.
                 } else if (ActualItem.IsValid() && ViewableTaggedSlots[SlotTag].IsValid() && ActualItem.Quantity != ViewableTaggedSlots[SlotTag].Quantity) {
                      // Server has different quantity, correct VM.
                       ViewableTaggedSlots[SlotTag].Quantity = ActualItem.Quantity;
                 }
                  // If both are valid and quantities match, prediction was good.
             }

            return;
        }
    }

    // Handle unpredicted server remove
     UE_LOG(LogRISInventory, Log, TEXT("HandleTaggedItemRemoved: Received unpredicted remove for %s x%d from tag %s. Updating visuals."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FTaggedItemBundle& TargetSlot = ViewableTaggedSlots[SlotTag];
        if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId)
        {
            // Get actual state from component
            FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
            TargetSlot.ItemId = ActualItem.ItemId; // Overwrite visual state with actual state
            TargetSlot.Quantity = ActualItem.Quantity;

            OnTaggedSlotUpdated.Broadcast(SlotTag);
        }
        else if (TargetSlot.IsValid() && TargetSlot.ItemId != ItemData->ItemId)
        {
             UE_LOG(LogRISInventory, Warning, TEXT("HandleTaggedItemRemoved: Server removed %s from tag %s, but VM shows %s. Forcing update."), *ItemData->ItemId.ToString(), *SlotTag.ToString(), *TargetSlot.ItemId.ToString());
             ForceFullInventoryUpdate(); // Resync everything if major discrepancy
        }
        // If TargetSlot is already empty visually, do nothing.
    }
     else {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemRemoved: Received remove for unmanaged tag %s!"), *SlotTag.ToString());
     }
}


FGameplayTag UInventoryGridViewModel::FindTaggedSlotForItem(const FGameplayTag& ItemId, int32 Quantity) const
{
    // This logic is identical to the original implementation, just moved here.
    // Validate
	if (!ItemId.IsValid() || !LinkedInventoryComponent) return FGameplayTag();

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData) return FGameplayTag(); // Ensure the item data is valid

	FGameplayTag FallbackSlot = FGameplayTag();
	FGameplayTag FallbackFallbackSlot = FGameplayTag(); // For compatible but non-empty/non-ideal universal
    FGameplayTag BestPartialStackSlot = FGameplayTag();

	// --- Pass 1: Check existing items for stacking ---
    for (const auto& Pair : ViewableTaggedSlots) {
         const FGameplayTag& SlotTag = Pair.Key;
         const FTaggedItemBundle& ExistingItem = Pair.Value;

         if (ExistingItem.IsValid() && ExistingItem.ItemId == ItemId) {
              // Found same item. Check if stackable and has space.
              if (ItemData->MaxStackSize > 1 && ExistingItem.Quantity < ItemData->MaxStackSize) {
                   // Found a partial stack, this is usually the best option.
                   BestPartialStackSlot = SlotTag;
                   break; // Found best option, stop searching for partials
              }
              // If MaxStackSize is 1 or stack is full, continue searching for empty slots.
         }
    }

     // If we found a partial stack, return it immediately.
     if (BestPartialStackSlot.IsValid()) {
          return BestPartialStackSlot;
     }


	// --- Pass 2: Find Empty & Compatible Slots ---
	// First try specialized slots
	for (const FGameplayTag& SlotTag : LinkedInventoryComponent->SpecializedTaggedSlots)
	{
		if (LinkedInventoryComponent->IsTaggedSlotCompatible(ItemId, SlotTag)) // Use component's check
		{
			if (IsTaggedSlotEmpty(SlotTag)) // Check visual emptiness
			{
				return SlotTag; // Found empty specialized slot
			}
			// If not empty, remember it as a potential non-empty fallback
			if (!FallbackSlot.IsValid()) FallbackSlot = SlotTag;
		}
	}

	// If we prefer NOT using empty universal slots AND we found a compatible non-empty specialized slot, return it now.
	if (!bPreferEmptyUniversalSlots && FallbackSlot.IsValid()) return FallbackSlot;

	// Then try universal slots
	for (const FUniversalTaggedSlot& UniSlot : LinkedInventoryComponent->UniversalTaggedSlots)
	{
         const FGameplayTag& SlotTag = UniSlot.Slot;
		// Check compatibility using the component's method, which handles blocking etc.
		if (LinkedInventoryComponent->CanItemBeEquippedInUniversalSlot(ItemId, SlotTag, true)) // Check blocking
		{
			if (IsTaggedSlotEmpty(SlotTag)) // Check visual emptiness
			{
                 // This is a candidate empty universal slot
                 // If we haven't found an empty specialized slot yet...
                 if(!FallbackSlot.IsValid() || !LinkedInventoryComponent->SpecializedTaggedSlots.Contains(FallbackSlot)) {
                      // Prioritize universal slots that match an item category? (Original logic)
                      bool bIsPreferredCategory = ItemData->ItemCategories.HasTag(SlotTag);
                      if(bIsPreferredCategory) {
                           return SlotTag; // Found an empty universal slot matching a preferred category
                      }
                      // If not preferred, remember it as a general empty universal slot
                      if(!FallbackSlot.IsValid()) FallbackSlot = SlotTag;
                 }

			}
			else
			{
				// Slot is compatible but not empty. Remember as lowest priority fallback.
				if (!FallbackFallbackSlot.IsValid()) FallbackFallbackSlot = SlotTag;
			}
		}
	}

	// --- Decide Fallback ---
    // If FallbackSlot is set (either non-empty specialized OR empty universal), use it.
	if (FallbackSlot.IsValid()) return FallbackSlot;

    // Otherwise, use the non-empty compatible universal slot if found.
	return FallbackFallbackSlot; // Might be invalid tag if nothing found
}

void UInventoryGridViewModel::ForceFullInventoryUpdate_Implementation()
{
    // Call base first to update the grid
    Super::ForceFullGridUpdate_Implementation();

    // Now update tagged slots
    if (!LinkedInventoryComponent)
    {
        UE_LOG(LogRISInventory, Error, TEXT("ForceFullInventoryUpdate: Cannot update tagged slots, LinkedInventoryComponent is null."));
        return;
    }

     UE_LOG(LogRISInventory, Log, TEXT("ForceFullInventoryUpdate: Resynchronizing visual tagged slots."));

     // Clear existing tagged items visually (keep slot structure)
     for (auto& Pair : ViewableTaggedSlots) {
          Pair.Value.ItemId = FGameplayTag();
          Pair.Value.Quantity = 0;
          // Keep Pair.Value.Tag as it defines the slot
     }

     // Repopulate tagged slots from component
     const TArray<FTaggedItemBundle>& ActualTaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
    for (const FTaggedItemBundle& TaggedItem : ActualTaggedItems)
    {
        if (ViewableTaggedSlots.Contains(TaggedItem.Tag))
        {
            ViewableTaggedSlots[TaggedItem.Tag] = TaggedItem; // Overwrite visual state
             OnTaggedSlotUpdated.Broadcast(TaggedItem.Tag); // Notify update
        }
        else
        {
            UE_LOG(LogRISInventory, Warning, TEXT("ForceFullInventoryUpdate: Tagged item %s found in component but tag %s is not registered visually."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
             // Optionally add the tag visually now if strictness allows
             // if(TaggedItem.Tag.IsValid()) {
             //     ViewableTaggedSlots.Add(TaggedItem.Tag, TaggedItem);
             //     OnTaggedSlotUpdated.Broadcast(TaggedItem.Tag);
             // }
        }
    }

     // It might be necessary to also broadcast updates for slots that became empty
     // Iterate ViewableTaggedSlots again? Or rely on UI checking state?
     // For now, assume direct overwrite and broadcast is sufficient.
}

// Override base pickup to call the inventory version
void UInventoryGridViewModel::PickupItemToContainer(AWorldItem* WorldItem, bool DestroyAfterPickup)
{
    // When called on an InventoryViewModel, default to the policy-based pickup
    PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, DestroyAfterPickup);
}

// Override grid drop/use to ensure correct component call (though base might already do this)
int32 UInventoryGridViewModel::DropItemFromGrid(int32 SlotIndex, int32 Quantity)
{
    // Ensure we call the method on the correct component type if needed,
    // but base UContainerGridViewModel::DropItemFromGrid likely calls LinkedContainerComponent->DropItems already.
    // This override might be redundant unless specific inventory logic is needed.
    return Super::DropItemFromGrid(SlotIndex, Quantity);
}

int32 UInventoryGridViewModel::UseItemFromGrid(int32 SlotIndex)
{
     // Similarly, this might be redundant.
    return Super::UseItemFromGrid(SlotIndex);
}