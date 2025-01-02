// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ItemInstanceData.generated.h"

/**
 * Abstract base class for replicated item instance data.
 */
UCLASS(Blueprintable, Abstract, BlueprintType)
class RANCINVENTORY_API UItemInstanceData : public UObject
{
	GENERATED_BODY()

public:
	// Constructor
	UItemInstanceData(){}

	/**
	 * This allows the object to be serialized for replication.
	 * It's required for UObject-derived classes to replicate properly.
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Replicated example property to demonstrate functionality */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "ItemInstance")
	FString InstanceID;
};