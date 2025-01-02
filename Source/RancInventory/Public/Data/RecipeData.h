// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "RecipeData.generated.h"

// This class is used to define a recipe for crafting any object type, RIS helps you specify the class but not instantiating the object
UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API UObjectRecipeData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit UObjectRecipeData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()){}
    
    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        // Create a string by appending the resulting item ID to each component
        FString ResultingItemIdString = ResultingObject ? ResultingObject->GetName() : FString("Null-");
        for (const auto& Component : Components)
        {
            ResultingItemIdString += Component.ItemId.ToString();
        }
        
        return FPrimaryRISRecipeId(TEXT("RancInventory_ItemRecipe"), *ResultingItemIdString);
    }
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    TSubclassOf<UObject> ResultingObject;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    int32 QuantityCreated = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    TArray<FItemBundle> Components;

    /* Tags can be used to group recipes, e.g. you might have Recipes.Items and Recipes.Buildings */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTagContainer Tags;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;

};


// This class is used to define a recipe for crafting RIS items specifically
UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API UItemRecipeData : public UObjectRecipeData
{
    GENERATED_BODY()

public:
    explicit UItemRecipeData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
    {
        
    }
    
    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        // Create a string by appending the resulting item ID to each component
        FString ResultingItemIdString = ResultingItemId.ToString();
        for (const auto& Component : Components)
        {
            ResultingItemIdString += Component.ItemId.ToString();
        }
        
        return FPrimaryAssetId(TEXT("RancInventory_ItemRecipe"), *ResultingItemIdString);
    }
public:
    // Replaces use of ResultingItem
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTag ResultingItemId;
};
