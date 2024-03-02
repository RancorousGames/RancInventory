// Copyright Rancorous Games, 2024

#include "SFRancInventoryTable.h"
#include <Management/RancInventoryFunctions.h>
#include <Management/RancInventoryData.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <Engine/AssetManager.h>

static const FName ColumnId_PrimaryIdLabel("PrimaryAssetId");
static const FName ColumnId_ItemIdLabel("Id");
static const FName ColumnId_NameLabel("Name");
static const FName ColumnId_TypeLabel("Primary Type");
static const FName ColumnId_WorldMeshLabel("World Mesh");
static const FName ColumnId_ValueLabel("Value");
static const FName ColumnId_WeightLabel("Weight");

class SRancItemTableRow final : public SMultiColumnTableRow<FRancItemPtr>
{
public:
    SLATE_BEGIN_ARGS(SRancItemTableRow) : _HightlightTextSource()
        {
        }

        SLATE_ARGUMENT(TSharedPtr<FText>, HightlightTextSource)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const FRancItemPtr InEntryItem)
    {
        HighlightText = InArgs._HightlightTextSource;
        Item = InEntryItem;

        SMultiColumnTableRow<FRancItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
    }

protected:
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        const FSlateFontInfo CellFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
        const FMargin CellMargin(4.f);

        const auto TextBlockCreator_Lambda = [this, &CellFont, &CellMargin](const FText& InText) -> TSharedRef<STextBlock>
            {
                return SNew(STextBlock)
                    .Text(InText)
                    .Font(CellFont)
                    .Margin(CellMargin)
                    .HighlightText(HighlightText.IsValid() ? *HighlightText.Get() : FText::GetEmpty());
            };

        if (ColumnName == ColumnId_PrimaryIdLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(Item->PrimaryAssetId.PrimaryAssetName.ToString()));
        }

        if (ColumnName == ColumnId_ItemIdLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(*Item->Id.ToString()));
        }

        if (ColumnName == ColumnId_NameLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(Item->Name.ToString()));
        }

        if (ColumnName == ColumnId_TypeLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(*Item->Id.ToString()));
        }

        if (ColumnName == ColumnId_WorldMeshLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(Item->WorldMesh.ToString()));
        }

        if (ColumnName == ColumnId_ValueLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(FString::SanitizeFloat(Item->Value)));
        }

        if (ColumnName == ColumnId_WeightLabel)
        {
            return TextBlockCreator_Lambda(FText::FromString(FString::SanitizeFloat(Item->Weight)));
        }

        return SNullWidget::NullWidget;
    }

private:
    FRancItemPtr Item;
    TSharedPtr<FText> HighlightText;
};

void SFRancInventoryTable::Construct([[maybe_unused]] const FArguments&)
{
    const TSharedPtr<SHeaderRow> HeaderRow = SNew(SHeaderRow);

    const auto HeaderColumnCreator_Lambda = [this](const FName& ColumnId, const FString& ColumnText, const float InWidth = 1.f) -> const SHeaderRow::FColumn::FArguments
        {
            return SHeaderRow::Column(ColumnId)
                .DefaultLabel(FText::FromString(ColumnText))
                .FillWidth(InWidth)
                .SortMode(this, &SFRancInventoryTable::GetColumnSort, ColumnId)
                .OnSort(this, &SFRancInventoryTable::OnColumnSort)
                .HeaderComboVisibility(EHeaderComboVisibility::OnHover);
        };

    //HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_PrimaryIdLabel, "Primary Asset Id", 0.75f));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_ItemIdLabel, "Id", 1.25f));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_NameLabel, "Name"));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_TypeLabel, "Primary Type", 1.25f));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_WorldMeshLabel, "World Mesh"));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_ValueLabel, "Value", 0.5f));
    HeaderRow->AddColumn(HeaderColumnCreator_Lambda(ColumnId_WeightLabel, "Weight", 0.5f));

    ChildSlot
        [
            ConstructContent(HeaderRow.ToSharedRef())
        ];

    UAssetManager::CallOrRegister_OnCompletedInitialScan(
        FSimpleMulticastDelegate::FDelegate::CreateLambda(
            [this]
            {
                UpdateItemList();
            }
        )
    );
}

TSharedRef<SWidget> SFRancInventoryTable::ConstructContent(const TSharedRef<SHeaderRow> HeaderRow)
{
    return SAssignNew(EdListView, SListView<FRancItemPtr>)
        .ListItemsSource(&ItemArr)
        .SelectionMode(ESelectionMode::Multi)
        .IsFocusable(true)
        .OnGenerateRow(this, &SFRancInventoryTable::OnGenerateWidgetForList)
        .HeaderRow(HeaderRow)
        .OnMouseButtonDoubleClick(this, &SFRancInventoryTable::OnTableItemDoubleClicked);
}

TSharedRef<ITableRow> SFRancInventoryTable::OnGenerateWidgetForList(const FRancItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
    return SNew(SRancItemTableRow, OwnerTable, InItem)
        .Visibility(this, &SFRancInventoryTable::GetIsVisible, InItem)
        .HightlightTextSource(SearchText);
}

// ReSharper disable once CppMemberFunctionMayBeStatic - This function is used by the Slate UI system
void SFRancInventoryTable::OnTableItemDoubleClicked(const FRancItemPtr RancItemRowData) const
{
    if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
    {
        UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        const FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(RancItemRowData->PrimaryAssetId);

        AssetEditorSubsystem->OpenEditorForAsset(AssetPath.ToString());
    }
}

EVisibility SFRancInventoryTable::GetIsVisible(const FRancItemPtr InItem) const
{
    EVisibility Output;

    if ([&InItem](const FString& InText) -> const bool
        {
            return InText.IsEmpty()
                || InItem->Id.ToString().Contains(InText, ESearchCase::IgnoreCase)
                || InItem->Name.ToString().Contains(InText, ESearchCase::IgnoreCase)
                || InItem->Type.ToString().Contains(InText, ESearchCase::IgnoreCase)
                || InItem->Categories.ToString().Contains(InText, ESearchCase::IgnoreCase)
                || InItem->WorldMesh.ToString().Contains(InText, ESearchCase::IgnoreCase)
                || FString::SanitizeFloat(InItem->Value).Contains(InText, ESearchCase::IgnoreCase)
                || FString::SanitizeFloat(InItem->Weight).Contains(InText, ESearchCase::IgnoreCase);
        }(SearchText.IsValid() ? SearchText->ToString() : FString())
                && (InItem->Categories.HasAny(AllowedTypes) ||
                    AllowedTypes.IsEmpty() ||
                    AllowedTypes.HasTag(InItem->Id) ||
                    AllowedTypes.HasTag(InItem->Type)))
    {
        Output = EVisibility::Visible;
    }
    else
    {
        Output = EVisibility::Collapsed;
    }

    return Output;
}

void SFRancInventoryTable::OnSearchTextModified(const FText& InText)
{
    SearchText = MakeShared<FText>(InText);
    EdListView->RebuildList();
}

void SFRancInventoryTable::OnSearchCategoriesModified(const FGameplayTagContainer& InCategories)
{
    AllowedTypes = InCategories;
    EdListView->RequestListRefresh();
}

void SFRancInventoryTable::UpdateItemList()
{
    ItemArr.Empty();

    for (const FPrimaryAssetId& Iterator : URISInventoryFunctions::GetAllRancItemPrimaryIds())
    {
        ItemArr.Add(MakeShared<FRancItemRowData>(Iterator));
    }

    EdListView->RequestListRefresh();

    if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized(); IsValid(AssetManager) && AssetManager->HasInitialScanCompleted() && URISInventoryFunctions::HasEmptyParam(ItemArr))
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Asset Manager could not find any Ranc Items. Please check your Asset Manager settings.")));
    }
}

TArray<FRancItemPtr> SFRancInventoryTable::GetSelectedItems() const
{
    return EdListView->GetSelectedItems();
}

void SFRancInventoryTable::OnColumnSort([[maybe_unused]] const EColumnSortPriority::Type, const FName& ColumnName, const EColumnSortMode::Type SortMode)
{
    ColumnBeingSorted = ColumnName;
    CurrentSortMode = SortMode;

    const auto Compare_Lambda = [&SortMode](const auto& Val1, const auto& Val2) -> bool
        {
            switch (SortMode)
            {
            case EColumnSortMode::Ascending:
                return Val1 < Val2;

            case EColumnSortMode::Descending:
                return Val1 > Val2;

            case EColumnSortMode::None:
                return Val1 < Val2;

            default:
                return false;
            }
        };

    const auto Sort_Lambda = [&ColumnName, &Compare_Lambda](const FRancItemPtr& Val1, const FRancItemPtr& Val2) -> bool
        {
            if (ColumnName == ColumnId_PrimaryIdLabel)
            {
                return Compare_Lambda(Val1->PrimaryAssetId.ToString(), Val2->PrimaryAssetId.ToString());
            }

            if (ColumnName == ColumnId_ItemIdLabel)
            {
                return Compare_Lambda(Val1->Id.ToString(), Val2->Id.ToString());
            }

            if (ColumnName == ColumnId_NameLabel)
            {
                return Compare_Lambda(Val1->Name.ToString(), Val2->Name.ToString());
            }

            if (ColumnName == ColumnId_TypeLabel)
            {
                const auto ItemTypeToString_Lambda = [&](const FGameplayTag InType) -> FString
                    {
                        return InType.ToString();
                    };

                return Compare_Lambda(ItemTypeToString_Lambda(Val1->Type), ItemTypeToString_Lambda(Val2->Type));
            }

            if (ColumnName == ColumnId_WorldMeshLabel)
            {
                return Compare_Lambda(Val1->WorldMesh.ToString(), Val2->WorldMesh.ToString());
            }

            if (ColumnName == ColumnId_ValueLabel)
            {
                return Compare_Lambda(Val1->Value, Val2->Value);
            }

            if (ColumnName == ColumnId_WeightLabel)
            {
                return Compare_Lambda(Val1->Weight, Val2->Weight);
            }

            return false;
        };

    Algo::Sort(ItemArr, Sort_Lambda);
    EdListView->RequestListRefresh();
}

EColumnSortMode::Type SFRancInventoryTable::GetColumnSort(const FName ColumnId) const
{
    if (ColumnBeingSorted != ColumnId)
    {
        return EColumnSortMode::None;
    }

    return CurrentSortMode;
}