// Copyright Rancorous Games, 2024

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RISNetworkingData.h"
#include "RISGridViewModel.generated.h"

class UInventoryComponent;
class RancInventoryComponent;


UCLASS(Blueprintable)
class RANCINVENTORY_API URISGridViewModel : public UObject
{
    GENERATED_BODY()

public:
	
    /* Initializes the slot mapper with a given inventory component, setting up initial mappings
     * Parameters:
     * InventoryComponent: The inventory component to be linked to the slot mapper
     * NumSlots: The number of slots to be initialized
     * bPreferEmptyUniversalSlots: Whether MoveItemToAnyTaggedSlot will prefer to use empty universal slots over occupied specialized slots
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    void Initialize(UInventoryComponent* InventoryComponent, int32 NumSlots = 9,  bool bPreferEmptyUniversalSlots = false);

    // Checks if a given slot is empty
    UFUNCTION(BlueprintCallable, BlueprintPure, Category=RIS)
    bool IsSlotEmpty(int32 SlotIndex) const;
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category=RIS)
    bool IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const;

    // Retrieves the item informatio<n for a given slot index
    UFUNCTION(BlueprintCallable, BlueprintPure, Category=RIS)
    FItemBundle GetItem(int32 SlotIndex) const;
    
    UFUNCTION(BlueprintCallable, Category=RIS)
    int32 DropItem(FGameplayTag TaggedSlot, int32 SlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintCallable, Category=RIS)
    const FItemBundle& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;
    
    
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
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    bool SplitItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    bool MoveItems(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex = -1,
                  FGameplayTag TargetTaggedSlot = FGameplayTag(), int32 TargetSlotIndex = -1);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    bool MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex);
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    bool CanSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const;
    
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=RIS)
    bool CanTaggedSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool CheckContainerLimits = true) const;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
    int32 NumberOfSlots;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
    bool PreferEmptyUniversalSlots = true;
    
    // Linked inventory component for direct interaction
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
    UInventoryComponent* LinkedInventoryComponent;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotUpdated, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable, Category=RIS)
    FOnSlotUpdated OnSlotUpdated;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaggedSlotUpdated, const FGameplayTag&, SlotTag);
    UPROPERTY(BlueprintAssignable, Category=RIS)
    FOnTaggedSlotUpdated OnTaggedSlotUpdated;
    
protected:
    FGameplayTag FindTaggedSlotForItem(const FItemBundle& Item) const;

    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    int32 FindSlotIndexForItem(const FGameplayTag& ItemId, int32 Quantity);
    
    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    void HandleItemAdded(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);
    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    void HandleItemRemoved(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);
    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    void HandleTaggedItemAdded(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);
    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    void HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);
    
    UFUNCTION(BlueprintNativeEvent , Category = RIS)
    void ForceFullUpdate();
	
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
    TArray<FItemBundle> ViewableGridSlots;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
    TMap<FGameplayTag, FItemBundle> ViewableTaggedSlots;

    UPROPERTY(VisibleAnywhere, Category=RIS)
    TArray<FRISExpectedOperation> OperationsToConfirm;
};

