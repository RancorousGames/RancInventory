#pragma once

#include "CoreMinimal.h"
#include "Actors/AWorldItem.h"
#include "UObject/Object.h"
#include "RancInventorySlotMapper.generated.h"

class RancInventoryComponent;


UENUM()
enum SlotOperation
{
    Add,
    AddTagged,
    Remove,
    RemoveTagged,
};

USTRUCT()
struct FExpectedOperation
{
    GENERATED_BODY()

    SlotOperation Operation = SlotOperation::Add;
    FGameplayTag TaggedSlot = FGameplayTag();
    FGameplayTag ItemId = FGameplayTag();
    int32 Quantity = 0;

    // Constructor for tagged operations
    FExpectedOperation(SlotOperation InOperation, FGameplayTag InTaggedSlot, FGameplayTag InItemID, int32 InQuantity)
        : Operation(InOperation), TaggedSlot(InTaggedSlot), ItemId(InItemID), Quantity(InQuantity) { }

    // Constructor for non-tagged operations
    FExpectedOperation(SlotOperation InOperation, FGameplayTag InItemID, int32 InQuantity)
        : Operation(InOperation), ItemId(InItemID), Quantity(InQuantity) { }

    // Default constructor
    FExpectedOperation() = default;
};

UCLASS(Blueprintable)
class RANCINVENTORY_API URancInventorySlotMapper : public UObject
{
    GENERATED_BODY()

protected:
    // Maps a UI slot to item information (ID and quantity)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FRancItemInstance> DisplayedSlots;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FGameplayTag, FRancItemInstance> DisplayedTaggedSlots;
    
public:
    URancInventorySlotMapper();

    /* Initializes the slot mapper with a given inventory component, setting up initial mappings
     * Parameters:
     * InventoryComponent: The inventory component to be linked to the slot mapper
     * NumSlots: The number of slots to be initialized
     * bPreferEmptyUniversalSlots: Whether MoveItemToAnyTaggedSlot will prefer to use empty universal slots over occupied specialized slots
     */
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void Initialize(URancInventoryComponent* InventoryComponent, int32 NumSlots = 9,  bool bPreferEmptyUniversalSlots = false);

    // Checks if a given slot is empty
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    bool IsSlotEmpty(int32 SlotIndex) const;
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    bool IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const;

    // Retrieves the item informatio<n for a given slot index
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    FRancItemInstance GetItem(int32 SlotIndex) const;
    
    /**
     * Attempts to split a specified quantity of an item from one slot to another.
     * If the source is a tagged slot, SourceTaggedSlot should be valid, and SourceSlotIndex is ignored.
     * If the source is a generic slot, SourceSlotIndex is used, and SourceTaggedSlot should be FGameplayTag::EmptyTag.
     * The same logic applies to the target slot with TargetTaggedSlot and TargetSlotIndex.
     * 
     * - If the source slot doesn't have enough quantity, the operation fails.
     * - If the target slot is occupied by a different item, the operation fails.
     * - If adding the items to the target slot would exceed the item's max stack size, the operation fails.
     * - The operation updates the quantity of items in both the source and target slots.
     * - If the operation results in the source slot being emptied, it is reset to an empty state.
     * - The operation broadcasts updates for both the source and target slots to reflect the changes.
     * 
     * @param SourceTaggedSlot The gameplay tag of the source tagged slot, if applicable.
     * @param SourceSlotIndex The index of the source generic slot, if applicable.
     * @param TargetTaggedSlot The gameplay tag of the target tagged slot, if applicable.
     * @param TargetSlotIndex The index of the target generic slot, if applicable.
     * @param Quantity The number of items to be split from the source slot to the target slot.
     */
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool SplitItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    int32 DropItem(FGameplayTag TaggedSlot, int32 SlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool MoveItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex = -1,
                  FGameplayTag TargetTaggedSlot = FGameplayTag(), int32 TargetSlotIndex = -1);

    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    const FRancItemInstance& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool CanSlotReceiveItem(const FRancItemInstance& ItemInstance, int32 SlotIndex) const;
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool CanTaggedSlotReceiveItem(const FRancItemInstance& ItemInstance, const FGameplayTag& SlotTag, bool CheckContainerLimits = true) const;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory Mapping")
    int32 NumberOfSlots;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory Mapping")
    bool PreferEmptyUniversalSlots = true;
    
    // Linked inventory component for direct interaction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory Mapping")
    URancInventoryComponent* LinkedInventoryComponent;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable, Category="Inventory Mapping")
    FOnSlotUpdated OnSlotUpdated;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaggedSlotUpdated, const FGameplayTag&, SlotTag);
    UPROPERTY(BlueprintAssignable, Category="Inventory Mapping")
    FOnTaggedSlotUpdated OnTaggedSlotUpdated;
    
protected:
    FGameplayTag FindTaggedSlotForItem(const FRancItemInstance& Item) const;
    
private:    
    UFUNCTION()
    void HandleItemAdded(const FRancItemInstance& Item);
    int32 FindSlotIndexForItem(const FRancItemInstance& Item);
    UFUNCTION()
    void HandleItemRemoved(const FRancItemInstance& ItemInstance);
    UFUNCTION()
    void HandleTaggedItemAdded(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInstance);
    UFUNCTION()
    void HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInstance);
    
    void ForceFullUpdate();

    TArray<FExpectedOperation> OperationsToConfirm;
};

