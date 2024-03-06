// Copyright Rancorous Games, 2024

#pragma once
#include "Management/RISInventoryData.h"
#include "TestDelegateForwardHelper.Generated.h"

UCLASS()
class UTestDelegateForwardHelper : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> CallFn;
	TFunction<bool(const FRISItemInstance&)> CallFuncItemToBool;
	UFUNCTION()
	void Dispatch() { CallFn(); }

	UFUNCTION()
	bool DispatchItemToBool(const FRISItemInstance& Item) { return CallFuncItemToBool(Item); }
};
