// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/ItemBundle.h"
#include "Data/RISDataTypes.h"
#include "RISNetworkingData.h"
#include "GameplayTagContainer.h" // Added for FGameplayTag
#include "Components/InventoryComponent.h" // Added for EPreferredSlotPolicy
#include "InventoryGridViewModel.generated.h"

// Forward Declarations
class UItemContainerComponent;
class UInventoryComponent;
class UItemStaticData;
class AWorldItem;
struct FTaggedItemBundle;

/**
 * Unified View Model for displaying and interacting with Item Containers,
 * handling both grid-only containers and full inventories with tagged slots.
 */
UCLASS(Blueprintable)
class RANCINVENTORY_API UInventoryGridViewModel : public UObject
{
    GENERATED_BODY()

public:
    // --- Initialization ---

    /**
     * Initializes the view model with a container component.
     * Handles both basic containers and full inventories.
     * @param ContainerComponent The container or inventory component to link.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel")
    void Initialize(UItemContainerComponent* ContainerComponent);

    // --- Grid Slot Functions ---

    /** Checks if a given grid slot index is empty. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ViewModel|Grid")
    bool IsGridSlotEmpty(int32 SlotIndex) const;

    /** Retrieves the item bundle for a given grid slot index. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ViewModel|Grid")
    FItemBundle GetGridItem(int32 SlotIndex) const;
	
	/** Retrieves the item bundle for a given tagged slot. Returns Empty if not an inventory or slot invalid. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="ViewModel|Tagged")
	const FItemBundle& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;

    /** Checks if a specific grid slot can visually accept the given item and quantity. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel|Grid")
    bool CanGridSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const;
	
	/** Checks if a tagged slot can receive the item (checks compatibility and stacking). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel|Tagged")
	bool CanTaggedSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, const FGameplayTag& SlotTag, bool FromInternal = true, bool AllowSwapback = false) const;
	
    // --- Tagged Slot Functions (Inventory Only) ---

    /** Checks if a given tagged slot is empty. Returns true if not an inventory or slot invalid. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ViewModel|Tagged")
    bool IsTaggedSlotEmpty(const FGameplayTag& SlotTag) const;

    /** Retrieves a modifiable reference to the item bundle for a given tagged slot. Use with caution. Returns ref to dummy if invalid. */
    UFUNCTION(BlueprintCallable, Category="ViewModel|Tagged")
    FItemBundle& GetMutableItemForTaggedSlot(const FGameplayTag& SlotTag);
	
    // --- Combined Grid/Tagged Slot Functions ---

    /** Attempts to split items between grid/tagged slots. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel|Actions")
	bool SplitItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 Quantity);

    /** Attempts to move/swap items between grid/tagged slots. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel|Actions")
    bool MoveItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot = FGameplayTag(), int32 TargetSlotIndex = -1);

    /** Attempts to move an item from grid/tagged slot to the most appropriate available tagged slot (Inventory only). */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ViewModel|Actions")
    bool MoveItemToAnyTaggedSlot(const FGameplayTag& SourceTaggedSlot, int32 SourceSlotIndex);

    /** Attempts to move an item from one view model to another.
     * Initiates moving or splitting an item from THIS view model to another container/inventory view model.
     * @param SourceTaggedSlot The source tagged slot from which to move the item if we want to move from a tagged slot.
     * @param SourceSlotIndex The index of the source slot if we want to move from a grid slot. Ignored if TaggedSlot provided.
     * @param TargetViewModel The target view model to which the item will be moved.
     * @param TargetTaggedSlot The target tagged slot in the target view model if we want to move to a tagged slot.
     * @param TargetGridSlotIndex The index of the target grid slot in the target view model if we want to move to a grid slot. Ignored if TaggedSlot provided.
     * @param Quantity The quantity of the item to move. If -1, the entire stack will be moved.
     * @return Whether move succeeded. */
    UFUNCTION(BlueprintCallable, Category = "ViewModel|Actions", meta = (AutoCreateRefTerm = "SourceTaggedSlot,TargetTaggedSlot"))
    bool MoveItemToOtherViewModel(
          FGameplayTag SourceTaggedSlot,
          int32 SourceSlotIndex,
          UInventoryGridViewModel* TargetViewModel, // Use the merged type
          FGameplayTag TargetTaggedSlot,
          int32 TargetGridSlotIndex = -1,
          int32 Quantity = -1
    );

	/** Attempts to use an item directly from a grid slot. */
	UFUNCTION(BlueprintCallable, Category="ViewModel|Grid|Actions")
	virtual int32 UseItem(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex = -1);
	
    /** Attempts to add an item from a WorldItem actor into the inventory/container. */
   UFUNCTION(BlueprintCallable, Category="ViewModel|Actions")
   void PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots, bool DestroyAfterPickup);
	
	/** Attempts to drop a quantity of an item from a grid or tagged slot into the world. */
	UFUNCTION(BlueprintCallable, Category="ViewModel|Grid|Actions")
	virtual int32 DropItem(FGameplayTag TaggedSlot, int32 GridSlotIndex, int32 Quantity);
	
    // --- State & Properties ---

    /** Checks if the view model has reconciled all expected operations from the linked component. */
    UFUNCTION(BlueprintCallable, Category="ViewModel|State")
	virtual bool AssertViewModelSettled() const;

    /** The number of grid slots managed by this view model. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel|Grid")
    int32 NumberOfGridSlots;

    /** The linked container component (base type). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel")
    TObjectPtr<UItemContainerComponent> LinkedContainerComponent;

    /** The linked inventory component (specific type, null if not an inventory). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel|Tagged")
    TObjectPtr<UInventoryComponent> LinkedInventoryComponent;

    /** If true, MoveItemToAnyTaggedSlot prefers empty universal slots over occupied specialized ones (Inventory only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel|Tagged")
    bool bPreferEmptyUniversalSlots = true;

    // --- Delegates ---

    /** Delegate broadcast when a grid slot's visual representation is updated. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGridSlotUpdated, int32, SlotIndex, const TArray<UItemInstanceData*>&, OldInstances);
    UPROPERTY(BlueprintAssignable, Category="ViewModel|Grid")
    FOnGridSlotUpdated OnGridSlotUpdated;

    /** Delegate broadcast when a tagged slot's visual representation is updated (Inventory only). */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaggedSlotUpdated, const FGameplayTag&, SlotTag, const TArray<UItemInstanceData*>&, OldInstances);
    UPROPERTY(BlueprintAssignable, Category="ViewModel|Tagged")
    FOnTaggedSlotUpdated OnTaggedSlotUpdated;

protected:
    // --- Internal Logic ---

    /** Finds the best grid slot index to place an incoming item or stack. Does not allow any kind of overriding */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    int32 FindGridSlotIndexForItem(const FGameplayTag& ItemId, int32 Quantity);

    /** Finds the best tagged slot for an item (Inventory only). */
    FGameplayTag FindTaggedSlotForItem(const FGameplayTag& ItemId, int32 Quantity, EPreferredSlotPolicy SlotPolicy) const;

    /** Handles item additions to the container (grid) notified by the linked component. */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    void HandleItemAdded(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesAdded, EItemChangeReason Reason);

    /** Handles item removals from the container (grid) notified by the linked component. */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    void HandleItemRemoved(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason);

    /** Handles item additions to tagged slots notified by the linked inventory component (Inventory only). */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    void HandleTaggedItemAdded(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& AddedInstances, FTaggedItemBundle PreviousItem, EItemChangeReason Reason);

    /** Handles item removals from tagged slots notified by the linked inventory component (Inventory only). */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    void HandleTaggedItemRemoved(const FGameplayTag& SlotTag, const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& RemovedInstances, EItemChangeReason Reason);

    /** Attempts to resolve blocking issues before moving an item to a tagged slot (Inventory only). */
    bool TryUnblockingMove(FGameplayTag TargetTaggedSlot, FGameplayTag ItemId);

    /** Internal implementation handling all move/split combinations. */
    virtual bool MoveItem_Internal(FGameplayTag SourceTaggedSlot, int32 SourceSlotIndex, FGameplayTag TargetTaggedSlot, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit);

    /** Tries to fully refresh the view model state from the linked component. */
    UFUNCTION(BlueprintNativeEvent, Category = "ViewModel")
    void ForceFullUpdate();

    /** Array representing the visual state of the grid slots. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel|Internal")
    TArray<FItemBundle> ViewableGridSlots;

    /** Map representing the visual state of the tagged slots (Inventory only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ViewModel|Internal")
    TMap<FGameplayTag, FItemBundle> ViewableTaggedSlots;

    /** Tracks pending operations expected from the linked component updates. */
    UPROPERTY(VisibleAnywhere, Category="ViewModel|Internal")
    TArray<FRISExpectedOperation> OperationsToConfirm;

    /** Flag to prevent re-initialization. */
    bool bIsInitialized = false;

    virtual void BeginDestroy() override;

private:
    // Make the mutable getter private or protected if external modification isn't desired
     FItemBundle& GetMutableItemForTaggedSlotInternal(const FGameplayTag& SlotTag);
     static FItemBundle DummyEmptyBundle; // Static dummy to return reference from const getter on fail

	friend class GridViewModelTestContext;
};