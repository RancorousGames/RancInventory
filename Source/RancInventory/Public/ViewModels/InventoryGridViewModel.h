// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "ViewModels/ContainerGridViewModel.h" // Inherit from base
#include "Data/ItemBundle.h"
#include "GameplayTagContainer.h" // Needed for FGameplayTag
#include "InventoryGridViewModel.generated.h"

// Forward Declarations
class UInventoryComponent; // Use the specific Inventory Component type
class AWorldItem;
struct FTaggedItemBundle; // Include or forward declare as needed

/**
 * View Model for displaying and interacting with a full Inventory Component,
 * including both the grid container slots and tagged equipment/hotbar slots.
 */
UCLASS(Blueprintable)
class RANCINVENTORY_API UInventoryGridViewModel : public UContainerGridViewModel // Inherit
{
    GENERATED_BODY()

public:
    /**
     * Initializes the view model with an Inventory component.
     * @param InventoryComponent The inventory component to link.
     * @param NumGridSlots The number of grid slots to display.
     * @param PreferEmptyUniversalSlots If true, MoveItemToAnyTaggedSlot prefers empty universal over occupied specialized.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="InventoryViewModel")
    void InitializeInventory(UInventoryComponent* InventoryComponent, int32 NumGridSlots = 9, bool PreferEmptyUniversalSlots = false);

    //~ Begin UContainerGridViewModel Overrides (or new functions)
    // Note: We override Initialize from base, but give it a more specific name for clarity in BP
    // If you need the base Initialize callable too, add `using Super::Initialize;` or call it explicitly.
    virtual void Initialize_Implementation(UItemContainerComponent* ContainerComponent, int32 NumSlots = 9) override;
    virtual bool AssertViewModelSettled() const override;
	virtual void ForceFullUpdate_Implementation() override;
    //~ End UContainerGridViewModel Overrides

    /** Checks if a given tagged slot is empty. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="InventoryViewModel")
    bool IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const;

    /** Retrieves the item bundle for a given tagged slot. Returns a reference for potential modification */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="InventoryViewModel")
    const FItemBundle& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const; // Keep const for getter

    /** Retrieves a modifiable reference to the item bundle for a given tagged slot. Use with caution. */
    UFUNCTION(BlueprintCallable, Category="InventoryViewModel")
    FItemBundle& GetMutableItemForTaggedSlot(const FGameplayTag& SlotTag);

    /** Attempts to drop a quantity of an item from a tagged slot. */
    UFUNCTION(BlueprintCallable, Category="InventoryViewModel")
    int32 DropItemFromTaggedSlot(FGameplayTag TaggedSlot, int32 Quantity);

    /** Attempts to use an item from a tagged slot. */
    UFUNCTION(BlueprintCallable, Category="InventoryViewModel")
    int32 UseItemFromTaggedSlot(FGameplayTag TaggedSlot);

    /** Attempts to split items between grid/tagged slots. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="InventoryViewModel")
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
     */    bool SplitItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity);

    /** Attempts to move/swap items between grid/tagged slots. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="InventoryViewModel")
    bool MoveItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot = FGameplayTag(), int32 TargetSlotIndex = -1);

    /** Attempts to move an item from grid/tagged slot to the most appropriate available tagged slot. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="InventoryViewModel")
    bool MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex);

    /** Checks if a tagged slot can receive the item (checks compatibility and stacking). */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="InventoryViewModel")
    bool CanTaggedSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool CheckContainerLimits = true) const;

	   /**
        * Attempts to add an item from a WorldItem actor into the inventory, preferring tagged or grid slots based on policy.
        * @param WorldItem The item actor to pick up.
        * @param PreferTaggedSlots Policy for choosing the destination slot type.
        * @param DestroyAfterPickup If true, destroys the WorldItem actor after successful pickup.
        */
	   UFUNCTION(BlueprintCallable, Category="InventoryViewModel")
	   void PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots, bool DestroyAfterPickup);

    /** If true, MoveItemToAnyTaggedSlot prefers empty universal slots over occupied specialized ones. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InventoryViewModel")
    bool bPreferEmptyUniversalSlots = true;

    /** The linked inventory component (specific type). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InventoryViewModel")
    TObjectPtr<UInventoryComponent> LinkedInventoryComponent; // Specific type needed

    /** Delegate broadcast when a tagged slot's visual representation is updated. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaggedSlotUpdated, const FGameplayTag&, SlotTag);
    UPROPERTY(BlueprintAssignable, Category="InventoryViewModel")
    FOnTaggedSlotUpdated OnTaggedSlotUpdated;

protected:
    /** Finds the best tagged slot for an item. */
    FGameplayTag FindTaggedSlotForItem(const FGameplayTag& ItemId, int32 Quantity, EPreferredSlotPolicy SlotPolicy) const;

    //~ Begin Event Handler Overrides/Implementations
    // We might override base handlers if inventory logic changes how grid items are handled,
    // but often we just need the new tagged handlers.
    // virtual void HandleItemAdded_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason) override;
    // virtual void HandleItemRemoved_Implementation(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason) override;

    /** Handles item additions to tagged slots notified by the linked inventory component. */
    UFUNCTION(BlueprintNativeEvent, Category = "InventoryViewModel")
    void HandleTaggedItemAdded(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& AddedInstances, FTaggedItemBundle PreviousItem, EItemChangeReason Reason);

    /** Handles item removals from tagged slots notified by the linked inventory component. */
    UFUNCTION(BlueprintNativeEvent, Category = "InventoryViewModel")
    void HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& RemovedInstances, EItemChangeReason Reason);

    /** Attempts to resolve blocking issues before moving an item to a tagged slot. */
    bool TryUnblockingMove(FGameplayTag TargetTaggedSlot, FGameplayTag ItemId);

    /** Internal implementation handling all move/split combinations. */
    virtual bool MoveItem_Internal(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit);
	
    //~ End Event Handler Overrides/Implementations

    /** Map representing the visual state of the tagged slots. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="InventoryViewModel|Internal")
    TMap<FGameplayTag, FItemBundle> ViewableTaggedSlots;

     virtual void BeginDestroy() override;
};