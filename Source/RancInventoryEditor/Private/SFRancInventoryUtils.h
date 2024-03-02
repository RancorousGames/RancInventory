// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>

class SFRancInventoryTable;

class SFRancInventoryUtils final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SFRancInventoryUtils) : _TableSource()
        {
        }

        SLATE_ARGUMENT(TSharedPtr<class SFRancInventoryTable>, TableSource)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent();
    FReply OnButtonClicked(const uint32 ButtonId) const;

    TSharedPtr<class SFRancInventoryTable> TableSource;
};
