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
	URecursiveContainerInstanceData();
	virtual ~URecursiveContainerInstanceData() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Will create a new container component and assign it to the RecursiveContainer property.
	virtual void Initialize_Implementation(bool OwnedByContainer, AWorldItem* OwningWorldItem, UItemContainerComponent* OwningContainer) override;

	virtual void OnDestroy_Implementation() override;
	
	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere, Category = "RIS")
	TObjectPtr<UItemContainerComponent> RepresentedContainer;

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RIS")
	int32 MaxSlotCount = 4;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RIS")
	float MaxWeight = 100.0f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RIS")
	TSubclassOf<UItemContainerComponent> ContainerClassToSpawn = nullptr; 
};

