// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>

#include "GameplayTagContainer.h"

enum class ERancItemType : uint8;

DECLARE_DELEGATE_OneParam(FOnRancItemSearchCategoriesChanged, FGameplayTagContainer);

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
    void OnSearchTypesTagContainerChanged(const FGameplayTagContainer& GameplayTags);
    
    TSharedRef<SWidget> ConstructContent();

    void TriggerOnCategoriesChanged(const FGameplayTagContainer& Categories);
    void TriggerOnSearchTextChanged(const FText& InText) const;

    FOnRancItemSearchCategoriesChanged OnCategoriesChangedEvent;
    FOnTextChanged OnTextChangedDelegate;
    FGameplayTagContainer SearchCategories;
};
