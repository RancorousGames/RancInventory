// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include "Data/ItemInstanceData.h"
#include "ItemBundle.generated.h"

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
struct FTaggedItemBundle
{
    GENERATED_BODY()

    static const FTaggedItemBundle EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FGameplayTag Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FItemBundle ItemBundle;
    
    bool IsValid() const
    {
        return Tag.IsValid() && ItemBundle.IsValid();
    }
    
    FTaggedItemBundle(){}
    FTaggedItemBundle(FGameplayTag InTag, FItemBundle InItemInfo)
    {
        ItemBundle = InItemInfo;
        Tag = InTag;
    }

    FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
        FItemBundle bundle = FItemBundle(InItemId, InQuantity);
        ItemBundle = bundle;
        Tag = InTag;
    }

    
    bool operator==(const FTaggedItemBundle& Other) const
    {
        return Tag == Other.Tag && ItemBundle == Other.ItemBundle;
    }

    bool operator!=(const FTaggedItemBundle& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FTaggedItemBundle& Other) const
    {
        return Tag.ToString() < Other.Tag.ToString();
    }
};

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct FItemBundleWithInstanceData
{
    GENERATED_BODY()

    static const FTaggedItemBundle EmptyItemBundle;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    TArray<UItemInstanceData*> InstanceData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    FItemBundle ItemBundle;
    
    bool IsValid() const;

    void DestroyQuantity(int32 Quantity);
    
    int32 ExtractQuantity(int32 Quantity, TArray<UItemInstanceData*> StateArrayToAppendTo);

    FItemBundleWithInstanceData(){}
    FItemBundleWithInstanceData(FItemBundle InItemInfo, const TArray<UItemInstanceData*>& InstanceData = TArray<UItemInstanceData*>())
    {
        ItemBundle = InItemInfo;
        this->InstanceData = InstanceData;
    }

    FItemBundleWithInstanceData(FGameplayTag InItemId, int32 InQuantity, const TArray<UItemInstanceData*>& InstanceData = TArray<UItemInstanceData*>())
    {
        ItemBundle = FItemBundle(InItemId, InQuantity);
        this->InstanceData = InstanceData;
    }

    
    bool operator==(const FItemBundleWithInstanceData& Other) const
    {
        return InstanceData == Other.InstanceData && ItemBundle == Other.ItemBundle;
    }

    bool operator!=(const FItemBundleWithInstanceData& Other) const
    {
        return !(*this == Other);
    }

    //bool operator<(const FItemBundleWithInstanceData& Other) const
    //{
    //    return InstanceData->ToString() < Other.InstanceData->ToString();
    //}
};
