// Copyright Rancorous Games, 2024
#include "ViewModels/InventoryGridViewModel.h"
#include "Components/InventoryComponent.h" // Include specific type
#include "Core/RISSubsystem.h"
#include "Data/ItemStaticData.h"
#include "Core/RISFunctions.h"
#include "Actors/WorldItem.h"
#include "LogRancInventorySystem.h"
#include "Data/UsableItemDefinition.h"


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

    LinkedInventoryComponent = InventoryComponent; // Store specific type
    
    // Call base class initialization FIRST for grid setup
    Super::Initialize_Implementation(InventoryComponent, NumGridSlots);

    // Now, bIsInitialized is true from the base call, but we continue *our* init.
    // If base init failed (e.g., null component), it should have returned early.
    if (!bIsInitialized || !InventoryComponent) {
         UE_LOG(LogRISInventory, Error, TEXT("InventoryGridViewModel::InitializeInventory failed after base initialization or with null InventoryComponent."));
         bIsInitialized = false; // Reset flag if something went wrong
         return;
    }
    
    bPreferEmptyUniversalSlots = InPreferEmptyUniversalSlots;
    ViewableTaggedSlots.Empty();

    // Subscribe to Inventory-specific events
    LinkedInventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemAdded);
    LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemRemoved);

    // Initialize Tagged Slots map structure with empty FItemBundle
    for (const FUniversalTaggedSlot& UniTag : LinkedInventoryComponent->UniversalTaggedSlots)
    {
        // Ensure tag is valid before adding
        if(UniTag.Slot.IsValid()) {
            ViewableTaggedSlots.Add(UniTag.Slot, FItemBundle(FGameplayTag(), 0));
        } else {
             UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Found invalid tag in UniversalTaggedSlots."));
        }
    }
    for (const FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots)
    {
         if(Tag.IsValid()) {
            ViewableTaggedSlots.Add(Tag, FItemBundle(FGameplayTag(), 0));
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
            ViewableTaggedSlots[TaggedItem.Tag] = TaggedItem.ItemId;
        }
        else
        {
            UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Tagged item %s found in component but tag %s is not registered in ViewableTaggedSlots."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
            // Optionally add it anyway? Depends on desired strictness.
             if(TaggedItem.Tag.IsValid()) {
                 ViewableTaggedSlots.Add(TaggedItem.Tag, FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData)); // Add FItemBundle part
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
    // Check FItemBundle::IsValid()
    const FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    return !Found || !Found->IsValid();
}

const FItemBundle& UInventoryGridViewModel::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
    // Returns FItemBundle
    if (const FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag))
    {
        return *Found;
    }
    // Return a static empty instance to avoid crashes on lookup failure
    UE_LOG(LogRISInventory, Warning, TEXT("GetItemForTaggedSlot: SlotTag %s not found."), *SlotTag.ToString());
    return FItemBundle::EmptyItemInstance;
}

FItemBundle& UInventoryGridViewModel::GetMutableItemForTaggedSlot(const FGameplayTag& SlotTag)
{
    FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    if (Found)
    {
        return *Found;
    }
    
    UE_LOG(LogRISInventory, Error, TEXT("GetMutableItemForTaggedSlot: Critical error: SlotTag %s not found. Adding new slot as a patch."), *SlotTag.ToString());
    ViewableTaggedSlots.Add(SlotTag, FItemBundle(FGameplayTag(), 0));
    return ViewableTaggedSlots[SlotTag]; // Return reference to newly added item
}

template <typename ElementType>
FORCEINLINE TArray<ElementType> GetLastElements(const TArray<ElementType>& SourceArray, int32 Quantity)
{
    const int32 SourceNum = SourceArray.Num();
    const int32 ValidQuantity = FMath::Clamp(Quantity, 0, SourceNum);
    TArray<ElementType> ResultArray;

    if (ValidQuantity == 0)
    {
        // No action needed
    }
    else if (ValidQuantity == SourceNum)
    {
        ResultArray = SourceArray;
    }
    else
    {
        const int32 StartIndex = SourceNum - ValidQuantity;
        ResultArray.Reserve(ValidQuantity);
        ResultArray.Append(SourceArray.GetData() + StartIndex, ValidQuantity);
    }

    return ResultArray;
}



int32 UInventoryGridViewModel::DropItemFromTaggedSlot(FGameplayTag TaggedSlot, int32 Quantity)
{
    if (!LinkedInventoryComponent || !TaggedSlot.IsValid() || Quantity <= 0)
        return 0;

    // Find the FItemBundle visually
    FItemBundle* SourceSlotItemPtr = ViewableTaggedSlots.Find(TaggedSlot);
    if (!SourceSlotItemPtr || !SourceSlotItemPtr->IsValid())
        return 0; // Slot is empty or doesn't exist

    FItemBundle& SourceSlotItem = *SourceSlotItemPtr;
    
    int32 QuantityToDrop = FMath::Min(Quantity, SourceSlotItem.Quantity);
     if (QuantityToDrop <= 0) return 0;

    if (QuantityToDrop > 0)
        OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, SourceSlotItem.ItemId, QuantityToDrop));
    
    auto InstancesToDrop = SourceSlotItem.GetInstancesFromEnd(QuantityToDrop);
    int32 DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(TaggedSlot, QuantityToDrop, InstancesToDrop);

    
    if (DroppedCount > 0) // Assume server might succeed
    {
        SourceSlotItem.Quantity -= DroppedCount;
        if (SourceSlotItem.Quantity <= 0)
        {
             SourceSlotItem.ItemId = FGameplayTag(); // Clear ItemId and Quantity
             SourceSlotItem.Quantity = 0;
        }
        
        if (InstancesToDrop.IsEmpty() || InstancesToDrop.Num() == DroppedCount)
            SourceSlotItem.InstanceData.Empty();
        else
            SourceSlotItem.InstanceData.RemoveAll([&InstancesToDrop](UItemInstanceData* Instance) {
                return InstancesToDrop.Contains(Instance);
            });
        
        OnTaggedSlotUpdated.Broadcast(TaggedSlot);
    }

    return DroppedCount;
}

int32 UInventoryGridViewModel::UseItemFromTaggedSlot(FGameplayTag TaggedSlot)
{
	if (!LinkedInventoryComponent || !TaggedSlot.IsValid())
        return 0;

	FItemBundle* SourceSlotItemPtr = ViewableTaggedSlots.Find(TaggedSlot);
	if (!SourceSlotItemPtr || !SourceSlotItemPtr->IsValid())
		return 0;

	FItemBundle& SourceSlotItem = *SourceSlotItemPtr;
	const FGameplayTag ItemIdToUse = SourceSlotItem.ItemId;

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemIdToUse);
    int32 UniqueInstanceIdToUse = -1;

    if (!ItemData)
    {
        UE_LOG(LogRISInventory, Error, TEXT("UseItemFromTaggedSlot: Item data not found for item ID %s."), *ItemIdToUse.ToString());
        return 0;
    }

    const UUsableItemDefinition* UsableDef = ItemData->GetItemDefinition<UUsableItemDefinition>();
    int32 QuantityToConsume = UsableDef ? UsableDef->QuantityPerUse : 0;

    if (SourceSlotItem.Quantity < QuantityToConsume)
        return 0;
    
    if (QuantityToConsume > 0)
    {
        if (SourceSlotItem.InstanceData.Num() > 0)
        {
            if (QuantityToConsume == 1)
                UniqueInstanceIdToUse = SourceSlotItem.InstanceData.Pop(EAllowShrinking::No)->UniqueInstanceId;
            else
            {
                for (int32 i = 0; i < QuantityToConsume; ++i) {
                    if(SourceSlotItem.InstanceData.Num() > 0)
                        SourceSlotItem.InstanceData.Pop(EAllowShrinking::No);
                }
            }
            SourceSlotItem.InstanceData.Shrink();
        }

        SourceSlotItem.Quantity -= QuantityToConsume;
        if (SourceSlotItem.Quantity <= 0) {
            SourceSlotItem.ItemId = FGameplayTag();
            SourceSlotItem.Quantity = 0;
            SourceSlotItem.InstanceData.Empty();
        }
        OnTaggedSlotUpdated.Broadcast(TaggedSlot);
        
        OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TaggedSlot, ItemIdToUse, QuantityToConsume));
    }
    
    LinkedInventoryComponent->UseItemFromTaggedSlot(TaggedSlot, UniqueInstanceIdToUse);
    
	return QuantityToConsume;
}

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

    const bool bSourceIsGrid = !SourceTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(SourceSlotIndex);
    const bool bSourceIsTag = SourceTaggedSlot.IsValid() && ViewableTaggedSlots.Contains(SourceTaggedSlot);
    const bool bTargetIsGrid = !TargetTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(TargetSlotIndex);
    const bool bTargetIsTag = TargetTaggedSlot.IsValid() && ViewableTaggedSlots.Contains(TargetTaggedSlot);

    if ((!bSourceIsGrid && !bSourceIsTag) || (!bTargetIsGrid && !bTargetIsTag)) return false;
    if (bSourceIsGrid && bTargetIsGrid && SourceSlotIndex == TargetSlotIndex) return false;
    if (bSourceIsTag && bTargetIsTag && SourceTaggedSlot == TargetTaggedSlot) return false;

    FItemBundle* SourceItem = nullptr;
    if (bSourceIsTag) {
        SourceItem = &ViewableTaggedSlots[SourceTaggedSlot]; // Use mutable map directly
    } else { // Source is Grid
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }
    if (!SourceItem->IsValid()) return false; // Cannot move/split empty source

    const FGameplayTag ItemIdToMove = SourceItem->ItemId;
    int32 MaxSourceQty = SourceItem->Quantity;
    int32 RequestedQuantity = IsSplit ? InQuantity : MaxSourceQty;

    if (RequestedQuantity <= 0) return false;
    if (IsSplit && RequestedQuantity > MaxSourceQty) {
        UE_LOG(LogRISInventory, Warning, TEXT("Cannot split more items than available."));
        return false;
    }
    
    FItemBundle* TargetItem  = nullptr;
     if (bTargetIsTag) {
         TargetItem = &ViewableTaggedSlots[TargetTaggedSlot];
     } else {
         TargetItem = &ViewableGridSlots[TargetSlotIndex];
     }

    if (bTargetIsTag) {
        if (!LinkedInventoryComponent->IsTaggedSlotCompatible(ItemIdToMove, TargetTaggedSlot)) {
            UE_LOG(LogRISInventory, Warning, TEXT("Item %s not compatible with tagged slot %s."), *ItemIdToMove.ToString(), *TargetTaggedSlot.ToString());
            return false;
        }
    }
    
    auto InstancesToMove = SourceItem->GetInstancesFromEnd(RequestedQuantity);

    FGameplayTag SwapItemId = FGameplayTag();
    int32 SwapQuantity = -1;
    if (!IsSplit && TargetItem->IsValid() && TargetItem->ItemId != ItemIdToMove) {
        SwapItemId = TargetItem->ItemId;
        SwapQuantity = TargetItem->Quantity;
    }

    // --- Validate Move with Inventory Component (Handles Blocking, etc.) ---
    int32 QuantityValidatedByServer = RequestedQuantity; // Assume client is correct for grid->grid
    if (bSourceIsTag || bTargetIsTag) // If involves tagged slots, ask server component
    {
        QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);

        // If initial validation failed due to blocking, try unblocking
        if (QuantityValidatedByServer <= 0 && bTargetIsTag && !IsSplit && TryUnblockingMove(TargetTaggedSlot, ItemIdToMove))
        {
             // Try validation again after unblocking attempt
             QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
        }

        if (QuantityValidatedByServer <= 0) {
             UE_LOG(LogRISInventory, Log, TEXT("Move/Split failed server-side validation."));
             return false; // Server component prevents this move
        }
    }

    // Adjust quantity if server validation reduced it (e.g., partial stack in target tagged slot)
    int32 QuantityToActuallyMove = QuantityValidatedByServer;
    
    FGenericItemBundle SourceItemGB(SourceItem);
    FGenericItemBundle TargetItemGB(TargetItem);
    FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItemGB, TargetItemGB, false, QuantityToActuallyMove, InstancesToMove, !IsSplit, !IsSplit); // Allow swaps only if not splitting

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
                        OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem->ItemId, SourceItem->Quantity));
                    else
                        OperationsToConfirm.Emplace(FRISExpectedOperation(Remove, SourceItem->ItemId, SourceItem->Quantity));
                    OperationsToConfirm.Emplace(FRISExpectedOperation(AddTagged, SourceTaggedSlot, SourceItem->ItemId, SourceItem->Quantity));
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
            		    OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveTagged, TargetTaggedSlot, SourceItem->ItemId, SourceItem->Quantity));
            		    OperationsToConfirm.Emplace(FRISExpectedOperation(Add, SourceItem->ItemId, SourceItem->Quantity));
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
            LinkedInventoryComponent->MoveItem(ItemIdToMove, QuantityToActuallyMove, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
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
          FItemBundle BlockingItem = GetItemForTaggedSlot(SlotToClear);

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
    const FGameplayTag TargetSlotTag = FindTaggedSlotForItem(SourceItem.GetItemId(), SourceItem.GetQuantity(), EPreferredSlotPolicy::PreferSpecializedTaggedSlot);

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

    // Check stacking within the specific tagged slot visually (using FItemBundle)
    const FItemBundle* TargetSlotItemPtr = ViewableTaggedSlots.Find(SlotTag);
    if (!TargetSlotItemPtr) return false; // Slot doesn't exist visually

    const FItemBundle& TargetSlotItem = *TargetSlotItemPtr; 
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
        for (const auto& Item : LinkedInventoryComponent->GetAllItems())
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
        for (const auto& Pair : ViewableTaggedSlots) // Pair is <FGameplayTag, FItemBundle>
        {
            if (Pair.Value.IsValid()) // Check FItemBundle
            {
                ViewModelTotalQuantities.FindOrAdd(Pair.Value.ItemId) += Pair.Value.Quantity;
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

void UInventoryGridViewModel::HandleTaggedItemAdded_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& AddedInstances, FTaggedItemBundle PreviousItem, EItemChangeReason Reason)
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
                auto& ViewableItem = ViewableTaggedSlots.FindOrAdd(SlotTag);
                ViewableItem.ItemId = ActualItem.ItemId;
                ViewableItem.InstanceData = ActualItem.InstanceData;
                ViewableItem.Quantity = ActualItem.Quantity;
                OnTaggedSlotUpdated.Broadcast(SlotTag);
             }

            return;
        }
    }

    // Handle unpredicted server add
    UE_LOG(LogRISInventory, Verbose, TEXT("HandleTaggedItemAdded: Received unpredicted add for %s x%d to tag %s. Updating viewmodel."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FItemBundle& TargetSlot = ViewableTaggedSlots[SlotTag];
        
        FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
        if (!TargetSlot.IsValid() || TargetSlot.ItemId == ItemData->ItemId)
        {
            TargetSlot.ItemId = ActualItem.ItemId;
            TargetSlot.InstanceData.Empty();
        }
        
        TargetSlot.Quantity = ActualItem.Quantity;
        TargetSlot.InstanceData = ActualItem.InstanceData;
        OnTaggedSlotUpdated.Broadcast(SlotTag);
    }
    else
    {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemAdded: Critical Error: Received add for unmanaged tag %s!"), *SlotTag.ToString());
    }
}

void UInventoryGridViewModel::HandleTaggedItemRemoved_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& RemovedInstances, EItemChangeReason Reason)
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
                     ViewableTaggedSlots[SlotTag].ItemId = ActualItem.ItemId;
                     ViewableTaggedSlots[SlotTag].Quantity = ActualItem.Quantity;
                     ViewableTaggedSlots[SlotTag].InstanceData = ActualItem.InstanceData;
                 } else if (ActualItem.IsValid() && ViewableTaggedSlots[SlotTag].IsValid() && ActualItem.Quantity != ViewableTaggedSlots[SlotTag].Quantity) {
                      // Server has different quantity, correct VM.
                       ViewableTaggedSlots[SlotTag].Quantity = ActualItem.Quantity;
                     ViewableTaggedSlots[SlotTag].InstanceData = ActualItem.InstanceData;
                 }
             }

            return;
        }
    }

    // Handle unpredicted server remove
     UE_LOG(LogRISInventory, Verbose, TEXT("HandleTaggedItemRemoved: Received unpredicted remove for %s x%d from tag %s. Updating visuals."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FItemBundle& TargetSlot = ViewableTaggedSlots[SlotTag];
        if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId)
        {
            // Get actual state from component
            FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag);
            if (ActualItem.IsValid()) {
                TargetSlot.ItemId = ActualItem.ItemId;
                TargetSlot.Quantity = ActualItem.Quantity;
                TargetSlot.InstanceData = ActualItem.InstanceData;
            } else {
                TargetSlot.ItemId = FGameplayTag();
                TargetSlot.Quantity = 0;
                TargetSlot.InstanceData = FItemBundle::NoInstances;
            }

            OnTaggedSlotUpdated.Broadcast(SlotTag);
        }
        else if (TargetSlot.IsValid() && TargetSlot.ItemId != ItemData->ItemId)
        {
             UE_LOG(LogRISInventory, Warning, TEXT("HandleTaggedItemRemoved: Server removed %s from tag %s, but VM shows %s. Forcing update."), *ItemData->ItemId.ToString(), *SlotTag.ToString(), *TargetSlot.ItemId.ToString());
             ForceFullUpdate(); // Resync everything if major discrepancy
        }
        // If TargetSlot is already empty visually, do nothing.
    }
     else {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemRemoved: Received remove for unmanaged tag %s!"), *SlotTag.ToString());
     }
}


FGameplayTag UInventoryGridViewModel::FindTaggedSlotForItem(const FGameplayTag& ItemId, int32 Quantity, EPreferredSlotPolicy SlotPolicy) const
{
    // Validate
    if (!ItemId.IsValid() || !LinkedInventoryComponent || Quantity <= 0) return FGameplayTag(); // Also check Quantity > 0

    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
    if (!ItemData) return FGameplayTag();

    FGameplayTag FullyFittingPartialStackSlot; // Priority 1
    FGameplayTag EmptyCompatibleSlot;          // Priority 2 (Replaces FallbackSlot conceptually for empty slots)
    FGameplayTag AnyPartialStackSlot;         // Priority 3
    FGameplayTag CompatibleNonEmptySlot;     // Priority 4 (Replaces FallbackFallbackSlot)

    // --- Pass 1: Check existing items for stacking ---
    if (ItemData->MaxStackSize > 1) // Only look for stacks if item is stackable
    {
        for (const auto& Pair : ViewableTaggedSlots) { // Pair is <FGameplayTag, FItemBundle>
            const FGameplayTag& SlotTag = Pair.Key;
            const FItemBundle& ExistingItem = Pair.Value;

            if (ExistingItem.IsValid() && ExistingItem.ItemId == ItemId) {
                // Found same item. Check space.
                int32 AvailableSpace = ItemData->MaxStackSize - ExistingItem.Quantity;
                if (AvailableSpace > 0) {
                    if (AvailableSpace >= Quantity) {
                        // Found a stack that can take the full quantity. Best option.
                        FullyFittingPartialStackSlot = SlotTag;
                        break; // Found best option, stop searching partials
                    } else if (!AnyPartialStackSlot.IsValid()) {
                         // Found a partial stack, but not enough for full quantity.
                         // Remember the first one we find as a potential fallback (Priority 3).
                         AnyPartialStackSlot = SlotTag;
                    }
                }
            }
        }
    }

    // If we found a stack that fits the full quantity, return it immediately.
    if (FullyFittingPartialStackSlot.IsValid()) {
        return FullyFittingPartialStackSlot;
    }

    // --- Pass 2: Find Empty & Compatible Slots (Priority 2) ---
    // Check specialized slots first
    for (const FGameplayTag& SlotTag : LinkedInventoryComponent->SpecializedTaggedSlots)
    {
        if (LinkedInventoryComponent->IsTaggedSlotCompatible(ItemId, SlotTag))
        {
            if (IsTaggedSlotEmpty(SlotTag) || SlotPolicy == EPreferredSlotPolicy::PreferSpecializedTaggedSlot)
            {
                EmptyCompatibleSlot = SlotTag; // Found empty specialized slot
                goto FoundBestEmptySlot; // Use goto for minimal restructuring to exit loops early
            }
            // If not empty, remember it as a potential non-empty fallback (Priority 4)
            else if (!CompatibleNonEmptySlot.IsValid()) {
                CompatibleNonEmptySlot = SlotTag;
            }
        }
    }

    // Then try universal slots if no empty specialized slot was found yet
    for (const FUniversalTaggedSlot& UniSlot : LinkedInventoryComponent->UniversalTaggedSlots)
    {
        const FGameplayTag& SlotTag = UniSlot.Slot;
        if (LinkedInventoryComponent->CanItemBeEquippedInUniversalSlot(ItemId, SlotTag, true)) // Check blocking & compatibility
        {
            if (IsTaggedSlotEmpty(SlotTag)) // Check visual emptiness
            {
                // Prioritize universal slots that match an item category (sub-priority within empty slots)
                 bool bIsPreferredCategory = ItemData->ItemCategories.HasTag(SlotTag);
                 if(bIsPreferredCategory) {
                      EmptyCompatibleSlot = SlotTag; // Found preferred empty universal slot
                      goto FoundBestEmptySlot; // Exit loops
                 }
                 // If not preferred, remember it as a general empty universal slot if we don't have one yet
                 if(!EmptyCompatibleSlot.IsValid()) {
                      EmptyCompatibleSlot = SlotTag;
                 }
            }
            else if (!CompatibleNonEmptySlot.IsValid()) // If we haven't already found a non-empty spec slot
            {
                // Slot is compatible but not empty. Remember as lowest priority fallback (Priority 4).
                CompatibleNonEmptySlot = SlotTag;
            }
        }
    }

    FoundBestEmptySlot: // Label for goto

    // --- Final Decision ---
    if (EmptyCompatibleSlot.IsValid()) {
        return EmptyCompatibleSlot;         // Priority 2: Found an empty compatible slot
    }
    if (AnyPartialStackSlot.IsValid()) {
        return AnyPartialStackSlot;         // Priority 3: Found a partial stack (couldn't fit all)
    }

    // Priority 4: Return compatible non-empty slot, preferring specialized if found, otherwise universal
    // CompatibleNonEmptySlot already holds the highest priority non-empty slot found during the loops.
    return CompatibleNonEmptySlot;          // Might be invalid tag if nothing found at all
}

void UInventoryGridViewModel::ForceFullUpdate_Implementation()
{
    Super::ForceFullUpdate_Implementation();

    if (!LinkedInventoryComponent)
    {
        UE_LOG(LogRISInventory, Error, TEXT("ForceFullInventoryUpdate: Cannot update tagged slots, LinkedInventoryComponent is null."));
        return;
    }

    UE_LOG(LogRISInventory, Log, TEXT("ForceFullInventoryUpdate: Resynchronizing visual tagged slots."));

    TSet<FGameplayTag> PreviouslyOccupiedTags;
    for (const auto& Pair : ViewableTaggedSlots) {
        if(Pair.Value.IsValid()) {
            PreviouslyOccupiedTags.Add(Pair.Key);
        }
    }

    for (auto& Pair : ViewableTaggedSlots) {
        Pair.Value.ItemId = FGameplayTag();
        Pair.Value.Quantity = 0;
        //Pair.Value.InstanceData.Empty(); // Consider if visual instance data needs clearing too
    }

    const TArray<FTaggedItemBundle>& ActualTaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
    for (const FTaggedItemBundle& TaggedItem : ActualTaggedItems)
    {
        if (ViewableTaggedSlots.Contains(TaggedItem.Tag))
        {
            ViewableTaggedSlots[TaggedItem.Tag] = FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData);
            OnTaggedSlotUpdated.Broadcast(TaggedItem.Tag);
            PreviouslyOccupiedTags.Remove(TaggedItem.Tag); // Was occupied, still is (or changed item), handled by broadcast above
        }
        else
        {
            UE_LOG(LogRISInventory, Warning, TEXT("ForceFullInventoryUpdate: Tagged item %s found in component but tag %s is not registered visually."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
        }
    }

    for(const FGameplayTag& TagNowEmpty : PreviouslyOccupiedTags)
    {
        // This tag was occupied before, but wasn't in ActualTaggedItems, so it's now empty.
        OnTaggedSlotUpdated.Broadcast(TagNowEmpty);
    }
}
