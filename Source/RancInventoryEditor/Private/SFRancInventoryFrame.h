// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>

class SFRancInventoryFrame final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SFRancInventoryFrame)
        {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent();

    TSharedPtr<class SFRancInventoryTable> Table;
};
