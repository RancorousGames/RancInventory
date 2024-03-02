// Copyright Rancorous Games, 2024

#include "SFRancInventorySearch.h"
#include <Management/RancInventoryData.h>
#include <Widgets/Input/SSearchBox.h>

#include "SGameplayTagContainerCombo.h"

void SFRancInventorySearch::Construct(const FArguments& InArgs)
{
    OnCategoriesChangedEvent = InArgs._OnCategoriesChanged;
    OnTextChangedDelegate = InArgs._OnSearchTextChanged;

    ChildSlot
        [
            ConstructContent()
        ];
}

FGameplayTagContainer SFRancInventorySearch::GetSearchTypesTagContainer() const
{
    return SearchCategories;
}

TSharedRef<SWidget> SFRancInventorySearch::ConstructContent()
{
#if ENGINE_MAJOR_VERSION < 5
    using FAppStyle = FEditorStyle;
#endif

    constexpr float SlotPadding = 4.f;

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSearchBox)
                .OnTextChanged(this, &SFRancInventorySearch::TriggerOnSearchTextChanged)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(SlotPadding)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Show types:")))
                .Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
       .Padding(SlotPadding)
       [
           SNew(SGameplayTagContainerCombo)
           .Filter(FString("Item, Items, RancInventory, Inventory, ItemTypes, Types"))
           .TagContainer(this, &SFRancInventorySearch::GetSearchTypesTagContainer)
           .OnTagContainerChanged(this, &SFRancInventorySearch::TriggerOnCategoriesChanged)
       ];
}

void SFRancInventorySearch::TriggerOnCategoriesChanged(const FGameplayTagContainer& Categories)
{
    SearchCategories = Categories;
    // ReSharper disable once CppExpressionWithoutSideEffects
    OnCategoriesChangedEvent.ExecuteIfBound(Categories);
}

void SFRancInventorySearch::TriggerOnSearchTextChanged(const FText& InText) const
{
    // ReSharper disable once CppExpressionWithoutSideEffects
    OnTextChangedDelegate.ExecuteIfBound(InText);
}