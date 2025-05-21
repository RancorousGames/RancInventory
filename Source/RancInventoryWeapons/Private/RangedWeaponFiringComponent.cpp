// Copyright Rancorous Games, 2024

#include "RangedWeaponFiringComponent.h"

#include "GameFramework/Character.h"
#include "Runtime/Engine/Classes/GameFramework/Controller.h"
#include "Runtime/Core/Public/Math/UnrealMathUtility.h"
#include "GearManagerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Runtime/Engine/Public/DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "WeaponActor.h"
#include "GearManagerComponent.h"

// Sets default values for this component's properties
URangedWeaponFiringComponent::URangedWeaponFiringComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	AWeaponActor* OwnerAsWeapon = Cast<AWeaponActor>(GetOwner());
	if (OwnerAsWeapon)
	{
		OwnerAsWeapon->OnWeaponEquipped.AddUniqueDynamic(this, &URangedWeaponFiringComponent::Initialize);
		//WeaponComponent needs to re-check for Owner Variables when its Equipped.
	}
}


// Called when the game starts
void URangedWeaponFiringComponent::BeginPlay()
{
	Super::BeginPlay();
	Initialize();
	// ...
}

void URangedWeaponFiringComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(AutoFireHandle);
	GetWorld()->GetTimerManager().ClearTimer(BurstFireHandle);
	Super::EndPlay(EndPlayReason);
}

void URangedWeaponFiringComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URangedWeaponFiringComponent, ClipAmmunition);
	DOREPLIFETIME(URangedWeaponFiringComponent, ReserveAmmunition);
}

// Called every frame
void URangedWeaponFiringComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


void URangedWeaponFiringComponent::Initialize()
{
	OwnerWeapon = GetOwner();
	if (!OwnerWeapon)
	{
		return;
	}

	if (!GetOwner()->GetAttachParentActor()) //Not attached
	{
		OwnerChar = Cast<ACharacter>(GetOwner());
		if (OwnerChar)
		{
			OwnerController = OwnerChar->GetController();
		}
	}
	else //attached to a weapon actor that is attached to some character
	{
		OwnerChar = Cast<ACharacter>(GetOwner()->GetAttachParentActor());
		if (OwnerChar)
		{
			//MovementUtility = OwnerChar->FindComponentByClass<UMovementUtilityComponent>();
			OwnerController = OwnerChar->GetController();
		}
	}

	WorldTimerManager = &(GetWorld()->GetTimerManager());
}

bool URangedWeaponFiringComponent::CanFire(int32 BulletsToFire, bool& bPassTimingCheck, bool& bPassAmmunitionConsumptionCheck)
{
	bPassTimingCheck = false;
	bPassAmmunitionConsumptionCheck = false;

	float CurrentTime = GetWorld()->GetTimeSeconds();


	if (WorldTimeLastFired <= 0.0f || Cooldown <= 0.0f)
	{
		bPassTimingCheck = true;
	}
	else if (CurrentTime - WorldTimeLastFired >= Cooldown)
	{
		bPassTimingCheck = true;
	}


	int32 TotalBullets = BulletsToFire;

	if (bOneAmmoPerShot)
	{
		TotalBullets = 1; //Each multishot counts as one.
	}

	if (TotalBullets <= ClipAmmunition && ClipAmmunition > 0)
	{
		bPassAmmunitionConsumptionCheck = true;
	}
	else if (bUnlimitedClip)
	{
		bPassAmmunitionConsumptionCheck = true;
	}

	if (bPassTimingCheck && bPassAmmunitionConsumptionCheck)
	{
		return true;
	}

	return false;
}

bool URangedWeaponFiringComponent::CooldownReady() const
{
	bool onCooldown = true;

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// Check if the cooldown has passed
	if (WorldTimeLastFired <= 0.0f || Cooldown <= 0.0f || (CurrentTime - WorldTimeLastFired >= Cooldown))
	{
		onCooldown = false;
	}

	return !onCooldown;
}

bool URangedWeaponFiringComponent::CanReload() const
{
	return (!IsReloading() && ReserveAmmunition >= ClipSize);
}


void URangedWeaponFiringComponent::FireLine(const FVector& SpawnLocation, const FRotator& InitialRotation, int32 ProjectilesPerSide, bool bFireMiddleShot) const
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy) //is controlled client,
	{
		FireLineServer(SpawnLocation, InitialRotation, ProjectilesPerSide, bFireMiddleShot);
	}
	else if (OwnerChar && OwnerChar->HasAuthority()) //
	{
		FireLineServer(SpawnLocation, InitialRotation, ProjectilesPerSide, bFireMiddleShot);
	}
}


void URangedWeaponFiringComponent::FireLineServer_Implementation(const FVector& SpawnLocation, const FRotator& InitialRotation, int32 ProjectilesPerSide, bool bFireMiddleShot) const
{
	FVector RightVector = GetRightVector();

	for (int32 i = 1; i <= ProjectilesPerSide; i++)
	{
		FVector SpawnLocationProjectileA = SpawnLocation + RightVector * FiringData.BulletSpacingForNonSpreadPattern * i;
		FVector SpawnLocationProjectileB = SpawnLocation + RightVector * FiringData.BulletSpacingForNonSpreadPattern * -1 * i;

		FRotator InitialRotationProjectileA = FRotator::ZeroRotator;
		FRotator InitialRotationProjectileB = FRotator::ZeroRotator;
		
		if (bUseLineTracesInsteadOfProjectiles)
		{
			FireLineTraceServer(SpawnLocationProjectileA, InitialRotationProjectileA);
			FireLineTraceServer(SpawnLocationProjectileB, InitialRotationProjectileB);
		}
		else
		{
			FireSingleProjectileServer(SpawnLocationProjectileA, InitialRotationProjectileA);
			FireSingleProjectileServer(SpawnLocationProjectileB, InitialRotationProjectileB);
		}
	}
	if (bFireMiddleShot)
	{
		FRotator SingleShotRotation = InitialRotation;

		if (bUseLineTracesInsteadOfProjectiles)
		{
			FireLineTraceServer(SpawnLocation, SingleShotRotation);
		}
		else
		{
			FireSingleProjectileServer(SpawnLocation, SingleShotRotation);
		}
	}
}

void URangedWeaponFiringComponent::FireLineTrace(const FVector StartLocation, const FRotator InitialRotation) const
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy) //is controlled client,
	{
		FireLineTraceServer(StartLocation, InitialRotation);
	}
	else if (OwnerChar && OwnerChar->HasAuthority())
	{
		FireLineTraceServer(StartLocation, InitialRotation);
	}
}


void URangedWeaponFiringComponent::FireLineTraceServer_Implementation(const FVector StartLocation, const FRotator InitialRotation) const
{
	FHitResult OutHitResult = FHitResult();

	FVector EndLocation = StartLocation + InitialRotation.Vector() * LineTraceDistance;

	GetWorld()->LineTraceSingleByChannel(
		OutHitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Camera
	);

	if (bShowDebugLineTrace)
	{
		DrawDebugLine(
			GetWorld(),
			StartLocation,
			EndLocation,
			DebugLineTraceColor,
			bDebugLineTracePersistent,
			1.0f,
			1,
			3.0f
		);
	}

	if (AActor* OutHitActor = OutHitResult.GetActor())
	{
		FVector HitFromDirection = FVector(0.0f, 0.0f, 0.0f);

		FVector OutHitActorForward = OutHitActor->GetActorForwardVector(); //This calculates the HitDirection
		FRotator LookRotation = UKismetMathLibrary::FindLookAtRotation(OutHitActorForward, OutHitResult.ImpactPoint);

		HitFromDirection = LookRotation.Vector();

		UGameplayStatics::ApplyPointDamage(

			OutHitActor,
			LineTraceDamage,
			HitFromDirection,
			OutHitResult,
			OwnerController,
			OwnerWeapon,
			LineTraceDamageTypeClass
		);
	}
}

void URangedWeaponFiringComponent::FirePattern()
{
	if (IsReloading())
	{
		return;
	}

	bool bPassAmmoCheck = false;
	bool bPassTimingCheck = false;
	bool bCanFire = CanFire(FiringData.ProjectilesPerShot, bPassTimingCheck, bPassAmmoCheck);

	if (!bCanFire || !bPassAmmoCheck)
	{
		if (!bPassAmmoCheck && bShowDebugWarnings && bAutomaticReload)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Can't Fire... trying to reload!"))
		}


		if (!bPassAmmoCheck && CanReload() && bAutomaticReload)
		{
			ReloadWeapon(true, true);
		}

		return;
	}


	if (WorldTimerManager && Bursts >= TimesToFirePerBurst) //If we Burst fired enough
	{
		Bursts = 0;
		WorldTimerManager->PauseTimer(BurstFireHandle);
		return;
	}


	FRotator InitialRotation = FRotator::ZeroRotator;

	FVector SpawnLocation = GetComponentLocation();

	bool bFireMiddleShot = false;
	int32 ProjectilesPerSide;

	if (FiringData.ProjectilesPerShot % 2 != 0) //IF ODD
	{
		bFireMiddleShot = true; //Odd number get the middle shot, as FireSingleProjectile()
	}

	if (bFireMiddleShot)
	{
		//ODD case
		ProjectilesPerSide = (FiringData.ProjectilesPerShot - 1) / 2;
	}
	else
	{
		//EVEN
		ProjectilesPerSide = (FiringData.ProjectilesPerShot) / 2;
	}
	WorldTimeLastFired = GetWorld()->GetTimeSeconds();

		FireLine(SpawnLocation, InitialRotation, ProjectilesPerSide, bFireMiddleShot);
	
	if (!bUnlimitedClip)
	{
		if (bOneAmmoPerShot)
		{
			ClipAmmunition = ClipAmmunition - 1;
		}
		else
		{
			ClipAmmunition = ClipAmmunition - FiringData.ProjectilesPerShot;
		}
	}
	if (CanReload() && bAutomaticReload && ClipAmmunition <= 0)
	{
		ReloadWeapon(true, true);
	}

	OnWeaponFire.Broadcast();
}


void URangedWeaponFiringComponent::FireSingleProjectile(const FVector SpawnLocation, const FRotator SpawnRotation) const
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy) //is controlled client,
	{
		FireSingleProjectileServer(SpawnLocation, SpawnRotation);
	}
	else if (OwnerChar && OwnerChar->HasAuthority())
	{
		FireSingleProjectileServer(SpawnLocation, SpawnRotation);
	}
}

void URangedWeaponFiringComponent::FireSingleProjectileServer_Implementation(const FVector SpawnLocation, const FRotator SpawnRotation) const
{
	TSubclassOf<AActor> ProjectileClass = FiringData.ProjectileSoftClass.Get();

	if (ProjectileClass != NULL)
	{
		UWorld* const World = GetWorld();

		if (World != NULL)
		{
			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;

			if (OwnerController)
			{
				ActorSpawnParams.Instigator = OwnerController->GetPawn();
			}

			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

			/*AActor* Projectile = */
			World->SpawnActor<AActor>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Soft pointer Projectile class data not loaded"))
	}
}

void URangedWeaponFiringComponent::FireWeaponComponent()
{
	FirePattern();
}

FRotator URangedWeaponFiringComponent::GetFiringInitialRotation(const ERotationType InputRotationType) const
{
	FRotator InitialRotation = FRotator();

	switch (InputRotationType)
	{
	case ERotationType::WorldSpaceOfComponent:
		{
			//InitialRotation = GetComponentRotation(); //rotation in world space
			break;
		}
	case ERotationType::ControllerRotation:
		{
			if (OwnerController)
			{
				InitialRotation = OwnerController->GetControlRotation();
			}

			break;
		}
	}

	return InitialRotation;
}

bool URangedWeaponFiringComponent::IsFiring() const
{
	bool bIsFiringMontagePlaying = false;
	UAnimMontage* Montage = FiringSoftAnimMontage.Get();
	if (OwnerChar && Montage)
	{
		if (USkeletalMeshComponent* MeshComp = OwnerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				bIsFiringMontagePlaying = AnimInst->Montage_IsPlaying(Montage);
			}
		}
	}

	bool bAutoFiring = false;
	bool bBurstingFiring = false;

	if (WorldTimerManager)
	{
		bAutoFiring = WorldTimerManager->IsTimerActive(AutoFireHandle);
		bBurstingFiring = WorldTimerManager->IsTimerActive(BurstFireHandle);
	}


	return (bIsFiringMontagePlaying || bBurstingFiring || bAutoFiring);
}

bool URangedWeaponFiringComponent::IsReloading() const
{
	bool bIsReloadMontagePlaying = false;
	UAnimMontage* ReloadMontage = ReloadSoftAnimMontage.Get();
	if (OwnerChar && ReloadMontage)
	{
		if (USkeletalMeshComponent* MeshComp = OwnerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				bIsReloadMontagePlaying = AnimInst->Montage_IsPlaying(ReloadMontage);
			}
		}
	}
	return bIsReloadMontagePlaying;
}


void URangedWeaponFiringComponent::PlayFiringMontage(FVector AimLocation)
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy)
	{
		PlayFiringMontageServer(AimLocation);
	}
	else if (OwnerWeapon && OwnerWeapon->HasAuthority())
	{
		PlayFiringMontageServer(AimLocation);
	}
}

void URangedWeaponFiringComponent::PlayFiringMontageMulticast_Implementation(FVector AimLocation)
{
	UAnimMontage* Montage = FiringSoftAnimMontage.Get();
	// URangedWeaponFiringComponent::PlayMontage(OwnerChar, Montage, FiringMontagePlayrate, FName(""), true);

	RotateToAimLocation(AimLocation);
}

void URangedWeaponFiringComponent::PlayFiringMontageServer_Implementation(FVector AimLocation)
{
	bool bIsMontagePlaying = false;
	UAnimMontage* Montage = FiringSoftAnimMontage.Get();
	if (OwnerChar && Montage)
	{
		if (USkeletalMeshComponent* MeshComp = OwnerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				bIsMontagePlaying = AnimInst->Montage_IsPlaying(Montage);
			}
			else if (bShowDebugWarnings)
			{
				UE_LOG(LogTemp, Warning, TEXT("AnimInst is nullptr, "))
			}
		}
		else if (bShowDebugWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("MeshComp is nullptr, "))
		}
	}
	else
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("OwnerChar or Montage is nullptr, Did you load the soft pointers? "))
		}

		return;
	}


	if (!bIsMontagePlaying)
	{
		PlayFiringMontageMulticast(AimLocation);
	}
}

void URangedWeaponFiringComponent::ReloadWeapon(bool bChangeAmmoCount, bool bPlayMontage)
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy)
	{
		ReloadWeaponServer(bChangeAmmoCount, bPlayMontage);
	}
	else if (OwnerWeapon && OwnerWeapon->HasAuthority())
	{
		ReloadWeaponServer(bChangeAmmoCount, bPlayMontage);
	}
}


void URangedWeaponFiringComponent::ReloadWeaponMulticast_Implementation()
{
	UAnimMontage* ReloadMontage = ReloadSoftAnimMontage.Get();
	if (!ReloadMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("ReloadSoftAnimMontage returns a nullptr, remember to load the soft pointers into memory before use! "))
		return;
	}
//	UGearEffectComponent::PlayMontage(OwnerChar, ReloadMontage, ReloadMontagePlayrate, FName(""), true);

	OnWeaponReload.Broadcast();
}

void URangedWeaponFiringComponent::ReloadWeaponServer_Implementation(bool bChangeAmmoCount, bool bPlayMontage)
{
	if (bChangeAmmoCount)
	{
		bool bEnoughAmmoReserve = false;
		if (ReserveAmmunition >= ClipSize && !bUnlimitedClip)
		{
			ClipAmmunition = ClipSize;
			ReserveAmmunition = ReserveAmmunition - ClipSize;
			bEnoughAmmoReserve = true;
		}
		else if (bUnlimitedClip)
		{
			ClipAmmunition = ClipSize;
			bEnoughAmmoReserve = true;
		}


		if (bPlayMontage)
		{
			ReloadWeaponMulticast();
		}
	}
	else
	{
		if (bPlayMontage)
		{
			ReloadWeaponMulticast();
		}
	}
}


void URangedWeaponFiringComponent::RestoreAmmunition(bool bRestoreClip, bool bRestoreReserve)
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy)
	{
		RestoreAmmunitionServer(bRestoreClip, bRestoreReserve);
	}
	else if (OwnerWeapon && OwnerWeapon->HasAuthority())
	{
		RestoreAmmunitionServer(bRestoreClip, bRestoreReserve);
	}
}


void URangedWeaponFiringComponent::RestoreAmmunitionMulticast_Implementation(bool bRestoreClip, bool bRestoreReserve)
{
	OnAmmunitionRestore.Broadcast();
}


void URangedWeaponFiringComponent::RestoreAmmunitionServer_Implementation(bool bRestoreClip, bool bRestoreReserve)
{
	if (bRestoreClip)
	{
		ClipAmmunition = ClipSize;
	}

	if (bRestoreReserve)
	{
		ReserveAmmunition = ReserveSize;
	}

	if (bRestoreClip || bRestoreReserve)
	{
		RestoreAmmunitionMulticast(bRestoreClip, bRestoreReserve);
	}
}

void URangedWeaponFiringComponent::StartFiringWeaponComponent()
{
	if (!WorldTimerManager)
	{
		return;
	}


	if (!bAutomaticFire)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("Weapon is not automatic, just call FireWeaponComponent() instead"))
		}

		FireWeaponComponent();
		return;
	}

	if (!AutoFireDelegate.IsBound())
	{
		AutoFireDelegate.BindUFunction(this, FName("FireWeaponComponent"));
	}

	if (!WorldTimerManager->IsTimerActive(AutoFireHandle))
	{
		FireWeaponComponent(); //Start firing.
		WorldTimerManager->SetTimer(AutoFireHandle, AutoFireDelegate, Cooldown, true);
	}
}

void URangedWeaponFiringComponent::StopFiringWeaponComponent()
{
	if (!WorldTimerManager)
	{
		return;
	}

	WorldTimerManager->ClearTimer(AutoFireHandle);
	UAnimMontage* Montage = FiringSoftAnimMontage.Get();
	float BlendOut = 1.0f;

	if (!Montage)
	{
		return;
	}


	StopMontage(BlendOut, Montage);
}

void URangedWeaponFiringComponent::StopMontage(float BlendOutTime, UAnimMontage* MontageToStop)
{
	if (OwnerChar && !OwnerChar->HasAuthority() && OwnerChar->GetLocalRole() == ROLE_AutonomousProxy)
	{
		StopMontageServer(BlendOutTime, MontageToStop);
	}
	else if (OwnerWeapon && OwnerWeapon->HasAuthority())
	{
		StopMontageServer(BlendOutTime, MontageToStop);
	}
}

void URangedWeaponFiringComponent::RotateToAimLocation(FVector AimLocation)
{
	//if (!AimLocation.IsZero() && MovementUtility)
	//{
	//	float yaw = FRotationMatrix::MakeFromX(AimLocation - OwnerChar->GetActorLocation()).Rotator().Yaw;
	//	MovementUtility->SmoothRotateToYaw(yaw, 5.0f);
	//}
}

void URangedWeaponFiringComponent::StopMontageMulticast_Implementation(float BlendOutTime, UAnimMontage* MontageToStop)
{
	if (OwnerChar && MontageToStop)
	{
		if (USkeletalMeshComponent* MeshComp = OwnerChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
			{
				AnimInst->Montage_Stop(BlendOutTime, MontageToStop);
			}
		}
	}
}


void URangedWeaponFiringComponent::StopMontageServer_Implementation(float BlendOutTime, UAnimMontage* MontageToStop)
{
	StopMontageMulticast(BlendOutTime, MontageToStop);
}
