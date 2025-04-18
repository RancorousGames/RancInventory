// Copyright Rancorous Games, 2024

#include "Core/RISSubsystem.h"

#include "LogRancInventorySystem.h"
#include "Components/ItemContainerComponent.h"
#include "Engine/StreamableManager.h"
#include "Core/RISFunctions.h"
#include "Data/ItemStaticData.h"
#include "Data/RecipeData.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// Initialize static variables
TMap<FGameplayTag, UItemStaticData*> URISSubsystem::AllLoadedItemsByTag;
TArray<FGameplayTag> URISSubsystem::AllItemIds;
TArray<UObjectRecipeData*> URISSubsystem::AllLoadedRecipes;

URISSubsystem::URISSubsystem()
{
	
}

URISSubsystem* URISSubsystem::Get(UObject* WorldContext)
{
	if (!WorldContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldContext is null in URISSubsystem::Get."));
		return nullptr;
	}

	auto* World = WorldContext->GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		if (!World->IsEditorWorld())
			UE_LOG(LogTemp, Warning, TEXT("GameInstance is null in URISSubsystem::Get."));
		return nullptr;
	}

	// Get the subsystem; if it doesn't exist, Unreal Engine will handle the creation automatically
	URISSubsystem* Subsystem = GameInstance->GetSubsystem<URISSubsystem>();

	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("URISSubsystem is not found, but it should have been automatically created."));
	}

	return Subsystem;
}

void URISSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	AllRecipesLoadedBroadcasted = false;
}

void URISSubsystem::PermanentlyLoadAllItemsAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllItemPrimaryIds = GetAllRancItemPrimaryIds();
		AssetManager->LoadPrimaryAssets(AllItemPrimaryIds, TArray<FName>{},
		                                FStreamableDelegate::CreateUObject(this, &URISSubsystem::AllItemsLoadedCallback));
	}
}


void URISSubsystem::AllItemsLoadedCallback()
{
	if (AllItemsLoadedBroadcasted)
		return;
	
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancInventoryItemDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				UItemStaticData* const CastedAsset = Cast<UItemStaticData>(Iterator);
				AllLoadedItemsByTag.Add(CastedAsset->ItemId, CastedAsset);
				LoadedItemsHeldRefs.Add(CastedAsset);
			}
		}

		// fill AllItemIds
		AllLoadedItemsByTag.GenerateKeyArray(AllItemIds);
		AllItemsLoadedBroadcasted = true;
		OnAllItemsLoaded.Broadcast();
	}
}


TArray<FPrimaryAssetId> URISSubsystem::GetAllRancItemPrimaryIds()
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

bool URISSubsystem::AreAllItemsLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}


void URISSubsystem::UnloadAllRISItems()
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

void URISSubsystem::UnloadRISItem(const FPrimaryRISItemId& InItemId)
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

UItemStaticData* URISSubsystem::GetSingleItemDataById(const FPrimaryRISItemId& InID,
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
		else
		{
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


TArray<UItemStaticData*> URISSubsystem::GetItemDataArrayById(const TArray<FPrimaryRISItemId>& InIDs,
                                                             const TArray<FName>& InBundles,
                                                             const bool bAutoUnload)
{
	TArray<UItemStaticData*> Output;

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

int32 URISSubsystem::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity,
                                                         EItemChangeReason Reason,
                                                         TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	return Quantity;
}

int32 URISSubsystem::GetContainedQuantity_Implementation(const FGameplayTag& ItemId)
{
	return MAX_int32;
}

TArray<UItemStaticData*> URISSubsystem::LoadRancItemData_Internal(
	UAssetManager* InAssetManager, const TArray<FPrimaryRISItemId>& InIDs, const TArray<FName>& InBundles,
	const bool bAutoUnload)
{
	TArray<UItemStaticData*> LoadedItems;

	for (const FPrimaryRISItemId& ItemId : InIDs)
	{
		UItemStaticData* ItemData = GetSingleItemDataById(ItemId, InBundles, bAutoUnload);
		if (ItemData)
		{
			LoadedItems.Add(ItemData);
		}
	}

	return LoadedItems;
}

TArray<FPrimaryAssetId> URISSubsystem::GetAllRISItemRecipeIds()
{
	TArray<FPrimaryAssetId> AllItemRecipeIds;
	if (const UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		AssetManager->GetPrimaryAssetIdList(FPrimaryAssetType(RancInventoryRecipeDataType), AllItemRecipeIds);
	}

	return AllItemRecipeIds;
}

void URISSubsystem::AllRecipesLoadedCallback()
{
	if (AllRecipesLoadedBroadcasted)
		return;
	
	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<UObject*> LoadedAssets;
		if (AssetManager->GetPrimaryAssetObjectList(FPrimaryAssetType(RancInventoryRecipeDataType), LoadedAssets))
		{
			for (UObject* const& Iterator : LoadedAssets)
			{
				UObjectRecipeData* const CastedAsset = Cast<UObjectRecipeData>(Iterator);
				AllLoadedRecipes.Add(CastedAsset);
				LoadedRecipesHeldRefs.Add(CastedAsset);
			}
		}

		AllRecipesLoadedBroadcasted = true;
		OnAllRecipesLoaded.Broadcast();
	}
}

void URISSubsystem::HardcodeItem(FGameplayTag ItemId, UItemStaticData* ItemData)
{
	if (AllLoadedItemsByTag.Contains(ItemId))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("HardcodeItem: Item with id %s already exists"), *ItemId.ToString());
		return;
	}

	AllLoadedItemsByTag.Add(ItemId, ItemData);
	AllItemIds.Add(ItemId);
}

void URISSubsystem::HardcodeRecipe(FGameplayTag RecipeId, UObjectRecipeData* RecipeData)
{
	if (AllLoadedRecipes.Contains(RecipeData))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("HardcodeRecipe: Recipe with id %s already exists"),
		       *RecipeId.ToString());
		return;
	}

	AllLoadedRecipes.Add(RecipeData);
}


void URISSubsystem::PermanentlyLoadAllRecipesAsync()
{
	if (AllLoadedItemsByTag.Num() > 0) return;

	if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	{
		const TArray<FPrimaryAssetId> AllRecipeIds = GetAllRISItemRecipeIds();
		AssetManager->LoadPrimaryAssets(AllRecipeIds, TArray<FName>{},
		                                FStreamableDelegate::CreateUObject(this, &URISSubsystem::AllRecipesLoadedCallback));
	}
}

TArray<UObjectRecipeData*> URISSubsystem::GetAllRISItemRecipes()
{
	return AllLoadedRecipes;
}

UItemStaticData* URISSubsystem::GetItemDataById(FGameplayTag TagId)
{
	UItemStaticData* FoundItem = AllLoadedItemsByTag.FindRef(TagId);

	if (!FoundItem)
	{
		if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
		{
			auto IDToLoad = FPrimaryAssetId(TEXT("RancInventory_ItemData"), *(TagId.ToString()));
			if (const TSharedPtr<FStreamableHandle> StreamableHandle = AssetManager->LoadPrimaryAsset(IDToLoad);
				StreamableHandle.IsValid())
			{
				StreamableHandle->WaitUntilComplete(5.f);
				return Cast<UItemStaticData>(StreamableHandle->GetLoadedAsset());
			}
		}
		return nullptr;
	}

	return FoundItem;
}

bool URISSubsystem::AreAllRecipesLoaded()
{
	return AllLoadedItemsByTag.Num() > 0;
}

TArray<FGameplayTag> URISSubsystem::GetAllRISItemIds()
{
	return AllItemIds;
}

TArray<FPrimaryAssetId> URISSubsystem::GetAllRISItemPrimaryIds()
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

TSubclassOf<AWorldItem> URISSubsystem::GetWorldItemClass(const FGameplayTag& ItemId,
    TSubclassOf<AWorldItem> DefaultClass) const
{
    if (UItemStaticData* ItemData = AllLoadedItemsByTag.FindRef(ItemId))
    {
        if (ItemData->WorldItemClassOverride)
        {
            return ItemData->WorldItemClassOverride;
        }
    }
    return DefaultClass;
}

AWorldItem* URISSubsystem::SpawnWorldItem(UObject* WorldContextObject,
    FItemBundleWithInstanceData Item,
    const FVector& Location,
    TSubclassOf<AWorldItem> WorldItemClass)
{
	auto* World = WorldContextObject->GetWorld();

	if (WorldItemClass == nullptr)
	{
		WorldItemClass = World->GetFirstPlayerController()->GetPawn()->GetComponentByClass<UItemContainerComponent>()->DropItemClass;
	}
	
    if (!World || !WorldItemClass || !Item.ItemId.IsValid())
    {
        UE_LOG(LogRISInventory, Warning, TEXT("SpawnWorldItem: Invalid parameters provided"));
        return nullptr;
    }

    // Get the item data
    UItemStaticData* ItemData = AllLoadedItemsByTag.FindRef(Item.ItemId);
    if (!ItemData)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("SpawnWorldItem: Could not find item data for ID: %s"), 
            *Item.ItemId.ToString());
        return nullptr;
    }

    // Get the appropriate world item class (either default or override)
    TSubclassOf<AWorldItem> FinalWorldItemClass = GetWorldItemClass(Item.ItemId, WorldItemClass);

    // Spawn parameters
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Spawn the actor
    AWorldItem* WorldItem = World->SpawnActor<AWorldItem>(FinalWorldItemClass, Location, 
        FRotator::ZeroRotator, SpawnParams);
    
    if (WorldItem)
    {
        // Set up the world item
        WorldItem->SetItem(Item);
        // TODO
        // WorldItem->SetInstanceData(InstanceData);
    }
    else
    {
        UE_LOG(LogRISInventory, Warning, TEXT("SpawnWorldItem: Failed to spawn world item for ID: %s"), 
            *Item.ItemId.ToString());
    }

    return WorldItem;
}
