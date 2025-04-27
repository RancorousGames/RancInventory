#pragma once

#include "CoreMinimal.h"
#include "ItemDefinitionBase.h"
#include "UObject/Object.h"
#include "UsableItemDefinition.generated.h"

class UItemStaticData;
class UItemInstanceData;

/**
 * Base class for defining usable items, allowing inline creation in editor.
 */
UCLASS(Abstract)
class RANCINVENTORY_API UUsableItemDefinition : public UItemDefinitionBase
{
	GENERATED_BODY()

public:
	// Constructor
	UUsableItemDefinition();

	/**
	 * Use function called on the target actor.
	 * @param Target - The actor on which the item is used.
	 * @param ItemStaticData - The static data of the item that was used.
	 * @param ItemInstanceData - Optional instance data for the item that was used.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void Use(AActor* Target, const UItemStaticData* ItemStaticData, UItemInstanceData* ItemInstanceData = nullptr);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
	int32 QuantityPerUse = 1;
};