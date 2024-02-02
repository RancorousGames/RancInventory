// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>
#include <Kismet/BlueprintFunctionLibrary.h>
#include <Runtime/Launch/Resources/Version.h>
#include "RancInventoryFunctions.generated.h"

UENUM(BlueprintType, Category = "Ranc Inventory | Enumerations")
enum class ERancItemSearchType : uint8
{
    Name,
    ID,
    Type
};

class URancInventoryComponent;
class UAssetManager;
class URancItemData;
struct FPrimaryRancItemId;

/**
 *
 */
UCLASS(Category = "Ranc Inventory | Functions")
class RANCINVENTORY_API URancInventoryFunctions final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /* Unload all items that were loaded by Asset Manager */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void UnloadAllRancItems();

    /* Unload a item that was loaded by Asset Manager */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void UnloadRancItem(const FPrimaryRancItemId& InItemId);

    /* Check if the ids are equal */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool CompareItemInfo(const FRancItemInfo& Info1, const FRancItemInfo& Info2);

    /* Check if the ids of the given item datas are equal */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool CompareItemData(const URancItemData* Data1, const URancItemData* Data2);

    /* Return the item data related to the given id */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static URancItemData* GetSingleItemDataById(const FPrimaryRancItemId& InID, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    /* Return a array of data depending of the given ids */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static TArray<URancItemData*> GetItemDataArrayById(const TArray<FPrimaryRancItemId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    /* Search all registered items and return a array of item data that match with the given parameters */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static TArray<URancItemData*> SearchRancItemData(const ERancItemSearchType SearchType, const FString& SearchString, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    /* Get the primary asset ids of all registered items */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TArray<FPrimaryAssetId> GetAllRancItemIds();

    /* Trade items between two inventory components */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void TradeRancItem(TArray<FRancItemInfo> ItemsToTrade, URancInventoryComponent* FromInventory, URancInventoryComponent* ToInventory);

    /* Check if the given item info have a valid id */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool IsItemValid(const FRancItemInfo InItemInfo);

    /* Check if the given item info represents a stackable item */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool IsItemStackable(const FRancItemInfo InItemInfo);

    /* Get item tags providing a parent tag */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static FGameplayTagContainer GetItemTagsWithParentTag(const FRancItemInfo InItemInfo, const FGameplayTag FromParentTag);

    /* Convert an item type enum value to string */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static FString RancItemEnumTypeToString(const ERancItemType InEnumName);

    template<typename Ty>
    constexpr static const bool HasEmptyParam(const Ty& Arg1)
    {
        if constexpr (std::is_base_of<FString, Ty>())
        {
            return Arg1.IsEmpty();
        }
        else if constexpr (std::is_base_of<FName, Ty>())
        {
            return Arg1.IsNone();
        }
        else
        {
#if ENGINE_MAJOR_VERSION >= 5
            return Arg1.IsEmpty();
#else
            return Arg1.Num() == 0;
#endif
        }
    }

    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TMap<FGameplayTag, FName> GetItemMetadatas(const FRancItemInfo InItemInfo);

    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TMap<FGameplayTag, FPrimaryRancItemIdContainer> GetItemRelations(const FRancItemInfo InItemInfo);

private:
    static TArray<URancItemData*> LoadRancItemDatas_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);
    static TArray<URancItemData*> LoadRancItemDatas_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryRancItemId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);

public:
    /* Filter the container and return only items that can be traded at the current context */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TArray<FRancItemInfo> FilterTradeableItems(URancInventoryComponent* FromInventory, URancInventoryComponent* ToInventory, const TArray<FRancItemInfo>& Items);
};
