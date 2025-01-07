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
	FItemBundleWithInstanceData RepresentedItem;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	UItemStaticData* ItemData = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItem(const FItemBundleWithInstanceData& NewItem);

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void OnRep_Item();
	void Initialize();

	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
	int32 ExtractItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo) override;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
	int32 GetContainedQuantity(const FGameplayTag& ItemId);
	

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};