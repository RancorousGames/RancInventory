// Author: Lucas Vilas-Boas
// Year: 2023

#include "RancInventoryEditor.h"
#include "SElementusDetailsPanel.h"
#include "SElementusFrame.h"
#include "SElementusItemCreator.h"
#include "ElementusStaticIds.h"
#include <ToolMenus.h>
#include <Widgets/Docking/SDockTab.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#define LOCTEXT_NAMESPACE "FRancInventoryEditorModule"

void FRancInventoryEditorModule::StartupModule()
{
    const auto RegisterDelegate = FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRancInventoryEditorModule::RegisterMenus);

    UToolMenus::RegisterStartupCallback(RegisterDelegate);

    const auto MakeInstanceDelegate = FOnGetPropertyTypeCustomizationInstance::CreateStatic(&SElementusDetailsPanel::MakeInstance);

    PropertyEditorModule = &FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    PropertyEditorModule->RegisterCustomPropertyTypeLayout(ItemStackPropertyId, MakeInstanceDelegate);
}

void FRancInventoryEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ElementusEditorTabId);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ItemCreatorTabId);

    PropertyEditorModule->UnregisterCustomPropertyTypeLayout(ItemStackPropertyId);
}

TSharedRef<SDockTab> FRancInventoryEditorModule::OnSpawnTab([[maybe_unused]] const FSpawnTabArgs&, const FName TabId) const
{
    TSharedPtr<SWidget> OutContent;

    if (TabId == ElementusEditorTabId)
    {
        OutContent = SNew(SElementusFrame);
    }
    else if (TabId == ItemCreatorTabId)
    {
        OutContent = SNew(SElementusItemCreator);
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

    const TSharedPtr<FWorkspaceItem> Menu = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(LOCTEXT("ElementusCategory", "Elementus"), LOCTEXT("ElementusCategoryTooltip", "Elementus Plugins Tabs"), FSlateIcon(AppStyleName, "InputBindingEditor.LevelViewport"));

    const auto EditorTabSpawnerDelegate = FOnSpawnTab::CreateRaw(this, &FRancInventoryEditorModule::OnSpawnTab, ElementusEditorTabId);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ElementusEditorTabId, EditorTabSpawnerDelegate)
        .SetDisplayName(FText::FromString(TEXT("Elementus Inventory Management")))
        .SetTooltipText(FText::FromString(TEXT("Open Elementus Inventory Window")))
        .SetGroup(Menu.ToSharedRef())
        .SetIcon(FSlateIcon(AppStyleName, "Icons.Package"));

    const auto ItemCreatorTabSpawnerDelegate = FOnSpawnTab::CreateRaw(this, &FRancInventoryEditorModule::OnSpawnTab, ItemCreatorTabId);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ItemCreatorTabId, ItemCreatorTabSpawnerDelegate)
        .SetDisplayName(FText::FromString(TEXT("Elementus Item Creator")))
        .SetGroup(Menu.ToSharedRef())
        .SetIcon(FSlateIcon(AppStyleName, "Icons.PlusCircle"));
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRancInventoryEditorModule, RancInventoryEditor)