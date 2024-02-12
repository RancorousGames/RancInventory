#pragma once

#include "CoreMinimal.h"
#include "Actors/AWorldItem.h"
#include "UObject/Object.h"
#include "RancInventorySlotMapper.generated.h"

class RancInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated, int32, SlotIndex);


UCLASS(Blueprintable)
class RANCINVENTORY_API URancInventorySlotMapper : public UObject
{
    GENERATED_BODY()

protected:
    // Maps a UI slot to item information (ID and quantity)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FRancItemInfo> SlotMappings;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 NumberOfSlots;
    
    // Linked inventory component for direct interaction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    URancInventoryComponent* LinkedInventoryComponent;
    
    UPROPERTY(BlueprintAssignable, Category="Inventory Mapping")
    FOnSlotUpdated OnSlotUpdated;
    

public:
    URancInventorySlotMapper();

    // Initializes the slot mapper with a given inventory component, setting up initial mappings
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void Initialize(URancInventoryComponent* InventoryComponent, int32 numSlots);

    // Checks if a given slot is empty
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    bool IsSlotEmpty(int32 SlotIndex) const;

    // Retrieves the item information for a given slot index
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
    FRancItemInfo GetItem(int32 SlotIndex) const;

    // Removes a specified number of items from a given slot
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void RemoveItemsFromSlot(const FRancItemInfo& ItemToRemove, int32 SlotIndex);

    // Removes items from the inventory, starting from any slot that contains the item, until the count is met
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void RemoveItems(const FRancItemInfo& ItemToRemove);

    // Splits a specified quantity of items from one slot to another, creating a new slot if necessary
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    int32 DropItem(int32 SlotIndex, int32 Count);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    void MoveItem(int32 SourceSlotIndex, int32 TargetSlotIndex);
    
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    int32 AddItems(const FRancItemInfo& ItemInfo);

    // returns remaining items that we could not add
    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    int32 AddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
    bool CanAddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex) const;

protected:
    int32 AddItemToSlotImplementation(const FRancItemInfo& ItemInfo, int32 SlotIndex, bool PushUpdates);
    
private:
    bool SuppressCallback = false;
    
    UFUNCTION()
    void HandleItemAdded(const FRancItemInfo& ItemInfo);
    UFUNCTION()
    void HandleItemRemoved(const FRancItemInfo& ItemInfo);
};