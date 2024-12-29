// Copyright Rancorous Games, 2024

#include "Management/RISFunctions.h"
#include "Management/RISInventoryData.h"
#include "Components/InventoryComponent.h"
#include "LogRancInventorySystem.h"
#include <Engine/AssetManager.h>
#include <Algo/Copy.h>

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RISInventoryFunctions)
#endif

TMap<FGameplayTag, URISItemData*> URISFunctions::AllLoadedItemsByTag;
TArray<FGameplayTag> URISFunctions::AllItemIds;
TArray<UObjectRecipeData*> URISFunctions::AllLoadedRecipes;

void URISFunctions::UnloadAllRancItems()
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

void URISFunctions::UnloadRancItem(const FPrimaryRISItemId& InItemId)
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

bool URISFunctions::CompareItemInfo(const FItemBundle& Info1, const FItemBundle& Info2)
{
	return Info1 == Info2;
}

bool URISFunctions::CompareItemData(const URISItemData* Data1, const URISItemData* Data2)
{
	return Data1->GetPrimaryAssetId() == Data2->GetPrimaryAssetId();
}

URISItemData* URISFunctions::GetSingleItemDataById(const FPrimaryRISItemId& InID,
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
		else // Original author wrote: "-> The object is already loaded"
		{
			// but ive seen it not be loaded, LoadPrimaryAsset and GetPrimaryAssetObject GetPrimaryAssetHandle all return null
			// I didn't manage to fix it but the issue stopped appearing apparently when i added more Items
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

TArray<URISItemData*> URISFunctions::GetItemDataArrayById(const TArray<FPrimaryRISItemId>& InIDs,
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

TArray<URISItemData*> URISFunctions::SearchRancItemData(const ERISItemSearchType SearchType,
                                                                   const FString& SearchString,
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
		TArray<URISItemData*> ReturnedValues = LoadRancItemData_Internal(
			AssetManager, GetAllRancItemPrimaryIds(), InBundles, bAutoUnload);

		for (URISItemData* const& Iterator : ReturnedValues)
		{
			UE_LOG(LogRISInventory, Display,
			       TEXT("%s: Filtering items. Current iteration: id %s and name %s"), *FString(__func__),
			       *Iterator->ItemId.ToString(), *Iterator->ItemName.ToString());

			bool bAddItem = false;
			switch (SearchType)
			{
			case ERISItemSearchType::Name:
				bAddItem = Iterator->ItemName.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			case ERISItemSearchType::ID:
				bAddItem = Iterator->ItemId.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			case ERISItemSearchType::Type:
				bAddItem = Iterator->ItemId.ToString().Contains(SearchString, ESearchCase::IgnoreCase);
				break;

			default:
				break;
			}

			if (bAddItem)
			{
				UE_LOG(LogRISInventory, Display,
				       TEXT("%s: Item with id %s and name %s matches the search parameters"), *FString(__func__),
				       *Iterator->ItemId.ToString(), *Iterator->ItemName.ToString());

				Output.Add(Iterator);
			}
		}
	}

	return Output;
}


TMap<FGameplayTag, FPrimaryRISItemIdContainer> URISFunctions::GetItemRelations(
	const FItemBundle InItemInfo)
{
	TMap<FGameplayTag, FPrimaryRISItemIdContainer> Output;
	if (const URISItemData* const Data = URISFunctions::GetItemDataById(InItemInfo.ItemId))
	{
		Output = Data->Relations;
	}

	return Output;
}

TArray<URISItemData*> URISFunctions::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	TArray<URISItemData*> Output;

	const auto CheckAssetValidity_Lambda = [FuncName = __func__](UObject* const& InAsset) -> bool
	{
		const bool bOutput = IsValid(InAsset);
		if (bOutput)
		{
			UE_LOG(LogRISInventory, Display, TEXT("%s: Item data %s found and loaded"), *FString(FuncName),
			       *InAsset->GetName());
		}
		else
		{
			UE_LOG(LogRISInventory, Error, TEXT("%s: Failed to load item data: Invalid Asset"),
			       *FString(FuncName));
		}

		return bOutput;
	};

	const auto PassItemArr_Lambda = [&CheckAssetValidity_Lambda, &Output, FuncName = __func__](TArray<UObject*>& InArr)
	{
		if (URISFunctions::HasEmptyParam(InArr))
		{
			UE_LOG(LogRISInventory, Error, TEXT("%s: Failed to find items with the given parameters"),
			       *FString(FuncName));
		}

		for (UObject* const& Iterator : InArr)
		{
			if (!CheckAssetValidity_Lambda(Iterator))
			{
				continue;
			}

			if (URISItemData* const CastedAsset = Cast<URISItemData>(Iterator))
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
			InAssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancInventoryItemDataType), LoadedAssets))
		{
			PassItemArr_Lambda(LoadedAssets);
		}

		if (!URISFunctions::HasEmptyParam(Output))
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

TArray<URISItemData*> URISFunctions::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryRISItemId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	const TArray<FPrimaryAssetId> PrimaryAssetIds(InIDs);
	return LoadRancItemData_Internal(InAssetManager, PrimaryAssetIds, InBundles, bAutoUnload);
}

TArray<FItemBundle> URISFunctions::FilterTradeableItems(UInventoryComponent* FromInventory,
                                                                        UInventoryComponent* ToInventory,
                                                                        const TArray<FItemBundle>& Items)
{
	TArray<FItemBundle> Output;
	float VirtualWeight = ToInventory->GetCurrentWeight();

	Algo::CopyIf(Items, Output,
	             [&](const FItemBundle& ItemInfo)
	             {
		             if (VirtualWeight >= ToInventory->GetMaxWeight())
		             {
			             return false;
		             }

		             bool bCanTradeIterator = FromInventory->DoesContainerContainItems(ItemInfo.ItemId) && ToInventory->
			             CanContainerReceiveItems(ItemInfo);

		             if (bCanTradeIterator)
		             {
			             if (const URISItemData* const ItemData = URISFunctions::GetItemDataById(
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


TArray<FGameplayTag> URISFunctions::GetAllRancItemIds()
{
	return AllItemIds;
}


void URISFunctions::AllItemsLoadedCallback()
{
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancInventoryItemDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				URISItemData* const CastedAsset = Cast<URISItemData>(Iterator);
				AllLoadedItemsByTag.Add(CastedAsset->ItemId, CastedAsset);
			}
		}

		// fill AllItemIds
		AllLoadedItemsByTag.GenerateKeyArray(AllItemIds);
	}
}

void URISFunctions::PermanentlyLoadAllItemsAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllItemPrimaryIds = GetAllRancItemPrimaryIds();
		AssetManager->LoadPrimaryAssets(AllItemPrimaryIds, TArray<FName>{},
		                                FStreamableDelegate::CreateStatic(AllItemsLoadedCallback));
	}
}

TArray<FPrimaryAssetId> URISFunctions::GetAllRancItemPrimaryIds()
{
	TArray<FPrimaryAssetId> Output;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
#else
	if (UAssetManager* const AssetManager = UAssetManager::GetIfValid())
#endif
	{
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(RancInventoryItemDataType), Output);
	}

	return Output;
}

bool URISFunctions::AreAllItemsLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}

URISItemData* URISFunctions::GetItemDataById(FGameplayTag TagId)
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
			return Cast<URISItemData>(StreamableHandle->GetLoadedAsset());
		}
	}

	return nullptr;
}

void URISFunctions::TradeRancItem(TArray<FItemBundle> ItemsToTrade,
                                            UItemContainerComponent* FromInventory,
                                            UItemContainerComponent* ToInventory)
{
	if (URISFunctions::HasEmptyParam(ItemsToTrade))
	{
		return;
	}

	// first check if frominventory has the items:
	for (const FItemBundle& Iterator : ItemsToTrade)
	{
		if (!FromInventory->DoesContainerContainItems(Iterator.ItemId, Iterator.Quantity))
		{
			UE_LOG(LogRISInventory, Warning,
			       TEXT("TradeRancItem: FromInventory does not contain the item %s"), *Iterator.ItemId.ToString());
			return;
		}
	}


	for (FItemBundle& Iterator : ItemsToTrade)
	{
		if (!FromInventory->RemoveItems_IfServer(Iterator))
		{
			UE_LOG(LogRISInventory, Warning,
			       TEXT("TradeRancItem: Failed to remove item %s from FromInventory"), *Iterator.ItemId.ToString());
			continue;
		}
		ToInventory->AddItems_IfServer(Iterator);
	}
}

bool URISFunctions::ShouldItemsBeSwapped(FItemBundle* Source, FItemBundle* Target)
{
	// This code is copied and slightly modified from MoveBetweenSlots
	if (Target->IsValid())
	{
		const URISItemData* SourceItemData = URISFunctions::GetItemDataById(Source->ItemId);
		if (!SourceItemData)
		{
			return false;
		}
		
		const bool ShouldStack = SourceItemData->bIsStackable && Source->ItemId == Target->ItemId;
		return !ShouldStack;
	}

	return false;
}

int32 URISFunctions::MoveBetweenSlots(FItemBundle* Source, FItemBundle* Target, bool IgnoreMaxStacks, int32 RequestedQuantity, bool AllowPartial)
{
	const URISItemData* SourceItemData = URISFunctions::GetItemDataById(Source->ItemId);
	if (!SourceItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to retrieve item data for source item"));
		return 0;
	}
	
	if (!AllowPartial && RequestedQuantity > Source->Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("AllowPartial set to false, can't move more than is contained."));
		return 0;
	}
		
	int32 TransferAmount = FMath::Min(RequestedQuantity, Source->Quantity);

	bool DoSimpleSwap = false;
	
	if (Target->IsValid())
	{
		const bool ShouldStack = SourceItemData->bIsStackable && Source->ItemId == Target->ItemId;
		if (!ShouldStack && Source->Quantity > RequestedQuantity)
		{
			UE_LOG(LogTemp, Warning, TEXT("Not possible to split source slot to a occupied slot with a different item."));
			return 0;
		}

		const int32 RemainingSpace = IgnoreMaxStacks || !ShouldStack ? TransferAmount : SourceItemData->MaxStackSize - Target->Quantity;
		TransferAmount = FMath::Min(TransferAmount, RemainingSpace);

		DoSimpleSwap = !ShouldStack;
	}
	else
	{
		DoSimpleSwap = TransferAmount >= Source->Quantity;
	}

	if (TransferAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Possible transfer amount was 0"));
		return 0;
	}
	
	if (!AllowPartial && TransferAmount < RequestedQuantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("AllowPartial set to false, and could not move the full requested amount"));
		return 0;
	}


	if (DoSimpleSwap)
	{
		const FItemBundle Temp = *Source;
		*Source = *Target; // Target might be invalid but that's fine
		*Target = Temp;
	}
	else
	{
		Target->ItemId = Source->ItemId;
		Target->Quantity += TransferAmount;
		Source->Quantity -= TransferAmount;
		if (Source->Quantity <= 0)
			*Source = FItemBundle::EmptyItemInstance;
	}

	return TransferAmount;
}

bool URISFunctions::IsItemValid(const FItemBundle InItemInfo)
{
	return InItemInfo.ItemId.IsValid() && InItemInfo != FItemBundle::EmptyItemInstance && InItemInfo.Quantity > 0;
}


TArray<FPrimaryAssetId> URISFunctions::GetAllRisItemRecipeIds()
{
	TArray<FPrimaryAssetId> AllItemRecipeIds;
	if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(RancInventoryRecipeDataType), AllItemRecipeIds);
	}

	return AllItemRecipeIds;
}

void URISFunctions::AllRecipesLoadedCallback()
{
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancInventoryRecipeDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				UObjectRecipeData* const CastedAsset = Cast<UObjectRecipeData>(Iterator);
				AllLoadedRecipes.Add(CastedAsset);
			}
		}
	}
}

void URISFunctions::HardcodeItem(FGameplayTag ItemId, URISItemData* ItemData)
{
	if (AllLoadedItemsByTag.Contains(ItemId))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("HardcodeItem: Item with id %s already exists"), *ItemId.ToString());
		return;
	}

	AllLoadedItemsByTag.Add(ItemId, ItemData);
	AllItemIds.Add(ItemId);
}

void URISFunctions::HardcodeRecipe(FGameplayTag RecipeId, UObjectRecipeData* RecipeData)
{
	if (AllLoadedRecipes.Contains(RecipeData))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("HardcodeRecipe: Recipe with id %s already exists"), *RecipeId.ToString());
		return;
	}

	AllLoadedRecipes.Add(RecipeData);
}


void URISFunctions::PermanentlyLoadAllRecipesAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllRecipeIds = GetAllRisItemRecipeIds();
		AssetManager->LoadPrimaryAssets(AllRecipeIds, TArray<FName>{},
		                                FStreamableDelegate::CreateStatic(AllRecipesLoadedCallback));
	}
}

TArray<UObjectRecipeData*> URISFunctions::GetAllRISItemRecipes()
{
	return AllLoadedRecipes;
}

bool URISFunctions::AreAllRISRecipesLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}
