#pragma once

#include "CoreMinimal.h"
#include "IItemSource.h"
#include "Engine/AssetManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "RISSubsystem.generated.h"

// Forward declarations
class UItemStaticData;
class UObjectRecipeData;

UCLASS()
class RANCINVENTORY_API URISSubsystem : public UGameInstanceSubsystem, public IItemSource
{
    GENERATED_BODY()

public:
    // Constructor
    URISSubsystem();

    UFUNCTION(BlueprintPure, Category = "RIS")
    static URISSubsystem* Get(UObject* WorldContext);

    // Subsystem Initialization
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Loading and Unloading Operations
    UFUNCTION(BlueprintCallable, Category = "RIS")
    void PermanentlyLoadAllItemsAsync();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    void PermanentlyLoadAllRecipesAsync();
    
    TArray<FPrimaryAssetId> GetAllRancItemPrimaryIds();
    
    UFUNCTION(BlueprintCallable, Category = "RIS")
    void UnloadAllRISItems();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    void UnloadRISItem(const FPrimaryRISItemId& InItemId);

    // Status Checks
    UFUNCTION(BlueprintPure, Category = "RIS")
    bool AreAllItemsLoaded();

    UFUNCTION(BlueprintPure, Category = "RIS")
    bool AreAllRecipesLoaded();

    // Data Retrieval
    UFUNCTION(BlueprintPure, Category = "RIS")
    TArray<FGameplayTag> GetAllRISItemIds();

    UFUNCTION(BlueprintPure, Category = "RIS")
    static TArray<FPrimaryAssetId> GetAllRISItemPrimaryIds();

    UFUNCTION(BlueprintPure, Category = "RIS")
    static TArray<FPrimaryAssetId> GetAllRISItemRecipeIds();

    UFUNCTION(BlueprintCallable, Category = "RIS")
    TArray<UObjectRecipeData*> GetAllRISItemRecipes();

    UFUNCTION(BlueprintPure, Category = "RIS")
    UItemStaticData* GetItemDataById(FGameplayTag TagId);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    UItemStaticData* GetSingleItemDataById(const FPrimaryRISItemId& InID, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    TArray<UItemStaticData*> GetItemDataArrayById(const TArray<FPrimaryRISItemId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload = true);

    // Debugging and Testing
    void HardcodeItem(FGameplayTag ItemId, UItemStaticData* ItemData);
    void HardcodeRecipe(FGameplayTag RecipeId, UObjectRecipeData* RecipeData);


    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
    int32 ExtractItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);

    virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo) override;
	
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
    int32 GetContainedQuantity(const FGameplayTag& ItemId);

    UFUNCTION(BlueprintCallable, Category = "RIS")
    AWorldItem* SpawnWorldItem(UWorld* World, 
                              TSubclassOf<AWorldItem> WorldItemClass,
                              const FGameplayTag& ItemId,
                              int32 Quantity,
                              const FVector& Location,
                              TArray<UItemInstanceData*> InstanceData);

    // Helper function to get the appropriate world item class
    TSubclassOf<AWorldItem> GetWorldItemClass(const FGameplayTag& ItemId, 
                                             TSubclassOf<AWorldItem> DefaultClass) const;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllItemsLoaded);
    UPROPERTY(BlueprintAssignable, Category = "RIS")
    FOnAllItemsLoaded OnAllItemsLoaded;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllRecipesLoaded);
    UPROPERTY(BlueprintAssignable, Category = "RIS")
    FOnAllRecipesLoaded OnAllRecipesLoaded;

    
//
private:
    void AllItemsLoadedCallback() const;
    void AllRecipesLoadedCallback() const;
    
    TArray<UItemStaticData*> LoadRancItemData_Internal(UAssetManager* InAssetManager, const TArray<FPrimaryRISItemId>& InIDs, const TArray<FName>& InBundles, const bool bAutoUnload);

    static TMap<FGameplayTag, UItemStaticData*> AllLoadedItemsByTag;
    static TArray<FGameplayTag> AllItemIds;
    static TArray<UObjectRecipeData*> AllLoadedRecipes;
};