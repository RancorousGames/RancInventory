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
	FRancItemInfo Item;
	
	UPROPERTY(BlueprintReadOnly, Meta = (DisplayName = "ItemData"), Category = "Item")
	URancItemData* ItemData = nullptr;
	
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker ) override;
};