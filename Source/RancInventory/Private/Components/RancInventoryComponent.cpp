// Rancorous Games, 2024

#include "Components/RancInventoryComponent.h"
#include <GameFramework/Actor.h>

#include <Engine/AssetManager.h>
#include "Management/RancInventoryFunctions.h"
#include "Management/RancInventorySettings.h"


URancInventoryComponent::URancInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void URancInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

    // Subscribe to base class inventory events
    OnInventoryItemAdded.AddDynamic(this, &URancInventoryComponent::OnInventoryItemAddedHandler);
    OnInventoryItemRemoved.AddDynamic(this, &URancInventoryComponent::OnInventoryItemRemovedHandler);

    // Initialize available recipes based on initial inventory and recipes
    CheckAndUpdateRecipeAvailability();
}

bool URancInventoryComponent::CanCraftRecipeId(const FPrimaryAssetId& RecipeId) const
{
    const URancItemRecipe* Recipe = Cast<URancItemRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
    return CanCraftRecipe(Recipe);
}


bool URancInventoryComponent::CanCraftRecipe(const URancItemRecipe* Recipe) const
{
    if (!Recipe) return false;

    for (const auto& Component : Recipe->Components)
    {
        if (!ContainsItem(Component.ItemId, Component.Quantity))
        {
            return false;
        }
    }
    return true;
}

bool URancInventoryComponent::CanCraftCraftingRecipe(const FPrimaryAssetId& RecipeId) const
{
    URancItemCraftingRecipe* CraftingRecipe = Cast<URancItemCraftingRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
    return CanCraftRecipe(CraftingRecipe);
}

bool URancInventoryComponent::CraftRecipeId(const FPrimaryAssetId& RecipeId, FOnCraftingSuccessOptionalDelegate OptionalSuccessDelegate)
{
    const URancItemRecipe* Recipe = Cast<URancItemRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
    return CraftRecipe(Recipe, OptionalSuccessDelegate);
}

bool URancInventoryComponent::CraftRecipe(const URancItemRecipe* Recipe, FOnCraftingSuccessOptionalDelegate OptionalSuccessDelegate)
{
    bool bSuccess = false;
    if (Recipe && CanCraftRecipe(Recipe))
    {
        for (const auto& Component : Recipe->Components)
        {
            RemoveItems(Component); // Assuming RemoveItems modifies the inventory appropriately.
        }
        bSuccess = true;

        if (OptionalSuccessDelegate.IsBound())
        {
            OptionalSuccessDelegate.Execute(Recipe->ResultingObject);
        }
        OnCraftingSuccess.Broadcast(Recipe->ResultingObject);
    }
    return bSuccess;
}

FRancItemInfo URancInventoryComponent::CraftCraftingRecipe(const FPrimaryAssetId& RecipeId, bool& bSuccess)
{
    URancItemCraftingRecipe* CraftingRecipe = Cast<URancItemCraftingRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
    bSuccess = false;
    FRancItemInfo Result = FRancItemInfo::EmptyItemInfo;
    
    if (CraftingRecipe && CanCraftRecipe(CraftingRecipe))
    {
        for (const auto& Component : CraftingRecipe->Components)
        {
            RemoveItems(Component); // Assuming RemoveItems modifies the inventory appropriately.
        }
        bSuccess = true;
        Result = CraftingRecipe->ResultingItem;
    }
    return Result;
}

void URancInventoryComponent::UnlockRecipe(const FPrimaryAssetId& RecipeId)
{
    if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
    {
        URancItemRecipe* Recipe = Cast<URancItemRecipe>(AssetManager->GetPrimaryAssetObject(RecipeId));
        if (Recipe && !AllAvailableRecipes.Contains(Recipe))
        {
            AllAvailableRecipes.Add(Recipe);
            CheckAndUpdateRecipeAvailability();
        }
    }
}

void URancInventoryComponent::LockRecipe(const FPrimaryAssetId& RecipeId)
{
    if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
    {
        URancItemRecipe* Recipe = Cast<URancItemRecipe>(AssetManager->GetPrimaryAssetObject(RecipeId));
        if (Recipe)
        {
            AllAvailableRecipes.Remove(Recipe);
            CheckAndUpdateRecipeAvailability();
        }
    }
}

TArray<URancItemRecipe*> URancInventoryComponent::GetRecipes(FGameplayTag Category)
{
    return CurrentAvailableRecipes.Contains(Category) ? CurrentAvailableRecipes[Category] : TArray<URancItemRecipe*>();
}

void URancInventoryComponent::OnInventoryItemAddedHandler(const FRancItemInfo& ItemInfo)
{
    CheckAndUpdateRecipeAvailability();
}

void URancInventoryComponent::OnInventoryItemRemovedHandler(const FRancItemInfo& ItemInfo)
{
    CheckAndUpdateRecipeAvailability();
}

void URancInventoryComponent::CheckAndUpdateRecipeAvailability()
{
    // Clear current available recipes
    CurrentAvailableRecipes.Empty();

    // Iterate through all available recipes and check if they can be crafted
    for (URancItemRecipe* Recipe : AllAvailableRecipes)
    {
        if (CanCraftRecipe(Recipe))
        {
            for (const FGameplayTag& Category : RecipeCategories)
            {
                // If recipe matches a category, add it to the corresponding list
                if (Recipe->Tags.HasTag(Category))
                {
                    if (!CurrentAvailableRecipes.Contains(Category))
                    {
                        CurrentAvailableRecipes.Add(Category, {});
                    }
                    CurrentAvailableRecipes[Category].Add(Recipe);
                }
            }
        }
    }
}