// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>

class SElementusFrame final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SElementusFrame)
        {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent();

    TSharedPtr<class SElementusTable> Table;
};
