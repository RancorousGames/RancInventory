#pragma once

#include "CoreMinimal.h"
#include "ItemDefinitionBase.h"
#include "UObject/Object.h"
#include "UsableItemDefinition.generated.h"

/**
 * Base class for defining usable items, allowing inline creation in editor.
 */
UCLASS(Blueprintable, Abstract, EditInlineNew)
class RANCINVENTORY_API UUsableItemDefinition : public UItemDefinitionBase
{
	GENERATED_BODY()

public:
	// Constructor
	UUsableItemDefinition();

	/**
	 * Use function called on the target actor.
	 * @param Target - The actor on which the item is used.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void Use(AActor* Target);
	
	virtual void Use_Impl(AActor* Target);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
	int32 QuantityPerUse = 1;
};