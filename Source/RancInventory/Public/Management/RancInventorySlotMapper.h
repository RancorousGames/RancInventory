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
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 NumberOfSlots;
    
    // Linked inventory component for direct interaction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    URancInventoryComponent* LinkedInventoryComponent;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable, Category="Inventory Mapping")
    FOnSlotUpdated OnSlotUpdated;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaggedSlotUpdated, const FGameplayTag&, SlotTag);
    UPROPERTY(BlueprintAssignable, Category="Inventory Mapping")
    FOnTaggedSlotUpdated OnTaggedSlotUpdated;
    
public:
    URancInventorySlotMapper();

    // Initializes the slot mapper with a given inventory component, setting up initial mappings
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void Initialize(URancInventoryComponent* InventoryComponent, int32 NumSlots = 9, bool AutoEquipToSpecialSlots = true, bool
                    PreferUniversalOverGenericSlots = false);

    // Checks if a given slot is empty
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    bool IsSlotEmpty(int32 SlotIndex) const;
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    bool IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const;

    // Retrieves the item informatio<n for a given slot index
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    FRancItemInstance GetItem(int32 SlotIndex) const;
    
    // Splits a specified quantity of items from one slot to another, creating a new slot if necessary
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    int32 DropItem(FGameplayTag TaggedSlot, int32 SlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool MoveItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex = -1,
                  FGameplayTag TargetTaggedSlot = FGameplayTag(), int32 TargetSlotIndex = -1);

    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    const FRancItemInstance& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool CanAddItemToSlot(const FRancItemInstance& ItemInfo, int32 SlotIndex) const;

    // If true, items are automatically equipped to specialized slots if they exist, if false they go into generic slots
    UPROPERTY(VisibleAnywhere, Category="Inventory Mapping")
    bool bAutoAddToSpecializedSlots;

    // If true, an item without a special slot as category will go into e.g. left/right hand slots before going into a generic slot
    UPROPERTY(VisibleAnywhere, Category="Inventory Mapping")
    bool bPreferUniversalOverGenericSlots;
    
protected:
    FGameplayTag FindTaggedSlotForItem(const FRancItemInstance& Item);
    
private:    
    UFUNCTION()
    void HandleItemAdded(const FRancItemInstance& Item);
    int32 FindSlotIndexForItem(const FRancItemInstance& Item);
    UFUNCTION()
    void HandleItemRemoved(const FRancItemInstance& ItemInfo);
    UFUNCTION()
    void HandleTaggedItemAdded(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInfo);
    UFUNCTION()
    void HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const FRancItemInstance& ItemInfo);
    
    void ForceFullUpdate();

    TArray<FExpectedOperation> OperationsToConfirm;
};

