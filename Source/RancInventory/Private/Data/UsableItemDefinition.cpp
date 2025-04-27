#include "Data/UsableItemDefinition.h"

#include "Data/ItemStaticData.h"
#include "GameFramework/Actor.h"

UUsableItemDefinition::UUsableItemDefinition()
{
}

void UUsableItemDefinition::Use_Implementation(AActor* Target, const UItemStaticData* ItemStaticData,
	UItemInstanceData* ItemInstanceData)
{
	UE_LOG(LogTemp, Display, TEXT("Item %s was used for actor %s"), *ItemStaticData->ItemId.ToString(), *Target->GetName());
}
