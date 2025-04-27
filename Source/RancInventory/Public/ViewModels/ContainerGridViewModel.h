// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/ItemBundle.h" // Include necessary data structures
#include "Data/RISDataTypes.h" // For EItemChangeReason, etc.
#include "RISNetworkingData.h" // For FRISExpectedOperation
#include "ContainerGridViewModel.generated.h"

// Forward Declarations
class UItemContainerComponent;
class UItemStaticData;
class AWorldItem;

/**
 * Base View Model for displaying and interacting with the grid portion
 * of an Item Container Component. Handles client-side visual slot layout
 * and basic container operations. Does NOT handle tagged/equipment slots.
 */
UCLASS(Blueprintable)
class RANCINVENTORY_API UContainerGridViewModel : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initializes the view model with a container component. Do not call for InventoryComponents, instead call InitializeInventory()
     * @param ContainerComponent The container component to link.
     * @param NumSlots The number of grid slots to display.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ContainerViewModel")
    void Initialize(UItemContainerComponent* ContainerComponent, int32 NumSlots = 9);

    /** Checks if a given grid slot index is empty. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ContainerViewModel")
    bool IsGridSlotEmpty(int32 SlotIndex) const;

    /** Retrieves the item bundle for a given grid slot index. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="ContainerViewModel")
    FItemBundle GetGridItem(int32 SlotIndex) const;

    /**
     * Attempts to drop a quantity of an item from a grid slot into the world.
     * @param SlotIndex The index of the grid slot to drop from.
     * @param Quantity The quantity to attempt to drop.
     * @return The quantity actually dropped.
     */
    UFUNCTION(BlueprintCallable, Category="ContainerViewModel")
    virtual int32 DropItemFromGrid(int32 SlotIndex, int32 Quantity);

    /**
     * Attempts to use an item directly from a grid slot.
     * @param SlotIndex The index of the grid slot containing the item to use.
     * @return Placeholder, currently returns 0. (Actual result depends on Use implementation).
     */
    UFUNCTION(BlueprintCallable, Category="ContainerViewModel")
    virtual int32 UseItemFromGrid(int32 SlotIndex);

    /**
     * Attempts to split a specified quantity of an item from one grid slot to another grid slot.
     * Fails if source doesn't have enough, target has a different item, or max stack size is exceeded.
     * @param SourceSlotIndex The index of the source grid slot.
     * @param TargetSlotIndex The index of the target grid slot.
     * @param Quantity The number of items to split.
     * @return True if the split was visually successful, false otherwise.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ContainerViewModel")
    bool SplitItemInGrid(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 Quantity);

    /**
     * Attempts to move or swap the entire contents of one grid slot with another grid slot.
     * @param SourceSlotIndex The index of the source grid slot.
     * @param TargetSlotIndex The index of the target grid slot.
     * @return True if the move/swap was visually successful, false otherwise.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ContainerViewModel")
    bool MoveItemInGrid(int32 SourceSlotIndex, int32 TargetSlotIndex);

    /**
     * Checks if a specific grid slot can visually accept the given item and quantity (considers stacking).
     * Does NOT check overall container weight/slot limits unless overridden.
     * @param ItemId The ID of the item to check.
     * @param Quantity The quantity to check.
     * @param SlotIndex The grid slot index to check.
     * @return True if the slot can visually receive the item.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ContainerViewModel")
    bool CanGridSlotReceiveItem(const FGameplayTag& ItemId, int32 Quantity, int32 SlotIndex) const;
 
    /** Checks if the view model has reconciled all expected operations from the linked container. */
    UFUNCTION(BlueprintCallable, Category="ContainerViewModel")
	   virtual bool AssertViewModelSettled() const;

    /** The number of grid slots managed by this view model. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ContainerViewModel")
    int32 NumberOfGridSlots;

    /** The linked container component for interaction. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ContainerViewModel")
    TObjectPtr<UItemContainerComponent> LinkedContainerComponent; // Use TObjectPtr

    /** Delegate broadcast when a grid slot's visual representation is updated. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGridSlotUpdated, int32, SlotIndex);
    UPROPERTY(BlueprintAssignable, Category="ContainerViewModel")
    FOnGridSlotUpdated OnGridSlotUpdated;

protected:
    /** Finds the best grid slot index to place an incoming item or stack. */
    UFUNCTION(BlueprintNativeEvent, Category = "ContainerViewModel")
    int32 FindGridSlotIndexForItem(const FGameplayTag& ItemId, int32 Quantity);

    /** Handles item additions notified by the linked container component. */
    UFUNCTION(BlueprintNativeEvent, Category = "ContainerViewModel")
    void HandleItemAdded(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesAdded, EItemChangeReason Reason);

    /** Handles item removals notified by the linked container component. */
    UFUNCTION(BlueprintNativeEvent, Category = "ContainerViewModel")
    void HandleItemRemoved(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason);

    /** Attempts to visually move/split items between two grid slots. */
    virtual bool MoveItemInGrid_Internal(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 InQuantity, bool IsSplit);

    /** Tries to fully refresh the view model state from the linked container. */
    UFUNCTION(BlueprintNativeEvent, Category = "ContainerViewModel")
    void ForceFullUpdate();

    /** Array representing the visual state of the grid slots. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ContainerViewModel")
    TArray<FItemBundle> ViewableGridSlots;

    /** Tracks pending operations expected from the linked container component updates. */
    UPROPERTY(VisibleAnywhere, Category="ContainerViewModel|Internal")
    TArray<FRISExpectedOperation> OperationsToConfirm;

    /** Flag to prevent re-initialization. */
    bool bIsInitialized = false;

    virtual void BeginDestroy() override;
};