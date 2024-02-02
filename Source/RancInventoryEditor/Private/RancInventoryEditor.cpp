// Author: Lucas Vilas-Boas
// Year: 2023

#include "RancInventoryEditor.h"
#include "SFRancInventoryDetailsPanel.h"
#include "SFRancInventoryFrame.h"
#include "SRancItemCreator.h"
#include "RancInventoryStaticIds.h"
#include <ToolMenus.h>
#include <Widgets/Docking/SDockTab.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#define LOCTEXT_NAMESPACE "FRancInventoryEditorModule"

void FRancInventoryEditorModule::StartupModule()
{
    const auto RegisterDelegate = FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRancInventoryEditorModule::RegisterMenus);

    UToolMenus::RegisterStartupCallback(RegisterDelegate);

    const auto MakeInstanceDelegate = FOnGetPropertyTypeCustomizationInstance::CreateStatic(&SFRancInventoryDetailsPanel::MakeInstance);

    PropertyEditorModule = &FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyEditorModule->RegisterCustomPropertyTypeLayout(ItemStackPropertyId, MakeInstanceDelegate);
}

void FRancInventoryEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RancInventoryEditorTabId);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ItemCreatorTabId);

    PropertyEditorModule->UnregisterCustomPropertyTypeLayout(ItemStackPropertyId);
}

TSharedRef<SDockTab> FRancInventoryEditorModule::OnSpawnTab([[maybe_unused]] const FSpawnTabArgs&, const FName TabId) const
{
    TSharedPtr<SWidget> OutContent;

    if (TabId == RancInventoryEditorTabId)
    {
        OutContent = SNew(SFRancInventoryFrame);
    }
    else if (TabId == ItemCreatorTabId)
    {
        OutContent = SNew(SRancItemCreator);
    }

    if (OutContent.IsValid())
    {
        return SNew(SDockTab)
            .TabRole(NomadTab)
            [
                OutContent.ToSharedRef()
            ];
    }

    return SNew(SDockTab);
}

void FRancInventoryEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

#if ENGINE_MAJOR_VERSION < 5
    const FName AppStyleName = FEditorStyle::GetStyleSetName();
#else
    const FName AppStyleName = FAppStyle::GetAppStyleSetName();
#endif

    const TSharedPtr<FWorkspaceItem> Menu = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(LOCTEXT("RancInventoryCategory", "RancInventory"), LOCTEXT("RancInventoryCategoryTooltip", "Ranc Inventory Plugins Tabs"), FSlateIcon(AppStyleName, "InputBindingEditor.LevelViewport"));

    const auto EditorTabSpawnerDelegate = FOnSpawnTab::CreateRaw(this, &FRancInventoryEditorModule::OnSpawnTab, RancInventoryEditorTabId);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(RancInventoryEditorTabId, EditorTabSpawnerDelegate)
        .SetDisplayName(FText::FromString(TEXT("Ranc Inventory Management")))
        .SetTooltipText(FText::FromString(TEXT("Open Ranc Inventory Window")))
        .SetGroup(Menu.ToSharedRef())
        .SetIcon(FSlateIcon(AppStyleName, "Icons.Package"));

    const auto ItemCreatorTabSpawnerDelegate = FOnSpawnTab::CreateRaw(this, &FRancInventoryEditorModule::OnSpawnTab, ItemCreatorTabId);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ItemCreatorTabId, ItemCreatorTabSpawnerDelegate)
        .SetDisplayName(FText::FromString(TEXT("Ranc Item Creator")))
        .SetGroup(Menu.ToSharedRef())
        .SetIcon(FSlateIcon(AppStyleName, "Icons.PlusCircle"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRancInventoryEditorModule, RancInventoryEditor)