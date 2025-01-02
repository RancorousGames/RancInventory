// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>

#include "GameplayTagContainer.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

enum class ERancItemType : uint8;

DECLARE_DELEGATE_OneParam(FOnRancItemSearchCategoriesChanged, const FGameplayTagContainer&);

class SFRancInventorySearch final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SFRancInventorySearch)
        {
        }
        SLATE_EVENT(FOnRancItemSearchCategoriesChanged, OnCategoriesChanged)
        SLATE_EVENT(FOnTextChanged, OnSearchTextChanged)

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FGameplayTagContainer GetSearchTypesTagContainer() const;
    
    TSharedRef<SWidget> ConstructContent();

    void TriggerOnCategoriesChanged(const FGameplayTagContainer& Categories);
    void TriggerOnSearchTextChanged(const FText& InText) const;

    FOnRancItemSearchCategoriesChanged OnCategoriesChangedEvent;
    FOnTextChanged OnTextChangedDelegate;
    FGameplayTagContainer SearchCategories;
};
