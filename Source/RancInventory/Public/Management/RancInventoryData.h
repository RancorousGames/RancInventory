// Copyright Rancorous Games, 2023

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "RancInventoryData.generated.h"

class UTexture2D;

constexpr auto RancItemDataType = TEXT("RancInventory_ItemData");

UENUM(BlueprintType, Category = "Ranc Inventory | Enumerations")
enum class ERancItemType : uint8
{
    None,
    Consumable,
    Armor,
    Weapon,
    Accessory,
    Crafting,
    Material,
    Information,
    Special,
    Event,
    Quest,
    Junk,
    Other,

    MAX
};

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

    explicit FRancItemInfo(const FPrimaryRancItemId& InItemId) : ItemId(InItemId)
    {
    }

    explicit FRancItemInfo(const FPrimaryRancItemId& InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }

    explicit FRancItemInfo(const FPrimaryRancItemId& InItemId, const int32& InQuant, const FGameplayTagContainer& InTags) : ItemId(InItemId), Quantity(InQuant), Tags(InTags)
    {
    }

    bool operator==(const FRancItemInfo& Other) const
    {
        return ItemId == Other.ItemId && Tags == Other.Tags && Level == Other.Level;
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
    FPrimaryRancItemId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    int32 Level = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FGameplayTagContainer Tags;
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "SoftData"))
    TSoftObjectPtr<UObject> ItemObject;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "SoftData"))
    TSoftClassPtr<UObject> ItemClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FName ItemName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data", MultiLine = "true"))
    FText ItemDescription;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTag ItemPrimaryType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    bool bIsStackable = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemValue;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ItemWeight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "UI"))
    TSoftObjectPtr<UTexture2D> ItemIcon;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AssetBundles = "Data"))
    FGameplayTagContainer ItemCategories;
    
    /* Allows to implement custom properties in this item data */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (DisplayName = "Custom Metadatas", AssetBundles = "Custom"))
    TMap<FGameplayTag, FName> Metadatas;

    /* Map containing a tag as key and a ID container as value to add relations to other items such as crafting requirements, etc. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (DisplayName = "Item Relations", AssetBundles = "Custom"))
    TMap<FGameplayTag, FPrimaryRancItemIdContainer> Relations;
};

UCLASS(Blueprintable, Category = "Ranc Inventory | Classes | Data")
class RANCINVENTORY_API UUsableRancItemData : public URancItemData
{
    GENERATED_BODY()

public:
    // Constructor
    UUsableRancItemData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), UseValue(25)
    {
        // ItemPrimaryType = FGameplayTag::RequestGameplayTag(TEXT("RancInventory.ItemType.Usable"));
    }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranc Inventory", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float UseValue;

};