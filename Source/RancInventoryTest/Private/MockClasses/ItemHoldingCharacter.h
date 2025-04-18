#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ItemHoldingCharacter.generated.h"

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom))
class AItemHoldingCharacter  : public ACharacter
{
	GENERATED_BODY()
public:

	AItemHoldingCharacter()
	{
		PrimaryActorTick.bCanEverTick = true;
		bReplicates = true;
		bReplicateUsingRegisteredSubObjectList = true;
	}
};
