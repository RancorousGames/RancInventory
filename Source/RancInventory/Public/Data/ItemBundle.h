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

	static const TArray<UItemInstanceData*> NoInstances;
    static const FItemBundle EmptyItemInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	FGameplayTag ItemId;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	int32 Quantity = 0;

	// For instanced items, this will contain the instances. for non instanced items this will be an empty array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	TArray<UItemInstanceData*> InstanceData;
    
    bool IsValid() const;

    int32 DestroyQuantity(int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDestroy, AActor* Owner);

	// Allows partial. If InstancesToExtract are provided then InQuantity is ignored
    int32 Extract(int32 InQuantity, const TArray<UItemInstanceData*>& InstancesToExtract, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner, bool bAllowPartial = true);

	// Checks if at least QuantityToCheck exists AND all provided (if any) instances are contained
	bool Contains(int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck) const;
	
    static TArray<int32> ToInstanceIds(const TArray<UItemInstanceData*> Instances);
	TArray<UItemInstanceData*> FromInstanceIds(const TArray<int32> InstanceIds) const;

    TArray<UItemInstanceData*> GetInstancesFromEnd(int32 Quantity) const;
	
    FItemBundle(){}

	FItemBundle(FGameplayTag InItemId)
    {
    	ItemId = InItemId;
    	Quantity = 0;
    	InstanceData = TArray<UItemInstanceData*>();
    }
	
    FItemBundle(FItemBundle InItemInfo, const TArray<UItemInstanceData*>& InstanceData)
    {
        ItemId = InItemInfo.ItemId;
    	Quantity = InItemInfo.Quantity;
        this->InstanceData = InstanceData;
    }

    FItemBundle(FGameplayTag InItemId, int32 InQuantity, const TArray<UItemInstanceData*>& InstanceData)
    {
        ItemId = InItemId;
    	Quantity = InQuantity;
        this->InstanceData = InstanceData;
    }

	// WARNING: This constructor should only be used when it is certain that instance data is not needed for this item type
	FItemBundle(FGameplayTag InItemId, int32 InQuantity);

    // This overload will spawn the instance data
	FItemBundle(FGameplayTag InItemId, int32 InQuantity, TSubclassOf<UItemInstanceData> InstanceDataClass, AActor* OwnerOfSpawnedInstanceData)
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
    
    bool operator==(const FItemBundle& Other) const
    {
        return InstanceData == Other.InstanceData && ItemId == Other.ItemId && Quantity == Other.Quantity;
    }

    bool operator!=(const FItemBundle& Other) const
    {
        return !(*this == Other);
    }

    //bool operator<(const FItemBundle& Other) const
    //{
    //    return InstanceData->ToString() < Other.InstanceData->ToString();
    //}
};


USTRUCT(BlueprintType, Category = "RIS | Structs")
struct RANCINVENTORY_API FTaggedItemBundle
{
    GENERATED_BODY()

    static const FTaggedItemBundle EmptyItemInstance;
    
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	FGameplayTag ItemId;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	int32 Quantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS")
	FGameplayTag Tag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RIS")
	bool IsBlocked = false;
	
	// For instanced items, this will contain the instances. for non instanced items this will be an empty array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS")
	TArray<UItemInstanceData*> InstanceData;
	
    bool IsValid() const;

    int32 DestroyQuantity(int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDestroy, AActor* Owner);
	
	// Allows partial. If InstancesToExtract are provided then InQuantity is ignored
    int32 Extract(int32 InQuantity, const TArray<UItemInstanceData*>& SpecificInstancesToExtract, TArray<UItemInstanceData*>& StateArrayToAppendTo, AActor* Owner, bool bAllowPartial = true);

    FTaggedItemBundle(){}
	
	// Checks if at least QuantityToCheck exists AND all provided (if any) instances are contained
	
	bool Contains(int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck) const;
	
	TArray<UItemInstanceData*> FromInstanceIds(const TArray<int32>& InstanceIds) const;

	
	FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId)
    {
    	Tag = InTag;
    	ItemId = InItemId;
    	Quantity = 0;
    	InstanceData = TArray<UItemInstanceData*>();
    }
	
	// This constructor should only be used when it is certain that instance data is not needed for this item type
	FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity)
    {
    	ItemId = InItemId;
    	Quantity = InQuantity;
    	Tag = InTag;
    	InstanceData = TArray<UItemInstanceData*>();
    }
	
    FTaggedItemBundle(FGameplayTag InTag, FItemBundle InItemInfo)
    {
    	Tag = InTag;
        ItemId = InItemInfo.ItemId;
    	Quantity = InItemInfo.Quantity;
        this->InstanceData = InItemInfo.InstanceData;
    }

    FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity, const TArray<UItemInstanceData*>& InstanceData)
    {
    	Tag = InTag;
        ItemId = InItemId;
    	Quantity = InQuantity;
        this->InstanceData = InstanceData;
    }

    // This overload will spawn the instance data
	FTaggedItemBundle(FGameplayTag InTag, FGameplayTag InItemId, int32 InQuantity, TSubclassOf<UItemInstanceData> InstanceDataClass, AActor* OwnerOfSpawnedInstanceData)
    {
    	Tag = InTag;
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
    
    bool operator==(const FTaggedItemBundle& Other) const
    {
        return Tag == Other.Tag && InstanceData == Other.InstanceData && ItemId == Other.ItemId && Quantity == Other.Quantity;
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


// Class to wrap any of the above item bundle types in a unified type since we cant use inheritance
struct RANCINVENTORY_API FGenericItemBundle
{
    using BundleVariant = std::variant<FItemBundle*, FTaggedItemBundle*, std::monostate>;
    
    BundleVariant Data;

    FGenericItemBundle() : Data(std::monostate{}) {}  // Initialize with monostate instead of nullptr
    FGenericItemBundle(FItemBundle* Bundle) : Data(Bundle ? Bundle : BundleVariant(std::monostate{})) {}
    FGenericItemBundle(FTaggedItemBundle* Bundle) : Data(Bundle ? Bundle : BundleVariant(std::monostate{})) {}

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

	bool Contains(int32 QuantityToCheck, const TArray<UItemInstanceData*>& InstancesToCheck) const 
    {
    	if (std::holds_alternative<std::monostate>(Data)) return false;
    	return std::visit([QuantityToCheck, InstancesToCheck](auto&& arg) -> bool {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return false;
			else
				return arg && arg->Contains(QuantityToCheck, InstancesToCheck);
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

	TArray<UItemInstanceData*>* GetInstances() const 
	{
		if (std::holds_alternative<std::monostate>(Data)) return nullptr;
		return std::visit([]<typename T0>(T0&& arg) -> TArray<UItemInstanceData*>* {
			using T = std::decay_t<T0>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return nullptr;
			else
				return arg ? &arg->InstanceData : nullptr;
		}, Data);
	}

	TArray<UItemInstanceData*> FromInstanceIds(TArray<int32> Ids) const 
    {
    	if (std::holds_alternative<std::monostate>(Data)) return TArray<UItemInstanceData*>();
    	return std::visit([Ids]<typename T0>(T0&& arg) -> TArray<UItemInstanceData*> {
			using T = std::decay_t<T0>;
			if constexpr (std::is_same_v<T, std::monostate>)
				return TArray<UItemInstanceData*>();
			else
				return arg ? arg->FromInstanceIds(Ids) : TArray<UItemInstanceData*>();
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
	
	void SetInstances(const TArray<UItemInstanceData*>& NewInstances)
	{
		if (std::holds_alternative<std::monostate>(Data)) return;
		std::visit([&NewInstances](auto&& arg) {
    				using T = std::decay_t<decltype(arg)>;
					if constexpr (!std::is_same_v<T, std::monostate>)
						if (arg) arg->InstanceData = NewInstances;
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
