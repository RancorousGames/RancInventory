// Copyright Rancorous Games, 2024

#pragma once

#include <Modules/ModuleInterface.h>

/**
 *
 */
class FRancInventoryModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};