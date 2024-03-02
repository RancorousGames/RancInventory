// Copyright Rancorous Games, 2024

#include "SRancItemCreator.h"
#include <..\..\RancInventory\Public\Management\RISInventoryData.h>
#include <..\..\RancInventory\Public\Management\RISInventoryFunctions.h>
#include <PropertyCustomizationHelpers.h>
#include <AssetThumbnail.h>
#include <AssetToolsModule.h>
#include <PackageTools.h>
#include <Engine/AssetManager.h>
#include <Factories/DataAssetFactory.h>
#include <Widgets/Input/SNumericEntryBox.h>
#include <Widgets/Input/SMultiLineEditableTextBox.h>
#include <Widgets/Input/STextComboBox.h>
#include <SGameplayTagContainerCombo.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SGridPanel.h>

#include "Widgets/Input/SVectorInputBox.h"

#if ENGINE_MAJOR_VERSION >= 5
#include <UObject/SavePackage.h>
#endif

void SRISItemCreator::Construct([[maybe_unused]] const FArguments&)
{
    UpdateFolders();

    ChildSlot
        [
            ConstructContent()
        ];
}

TSharedRef<SWidget> SRISItemCreator::ConstructContent()
{
    constexpr float SlotPadding = 4.f;

    ImageIcon_ThumbnailPool = MakeShared<FAssetThumbnailPool>(1024u);

#if ENGINE_MAJOR_VERSION < 5
    using FAppStyle = FEditorStyle;
#endif

    const auto ObjEntryBoxCreator_Lambda = [this](UClass* const ObjClass, const int32 ObjId) -> const TSharedRef<SObjectPropertyEntryBox>
        {
            return SNew(SObjectPropertyEntryBox)
                .IsEnabled(true)
                .AllowedClass(ObjClass)
                .AllowClear(true)
                .DisplayUseSelected(true)
                .DisplayBrowse(true)
                .DisplayThumbnail(true)
                .ThumbnailPool(ImageIcon_ThumbnailPool.ToSharedRef())
                .ObjectPath(this, &SRISItemCreator::GetObjPath, ObjId)
                .OnObjectChanged(this, &SRISItemCreator::OnObjChanged, ObjId);
        };

    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SGridPanel)
                .FillColumn(0, 0.3f)
                .FillColumn(1, 0.7f)
                + SGridPanel::Slot(0, 0)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("ID")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 0)
                .Padding(SlotPadding)
                [
                    SNew(SGameplayTagContainerCombo)
                    .Filter(FString("Item, Items, RISInventory, Inventory"))
		            .TagContainer(this, &SRISItemCreator::GetIdTagContainer)
		            .OnTagContainerChanged(this, &SRISItemCreator::OnIdTagContainerChanged)
                ]
                + SGridPanel::Slot(0, 1)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("WorldMesh")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 1)
                .Padding(SlotPadding)
                [
                    ObjEntryBoxCreator_Lambda(UStaticMesh::StaticClass(), 0)
                ]
                + SGridPanel::Slot(0, 2)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("WorldScale")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 2)
                .Padding(SlotPadding)
                [
                    SNew(SVectorInputBox)
                    .X(ItemWorldScale.X)
                    .Y(ItemWorldScale.Y)
                    .Z(ItemWorldScale.Z)
                    .AllowSpin(false)
                    .bColorAxisLabels(false)
                    .OnXCommitted(this, &SRISItemCreator::OnSetArriveTangent, EAxis::X)
                    .OnYCommitted(this, &SRISItemCreator::OnSetArriveTangent, EAxis::Y)
                    .OnZCommitted(this, &SRISItemCreator::OnSetArriveTangent, EAxis::Z)
                    .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                /*  
                .Padding(SlotPadding)
                [
                    SNew(SClassPropertyEntryBox)
                        .AllowAbstract(true)
                        .SelectedClass(this, &SRancItemCreator::GetSelectedEntryClass)
                        .OnSetClass(this, &SRancItemCreator::HandleNewEntryClassSelected)
                ]
                */
                + SGridPanel::Slot(0, 3)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Name")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 3)
                .Padding(SlotPadding)
                [
                    SNew(SEditableTextBox)
                        .OnTextChanged_Lambda(
                            [this](const FText& InText)
                            {
                                ItemName = *InText.ToString();
                            }
                        )
                ]
                + SGridPanel::Slot(0, 4)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Description")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 4)
                .Padding(SlotPadding)
                [
                    SNew(SMultiLineEditableTextBox)
                        .OnTextChanged_Lambda(
                            [this](const FText& InText)
                            {
                                ItemDescription = InText;
                            }
                        )
                ]
                + SGridPanel::Slot(0, 5)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Type")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 5)
                .Padding(SlotPadding)
                [
                    SNew(SGameplayTagContainerCombo)
                    .Filter(FString("Item, Items, RISInventory, Inventory, Types, ItemTypes"))
                    .TagContainer(this, &SRISItemCreator::GetTypeTagContainer)
                    .OnTagContainerChanged(this, &SRISItemCreator::OnTypeTagContainerChanged)
                ]
                + SGridPanel::Slot(0, 6)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Categories")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 6)
                .Padding(SlotPadding)
                [
                    SNew(SGameplayTagContainerCombo)
                    .Filter(FString("Item, Items, RISInventory, Inventory, Categories, ItemCategories"))
                    .TagContainer(this, &SRISItemCreator::GetCategoryTagContainer)
                    .OnTagContainerChanged(this, &SRISItemCreator::OnCategoryTagContainerChanged)
                ]
                + SGridPanel::Slot(0, 7)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Is Stackable")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 7)
                .Padding(SlotPadding)
                [
                    SNew(SCheckBox)
                        .IsChecked(ECheckBoxState::Checked)
                        .OnCheckStateChanged_Lambda(
                            [this](const ECheckBoxState InState)
                            {
                                bIsStackable = InState == ECheckBoxState::Checked;
                            }
                        )
                ]
                + SGridPanel::Slot(0, 8)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Value")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 8)
                .Padding(SlotPadding)
                [
                    SNew(SNumericEntryBox<float>)
                        .AllowSpin(false)
                        .MinValue(0.0f)
                        .Value_Lambda([this] { return ItemValue; })
                        .OnValueChanged_Lambda(
                            [this](const float InValue)
                            {
                                ItemValue = InValue;
                            }
                        )
                ]
                + SGridPanel::Slot(0, 9)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Weight")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 9)
                .Padding(SlotPadding)
                [
                    SNew(SNumericEntryBox<float>)
                        .AllowSpin(false)
                        .MinValue(0.0f)
                        .Value_Lambda([this] { return ItemWeight; })
                        .OnValueChanged_Lambda(
                            [this](const float InValue)
                            {
                                ItemWeight = InValue;
                            }
                        )
                ]
                + SGridPanel::Slot(0, 10)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Icon")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 10)
                .Padding(SlotPadding)
                [
                    ObjEntryBoxCreator_Lambda(UTexture2D::StaticClass(), 1)
                ]
                + SGridPanel::Slot(0, 11)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Asset Name")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 11)
                .Padding(SlotPadding)
                [
                    SNew(SEditableTextBox)
                        .OnTextChanged_Lambda(
                            [this](const FText& InText)
                            {
                                AssetName = *InText.ToString();
                            }
                        )
                ]
                + SGridPanel::Slot(0, 12)
                .Padding(SlotPadding)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Asset Folder")))
                        .TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
                        .Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
                ]
                + SGridPanel::Slot(1, 12)
                .Padding(SlotPadding)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        [
                            SNew(STextComboBox)
                                .OptionsSource(&AssetFoldersArr)
                                .OnSelectionChanged_Lambda(
                                    [this](const FTextDisplayStringPtr& InStr, [[maybe_unused]] ESelectInfo::Type)
                                    {
                                        AssetFolder = FName(*InStr.Get());
                                    }
                                )
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                                .OnClicked_Lambda(
                                    [this]() -> FReply
                                    {
                                        UpdateFolders();
                                        return FReply::Handled();
                                    }
                                )
                                .Content()
                                        [
                                            SNew(SImage)
                                                .Image(FAppStyle::Get().GetBrush("Icons.Refresh"))
                                        ]
                        ]
                ]
                + SGridPanel::Slot(1, 13)
                .Padding(SlotPadding * 2.f)
                .HAlign(HAlign_Left)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Create Item")))
                        .OnClicked(this, &SRISItemCreator::HandleCreateItemButtonClicked)
                        .IsEnabled(this, &SRISItemCreator::IsCreateEnabled)
                        .ToolTip(
                            SNew(SToolTip)
                            .Text(FText::FromString(TEXT("Already exists a item with this Id.")))
                            .Visibility_Lambda(
                                [this]() -> EVisibility
                                {
                                    return IsCreateEnabled() ? EVisibility::Collapsed : EVisibility::Visible;
                                }
                            )
                        )
                ]
        ];
}

void SRISItemCreator::OnIdTagContainerChanged(const FGameplayTagContainer& NewTagContainer)
{
    const auto FirstTag = NewTagContainer.First();
    if (FirstTag.IsValid())
    {
        ItemId = FirstTag;
    }
    else
    {
        ItemId = FGameplayTag();
    }
}

FGameplayTagContainer SRISItemCreator::GetIdTagContainer() const
{
    return ItemId.GetSingleTagContainer();
}

void SRISItemCreator::OnTypeTagContainerChanged(const FGameplayTagContainer& NewTagContainer)
{
    const auto FirstTag = NewTagContainer.First();
    if (FirstTag.IsValid())
    {
        ItemType = FirstTag;
    }
    else
    {
        ItemType = FGameplayTag();
    }
}

FGameplayTagContainer SRISItemCreator::GetTypeTagContainer() const
{
    return ItemType.GetSingleTagContainer();
}

void SRISItemCreator::OnCategoryTagContainerChanged(const FGameplayTagContainer& NewItemCategories)
{
    ItemCategories = NewItemCategories;
}

FGameplayTagContainer SRISItemCreator::GetCategoryTagContainer() const
{
    return ItemCategories;
}

void SRISItemCreator::OnObjChanged(const FAssetData& AssetData, const int32 ObjId)
{
    ObjectMap.FindOrAdd(ObjId) = AssetData.GetAsset();
}

FString SRISItemCreator::GetObjPath(const int32 ObjId) const
{
    return ObjectMap.Contains(ObjId) && ObjectMap.FindRef(ObjId).IsValid() ? ObjectMap.FindRef(ObjId)->GetPathName() : FString();
}

void SRISItemCreator::HandleNewEntryClassSelected(const UClass* Class)
{
    ItemClass = Class;
}

const UClass* SRISItemCreator::GetSelectedEntryClass() const
{
    return ItemClass.Get();
}

void SRISItemCreator::UpdateFolders()
{
    AssetFoldersArr.Empty();

    if (const UAssetManager* const AssetManager = UAssetManager::GetIfValid())
    {
        if (FPrimaryAssetTypeInfo Info; AssetManager->GetPrimaryAssetTypeInfo(FPrimaryAssetType(RancItemDataType), Info))
        {
            for (const FString& Path : Info.AssetScanPaths)
            {
                AssetFoldersArr.Add(MakeShared<FString>(Path));
            }
        }
    }

    if (const UAssetManager* const AssetManager = UAssetManager::GetIfValid(); IsValid(AssetManager) && AssetManager->HasInitialScanCompleted() && URISInventoryFunctions::HasEmptyParam(AssetFoldersArr))
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Asset Manager could not find any folder. Please check your Asset Manager settings.")));
    }
}

FReply SRISItemCreator::HandleCreateItemButtonClicked() const
{
    if (AssetFolder.IsNone() || AssetName.IsNone())
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Please enter the asset name and folder for the new item.")));

        return FReply::Handled();
    }

    const FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

    const FString PackageName = UPackageTools::SanitizePackageName(AssetFolder.ToString() + TEXT("/") + AssetName.ToString());

    UDataAssetFactory* const Factory = NewObject<UDataAssetFactory>();

    if (UObject* const NewData = AssetToolsModule.Get().CreateAsset(AssetName.ToString(), FPackageName::GetLongPackagePath(PackageName), URISItemData::StaticClass(), Factory))
    {
        URISItemData* const ItemData = Cast<URISItemData>(NewData);
        ItemData->ItemId = ItemId;
        ItemData->ItemWorldMesh = Cast<UStaticMesh>(ObjectMap.FindRef(0)); ;
        ItemData->ItemWorldScale = ItemWorldScale;
        ItemData->ItemName = ItemName;
        ItemData->ItemDescription = ItemDescription;
        ItemData->ItemPrimaryType = ItemType;
        ItemData->ItemCategories = ItemCategories;
        ItemData->bIsStackable = bIsStackable;
        ItemData->ItemValue = ItemValue;
        ItemData->ItemWeight = ItemWeight;
        ItemData->ItemIcon = Cast<UTexture2D>(ObjectMap.FindRef(1));

        TArray<FAssetData> SyncAssets;
        SyncAssets.Add(FAssetData(ItemData));
        GEditor->SyncBrowserToObjects(SyncAssets);

        const FString TempPackageName = ItemData->GetPackage()->GetName();
        const FString TempPackageFilename = FPackageName::LongPackageNameToFilename(TempPackageName, FPackageName::GetAssetPackageExtension());

#if ENGINE_MAJOR_VERSION >= 5
        FSavePackageArgs SaveArgs;
        SaveArgs.SaveFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(ItemData->GetPackage(), ItemData, *TempPackageFilename, SaveArgs);
#else
        UPackage::SavePackage(ItemData->GetPackage(), ItemData, RF_Public | RF_Standalone, *TempPackageFilename);
#endif
    }

    return FReply::Handled();
}

bool SRISItemCreator::IsCreateEnabled() const
{
    if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
    {
        return ItemId.IsValid() && !AssetManager->GetPrimaryAssetPath(FPrimaryAssetId(RancItemDataType, *ItemId.ToString())).IsValid();
    }

    return false;
}

void SRISItemCreator::OnSetArriveTangent(float value, ETextCommit::Type CommitType, EAxis::Type Axis)
{
    switch (Axis)
    {
    case EAxis::X:
        ItemWorldScale.X = value;
        break;
    case EAxis::Y:
        ItemWorldScale.Y = value;
        break;
    case EAxis::Z:
        ItemWorldScale.Z = value;
        break;
    default:
        break;
    }
}
