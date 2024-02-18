#pragma once
#include "Engine/StaticMeshActor.h"
#include "Management/RancInventoryData.h"
#include "AWorldItem.generated.h"

UCLASS()
class RANCINVENTORY_API AWorldItem : public AStaticMeshActor
{
GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadWrite, Meta = (DisplayName = "ItemInstance", ExposeOnSpawn = true), Category = "Item")
	FRancItemInstance Item;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	URancItemData* ItemData = nullptr;

	virtual void OnConstruction(const FTransform& Transform) override;
	
	virtual void BeginPlay() override;
void SetItem(const FRancItemInstance& NewItem);
void OnRep_Item();
void Initialize();

virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};