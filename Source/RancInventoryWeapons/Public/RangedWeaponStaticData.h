#pragma once

#include "CoreMinimal.h"
#include "WeaponStaticData.h"
#include "RangedWeaponStaticData.generated.h"

UCLASS(NotBlueprintable, NotPlaceable, Category = "RIS | Classes | Data")
class RANCINVENTORYWEAPONS_API URangedWeaponStaticData : public UWeaponStaticData
{
	GENERATED_BODY()

public:
	explicit URangedWeaponStaticData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()):
		MagazineSize(0), bInfiniteAmmo(false), bInfiniteReserve(false), bSpawnProjectiles(false),
		RandomSpreadDegrees(0), ReloadTime(0), bAutomaticReload(false), FalloffFactor(0), Damage(0), TraceChannel()	{}

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

	/** Damage value for ranged instant-hit (if applicable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon",
		meta = (UIMin = 0, ClampMin = 0, AssetBundles = "Data"))
	float Damage;

	/** Trace channel for hitscan weapons */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Reload animation montage data */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RIS | Ranged Weapon", meta = (AssetBundles = "Data"))
	FMontageData ReloadMontage;
};
