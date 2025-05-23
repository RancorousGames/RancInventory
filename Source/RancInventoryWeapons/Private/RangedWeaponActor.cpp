﻿#include "RangedWeaponActor.h"

#include "LogRancInventorySystem.h"
#include "Components/ItemContainerComponent.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/Character.h"

void ARangedWeaponActor::BeginPlay()
{
    Super::BeginPlay();
}

ARangedWeaponActor::ARangedWeaponActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), RangedWeaponData(nullptr), InternalMagazineAmmoContainer(nullptr), ReserveAmmoContainer(nullptr)
{
}

void ARangedWeaponActor::Initialize_Implementation(bool InitializeWeaponData, bool InitializeStaticMesh)
{
    Super::Initialize_Implementation(InitializeWeaponData, InitializeStaticMesh);
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("Initialize_Impl called for %s"), *GetName());

    InternalMagazineAmmoContainer = GetComponentByClass<UItemContainerComponent>();
    if (!InternalMagazineAmmoContainer)
    {
        InternalMagazineAmmoContainer = NewObject<UItemContainerComponent>(this, UItemContainerComponent::StaticClass());
        InternalMagazineAmmoContainer->RegisterComponent();
        UE_LOG(LogRancInventorySystem, Warning, TEXT("MagazineAmmoContainer created for %s"), *GetName());
    }

    WeaponHolder = Cast<ACharacter>(GetOwner());
    if (WeaponHolder)
    {
        InstigatingController = WeaponHolder->GetInstigatorController();
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("WeaponHolder and InstigatingController set for %s"), *GetName());
    }
    
    if (URISSubsystem::Get(this)->AreAllItemsLoaded())
    {
        RangedWeaponData = Cast<URangedWeaponDefinition>(WeaponData);

        if (!WeaponData || !RangedWeaponData)
        {
            UE_LOG(LogRancInventorySystem, Error, TEXT("WeaponData is not of type URangedWeaponStaticData for ranged weapon actor %s"), *GetName());
            return;
        }

        if (RangedWeaponData && !RangedWeaponData->bInfiniteReserve && !ReserveAmmoContainer && WeaponHolder)
        {
            ReserveAmmoContainer = WeaponHolder->FindComponentByClass<UItemContainerComponent>();
            UE_LOG(LogRancInventorySystem, Verbose, TEXT("ReserveAmmoContainer set for %s"), *GetName());
        }

        if (IsValid(GetStaticMeshComponent()->GetStaticMesh()))
        {
            if (UStaticMeshSocket* socket = GetStaticMeshComponent()->GetStaticMesh()->FindSocket(RangedWeaponData->ArrowSpawnSocketName))
            {
                FireOriginOffset = socket->RelativeLocation;
                UE_LOG(LogRancInventorySystem, Verbose, TEXT("FireOriginOffset set from StaticMesh for %s"), *GetName());
            }
        }
        else
        {
            USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetComponentByClass(USkeletalMeshComponent::StaticClass()));
            if (SkeletalMeshComponent)
            {
                if (USkeletalMeshSocket* socket = SkeletalMeshComponent->GetSkinnedAsset()->FindSocket(RangedWeaponData->ArrowSpawnSocketName))
                {
                    FireOriginOffset = socket->RelativeLocation;
                    UE_LOG(LogRancInventorySystem, Verbose, TEXT("FireOriginOffset set from SkeletalMesh for %s"), *GetName());
                }
            }
        }
    }
}

bool ARangedWeaponActor::CanAttack_Implementation()
{
    bool bCanAttack = RangedWeaponData && RangedWeaponData->bInfiniteAmmo || CurrentAmmo > 0;
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("CanAttack_Impl called for %s, result: %s"), *GetName(), bCanAttack ? TEXT("true") : TEXT("false"));

    if (!bCanAttack && RangedWeaponData->bAutomaticReload && CurrentAmmo == 0)
    {
        ReloadWeapon();
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("Automatic reload initiated for %s"), *GetName());
    }
    
    return bCanAttack;
}

void ARangedWeaponActor::OnAttackPerformed_Implementation()
{
    Super::OnAttackPerformed_Implementation();
    
    if (CurrentAmmo <= 0 || !RangedWeaponData)
    {
        UE_LOG(LogRancInventorySystem, Warning, TEXT("PerformAttack_Impl called for %s but no ammo or RangedWeaponData"), *GetName());
        return;
    }

    FVector Start = GetActorLocation();
    FVector FireOrigin;

    // Get the fire origin from the socket location
    if (IsValid(GetStaticMeshComponent()->GetStaticMesh()))
    {
        if (UStaticMeshSocket* Socket = GetStaticMeshComponent()->GetStaticMesh()->FindSocket(RangedWeaponData->ArrowSpawnSocketName))
        {
            FTransform socketTransform;
            Socket->GetSocketTransform(socketTransform, GetStaticMeshComponent());
            FireOrigin = socketTransform.GetLocation();
        }
    }
    else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetComponentByClass(USkeletalMeshComponent::StaticClass())))
    {
        if (USkeletalMeshSocket* Socket = SkeletalMeshComponent->GetSkinnedAsset()->FindSocket(RangedWeaponData->ArrowSpawnSocketName))
        {
            FireOrigin = Socket->GetSocketLocation(SkeletalMeshComponent);
        }
    }

    if (FireOrigin.IsZero())
    {
        FireOrigin = GetActorLocation(); // Fallback to actor location if no socket found
    }

    if (GetLocalRole() == ROLE_Authority)
    {
        if (RangedWeaponData->bSpawnProjectiles)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = WeaponHolder;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            FVector End = FireOrigin + GetActorForwardVector() * RangedWeaponData->Range;
            FVector Direction = End - FireOrigin;
            Direction.Normalize();

            FVector Spread = FMath::VRandCone(Direction, FMath::DegreesToRadians(RangedWeaponData->RandomSpreadDegrees));
            FVector EndPoint = FireOrigin + Direction * RangedWeaponData->Range + Spread * RangedWeaponData->Range * RangedWeaponData->FalloffFactor;
            FRotator SpawnRotation = FRotationMatrix::MakeFromZ(EndPoint - FireOrigin).Rotator();

            if (AActor* Projectile = GetWorld()->SpawnActor<AActor>(RangedWeaponData->ProjectileClass.Get(), FireOrigin, SpawnRotation, SpawnParams))
            {
                Projectile->SetOwner(this);
                Projectile->SetInstigator(WeaponHolder);
                UE_LOG(LogRancInventorySystem, Verbose, TEXT("Projectile spawned by %s"), *GetName());
            }

            // Debug lines
            DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + GetActorForwardVector() * RangedWeaponData->Range, FColor::Red, false, 5, 0, 2);
            DrawDebugLine(GetWorld(), FireOrigin, End, FColor::Yellow, false, 5, 0, 2);
        }
        else
        {
            FHitResult HitResult;
            FVector End = FireOrigin + GetActorForwardVector() * RangedWeaponData->Range;
            if (RangedWeaponData->RandomSpreadDegrees > 0)
            {
                FVector Direction = End - FireOrigin;
                Direction.Normalize();
                FVector SpreadDirection = FMath::VRandCone(Direction, FMath::DegreesToRadians(RangedWeaponData->RandomSpreadDegrees));
                End = FireOrigin + SpreadDirection * RangedWeaponData->Range;
            }

            FCollisionQueryParams CollisionParams;
            CollisionParams.AddIgnoredActor(this);
            CollisionParams.AddIgnoredActor(WeaponHolder);
            GetWorld()->LineTraceSingleByChannel(HitResult, FireOrigin, End, RangedWeaponData->TraceChannel, CollisionParams);
            ApplyInstantHit(HitResult);
            UE_LOG(LogRancInventorySystem, Verbose, TEXT("Line trace performed by %s"), *GetName());
        }
    }

    CurrentAmmo--;
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("CurrentAmmo decremented for %s, new value: %d"), *GetName(), CurrentAmmo);
    if (CurrentAmmo <= 0 && RangedWeaponData->bAutomaticReload)
    {
        ReloadWeapon();
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("Automatic reload initiated for %s"), *GetName());
    }
	/*
    if (CurrentAmmo <= 0 && !RangedWeaponData)
    {
        UE_LOG(LogRancInventorySystem, Warning, TEXT("PerformAttack_Impl called for %s but no ammo or RangedWeaponData"), *GetName());
        return;
    }

    if (GetLocalRole() == ROLE_Authority)
    {
        if (RangedWeaponData->bSpawnProjectiles)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = WeaponHolder;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            FVector RotatedOffset = GetActorRotation().RotateVector(FireOriginOffset);
            FVector Start = GetActorLocation() + RotatedOffset;
            FVector End = Start + GetActorForwardVector() * RangedWeaponData->Range;
            FVector Direction = End - Start;
            Direction.Normalize();
            FVector Spread = FMath::VRandCone(Direction, FMath::DegreesToRadians(RangedWeaponData->RandomSpreadDegrees));
            FVector EndPoint = Start + Direction * RangedWeaponData->Range + Spread * RangedWeaponData->Range * RangedWeaponData->FalloffFactor;
            FRotator SpawnRotation = FRotationMatrix::MakeFromZ(EndPoint - Start).Rotator();

            FVector RelativePosition(10, 0, 0); // Relative position vector
            FVector WorldPosition(0, 0, 0); // World position vector
            FRotator WorldRotation(0, 90, 0); // World rotation (pitch, yaw, roll)

            // Convert the rotation to a quaternion or rotation matrix to transform the relative position
            FVector TransformedPosition = WorldRotation.RotateVector(RelativePosition);

            // Add the transformed relative position to the world position
            FVector NewWorldPosition = WorldPosition + TransformedPosition;
            
            
            if (AActor* Projectile = GetWorld()->SpawnActor<AActor>(RangedWeaponData->ProjectileClass.Get(), Start, SpawnRotation, SpawnParams))
            {
                Projectile->SetOwner(this);
                Projectile->SetInstigator(WeaponHolder);
                UE_LOG(LogRancInventorySystem, Verbose, TEXT("Projectile spawned by %s"), *GetName());
            }

            
            // Draw a debugline from actor position towards actor forward vector
            DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + GetActorForwardVector() * RangedWeaponData->Range, FColor::Red, false, 5, 0, 2);
            DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 5, 0, 2);
        }
        else
        {
            FHitResult HitResult;
            FVector Start = GetActorLocation() + FireOriginOffset;
            FVector End = Start + GetActorForwardVector() * RangedWeaponData->Range;
            if (RangedWeaponData->RandomSpreadDegrees > 0)
            {
                FVector Direction = End - Start;
                Direction.Normalize();
                FVector SpreadDirection = FMath::VRandCone(Direction, FMath::DegreesToRadians(RangedWeaponData->RandomSpreadDegrees));
                End = Start + SpreadDirection * RangedWeaponData->Range;
            }

            FCollisionQueryParams CollisionParams;
            CollisionParams.AddIgnoredActor(this);
            CollisionParams.AddIgnoredActor(WeaponHolder);
            GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, RangedWeaponData->TraceChannel, CollisionParams);
            ApplyInstantHit(HitResult);
            UE_LOG(LogRancInventorySystem, Verbose, TEXT("Line trace performed by %s"), *GetName());
        }
    }

    CurrentAmmo--;
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("CurrentAmmo decremented for %s, new value: %d"), *GetName(), CurrentAmmo);
    if (CurrentAmmo <= 0)
    {
        if (RangedWeaponData->bAutomaticReload)
        {
            ReloadWeapon();
            UE_LOG(LogRancInventorySystem, Verbose, TEXT("Automatic reload initiated for %s"), *GetName());
        }
    }*/
}

void ARangedWeaponActor::Equip_Impl_Implementation()
{
    Super::Equip_Impl_Implementation();

    if (!RangedWeaponData)
    {
        UE_LOG(LogRancInventorySystem, Warning, TEXT("EquipMulticastImpl called for %s but no RangedWeaponData"), *GetName());
        return;
    }
    
    if (!RangedWeaponData->bInfiniteAmmo && CurrentAmmo <= 0)
    {
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ARangedWeaponActor::ReloadWeapon, 1, false);
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("ReloadWeapon timer set for %s"), *GetName());
    }
}

void ARangedWeaponActor::ReloadWeapon()
{
    if (bIsReloading) return;

    if ((!ReserveAmmoContainer && !RangedWeaponData->bInfiniteReserve) ||
        ReserveAmmoContainer->GetQuantityTotal_Implementation(RangedWeaponData->AmmoItemId) <= 0)
        return;

    bIsReloading = true;
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("Reloading started for %s"), *GetName());

    float ReloadAnimDuration = PlayMontage(WeaponHolder, RangedWeaponData->ReloadMontage.Montage, RangedWeaponData->ReloadMontage.PlayRate, NAME_None, false);
    float ReloadCallbackDuration = RangedWeaponData->ReloadTime > 0 ? RangedWeaponData->ReloadTime : ReloadAnimDuration;

    if (GetLocalRole() == ROLE_Authority && ReloadCallbackDuration > 0)
    {
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, this, &ARangedWeaponActor::OnReloadComplete_IfServer, ReloadCallbackDuration, false);
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("Reload callback timer set for %s"), *GetName());
    }
}

void ARangedWeaponActor::OnReloadComplete_IfServer()
{
    if (GetLocalRole() != ROLE_Authority) return;

    bIsReloading = false;
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("Reloading completed for %s"), *GetName());
   

    if (RangedWeaponData->bInfiniteReserve)
    {
        CurrentAmmo = RangedWeaponData->MagazineSize;
    }
    else
    {
        if (!ReserveAmmoContainer)
        {
            UE_LOG(LogRancInventorySystem, Warning, TEXT("No ammo reserve container set for weapon %s"), *GetName());
            return;
        }
        
        CurrentAmmo = InternalMagazineAmmoContainer->AddItem_IfServer(ReserveAmmoContainer, RangedWeaponData->AmmoItemId, RangedWeaponData->MagazineSize, true);
    }

 //   OnReloadComplete_Client(CurrentAmmo);
}

//void ARangedWeaponActor::OnReloadComplete_Client_Implementation(int32 AmmoToReload)
//{
//    CurrentAmmo = AmmoToReload;
//    UE_LOG(LogRancInventorySystem, Verbose, TEXT("OnReloadComplete_Client called for %s, ammo reloaded: %d"), *GetName(), AmmoToReload);
//}

void ARangedWeaponActor::ApplyInstantHit_Implementation(FHitResult HitResult)
{
    ApplyInstantHit_Impl(HitResult);
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("ApplyInstantHit_Implementation called for %s"), *GetName());
}

void ARangedWeaponActor::ApplyInstantHit_Impl(FHitResult HitResult)
{
    if (HitResult.GetActor())
    {
        HitResult.GetActor()->TakeDamage(RangedWeaponData->Damage, FDamageEvent(), InstigatingController, this);
        UE_LOG(LogRancInventorySystem, Verbose, TEXT("Damage applied to %s by %s"), *HitResult.GetActor()->GetName(), *GetName());
    }
}

float ARangedWeaponActor::PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName, bool bShowDebugWarnings)
{
    if (!OwnerChar || !Montage || PlayRate < 0.0f)
    {
        if (bShowDebugWarnings)
        {
            UE_LOG(LogRancInventorySystem, Warning, TEXT("Invalid parameters for PlayMontage!"))
        }

        return 0.0f;
    }

    const float duration = OwnerChar->PlayAnimMontage(Montage, PlayRate, StartSectionName);
    UE_LOG(LogRancInventorySystem, Verbose, TEXT("RangedWeaponActor PlayMontage called for %s, montage: %s"), *OwnerChar->GetName(), *Montage->GetName());

    return duration;
}