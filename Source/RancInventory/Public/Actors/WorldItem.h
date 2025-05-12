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
	
	UFUNCTION(BlueprintNativeEvent, meta=(DisplayName = "Initialize"))
	void Initialize();
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial) override;
	
	virtual int32 GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};