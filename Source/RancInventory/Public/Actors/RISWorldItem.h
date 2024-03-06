// Copyright Rancorous Games, 2024

#pragma once
#include "Engine/StaticMeshActor.h"
#include "..\Management\RISInventoryData.h"
#include "RISWorldItem.generated.h"

UCLASS()
class RANCINVENTORY_API ARISWorldItem : public AStaticMeshActor
{
GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadWrite, Meta = (DisplayName = "ItemInstance", ExposeOnSpawn = true), Category = "Item")
	FRISItemInstance Item;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	URISItemData* ItemData = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItem(const FRISItemInstance& NewItem);

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void OnRep_Item();
	void Initialize();

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};