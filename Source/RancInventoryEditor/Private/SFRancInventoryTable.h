// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>
#include "Management/RancInventoryData.h"
#include "Management/RancInventoryFunctions.h"

struct FRancItemRowData
{
    explicit FRancItemRowData(const FPrimaryRancItemId& InPrimaryAssetId)
    {
        const auto ItemData = URancInventoryFunctions::GetSingleItemDataById(InPrimaryAssetId, { TEXT("Data"), TEXT("SoftData"), }, false);

        PrimaryAssetId = InPrimaryAssetId;
        Id = ItemData->ItemId;
        Name = ItemData->ItemName;
        Type = ItemData->ItemType;
        Class = ItemData->ItemClass.IsValid() ? *ItemData->ItemClass.LoadSynchronous()->GetName() : FName();
        Object = ItemData->ItemObject.IsValid() ? *ItemData->ItemObject.LoadSynchronous()->GetName() : FName();
        Value = ItemData->ItemValue;
        Weight = ItemData->ItemWeight;

        URancInventoryFunctions::UnloadRancItem(InPrimaryAssetId);
    }

    explicit FRancItemRowData(const FPrimaryAssetId& InPrimaryAssetId) : FRancItemRowData(FPrimaryRancItemId(InPrimaryAssetId))
    {
    }

    FPrimaryAssetId PrimaryAssetId;
    int32 Id = -1;
    FName Name = NAME_None;
    ERancItemType Type = ERancItemType::None;
    FName Class = NAME_None;
    FName Object = NAME_None;
    float Value = -1.f;
    float Weight = -1.f;
};

using FRancItemPtr = TSharedPtr<FRancItemRowData, ESPMode::ThreadSafe>;

class SFRancInventoryTable final : public SCompoundWidget
{
    friend class SFRancInventoryFrame;
    friend class SFRancInventoryUtils;

public:
    SLATE_USER_ARGS(SFRancInventoryTable)
        {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent(const TSharedRef<class SHeaderRow> HeaderRow);

    TSharedRef<ITableRow> OnGenerateWidgetForList(const FRancItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const;
    void OnTableItemDoubleClicked(FRancItemPtr RancItemRowData) const;
    void OnColumnSort(EColumnSortPriority::Type SortPriority, const FName& ColumnName, EColumnSortMode::Type SortMode);
    EColumnSortMode::Type GetColumnSort(const FName ColumnId) const;
    EVisibility GetIsVisible(const FRancItemPtr InItem) const;
    void OnSearchTextModified(const FText& InText);
    void OnSearchTypeModified(const ECheckBoxState InState, const int32 InType);
    void UpdateItemList();
    TArray<FRancItemPtr> GetSelectedItems() const;

    TArray<FRancItemPtr> ItemArr;
    TArray<int32> AllowedTypes;
    TSharedPtr<FText> SearchText;
    FName ColumnBeingSorted = NAME_None;
    EColumnSortMode::Type CurrentSortMode = EColumnSortMode::None;

    TSharedPtr<SListView<FRancItemPtr>> EdListView;
};
