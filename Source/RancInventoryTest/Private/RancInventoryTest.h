// Copyright Rancorous Games, 2024

#pragma once

#include "Modules/ModuleManager.h"

class FRancInventoryTestModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
