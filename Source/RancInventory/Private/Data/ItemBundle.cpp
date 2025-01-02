#include "Data/ItemBundle.h"


const FItemBundle FItemBundle::EmptyItemInstance(FGameplayTag(), 0);

const FTaggedItemBundle FTaggedItemBundle::EmptyItemInstance(FGameplayTag(), FGameplayTag(), 0);


bool FItemBundleWithInstanceData::IsValid() const
{
	return /*InstanceData->IsValid() &&*/ ItemBundle.IsValid();
}

void FItemBundleWithInstanceData::DestroyQuantity(int32 quantity)
{
	// Prevent underflow - only destroy up to available quantity
	const int32 numToDestroy = FMath::Min(quantity, InstanceData.Num());
   
	// Remove items from the end
	if (numToDestroy > 0)
	{
		InstanceData.RemoveAt(InstanceData.Num() - numToDestroy, numToDestroy);
		ItemBundle.Quantity = InstanceData.Num();
	}
}

int32 FItemBundleWithInstanceData::ExtractQuantity(int32 Quantity, TArray<UItemInstanceData*> StateArrayToAppendTo)
{
	// Prevent underflow
	const int32 numToExtract = FMath::Min(Quantity, InstanceData.Num());
   
	if (numToExtract > 0)
	{
		// Get indices for items to extract
		const int32 startIndex = InstanceData.Num() - numToExtract;
       
		// Copy to target array
		StateArrayToAppendTo.Append(InstanceData.GetData() + startIndex, numToExtract);
       
		// Remove from source
		InstanceData.RemoveAt(startIndex, numToExtract);
		ItemBundle.Quantity = InstanceData.Num();
	}
   
	return numToExtract;
}