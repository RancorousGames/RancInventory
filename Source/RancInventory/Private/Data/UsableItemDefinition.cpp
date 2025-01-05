#include "Data/UsableItemDefinition.h"
#include "GameFramework/Actor.h"

UUsableItemDefinition::UUsableItemDefinition()
{
}

void UUsableItemDefinition::Use(AActor* Target)
{
	if (Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("UUsableItemDefinition::Use called on target: %s"), *Target->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UUsableItemDefinition::Use called with no target."));
	}
}
