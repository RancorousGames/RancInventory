// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "RISInventoryData.generated.h"

class UTexture2D;

constexpr auto RancInventoryItemDataType = TEXT("RancInventory_ItemData");
constexpr auto RancInventoryRecipeDataType = TEXT("RancInventory_ItemRecipe");

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FPrimaryRISItemId : public FPrimaryAssetId
{
    GENERATED_BODY()

    FPrimaryRISItemId() : Super()
    {
    }

    explicit FPrimaryRISItemId(const FPrimaryAssetId& InId) : Super(InId.PrimaryAssetType, InId.PrimaryAssetName)
    {
    }

    explicit FPrimaryRISItemId(const FString& TypeAndName) : Super(TypeAndName)
    {
    }

    bool operator>(const FPrimaryRISItemId& Other) const
    {
        return ToString() > Other.ToString();
    }

    bool operator<(const FPrimaryRISItemId& Other) const
    {
        return ToString() < Other.ToString();
    }
};


USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FPrimaryRISRecipeId : public FPrimaryAssetId
{
    GENERATED_BODY()

    FPrimaryRISRecipeId() : Super()
    {
    }

    FPrimaryRISRecipeId(FPrimaryAssetType InAssetType, FName InAssetName)
        : Super(InAssetType, InAssetName)
    {}
    
    explicit FPrimaryRISRecipeId(const FPrimaryRISRecipeId& InId) : Super(InId.PrimaryAssetType, InId.PrimaryAssetName)
    {
    }

    explicit FPrimaryRISRecipeId(const FString& TypeAndName) : Super(TypeAndName)
    {
    }

    bool operator>(const FPrimaryRISRecipeId& Other) const
    {
        return ToString() > Other.ToString();
    }

    bool operator<(const FPrimaryRISRecipeId& Other) const
    {
        return ToString() < Other.ToString();
    }
};

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FPrimaryRISItemIdContainer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    TArray<FPrimaryRISItemId> Items;
};

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRISItemInstance
{
    GENERATED_BODY()

    static const FRISItemInstance EmptyItemInstance;

    FRISItemInstance() = default;

    explicit FRISItemInstance(const FGameplayTag& InItemId) : ItemId(InItemId)
    {
    }

    explicit FRISItemInstance(const FGameplayTag& InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }

    bool operator==(const FRISItemInstance& Other) const
    {
        return ItemId == Other.ItemId;
    }

    bool operator!=(const FRISItemInstance& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FRISItemInstance& Other) const
    {
        return ItemId.ToString() < Other.ItemId.ToString();
    }

    bool IsValid() const
    {
        return ItemId.IsValid();
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FGameplayTag ItemId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    int32 Quantity = 0;
};

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRancInitialItem
{
    GENERATED_BODY()

    static const FRancInitialItem EmptyItemInfo;

    FRancInitialItem() = default;

    explicit FRancInitialItem(FPrimaryRISItemId InItemId) : ItemId(InItemId)
    {
    }

    explicit FRancInitialItem(FPrimaryRISItemId InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FPrimaryRISItemId ItemId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    int32 Quantity = 1;
};

UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API URISItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit URISItemData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    FORCEINLINE virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(TEXT("RancInventory_ItemData"), *(ItemId.ToString()));
    }
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTag ItemId;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FName ItemName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data", MultiLine = "true"))
    FText ItemDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTag ItemPrimaryType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    bool bIsStackable = true;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    int32 MaxStackSize = 5;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemValue;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemWeight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> ItemIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTagContainer ItemCategories;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    UStaticMesh* ItemWorldMesh = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    FVector ItemWorldScale = FVector(1.0f, 1.0f, 1.0f);

    /* Allows to implement custom properties in this item data */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (DisplayName = "Custom Metadatas", AssetBundles = "Custom"))
    TMap<FGameplayTag, FName> Metadatas;

    /* Map containing a tag as key and a ID container as value to add relations to other items such as crafting requirements, etc. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (DisplayName = "Item Relations", AssetBundles = "Custom"))
    TMap<FGameplayTag, FPrimaryRISItemIdContainer> Relations;
};


UCLASS(Blueprintable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API USpawnableRancItemData : public URISItemData
{
    GENERATED_BODY()

public:
    // Constructor
    USpawnableRancItemData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), SpawnableObjectClass(nullptr)
    {
    }

    virtual void PostLoad() override
    {
        Super::PostLoad();
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    TSubclassOf<UObject> SpawnableObjectClass;
};

// This class is used to define a recipe for crafting any object type, RIS helps you specify the class but not instantiating the object
UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API URISObjectRecipeData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit URISObjectRecipeData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
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
    TArray<FRISItemInstance> Components;

    /* Tags can be used to group recipes, e.g. you might have Recipes.Items and Recipes.Buildings */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
    FGameplayTagContainer Tags;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> Icon;

};


// This class is used to define a recipe for crafting RIS items specifically
UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API URISItemRecipeData : public URISObjectRecipeData
{
    GENERATED_BODY()

public:
    explicit URISItemRecipeData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
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

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRancTaggedItemInstance
{
    GENERATED_BODY()

    static const FRancTaggedItemInstance EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FRISItemInstance ItemInstance;
    
    bool IsValid() const
    {
        return Tag.IsValid() && ItemInstance.IsValid();
    }
    
    FRancTaggedItemInstance(){}
    FRancTaggedItemInstance(FGameplayTag InTag, FRISItemInstance InItemInfo)
    {
        ItemInstance = InItemInfo;
        Tag = InTag;
    }

    FRancTaggedItemInstance(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
        ItemInstance = FRISItemInstance(InItemId, InQuantity);
        Tag = InTag;
    }

    
    bool operator==(const FRancTaggedItemInstance& Other) const
    {
        return Tag == Other.Tag && ItemInstance == Other.ItemInstance;
    }

    bool operator!=(const FRancTaggedItemInstance& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FRancTaggedItemInstance& Other) const
    {
        return Tag.ToString() < Other.Tag.ToString();
    }
};