// Author: Lucas Vilas-Boas
// Year: 2023

#include "Management/RancInventoryData.h"

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RancInventoryData)
#endif

const FRancItemInstance FRancItemInstance::EmptyItemInstance(FGameplayTag(), -1);
const FRancTaggedItemInstance FRancTaggedItemInstance::EmptyItemInstance(FGameplayTag(), FRancItemInstance::EmptyItemInstance);

URancItemData::URancItemData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

URancRecipe::URancRecipe(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}
