#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>

#include "ItemDefinitionBase.h"
#include "Actors/WorldItem.h"
#include "Data/RISDataTypes.h"
#include "ItemStaticData.generated.h"

class UUsableItemDefinition;
class UTexture2D;
class UStaticMesh;

UENUM(BlueprintType)
enum class EFoundState : uint8
{
	Found UMETA(DisplayName = "Found"),
	NotFound UMETA(DisplayName = "Not Found")
};

UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew, CollapseCategories, HideCategories = Object, Abstract)
class UTestBaseClass : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS | Classes | Data")
	int32 TestInt = 0;
};

UCLASS(Blueprintable, editinlinenew)
class  USubClassTest : public UTestBaseClass
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
	FVector SubClassTest;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Test")
	FVector SubClassTestX;
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
	int32 MaxStackSize = 1;

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
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "RIS")
	TArray<UItemDefinitionBase*> ItemDefinitions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	FVector ItemWorldScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "RIS", meta = (AssetBundles = "Data"))
	TSubclassOf<AWorldItem> WorldItemClassOverride;

	/* If this is set to a class, then dynamicitem instances will be created with this class as the instance data class
	 * E.g. to track dynamic durability or randomized stats per item instance
	 */
	UPROPERTY(EditDefaultsOnly, Category = "RIS", meta = (AssetBundles = "Data"))
	TSubclassOf<UItemInstanceData> ItemInstanceDataClass;	
	
	/* Allows to implement custom properties in this item data */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS",
		meta = (DisplayName = "Custom Metadatas", AssetBundles = "Custom"))
	TMap<FGameplayTag, FName> Metadatas;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "RIS", meta = (DeterminesOutputType = "Definition"))
	UObject* GetItemDefinition(class TSubclassOf<UItemDefinitionBase> Definition, bool& Found)
	{
		for (UItemDefinitionBase* ItemDefinition : ItemDefinitions)
		{
			if (ItemDefinition && ItemDefinition->IsA(Definition))
			{
				Found = true;
				return ItemDefinition;
			}
		}
		
		Found = false;
		return nullptr;
	}

	template<typename T>
	T* GetItemDefinition() const
		{
			static_assert(std::is_base_of_v<UItemDefinitionBase, T>, 
						  "T must derive from UItemDefinitionBase");
	                  
			TSubclassOf<T> Definition = T::StaticClass();
	    
			for (UItemDefinitionBase* ItemDefinition : ItemDefinitions)
			{
				if (ItemDefinition && ItemDefinition->IsA(Definition))
				{
					return Cast<T>(ItemDefinition);
				}
			}

			return nullptr;
		}
};
