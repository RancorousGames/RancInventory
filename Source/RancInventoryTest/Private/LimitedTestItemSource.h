#pragma once
#include "Core/IItemSource.h"
#include "LimitedTestItemSource.generated.h"

UCLASS()
class ULimitedTestItemSource : public UObject, public IItemSource
{
    GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	int32 SourceRemainder;
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& _, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial) override;
	
    virtual int32 GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const override;
	
    
};