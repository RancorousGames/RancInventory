#pragma once

#include "CoreMinimal.h"
#include "Data/UsableItemDefinition.h"
#include "TestUsableItemDefinition.generated.h"

UCLASS(Blueprintable, BlueprintType)
class UTestUsableItemDefinition : public UUsableItemDefinition
{
	GENERATED_BODY()
public:

	virtual void Use_Implementation(AActor* Target, const UItemStaticData* ItemStaticData, UItemInstanceData* ItemInstanceData = nullptr) override
	{
		UE_LOG(LogTemp, Verbose, TEXT("TestUsableItemDefinition::Use called on %s"), *GetName());
	} 
	
};
