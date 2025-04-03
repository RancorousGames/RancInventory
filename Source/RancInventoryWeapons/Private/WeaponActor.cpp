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

    bReplicates = true;
    bNetLoadOnClient = true;
    AActor::SetReplicateMovement(false);
}

void AWeaponActor::BeginPlay()
{
    Super::BeginPlay();
    Initialize(true, true);
}

void AWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

   DOREPLIFETIME_CONDITION(AWeaponActor, ItemId, COND_InitialOnly);
   DOREPLIFETIME_CONDITION(AWeaponActor, HandSlotIndex, COND_InitialOnly);
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
    // I dont remember why InitializeWeaponData and InitializeStaticMesh were added
    
    // Set static actor model based on weapon data
    if (!ItemId.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: ItemId is invalid."));
        return;
    }

    if (!ItemData)
    {
        ItemData = URISSubsystem::GetItemDataById(ItemId);
    }
    
    if (ItemData)
    {
        if (InitializeStaticMesh && ItemData->ItemWorldMesh)
        {
            GetStaticMeshComponent()->SetStaticMesh(ItemData->ItemWorldMesh);
            SetActorScale3D(ItemData->ItemWorldScale); // This is overwritten when attaching, see GetAttachTransform_Impl
            GetStaticMeshComponent()->SetSimulatePhysics(false);
            GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            GetStaticMeshComponent()->SetCollisionProfileName(TEXT("NoCollision"));
        }
        
        if (InitializeWeaponData)
        {
            WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>();

            if (!WeaponData)
            {
                UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: Item %s Does not have weapon definition."), *ItemId.ToString());
            }
        }

        if (GetOwner())
        {
            if (auto* GearManager = GetOwner()->FindComponentByClass<UGearManagerComponent>())
            {
                GearManager->RegisterSpawnedWeapon(this);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: Owner %s has no GearManagerComponent."), *GetOwner()->GetName());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: Weapon %s has no owner."), *ItemId.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::Initialize_Impl: Failed to load itemdata for item %s."), *ItemId.ToString());
    }
}

bool AWeaponActor::CanAttack_Implementation()
{
    // Default implementation: Always return true
    return true;
}

void AWeaponActor::OnAttackPerformed_Implementation()
{
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
            return FTransform(Socket->RelativeRotation, Socket->RelativeLocation, ItemData->ItemWorldScale);
        }
    }
    auto Transform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, ItemData->ItemWorldScale);
    return Transform;
}


int32 AWeaponActor::GetAttackMontageId_Implementation(int32 MontageIdOverride)
{
    if (WeaponData->AttackMontages.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::GetAttackMontage_Impl: No attack montages found."));
        return -1;
    }
    
    if (MontageIdOverride >= 0)
    {
        MontageCycleIndex = MontageIdOverride;
    }
    else
    {
        MontageCycleIndex = (MontageCycleIndex+1) % WeaponData->AttackMontages.Num();
    }

    return MontageCycleIndex;
}

FAttackMontageData AWeaponActor::GetAttackMontage_Implementation(int32 MontageId)
{
    if (MontageId >= WeaponData->AttackMontages.Num() || MontageId < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("WeaponActor::GetAttackMontage: MontageId %d out of range."), MontageId);
        return FAttackMontageData();
    }
    return WeaponData->AttackMontages[MontageId];
}

void AWeaponActor::Equip_Impl_Implementation()
{
    OnWeaponEquipped.Broadcast();
}

void AWeaponActor::Equip_Server_Implementation()
{
    Equip_Multicast();
}

void AWeaponActor::Equip_Multicast_Implementation()
{
    Equip_Impl();
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
