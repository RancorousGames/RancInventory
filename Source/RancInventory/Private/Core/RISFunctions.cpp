// Copyright Rancorous Games, 2024

#include "Core/RISFunctions.h"
#include "Components/InventoryComponent.h"
#include "LogRancInventorySystem.h"
#include <Engine/AssetManager.h>
#include <Algo/Copy.h>

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


bool URISFunctions::CompareItemData(const UItemStaticData* Data1, const UItemStaticData* Data2)
{
	return Data1->GetPrimaryAssetId() == Data2->GetPrimaryAssetId();
}

UItemStaticData* URISFunctions::GetSingleItemDataById(const FPrimaryRISItemId& InID,
                                                              const TArray<FName>& InBundles, const bool bAutoUnload)
{
	UItemStaticData* Output = nullptr;

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
			Output = Cast<UItemStaticData>(StreamableHandle->GetLoadedAsset());
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

			Output = AssetManager->GetPrimaryAssetObject<UItemStaticData>(InID);
		}

		if (bAutoUnload)
		{
			AssetManager->UnloadPrimaryAsset(InID);
		}
	}

	return Output;
}

UItemStaticData* URISFunctions::GetItemDataById(FGameplayTag ItemId)
{
	return URISSubsystem::GetItemDataById(ItemId);
}

TArray<UItemStaticData*> URISFunctions::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	TArray<UItemStaticData*> Output;

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

			if (UItemStaticData* const CastedAsset = Cast<UItemStaticData>(Iterator))
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


bool URISFunctions::ShouldItemsBeSwapped(const FGameplayTag& Source, const FGameplayTag& Target)
{
	// This code is copied and slightly modified from MoveBetweenSlots
	if (Target.IsValid())
	{
		const UItemStaticData* SourceItemData = URISSubsystem::GetItemDataById(Source);
		if (!SourceItemData)
		{
			return false;
		}
		
		const bool ShouldStack = SourceItemData->MaxStackSize > 1 && Source == Target;
		return !ShouldStack;
	}

	return false;
}

FRISMoveResult URISFunctions::MoveBetweenSlots(FGenericItemBundle& Source, FGenericItemBundle& Target, bool IgnoreMaxStacks, int32 RequestedQuantity, bool AllowPartial, bool AllowSwap)
{
	const UItemStaticData* SourceItemData = URISSubsystem::GetItemDataById(Source.GetItemId());
	if (!SourceItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to retrieve item data for source item"));
		return FRISMoveResult(0, false);
	}
	
	if (!AllowPartial && RequestedQuantity > Source.GetQuantity())
	{
		UE_LOG(LogTemp, Warning, TEXT("AllowPartial set to false, can't move more than is contained."));
		return FRISMoveResult(0, false);
	}
		
	int32 TransferAmount = FMath::Min(RequestedQuantity, Source.GetQuantity());

	bool DoSimpleSwap = false;
	
	if (Target.IsValid())
	{
		const bool ShouldStack = SourceItemData->MaxStackSize > 1 && Source.GetItemId()== Target.GetItemId();
		if (!ShouldStack && Source.GetQuantity() > RequestedQuantity)
		{
			UE_LOG(LogTemp, Warning, TEXT("Not possible to split source slot to a occupied slot with a different item."));
			return FRISMoveResult(0, false);
		}

		const int32 RemainingSpace = IgnoreMaxStacks || !ShouldStack ? TransferAmount : SourceItemData->MaxStackSize - Target.GetQuantity();
		TransferAmount = FMath::Min(TransferAmount, RemainingSpace);

		DoSimpleSwap = !ShouldStack && AllowSwap;
	}
	else
	{
		DoSimpleSwap = TransferAmount >= Source.GetQuantity();
	}

	if (TransferAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Possible transfer amount was 0"));
		return FRISMoveResult(0, false);
	}
	
	if (!AllowPartial && TransferAmount < RequestedQuantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("AllowPartial set to false, and could not move the full requested amount"));
		return FRISMoveResult(0, false);
	}


	if (DoSimpleSwap)
	{
		const FGameplayTag TempId = Source.GetItemId();
		const int32 TempQuantity = Source.GetQuantity();
		Source.SetItemId(Target.GetItemId());  // Target might be invalid but that's fine
		Source.SetQuantity(Target.GetQuantity());
		Target.SetItemId(TempId);
		Target.SetQuantity(TempQuantity);
		
		return FRISMoveResult(TransferAmount, Source.IsValid());
	}

	Target.SetItemId(Source.GetItemId());
	Target.SetQuantity(Target.GetQuantity() + TransferAmount);
	Source.SetQuantity(Source.GetQuantity() - TransferAmount);
	if (Source.GetQuantity() <= 0)
	{
		Source.SetItemId(FItemBundle::EmptyItemInstance.ItemId);
		Source.SetQuantity(FItemBundle::EmptyItemInstance.Quantity);
	}
	return FRISMoveResult(TransferAmount, false);
}
