// Copyright Rancorous Games, 2024

#include "..\..\Public\Management\RISInventoryData.h"

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RISInventoryData)
#endif

const FRISItemInstance FRISItemInstance::EmptyItemInstance(FGameplayTag(), 0);
const FRancTaggedItemInstance FRancTaggedItemInstance::EmptyItemInstance(FGameplayTag(), FRISItemInstance::EmptyItemInstance);

URISItemData::URISItemData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

URISRecipe::URISRecipe(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}
