// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "RancInventoryData.generated.h"

class UTexture2D;

constexpr auto RancItemDataType = TEXT("RancInventory_ItemData");
constexpr auto RancItemRecipeType = TEXT("RancInventory_ItemRecipe");

USTRUCT(BlueprintType, Category = "Ranc Inventory | Structs")
struct FPrimaryRancItemId : public FPrimaryAssetId
{
    GENERATED_BODY()

    FPrimaryRancItemId() : Super()
    {
    }

    explicit FPrimaryRancItemId(const FPrimaryAssetId& InId) : Super(InId.PrimaryAssetType, InId.PrimaryAssetName)
    {
    }

    explicit FPrimaryRancItemId(const FString& TypeAndName) : Super(TypeAndName)
    {
    }

    bool operator>(const FPrimaryRancItemId& Other) const
    {
        return ToString() > Other.ToString();
    }

    bool operator<(const FPrimaryRancItemId& Other) const
    {
        return ToString() < Other.ToString();
    }
};

USTRUCT(BlueprintType, Category = "Ranc Inventory | Structs")
struct FPrimaryRancItemIdContainer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    TArray<FPrimaryRancItemId> Items;
};

USTRUCT(BlueprintType, Category = "Ranc Inventory | Structs")
struct FRancItemInfo
{
    GENERATED_BODY()

    static const FRancItemInfo EmptyItemInfo;

    FRancItemInfo() = default;

    explicit FRancItemInfo(const FGameplayTag& InItemId) : ItemId(InItemId)
    {
    }

    explicit FRancItemInfo(const FGameplayTag& InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }

    bool operator==(const FRancItemInfo& Other) const
    {
        return ItemId == Other.ItemId;
    }

    bool operator!=(const FRancItemInfo& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FRancItemInfo& Other) const
    {
        return ItemId.ToString() < Other.ItemId.ToString();
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FGameplayTag ItemId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    int32 Quantity = 1;
};

USTRUCT(BlueprintType, Category = "Ranc Inventory | Structs")
struct FRancInitialItem
{
    GENERATED_BODY()

    static const FRancInitialItem EmptyItemInfo;

    FRancInitialItem() = default;

    explicit FRancInitialItem(FPrimaryRancItemId InItemId) : ItemId(InItemId)
    {
    }

    explicit FRancInitialItem(FPrimaryRancItemId InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FPrimaryRancItemId ItemId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    int32 Quantity = 1;
};

UCLASS(NotBlueprintable, NotPlaceable, Category = "Ranc Inventory | Classes | Data")
class RANCINVENTORY_API URancItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit URancItemData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("RancInventory_ItemData"), *(ItemId.ToString()));
    }
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTag ItemId;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FName ItemName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data", MultiLine = "true"))
    FText ItemDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTag ItemPrimaryType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    bool bIsStackable = true;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    int32 MaxStackSize = 5;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemValue;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemWeight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> ItemIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTagContainer ItemCategories;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    UStaticMesh* ItemWorldMesh = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    FVector ItemWorldScale = FVector(1.0f, 1.0f, 1.0f);

    /* Allows to implement custom properties in this item data */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (DisplayName = "Custom Metadatas", AssetBundles = "Custom"))
    TMap<FGameplayTag, FName> Metadatas;

    /* Map containing a tag as key and a ID container as value to add relations to other items such as crafting requirements, etc. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (DisplayName = "Item Relations", AssetBundles = "Custom"))
    TMap<FGameplayTag, FPrimaryRancItemIdContainer> Relations;
};


UCLASS(NotBlueprintable, NotPlaceable, Category = "Ranc Inventory | Classes | Data")
class RANCINVENTORY_API URancItemRecipe : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit URancItemRecipe(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        // Create a string by appending the resulting item ID to each component
        FString ResultingItemIdString = ResultingObject ? ResultingObject->GetName() : FString("Null-");
        for (const auto& Component : Components)
        {
            ResultingItemIdString += Component.ItemId.ToString();
        }
        
        return FPrimaryAssetId(TEXT("RancInventory_ItemRecipe"), *ResultingItemIdString);
    }
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    TSubclassOf<UObject> ResultingObject;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    int32 AmountCreated = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    TArray<FRancItemInfo> Components;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTagContainer Tags;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;

};



UCLASS(NotBlueprintable, NotPlaceable, Category = "Ranc Inventory | Classes | Data")
class RANCINVENTORY_API URancItemCraftingRecipe : public URancItemRecipe
{
    GENERATED_BODY()

public:
    explicit URancItemCraftingRecipe(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()){}
    
    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        // Create a string by appending the resulting item ID to each component
        FString ResultingItemIdString = ResultingItem.ItemId.ToString();
        for (const auto& Component : Components)
        {
            ResultingItemIdString += Component.ItemId.ToString();
        }
        
        return FPrimaryAssetId(TEXT("RancInventory_ItemRecipe"), *ResultingItemIdString);
    }
public:
    // Replaces use of ResultingItem
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FRancItemInfo ResultingItem;
};