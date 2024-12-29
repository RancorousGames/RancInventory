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
struct FItemBundle
{
    GENERATED_BODY()

    static const FItemBundle EmptyItemInstance;

    FItemBundle() = default;

    explicit FItemBundle(const FGameplayTag& InItemId) : ItemId(InItemId)
    {
    }

    explicit FItemBundle(const FGameplayTag& InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }

    bool operator==(const FItemBundle& Other) const
    {
        return ItemId == Other.ItemId;
    }

    bool operator!=(const FItemBundle& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FItemBundle& Other) const
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

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRancTaggedItemInstance
{
    GENERATED_BODY()

    static const FRancTaggedItemInstance EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FItemBundle ItemInstance;
    
    bool IsValid() const
    {
        return Tag.IsValid() && ItemInstance.IsValid();
    }
    
    FRancTaggedItemInstance(){}
    FRancTaggedItemInstance(FGameplayTag InTag, FItemBundle InItemInfo)
    {
        ItemInstance = InItemInfo;
        Tag = InTag;
    }

    FRancTaggedItemInstance(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
        ItemInstance = FItemBundle(InItemId, InQuantity);
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

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FItemBundleWithInstanceData
{
    GENERATED_BODY()

    static const FRancTaggedItemInstance EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    UItemInstanceData* InstanceData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FItemBundle ItemInstance;
    
    bool IsValid() const
    {
        return InstanceData->IsValid() && ItemInstance.IsValid();
    }
    
    FItemBundleWithInstanceData(){}
    FItemBundleWithInstanceData(FGameplayTag InTag, FItemBundle InItemInfo)
    {
        ItemInstance = InItemInfo;
        InstanceData = InTag;
    }

    FRancTaggedItemInstance(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
        ItemInstance = FItemBundle(InItemId, InQuantity);
        InstanceData = InTag;
    }

    
    bool operator==(const FRancTaggedItemInstance& Other) const
    {
        return InstanceData == Other.Tag && ItemInstance == Other.ItemInstance;
    }

    bool operator!=(const FRancTaggedItemInstance& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FRancTaggedItemInstance& Other) const
    {
        return InstanceData.ToString() < Other.Tag.ToString();
    }
};

