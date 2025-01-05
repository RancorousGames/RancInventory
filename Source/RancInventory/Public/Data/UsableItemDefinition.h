#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UsableItemDefinition.generated.h"

/**
 * Base class for defining usable items, allowing inline creation in editor.
 */
UCLASS(Blueprintable, EditInlineNew)
class RANCINVENTORY_API UUsableItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	// Constructor
	UUsableItemDefinition();

	/**
	 * Use function called on the target actor.
	 * @param Target - The actor on which the item is used.
	 */
	UFUNCTION(BlueprintCallable, Category="UsableItem")
	virtual void Use(AActor* Target);
};