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
	TFunction<bool(const FItemBundle&)> CallFuncItemToBool;
	UFUNCTION()
	void Dispatch() { CallFn(); }

	UFUNCTION()
	bool DispatchItemToBool(const FItemBundle& Item) { return CallFuncItemToBool(Item); }
};
