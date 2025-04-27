#include "Data/ItemBundle.h"

#include "AssetViewWidgets.h"
#include "Core/RISSubsystem.h"
#include "Data/ItemStaticData.h"

const FItemBundle FItemBundle::EmptyItemInstance(FGameplayTag(), 0);
const TArray<UItemInstanceData*> FItemBundle::NoInstances;

const FTaggedItemBundle FTaggedItemBundle::EmptyItemInstance(FGameplayTag(), FGameplayTag(), 0);


bool FItemBundle::IsValid() const
{
	return ItemId.IsValid() && Quantity > 0 &&  (InstanceData.Num() == 0 || InstanceData.Num() == Quantity);
}

bool FTaggedItemBundle::IsValid() const
{
	return Tag.IsValid() && ItemId.IsValid() && Quantity > 0 && (InstanceData.Num() == 0 || InstanceData.Num() == Quantity);
}

bool ContainsImpl(int32 QuantityContained, const TArray<UItemInstanceData*>& InstanceDataContained, int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck)
{
	if (QuantityToCheck <= 0)
		return true;

	if (QuantityContained < QuantityToCheck)
		return false;

	const bool bCheckSpecificInstances = InstancesToCheck.Num() > 0;

	// If not checking specific instances OR if this item bundle doesn't use instance data anyway
	if (!bCheckSpecificInstances || InstanceDataContained.IsEmpty())
	{
		return QuantityContained >= QuantityToCheck;
	}

	ensureMsgf(InstancesToCheck.Num() <= QuantityToCheck, 
		TEXT("InstancesToCheck should not exceed QuantityToCheck."));
	
    //We need to check that InstanceDataContained.Contains(InstancesToCheck);
	for (UItemInstanceData* InstanceToCheck : InstancesToCheck)
	{
		if (!InstanceDataContained.Contains(InstanceToCheck))
		{
			return false;
		}
	}

	return QuantityContained >= QuantityToCheck;
}

bool FItemBundle::Contains(int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck) const
{
	return ContainsImpl(Quantity, InstanceData, QuantityToCheck, InstancesToCheck);
}

bool FTaggedItemBundle::Contains(int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck) const
{
	return ContainsImpl(Quantity, InstanceData, QuantityToCheck, InstancesToCheck);
}

int32 DestroyQuantityImpl(int32& ContainedQuantity, TArray<UItemInstanceData*>& ContainedInstanceData, int32 InQuantity, const TArray<UItemInstanceData*>& InstancesToDestroy, AActor* Owner)
{
    // Ensure only happens if InstancesToDestroy is used correctly (either empty or matching quantity)
    // Note: This ensure might be too strict if you intend to destroy a subset *up to* InQuantity using specific instances.
    // If that's the case, adjust the ensure logic or remove it if the calling code handles it.
    ensureMsgf(InstancesToDestroy.Num() == 0 || InstancesToDestroy.Num() <= InQuantity,
               TEXT("InstancesToDestroy count should not exceed the quantity to destroy."));

    int32 MaxQuantityToDestroy = FMath::Min(InQuantity, ContainedQuantity);
    int32 QuantityDestroyed = 0;

    if (InstancesToDestroy.Num() > 0)
    {
        // --- Handle specific instance destruction ---
        int32 InstancesFoundAndDestroyed = 0;
        // Iterate backwards for safe removal
        for (int32 i = ContainedInstanceData.Num() - 1; i >= 0; --i)
        {
            // Stop if we've already destroyed the required quantity
            if (InstancesFoundAndDestroyed >= MaxQuantityToDestroy)
            {
                break;
            }

            UItemInstanceData* CurrentInstance = ContainedInstanceData[i];
            if (InstancesToDestroy.Contains(CurrentInstance))
            {
                // Found an instance we need to destroy
                UItemInstanceData* InstanceDataToDestroyPtr = ContainedInstanceData[i]; // Keep pointer before removal
                ContainedInstanceData.RemoveAt(i); // Remove from the array

                // Destroy and unregister
                if (InstanceDataToDestroyPtr) // Check if pointer is valid before destroying
                {
                    InstanceDataToDestroyPtr->ConditionalBeginDestroy();
                    InstanceDataToDestroyPtr->OnDestroy(); // Call blueprint event if needed
                    if (Owner) // Check owner validity
                    {
                       Owner->RemoveReplicatedSubObject(InstanceDataToDestroyPtr);
                    }
                }
                InstancesFoundAndDestroyed++;
            }
        }
        QuantityDestroyed = InstancesFoundAndDestroyed;

        // Optional: Log if not all requested specific instances were found/destroyed
        if (InstancesFoundAndDestroyed < InstancesToDestroy.Num() && InstancesFoundAndDestroyed < MaxQuantityToDestroy)
        {
             UE_LOG(LogTemp, Warning, TEXT("DestroyQuantityImpl: Could not find/destroy all %d requested specific instances. Destroyed %d."), InstancesToDestroy.Num(), InstancesFoundAndDestroyed);
        }
    }
    else if (ContainedInstanceData.Num() > 0)
    {
        // --- Handle generic quantity destruction (original logic) ---
        int32 QuantityToDestroyFromEnd = FMath::Min(MaxQuantityToDestroy, ContainedInstanceData.Num());

        while (QuantityDestroyed < QuantityToDestroyFromEnd && ContainedInstanceData.Num() > 0)
        {
            UItemInstanceData* InstanceDataToDestroyPtr = ContainedInstanceData.Pop(); // Pop from end

            // Destroy and unregister
            if (InstanceDataToDestroyPtr)
            {
                 InstanceDataToDestroyPtr->ConditionalBeginDestroy();
                 InstanceDataToDestroyPtr->OnDestroy();
                 if (Owner)
                 {
                    Owner->RemoveReplicatedSubObject(InstanceDataToDestroyPtr);
                 }
            }
            QuantityDestroyed++;
        }
    }
    else
    {
        // --- Handle destruction for items without instance data ---
        QuantityDestroyed = MaxQuantityToDestroy; // Simply destroy the quantity conceptually
    }

    ContainedQuantity -= QuantityDestroyed;
    // Ensure quantity doesn't go below zero, although RemoveAt/Pop should handle the instance array size.
    ContainedQuantity = FMath::Max(0, ContainedQuantity);

    // If instance data exists, ensure the quantity matches the remaining instance count
    if (ContainedInstanceData.Num() > 0 && ContainedQuantity != ContainedInstanceData.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("DestroyQuantityImpl: Mismatch between ContainedQuantity (%d) and ContainedInstanceData count (%d) after destruction!"), ContainedQuantity, ContainedInstanceData.Num());
        ContainedQuantity = ContainedInstanceData.Num(); // Correct the quantity
    }
     else if (ContainedInstanceData.IsEmpty() && ContainedQuantity < 0) // Added check if instances are gone but quantity is somehow negative
    {
        ContainedQuantity = 0;
    }


    return QuantityDestroyed;
}

int32 FItemBundle::DestroyQuantity(int32 InQuantity, const TArray<UItemInstanceData*>& InstancesToDestroy,  AActor* Owner)
{
	return DestroyQuantityImpl(Quantity, InstanceData, InQuantity, InstancesToDestroy, Owner);
}

int32 FTaggedItemBundle::DestroyQuantity(int32 InQuantity, const TArray<UItemInstanceData*>& InstancesToDestroy,  AActor* Owner)
{
	return DestroyQuantityImpl(Quantity, InstanceData, InQuantity, InstancesToDestroy, Owner);
}

int32 ExtractQuantityImpl(int32& ContainedQuantity, TArray<UItemInstanceData*>& ContainedInstanceData, int32 InQuantity, const TArray<UItemInstanceData*>& SpecificInstancesToExtract, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner, bool bAllowPartial)
{
	const bool bSpecificInstancesProvided = SpecificInstancesToExtract.Num() > 0;
	const int32 QuantityToRequest = bSpecificInstancesProvided ? SpecificInstancesToExtract.Num() : InQuantity;

	if (QuantityToRequest <= 0)
		return 0;

	if (!bAllowPartial && !ContainsImpl(ContainedQuantity, ContainedInstanceData, QuantityToRequest, SpecificInstancesToExtract))
		return 0;
	
	const int32 MaxPossibleToExtract = FMath::Min(QuantityToRequest, ContainedQuantity);

	int32 ActualExtractedCount = 0;
	if (bSpecificInstancesProvided)
	{
		for (UItemInstanceData* InstanceToExtract : SpecificInstancesToExtract)
		{
            // Only extract up to the available quantity if partial is allowed, as user might have provided more instances than exists
            if (bAllowPartial && ActualExtractedCount >= MaxPossibleToExtract) break;

			// Loop ContainedInstanceData in reverse
			for (int32 i = ContainedInstanceData.Num() - 1; i >= 0; --i)
			{
				if (ContainedInstanceData[i] == InstanceToExtract)
				{
					auto* InstanceDataPtr = ContainedInstanceData[i];
					ActualExtractedCount++;
					StateArrayToAppendTo.Add(InstanceDataPtr);
					if (Owner)
						Owner->RemoveReplicatedSubObject(InstanceDataPtr);
					ContainedInstanceData.RemoveAt(i, 1, EAllowShrinking::No); // Don't shrink array yet
					break;
				}
			}
			ContainedInstanceData.Shrink();
		}
		ContainedQuantity -= ActualExtractedCount;

        ensureMsgf (bAllowPartial || ActualExtractedCount == QuantityToRequest, TEXT("ExtractQuantityImpl: Found %d specific instances, but expected %d despite passing pre-check."), ActualExtractedCount, QuantityToRequest);
	}
	else // Extracting by quantity
	{
		ActualExtractedCount = MaxPossibleToExtract; // We will extract the max possible allowed amount

		if (ContainedInstanceData.Num() > 0)
		{
		    for (int32 i = 0; i < ActualExtractedCount; ++i)
		    {
			    if (UItemInstanceData* InstanceDataPtr = ContainedInstanceData.Pop())
			    {
				    if (Owner) Owner->RemoveReplicatedSubObject(InstanceDataPtr);
				    StateArrayToAppendTo.Add(InstanceDataPtr);
			    }
		    }
		}

		ContainedQuantity -= ActualExtractedCount;
	}

	ContainedQuantity = FMath::Max(0, ContainedQuantity);

	return ActualExtractedCount;
}


int32 FItemBundle::Extract(int32 InQuantity, const TArray<UItemInstanceData*>& SpecificInstancesToExtract, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner, bool bAllowPartial)
{
	return ExtractQuantityImpl(Quantity, InstanceData, InQuantity, SpecificInstancesToExtract, StateArrayToAppendTo, Owner, bAllowPartial);
}

int32 FTaggedItemBundle::Extract(int32 InQuantity, const TArray<UItemInstanceData*>& SpecificInstancesToExtract, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner, bool bAllowPartial)
{
	return ExtractQuantityImpl(Quantity, InstanceData, InQuantity, SpecificInstancesToExtract, StateArrayToAppendTo, Owner, bAllowPartial);
}

TArray<int32> FItemBundle::ToInstanceIds(const TArray<UItemInstanceData*> Instances)
{
	TArray<int32> InstanceIds;
	InstanceIds.Reserve(Instances.Num());
	for (UItemInstanceData* Instance : Instances)
	{
		if (Instance)
		{
			InstanceIds.Add(Instance->UniqueInstanceId);
		}
	}
	return InstanceIds;
}

TArray<UItemInstanceData*> FromInstanceIdsImpl(const TArray<UItemInstanceData*>& ContainedInstances, const TArray<int32>& InstanceIds)
{
	TArray<UItemInstanceData*> MatchingInstanceData;
	if (InstanceIds.Num() == 0) return MatchingInstanceData;
	
	MatchingInstanceData.Reserve(InstanceIds.Num());
	for (UItemInstanceData* Instance : ContainedInstances)
	{
		if (Instance && InstanceIds.Contains(Instance->UniqueInstanceId))
		{
			MatchingInstanceData.Add(Instance);
		}
	}
	return MatchingInstanceData;
}


TArray<UItemInstanceData*> FItemBundle::FromInstanceIds(const TArray<int32> InstanceIds) const
{
	return FromInstanceIdsImpl(InstanceData, InstanceIds);
}

TArray<UItemInstanceData*> FItemBundle::GetInstancesFromEnd(int32 InQuantity) const
{
	if (InstanceData.IsEmpty() || InQuantity == 0)
		return NoInstances;

	const int32 SourceNum = InstanceData.Num();

	if (SourceNum <= InQuantity)
	{
		return InstanceData;
	}
	
	TArray<UItemInstanceData*> PartialInstances;
	PartialInstances.Reserve(InQuantity);
	for (int32 i = 0; i < InQuantity; ++i)
	{
		PartialInstances.Add(InstanceData[SourceNum - InQuantity + i]);
	}
	return PartialInstances;
}

TArray<UItemInstanceData*> FTaggedItemBundle::FromInstanceIds(const TArray<int32>& InstanceIds) const
{
	return FromInstanceIdsImpl(InstanceData, InstanceIds);
}

FItemBundle::FItemBundle(FGameplayTag InItemId, int32 InQuantity)
{
	ItemId = InItemId;
	Quantity = InQuantity;
	InstanceData = TArray<UItemInstanceData*>();

	//ensureMsgf(!URISSubsystem::GetItemDataById(InItemId) || !URISSubsystem::GetItemDataById(InItemId)->ItemInstanceDataClass->IsValidLowLevel(),
	//		   TEXT("FItemBundle constructor called without valid ItemInstanceData"), *InItemId.ToString());
}
