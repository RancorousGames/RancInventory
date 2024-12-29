#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "RISSubsystem.generated.h"

// Forward declarations
class UItemStaticData;
class UObjectRecipeData;
class UInfiniteItemSource;

UCLASS()
class WARTRIBES_API URISSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Constructor
    URISSubsystem();

    // Subsystem Initialization
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Loading and Unloading Operations
    UFUNCTION(BlueprintCallable, Category = "RIS")
    void PermanentlyLoadAllItemsAsync();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    void PermanentlyLoadAllRecipesAsync();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    void UnloadAllRISItems();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    void UnloadRISItem(const FRISItemPrimaryAssetId& InItemId);

    // Status Checks
    UFUNCTION(BlueprintPure, Category = "RIS")
    bool AreAllItemsLoaded();

    UFUNCTION(BlueprintPure, Category = "RIS")
    bool AreAllRecipesLoaded();

    // Data Retrieval
    UFUNCTION(BlueprintPure, Category = "RIS")
    TArray<FGameplayTag> GetAllRISItemIds();

    UFUNCTION(BlueprintPure, Category = "RIS")
    TArray<FPrimaryAssetId> GetAllRISItemPrimaryIds();

    UFUNCTION(BlueprintPure, Category = "RIS")
    static TArray<FPrimaryAssetId> GetAllRISItemRecipeIds();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    TArray<UObjectRecipeData*> GetAllRISItemRecipes();

    UFUNCTION(BlueprintPure, Category = "RIS")
    UItemStaticData* GetItemDataById(FGameplayTag TagId);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    UItemStaticData* GetSingleItemDataById(const FRISItemPrimaryAssetId& InID, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    TArray<UItemStaticData*> GetItemDataArrayById(const TArray<FRISItemPrimaryAssetId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    TArray<UItemStaticData*> SearchRISItemData(const ERISItemSearchType SearchType, const FString& SearchString, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    // Debugging and Testing
    void HardcodeItem(FGameplayTag ItemId, UItemStaticData* ItemData);
    void HardcodeRecipe(FGameplayTag RecipeId, UObjectRecipeData* RecipeData);

    // Callback for Asset Loading
    void AllItemsLoadedHandler();

    // Properties
    UPROPERTY(Transient)
    UInfiniteItemSource* InfiniteItemSource;

private:
    // Any private members or helper methods
};