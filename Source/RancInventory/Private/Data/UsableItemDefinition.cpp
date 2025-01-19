#include "Data/UsableItemDefinition.h"
#include "GameFramework/Actor.h"

UUsableItemDefinition::UUsableItemDefinition()
{
}

void UUsableItemDefinition::Use_Implementation(AActor* Target)
{
	Use_Impl(Target);
}

void UUsableItemDefinition::Use_Impl(AActor* Target)
{
}
