// Copyright Rancorous Games, 2024
#include "ViewModels/InventoryGridViewModel.h"
#include "Components/ItemContainerComponent.h"
#include "Components/InventoryComponent.h" // Include specific type
#include "Core/RISSubsystem.h"
#include "Data/ItemStaticData.h"
#include "Core/RISFunctions.h"
#include "LogRancInventorySystem.h"
#include "Data/UsableItemDefinition.h"
#include "Actors/WorldItem.h" // Added include

// Define the static dummy member
FItemBundle UInventoryGridViewModel::DummyEmptyBundle = FItemBundle();

// --- Initialization and Lifecycle ---

void UInventoryGridViewModel::Initialize_Implementation(UItemContainerComponent* ContainerComponent)
{
    if (bIsInitialized || !ContainerComponent)
    {
        if (!ContainerComponent) UE_LOG(LogRISInventory, Warning, TEXT("RisInventoryViewModel::Initialize failed: ContainerComponent is null."));
        return;
    }

    LinkedContainerComponent = ContainerComponent;
    LinkedInventoryComponent = Cast<UInventoryComponent>(ContainerComponent); // Attempt cast

    NumberOfGridSlots = LinkedContainerComponent->MaxSlotCount;
    ViewableGridSlots.Init(FItemBundle::EmptyItemInstance, NumberOfGridSlots);
    OperationsToConfirm.Empty();

    // Subscribe to BASE container events
    LinkedContainerComponent->OnItemAddedToContainer.AddDynamic(this, &UInventoryGridViewModel::HandleItemAdded);
    LinkedContainerComponent->OnItemRemovedFromContainer.AddDynamic(this, &UInventoryGridViewModel::HandleItemRemoved);

    // --- Initialize Tagged Slots ONLY if it's an Inventory ---
    if (LinkedInventoryComponent)
    {
        bPreferEmptyUniversalSlots = true; // Set default or pass as param if needed later
        ViewableTaggedSlots.Empty();

        // Subscribe to Inventory-specific events
        LinkedInventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemAdded);
        LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UInventoryGridViewModel::HandleTaggedItemRemoved);

        // Initialize Tagged Slots map structure
        for (const FUniversalTaggedSlot& UniTag : LinkedInventoryComponent->UniversalTaggedSlots) {
            if(UniTag.Slot.IsValid()) ViewableTaggedSlots.Add(UniTag.Slot, FItemBundle::EmptyItemInstance);
        }
        for (const FGameplayTag& Tag : LinkedInventoryComponent->SpecializedTaggedSlots) {
             if(Tag.IsValid()) ViewableTaggedSlots.Add(Tag, FItemBundle::EmptyItemInstance);
        }
        // Initial population of tagged slots
        const TArray<FTaggedItemBundle>& ActualTaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
        for (const FTaggedItemBundle& TaggedItem : ActualTaggedItems) {
            if (ViewableTaggedSlots.Contains(TaggedItem.Tag)) {
                 ViewableTaggedSlots[TaggedItem.Tag] = FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData);
            } else if (TaggedItem.Tag.IsValid()) {
                 UE_LOG(LogRISInventory, Warning, TEXT("InitializeInventory: Tagged item %s found in component but tag %s is not registered in ViewableTaggedSlots. Adding it."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
                 ViewableTaggedSlots.Add(TaggedItem.Tag, FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData));
            }
        }
    }
    // --- End Tagged Slot Initialization ---

    bIsInitialized = true;

    // Initial population of the grid (applies to both container and inventory)
    ForceFullUpdate();
}

void UInventoryGridViewModel::BeginDestroy()
{
    // Unsubscribe from events to prevent crashes
    if (LinkedContainerComponent)
    {
        LinkedContainerComponent->OnItemAddedToContainer.RemoveDynamic(this, &UInventoryGridViewModel::HandleItemAdded);
        LinkedContainerComponent->OnItemRemovedFromContainer.RemoveDynamic(this, &UInventoryGridViewModel::HandleItemRemoved);
    }
    // Unsubscribe inventory-specific events
     if (LinkedInventoryComponent)
     {
         LinkedInventoryComponent->OnItemAddedToTaggedSlot.RemoveDynamic(this, &UInventoryGridViewModel::HandleTaggedItemAdded);
         LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.RemoveDynamic(this, &UInventoryGridViewModel::HandleTaggedItemRemoved);
     }
    Super::BeginDestroy();
}

// --- Grid Slot Functions ---

bool UInventoryGridViewModel::IsGridSlotEmpty(int32 SlotIndex) const
{
    return !ViewableGridSlots.IsValidIndex(SlotIndex) || !ViewableGridSlots[SlotIndex].IsValid();
}

FItemBundle UInventoryGridViewModel::GetGridItem(int32 SlotIndex) const
{
    if (ViewableGridSlots.IsValidIndex(SlotIndex))
    {
        return ViewableGridSlots[SlotIndex];
    }
    return FItemBundle::EmptyItemInstance;
}

int32 UInventoryGridViewModel::DropItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, int32 Quantity)
{
    if (!LinkedContainerComponent || Quantity <= 0) return 0;

    const bool bSourceIsGrid = !SourceTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(SourceSlotIndex);
    const bool bSourceIsTag = SourceTaggedSlot.IsValid();

    FItemBundle* SourceItemPtr = nullptr;
    ERISSlotOperation ExpectedOperationType;

    if (bSourceIsGrid) {
        SourceItemPtr = &ViewableGridSlots[SourceSlotIndex];
        ExpectedOperationType = ERISSlotOperation::Remove;
    } else if (bSourceIsTag) {
        if (!LinkedInventoryComponent) return 0;
        SourceItemPtr = ViewableTaggedSlots.Find(SourceTaggedSlot);
        ExpectedOperationType = ERISSlotOperation::RemoveTagged;
    } else {
        return 0;
    }

    if (!SourceItemPtr || !SourceItemPtr->IsValid()) return 0;

    FItemBundle& SourceItem = *SourceItemPtr;
    const FGameplayTag ItemIdToDrop = SourceItem.ItemId;
    int32 QuantityToDrop = FMath::Min(Quantity, SourceItem.Quantity);

    if (QuantityToDrop <= 0) return 0;

    OperationsToConfirm.Emplace(FRISExpectedOperation(ExpectedOperationType, SourceTaggedSlot, ItemIdToDrop, QuantityToDrop));

    auto InstancesToDrop = SourceItem.GetInstancesFromEnd(QuantityToDrop);
    int32 DroppedCount;

    if (bSourceIsGrid)
        DroppedCount = LinkedContainerComponent->DropItem(ItemIdToDrop, QuantityToDrop, InstancesToDrop);
    else // bSourceIsTag
        DroppedCount = LinkedInventoryComponent->DropFromTaggedSlot(SourceTaggedSlot, QuantityToDrop, InstancesToDrop);

    if (DroppedCount > 0) {
        SourceItem.Quantity -= DroppedCount;
        if (SourceItem.InstanceData.Num() > 0 && SourceItem.InstanceData.Num() >= DroppedCount) {
            for (int32 i = 0; i < DroppedCount; ++i) {
                if (SourceItem.InstanceData.Num() > 0) SourceItem.InstanceData.Pop(EAllowShrinking::No);
            }
            SourceItem.InstanceData.Shrink();
        } else if (SourceItem.InstanceData.Num() > 0 && SourceItem.InstanceData.Num() < DroppedCount) {
            SourceItem.InstanceData.Empty();
        }

        if (SourceItem.Quantity <= 0) {
            SourceItem = FItemBundle::EmptyItemInstance;
        }

        if (bSourceIsGrid) OnGridSlotUpdated.Broadcast(SourceSlotIndex);
        else OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
    }
    else
    {
        for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i) {
            const FRISExpectedOperation& Op = OperationsToConfirm[i];
            if (Op.Operation == ExpectedOperationType &&
                Op.ItemId == ItemIdToDrop &&
                Op.Quantity == QuantityToDrop &&
                ((bSourceIsGrid && !Op.TaggedSlot.IsValid()) || (bSourceIsTag && Op.TaggedSlot == SourceTaggedSlot))) {
                OperationsToConfirm.RemoveAt(i);
                break;
            }
        }
    }
    return DroppedCount;
}


bool UInventoryGridViewModel::CanGridSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const
{
    // Implementation moved from UContainerGridViewModel::CanGridSlotReceiveItem_Implementation
    if (!ViewableGridSlots.IsValidIndex(SlotIndex) || Quantity <= 0 || !ItemId.IsValid())
    {
        return false;
    }

    if (!LinkedContainerComponent || !LinkedContainerComponent->CanReceiveItem(ItemId, Quantity)) return false;

    const FItemBundle& TargetSlotItem = ViewableGridSlots[SlotIndex];
    const bool TargetSlotEmpty = !TargetSlotItem.IsValid();

    if (TargetSlotEmpty || TargetSlotItem.ItemId == ItemId)
    {
        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
        if (!ItemData)
        {
            return false;
        }

        const int32 AvailableSpace = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize - TargetSlotItem.Quantity : TargetSlotEmpty ? 1 : 0;
        return AvailableSpace >= Quantity;
    }

    return false;
}

// --- Tagged Slot Functions ---

bool UInventoryGridViewModel::IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const
{
    if (!LinkedInventoryComponent) return true; // No tagged slots if not an inventory
    const FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    return !Found || !Found->IsValid();
}

const FItemBundle& UInventoryGridViewModel::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
    if (!LinkedInventoryComponent) return DummyEmptyBundle;
    if (const FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag))
    {
        return *Found;
    }
    UE_LOG(LogRISInventory, Warning, TEXT("GetItemForTaggedSlot: SlotTag %s not found visually."), *SlotTag.ToString());
    return DummyEmptyBundle;
}

FItemBundle& UInventoryGridViewModel::GetMutableItemForTaggedSlot(const FGameplayTag& SlotTag)
{
     return GetMutableItemForTaggedSlotInternal(SlotTag); // Call private helper
}

FItemBundle& UInventoryGridViewModel::GetMutableItemForTaggedSlotInternal(const FGameplayTag& SlotTag)
{
    // This one is non-const and used internally or when mutation is explicitly needed
     if (!LinkedInventoryComponent) {
         UE_LOG(LogRISInventory, Error, TEXT("GetMutableItemForTaggedSlotInternal: Not an inventory component. Returning dummy.") );
         return DummyEmptyBundle;
     }
    FItemBundle* Found = ViewableTaggedSlots.Find(SlotTag);
    if (Found)
    {
        return *Found;
    }

    UE_LOG(LogRISInventory, Error, TEXT("GetMutableItemForTaggedSlotInternal: Critical error: SlotTag %s not found visually. Adding dummy."), *SlotTag.ToString());
    // Add a dummy entry to avoid returning a reference to a temporary.
    return ViewableTaggedSlots.Add(SlotTag, FItemBundle::EmptyItemInstance);
}

// In InventoryGridViewModel.cpp

int32 UInventoryGridViewModel::UseItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex)
{
    if (!LinkedContainerComponent) return 0;

    const bool bSourceIsGrid = !SourceTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(SourceSlotIndex);
    const bool bSourceIsTag = SourceTaggedSlot.IsValid();

    FItemBundle* SourceItemPtr = nullptr;
    ERISSlotOperation ExpectedOperationType;

    if (bSourceIsGrid) {
        SourceItemPtr = &ViewableGridSlots[SourceSlotIndex];
        ExpectedOperationType = ERISSlotOperation::Remove;
    } else if (bSourceIsTag) {
        if (!LinkedInventoryComponent) return 0;
        SourceItemPtr = ViewableTaggedSlots.Find(SourceTaggedSlot);
        ExpectedOperationType = ERISSlotOperation::RemoveTagged;
    } else {
        return 0;
    }

    if (!SourceItemPtr || !SourceItemPtr->IsValid()) return 0;

    FItemBundle& SourceItem = *SourceItemPtr;
    const FGameplayTag ItemIdToUse = SourceItem.ItemId;
    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemIdToUse);

    if (!ItemData) return 0;

    const UUsableItemDefinition* UsableDef = ItemData->GetItemDefinition<UUsableItemDefinition>();
    int32 QuantityToConsume = UsableDef ? UsableDef->QuantityPerUse : 0;

    if (SourceItem.Quantity < QuantityToConsume && QuantityToConsume > 0) return 0; // Check if quantity is enough only if consuming
    // If QuantityToConsume is 0, the item might be usable without consumption

    if (QuantityToConsume > 1 && SourceItem.InstanceData.Num() > 0) {
        UE_LOG(LogRISInventory, Error, TEXT("Using item '%s' with consume count > 1 and instance data is not currently supported."), *ItemIdToUse.ToString());
        return 0;
    }

    int32 UniqueInstanceIdToUse = -1;
    FItemBundle OriginalSourceItemForOpConfirm = SourceItem; // Copy for op confirm before modification

    if (QuantityToConsume > 0) { // Only modify state if items are actually consumed
        if (SourceItem.InstanceData.Num() > 0) {
            UniqueInstanceIdToUse = SourceItem.InstanceData.Last()->UniqueInstanceId; // Get ID before pop
            SourceItem.InstanceData.Pop(EAllowShrinking::No);
            SourceItem.InstanceData.Shrink();
        }
        SourceItem.Quantity -= QuantityToConsume;
        OperationsToConfirm.Emplace(FRISExpectedOperation(ExpectedOperationType, SourceTaggedSlot, ItemIdToUse, QuantityToConsume));
    }
    // If QuantityToConsume is 0, we still proceed to call UseItem/UseItemFromTaggedSlot
    // but don't modify ViewModel state or add to OperationsToConfirm for removal.

    int32 ActualConsumed = 0;
    if (bSourceIsGrid) {
        ActualConsumed = LinkedContainerComponent->UseItem(ItemIdToUse, UniqueInstanceIdToUse);
    } else { // bSourceIsTag
        ActualConsumed = LinkedInventoryComponent->UseItemFromTaggedSlot(SourceTaggedSlot, UniqueInstanceIdToUse);
    }

    // Update ViewModel and broadcast only if state actually changed due to consumption
    // Or if the operation itself (even non-consuming) is expected to trigger a UI update.
    // For simplicity, we update if consumption happened, or if the call to UseItem might have other side effects
    // that the server will replicate (though pure "use" without consumption often doesn't change inventory state directly).
    if (QuantityToConsume > 0) { // If we predicted consumption
        if (ActualConsumed > 0 || ActualConsumed == QuantityToConsume ) { // Server confirmed consumption (or at least some)
            // ViewModel state already updated predictively
            if (SourceItem.Quantity <= 0 && QuantityToConsume > 0) { // Double check if consumed item made quantity zero
                SourceItem = FItemBundle::EmptyItemInstance;
            }
        } else { // Server rejected consumption or consumed 0 when we expected >0
            // Revert predictive changes if server consumed 0 but we predicted >0
            SourceItem = OriginalSourceItemForOpConfirm; // Restore original state
            for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i) { // Remove the pending op
                 const FRISExpectedOperation& Op = OperationsToConfirm[i];
                 if (Op.Operation == ExpectedOperationType && Op.ItemId == ItemIdToUse && Op.Quantity == QuantityToConsume &&
                     ((bSourceIsGrid && !Op.TaggedSlot.IsValid()) || (bSourceIsTag && Op.TaggedSlot == SourceTaggedSlot))) {
                     OperationsToConfirm.RemoveAt(i);
                     break;
                 }
            }
            // No UI update broadcast needed if we reverted
            return ActualConsumed; // Return what server said
        }

        // Broadcast UI update if consumption was predicted and (at least partially) confirmed
        if (bSourceIsGrid) OnGridSlotUpdated.Broadcast(SourceSlotIndex);
        else OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
    }
    // If QuantityToConsume was 0, ActualConsumed should also likely be 0 unless UseItem can return other codes.
    // In this case, no ViewModel state change due to consumption, so no specific broadcast here.
    // Any UI changes for non-consuming uses would typically be driven by other game events or specific OnRep handlers.

    return ActualConsumed;
}

bool UInventoryGridViewModel::CanTaggedSlotReceiveItem_Implementation(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool FromInternal, bool AllowSwapback) const
{
    // Implementation moved from UInventoryGridViewModel::CanTaggedSlotReceiveItem_Implementation
    if (!LinkedInventoryComponent || !ItemId.IsValid() || !SlotTag.IsValid() || Quantity <= 0) return false;

    if (!FromInternal && LinkedInventoryComponent->GetQuantityContainerCanReceiveByWeight(URISSubsystem::GetItemDataById(ItemId)) < Quantity)
        return false; // Not enough space in container
    
    return LinkedInventoryComponent->CanReceiveItemInTaggedSlot(ItemId, Quantity, SlotTag, AllowSwapback);
    /*
    if (!LinkedInventoryComponent->CanReceiveItemInTaggedSlot(ItemId, Quantity, SlotTag)) return false;
    if (LinkedInventoryComponent->IsTaggedSlotBlocked(SlotTag)) return false;

    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
    if (LinkedInventoryComponent->ValidateReceiveItem(ItemData, Quantity, SlotTag, true) < Quantity)
    {
        return false;
    }

    const FItemBundle* TargetSlotItemPtr = ViewableTaggedSlots.Find(SlotTag);
    if (!TargetSlotItemPtr) return false;

    const FItemBundle& TargetSlotItem = *TargetSlotItemPtr;
    const bool bTargetSlotEmpty = !TargetSlotItem.IsValid();

    if (bTargetSlotEmpty) {
        return true;
    }
    else if (TargetSlotItem.ItemId == ItemId) {
        if (!ItemData) return false;
        if (ItemData->MaxStackSize <= 1) return false;
        return (TargetSlotItem.Quantity + Quantity) <= ItemData->MaxStackSize;
    }
    else {
        return false;
    }*/
}

// --- Combined Grid/Tagged Slot Functions ---

bool UInventoryGridViewModel::SplitItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity)
{
    return MoveItem_Internal(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, Quantity, true);
}

bool UInventoryGridViewModel::MoveItem_Implementation(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex)
{
    return MoveItem_Internal(SourceTaggedSlot, SourceSlotIndex, TargetTaggedSlot, TargetSlotIndex, 0, false);
}

bool UInventoryGridViewModel::MoveItemToAnyTaggedSlot_Implementation(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex)
{
    // Implementation moved from UInventoryGridViewModel::MoveItemToAnyTaggedSlot_Implementation
    if (!LinkedInventoryComponent) return false; // Requires inventory features

    const bool bSourceIsTag = SourceTaggedSlot.IsValid();
    const bool bSourceIsGrid = !bSourceIsTag && ViewableGridSlots.IsValidIndex(SourceSlotIndex);

    if (!bSourceIsTag && !bSourceIsGrid) return false;

    FGenericItemBundle SourceItem;
    if (bSourceIsTag) {
        SourceItem = &GetMutableItemForTaggedSlotInternal(SourceTaggedSlot); // Use internal helper
    } else {
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }

    if (!SourceItem.IsValid()) return false;

    const FGameplayTag TargetSlotTag = FindTaggedSlotForItem(SourceItem.GetItemId(), SourceItem.GetQuantity(), EPreferredSlotPolicy::PreferSpecializedTaggedSlot);

    if (!TargetSlotTag.IsValid()) {
        return false;
    }

    return MoveItem(SourceTaggedSlot, SourceSlotIndex, TargetSlotTag, -1);
}

bool UInventoryGridViewModel::MoveItemToOtherViewModel(
                    FGameplayTag SourceTaggedSlot,
                    int32 SourceSlotIndex,
                    UInventoryGridViewModel* TargetViewModel, // Use merged type
                    FGameplayTag TargetTaggedSlot,
                    int32 TargetGridSlotIndex,
                    int32 Quantity)
{
    // Implementation mostly from UContainerGridViewModel::MoveItemToOtherViewModel
    if (!TargetViewModel || !this->LinkedContainerComponent || !TargetViewModel->LinkedContainerComponent) return false;
    if (this == TargetViewModel) return false;

    const bool bSourceIsTag = SourceTaggedSlot.IsValid();
    const bool bSourceIsGrid = !bSourceIsTag && SourceSlotIndex >= 0 && SourceSlotIndex < this->NumberOfGridSlots;
    const bool bTargetIsTag = TargetTaggedSlot.IsValid();
    const bool bTargetIsGrid = !bTargetIsTag && TargetGridSlotIndex >= 0 && TargetGridSlotIndex < TargetViewModel->NumberOfGridSlots;

    if ((!bSourceIsGrid && !bSourceIsTag) || (!bTargetIsGrid && !bTargetIsTag)) return false;

    FItemBundle SourceItemCopy;
    TArray<UItemInstanceData*> SourceInstanceDataPtrs;

    if (bSourceIsTag)
    {
        if (!this->LinkedInventoryComponent) return false; // Source Tag requires source to be Inventory
        SourceItemCopy = this->GetItemForTaggedSlot(SourceTaggedSlot); // Use const getter for copy
        SourceInstanceDataPtrs = SourceItemCopy.InstanceData;
    }
    else // Source is Grid
    {
        SourceItemCopy = this->GetGridItem(SourceSlotIndex);
        SourceInstanceDataPtrs = SourceItemCopy.InstanceData;
    }

    if (!SourceItemCopy.IsValid()) return false;

    const FGameplayTag ItemIdToMove = SourceItemCopy.ItemId;
    int32 QuantityToMove = (Quantity < 0) ? SourceItemCopy.Quantity : FMath::Min(Quantity, SourceItemCopy.Quantity);
    if (QuantityToMove <= 0) return false;

    auto* ItemData = URISSubsystem::GetItemDataById(ItemIdToMove);

    if (bTargetIsTag)
    {
        if (!TargetViewModel->LinkedInventoryComponent)
        {
            UE_LOG(LogRISInventory, Warning, TEXT("MoveItemToOtherViewModel: Target tagged slot %s requires target to be an inventory."), *TargetTaggedSlot.ToString());
            return false; // Target Tag requires target to be Inventory
        }
        QuantityToMove = TargetViewModel->CanTaggedSlotReceiveItem(ItemIdToMove, QuantityToMove, TargetTaggedSlot, false, true) ? QuantityToMove : 0;
    }
    else
        QuantityToMove = TargetViewModel->CanGridSlotReceiveItem(ItemIdToMove, QuantityToMove, TargetGridSlotIndex) ? QuantityToMove : 0;
    
    if (QuantityToMove <= 0) return false;
    
    TArray<UItemInstanceData*> InstancesToMove = SourceItemCopy.GetInstancesFromEnd(QuantityToMove);
    if (!InstancesToMove.IsEmpty() && InstancesToMove.Num() != QuantityToMove)
    {
         QuantityToMove = InstancesToMove.Num();
         if (QuantityToMove <= 0) return false;
    }

    // --- Client-Side Visual Prediction ---
    if (bSourceIsTag)
    {
         FItemBundle& ActualSourceItem = this->GetMutableItemForTaggedSlotInternal(SourceTaggedSlot); // Use internal helper
         ActualSourceItem.Quantity -= QuantityToMove;
         if (ActualSourceItem.Quantity <= 0)
         {
             ActualSourceItem = FItemBundle::EmptyItemInstance;
         } else if (!InstancesToMove.IsEmpty()) {
             ActualSourceItem.InstanceData.RemoveAll([&InstancesToMove](UItemInstanceData* Inst){ return InstancesToMove.Contains(Inst); });
         }
         this->OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot);
    }
    else // Source is Grid
    {
        this->ViewableGridSlots[SourceSlotIndex].Quantity -= QuantityToMove;
        if (this->ViewableGridSlots[SourceSlotIndex].Quantity <= 0)
        {
             this->ViewableGridSlots[SourceSlotIndex] = FItemBundle::EmptyItemInstance;
        } else if (!InstancesToMove.IsEmpty()) {
             this->ViewableGridSlots[SourceSlotIndex].InstanceData.RemoveAll([&InstancesToMove](UItemInstanceData* Inst){ return InstancesToMove.Contains(Inst); });
        }
        this->OnGridSlotUpdated.Broadcast(SourceSlotIndex);
    }

    if (bTargetIsTag)
    {
        if (!TargetViewModel->LinkedInventoryComponent) return false; // Target Tag requires target to be Inventory
        FItemBundle& ActualTargetItem = TargetViewModel->GetMutableItemForTaggedSlotInternal(TargetTaggedSlot); // Use internal helper
        ActualTargetItem.Quantity += QuantityToMove;
        if (ActualTargetItem.ItemId != ItemIdToMove)
        {
            ActualTargetItem = FItemBundle(ItemIdToMove, QuantityToMove, InstancesToMove);
        }
        TargetViewModel->OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot);
    }
    else // Target is Grid
    {
        TargetViewModel->ViewableGridSlots[TargetGridSlotIndex].Quantity += QuantityToMove;
        if (TargetViewModel->ViewableGridSlots[TargetGridSlotIndex].ItemId != ItemIdToMove)
        {
            TargetViewModel->ViewableGridSlots[TargetGridSlotIndex] = FItemBundle(ItemIdToMove, QuantityToMove, InstancesToMove);
        }
        TargetViewModel->OnGridSlotUpdated.Broadcast(TargetGridSlotIndex);
    }

    ERISSlotOperation RemoveOp = bSourceIsTag ? RemoveTagged : Remove;
    this->OperationsToConfirm.Emplace(FRISExpectedOperation(RemoveOp, SourceTaggedSlot, ItemIdToMove, QuantityToMove));

    ERISSlotOperation AddOp = bTargetIsTag ? AddTagged : Add;
    TargetViewModel->OperationsToConfirm.Emplace(FRISExpectedOperation(AddOp, TargetTaggedSlot, ItemIdToMove, QuantityToMove));

    // --- Send Server Request ---
    UItemContainerComponent* SourceComponent = this->LinkedContainerComponent;
    UItemContainerComponent* TargetComponent = TargetViewModel->LinkedContainerComponent;

    if (SourceComponent)
    {
        // RequestMoveItemToOtherContainer_Server needs to be implemented on UItemContainerComponent
        SourceComponent->RequestMoveItemToOtherContainer(TargetComponent, ItemIdToMove, QuantityToMove, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot);
        return true;
    }

    // Rollback prediction if needed
    this->OperationsToConfirm.Pop();
    TargetViewModel->OperationsToConfirm.Pop();
    return false;
}

void UInventoryGridViewModel::PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots, bool DestroyAfterPickup)
{
    // Implementation moved from UInventoryGridViewModel::PickupItem
    if (!WorldItem || !LinkedContainerComponent) return;

    if(LinkedInventoryComponent) // Use Inventory Component's pickup if available
    {
        LinkedInventoryComponent->PickupItem(WorldItem, PreferTaggedSlots, DestroyAfterPickup);
    }
    else // Fallback to basic container AddItem if not a full inventory
    {
        const FItemBundle& ItemToPickup = WorldItem->RepresentedItem;
        if (!ItemToPickup.IsValid()) return;

        auto* ItemData = URISSubsystem::GetItemDataById(ItemToPickup.ItemId);
        int32 ReceivableQty = LinkedContainerComponent->GetReceivableQuantity(ItemData);
        int32 QuantityToPickup = FMath::Min(ItemToPickup.Quantity, ReceivableQty);
        if (QuantityToPickup <= 0) return;

        OperationsToConfirm.Emplace(FRISExpectedOperation(Add, ItemToPickup.ItemId, QuantityToPickup));
        int32 AddedQty = LinkedContainerComponent->AddItem_IfServer(WorldItem, ItemToPickup.ItemId, QuantityToPickup, true);

        // Basic grid visual update prediction (can be simplified if HandleItemAdded handles it)
        if(AddedQty > 0) {
             // Find first available grid slot visually and add there
             int32 TargetSlot = FindGridSlotIndexForItem(ItemToPickup.ItemId, AddedQty);
             if (TargetSlot != -1) {
                 FItemBundle& Slot = ViewableGridSlots[TargetSlot];
                 if (!Slot.IsValid()) {
                     Slot.ItemId = ItemToPickup.ItemId;
                     Slot.Quantity = 0;
                     Slot.InstanceData = TArray<UItemInstanceData*>(); // Assume instances are handled by HandleItemAdded later
                 }
                 Slot.Quantity += AddedQty;
                 OnGridSlotUpdated.Broadcast(TargetSlot);
             }
             // If server interaction succeeded, and DestroyAfterPickup is true, destroy the world item.
             if (DestroyAfterPickup && WorldItem && WorldItem->GetQuantityTotal_Implementation(ItemToPickup.ItemId) <= 0) // Check if source is empty
             {
                 WorldItem->Destroy();
             }
        } else {
            // Remove pending op if AddItem failed immediately
            for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i) {
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
}

// --- State & Helpers ---

bool UInventoryGridViewModel::AssertViewModelSettled() const
{
    // Implementation moved from UInventoryGridViewModel::AssertViewModelSettled
    bool bOpsSettled = OperationsToConfirm.IsEmpty();
    ensureMsgf(bOpsSettled, TEXT("ViewModel is not settled. %d operations pending."), OperationsToConfirm.Num());
    if (!bOpsSettled)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("ViewModel pending ops: %d"), OperationsToConfirm.Num());
        for(const auto& Op : OperationsToConfirm) {
             if(Op.TaggedSlot.IsValid()) {
                  UE_LOG(LogRISInventory, Warning, TEXT("  - Pending Tagged Op: %d for %s on %s (Qty: %d)"), (int)Op.Operation, *Op.ItemId.ToString(), *Op.TaggedSlot.ToString(), Op.Quantity);
             } else {
                  UE_LOG(LogRISInventory, Warning, TEXT("  - Pending Grid Op: %d for %s (Qty: %d)"), (int)Op.Operation, *Op.ItemId.ToString(), Op.Quantity);
             }
        }
    }

    bool bQuantitiesMatch = true;
    bool bTaggedConsistency = true;

    if (LinkedContainerComponent)
    {
        TMap<FGameplayTag, int32> ComponentTotalQuantities;
        TMap<FGameplayTag, int32> ViewModelTotalQuantities;

        // Get Component Totals
        for (const auto& Item : LinkedContainerComponent->GetAllItems())
        {
            if (Item.Quantity > 0 && Item.ItemId.IsValid()) {
                 ComponentTotalQuantities.FindOrAdd(Item.ItemId) += Item.Quantity;
            }
        }

        // Get ViewModel Totals (Grid)
        for (const FItemBundle& Slot : ViewableGridSlots)
        {
            if (Slot.IsValid())
            {
                ViewModelTotalQuantities.FindOrAdd(Slot.ItemId) += Slot.Quantity;
            }
        }
        // Get ViewModel Totals (Tagged - only if Inventory)
        if(LinkedInventoryComponent)
        {
            for (const auto& Pair : ViewableTaggedSlots)
            {
                if (Pair.Value.IsValid())
                {
                    ViewModelTotalQuantities.FindOrAdd(Pair.Value.ItemId) += Pair.Value.Quantity;
                }
            }
        }

        // Compare Totals
        TSet<FGameplayTag> AllItemIds;
        ComponentTotalQuantities.GetKeys(AllItemIds);
        ViewModelTotalQuantities.GetKeys(AllItemIds);

        for(const FGameplayTag& ItemId : AllItemIds)
        {
            int32 CompQty = ComponentTotalQuantities.FindRef(ItemId);
            int32 VmQty = ViewModelTotalQuantities.FindRef(ItemId);
            if(CompQty != VmQty)
            {
                bQuantitiesMatch = false;
                ensureMsgf(false, TEXT("Total Quantity mismatch for %s. Component: %d, ViewModel(Grid+Tagged): %d"), *ItemId.ToString(), CompQty, VmQty);
                UE_LOG(LogRISInventory, Warning, TEXT("Total Quantity mismatch for %s. Component: %d, ViewModel(Grid+Tagged): %d"), *ItemId.ToString(), CompQty, VmQty);
            }
        }
         ensureMsgf(bQuantitiesMatch, TEXT("ViewModel total quantities (Grid+Tagged) do not match LinkedComponent totals."));
          if (!bQuantitiesMatch) UE_LOG(LogRISInventory, Warning, TEXT("ViewModel total quantity mismatch."));


        // Check Tagged Slot Consistency (Only if Inventory)
        if (LinkedInventoryComponent)
        {
             TMap<FGameplayTag, FTaggedItemBundle> ActualTaggedItemsMap;
             for(const auto& Item : LinkedInventoryComponent->GetAllTaggedItems()) {
                  if (Item.Tag.IsValid()) ActualTaggedItemsMap.Add(Item.Tag, Item);
             }

             TSet<FGameplayTag> AllTags;
             ViewableTaggedSlots.GetKeys(AllTags);
             ActualTaggedItemsMap.GetKeys(AllTags);

             for (const FGameplayTag& Tag : AllTags)
             {
                  const FItemBundle* VmItemPtr = ViewableTaggedSlots.Find(Tag);
                  const FTaggedItemBundle* ActualItem = ActualTaggedItemsMap.Find(Tag);
                  bool bVmValid = VmItemPtr && VmItemPtr->IsValid();
                  bool bActualValid = ActualItem && ActualItem->IsValid();

                  if (bVmValid != bActualValid) {
                       bTaggedConsistency = false;
                       // Log specific error message
                       ensureMsgf(false, TEXT("Tagged slot validity mismatch for %s: VMValid=%d, ActualValid=%d"), *Tag.ToString(), bVmValid, bActualValid);
                       UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot validity mismatch for %s: VMValid=%d, ActualValid=%d"), *Tag.ToString(), bVmValid, bActualValid);
                  } else if (bVmValid && bActualValid) { // Both valid, compare contents
                       if (ActualItem->ItemId != VmItemPtr->ItemId || ActualItem->Quantity != VmItemPtr->Quantity) {
                            bTaggedConsistency = false;
                            // Log specific error message
                            ensureMsgf(false, TEXT("Tagged slot content mismatch for %s: VM=%s(x%d), Actual=%s(x%d)"), *Tag.ToString(), *VmItemPtr->ItemId.ToString(), VmItemPtr->Quantity, *ActualItem->ItemId.ToString(), ActualItem->Quantity);
                            UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot content mismatch for %s: VM=%s(x%d), Actual=%s(x%d)"), *Tag.ToString(), *VmItemPtr->ItemId.ToString(), VmItemPtr->Quantity, *ActualItem->ItemId.ToString(), ActualItem->Quantity);
                       }
                       // Instance Data Pointer Check (if needed)
                       // if (ActualItem->InstanceData != VmItemPtr->InstanceData) { ... } // Be careful comparing TArrays directly like this
                       if (ActualItem->InstanceData.Num() != VmItemPtr->InstanceData.Num()) {
                           bTaggedConsistency = false;
                           ensureMsgf(false, TEXT("Tagged slot instance count mismatch for %s: VM=%d, Actual=%d"), *Tag.ToString(), VmItemPtr->InstanceData.Num(), ActualItem->InstanceData.Num());
                           UE_LOG(LogRISInventory, Warning, TEXT("Tagged slot instance count mismatch for %s: VM=%d, Actual=%d"), *Tag.ToString(), VmItemPtr->InstanceData.Num(), ActualItem->InstanceData.Num());
                       } else {
                            // Optionally, compare actual instance pointers if order matters
                       }
                  }
             }
              ensureMsgf(bTaggedConsistency, TEXT("ViewModel tagged slots do not match LinkedInventoryComponent state."));
               if (!bTaggedConsistency) UE_LOG(LogRISInventory, Warning, TEXT("ViewModel tagged slot state mismatch."));
        }
    }
    else
    {
        UE_LOG(LogRISInventory, Warning, TEXT("AssertViewModelSettled: LinkedContainerComponent is null. Cannot verify quantities."));
        bQuantitiesMatch = false;
        bTaggedConsistency = false;
    }

    return bOpsSettled && bQuantitiesMatch && bTaggedConsistency;
}


int32 UInventoryGridViewModel::FindGridSlotIndexForItem_Implementation(const FGameplayTag& ItemId, int32 Quantity)
{
    // Implementation moved from UContainerGridViewModel::FindGridSlotIndexForItem_Implementation
    if (!ItemId.IsValid()) return -1;

    const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
    if (!ItemData) return -1;

    int32 FirstEmptySlot = -1;

    // Pass 1: Find existing partial stack
    if (ItemData->MaxStackSize > 1) {
        for (int32 Index = 0; Index < ViewableGridSlots.Num(); ++Index) {
            const FItemBundle& ExistingItem = ViewableGridSlots[Index];
            if (ExistingItem.IsValid() && ExistingItem.ItemId == ItemId && ExistingItem.Quantity < ItemData->MaxStackSize) {
                return Index; // Found first partial stack
            }
        }
    }

    // Pass 2: Find first empty slot
    for (int32 Index = 0; Index < ViewableGridSlots.Num(); ++Index) {
        if (!ViewableGridSlots[Index].IsValid()) {
            return Index; // Found first empty slot
        }
    }

    return -1; // No suitable slot found
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
        if (LinkedInventoryComponent->GetReceivableQuantityForTaggedSlot(ItemData, SlotTag, Quantity, true, true) == ItemData->MaxStackSize)
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
        if (LinkedInventoryComponent->GetReceivableQuantityForTaggedSlot(ItemData, SlotTag, Quantity, true, SlotPolicy > EPreferredSlotPolicy::PreferGenericInventory) > 0) // Check blocking & compatibility
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

// --- Event Handlers ---

void UInventoryGridViewModel::HandleItemAdded_Implementation(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesAdded, EItemChangeReason Reason)
{
    // Implementation mostly from UContainerGridViewModel::HandleItemAdded_Implementation
    if (!ItemData || Quantity <= 0) return;

    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == ERISSlotOperation::Add &&
            !OperationsToConfirm[i].TaggedSlot.IsValid() && // Grid only
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
            // Verify visual state if needed, otherwise assume prediction was correct
            return;
        }
    }

    UE_LOG(LogRISInventory, Log, TEXT("HandleItemAdded: Received unpredicted add for %s x%d. Updating visuals."), *ItemData->ItemId.ToString(), Quantity);
    int32 RemainingItems = Quantity;
    int32 InstanceIdx = 0; // Track index for added instances
    while (RemainingItems > 0)
    {
        int32 SlotIndex = FindGridSlotIndexForItem(ItemData->ItemId, RemainingItems);
        if (SlotIndex < 0)
        {
             UE_LOG(LogRISInventory, Error, TEXT("HandleItemAdded: No available visual slot found for server-added item %s."), *ItemData->ItemId.ToString());
             ForceFullUpdate(); // Resync if visual state seems wrong
             break;
        }

        FItemBundle& TargetSlot = ViewableGridSlots[SlotIndex];
        int32 AddableQuantity = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;
         if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId) {
             AddableQuantity -= TargetSlot.Quantity;
         } else if (!TargetSlot.IsValid()) {
             TargetSlot.ItemId = ItemData->ItemId;
             TargetSlot.Quantity = 0;
             TargetSlot.InstanceData.Empty();
         } else {
             UE_LOG(LogRISInventory, Error, TEXT("HandleItemAdded: FindGridSlotIndexForItem returned incompatible slot %d."), SlotIndex);
             ForceFullUpdate(); // Resync
             break;
         }

        int32 ActuallyAddedToSlot = FMath::Min(RemainingItems, AddableQuantity);
        if (ActuallyAddedToSlot <= 0) {
              UE_LOG(LogRISInventory, Warning, TEXT("HandleItemAdded: Could not add to found slot %d (already full?). Forcing full update."), SlotIndex);
              ForceFullUpdate();
              break;
        }

        TargetSlot.Quantity += ActuallyAddedToSlot;
        // Append instance data for the amount added to this slot
        if (InstanceIdx < InstancesAdded.Num()) {
            int32 NumInstancesToAdd = FMath::Min(ActuallyAddedToSlot, InstancesAdded.Num() - InstanceIdx);
            for(int32 k=0; k<NumInstancesToAdd; ++k) {
                TargetSlot.InstanceData.Add(InstancesAdded[InstanceIdx++]);
            }
        }

        RemainingItems -= ActuallyAddedToSlot;
        OnGridSlotUpdated.Broadcast(SlotIndex);
    }
}

void UInventoryGridViewModel::HandleItemRemoved_Implementation(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason)
{
    // Implementation mostly from UContainerGridViewModel::HandleItemRemoved_Implementation
    if (!ItemData || Quantity <= 0) return;

    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == Remove &&
            !OperationsToConfirm[i].TaggedSlot.IsValid() && // Grid only
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
            // Verify visual state if needed
            return;
        }
    }

    UE_LOG(LogRISInventory, Log, TEXT("HandleItemRemoved: Received unpredicted remove for %s x%d. Updating visuals."), *ItemData->ItemId.ToString(), Quantity);
    int32 RemainingToRemove = Quantity;
    for (int32 SlotIndex = 0; SlotIndex < ViewableGridSlots.Num() && RemainingToRemove > 0; ++SlotIndex)
    {
        FItemBundle& CurrentSlot = ViewableGridSlots[SlotIndex];
        if (CurrentSlot.IsValid() && CurrentSlot.ItemId == ItemData->ItemId)
        {
            if (!InstancesRemoved.IsEmpty())
            {
                int32 RemovedCount = CurrentSlot.InstanceData.RemoveAll([&InstancesRemoved](UItemInstanceData* Instance) {
                    return InstancesRemoved.Contains(Instance);
                });
                int32 OldQuantity = CurrentSlot.Quantity;
                CurrentSlot.Quantity = CurrentSlot.InstanceData.Num(); // Sync quantity with instances

                if (RemovedCount > 0) {
                    RemainingToRemove -= RemovedCount;
                    if (CurrentSlot.Quantity <= 0) CurrentSlot = FItemBundle::EmptyItemInstance;
                    OnGridSlotUpdated.Broadcast(SlotIndex);
                }
            }
            else // Remove by quantity
            {
                int32 CanRemoveFromSlot = FMath::Min(RemainingToRemove, CurrentSlot.Quantity);
                if (CanRemoveFromSlot > 0)
                {
                    CurrentSlot.Quantity -= CanRemoveFromSlot;
                    RemainingToRemove -= CanRemoveFromSlot;
                    // Remove instances from the end if removing by quantity
                    if(CurrentSlot.InstanceData.Num() > 0) {
                        int32 InstToRemove = FMath::Min(CanRemoveFromSlot, CurrentSlot.InstanceData.Num()); // Should match CanRemoveFromSlot unless data is inconsistent
                        for(int32 k=0; k<InstToRemove; ++k) CurrentSlot.InstanceData.Pop(EAllowShrinking::No);
                        CurrentSlot.InstanceData.Shrink();
                    }

                    if (CurrentSlot.Quantity <= 0)
                        CurrentSlot = FItemBundle::EmptyItemInstance;

                    OnGridSlotUpdated.Broadcast(SlotIndex);
                }
            }
        }
    }

    if (RemainingToRemove > 0)
    {
        UE_LOG(LogRISInventory, Error, TEXT("HandleItemRemoved: Could not remove %d items of type %s visually from grid. Forcing full update."), RemainingToRemove, *ItemData->ItemId.ToString());
        ForceFullUpdate();
    }
}

void UInventoryGridViewModel::HandleTaggedItemAdded_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& AddedInstances, FTaggedItemBundle PreviousItem, EItemChangeReason Reason)
{
    // Implementation moved from UInventoryGridViewModel::HandleTaggedItemAdded_Implementation
    if (!LinkedInventoryComponent || !ItemData || Quantity <= 0 || !SlotTag.IsValid()) return; // Added LinkedInventoryComponent check

    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == ERISSlotOperation::AddTagged &&
            OperationsToConfirm[i].TaggedSlot == SlotTag &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
             if (ViewableTaggedSlots.Contains(SlotTag)) {
                 FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag); // Get real state
                 auto& ViewableItem = ViewableTaggedSlots[SlotTag];
                 if (ActualItem.IsValid()) {
                     if(!ViewableItem.IsValid() || ViewableItem.ItemId != ActualItem.ItemId || ViewableItem.Quantity != ActualItem.Quantity || ViewableItem.InstanceData.Num() != ActualItem.InstanceData.Num()) {
                          UE_LOG(LogRISInventory, Log, TEXT("Correcting visual tag %s after confirmed add."), *SlotTag.ToString());
                          ViewableItem.ItemId = ActualItem.ItemId;
                          ViewableItem.Quantity = ActualItem.Quantity;
                          ViewableItem.InstanceData = ActualItem.InstanceData;
                          // Potentially rebroadcast if state was wrong? OnTaggedSlotUpdated.Broadcast(SlotTag);
                     }
                 } else if (ViewableItem.IsValid()) { // Server says empty, VM has item?
                      UE_LOG(LogRISInventory, Warning, TEXT("Mismatch after confirming AddTagged for tag %s (server empty). Forcing slot update."), *SlotTag.ToString());
                       ViewableItem = FItemBundle::EmptyItemInstance;
                       OnTaggedSlotUpdated.Broadcast(SlotTag);
                 }
             }
            return;
        }
    }

    UE_LOG(LogRISInventory, Verbose, TEXT("HandleTaggedItemAdded: Received unpredicted add for %s x%d to tag %s. Updating viewmodel."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FItemBundle& TargetSlot = GetMutableItemForTaggedSlotInternal(SlotTag);
        FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag); // Get source of truth
        if (ActualItem.IsValid()) {
            TargetSlot.ItemId = ActualItem.ItemId;
            TargetSlot.Quantity = ActualItem.Quantity;
            TargetSlot.InstanceData = ActualItem.InstanceData;
            OnTaggedSlotUpdated.Broadcast(SlotTag);
        } else {
             UE_LOG(LogRISInventory, Warning, TEXT("HandleTaggedItemAdded: Component reported add but tag %s is empty in component state?"), *SlotTag.ToString());
             // Maybe clear the visual slot if it wasn't already?
             if(TargetSlot.IsValid()) {
                 TargetSlot = FItemBundle::EmptyItemInstance;
                 OnTaggedSlotUpdated.Broadcast(SlotTag);
             }
        }
    }
    else
    {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemAdded: Critical Error: Received add for unmanaged tag %s!"), *SlotTag.ToString());
    }
}

void UInventoryGridViewModel::HandleTaggedItemRemoved_Implementation(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason)
{
    // Implementation moved from UInventoryGridViewModel::HandleTaggedItemRemoved_Implementation
    if (!LinkedInventoryComponent || !ItemData || Quantity <= 0 || !SlotTag.IsValid()) return; // Added LinkedInventoryComponent check

    for (int32 i = OperationsToConfirm.Num() - 1; i >= 0; --i)
    {
        if (OperationsToConfirm[i].Operation == RemoveTagged &&
            OperationsToConfirm[i].TaggedSlot == SlotTag &&
            OperationsToConfirm[i].ItemId == ItemData->ItemId &&
            OperationsToConfirm[i].Quantity == Quantity)
        {
            OperationsToConfirm.RemoveAt(i);
             if (ViewableTaggedSlots.Contains(SlotTag)) {
                 FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag); // Get real state
                 auto& ViewableItem = ViewableTaggedSlots[SlotTag];
                  if (ActualItem.IsValid()) {
                     if(!ViewableItem.IsValid() || ViewableItem.ItemId != ActualItem.ItemId || ViewableItem.Quantity != ActualItem.Quantity || ViewableItem.InstanceData.Num() != ActualItem.InstanceData.Num()) {
                          UE_LOG(LogRISInventory, Log, TEXT("Correcting visual tag %s after confirmed remove (item still present)."), *SlotTag.ToString());
                          ViewableItem.ItemId = ActualItem.ItemId;
                          ViewableItem.Quantity = ActualItem.Quantity;
                          ViewableItem.InstanceData = ActualItem.InstanceData;
                     }
                 } else if (ViewableItem.IsValid()) { // Server says empty, VM has item
                      UE_LOG(LogRISInventory, Log, TEXT("Correcting visual tag %s after confirmed remove (now empty)."), *SlotTag.ToString());
                       ViewableItem = FItemBundle::EmptyItemInstance;
                 }
                 // If both are empty, visual state matches, do nothing.
             }
            return;
        }
    }

    UE_LOG(LogRISInventory, Verbose, TEXT("HandleTaggedItemRemoved: Received unpredicted remove for %s x%d from tag %s. Updating visuals."), *ItemData->ItemId.ToString(), Quantity, *SlotTag.ToString());
    if (ViewableTaggedSlots.Contains(SlotTag))
    {
        FItemBundle& TargetSlot = GetMutableItemForTaggedSlotInternal(SlotTag);
        if (TargetSlot.IsValid() && TargetSlot.ItemId == ItemData->ItemId)
        {
            FTaggedItemBundle ActualItem = LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag); // Get source of truth
            if (ActualItem.IsValid()) {
                TargetSlot.ItemId = ActualItem.ItemId;
                TargetSlot.Quantity = ActualItem.Quantity;
                TargetSlot.InstanceData = ActualItem.InstanceData;
            } else {
                TargetSlot = FItemBundle::EmptyItemInstance; // Reset
            }
            OnTaggedSlotUpdated.Broadcast(SlotTag);
        }
        else if (TargetSlot.IsValid() && TargetSlot.ItemId != ItemData->ItemId)
        {
             UE_LOG(LogRISInventory, Warning, TEXT("HandleTaggedItemRemoved: Server removed %s from tag %s, but VM shows %s. Forcing full update."), *ItemData->ItemId.ToString(), *SlotTag.ToString(), *TargetSlot.ItemId.ToString());
             ForceFullUpdate();
        }
    }
     else {
         UE_LOG(LogRISInventory, Error, TEXT("HandleTaggedItemRemoved: Received remove for unmanaged tag %s!"), *SlotTag.ToString());
     }
}

// --- Internal Helper Functions ---

bool UInventoryGridViewModel::TryUnblockingMove(FGameplayTag TargetTaggedSlot, FGameplayTag ItemId)
{
    // Implementation moved from UInventoryGridViewModel::TryUnblockingMove
    if (!LinkedInventoryComponent) return false;

     bool bUnblocked = false;
     const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
     if(!ItemData) return false;

     if (auto* BlockingInfo = LinkedInventoryComponent->WouldItemMoveIndirectlyViolateBlocking(TargetTaggedSlot, ItemData))
     {
          FGameplayTag SlotToClear = BlockingInfo->UniversalSlotToBlock;
          FItemBundle BlockingItem = GetItemForTaggedSlot(SlotToClear); // Use const getter

          if (BlockingItem.IsValid())
          {
               int32 TargetGridIndex = FindGridSlotIndexForItem(BlockingItem.ItemId, BlockingItem.Quantity);
               if (TargetGridIndex != -1 && IsGridSlotEmpty(TargetGridIndex))
               {
                   UE_LOG(LogRISInventory, Log, TEXT("TryUnblockingMove: Attempting to move blocking item %s from slot %s to grid slot %d."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString(), TargetGridIndex);
                   // Call internal move, passing full quantity (0 means full move)
                   bUnblocked = MoveItem_Internal(SlotToClear, -1, FGameplayTag(), TargetGridIndex, 0, false);
                   if (!bUnblocked) {
                        UE_LOG(LogRISInventory, Warning, TEXT("TryUnblockingMove: Failed to move blocking item %s from %s."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString());
                   }
               } else {
                    UE_LOG(LogRISInventory, Warning, TEXT("TryUnblockingMove: No empty grid slot found for blocking item %s from slot %s."), *BlockingItem.ItemId.ToString(), *SlotToClear.ToString());
               }
          }
     }
     return bUnblocked;
}

bool UInventoryGridViewModel::MoveItem_Internal(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit)
{
    // Implementation moved from UInventoryGridViewModel::MoveItem_Internal
    if (!LinkedContainerComponent) return false;

    const bool bSourceIsGrid = !SourceTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(SourceSlotIndex);
    const bool bSourceIsTag = SourceTaggedSlot.IsValid() && (!LinkedInventoryComponent || ViewableTaggedSlots.Contains(SourceTaggedSlot)); // Source tag requires inventory
    const bool bTargetIsGrid = !TargetTaggedSlot.IsValid() && ViewableGridSlots.IsValidIndex(TargetSlotIndex);
    const bool bTargetIsTag = TargetTaggedSlot.IsValid() && (!LinkedInventoryComponent || ViewableTaggedSlots.Contains(TargetTaggedSlot)); // Target tag requires inventory

    // Basic validation
    if ((!bSourceIsGrid && !bSourceIsTag) || (!bTargetIsGrid && !bTargetIsTag)) return false;
    if (bSourceIsGrid && bTargetIsGrid && SourceSlotIndex == TargetSlotIndex) return false;
    if (bSourceIsTag && bTargetIsTag && SourceTaggedSlot == TargetTaggedSlot) return false;
    // If tagged slots are involved, ensure we have an Inventory Component
    if ((bSourceIsTag || bTargetIsTag) && !LinkedInventoryComponent) {
         UE_LOG(LogRISInventory, Error, TEXT("MoveItem_Internal: Tagged slot operation attempted on a non-Inventory ViewModel."));
         return false;
    }


    FItemBundle* SourceItem = nullptr;
    if (bSourceIsTag) {
        SourceItem = &GetMutableItemForTaggedSlotInternal(SourceTaggedSlot);
    } else {
        SourceItem = &ViewableGridSlots[SourceSlotIndex];
    }
    if (!SourceItem || !SourceItem->IsValid()) return false;

    const FGameplayTag ItemIdToMove = SourceItem->ItemId;
    int32 MaxSourceQty = SourceItem->Quantity;
    int32 RequestedQuantity = IsSplit ? InQuantity : MaxSourceQty;

    if (RequestedQuantity <= 0) return false;
    if (IsSplit && RequestedQuantity > MaxSourceQty) return false;

    FItemBundle* TargetItem  = nullptr;
     if (bTargetIsTag) {
         TargetItem = &GetMutableItemForTaggedSlotInternal(TargetTaggedSlot);
     } else {
         TargetItem = &ViewableGridSlots[TargetSlotIndex];
     }
     if(!TargetItem) return false; // Should not happen if logic above is correct

    // Check compatibility if target is tagged
    if (bTargetIsTag) {
        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemIdToMove);
        if (LinkedInventoryComponent->GetReceivableQuantityForTaggedSlot(ItemData, TargetTaggedSlot, SourceItem->Quantity, true, true) <= 0) {
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

    // --- Validate Move with Inventory Component (if needed) ---
    int32 QuantityValidatedByServer = RequestedQuantity;
    if (LinkedInventoryComponent && (bSourceIsTag || bTargetIsTag))
    {
        QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
        if (QuantityValidatedByServer <= 0 && bTargetIsTag && !IsSplit && TryUnblockingMove(TargetTaggedSlot, ItemIdToMove))
        {
             QuantityValidatedByServer = LinkedInventoryComponent->ValidateMoveItem(ItemIdToMove, RequestedQuantity, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
        }
        if (QuantityValidatedByServer <= 0) return false;
    }

    int32 QuantityToActuallyMove = QuantityValidatedByServer;
    InstancesToMove = SourceItem->GetInstancesFromEnd(QuantityToActuallyMove); // Re-get instances for the validated quantity
    TArray<int32> InstanceIdsToMove = FItemBundle::ToInstanceIds(InstancesToMove);
    
    // --- Perform Visual Move ---
    FGenericItemBundle SourceItemGB(SourceItem);
    FGenericItemBundle TargetItemGB(TargetItem);
    // Important: Pass correct instances to MoveBetweenSlots
    FRISMoveResult MoveResult = URISFunctions::MoveBetweenSlots(SourceItemGB, TargetItemGB, false, QuantityToActuallyMove, InstancesToMove, !IsSplit);

    // --- Handle Results and Pending Operations ---
    if (MoveResult.QuantityMoved > 0 || MoveResult.WereItemsSwapped)
    {
        // --- Broadcast Updates ---
        if(bSourceIsTag) OnTaggedSlotUpdated.Broadcast(SourceTaggedSlot); else OnGridSlotUpdated.Broadcast(SourceSlotIndex);
        if(bTargetIsTag) OnTaggedSlotUpdated.Broadcast(TargetTaggedSlot); else OnGridSlotUpdated.Broadcast(TargetSlotIndex);

        if (bSourceIsTag || bTargetIsTag)
        {
            // --- Update Pending Operations ---
            // Source Remove
            if (MoveResult.QuantityMoved > 0) {
                OperationsToConfirm.Emplace(FRISExpectedOperation(bSourceIsTag ? RemoveTagged : Remove, SourceTaggedSlot, ItemIdToMove, MoveResult.QuantityMoved));
            }
            // Target Add (of original item)
            if (MoveResult.QuantityMoved > 0) {
                OperationsToConfirm.Emplace(FRISExpectedOperation(bTargetIsTag ? AddTagged : Add, TargetTaggedSlot, ItemIdToMove, MoveResult.QuantityMoved));
            }
            // Swap Operations (if occurred)
            if(MoveResult.WereItemsSwapped && TargetItem->IsValid()) { // TargetItem now holds the swapped-out item info
                // Remove swapped item from Target
                OperationsToConfirm.Emplace(FRISExpectedOperation(bTargetIsTag ? RemoveTagged : Remove, TargetTaggedSlot, SourceItem->ItemId, SourceItem->Quantity));
                // Add swapped item to Source
                OperationsToConfirm.Emplace(FRISExpectedOperation(bSourceIsTag ? AddTagged : Add, SourceTaggedSlot, SourceItem->ItemId, SourceItem->Quantity));
            }


            // --- Request Server Action (only if necessary) ---
            if (LinkedInventoryComponent && (bSourceIsTag || bTargetIsTag)) {
                LinkedInventoryComponent->MoveItem(ItemIdToMove, QuantityToActuallyMove, InstancesToMove, SourceTaggedSlot, TargetTaggedSlot, SwapItemId, SwapQuantity);
            }
        }

        return true; // Visual move succeeded
    }

    return false; // Visual move failed
}

void UInventoryGridViewModel::ForceFullUpdate_Implementation()
{
    // Implementation moved from UInventoryGridViewModel::ForceFullUpdate_Implementation
    if (!LinkedContainerComponent)
    {
        UE_LOG(LogRISInventory, Error, TEXT("ForceFullUpdate: Cannot update, LinkedContainerComponent is null."));
        return;
    }

    UE_LOG(LogRISInventory, Log, TEXT("ForceFullUpdate: Resynchronizing visual slots."));

    // Clear pending operations first
    OperationsToConfirm.Empty();

    // --- Update Grid Slots ---
    TSet<int32> ChangedGridSlots;
    for(int32 i=0; i<NumberOfGridSlots; ++i) {
        if(ViewableGridSlots[i].IsValid()) ChangedGridSlots.Add(i); // Mark previously occupied slots
    }
    ViewableGridSlots.Init(FItemBundle::EmptyItemInstance, NumberOfGridSlots);

    TArray<FItemBundle> ActualItems = LinkedContainerComponent->GetAllItems();
    for (const FItemBundle& BackingItem : ActualItems)
    {
        if (BackingItem.Quantity <= 0) continue;

        const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(BackingItem.ItemId);
        if (!ItemData) continue;

        int32 RemainingQuantity = BackingItem.Quantity;
        int32 InstanceIdx = 0; // Track instances from the backing item

        while (RemainingQuantity > 0)
        {
            int32 SlotToAddTo = FindGridSlotIndexForItem(BackingItem.ItemId, RemainingQuantity);
            if (SlotToAddTo == -1)
            {
                UE_LOG(LogRISInventory, Error, TEXT("ForceFullUpdate: Failed to find visual grid slot for item %s during resync."), *BackingItem.ItemId.ToString());
                break;
            }

            FItemBundle& TargetSlot = ViewableGridSlots[SlotToAddTo];
            int32 AddLimit = ItemData->MaxStackSize > 1 ? ItemData->MaxStackSize : 1;

            if (TargetSlot.IsValid() && TargetSlot.ItemId == BackingItem.ItemId) {
                 AddLimit -= TargetSlot.Quantity;
            } else if (!TargetSlot.IsValid()) {
                 TargetSlot.ItemId = BackingItem.ItemId;
                 TargetSlot.Quantity = 0;
                 TargetSlot.InstanceData.Empty();
            } else {
                 UE_LOG(LogRISInventory, Error, TEXT("ForceFullUpdate: FindGridSlotIndexForItem returned incompatible grid slot %d."), SlotToAddTo);
                 break;
            }

            int32 AddedAmount = FMath::Min(RemainingQuantity, AddLimit);
            if(AddedAmount <= 0) {
                 UE_LOG(LogRISInventory, Error, TEXT("ForceFullUpdate: Calculated Grid AddedAmount is zero for slot %d."), SlotToAddTo);
                 break;
            }

            TargetSlot.Quantity += AddedAmount;
            // Append correct instances
            if(InstanceIdx < BackingItem.InstanceData.Num()) {
                int32 NumInstancesToAdd = FMath::Min(AddedAmount, BackingItem.InstanceData.Num() - InstanceIdx);
                for(int32 k=0; k < NumInstancesToAdd; ++k) {
                    TargetSlot.InstanceData.Add(BackingItem.InstanceData[InstanceIdx++]);
                }
            }

            RemainingQuantity -= AddedAmount;
            ChangedGridSlots.Add(SlotToAddTo); // Mark this slot as changed (or newly occupied)
        }
    }
    // Broadcast grid updates
    for(int32 Index : ChangedGridSlots) { OnGridSlotUpdated.Broadcast(Index); }


    // --- Update Tagged Slots (if inventory) ---
    if (LinkedInventoryComponent)
    {
        TSet<FGameplayTag> ChangedTaggedSlots;
        for (const auto& Pair : ViewableTaggedSlots) {
            if(Pair.Value.IsValid()) ChangedTaggedSlots.Add(Pair.Key); // Mark previously occupied
        }

        // Clear visual tagged slots first (or update in place)
         for (auto& Pair : ViewableTaggedSlots) { Pair.Value = FItemBundle::EmptyItemInstance; }

        const TArray<FTaggedItemBundle>& ActualTaggedItems = LinkedInventoryComponent->GetAllTaggedItems();
        for (const FTaggedItemBundle& TaggedItem : ActualTaggedItems)
        {
            if (ViewableTaggedSlots.Contains(TaggedItem.Tag))
            {
                ViewableTaggedSlots[TaggedItem.Tag] = FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData);
                ChangedTaggedSlots.Add(TaggedItem.Tag); // Mark as changed/occupied
            }
            else if (TaggedItem.Tag.IsValid())
            {
                 UE_LOG(LogRISInventory, Warning, TEXT("ForceFullUpdate: Tagged item %s found in component but tag %s is not registered visually. Adding."), *TaggedItem.ItemId.ToString(), *TaggedItem.Tag.ToString());
                 ViewableTaggedSlots.Add(TaggedItem.Tag, FItemBundle(TaggedItem.ItemId, TaggedItem.Quantity, TaggedItem.InstanceData));
                 ChangedTaggedSlots.Add(TaggedItem.Tag);
            }
        }
        // Broadcast tagged updates
         for(const FGameplayTag& Tag : ChangedTaggedSlots) { OnTaggedSlotUpdated.Broadcast(Tag); }
    }
}