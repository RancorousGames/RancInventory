// Copyright Rancorous Games, 2024

#pragma once
#include "Data/RISInventoryData.h"
#include "Engine/StaticMeshActor.h"
#include "WorldItem.generated.h"

UCLASS()
class RANCINVENTORY_API AWorldItem : public AStaticMeshActor
{
GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadWrite, Meta = (DisplayName = "ItemInstance", ExposeOnSpawn = true), Category = "Item")
	FItemBundle Item;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	UItemStaticData* ItemData = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItem(const FItemBundle& NewItem);

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void OnRep_Item();
	void Initialize();

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};