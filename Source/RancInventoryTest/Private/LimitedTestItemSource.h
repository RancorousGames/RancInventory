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
    
    
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
	int32 ExtractItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);
	
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
    int32 GetContainedQuantity(const FGameplayTag& ItemId);
	
    
};