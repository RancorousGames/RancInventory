#include "LimitedTestItemSource.h"



int32 ULimitedTestItemSource::GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const
{
	return SourceRemainder;
}

int32 ULimitedTestItemSource::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& _,
	EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial)
{
	int32 SupplyableAmount = FMath::Min(SourceRemainder, Quantity);
	SourceRemainder -= SupplyableAmount;
	return SupplyableAmount;
}
