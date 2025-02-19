#pragma once

#include "CoreMinimal.h"
#include "Data/ItemInstanceData.h"
#include "ItemDurabilityTestInstanceData.generated.h"

UCLASS(Blueprintable, BlueprintType)
class UItemDurabilityTestInstanceData : public UItemInstanceData
{
	GENERATED_BODY()

public:
	/** Durability value of the item instance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, Category = "Item")
	float Durability;

	
	UItemDurabilityTestInstanceData(): Durability(0)
	{
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	}
};