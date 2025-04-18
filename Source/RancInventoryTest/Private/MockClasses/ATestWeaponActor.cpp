#include "ATestWeaponActor.h"


void ATestWeaponActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Initialize();
}
