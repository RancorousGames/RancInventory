// Copyright Rancorous Games, 2024

#include "WeaponActor.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "RecordingSystem/WeaponAttackRecorderComponent.h"

AWeaponActor::AWeaponActor(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer), WeaponData(nullptr), ItemData(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;
    SetMobility(EComponentMobility::Movable);
    GetStaticMeshComponent()->SetSimulatePhysics(false);
    GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
}

void AWeaponActor::BeginPlay()
{
    Super::BeginPlay();
    Initialize(true, true);
}

void AWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

   // DOREPLIFETIME(AWeaponActor, PlacedInWorld);
   // DOREPLIFETIME(AWeaponActor, MontageCycleIndex);
   // DOREPLIFETIME(AWeaponActor, WeaponData);
}

UNetConnection* AWeaponActor::GetNetConnection() const
{
    if (AActor* ParentActor = GetAttachParentActor())
    {
        return ParentActor->GetNetConnection();
    }
    return Super::GetNetConnection();
}

void AWeaponActor::Initialize_Implementation(bool InitializeWeaponData, bool InitializeStaticMesh)
{
    Initialize_Impl(InitializeWeaponData, InitializeStaticMesh);
}

void AWeaponActor::Initialize_Impl(bool InitializeWeaponData, bool InitializeStaticMesh)
{
    // Set static actor model based on weapon data
    if (ItemData && ItemData->ItemWorldMesh)
    {
        if (InitializeStaticMesh)
        {
            GetStaticMeshComponent()->SetStaticMesh(ItemData->ItemWorldMesh);
            GetStaticMeshComponent()->SetWorldScale3D(ItemData->ItemWorldScale);
            GetStaticMeshComponent()->SetSimulatePhysics(false);
            GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
        }
        if (InitializeWeaponData)
        {
            WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>(UWeaponDefinition::StaticClass());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: ItemData is nullptr."));
    }
}

bool AWeaponActor::CanAttack_Implementation()
{
    return CanAttack_Impl();
}

bool AWeaponActor::CanAttack_Impl()
{
    // Default implementation: Always return true
    return true;
}

void AWeaponActor::PerformAttack_Implementation()
{
    PerformAttack_Impl();
}


FTransform AWeaponActor::GetAttachTransform_Implementation(FName SocketName)
{
    return GetAttachTransform_Impl(SocketName);
}

FTransform AWeaponActor::GetAttachTransform_Impl(FName SocketName)
{
    if (const UStaticMeshComponent* Mesh = GetStaticMeshComponent())
    {
        if (const auto* Socket = Mesh->GetSocketByName(SocketName))
        {
            return FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale);
        }
    }
    return FTransform();
}

FMontageData AWeaponActor::GetAttackMontage_Implementation(int32 MontageIdOverride)
{
    return GetAttackMontage_Impl(MontageIdOverride);
}

FMontageData AWeaponActor::GetAttackMontage_Impl(int32 MontageIdOverride)
{
    if (WeaponData->AttackMontages.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::GetAttackMontage_Impl: No attack montages found."));
        return FMontageData();
    }
    
    if (MontageIdOverride >= 0)
    {
        MontageCycleIndex = MontageIdOverride;
    }
    else
    {
        MontageCycleIndex = (MontageCycleIndex+1) % WeaponData->AttackMontages.Num();
    }

    return WeaponData->AttackMontages[MontageCycleIndex];
}

void AWeaponActor::Equip()
{
    EquipMulticast();
    EquipMulticastImpl();
}

void AWeaponActor::EquipMulticast_Implementation()
{
    EquipMulticastImpl();
}

void AWeaponActor::EquipMulticastImpl()
{
    OnWeaponEquipped.Broadcast();
}

void AWeaponActor::EquipServer_Implementation()
{
    Equip();
}

void AWeaponActor::Holster()
{
    HolsterMulticast();
}

void AWeaponActor::HolsterMulticast_Implementation()
{
    OnWeaponHolstered.Broadcast();
}

void AWeaponActor::HolsterServer_Implementation()
{
    Holster();
}

void AWeaponActor::Remove()
{
    RemoveMulticast();
}

void AWeaponActor::RemoveMulticast_Implementation()
{
    // Placeholder for cleanup logic
}

void AWeaponActor::RemoveServer_Implementation()
{
    Remove();
}


void AWeaponActor::PerformAttack_Impl()
{
	// Get the current montage data
	FMontageData CurrentMontage = GetAttackMontage();

	// Perform normal attack logic
	if (CanAttack())
	{
		// Start playing animation and attack logic...

		// If the weapon has an attached recorder component, start recording
	    // I believe this is no longer used as we rely on anim notifies instead
		//if (UWeaponAttackRecorderComponent* Recorder = FindComponentByClass<UWeaponAttackRecorderComponent>())
		//{
		//	Recorder->Star();
		//}
	}
}