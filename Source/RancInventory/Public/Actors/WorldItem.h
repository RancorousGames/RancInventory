// Copyright Rancorous Games, 2024

#pragma once
#include "Core/IItemSource.h"
#include "Data/RISDataTypes.h"
#include "Engine/StaticMeshActor.h"
#include "WorldItem.generated.h"

class UItemStaticData;

UCLASS()
class RANCINVENTORY_API AWorldItem : public AStaticMeshActor, public IItemSource
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadWrite, Meta = (DisplayName = "ItemInstance", ExposeOnSpawn = true), Category = "Item")
	FItemBundle RepresentedItem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	UItemStaticData* ItemData = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItem(const FItemBundle& NewItem);


	// Called when this item is picked up by a new inventory, this is right before its destruction if item is not set to be visible when held
	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Ranc Inventory")
	void OnPickedUp(UInventoryComponent* NewInventory);
	
	// Called when this item is dropped by an inventory, this is right after its creation if item is not set to be visible when held
	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Ranc Inventory")
	void OnDropped(UInventoryComponent* NewInventory);
	
	UFUNCTION(BlueprintNativeEvent , Category = "Ranc Inventory")
	void PredictDestruction();
	
	UFUNCTION(BlueprintNativeEvent , Category = "Ranc Inventory")
	void PredictDestructionRollback();

	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void OnRep_Item();
	
	UFUNCTION(BlueprintNativeEvent, meta=(DisplayName = "Initialize"))
	void Initialize();
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial) override;
	
	virtual int32 GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;

private:
	FTimerHandle RollbackTimerHandle;
	bool bWaitingForItemLoad = false;
};