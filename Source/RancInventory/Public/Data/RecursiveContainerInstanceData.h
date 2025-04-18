#pragma once

#include "ItemInstanceData.h"
#include "RecursiveContainerInstanceData.generated.h"

class UItemContainerComponent;

/**
 * This class is used to represent an item that itself is a container for items. E.g. a backpack or horadric cube.
 * When this item is created it will create a new container component and assign it to the RecursiveContainer property.
 */
UCLASS(Blueprintable, BlueprintType)
class RANCINVENTORY_API URecursiveContainerInstanceData : public UItemInstanceData
{
	GENERATED_BODY()

public:
	// Constructor
	URecursiveContainerInstanceData(){}
	virtual ~URecursiveContainerInstanceData() override {}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(Replicated, BlueprintReadOnly, EditAnywhere, Category = "Test")
	TObjectPtr<UItemContainerComponent> RecursiveContainer;
};

