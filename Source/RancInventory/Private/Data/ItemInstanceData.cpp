#include "Data/ItemInstanceData.h"

#include "Net/UnrealNetwork.h"

void UItemInstanceData::Initialize_Implementation(bool OwnedByComponent, AWorldItem* OwningWorldItem,
	UItemContainerComponent* OwningContainer)
{
	UniqueInstanceId = GetUniqueID();
}

void UItemInstanceData::OnDestroy_Implementation()
{
}

void UItemInstanceData::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);
}

UItemInstanceData* UItemInstanceData::PickInstanceToRemove_Implementation(
	const TArray<UItemInstanceData*>& StateInstances)
{
	return StateInstances.Last();
}
