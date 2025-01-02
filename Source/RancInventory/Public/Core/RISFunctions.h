// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <Kismet/BlueprintFunctionLibrary.h>
#include <Runtime/Launch/Resources/Version.h>
#include "RISFunctions.generated.h"

UENUM(BlueprintType, Category = "Ranc Inventory | Enumerations")
enum class ERISItemSearchType : uint8
{
    Name,
    ID,
    Type
};

class UInventoryComponent;
class UAssetManager;
class UItemStaticData;
struct FPrimaryRISItemId;

/**
 * Utility functions for the Ranc Inventory System
 */
UCLASS(Category = "Ranc Inventory | Functions")
class RANCINVENTORY_API URISFunctions final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /* Unload all items that were loaded by Asset Manager */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void UnloadAllRancItems();

    /* Unload a item that was loaded by Asset Manager */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static void UnloadRancItem(const FPrimaryRISItemId& InItemId);
    
    /* Check if the ids of the given item datas are equal */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static bool CompareItemData(const UItemStaticData* Data1, const UItemStaticData* Data2);

    /* Return the item data related to the given id */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static UItemStaticData* GetSingleItemDataById(const FPrimaryRISItemId& InID, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    /* Return a array of data depending of the given ids */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    static TArray<UItemStaticData*> GetItemDataArrayById(const TArray<FPrimaryRISItemId> InIDs, const TArray<FName>& InBundles, const bool bAutoUnload = true);
    
    /* Uses static map from GameplayTag to URancItemData, works faster after having called PermanentlyLoadAllItems */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    static UItemStaticData* GetItemDataById(FGameplayTag TagId);
    
    // Utiltiy function to check if moving an item from one slot to another would cause a swap
    static bool ShouldItemsBeSwapped(FItemBundle* Source, FItemBundle* Target);

    
    /* Moves from a source ItemInstance to a target one, either moving, stacking stackable items or swapping
     * Not exposed to blueprint directly as its a utility function for other inventory classes
     * IgnoreMaxStacks will allow a target slot to go above the item datas maxstacksize (used for itemcontainer)
     * AllowPartial if enabled will allow a move to partially succeed, e.g. only move 2 of requested 3 quantity, if false moves full or nothing
     */
    static FRISMoveResult MoveBetweenSlots(FItemBundle* Source, FItemBundle* Target, bool IgnoreMaxStacks, int32 RequestedQuantity, bool AllowPartial);
    
    
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

private:
    static TArray<UItemStaticData*> LoadRancItemData_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryAssetId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);
};
