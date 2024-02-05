#include "RancInventorySlotMapper.h"

#include "Components/RancInventoryComponent.h"
#include "Net/UnrealNetwork.h"

URancInventorySlotMapper::URancInventorySlotMapper()
{
}

int32 URancInventorySlotMapper::GetInventoryIndexBySlot(int32 SlotIndex) const
{
    if(SlotMappings.IsValidIndex(SlotIndex))
    {
        return SlotMappings[SlotIndex];
    }
    return -1; // Return -1 if the slot index is invalid or the slot is empty
}

int32 URancInventorySlotMapper::GetSlotIndexByInventoryIndex(int32 InventoryIndex) const
{
    // Find the first slot index that matches the given inventory index
    return SlotMappings.IndexOfByKey(InventoryIndex);
}

void URancInventorySlotMapper::Initialize(URancInventoryComponent* InventoryComponent)
{
    if (!InventoryComponent) return;
    LinkedInventoryComponent = InventoryComponent;

    const TArray<FRancItemInfo>& Items = LinkedInventoryComponent->GetItemsArray();
    SlotMappings.Init(-1, Items.Num());
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        SlotMappings[i] = i; // Direct 1-to-1 mapping
    }
}

void URancInventorySlotMapper::SetSlotIndex(int32 SlotIndex, int32 InventoryIndex)
{
    // Ensure the slot index is within bounds and the inventory index is valid
    if (SlotIndex < 0 || InventoryIndex < -1 || !LinkedInventoryComponent) return;

    // Resize SlotMappings if needed to accommodate the SlotIndex
    if (SlotIndex >= SlotMappings.Num())
    {
        SlotMappings.SetNum(SlotIndex + 1, false); // False to not initialize new elements, keeping them at default int32 value which is 0, not -1. Adjust accordingly.
    }

    // Update the mapping for the specified slot
    SlotMappings[SlotIndex] = InventoryIndex;
}


void URancInventorySlotMapper::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(URancInventorySlotMapper, LinkedInventoryComponent);
    DOREPLIFETIME(URancInventorySlotMapper, SlotMappings);
}

void URancInventorySlotMapper::OnRep_SlotMappings()
{
    // React to replication: Possibly trigger UI update or other logic
}

bool URancInventorySlotMapper::IsSlotEmpty(int32 SlotIndex) const
{
    // Check if the slot index is valid and within the bounds of SlotMappings
    if (SlotIndex >= 0 && SlotIndex < SlotMappings.Num())
    {
        // A slot is considered empty if its mapping is -1
        return SlotMappings[SlotIndex] == -1;
    }

    // Return true by default if the slot index is out of bounds, indicating it's effectively "empty"
    return true;
}

FRancItemInfo URancInventorySlotMapper::GetItem(int32 SlotIndex)
{
    int32 InventoryIndex = GetInventoryIndexBySlot(SlotIndex);
    return LinkedInventoryComponent->GetItemCopyAt(InventoryIndex);
}

int32 URancInventorySlotMapper::FindFirstSlotIndexWithTags(const FGameplayTagContainer& Tags) const
{
    int32 OutIndex = -1;
    if (LinkedInventoryComponent->FindFirstItemIndexWithTags(Tags, OutIndex, FGameplayTagContainer(), 0))
    {
        return GetSlotIndexByInventoryIndex(OutIndex);
    }
    return -1;
}

int32 URancInventorySlotMapper::FindFirstSlotIndexWithId(const FPrimaryRancItemId& ItemId) const
{
    int32 OutIndex = -1;
    if (LinkedInventoryComponent->FindFirstItemIndexWithId(ItemId, OutIndex, FGameplayTagContainer(), 0))
    {
        return GetSlotIndexByInventoryIndex(OutIndex);
    }
    return -1;
}

void URancInventorySlotMapper::GiveItemsTo(URancInventoryComponent* OtherInventory, const TArray<int32>& SlotIndexes)
{
    if (!LinkedInventoryComponent || !OtherInventory) return;

    TArray<int32> InventoryIndexes;
    for (int32 SlotIndex : SlotIndexes)
    {
        int32 InventoryIndex = GetInventoryIndexBySlot(SlotIndex);
        if (InventoryIndex >= 0) InventoryIndexes.Add(InventoryIndex);
    }
    LinkedInventoryComponent->GiveItemIndexesTo(OtherInventory, InventoryIndexes);
}

void URancInventorySlotMapper::DiscardItems(const TArray<int32>& SlotIndexes)
{
    if (!LinkedInventoryComponent) return;

    TArray<int32> InventoryIndexes;
    for (int32 SlotIndex : SlotIndexes)
    {
        int32 InventoryIndex = GetInventoryIndexBySlot(SlotIndex);
        if (InventoryIndex >= 0) InventoryIndexes.Add(InventoryIndex);
    }
    LinkedInventoryComponent->DiscardItemIndexes(InventoryIndexes);
}

bool URancInventorySlotMapper::RemoveItemCount(const FRancItemInfo& ItemInfo)
{
    if (!LinkedInventoryComponent) return false;

    // Validation Step: Check if there are enough items to remove
    int32 TotalCount = 0;
    for (const FRancItemInfo& InventoryItem : LinkedInventoryComponent->GetItemsArray())
    {
        if (InventoryItem.ItemId == ItemInfo.ItemId)
        {
            TotalCount += InventoryItem.Quantity;
        }
    }

    if (TotalCount < ItemInfo.Quantity)
    {
        // Not enough items available for removal
        return false;
    }

    // Enough items available, proceed with removal
    TArray<FRancItemInfo> Modifiers;
    Modifiers.Add(ItemInfo); // Assuming ItemInfo.Quantity specifies the total amount to remove
    LinkedInventoryComponent->UpdateRancItems(Modifiers, ERancInventoryUpdateOperation::Remove);

    return true;
}

bool URancInventorySlotMapper::RemoveItemCountFromSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex)
{
    if (!LinkedInventoryComponent || SlotIndex < 0 || SlotIndex >= SlotMappings.Num()) return false;

    int32 InventoryIndex = GetInventoryIndexBySlot(SlotIndex);
    if (InventoryIndex == -1) return false; // Slot is empty, nothing to remove

    FRancItemInfo& InventoryItem = LinkedInventoryComponent->GetItemReferenceAt(InventoryIndex);
    if (InventoryItem.ItemId != ItemInfo.ItemId || InventoryItem.Quantity < ItemInfo.Quantity)
    {
        // Not enough items in the specified slot or different item
        return false;
    }

    // Validation passed, enough items in the slot, proceed with removal
    TArray<FRancItemInfo> Modifiers;
    FRancItemInfo ModifiedItem = InventoryItem;
    ModifiedItem.Quantity = ItemInfo.Quantity; // Set the quantity to be removed
    Modifiers.Add(ModifiedItem);
    LinkedInventoryComponent->UpdateRancItems(Modifiers, ERancInventoryUpdateOperation::Remove);

    return true;
}

void URancInventorySlotMapper::SortInventory(ERancInventorySortingMode Mode, ERancInventorySortingOrientation Orientation)
{
    if (!LinkedInventoryComponent) return;

    LinkedInventoryComponent->SortInventory(Mode, Orientation);
}

bool URancInventorySlotMapper::AddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex)
{
    // Validate the slot index and linked inventory component
    if (!LinkedInventoryComponent || SlotIndex < 0 || SlotIndex >= SlotMappings.Num()) return false;

    // Check if the slot is already mapped (indicating it's not empty)
    int32 CurrentInventoryIndex = GetInventoryIndexBySlot(SlotIndex);
    if (CurrentInventoryIndex != -1) 
    {
        // Slot is not empty; you may want to handle this differently, such as allowing or disallowing stacking
        return false;
    }

    // Check if the item can be received by the inventory
    if (!LinkedInventoryComponent->CanReceiveItem(ItemInfo)) return false;

    // Add the item to the inventory
    // that adds the item to the end of the inventory list and returns the new item's index
    LinkedInventoryComponent->AddItem(ItemInfo); // Assuming this adds to the end and doesn't need an index

    // The new item's index in the inventory is the last position after addition
    int32 NewInventoryIndex = LinkedInventoryComponent->GetItemsArray().Num() - 1;

    // Ensure the slot mappings array is large enough to include this new slot index
    if (SlotMappings.Num() <= SlotIndex) 
    {
        SlotMappings.SetNum(SlotIndex + 1, true); // Resize with default value -1 for new elements
    }

    // Update the mapping to reflect the new item's position in the inventory
    SlotMappings[SlotIndex] = NewInventoryIndex;

    return true;
}

bool URancInventorySlotMapper::SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 SplitAmount)
{
    if (!LinkedInventoryComponent || SourceSlotIndex < 0 || SourceSlotIndex >= SlotMappings.Num() || SlotMappings[TargetSlotIndex] < 0) return false;

    int32 InventoryIndex = GetInventoryIndexBySlot(SourceSlotIndex);
    FRancItemInfo& ItemToSplit = LinkedInventoryComponent->GetItemReferenceAt(InventoryIndex);

    // Check if the item exists and has enough quantity to split
    if (ItemToSplit.Quantity > SplitAmount)
    {
        // Create a new item with the split quantity
        FRancItemInfo NewItem = ItemToSplit; // Copy constructor or similar logic to clone the item
        NewItem.Quantity = SplitAmount;

        // Reduce the original item's quantity
        ItemToSplit.Quantity -= SplitAmount;

        // Add the new item to the inventory. This might involve finding the next empty slot or expanding the inventory.
        // Assume URancInventoryComponent has a method to add a new item and return its index
        LinkedInventoryComponent->AddItem(NewItem);
        int32 NewInventoryIndex = LinkedInventoryComponent->GetItemsArray().Num() - 1;

        SlotMappings[TargetSlotIndex] = NewInventoryIndex; // Or other logic to maintain the slot to index mapping
        return true;
    }

    return false;
}
