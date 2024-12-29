#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Engine/DataAsset.h>
#include "ItemStaticData.generated.h"

UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORY_API UItemStaticData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    explicit UItemStaticData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

