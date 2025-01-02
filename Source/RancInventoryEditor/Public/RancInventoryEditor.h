// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>

#include "PropertyEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/Docking/SDockTab.h"

class FRancInventoryEditorModule : public IModuleInterface
{
protected:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs, FName TabId) const;

private:
    void RegisterMenus();
    FPropertyEditorModule* PropertyEditorModule = nullptr;
};
