#pragma once

#include "CoreMinimal.h"
#include "WeaponDefinition.h"
#include "RangedWeaponDefinition.generated.h"

/**
 * URangedWeaponDefinition - Data definition class for ranged weapon-specific data.
 */
UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew, Category = "RIS | Classes | Data")
class RANCINVENTORYWEAPONS_API URangedWeaponDefinition : public UWeaponDefinition
{
    GENERATED_BODY()

public:
    explicit URangedWeaponDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
        : MagazineSize(0), bInfiniteAmmo(false), bInfiniteReserve(false), bSpawnProjectiles(false),
          RandomSpreadDegrees(0.0f), ReloadTime(0.0f), bAutomaticReload(false), FalloffFactor(0.0f), TraceChannel()
    {
    }

    /** The size of the magazine */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon",
        meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    int32 MagazineSize;

    /** Whether the weapon has infinite ammo */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    bool bInfiniteAmmo;

    /** Whether the weapon has infinite reserve ammo */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    bool bInfiniteReserve;

    /** The class of projectile this weapon spawns */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    TSubclassOf<AActor> ProjectileClass;

    /** Whether the weapon spawns projectiles or uses hitscan */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    bool bSpawnProjectiles;

    /** The random spread of the weapon in degrees */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon",
        meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float RandomSpreadDegrees;

    /** The time it takes to reload the weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon",
        meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float ReloadTime;

    /** Whether the weapon reloads automatically when out of ammo */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    bool bAutomaticReload;

    /** The tag identifying the ammo item used by this weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    FGameplayTag AmmoItemId;

    /** Socket name for spawning projectiles or defining the fire origin */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    FName ArrowSpawnSocketName;

    /** Falloff factor for projectile spread at maximum range */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon",
        meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
    float FalloffFactor;

    /** Trace channel for hitscan weapons */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    TEnumAsByte<ECollisionChannel> TraceChannel;

    /** Reload animation montage data */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
    FMontageData ReloadMontage;
};