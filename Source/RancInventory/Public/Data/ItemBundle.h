// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <variant>

#include "Data/ItemInstanceData.h"
#include "ItemBundle.generated.h"

USTRUCT(BlueprintType, Category = "RIS | Structs")
struct RANCINVENTORY_API FItemBundle
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
struct RANCINVENTORY_API FTaggedItemBundle
{
    GENERATED_BODY()

    static const FTaggedItemBundle EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
    FGameplayTag Tag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	FGameplayTag ItemId;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	int32 Quantity = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RIS")
	bool IsBlocked = false;
	
    bool IsValid() const
    {
        return Tag.IsValid() && ItemId.IsValid() && Quantity > 0;
    }
    
    FTaggedItemBundle(){}
    FTaggedItemBundle(FGameplayTag InTag, FItemBundle InItemInfo)
    {
        ItemId = InItemInfo.ItemId;
    	Quantity = InItemInfo.Quantity;
        Tag = InTag;
    }

    FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
        FItemBundle bundle = FItemBundle(InItemId, InQuantity);
        ItemId = bundle.ItemId;
    	Quantity = bundle.Quantity;
        Tag = InTag;
    }

    
    bool operator==(const FTaggedItemBundle& Other) const
    {
        return Tag == Other.Tag && ItemId == Other.ItemId && Quantity == Other.Quantity;
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


// TODO: Figure out if we want to keep this or replace it with instance data
USTRUCT(BlueprintType, Category = "RIS | Structs")
struct RANCINVENTORY_API FItemBundleWithInstanceId
{
	GENERATED_BODY()

	static const FTaggedItemBundle EmptyItemInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	FGameplayTag ItemId;
    
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	int32 Quantity = 0;
	
	// Optionally defined unique ID for items that has instance data
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	int32 InstanceId = -1;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RIS")
	bool IsBlocked = false;
	
	bool IsValid() const
	{
		return Tag.IsValid() && ItemId.IsValid() && Quantity > 0;
	}
    
	FTaggedItemBundle(){}
	FTaggedItemBundle(FGameplayTag InTag, FItemBundle InItemInfo)
	{
		ItemId = InItemInfo.ItemId;
		Quantity = InItemInfo.Quantity;
		Tag = InTag;
	}

	FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
	{
		FItemBundle bundle = FItemBundle(InItemId, InQuantity);
		ItemId = bundle.ItemId;
		Quantity = bundle.Quantity;
		Tag = InTag;
	}

    
	bool operator==(const FTaggedItemBundle& Other) const
	{
		return Tag == Other.Tag && ItemId == Other.ItemId && Quantity == Other.Quantity;
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
struct RANCINVENTORY_API FItemBundleWithInstanceData
{
    GENERATED_BODY()

    static const FItemBundleWithInstanceData EmptyItemInstance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
    TArray<UItemInstanceData*> InstanceData;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	FGameplayTag ItemId;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	int32 Quantity = 0;
    
    bool IsValid() const;

    void DestroyQuantity(int32 Quantity, AActor* Owner);
    
    int32 ExtractQuantity(int32 InQuantity, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner);

    FItemBundleWithInstanceData(){}

	FItemBundleWithInstanceData(FGameplayTag InItemId)
    {
    	ItemId = InItemId;
    	Quantity = 0;
    	InstanceData = TArray<UItemInstanceData*>();
    }
	
    FItemBundleWithInstanceData(FItemBundle InItemInfo, const TArray<UItemInstanceData*>& InstanceData)
    {
        ItemId = InItemInfo.ItemId;
    	Quantity = InItemInfo.Quantity;
        this->InstanceData = InstanceData;
    }

    FItemBundleWithInstanceData(FGameplayTag InItemId, int32 InQuantity, const TArray<UItemInstanceData*>& InstanceData)
    {
        ItemId = InItemId;
    	Quantity = InQuantity;
        this->InstanceData = InstanceData;
    }

	// WARNING: This constructor should only be used when it is certain that instance data is not needed for this item type
	FItemBundleWithInstanceData(FGameplayTag InItemId, int32 InQuantity);

    // This overload will spawn the instance data
	FItemBundleWithInstanceData(FGameplayTag InItemId, int32 InQuantity, TSubclassOf<UItemInstanceData> InstanceDataClass, AActor* OwnerOfSpawnedInstanceData)
    {
    	ItemId = InItemId;
    	Quantity = InQuantity;

    	for (int32 i = 0; i < InQuantity; ++i)
		{
			if (UItemInstanceData* NewInstanceData = NewObject<UItemInstanceData>(OwnerOfSpawnedInstanceData, InstanceDataClass))
			{
				NewInstanceData->SetFlags(RF_Transient); // Not saved to disk
				InstanceData.Add(NewInstanceData);

				OwnerOfSpawnedInstanceData->AddReplicatedSubObject(NewInstanceData);
			}
		}
    }
    
    bool operator==(const FItemBundleWithInstanceData& Other) const
    {
        return InstanceData == Other.InstanceData && ItemId == Other.ItemId && Quantity == Other.Quantity;
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

// Class to wrap any of the above item bundle types in a unified type since we cant use inheritance
struct RANCINVENTORY_API FGenericItemBundle
{
    using BundleVariant = std::variant<FItemBundle*, FTaggedItemBundle*, FItemBundleWithInstanceData*, std::monostate>;
    
    BundleVariant Data;

    FGenericItemBundle() : Data(std::monostate{}) {}  // Initialize with monostate instead of nullptr
    FGenericItemBundle(FItemBundle* Bundle) : Data(Bundle ? Bundle : BundleVariant(std::monostate{})) {}
    FGenericItemBundle(FTaggedItemBundle* Bundle) : Data(Bundle ? Bundle : BundleVariant(std::monostate{})) {}
    FGenericItemBundle(FItemBundleWithInstanceData* Bundle) : Data(Bundle ? Bundle : BundleVariant(std::monostate{})) {}

    bool IsValid() const 
    {
        if (std::holds_alternative<std::monostate>(Data)) return false;
        return std::visit([](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return false;
            else
                return arg && arg->ItemId.IsValid();
        }, Data);
    }

    FGameplayTag GetItemId() const 
    {
        if (std::holds_alternative<std::monostate>(Data)) return FGameplayTag();
        return std::visit([](auto&& arg) -> FGameplayTag {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return FGameplayTag();
            else
                return arg ? arg->ItemId : FGameplayTag();
        }, Data);
    }

    int32 GetQuantity() const 
    {
        if (std::holds_alternative<std::monostate>(Data)) return 0;
        return std::visit([](auto&& arg) -> int32 {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return 0;
            else
                return arg ? arg->Quantity : 0;
        }, Data);
    }

    void SetQuantity(int32 NewQuantity) 
    {
        if (std::holds_alternative<std::monostate>(Data)) return;
        std::visit([NewQuantity](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (!std::is_same_v<T, std::monostate>)
                if (arg) arg->Quantity = NewQuantity;
        }, Data);
    }

    void SetItemId(const FGameplayTag& NewId) 
    {
        if (std::holds_alternative<std::monostate>(Data)) return;
        std::visit([&NewId](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (!std::is_same_v<T, std::monostate>)
                if (arg) arg->ItemId = NewId;
        }, Data);
    }

	bool IsBlocked() const 
    {
    	// Return false if bundle is FTaggedItemBundle and IsBlocked is true
    	return std::visit([](auto&& arg) -> bool {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, FTaggedItemBundle*>)
			{
				return arg ? arg->IsBlocked : false;
			}
			else
			{
				return false;
			}
		}, Data);
    }

	// Gets tag if bundle is FTaggedItemBundle
	FGameplayTag GetSlotTag() const
    {
    	return std::visit([](auto&& arg) -> FGameplayTag {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, FTaggedItemBundle*>)
			{
				return arg ? arg->Tag : FGameplayTag();
			}
			else
			{
				return FGameplayTag();
			}
		}, Data);
    }

	// Sets tag if bundle is FTaggedItemBundle
	void SetSlotTag(const FGameplayTag& NewTag)
    {
    	std::visit([&NewTag](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, FTaggedItemBundle*>)
			{
				if (arg) arg->Tag = NewTag;
			}
		}, Data);
    }
};
