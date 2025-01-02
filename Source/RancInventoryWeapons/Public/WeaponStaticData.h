#pragma once

#include "CoreMinimal.h"
#include "Data/ItemStaticData.h"
#include "RISWeaponsDataTypes.h"
#include "WeaponStaticData.generated.h"

/**
 * UWeaponData - Data asset class for weapon-specific data, derived from UItemStaticData.
 */

UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORYWEAPONS_API UWeaponStaticData : public UItemStaticData
{
    GENERATED_BODY()

public:
    explicit UWeaponStaticData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()):
	    HandCompatability(), Cooldown(0), BaseDamage(0), Range(0), WeaponWeight(0), WeaponMesh(nullptr) {}

    /** The hand compatibility of the weapon (e.g., mainhand, offhand, or two-handed) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
    EHandCompatibility HandCompatability;

    /** Animation montage data for equipping the weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
    FMontageData EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
	FMontageData HolsterMontage;
	
    /** Cooldown time between attacks */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float Cooldown;

    /** Base damage dealt by the weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float BaseDamage;

    /** Range of the weapon for attacks */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float Range;

    /** Weight of the weapon, contributing to inventory weight limits */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float WeaponWeight;

    /** A static mesh representing the weapon in the world */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
    UStaticMesh* WeaponMesh;

    /** The socket name where the weapon will be attached to the character */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
    FName AttachSocketName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (AssetBundles = "Data"))
	TSubclassOf<AActor> WeaponActorClass;
	
    /** Allows implementation of custom weapon-specific metadata */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (DisplayName = "Weapon Metadatas", AssetBundles = "Custom"))
    TMap<FGameplayTag, FName> WeaponMetadatas;

    /** Customizable weapon attributes, such as elemental damage types */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Weapon", meta = (DisplayName = "Custom Weapon Attributes", AssetBundles = "Custom"))
    TMap<FGameplayTag, float> WeaponAttributes;
};