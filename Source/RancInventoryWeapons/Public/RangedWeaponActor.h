#pragma once

#include "CoreMinimal.h"
#include "WeaponActor.h"
#include "RangedWeaponStaticData.h"
#include "Components/ItemContainerComponent.h"
#include "RangedWeaponActor.generated.h"

UCLASS()
class YOURGAME_API ARangedWeaponActor : public AWeaponActor
{
    GENERATED_BODY()

public:
    ARangedWeaponActor(const FObjectInitializer& ObjectInitializer);

    virtual bool CanAttack_Impl() override;
    virtual void PerformAttack_Impl() override;
	
protected:
    virtual void BeginPlay() override;

    virtual void Initialize_Impl() override;

    void EquipMulticastImpl();
    void ReloadWeapon();
    void OnReloadComplete_IfServer();
    UFUNCTION(Client, Reliable)
    void OnReloadComplete_Client(int32 AmmoToReload);

    void ApplyInstantHit_Implementation(FHitResult HitResult);
    void ApplyInstantHit_Impl(FHitResult HitResult);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    URangedWeaponStaticData* RangedWeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    UItemContainerComponent* InternalMagazineAmmoContainer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    UItemContainerComponent* ReserveAmmoContainer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    FVector FireOriginOffset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    int32 CurrentAmmo;

    bool bIsReloading;

    UFUNCTION(BlueprintCallable, Category = "Ranged Weapon")
    float PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName, bool bShowDebugWarnings = true);
};