#include "Data/ItemInstanceData.h"

#include "Net/UnrealNetwork.h"

void UItemInstanceData::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UTestItemInstanceData::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UTestItemInstanceData, TestInt, FDoRepLifetimeParams());
}
