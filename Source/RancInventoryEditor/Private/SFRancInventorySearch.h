// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>

enum class ERancItemType : uint8;

DECLARE_DELEGATE_TwoParams(FOnRancItemCheckStateChanged, ECheckBoxState, int32 ItemType);

class SFRancInventorySearch final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SFRancInventorySearch)
        {
        }
        SLATE_EVENT(FOnRancItemCheckStateChanged, OnCheckboxStateChanged)
        SLATE_EVENT(FOnTextChanged, OnSearchTextChanged)

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent();

    void TriggerOnCheckboxStateChanged(ECheckBoxState NewState, int32 InType) const;
    void TriggerOnSearchTextChanged(const FText& InText) const;

    FOnRancItemCheckStateChanged OnCheckStateChanged;
    FOnTextChanged OnTextChangedDelegate;
};
