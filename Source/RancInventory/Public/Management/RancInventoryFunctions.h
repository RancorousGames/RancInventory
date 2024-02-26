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
    static bool CompareItemInfo(const FRancItemInstance& Info1, const FRancItemInstance& Info2);

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
    static TArray<FPrimaryAssetId> GetAllRancItemPrimaryIds();
    
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TArray<FGameplayTag> GetAllRancItemIds();

    
    /* Loads all item data assets, removing the need for GetSingleItemDataById and instead allowing use of GetItemById */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void PermanentlyLoadAllItemsAsync();
    
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool AreAllItemsLoaded();
    
    /* Uses static map from GameplayTag to URancItemData, works faster after having called PermanentlyLoadAllItems */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static URancItemData* GetItemDataById(FGameplayTag TagId);
    
    /* Trade items between two inventory components */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void TradeRancItem(TArray<FRancItemInstance> ItemsToTrade, URancItemContainerComponent* FromInventory, URancItemContainerComponent* ToInventory);

    // Utiltiy function to check if moving an item from one slot to another would cause a swap
    static bool ShouldItemsBeSwapped(FRancItemInstance* Source, FRancItemInstance* Target);

    
    /* Moves from a source ItemInstance to a target one, either moving, stacking stackable items or swapping
     * Not exposed to blueprint directly as its a utility function for other inventory classes
     * IgnoreMaxStacks will allow a target slot to go above the item datas maxstacksize (used for itemcontainer)
     * AllowPartial if enabled will allow a move to partially succeed, e.g. only move 2 of requested 3 quantity, if false moves full or nothing
     */
    static int32 MoveBetweenSlots(FRancItemInstance* Source, FRancItemInstance* Target, bool IgnoreMaxStacks, int32 RequestedQuantity, bool AllowPartial);


    /* Check if the given item info have a valid id */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool IsItemValid(const FRancItemInstance InItemInfo);
    
    /* Loads all item recipe data assets, allowing use of GetItemById */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void PermanentlyLoadAllRecipesAsync();
    
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static bool AreAllRecipesLoaded();
    
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static TArray<URancRecipe*> GetAllRancItemRecipes();

    /* Includes both RancItemRecipe and RancItemCraftingRecipe (for item to item) */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TArray<FPrimaryAssetId> GetAllRancItemRecipeIds();
    
    
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
    static TMap<FGameplayTag, FPrimaryRancItemIdContainer> GetItemRelations(const FRancItemInstance InItemInfo);

    static void AllItemsLoadedCallback();
    static void AllRecipesLoadedCallback();

    // Below are used for e.g. unit tests
    static void HardcodeItem(FGameplayTag ItemId, URancItemData* ItemData);
    static void HardcodeRecipe(FGameplayTag RecipeId, URancRecipe* RecipeData);

private:
    static TArray<URancItemData*> LoadRancItemData_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);
    static TArray<URancItemData*> LoadRancItemData_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryRancItemId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);

    
    static TMap<FGameplayTag, URancItemData*> AllLoadedItemsByTag;
   // static TMap<FGameplayTag, FName> AllLoadedItemAssetNamesByTag;
    static TArray<FGameplayTag> AllItemIds;
    
    static TArray<URancRecipe*> AllLoadedRecipes;

public:
    /* Filter the container and return only items that can be traded at the current context */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static TArray<FRancItemInstance> FilterTradeableItems(URancInventoryComponent* FromInventory, URancInventoryComponent* ToInventory, const TArray<FRancItemInstance>& Items);
};
