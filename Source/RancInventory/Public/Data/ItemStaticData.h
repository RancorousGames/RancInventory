#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>

#include "Actors/WorldItem.h"
#include "Data/RISDataTypes.h"
#include "ItemStaticData.generated.h"

class UTexture2D;
class UStaticMesh;

UENUM(BlueprintType)
enum class EFoundState : uint8
{
	Found UMETA(DisplayName = "Found"),
	NotFound UMETA(DisplayName = "Not Found")
};


UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API UItemStaticData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	explicit UItemStaticData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()): ItemValue(0),
		ItemWeight(0){}

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	float ItemValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	float ItemWeight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	int32 JigsawSizeX = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	int32 JigsawSizeY = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "UI"))
	TSoftObjectPtr<UTexture2D> ItemIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
	FGameplayTagContainer ItemCategories;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS", meta = (AssetBundles = "Data"))
	UStaticMesh* ItemWorldMesh = nullptr;
	
	/* Allows extending item data without inheritance. Similar to components */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (DisplayName = "Item definitions"))
	TMap<TSubclassOf<UObject>, UObject*> ItemDefinitions;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	FVector ItemWorldScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "RIS", meta = (AssetBundles = "Data"))
	TSubclassOf<AWorldItem> WorldItemClassOverride;
	
	/* Allows to implement custom properties in this item data */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (DisplayName = "Custom Metadatas", AssetBundles = "Custom"))
	TMap<FGameplayTag, FName> Metadatas;


	/* Map containing a tag as key and a ID container as value to add relations to other items such as crafting requirements, etc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (DisplayName = "Item Relations", AssetBundles = "Custom"))
	TMap<FGameplayTag, FPrimaryRISItemIdContainer> Relations;
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "Definition", ExpandEnumAsExecs = "Found"))
	UObject* GetItemDefinition(class TSubclassOf<UObject> Definition, EFoundState& Found)
	{
		if (ItemDefinitions.Contains(Definition))
		{
			Found = EFoundState::Found;
			return ItemDefinitions[Definition];
		}
		
		Found = EFoundState::NotFound;
		return nullptr;
	}
};
