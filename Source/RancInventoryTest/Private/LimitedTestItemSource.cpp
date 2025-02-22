#include "LimitedTestItemSource.h"



int32 ULimitedTestItemSource::GetContainedQuantity_Implementation(const FGameplayTag& ItemId)
{
	return SourceRemainder;
}

int32 ULimitedTestItemSource::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity,
	EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	int32 SupplyableAmount = FMath::Min(SourceRemainder, Quantity);
	SourceRemainder -= SupplyableAmount;
	return SupplyableAmount;
}
