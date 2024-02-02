// Author: Lucas Vilas-Boas
// Year: 2023

#include "SFRancInventorySearch.h"
#include <Management/RancInventoryData.h>
#include <Management/RancInventoryFunctions.h>
#include <Widgets/Input/SSearchBox.h>
#include <Widgets/Layout/SUniformGridPanel.h>

void SFRancInventorySearch::Construct(const FArguments& InArgs)
{
    OnCheckStateChanged = InArgs._OnCheckboxStateChanged;
    OnTextChangedDelegate = InArgs._OnSearchTextChanged;

    ChildSlot
        [
            ConstructContent()
        ];
}

TSharedRef<SWidget> SFRancInventorySearch::ConstructContent()
{
#if ENGINE_MAJOR_VERSION < 5
    using FAppStyle = FEditorStyle;
#endif

    constexpr float SlotPadding = 4.f;

    const auto CheckBoxCreator_Lambda = [this](const ERancItemType& InType) -> const TSharedRef<SCheckBox>
        {
            constexpr float CheckBoxPadding = 2.f;
            const int32 Index = static_cast<int32>(InType);

            return SNew(SCheckBox)
                .Padding(CheckBoxPadding)
                .OnCheckStateChanged(this, &SFRancInventorySearch::TriggerOnCheckboxStateChanged, Index)
                .Content()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(URancInventoryFunctions::RancItemEnumTypeToString(static_cast<ERancItemType>(Index))))
                        .Margin(CheckBoxPadding)
                ];
        };

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
            SNew(SUniformGridPanel)
                .SlotPadding(1.f)
                + SUniformGridPanel::Slot(0, 0)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Accessory)
                ]
                + SUniformGridPanel::Slot(1, 0)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Armor)
                ]
                + SUniformGridPanel::Slot(0, 1)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Weapon)
                ]
                + SUniformGridPanel::Slot(1, 1)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Consumable)
                ]
                + SUniformGridPanel::Slot(0, 2)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Material)
                ]
                + SUniformGridPanel::Slot(1, 2)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Crafting)
                ]
                + SUniformGridPanel::Slot(0, 3)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Information)
                ]
                + SUniformGridPanel::Slot(1, 3)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Event)
                ]
                + SUniformGridPanel::Slot(0, 4)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Quest)
                ]
                + SUniformGridPanel::Slot(1, 4)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Junk)
                ]
                + SUniformGridPanel::Slot(0, 5)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Special)
                ]
                + SUniformGridPanel::Slot(1, 5)
                [
                    CheckBoxCreator_Lambda(ERancItemType::Other)
                ]
                + SUniformGridPanel::Slot(0, 6)
                [
                    CheckBoxCreator_Lambda(ERancItemType::None)
                ]
        ];
}

void SFRancInventorySearch::TriggerOnCheckboxStateChanged(const ECheckBoxState NewState, const int32 InType) const
{
    OnCheckStateChanged.ExecuteIfBound(NewState, InType);
}

void SFRancInventorySearch::TriggerOnSearchTextChanged(const FText& InText) const
{
    OnTextChangedDelegate.ExecuteIfBound(InText);
}