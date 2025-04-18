#pragma once

#include "CoreMinimal.h"
#include "WeaponActor.h"
#include "ATestWeaponActor.generated.h"

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom))
class ATestWeaponActor : public AWeaponActor
{
	GENERATED_BODY()
public:


	virtual void OnConstruction(const FTransform& Transform) override;
};
