#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "RancItemContainerComponent.h"
#include "RancInventoryComponent.generated.h"


UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API URancInventoryComponent : public URancItemContainerComponent
{
    GENERATED_BODY()

public:
    explicit URancInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void InitializeComponent() override;

    
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    bool CanCraftRecipeId(const FPrimaryAssetId& RecipeId) const;

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    bool CanCraftRecipe(const URancItemRecipe* Recipe) const;
    
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    bool CanCraftCraftingRecipe(const FPrimaryAssetId& RecipeId) const;

    DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCraftingSuccessOptionalDelegate, TSubclassOf<UObject>, CraftedObjectClass);
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    bool CraftRecipeId(const FPrimaryAssetId& RecipeId, FOnCraftingSuccessOptionalDelegate OptionalSuccessDelegate);
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    bool CraftRecipe(const URancItemRecipe* Recipe, FOnCraftingSuccessOptionalDelegate OptionalSuccessDelegate);
    
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
    FRancItemInfo CraftCraftingRecipe(const FPrimaryAssetId& RecipeId, bool& bSuccess);

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Recipes")
    void UnlockRecipe(const FPrimaryAssetId& RecipeId);

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Recipes")
    void LockRecipe(const FPrimaryAssetId& RecipeId);

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Recipes")
    TArray<URancItemRecipe*> GetRecipes(FGameplayTag Category);

    
    // Adjusted delegate type to match the UObject crafting context
    
    // Delegate for successful crafting, now provides a class reference instead of item info
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraftingSuccess, TSubclassOf<UObject>, CraftedObjectClass);
    UPROPERTY(BlueprintAssignable, Category="Ranc Inventory | Crafting")
    FOnCraftingSuccess OnCraftingSuccess;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ranc Inventory | Recipes")
    FGameplayTagContainer RecipeCategories;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ranc Inventory | Recipes")
    TArray<URancItemRecipe*> AllAvailableRecipes;
    
    UFUNCTION()
    void OnInventoryItemAddedHandler(const FRancItemInfo& ItemInfo);
    UFUNCTION()
    void OnInventoryItemRemovedHandler(const FRancItemInfo& ItemInfo);

protected:

    void CheckAndUpdateRecipeAvailability();
private:
    
    TMap<FGameplayTag, TArray<URancItemRecipe*>> CurrentAvailableRecipes;
};