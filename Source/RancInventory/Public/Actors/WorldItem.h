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

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void OnRep_Item();
	
	virtual void Initialize();
	
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Initialize"))
	void ReceiveInitialize();


	//TODO: TEST IF THIS WORKS FROM BP, this is how chatgpt says to do it, then look for all // ReSharper disable once CppHidingFunction
	//UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory")
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo) override;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
	int32 GetContainedQuantity(const FGameplayTag& ItemId);
	

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};