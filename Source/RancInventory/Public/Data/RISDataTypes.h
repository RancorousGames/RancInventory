// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "Data/ItemInstanceData.h"
#include "ItemBundle.h"
#include "RISDataTypes.generated.h"

class UItemStaticData;
class UTexture2D;

constexpr auto RancInventoryItemDataType = TEXT("RancInventory_ItemData");
constexpr auto RancInventoryRecipeDataType = TEXT("RancInventory_ItemRecipe");

UENUM(BlueprintType)
enum class EItemChangeReason : uint8
{
    Added UMETA(DisplayName = "Added"),      
    Removed UMETA(DisplayName = "Removed"),  
    Updated UMETA(DisplayName = "Updated"), 
    Equipped UMETA(DisplayName = "Equipped"),
    Unequipped UMETA(DisplayName = "Unequipped") ,
    ForceDestroyed UMETA(DisplayName = "ForceDestroyed") ,
    Moved UMETA(DisplayName = "Moved"),
    Dropped UMETA(DisplayName = "Dropped"),
    Consumed UMETA(DisplayName = "Consumed"),
    Transformed UMETA(DisplayName = "Transformed"),
    Transferred UMETA(DisplayName = "Transferred"),
    Synced,
};


USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRISMoveResult
{
    GENERATED_BODY()
    
    FRISMoveResult() = default;
    
    FRISMoveResult(int32 Quantity, bool Swapped) : QuantityMoved(Quantity), WereItemsSwapped(Swapped) {};

    UPROPERTY()
    int32 QuantityMoved;
    UPROPERTY()
    bool WereItemsSwapped;
};


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
struct FInitialItem
{
    GENERATED_BODY()

    static const FInitialItem EmptyItemInfo;

    FInitialItem() = default;

    explicit FInitialItem(FPrimaryRISItemId InItemId) : ItemId(InItemId)
    {
    }

    explicit FInitialItem(FPrimaryRISItemId InItemId, const int32& InQuant) : ItemId(InItemId), Quantity(InQuant)
    {
    }

    // TODO RECOVERY: 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FPrimaryRISItemId ItemId;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    UItemStaticData* ItemData;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    int32 Quantity = 1;
};


USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FRandomItemPool
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FName Description;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    TArray<float> ItemWeights;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    TArray<FItemBundle> Items;
    // TArray<FRandomItemSelection> Items; old version had a struct but why cant we just use itembundle? i dont know what was in that struct originally
};