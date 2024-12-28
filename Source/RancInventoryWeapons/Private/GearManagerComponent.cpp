// Copyright Rancorous Games, 2024


/*
A replicated component that manages the inventory of AWeaponActor's. 
Meant to attached to ACharacter with a skeletal mesh.

Responsible for: 
1. Attaching and detaching weapon to the owner's skeletal mesh at specific sockets
2. Playing equip and holster montages, (optional to change the equipped weapon)
3. Having an active weapon, but have said equip weapon be either equipped or holstered
4.Manage changes in the Owner's UStatManager and the Weapon's UStatManager to respond to equipment changes. Replicated.
When EquipWeapon() and HolsterWeapon() are called with the parameter bImmediatelyChangeActorSocket = true, then the AWeaponActor's 
Equip() and Holster() will be called respectively. This passes down Owner's UStatManager to the weapon, such that the weapon can 
add it's stat component to the owner's OtherStatManagers Array. 
The AWeaponActor also changes bStatDictionaryCanModifyOtherStatDictionary in its UStatManager to respond to whether it is equipped or holstered. 

*/
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RANCINVENTORYWEAPONS_API UGearManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	UGearManagerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear")
	TArray<FGearSlotDefinition> GearSlots;

// not using this as i dont want a mirror when i have gearslotdefinition as mutable due to mesh
	//	// Mirror of GearSlots, but with the SlotTag as the key
//	TMap<FGameplayTag, FGearSlotDefinition> GearDefinitionsPerSlot;

	// Shortcuts into the GearDefinitionsPerSlot
	const FGearSlotDefinition* MainHandSlot;
	const FGearSlotDefinition* OffhandSlot;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear")
	FWeaponEvent OnEquippedWeaponsChange;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear")
	FWeaponState OnWeaponSelected;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear")
	FWeaponState OnWeaponHolstered;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear")
	FGearUpdated OnGearEquipped;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear")
	FGearUpdated OnGearUnequipped;
	
	/*
	Called after SpawnWeaponsFromSavedData() is done
	*/
	UPROPERTY(BlueprintAssignable)
	FWeaponEvent OnFinishSpawningWeapons;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons |Gear| Configuration")
	bool bMakeHolsteredWeaponInvisible = true;
	
	/*
	If true, we will resize the weapon to it's original world scale after attaching
	*/	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	bool bUseWeaponScaleInsteadOfSocketScale = false;
	
	/* Max number of weapons to be at the ready	*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	int32 MaxSelectableWeaponCount = 1;
	
	/* If true, the actor will automatically rotate smoothly in the given direction of attack	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	bool bRotateToAttackDirection = false;

	/* Rotation speed in degrees per second	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float RotateToAttackDirectionSpeed = 12.0f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	FMontageData DefaultEquipMontage = FMontageData();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	UAnimMontage* DefaultUnequipMontage = nullptr;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float DefaultUnequipMontagePlayRate = 1.0f;

	/* The data definition for unarmed attacks */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	UWeaponStaticData* DefaultUnarmedWeaponData = nullptr;
	
	/*
	 * The notify that must be triggered to complete equipment changes.
	 * If this and Equip/Unequip/WeaponSelectDelay are not set then the equipment change will happen immediately
	 * If both this and  Equip/Unequip/WeaponSelectDelay are set then the notify is used
	 * For a multiplayer pvp game you preferably dont use notify as the server shouldn't play animations and we dont want to trust client
	 * If you want to manually control the timing of the equipment change, then call the equip again with bPlayEquipMontage = false
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	FName GearChangeCommitAnimNotifyName = FName();

	/*
	 * Delay before gear equip actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float EquipDelay = 0.0f;

	/*
	 * Delay before gear unequip actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float UnequipDelay = 0.0f;

	/*
	 * Delay before gear weapon select actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float WeaponSelectDelay = 0.0f;

	/* When replacing an item and playing both an unequip and equip montage, this determine how long until the equip starts blending in
	 * This might have unintended consequences when used with anim notify */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float EquipUnequipAnimBlendDelay = 0.5f;
	
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSlot,VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	int32 ActiveWeaponIndex = 0;;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	AWeaponActor* MainhandWeapon = nullptr;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	AWeaponActor* OffhandWeapon = nullptr;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	ACharacter* Owner = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Internal")
	TArray<const UWeaponStaticData*> SelectableWeaponsData;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	AWeaponActor* UnarmedWeaponActor = nullptr;
	
	// Delayed gear change state. Note that the delayed change is triggered by the client as we dont rely on the server playing animations
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	EPendingGearChangeType PendingGearChangeType = EPendingGearChangeType::NoChange;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	FGameplayTag DelayedGearChangeSlot = FGameplayTag::EmptyTag;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	int32 DelayedWeaponSelectionIndex = 0;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	const UItemStaticData* DelayedGearChangeItemData = nullptr;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	FMontageData EquipMontageToBlendInto = FMontageData();
	
	
	/*
	FUNCTIONS. Note Initialize is not in alphabetical order because its important.
	PlayMontage is also out of order because it is the only static function here.
	*/

	/*	Initialized variables. Called from Begin play	*/
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void Initialize();


	/* Returns the weapon currently equipped, if two weapons are equipped, returns the mainhand weapon	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear")
	AWeaponActor* GetActiveWeapon();
	
	/* Convenience function to get the mainhand weapons data.
	 * Equivalent to WeaponPerSlot[MainHandSlot]*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear")
	const UWeaponStaticData* GetMainhandWeaponData();

	/* Convenience function to get the Offhand weapons data.
	 * Equivalent to WeaponPerSlot[OffhandSlot] */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear")
	const UWeaponStaticData* GetOffhandWeaponData();

	
	UFUNCTION()
	void HandleItemAddedToSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity);

	UFUNCTION()
	void HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity);

	/* Try to perform an attack montage
	 * This will succeed if cooldown is ready The weapon is notified of the attack
	 * @Param UseOffhand - If true, we will use the Offhand weapon
	 * @Param MontageIdOverride - If >= 0, we will use this montage id instead of cycling
	 * Multicasts the attack to all clients
	 */
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void TryAttack_Server(FVector AimLocation = FVector(0,0,0), bool ForceOffHand = false, int32 MontageIdOverride = -1);

	UFUNCTION(Reliable, NetMulticast, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void Attack_Multicast(FVector AimLocation, bool UseOffhand = false, int32 MontageIdOverride = -1);

	/*
	Attaches the given weapon to the owner mesh at the desired socket.
	*/
	void AttachWeaponToOwner(AWeaponActor* InputWeaponActor,FName SocketName);
		
	/* Adds the weapon to the list of weapons that can be hotswapped to with
	 * SelectNextActiveWeapon, SelectPreviousWeapon and SelectActiveWeapon	*/
	void AddAndSelectWeapon(const UWeaponStaticData* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void SelectNextActiveWeapon(bool bPlayMontage = true);

	UFUNCTION(Server,Reliable)
	void SelectNextActiveWeaponServer(bool bPlayMontage = true);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void SelectPreviousWeapon(bool bPlayMontage = true);

	UFUNCTION(Server,Reliable)
	void SelectPreviousActiveWeaponServer(bool bPlayMontage = true);

	/*
	If bPlayEquipMontage is false then the swap will happen immediately
	Otherwise the swap time will depend on configuration of GearChangeCommitAnimNotifyName
	*/
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void SelectWeapon_Server(int32 WeaponIndex, bool bPlayEquipMontage = false);

	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void SelectUnarmed_Server();

	/*
	 * If PlayEquipMontage is false then the swap will happen immediately
	 * This is called internally by HandleItemAddedToSlot
	 */ 
	void EquipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayEquipMontage, bool bPlayHolsterMontage = false);

	FGearSlotDefinition* FindGearSlotDefinition(FGameplayTag SlotTag);
	
	/*
	* If PlayUnequipMontage is false then the swap will happen immediately
	* This is called internally by HandleItemRemovedFromSlot
	*/
	void UnequipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayUnequipMontage);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	void CancelGearChange();

	/* Asks the linked inventory to drop the item, converting it to a WorldItem */
	void DropGearFromSlot(FGameplayTag Slot) const;

	
	/*
	returns false if WeaponActor || Owner is a nullptr
	Returns true if pointers are valid
	*/
	bool Check(AWeaponActor* InputWeaponActor) const;

	bool IsWeaponVisible(AWeaponActor* InputWeaponActor);

	UFUNCTION()
	void OnRep_ActiveWeaponSlot();

	UFUNCTION()
	void OnRep_ActiveWeapon();
	
	bool PlayEquipMontage(AWeaponActor* WeaponActor);
	
	bool PlayWeaponHolsterMontage(AWeaponActor* InputWeaponActor);
	void PlayBlendInEquipMontage();
	
	/*
	Spawns the weapon on the server, and tries to add it to the array.
	Weapon spawning is handled server side, and the weapon actor is set to replicate so it
	will appear to all clients.
	Note the spawn location is OwnerLocation + FVector(0.0f,0.0f,600.0f); 
	I didn't bother exposing it because ESpawnActorCollisionHandlingMethod::AlwaysSpawn, and  
	the spawned actor is immediately passed to AddWeaponToArray(NewWeaponActor)
	*/
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear")
	AWeaponActor* SpawnWeapon_IfServer(const UWeaponStaticData* WeaponType);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	/*
	Plays montage safely, and handles nullptr, negative playrate. 
	return OwnerChar->PlayAnimMontage(Montage,PlayRate,StartSectionName);
	returns 0.0f if can't play the montage cause of nullptrs
	*/
	static float PlayMontage( ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate = 1.0f,  FName StartSectionName = FName(""), bool bShowDebugWarnings = false);

	UFUNCTION()
    void OnGearChangeAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);
	
	// Block below is used for rotating towards the attack direction
	float TargetYaw;
	FTimerHandle TimerHandle_RotationUpdate;
	
	FTimerHandle TimerHandle_EquipDelay;
	
	FTimerHandle TimerHandle_UnequipEquipBlendDelay;
	
	
	void RotateToAimLocation(FVector AimLocation);
	void UpdateRotation();
	
	const FGearSlotDefinition* GetHandSlotToUse(const UWeaponStaticData* WeaponData) const;
	const AWeaponActor* GetWeaponForSlot(const FGearSlotDefinition* Slot) const;
	
	bool SetupDelayedGearChange(EPendingGearChangeType InPendingGearChangeType, const FGameplayTag& GearChangeSlot, const UItemStaticData* ItemData, int32 WeaponSelectionIndex = 0);
	UFUNCTION()
	void DelayedGearChangeTriggered();

	
	virtual void GetLifetimeReplicatedProps(TArray < class FLifetimeProperty > & OutLifetimeProps) const override;

	UInventoryComponent* LinkedInventoryComponent = nullptr;

	// Last attack time
	float LastAttackTime = 0.0f;
	
	bool UseOffhandNext = false;
	
};

// Copyright Rancorous Games, 2024


#include "GearManagerComponent.h"
#include "WeaponActor.h"
#include "Engine/EngineTypes.h"
#include "LogRancInventorySystem.h"
#include "Async/IAsyncTask.h"
#include "Core/RISFunctions.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
//#include "../FunctionLibrary/UtilityFunctions.h"

// Sets default values for this component's properties
UGearManagerComponent::UGearManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Get component
	Owner = Cast<ACharacter>(GetOwner());
	if (Owner)
	{
		LinkedInventoryComponent = Owner->GetComponentByClass<UInventoryComponent>();

		if (!LinkedInventoryComponent)
		{
			UE_LOG(LogRancInventorySystem, Warning, TEXT("LinkedInventoryComponent is nullptr."))
			return;
		}
	}
}


// Called when the game starts
void UGearManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// get from actor
	Initialize();
}


void UGearManagerComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGearManagerComponent, ActiveWeaponIndex);
	DOREPLIFETIME(UGearManagerComponent, SelectableWeaponsData);
}

// Called every frame
void UGearManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


void UGearManagerComponent::Initialize()
{
	Owner = Cast<ACharacter>(GetOwner());
	if (!Owner)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("Owner is nullptr."))
		return;
	}

	// If you swap anim instances this might cause issues
	if (UAnimInstance* AnimInstance = Owner->GetMesh()->GetAnimInstance())
	{
		AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UGearManagerComponent::OnGearChangeAnimNotify);
	}

	LinkedInventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UGearManagerComponent::HandleItemAddedToSlot);
	LinkedInventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UGearManagerComponent::HandleItemRemovedFromSlot);

	
	for (FGearSlotDefinition& GearSlot : GearSlots)
	{
		// if mainhand or offhand then save reference MainHandSlot OffhandSlot
		if (GearSlot.SlotType == EGearSlotType::Mainhand)
		{
			MainHandSlot = &GearSlot;
		}
		else if (GearSlot.SlotType == EGearSlotType::Offhand)
		{
			OffhandSlot = &GearSlot;
		}
		
		if (UStaticMeshComponent* NewMeshComponent = NewObject<UStaticMeshComponent>(Owner, UStaticMeshComponent::StaticClass()))
		{
			NewMeshComponent->AttachToComponent(Owner->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, GearSlot.AttachSocketName);
			NewMeshComponent->RegisterComponent();
			
			// Save the mesh component
			GearSlot.MeshComponent = NewMeshComponent;
			GearSlot.MeshComponent->SetVisibility(GearSlot.bVisibleOnCharacter);
		}
	}

	if (Owner->HasAuthority())
	{
		if (DefaultUnarmedWeaponData)
		{
			AddAndSelectWeapon(DefaultUnarmedWeaponData);
			UnarmedWeaponActor = MainhandWeapon;
		}
	}
}

AWeaponActor* UGearManagerComponent::GetActiveWeapon()
{
	if (MainhandWeapon) return MainhandWeapon;

	return OffhandWeapon;
}

const UWeaponStaticData* UGearManagerComponent::GetMainhandWeaponData()
{
	if (!MainhandWeapon) return nullptr;

	return MainhandWeapon->WeaponData;
}

const UWeaponStaticData* UGearManagerComponent::GetOffhandWeaponData()
{
	if (!OffhandWeapon) return nullptr;

	return OffhandWeapon->WeaponData;
}

void UGearManagerComponent::HandleItemAddedToSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity)
{
	if (const UWeaponStaticData* WeaponData = Cast<UWeaponStaticData>(Data))
	{
		
		AddAndSelectWeapon(WeaponData);
	}
	else
	{
		EquipGear(SlotTag, Data, true);
	}
}

void UGearManagerComponent::HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity)
{
	UnequipGear(SlotTag, Data, true);
}

void UGearManagerComponent::TryAttack_Server_Implementation(FVector AimLocation, bool ForceOffHand, int32 MontageIdOverride)
{
	auto* WeaponActor =  MainhandWeapon;
	if (!WeaponActor || !WeaponActor->CanAttack() || ForceOffHand) WeaponActor = OffhandWeapon;
	
	if (!IsValid(WeaponActor))
		return;

	const float WeaponCooldown = WeaponActor->WeaponData->Cooldown;

	if (GetWorld()->GetTimeSeconds() - LastAttackTime > WeaponCooldown)
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();

		if (WeaponActor->CanAttack())
		{
			Attack_Multicast(AimLocation, WeaponActor == OffhandWeapon, MontageIdOverride);
		}
	}
}

void UGearManagerComponent::Attack_Multicast_Implementation(FVector AimLocation, bool UseOffhand, int32 MontageIdOverride)
{
	AWeaponActor* WeaponActor = UseOffhand ? OffhandWeapon : MainhandWeapon;

	if (!WeaponActor)
		return;

	RotateToAimLocation(AimLocation);
	WeaponActor->PerformAttack();
	FMontageData AttackMontage = WeaponActor->GetAttackMontage(MontageIdOverride);

	PlayMontage(Owner, AttackMontage.Montage.Get(), AttackMontage.PlayRate, FName(""));
}

void UGearManagerComponent::AddAndSelectWeapon(const UWeaponStaticData* WeaponData)
{
	if (!IsValid(WeaponData))
	{
		return;
	}

	int32 ExistingEntry = SelectableWeaponsData.Find(WeaponData);
	if (ExistingEntry != INDEX_NONE)
	{
		SelectWeapon_Server(ExistingEntry, true);
	}
	else
	{
		if (SelectableWeaponsData.Num() == MaxSelectableWeaponCount)
		{
			UE_LOG(LogRancInventorySystem, Warning, TEXT("NumberOfWeaponsAcquired >= WeaponSlots, replaced earliest weapon"))

			SelectableWeaponsData.RemoveAt(0);
		}

		SelectableWeaponsData.Add(WeaponData);
		SelectWeapon_Server(SelectableWeaponsData.Num() - 1, true);
	}
}

const FGearSlotDefinition* UGearManagerComponent::GetHandSlotToUse(const UWeaponStaticData* WeaponData) const
{
	switch (WeaponData->HandCompatability)
	{
	case EHandCompatability::OnlyMainhand:
	case EHandCompatability::TwoHanded:
		return MainHandSlot;
	case EHandCompatability::OnlyOffhand:
		// Return offhand unless mainhand is two handed
		return MainhandWeapon && MainhandWeapon->WeaponData->HandCompatability == EHandCompatability::TwoHanded ? nullptr : OffhandSlot;
	case EHandCompatability::AnyHand:
		{
			if (MainhandWeapon && MainhandWeapon->WeaponData->HandCompatability != EHandCompatability::TwoHanded && !OffhandWeapon)
				return OffhandSlot;
			return MainHandSlot;
		}
	}

	return nullptr;
}

const AWeaponActor* UGearManagerComponent::GetWeaponForSlot(const FGearSlotDefinition* Slot) const
{
	if (Slot == MainHandSlot)
	{
		return MainhandWeapon;
	}
	else if (Slot == OffhandSlot)
	{
		return OffhandWeapon;
	}

	return nullptr;
}

void UGearManagerComponent::AttachWeaponToOwner(AWeaponActor* InputWeaponActor, FName SocketName)
{
	if (!Check(InputWeaponActor))
	{
		return;
	}

	FAttachmentTransformRules AttachRules = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
	//false means don't weld the bodies together.

	USkeletalMeshComponent* CharMesh = Owner->GetMesh();

	if (!CharMesh)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("AttachWeaponToOwner() failed, CharMesh is nullptr"))
		return;
	}

	FVector WeaponWorldScale = InputWeaponActor->GetActorScale3D(); //WorldSpace scale

	InputWeaponActor->AttachToComponent(CharMesh, AttachRules, SocketName);
	FTransform WeaponAttachOffset = InputWeaponActor->GetAttachTransform(SocketName);
	InputWeaponActor->SetMobility(EComponentMobility::Movable);
	InputWeaponActor->SetActorRelativeTransform(WeaponAttachOffset);
	InputWeaponActor->SetMobility(EComponentMobility::Stationary);

	if (bUseWeaponScaleInsteadOfSocketScale)
	{
		InputWeaponActor->SetActorScale3D(WeaponWorldScale);
	}
}


bool UGearManagerComponent::Check(AWeaponActor* InputWeaponActor) const
{
	if (!Owner)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("Owner a nullptr. Initialize this component propertly!"));

		return false;
	}


	if (!InputWeaponActor)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("InputWeaponActor is nullptr!"))
		return false;
	}
	return true;
}

void UGearManagerComponent::SelectPreviousWeapon(bool bPlayMontage)
{
	if (!Owner->HasAuthority() && Owner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		SelectPreviousActiveWeaponServer(bPlayMontage);
	}
	else if (Owner->HasAuthority())
	{
		SelectPreviousActiveWeaponServer(bPlayMontage);
	}
}

void UGearManagerComponent::SelectPreviousActiveWeaponServer_Implementation(bool bPlayMontage)
{
	// todo


	/*	int32 NumberOfWeapons = WeaponPerSlot.Num();
	
		if (NumberOfWeapons <= 1) //No weapon to change
		{
			return;
		}
	
		int32 NextWeaponSlot = (ActiveWeaponSlot - 1);
	
		if (NextWeaponSlot < 0)
		{
			NextWeaponSlot = NumberOfWeapons - 1;
		}
		else
		{
			//NextWeaponSlot % NumberOfWeapons; 
		}
	
		SetActiveEquipSlot(NextWeaponSlot, bPlayMontage);*/
}

void UGearManagerComponent::DropGearFromSlot(FGameplayTag Slot) const
{
	if (!LinkedInventoryComponent) return;

	// this will call back into HandleItemRemovedFromSlot
	LinkedInventoryComponent->DropFromTaggedSlot(Slot, 999);
}


void UGearManagerComponent::SelectWeapon_Server_Implementation(int32 WeaponIndex, bool bPlayEquipMontage)
{

	if (SelectableWeaponsData.Num() == 0 || WeaponIndex < 0 || WeaponIndex > SelectableWeaponsData.Num())
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("Slot is < 0 || Slot > WeaponSlot. EquipWeapon() Failed"))
		return;
	}
	
	bool DelayConfigured = false;
	if (bPlayEquipMontage)
	{
		// When a delay is configured, we want to play 
		DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Equip, FGameplayTag::EmptyTag, nullptr, WeaponIndex);
	}

	const UWeaponStaticData* WeaponData = SelectableWeaponsData[WeaponIndex];

	if (!WeaponData)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("WeaponData is nullptr."))
		return;
	}

	AWeaponActor* WeaponActor = nullptr;
	if (!DelayConfigured)
	{
		WeaponActor = SpawnWeapon_IfServer(WeaponData);

		if (!Check(WeaponActor) || !Owner)
		{
			return;
		}
	}

	const FGearSlotDefinition* HandSlot = GetHandSlotToUse(WeaponData);
	
	if (const AWeaponActor* WeaponToReplace = GetWeaponForSlot(HandSlot))
	{
		if (bPlayEquipMontage)
		{
			EquipMontageToBlendInto = WeaponData->EquipMontage;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_UnequipEquipBlendDelay, this, &UGearManagerComponent::PlayBlendInEquipMontage, EquipUnequipAnimBlendDelay, false);
		}
		
		UnequipGear(HandSlot->SlotTag, WeaponToReplace->WeaponData, bPlayEquipMontage);
	}

	if (!bPlayEquipMontage && !DelayConfigured)
	{
		if (WeaponActor->IsAttachedTo(Owner))
		{
			DropGearFromSlot(MainHandSlot->SlotTag); //Need to detach before can change socket
		}

		AttachWeaponToOwner(WeaponActor, HandSlot->AttachSocketName);

		if (HandSlot == MainHandSlot)
		{
			MainhandWeapon = WeaponActor;
		}
		else if (HandSlot == OffhandSlot)
		{
			OffhandWeapon = WeaponActor;
		}
		ActiveWeaponIndex = WeaponIndex;
		WeaponActor->Equip();
	}

	if (!DelayConfigured)
		OnWeaponSelected.Broadcast(WeaponActor);
}

void UGearManagerComponent::SelectUnarmed_Server_Implementation()
{
	if (UnarmedWeaponActor)
	{
		OffhandWeapon = nullptr;
		ActiveWeaponIndex = 0;
		MainhandWeapon = UnarmedWeaponActor;
		UnarmedWeaponActor->Equip();
		
		OnWeaponSelected.Broadcast(UnarmedWeaponActor);
	}
}

void UGearManagerComponent::EquipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayEquipMontage, bool PlayUnequipMontage)
{
	bool DelayConfigured = false;
    if (PlayEquipMontage)
    {
    	DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Equip, Slot, ItemData);
    }
	
	// check if its weapon item data
	if (const UWeaponStaticData* WeaponData = Cast<UWeaponStaticData>(ItemData))
	{
		if (!DelayConfigured)
			AddAndSelectWeapon(WeaponData);
	}
	else
	{
		if (DelayConfigured)
		{
			FGearSlotDefinition* GearSlot = FindGearSlotDefinition(Slot);
			if (!GearSlot) return;

			if (GearSlot->MeshComponent->GetStaticMesh() != nullptr)
			{
				UnequipGear(Slot, nullptr, PlayUnequipMontage);
				if (PlayUnequipMontage)
				{
					EquipMontageToBlendInto = DefaultEquipMontage;
					GetWorld()->GetTimerManager().SetTimer(TimerHandle_UnequipEquipBlendDelay, this, &UGearManagerComponent::PlayBlendInEquipMontage, EquipUnequipAnimBlendDelay, false);
				}
			}

			GearSlot->MeshComponent->SetStaticMesh(ItemData->ItemWorldMesh);
			GearSlot->MeshComponent->SetWorldScale3D(ItemData->ItemWorldScale);
		}

		if (!EquipMontageToBlendInto.IsValid())
			PlayMontage(Owner, DefaultEquipMontage.Montage.Get(), DefaultEquipMontage.PlayRate, FName(""));
	}
}

void UGearManagerComponent::PlayBlendInEquipMontage()
{
	if (EquipMontageToBlendInto.IsValid())
	{
		PlayMontage(Owner, EquipMontageToBlendInto.Montage.Get(), EquipMontageToBlendInto.PlayRate, FName(""));
		EquipMontageToBlendInto.Montage = nullptr;
	}
}

bool UGearManagerComponent::SetupDelayedGearChange(EPendingGearChangeType InPendingGearChangeType, const FGameplayTag& GearChangeSlot, const UItemStaticData* ItemData, int32 WeaponSelectionIndex)
{
	bool DelayUntilNotify = GearChangeCommitAnimNotifyName != NAME_None;
	bool NeedsEquipDelay = PendingGearChangeType == EPendingGearChangeType::Equip && EquipDelay > 0;
	bool NeedsUnequipDelay = PendingGearChangeType == EPendingGearChangeType::Unequip && UnequipDelay > 0;
	bool NeedsWeaponSelectDelay = PendingGearChangeType == EPendingGearChangeType::WeaponSelect && WeaponSelectDelay > 0;
	
	PendingGearChangeType = InPendingGearChangeType;
	if (DelayUntilNotify || NeedsEquipDelay || NeedsUnequipDelay || NeedsWeaponSelectDelay)
	{
		DelayedWeaponSelectionIndex = WeaponSelectionIndex;
		DelayedGearChangeSlot = GearChangeSlot;
		DelayedGearChangeItemData = ItemData;

		if (!DelayUntilNotify)
		{
			if (TimerHandle_EquipDelay.IsValid())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_EquipDelay);
			}

			float GearChangeDelay = NeedsEquipDelay ? EquipDelay : NeedsUnequipDelay ? UnequipDelay : WeaponSelectDelay;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_EquipDelay, this, &UGearManagerComponent::DelayedGearChangeTriggered, GearChangeDelay, false);
		}

		return true;
	}
	
	return false;
}

void UGearManagerComponent::DelayedGearChangeTriggered()
{
	switch (PendingGearChangeType)
	{
	case EPendingGearChangeType::Equip:
		EquipGear(DelayedGearChangeSlot, DelayedGearChangeItemData, false);
		break;
	case EPendingGearChangeType::Unequip:
		UnequipGear(DelayedGearChangeSlot, DelayedGearChangeItemData, false);
		break;
	case EPendingGearChangeType::WeaponSelect:
		SelectWeapon_Server(DelayedWeaponSelectionIndex, false);
		break;
	default: ;
	}

	DelayedGearChangeSlot = FGameplayTag();
	DelayedGearChangeItemData = nullptr;
	DelayedWeaponSelectionIndex = -1;
}

void UGearManagerComponent::OnGearChangeAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& _)
{
	if (NotifyName.Compare(GearChangeCommitAnimNotifyName) == 0)
	{
		DelayedGearChangeTriggered();
	}
}


void UGearManagerComponent::CancelGearChange()
{
	DelayedGearChangeSlot = FGameplayTag();
	DelayedGearChangeItemData = nullptr;
	DelayedWeaponSelectionIndex = -1;
	
}

FGearSlotDefinition* UGearManagerComponent::FindGearSlotDefinition(FGameplayTag SlotTag)
{
	for (FGearSlotDefinition& GearSlot : GearSlots)
	{
		if (GearSlot.SlotTag == SlotTag)
		{
			return &GearSlot;
		}
	}

	return nullptr;
}

void UGearManagerComponent::UnequipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayUnequipMontage)
{
	bool DelayConfigured = false;
	if (PlayUnequipMontage)
	{
		DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Unequip, Slot, ItemData);
	}
	
	if (ItemData && Slot == MainHandSlot->SlotTag || Slot == OffhandSlot->SlotTag)
	{
		AWeaponActor* WeaponToUnequip = MainhandWeapon; // assume mainhand
		
		// cast to weapon data
		const UWeaponStaticData* WeaponData = Cast<UWeaponStaticData>(ItemData);

		if (!WeaponData) return;
		
		if (Slot == MainHandSlot->SlotTag && WeaponData->HandCompatability == EHandCompatability::OnlyOffhand && OffhandWeapon)
		{
			WeaponToUnequip = OffhandWeapon;
			OffhandWeapon = nullptr;
		}
		else if (Slot == OffhandSlot->SlotTag && OffhandWeapon)
		{
			WeaponToUnequip = OffhandWeapon;
			OffhandWeapon = nullptr;
		}
		else
		{
			MainhandWeapon = nullptr;
		}

		if (PlayUnequipMontage)
		{
			PlayWeaponHolsterMontage(WeaponToUnequip);
		}
		if (!DelayConfigured)
		{
			WeaponToUnequip->Holster();
			OnWeaponHolstered.Broadcast(WeaponToUnequip);
			if (!DefaultUnarmedWeaponData || WeaponToUnequip->WeaponData != DefaultUnarmedWeaponData)
				WeaponToUnequip->Destroy();
			OnEquippedWeaponsChange.Broadcast();

			if (!MainhandWeapon && !OffhandWeapon && UnarmedWeaponActor)
			{
				SelectUnarmed_Server();
			}
		}
	}
	else if (!DelayConfigured)
	{
		FGearSlotDefinition* GearSlot = FindGearSlotDefinition(Slot);

		if (GearSlot->MeshComponent && GearSlot->MeshComponent->GetStaticMesh() != nullptr)
		{
			GearSlot->MeshComponent->SetStaticMesh(nullptr);

			PlayMontage(Owner, DefaultUnequipMontage, 1.0f, FName(""));
		}
	}

	if (!DelayConfigured)
		OnGearUnequipped.Broadcast(Slot, ItemData->ItemId);
}

void UGearManagerComponent::SelectNextActiveWeapon(bool bPlayMontage)
{
	if (!Owner->HasAuthority() && Owner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		SelectNextActiveWeaponServer(bPlayMontage);
	}
	else if (Owner->HasAuthority())
	{
		SelectNextActiveWeaponServer(bPlayMontage);
	}
}

void UGearManagerComponent::SelectNextActiveWeaponServer_Implementation(bool bPlayMontage)
{
	if (SelectableWeaponsData.Num() == 0)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("No weapons to select!"))
		return;
	}

	const int32 NextWeaponIndex = (ActiveWeaponIndex + 1) % SelectableWeaponsData.Num();
	
	SelectWeapon_Server(NextWeaponIndex, bPlayMontage);
}

bool UGearManagerComponent::IsWeaponVisible(AWeaponActor* InputWeaponActor)
{
	if (!InputWeaponActor)
	{
		return false;
	}
	return InputWeaponActor->GetRootComponent()->IsVisible();
}


void UGearManagerComponent::OnRep_ActiveWeapon()
{
	OnEquippedWeaponsChange.Broadcast();
}

void UGearManagerComponent::OnRep_ActiveWeaponSlot()
{
	OnEquippedWeaponsChange.Broadcast();
}

bool UGearManagerComponent::PlayEquipMontage(AWeaponActor* WeaponActor)
{
	if (!Check(WeaponActor) || !WeaponActor->WeaponData)
	{
		UE_LOG(LogRancInventorySystem, Warning, TEXT("WeaponActor or WeaponData is nullptr."))
		return false;
	}

	UAnimMontage* EquipMontage = WeaponActor->WeaponData->EquipMontage.Montage.Get();
	const float PlayLength = PlayMontage(Owner, EquipMontage, WeaponActor->WeaponData->EquipMontage.PlayRate, FName(""));

	if (PlayLength == 0.0f)
	{
		return false;
	}

	return true;
}

bool UGearManagerComponent::PlayWeaponHolsterMontage(AWeaponActor* InputWeaponActor)
{
	if (!Check(InputWeaponActor))
	{
		return false;
	}

	float PlayLength = PlayMontage(Owner, InputWeaponActor->WeaponData->HolsterMontage.Montage.Get(), InputWeaponActor->WeaponData->HolsterMontage.PlayRate, FName(""));

	if (PlayLength == 0.0f)
	{
		return false;
	}

	return true;
}

AWeaponActor* UGearManagerComponent::SpawnWeapon_IfServer(const UWeaponStaticData* WeaponData)
{
	if (!Owner || !Owner->HasAuthority())
	{
		return nullptr;
	}

	FVector SpawnLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, 600.0f);

	FActorSpawnParameters ActorSpawnParams;
	ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto WeaponClass = WeaponData->WeaponActorClass;
	if (!IsValid(WeaponClass))
	{
		WeaponClass = AWeaponActor::StaticClass();
	}
	
	if (AWeaponActor* NewWeaponActor = Cast<AWeaponActor>( GetWorld()->SpawnActorDeferred<AWeaponActor>(WeaponClass, FTransform(FRotator(0.0f, 0.0f, 0.0f), SpawnLocation, FVector::OneVector), GetOwner())))
		{
			NewWeaponActor->WeaponData = WeaponData; // Set additional properties here

			NewWeaponActor->FinishSpawning(FTransform(FRotator(0.0f, 0.0f, 0.0f), SpawnLocation, FVector::OneVector));
			return NewWeaponActor;
		}

	UE_LOG(LogRancInventorySystem, Warning, TEXT("Failed to spawn weapon actor!"))
	return nullptr;
}

float UGearManagerComponent::PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName, bool bShowDebugWarnings)
{
	if (!OwnerChar)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRancInventorySystem, Warning, TEXT("OwnerChar is a nullptr, PlayMontage() returning 0.0f "))
		}

		return 0.0f;
	}

	if (!Montage)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRancInventorySystem, Warning, TEXT("Montage is a nullptr, PlayMontage() returning 0.0f "))
		}


		return 0.0f;
	}

	if (PlayRate < 0.0f)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRancInventorySystem, Warning, TEXT("Playrate was < 0, setting Playrate = 1!"))
		}
		PlayRate = 1.0f;
	}

	return OwnerChar->PlayAnimMontage(Montage, PlayRate, StartSectionName);
}


void UGearManagerComponent::RotateToAimLocation(FVector AimLocation)
{
	if (bRotateToAttackDirection && !AimLocation.IsZero())
	{
		TargetYaw = FRotationMatrix::MakeFromX(AimLocation - Owner->GetActorLocation()).Rotator().Yaw;

		// Start rotation immediately on the next tick
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_RotationUpdate, this, &UGearManagerComponent::UpdateRotation, GetWorld()->GetDeltaSeconds(), true);
	}
}

void UGearManagerComponent::UpdateRotation()
{
	if (!Owner)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RotationUpdate);
		return;
	}

	// Get the delta time from the world
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	// Get current rotation as a quaternion
	const FQuat CurrentQuat = Owner->GetActorRotation().Quaternion();
	// Create target rotation as a quaternion
	FQuat TargetQuat = FQuat(FRotator(0.f, TargetYaw, 0.f));

	// Perform spherical interpolation between the current and target rotations
	FQuat NewQuat = FQuat::Slerp(CurrentQuat, TargetQuat, RotateToAttackDirectionSpeed * DeltaTime);

	// Ensure the quaternion is normalized (SLERP can sometimes return a non-normalized quaternion)
	NewQuat.Normalize();

	// Set the new rotation based on the interpolated quaternion
	Owner->SetActorRotation(NewQuat);

	// Check if the target yaw has been reached within a small error margin
	if (NewQuat.Equals(TargetQuat, 0.01f))
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RotationUpdate);
	}
}

void UGearManagerComponent::PlayRecordedAttackSequence(const UWeaponAttackData* AttackData)
{
    if (!IsValid(AttackData) || AttackData->AttackSequence.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid attack data or empty attack sequence."));
        return;
    }

    // Initialize replay session
    ReplayCurrentIndex = 0;
    bReplayInitialOwnerPositionSaved = false;
    ReplayedAttackData = AttackData;
    
    // Start the trace replay
    StartAttackReplay();
}

void UGearManagerComponent::StartAttackReplay()
{
    if (!IsValid(ReplayedAttackData) || ReplayedAttackData->AttackSequence.Num() == 0)
    {
        StopAttackReplay();
        return;
    }

    // Save initial position if not already done
    if (!bReplayInitialOwnerPositionSaved)
    {
        ReplayInitialOwnerPosition = GetOwner()->GetActorTransform();
        bReplayInitialOwnerPositionSaved = true;
    }

    // Stop if we reached the end of the attack sequence
    if (ReplayCurrentIndex >= ReplayedAttackData->AttackSequence.Num() - 1)
    {
        StopAttackReplay();
        return;
    }

    // Get current and next timestamps in the sequence
    const FWeaponAttackTimestamp& CurrentTimestamp = ReplayedAttackData->AttackSequence[ReplayCurrentIndex];
    const FWeaponAttackTimestamp& NextTimestamp = ReplayedAttackData->AttackSequence[ReplayCurrentIndex + 1];
    
    // Calculate time delta between timestamps for proper replay timing
    float TimeDelta = NextTimestamp.Timestamp - CurrentTimestamp.Timestamp;

    // Perform line traces between the recorded socket positions
    for (int32 SocketIndex = 0; SocketIndex < CurrentTimestamp.SocketPositions.Num(); ++SocketIndex)
    {
        FVector StartPosition = ReplayInitialOwnerPosition.TransformPosition(CurrentTimestamp.SocketPositions[SocketIndex]);
        FVector EndPosition = ReplayInitialOwnerPosition.TransformPosition(NextTimestamp.SocketPositions[SocketIndex]);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartPosition, EndPosition, TraceChannel, QueryParams);

        if (bHit)
        {
            AActor* HitActor = HitResult.GetActor();
            if (HitActor)
            {
                UE_LOG(LogTemp, Log, TEXT("Hit detected on actor: %s"), *HitActor->GetName());

                // Broadcast an event when a hit is detected
                OnHitDetected.Broadcast(HitActor, HitResult);
            }
        }

        // Optional: draw debug lines for each socket
        #if WITH_EDITOR
        DrawDebugLine(GetWorld(), StartPosition, EndPosition, FColor::Red, false, TimeDelta, 0, 2.0f);
        #endif
    }

    // Schedule the next trace step
    ReplayCurrentIndex++;
    GetWorld()->GetTimerManager().SetTimer(ReplayTimerHandle, this, &UGearManagerComponent::StartAttackReplay, TimeDelta, false);
}

void UGearManagerComponent::StopAttackReplay()
{
    GetWorld()->GetTimerManager().ClearTimer(ReplayTimerHandle);
    ReplayedAttackData = nullptr;
    bReplayInitialOwnerPositionSaved = false;
    UE_LOG(LogTemp, Log, TEXT("Attack replay stopped."));
}