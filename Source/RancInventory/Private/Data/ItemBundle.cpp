#include "Data/ItemBundle.h"


const FItemBundle FItemBundle::EmptyItemInstance(FGameplayTag(), 0);

const FTaggedItemBundle FTaggedItemBundle::EmptyItemInstance(FGameplayTag(), FGameplayTag(), 0);

const FItemBundleWithInstanceData FItemBundleWithInstanceData::EmptyItemInstance(FGameplayTag(), 0);

bool FItemBundleWithInstanceData::IsValid() const
{
	return ItemId.IsValid() && Quantity > 0 && (InstanceData.Num() == Quantity || (InstanceData.Num() == 0));
}

void FItemBundleWithInstanceData::DestroyQuantity(int32 InQuantity)
{
	// Prevent underflow - only destroy up to available quantity
	
	int32 NumToDestroy = FMath::Min(InQuantity, Quantity);
	if (InstanceData.Num() > 0)
	{
		NumToDestroy = FMath::Min(NumToDestroy, InstanceData.Num());
		
		// TODO: Prioritized destruction
		if (NumToDestroy > 0)
		{
			InstanceData.RemoveAt(InstanceData.Num() - NumToDestroy, NumToDestroy);
		}
	}
   
	Quantity -= NumToDestroy;
}

int32 FItemBundleWithInstanceData::ExtractQuantity(int32 InQuantity, TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	const int32 numToExtract = FMath::Min(InQuantity, Quantity);
   
	if (numToExtract > 0)
	{
		// Get indices for items to extract
		const int32 startIndex = InstanceData.Num() - numToExtract;
       
		// Copy to target array
		int32 DynamicDataCount = FMath::Min(numToExtract, InstanceData.Num());
		if (DynamicDataCount > 0)
		{
			StateArrayToAppendTo.Append(InstanceData.GetData() + startIndex, DynamicDataCount);
			InstanceData.RemoveAt(startIndex, DynamicDataCount);
			Quantity = InstanceData.Num();
		}
		else
		{
			Quantity -= numToExtract;
		}
	}
   
	return numToExtract;
}