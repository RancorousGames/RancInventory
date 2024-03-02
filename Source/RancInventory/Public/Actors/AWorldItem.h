// Copyright Rancorous Games, 2024

#pragma once
#include "Engine/StaticMeshActor.h"
#include "Management/RancInventoryData.h"
#include "AWorldItem.generated.h"

UCLASS()
class RANCINVENTORY_API ARISWorldItem : public AStaticMeshActor
{
GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadWrite, Meta = (DisplayName = "ItemInstance", ExposeOnSpawn = true), Category = "Item")
	FRISItemInstance Item;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	URisItemData* ItemData = nullptr;

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
	void SetItem(const FRISItemInstance& NewItem);
	void OnRep_Item();
	void Initialize();

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};