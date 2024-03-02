// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>

class FRancInventoryTestModule : public IModuleInterface
{
protected:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
