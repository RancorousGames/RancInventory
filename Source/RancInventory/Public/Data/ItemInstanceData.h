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
	virtual ~UItemInstanceData() override {}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

};