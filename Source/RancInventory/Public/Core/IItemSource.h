#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/RISDataTypes.h"
#include "IItemSource.Generated.h"

// Forward declarations
class UItemInstanceData;

/**
 * Interface for Item Source functionality.
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UItemSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface class for Item Source functionality.
 */
class IItemSource
{
	GENERATED_BODY()

public:

	/**
	 * Extracts items from the source if the operation is performed on the server.
	 *
	 * @param ItemId The ID of the item to extract.
	 * @param Quantity The amount of the item to extract.
	 * @param InstancesToExtract The instances of the item to extract.
	 * @param Reason The reason for the item change.
	 * @param StateArrayToAppendTo The array to append state data to.
	 * @return The number of items successfully extracted.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory")
	int32 ExtractItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);
	
	/**
	 * Gets the quantity of a specific item contained in the source.
	 *
	 * @param ItemId The ID of the item to check.
	 * @return The quantity of the item contained in the source.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory")
	int32 GetContainedQuantity(const FGameplayTag& ItemId);
};