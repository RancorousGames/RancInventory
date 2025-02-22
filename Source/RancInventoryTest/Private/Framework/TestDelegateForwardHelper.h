// Copyright Rancorous Games, 2024

#pragma once
#include "Data/ItemBundle.h"
#include "TestDelegateForwardHelper.Generated.h"

UCLASS()
class UTestDelegateForwardHelper : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> CallFn;
	TFunction<int(const FGameplayTag&, int32)> CallFuncItemToInt;
	UFUNCTION()
	void Dispatch() { CallFn(); }

	UFUNCTION()
	int32 DispatchItemToInt(const FGameplayTag& ItemId, int32 Quantity) { return CallFuncItemToInt(ItemId, Quantity); }
};
