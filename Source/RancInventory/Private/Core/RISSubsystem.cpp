// Copyright Rancorous Games, 2024

#include "Core/RISSubsystem.h"
#include "Engine/StreamableManager.h"
#include "URISItemData.h"
#include "URISFunctions.h"
#include "Algo/Copy.h"
#include "Core/RISFunctions.h"
#include "Data/RISInventoryData.h"

// Initialize static variables
TMap<FGameplayTag, URISItemData*> URISSubsystem::AllLoadedItemsByTag;
TArray<FGameplayTag> URISSubsystem::AllItemIds;
TArray<UObjectRecipeData*> URISSubsystem::AllLoadedRecipes;

void URISSubsystem::UnloadAllRancItems()
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
    {
        AssetManager->UnloadPrimaryAssetsWithType(FPrimaryAssetType(RancInventoryItemDataType));
    }
}

void URISSubsystem::UnloadRancItem(const FPrimaryRISItemId& InItemId)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
    {
        AssetManager->UnloadPrimaryAsset(FPrimaryAssetId(InItemId));
    }
}

bool URISSubsystem::CompareItemInfo(const FItemBundle& Info1, const FItemBundle& Info2)
{
    return Info1 == Info2;
}

bool URISSubsystem::CompareItemData(const URISItemData* Data1, const URISItemData* Data2)
{
    return Data1->GetPrimaryAssetId() == Data2->GetPrimaryAssetId();
}

URISItemData* URISSubsystem::GetSingleItemDataById(const FPrimaryRISItemId& InID,
                                                    const TArray<FName>& InBundles, const bool bAutoUnload)
{
    URISItemData* Output = nullptr;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
    {
        if (const TSharedPtr<FStreamableHandle> StreamableHandle = AssetManager->LoadPrimaryAsset(InID, InBundles);
            StreamableHandle.IsValid())
        {
            StreamableHandle->WaitUntilComplete(5.f);
            Output = Cast<URISItemData>(StreamableHandle->GetLoadedAsset());
        }
        else
        {
            const TSharedPtr<FStreamableHandle> StreamableHandleProgress = AssetManager->GetPrimaryAssetHandle(InID);
            if (StreamableHandleProgress.IsValid())
            {
                StreamableHandleProgress->WaitUntilComplete(5.f);
            }
            Output = AssetManager->GetPrimaryAssetObject<URISItemData>(InID);
        }

        if (bAutoUnload)
        {
            AssetManager->UnloadPrimaryAsset(InID);
        }
    }

    return Output;
}

TArray<URISItemData*> URISSubsystem::GetItemDataArrayById(const TArray<FPrimaryRISItemId>& InIDs,
                                                           const TArray<FName>& InBundles,
                                                           const bool bAutoUnload)
{
    TArray<URISItemData*> Output;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
    {
        Output = LoadRancItemData_Internal(AssetManager, InIDs, InBundles, bAutoUnload);
    }
    return Output;
}

TArray<URISItemData*> URISSubsystem::LoadRancItemData_Internal(
    UAssetManager* InAssetManager, const TArray<FPrimaryRISItemId>& InIDs, const TArray<FName>& InBundles,
    const bool bAutoUnload)
{
    TArray<URISItemData*> Output;

    const auto CheckAssetValidity_Lambda = [](UObject* const& InAsset) -> bool
    {
        return IsValid(InAsset);
    };

    if (const TSharedPtr<FStreamableHandle> StreamableHandle = InAssetManager->LoadPrimaryAssets(InIDs, InBundles);
        StreamableHandle.IsValid())
    {
        StreamableHandle->WaitUntilComplete(5.f);

        TArray<UObject*> LoadedAssets;
        StreamableHandle->GetLoadedAssets(LoadedAssets);

        for (UObject* const& Asset : LoadedAssets)
        {
            if (CheckAssetValidity_Lambda(Asset))
            {
                if (URISItemData* const CastedAsset = Cast<URISItemData>(Asset))
                {
                    Output.Add(CastedAsset);
                }
            }
        }
    }

    if (bAutoUnload)
    {
        InAssetManager->UnloadPrimaryAssets(InIDs);
    }

    return Output;
}
