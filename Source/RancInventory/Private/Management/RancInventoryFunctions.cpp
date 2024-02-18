// Author: Lucas Vilas-Boas
// Year: 2023

#include "Management/RancInventoryFunctions.h"
#include <Components/RancInventoryComponent.h>
#include "Management/RancInventoryData.h"
#include "LogRancInventory.h"
#include <Engine/AssetManager.h>
#include <Algo/Copy.h>

#include "UObject/SavePackage.h"

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RancInventoryFunctions)
#endif

TMap<FGameplayTag, URancItemData*> URancInventoryFunctions::AllLoadedItemsByTag;
TArray<FGameplayTag> URancInventoryFunctions::AllItemIds;
TArray<URancRecipe*> URancInventoryFunctions::AllLoadedRecipes;

void URancInventoryFunctions::UnloadAllRancItems()
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
	{
		AssetManager->UnloadPrimaryAssetsWithType(FPrimaryAssetType(RancItemDataType));
	}
}

void URancInventoryFunctions::UnloadRancItem(const FPrimaryRancItemId& InItemId)
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

bool URancInventoryFunctions::CompareItemInfo(const FRancItemInstance& Info1, const FRancItemInstance& Info2)
{
	return Info1 == Info2;
}

bool URancInventoryFunctions::CompareItemData(const URancItemData* Data1, const URancItemData* Data2)
{
	return Data1->GetPrimaryAssetId() == Data2->GetPrimaryAssetId();
}

URancItemData* URancInventoryFunctions::GetSingleItemDataById(const FPrimaryRancItemId& InID,
                                                              const TArray<FName>& InBundles, const bool bAutoUnload)
{
	URancItemData* Output = nullptr;

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
			Output = Cast<URancItemData>(StreamableHandle->GetLoadedAsset());
		}
		else // Original author wrote: "-> The object is already loaded"
		{
			// but ive seen it not be loaded, LoadPrimaryAsset and GetPrimaryAssetObject GetPrimaryAssetHandle all return null
			// I didn't manage to fix it but the issue stopped appearing apparently when i added more Items
			const TSharedPtr<FStreamableHandle> StreamableHandleProgress = AssetManager->GetPrimaryAssetHandle(InID);

			if (StreamableHandleProgress.IsValid())
			{
				StreamableHandleProgress->WaitUntilComplete(5.f);
			}

			Output = AssetManager->GetPrimaryAssetObject<URancItemData>(InID);
		}

		if (bAutoUnload)
		{
			AssetManager->UnloadPrimaryAsset(InID);
		}
	}

	return Output;
}

TArray<URancItemData*> URancInventoryFunctions::GetItemDataArrayById(const TArray<FPrimaryRancItemId>& InIDs,
                                                                     const TArray<FName>& InBundles,
                                                                     const bool bAutoUnload)
{
	TArray<URancItemData*> Output;

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

TArray<URancItemData*> URancInventoryFunctions::SearchRancItemData(const ERancItemSearchType SearchType,
                                                                   const FString& SearchString,
                                                                   const TArray<FName>& InBundles,
                                                                   const bool bAutoUnload)
{
	TArray<URancItemData*> Output;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
    if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
	{
		TArray<URancItemData*> ReturnedValues = LoadRancItemData_Internal(
			AssetManager, GetAllRancItemPrimaryIds(), InBundles, bAutoUnload);

		for (URancItemData* const& Iterator : ReturnedValues)
		{
			UE_LOG(LogRancInventory_Internal, Display,
			       TEXT("%s: Filtering items. Current iteration: id %s and name %s"), *FString(__func__),
			       *Iterator->ItemId.ToString(), *Iterator->ItemName.ToString());

			bool bAddItem = false;
			switch (SearchType)
			{
			case ERancItemSearchType::Name:
				bAddItem = Iterator->ItemName.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			case ERancItemSearchType::ID:
				bAddItem = Iterator->ItemId.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			case ERancItemSearchType::Type:
				bAddItem = Iterator->ItemId.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			default:
				break;
			}

			if (bAddItem)
			{
				UE_LOG(LogRancInventory_Internal, Display,
				       TEXT("%s: Item with id %s and name %s matches the search parameters"), *FString(__func__),
				       *Iterator->ItemId.ToString(), *Iterator->ItemName.ToString());

				Output.Add(Iterator);
			}
		}
	}

	return Output;
}


TMap<FGameplayTag, FPrimaryRancItemIdContainer> URancInventoryFunctions::GetItemRelations(
	const FRancItemInstance InItemInfo)
{
	TMap<FGameplayTag, FPrimaryRancItemIdContainer> Output;
	if (const URancItemData* const Data = URancInventoryFunctions::GetItemDataById(InItemInfo.ItemId))
	{
		Output = Data->Relations;
	}

	return Output;
}

TArray<URancItemData*> URancInventoryFunctions::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	TArray<URancItemData*> Output;

	const auto CheckAssetValidity_Lambda = [FuncName = __func__](UObject* const& InAsset) -> bool
	{
		const bool bOutput = IsValid(InAsset);
		if (bOutput)
		{
			UE_LOG(LogRancInventory_Internal, Display, TEXT("%s: Item data %s found and loaded"), *FString(FuncName),
			       *InAsset->GetName());
		}
		else
		{
			UE_LOG(LogRancInventory_Internal, Error, TEXT("%s: Failed to load item data: Invalid Asset"),
			       *FString(FuncName));
		}

		return bOutput;
	};

	const auto PassItemArr_Lambda = [&CheckAssetValidity_Lambda, &Output, FuncName = __func__](TArray<UObject*>& InArr)
	{
		if (URancInventoryFunctions::HasEmptyParam(InArr))
		{
			UE_LOG(LogRancInventory_Internal, Error, TEXT("%s: Failed to find items with the given parameters"),
			       *FString(FuncName));
		}

		for (UObject* const& Iterator : InArr)
		{
			if (!CheckAssetValidity_Lambda(Iterator))
			{
				continue;
			}

			if (URancItemData* const CastedAsset = Cast<URancItemData>(Iterator))
			{
				Output.Add(CastedAsset);
			}
		}
	};

	if (const TSharedPtr<FStreamableHandle> StreamableHandle = InAssetManager->LoadPrimaryAssets(InIDs, InBundles);
		StreamableHandle.IsValid())
	{
		StreamableHandle->WaitUntilComplete(5.f);

		TArray<UObject*> LoadedAssets;
		StreamableHandle->GetLoadedAssets(LoadedAssets);

		PassItemArr_Lambda(LoadedAssets);
	}
	else // Objects already loaded
	{
		if (TArray<UObject*> LoadedAssets;
			InAssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancItemDataType), LoadedAssets))
		{
			PassItemArr_Lambda(LoadedAssets);
		}

		if (!URancInventoryFunctions::HasEmptyParam(Output))
		{
			for (int32 Iterator = 0; Iterator < InIDs.Num(); ++Iterator)
			{
				if (!InIDs.Contains(Output[Iterator]->GetPrimaryAssetId()))
				{
					Output.RemoveAt(Iterator);
					--Iterator;
				}
			}
		}
	}

	if (bAutoUnload)
	{
		// Unload all elementus item assets
		InAssetManager->UnloadPrimaryAssets(InIDs);
	}

	return Output;
}

TArray<URancItemData*> URancInventoryFunctions::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryRancItemId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	const TArray<FPrimaryAssetId> PrimaryAssetIds(InIDs);
	return LoadRancItemData_Internal(InAssetManager, PrimaryAssetIds, InBundles, bAutoUnload);
}

TArray<FRancItemInstance> URancInventoryFunctions::FilterTradeableItems(URancInventoryComponent* FromInventory,
                                                                        URancInventoryComponent* ToInventory,
                                                                        const TArray<FRancItemInstance>& Items)
{
	TArray<FRancItemInstance> Output;
	float VirtualWeight = ToInventory->GetCurrentWeight();

	Algo::CopyIf(Items, Output,
	             [&](const FRancItemInstance& ItemInfo)
	             {
		             if (VirtualWeight >= ToInventory->GetMaxWeight())
		             {
			             return false;
		             }

		             bool bCanTradeIterator = FromInventory->ContainsItems(ItemInfo.ItemId) && ToInventory->
			             CanReceiveItem(ItemInfo);

		             if (bCanTradeIterator)
		             {
			             if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemDataById(
				             ItemInfo.ItemId))
			             {
				             VirtualWeight += ItemInfo.Quantity * ItemData->ItemWeight;
				             bCanTradeIterator = bCanTradeIterator && VirtualWeight <= ToInventory->GetMaxWeight();
			             }
			             else
			             {
				             return false;
			             }
		             }

		             return bCanTradeIterator;
	             }
	);

	return Output;
}



TArray<FGameplayTag> URancInventoryFunctions::GetAllRancItemIds()
{
	return AllItemIds;
}


void URancInventoryFunctions::AllItemsLoadedCallback()
{
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancItemDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				URancItemData* const CastedAsset = Cast<URancItemData>(Iterator);
				AllLoadedItemsByTag.Add(CastedAsset->ItemId, CastedAsset);
			}
		}

		// fill AllItemIds
		AllLoadedItemsByTag.GenerateKeyArray(AllItemIds);
	}
}

void URancInventoryFunctions::PermanentlyLoadAllItemsAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllItemPrimaryIds = GetAllRancItemPrimaryIds();
		AssetManager->LoadPrimaryAssets(AllItemPrimaryIds, TArray<FName>{},
		                                FStreamableDelegate::CreateStatic(AllItemsLoadedCallback));
	}
}

TArray<FPrimaryAssetId> URancInventoryFunctions::GetAllRancItemPrimaryIds()
{
	TArray<FPrimaryAssetId> Output;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
	if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
	{
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(RancItemDataType), Output);
	}

	return Output;
}

bool URancInventoryFunctions::AreAllItemsLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}

URancItemData* URancInventoryFunctions::GetItemDataById(FGameplayTag TagId)
{
	if (const auto* ItemData = AllLoadedItemsByTag.Find(TagId))
	{
		return *ItemData;
	}

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		auto IDToLoad = FPrimaryAssetId(TEXT("RancInventory_ItemData"), *(TagId.ToString()));
		if (const TSharedPtr<FStreamableHandle> StreamableHandle = AssetManager->LoadPrimaryAsset(IDToLoad);
			StreamableHandle.IsValid())
		{
			StreamableHandle->WaitUntilComplete(5.f);
			return Cast<URancItemData>(StreamableHandle->GetLoadedAsset());
		}
	}

	return nullptr;
}

void URancInventoryFunctions::TradeRancItem(TArray<FRancItemInstance> ItemsToTrade,
                                            URancItemContainerComponent* FromInventory,
                                            URancItemContainerComponent* ToInventory)
{
	if (URancInventoryFunctions::HasEmptyParam(ItemsToTrade))
	{
		return;
	}

	// first check if frominventory has the items:
	for (const FRancItemInstance& Iterator : ItemsToTrade)
	{
		if (!FromInventory->ContainsItems(Iterator.ItemId, Iterator.Quantity))
		{
			UE_LOG(LogRancInventory_Internal, Warning,
			       TEXT("TradeRancItem: FromInventory does not contain the item %s"), *Iterator.ItemId.ToString());
			return;
		}
	}


	for (FRancItemInstance& Iterator : ItemsToTrade)
	{
		if (!FromInventory->RemoveItems_IfServer(Iterator))
		{
			UE_LOG(LogRancInventory_Internal, Warning,
			       TEXT("TradeRancItem: Failed to remove item %s from FromInventory"), *Iterator.ItemId.ToString());
			continue;
		}
		ToInventory->AddItems_IfServer(Iterator);
	}
}

bool URancInventoryFunctions::IsItemValid(const FRancItemInstance InItemInfo)
{
	return InItemInfo.ItemId.IsValid() && InItemInfo != FRancItemInstance::EmptyItemInstance && InItemInfo.Quantity > 0;
}


TArray<FPrimaryAssetId> URancInventoryFunctions::GetAllRancItemRecipeIds()
{
	TArray<FPrimaryAssetId> AllItemRecipeIds;
	if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(RancItemRecipeType), AllItemRecipeIds);
	}

	return AllItemRecipeIds;
}

void URancInventoryFunctions::AllRecipesLoadedCallback()
{
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancItemRecipeType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				URancRecipe* const CastedAsset = Cast<URancRecipe>(Iterator);
				AllLoadedRecipes.Add(CastedAsset);
			}
		}
	}
}

void URancInventoryFunctions::HardcodeItem(FGameplayTag ItemId, URancItemData* ItemData)
{
	if (AllLoadedItemsByTag.Contains(ItemId))
	{
		UE_LOG(LogRancInventory_Internal, Warning, TEXT("HardcodeItem: Item with id %s already exists"), *ItemId.ToString());
		return;
	}

	AllLoadedItemsByTag.Add(ItemId, ItemData);
	AllItemIds.Add(ItemId);
}

void URancInventoryFunctions::HardcodeRecipe(FGameplayTag RecipeId, URancRecipe* RecipeData)
{
	if (AllLoadedRecipes.Contains(RecipeData))
	{
		UE_LOG(LogRancInventory_Internal, Warning, TEXT("HardcodeRecipe: Recipe with id %s already exists"), *RecipeId.ToString());
		return;
	}

	AllLoadedRecipes.Add(RecipeData);
}


void URancInventoryFunctions::PermanentlyLoadAllRecipesAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllRecipeIds = GetAllRancItemRecipeIds();
		AssetManager->LoadPrimaryAssets(AllRecipeIds, TArray<FName>{},
		                                FStreamableDelegate::CreateStatic(AllRecipesLoadedCallback));
	}
}

TArray<URancRecipe*> URancInventoryFunctions::GetAllRancItemRecipes()
{
	return AllLoadedRecipes;
}

bool URancInventoryFunctions::AreAllRecipesLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}
