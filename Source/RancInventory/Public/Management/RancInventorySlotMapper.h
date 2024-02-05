#pragma once

#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "RancInventorySlotMapper.generated.h"

UCLASS(Blueprintable)
class RANCINVENTORY_API URancInventorySlotMapper : public UObject
{
	GENERATED_BODY()

protected:
	// Directly maps a UI slot index to an inventory array index. -1 indicates an empty slot.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SlotMappings)
	TArray<int32> SlotMappings;

public:
	URancInventorySlotMapper();

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	void Initialize(URancInventoryComponent* InventoryComponent);

	
	// Sets the inventory array index for a given UI slot index
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	void SetSlotIndex(int32 SlotIndex, int32 InventoryIndex);

	// Gets the inventory array index for a given UI slot index, returns -1 if the slot is empty
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
	int32 GetInventoryIndexBySlot(int32 SlotIndex) const;

	// Finds the slot index for a given inventory index, returns -1 if not found
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
	int32 GetSlotIndexByInventoryIndex(int32 InventoryIndex) const;

	// Checks if a given slot is empty
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory Mapping")
	bool IsSlotEmpty(int32 SlotIndex) const;

	// Forwarded Methods
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	FRancItemInfo GetItem(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	int32 FindFirstSlotIndexWithTags(const FGameplayTagContainer& Tags) const;

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	int32 FindFirstSlotIndexWithId(const FPrimaryRancItemId& ItemId) const;

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	void GiveItemsTo(URancInventoryComponent* OtherInventory, const TArray<int32>& SlotIndexes);

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	void DiscardItems(const TArray<int32>& SlotIndexes);

	// Removes a number of items from the inventory, if it has enough, potentially removing from several slots
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool RemoveItemCount(const FRancItemInfo& ItemInfo);
	
	// Removes a number of items from the inventory from the specified slot index if it has enough
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool RemoveItemCountFromSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	void SortInventory(ERancInventorySortingMode Mode, ERancInventorySortingOrientation Orientation);

	// New Methods
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool AddItemToSlot(const FRancItemInfo& ItemInfo, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool SplitItem(int32 SourceSlotIndex, int32 TargetSlotIndex, int32 SplitAmount);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Inventory Mapping")
	URancInventoryComponent* LinkedInventoryComponent;
	
	// Network replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	// Replication notification function
	UFUNCTION()
	void OnRep_SlotMappings();
};