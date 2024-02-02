// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>

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
