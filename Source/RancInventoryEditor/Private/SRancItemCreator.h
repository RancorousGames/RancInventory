// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>

#include "GameplayTagContainer.h"

class SRancItemCreator final : public SCompoundWidget
{
public:
    SLATE_USER_ARGS(SRancItemCreator)
        {
        }

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> ConstructContent();
    
    void OnIdTagContainerChanged(const FGameplayTagContainer& NewTagContainer);
    FGameplayTagContainer GetIdTagContainer() const;
    
    void OnTypeTagContainerChanged(const FGameplayTagContainer& NewTagContainer);
    FGameplayTagContainer GetTypeTagContainer() const;
    
    void OnCategoryTagContainerChanged(const FGameplayTagContainer& NewTagContainer);
    FGameplayTagContainer GetCategoryTagContainer() const;

    FString GetObjPath(const int32 ObjId) const;
    void OnObjChanged(const struct FAssetData& AssetData, const int32 ObjId);

    const class UClass* GetSelectedEntryClass() const;
    void HandleNewEntryClassSelected(const class UClass* Class);

    void UpdateFolders();

    FReply HandleCreateItemButtonClicked() const;
    bool IsCreateEnabled() const;

    TArray<FTextDisplayStringPtr> GetEnumValuesAsStringArray() const;

    TMap<int32, TWeakObjectPtr<class UObject>> ObjectMap;
    TSharedPtr<class FAssetThumbnailPool> ImageIcon_ThumbnailPool;
    TArray<FTextDisplayStringPtr> AssetFoldersArr;

    FName AssetName;
    FName AssetFolder;
    FGameplayTag ItemId;
    TWeakObjectPtr<const class UClass> ItemClass;
    FName ItemName;
    FText ItemDescription;
    FGameplayTag ItemType;
    FGameplayTagContainer ItemCategories;
    bool bIsStackable = false;
    float ItemValue = 0.f;
    float ItemWeight = 0.f;
};
