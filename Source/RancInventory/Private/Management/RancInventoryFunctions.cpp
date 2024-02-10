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

bool URancInventoryFunctions::CompareItemInfo(const FRancItemInfo& Info1, const FRancItemInfo& Info2)
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
		Output = LoadRancItemDatas_Internal(AssetManager, InIDs, InBundles, bAutoUnload);
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
		TArray<URancItemData*> ReturnedValues = LoadRancItemDatas_Internal(
			AssetManager, GetAllRancItemIds(), InBundles, bAutoUnload);

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

TMap<FGameplayTag, FName> URancInventoryFunctions::GetItemMetadatas(const FRancItemInfo InItemInfo)
{
	TMap<FGameplayTag, FName> Output;
	if (URancItemData* const Data = GetSingleItemDataById(InItemInfo.ItemId, TArray<FName>{"Custom"}))
	{
		Output = Data->Metadatas;
		UnloadRancItem(InItemInfo.ItemId);
	}

	return Output;
}

TMap<FGameplayTag, FPrimaryRancItemIdContainer> URancInventoryFunctions::GetItemRelations(
	const FRancItemInfo InItemInfo)
{
	TMap<FGameplayTag, FPrimaryRancItemIdContainer> Output;
	if (URancItemData* const Data = GetSingleItemDataById(InItemInfo.ItemId, TArray<FName>{"Custom"}))
	{
		Output = Data->Relations;
		UnloadRancItem(InItemInfo.ItemId);
	}

	return Output;
}

TArray<URancItemData*> URancInventoryFunctions::LoadRancItemDatas_Internal(
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

TArray<URancItemData*> URancInventoryFunctions::LoadRancItemDatas_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryRancItemId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	const TArray<FPrimaryAssetId> PrimaryAssetIds(InIDs);
	return LoadRancItemDatas_Internal(InAssetManager, PrimaryAssetIds, InBundles, bAutoUnload);
}

TArray<FRancItemInfo> URancInventoryFunctions::FilterTradeableItems(URancInventoryComponent* FromInventory,
                                                                    URancInventoryComponent* ToInventory,
                                                                    const TArray<FRancItemInfo>& Items)
{
	TArray<FRancItemInfo> Output;
	float VirtualWeight = ToInventory->GetCurrentWeight();

	Algo::CopyIf(Items, Output,
	             [&](const FRancItemInfo& Iterator)
	             {
		             if (VirtualWeight >= ToInventory->GetMaxWeight())
		             {
			             return false;
		             }

		             bool bCanTradeIterator = FromInventory->ContainsItem(Iterator.ItemId) && ToInventory->
			             CanReceiveItem(Iterator);

		             if (bCanTradeIterator)
		             {
			             if (const URancItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(
				             Iterator.ItemId, {"Data"}))
			             {
				             VirtualWeight += Iterator.Quantity * ItemData->ItemWeight;
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

TArray<FPrimaryAssetId> URancInventoryFunctions::GetAllRancItemIds()
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

void URancInventoryFunctions::AllItemsLoadedCallback()
{
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllItemIds = GetAllRancItemIds();
		
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancItemDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				URancItemData* const CastedAsset = Cast<URancItemData>(Iterator);
				AllLoadedItemsByTag.Add(CastedAsset->ItemId, CastedAsset);
			}
		}

		
	}
}

void URancInventoryFunctions::PermanentlyLoadAllItemsAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllItemIds = GetAllRancItemIds();
		AssetManager->LoadPrimaryAssets(AllItemIds, TArray<FName>{},
		                                FStreamableDelegate::CreateStatic(AllItemsLoadedCallback));
	}
}

bool URancInventoryFunctions::AreAllItemsLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}

URancItemData* URancInventoryFunctions::GetItemById(FGameplayTag TagId)
{
	if (const auto* ItemData = AllLoadedItemsByTag.Find(TagId))
	{
		return *ItemData;
	}

	return nullptr;
}

void URancInventoryFunctions::TradeRancItem(TArray<FRancItemInfo> ItemsToTrade, URancInventoryComponent* FromInventory,
                                            URancInventoryComponent* ToInventory)
{
	if (URancInventoryFunctions::HasEmptyParam(ItemsToTrade))
	{
		return;
	}

	// first check if frominventory has the items:
	for (const FRancItemInfo& Iterator : ItemsToTrade)
	{
		if (!FromInventory->ContainsItem(Iterator.ItemId, Iterator.Quantity))
		{
			UE_LOG(LogRancInventory_Internal, Warning,
			       TEXT("TradeRancItem: FromInventory does not contain the item %s"), *Iterator.ItemId.ToString());
			return;
		}
	}


	for (FRancItemInfo& Iterator : ItemsToTrade)
	{
		if (!FromInventory->RemoveItems(Iterator))
		{
			UE_LOG(LogRancInventory_Internal, Warning,
			       TEXT("TradeRancItem: Failed to remove item %s from FromInventory"), *Iterator.ItemId.ToString());
			continue;
		}
		ToInventory->AddItems(Iterator);
	}
}

bool URancInventoryFunctions::IsItemValid(const FRancItemInfo InItemInfo)
{
	return InItemInfo.ItemId.IsValid() && InItemInfo != FRancItemInfo::EmptyItemInfo && InItemInfo.Quantity > 0;
}

bool URancInventoryFunctions::IsItemStackable(const FRancItemInfo InItemInfo)
{
	if (!IsItemValid(InItemInfo))
	{
		return false;
	}

	if (const URancItemData* const ItemData = GetSingleItemDataById(InItemInfo.ItemId, {"Data"}))
	{
		return ItemData->bIsStackable;
	}

	return true;
}

FGameplayTagContainer URancInventoryFunctions::GetItemTagsWithParentTag(const FRancItemInfo InItemInfo,
                                                                        const FGameplayTag FromParentTag)
{
	FGameplayTagContainer Output;
	for (const FGameplayTag& Iterator : InItemInfo.Tags)
	{
		if (Iterator.MatchesTag(FromParentTag))
		{
			Output.AddTag(Iterator);
		}
	}

	return Output;
}

FString URancInventoryFunctions::RancItemEnumTypeToString(const ERancItemType InEnumName)
{
	switch (InEnumName)
	{
	case ERancItemType::None:
		return "None";

	case ERancItemType::Consumable:
		return "Consumable";

	case ERancItemType::Armor:
		return "Armor";

	case ERancItemType::Weapon:
		return "Weapon";

	case ERancItemType::Accessory:
		return "Accessory";

	case ERancItemType::Crafting:
		return "Crafting";

	case ERancItemType::Material:
		return "Material";

	case ERancItemType::Information:
		return "Information";

	case ERancItemType::Special:
		return "Special";

	case ERancItemType::Event:
		return "Event";

	case ERancItemType::Quest:
		return "Quest";

	case ERancItemType::Junk:
		return "Junk";

	case ERancItemType::Other:
		return "Other";

	default:
		break;
	}

	return FString();
}
