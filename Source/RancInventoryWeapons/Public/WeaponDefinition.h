#pragma once

#include "CoreMinimal.h"
#include "GearDefinition.h"
#include "Data/ItemStaticData.h"
#include "RISWeaponsDataTypes.h"
#include "Data/ItemDefinitionBase.h"
#include "WeaponDefinition.generated.h"

/**
 * UWeaponDefinition - Data definition class for weapon-specific data.
 */
UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew, Category = "RIS | Classes | Data")
class RANCINVENTORYWEAPONS_API UWeaponDefinition : public UGearDefinition
{
    GENERATED_BODY()

public:
    explicit UWeaponDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()):
	    HandCompatability(), Cooldown(0), Damage(0), Range(0), IsLowPriority(false)
    {
    }

    /** The hand compatibility of the weapon (e.g., mainhand, offhand, or two-handed) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AssetBundles = "Data"))
    EHandCompatibility HandCompatability;

	/** The hand compatibility of the weapon (e.g., mainhand, offhand, or two-handed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AssetBundles = "Data"))
	FGameplayTag WeaponType;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AssetBundles = "Data"))
	TArray<FAttackMontageData> AttackMontages;
	
    /** Cooldown time between attacks */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float Cooldown;

    /** Base damage dealt by the weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float Damage;

    /** Range of the weapon for attacks */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float Range;

	// If a weapon is low priority then it will be readily replaced by other non-low priority weapons
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AssetBundles = "Data"))
	bool IsLowPriority;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AssetBundles = "Data"))
	TSubclassOf<AActor> WeaponActorClass;
};