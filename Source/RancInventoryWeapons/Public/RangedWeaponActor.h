#pragma once

#include "CoreMinimal.h"
#include "RangedWeaponDefinition.h"
#include "WeaponActor.h"
#include "RangedWeaponStaticData.h"
#include "Components/ItemContainerComponent.h"
#include "RangedWeaponActor.generated.h"

UCLASS()
class RANCINVENTORYWEAPONS_API ARangedWeaponActor : public AWeaponActor
{
    GENERATED_BODY()

public:
    ARangedWeaponActor(const FObjectInitializer& ObjectInitializer);

    virtual bool CanAttack_Impl() override;
    virtual void PerformAttack_Impl() override;
	
protected:
    virtual void BeginPlay() override;

    virtual void Initialize_Impl(bool InitializeWeaponData = true, bool InitializeStaticMesh = true) override;

    void EquipMulticastImpl();
    void ReloadWeapon();
    void OnReloadComplete_IfServer();
    
   // UFUNCTION(Client, Reliable)
   // void OnReloadComplete_Client(int32 AmmoToReload);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category= "Ranged Weapon")
    void ApplyInstantHit(FHitResult HitResult);
    
    void ApplyInstantHit_Impl(FHitResult HitResult);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    const URangedWeaponDefinition* RangedWeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    UItemContainerComponent* InternalMagazineAmmoContainer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    UItemContainerComponent* ReserveAmmoContainer;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    FVector FireOriginOffset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    int32 CurrentAmmo;

    bool bIsReloading;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    ACharacter* WeaponHolder;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranged Weapon")
    AController* InstigatingController;

    UFUNCTION(BlueprintCallable, Category = "Ranged Weapon")
    float PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName, bool bShowDebugWarnings = true);
};