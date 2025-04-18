#include "Data/RecursiveContainerInstanceData.h"

#include "Net/UnrealNetwork.h"

inline void URecursiveContainerInstanceData::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URecursiveContainerInstanceData, RecursiveContainer);
}
