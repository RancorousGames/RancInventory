// Copyright Rancorous Games, 2023

#include "SFRancInventoryFrame.h"
#include "SFRancInventorySearch.h"
#include "SFRancInventoryTable.h"
#include "SFRancInventoryUtils.h"
#include <Widgets/Layout/SScrollBox.h>

void SFRancInventoryFrame::Construct([[maybe_unused]] const FArguments& InArgs)
{
    ChildSlot
        [
            ConstructContent()
        ];
}

TSharedRef<SWidget> SFRancInventoryFrame::ConstructContent()
{
    constexpr float SlotPadding = 4.f;

    SAssignNew(Table, SFRancInventoryTable);

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .MaxWidth(300.f)
        [
            SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .Padding(SlotPadding)
                        .AutoHeight()
                        [
                            SNew(SFRancInventorySearch)
                                .OnSearchTextChanged(Table.ToSharedRef(), &SFRancInventoryTable::OnSearchTextModified)
                                .OnCategoriesChanged(Table.ToSharedRef(), &SFRancInventoryTable::OnSearchCategoriesModified)
                        ]
                        + SVerticalBox::Slot()
                        .Padding(SlotPadding)
                        .AutoHeight()
                        [
                            SNew(SFRancInventoryUtils)
                                .TableSource(Table)
                        ]
                ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.f)
        [
            Table.ToSharedRef()
        ];
}