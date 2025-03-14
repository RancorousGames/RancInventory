// Copyright Rancorous Games, 2024

#pragma once

#include "Components/ActorComponent.h"
#include "Components/InventoryComponent.h"
#include "GearDefinition.h"
#include "RISWeaponsDataTypes.h"
#include "Engine/HitResult.h"
#include "GearManagerComponent.generated.h"

class UWeaponDefinition;
class UWeaponStaticData;
class AWeaponActor;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponState, FGameplayTag, Slot, AWeaponActor*, WeaponActor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGearUpdated, FGameplayTag, Slot, FGameplayTag, ItemId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponEquipState, FName, WeaponType, bool , bEquipState);


UENUM(BlueprintType)
enum class EGearSlotType : uint8 {MainHand, OffHand, Armor};

USTRUCT(BlueprintType)
struct FGearSlotDefinition
{
    GENERATED_BODY()

	FGearSlotDefinition() : SlotTag(FGameplayTag::EmptyTag), AttachSocketName(NAME_None), SlotType(EGearSlotType::MainHand), bVisibleOnCharacter(true) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FGameplayTag SlotTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    FName AttachSocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    EGearSlotType SlotType = EGearSlotType::MainHand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	bool bVisibleOnCharacter = true;
	
    // Transient properties to avoid serialization and replication of the component reference
    UPROPERTY(Transient)
    UStaticMeshComponent* MeshComponent = nullptr;
};

UENUM(BlueprintType)
enum class EPendingGearChangeType : uint8
{
	NoChange,
	Equip,
	Unequip,
	WeaponSelect
};

USTRUCT()
struct FGearChangeTransaction
{
	GENERATED_BODY()
	
	EPendingGearChangeType ChangeType;
	EGearChangeStep NextStep;
	FGameplayTag Slot;
	const UItemStaticData* NewItemData;
	const UItemStaticData* OldItemData;
	FTaggedItemBundle PreviousItem;
	bool bRequiresUnequipFirst = false;
};

UENUM(BlueprintType)
enum class EGearChangeStep : uint8
{
	Request,
	PlayAnim,
	Apply,
};


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

	virtual void InitializeComponent() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear")
	TArray<FGearSlotDefinition> GearSlots;

// not using this as i dont want a mirror when i have gearslotdefinition as mutable due to mesh
	//	// Mirror of GearSlots, but with the SlotTag as the key
//	TMap<FGameplayTag, FGearSlotDefinition> GearDefinitionsPerSlot;

	// Shortcuts into the GearDefinitionsPerSlot
	const FGearSlotDefinition* MainHandSlot;
	const FGearSlotDefinition* OffhandSlot;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FWeaponEvent OnEquippedWeaponsChange;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FWeaponState OnWeaponSelected;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FWeaponState OnWeaponHolstered;

	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FGearUpdated OnGearEquipped;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FGearUpdated OnGearUnequipped;
	
	/*
	Called after SpawnWeaponsFromSavedData() is done
	*/
	UPROPERTY(BlueprintAssignable)
	FWeaponEvent OnFinishSpawningWeapons;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitDetected, AActor*, HitActor, FHitResult, HitResult);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FOnHitDetected OnHitDetected;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackPerformed, FMontageData, MontageData);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FOnAttackPerformed OnAttackPerformed;


	// Called when a trace replay has ended i think
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackAnimNotifyEndEvent);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons|Gear")
	FOnAttackAnimNotifyEndEvent OnAttackAnimNotifyEndEvent;
	
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|Configuration")
	bool bMakeHolsteredWeaponInvisible = true;
	
	/*
	If true, we will resize the weapon to it's original world scale after attaching
	*/	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	bool bUseWeaponScaleInsteadOfSocketScale = false;
	
	/* Max number of weapons to be at the ready	*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	int32 MaxSelectableWeaponCount = 1;
	
	/* If true, the actor will automatically rotate smoothly in the given direction of attack	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	bool bRotateToAttackDirection = false;

	/* Rotation speed in degrees per second	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float RotateToAttackDirectionSpeed = 12.0f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	FMontageData DefaultEquipMontage = FMontageData();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	FMontageData DefaultUnequipMontage = FMontageData();
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float DefaultUnequipMontagePlayRate = 1.0f;

	/* The data definition for unarmed attacks */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	UItemStaticData* DefaultUnarmedWeaponData = nullptr;
	
	/*
	 * The notify that must be triggered to complete equipment changes.
	 * If this and Equip/Unequip/WeaponSelectDelay are not set then the equipment change will happen immediately
	 * If both this and  Equip/Unequip/WeaponSelectDelay are set then the notify is used
	 * For a multiplayer pvp game you preferably dont use notify as the server shouldn't play animations and we dont want to trust client
	 * If you want to manually control the timing of the equipment change, then call the equip again with bPlayEquipMontage = false
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	FName GearChangeCommitAnimNotifyName = FName();

	/*
	 * Delay before gear equip actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float EquipDelay = 0.0f;

	/*
	 * Delay before gear unequip actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float UnequipDelay = 0.0f;

	/*
	 * Delay before gear weapon select actually applies (to give a chance for the animation to play partially first)
	 * If this and GearChangeCommitAnimNotifyName are not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float WeaponSelectDelay = 0.0f;

	/* When replacing an item and playing both an unequip and equip montage, this determine how long until the equip starts blending in
	 * This might have unintended consequences when used with anim notify */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	float EquipUnequipAnimBlendDelay = 0.5f;
	
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSlot,VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|State")
	int32 ActiveWeaponIndex = 0;;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|State")
	AWeaponActor* MainhandSlotWeapon = nullptr;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|State")
	AWeaponActor* OffhandSlotWeapon = nullptr;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|Internal")
	ACharacter* Owner = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Internal")
	TArray<const UItemStaticData*> SelectableWeaponsData;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|Internal")
	AWeaponActor* UnarmedWeaponActor = nullptr;

	/* If this is true, the WeaponAttackRecorderComponent will be added to all spawned weapons which will cause attack trace assets to get recorded */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons|Gear|Configuration")
	bool bRecordAttackTraces = false;
	
	// Delayed gear change state. Note that the delayed change is triggered by the client as we dont rely on the server playing animations


	UPROPERTY()
	TArray<FGearChangeTransaction> PendingGearChanges;

	FGearChangeTransaction* ActiveGearChange = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranc Inventory Weapons|Gear|Internal", meta = (AllowPrivateAccess = "true"))
	bool bHasActiveTransaction;

	UPROPERTY()
	FTimerHandle GearChangeCommitHandle;

	UPROPERTY()
	bool bIsInterrupted;

	void ProcessNextGearChange();
	void QueueGearChange(const FGearChangeTransaction& Transaction);
	void HandleInterruption();
	
	/*	Initialized variables. Called from Begin play	*/
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void Initialize();


	/* Returns the weapon currently equipped, if two weapons are equipped, returns the mainhand weapon	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons|Gear")
	AWeaponActor* GetActiveWeapon();
	
	/* Convenience function to get the mainhand weapons data.
	 * Equivalent to WeaponPerSlot[MainHandSlot]*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons|Gear")
	const UWeaponDefinition* GetMainhandWeaponSlotData();

	/* Convenience function to get the Offhand weapons data.
	 * Equivalent to WeaponPerSlot[OffhandSlot] */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons|Gear")
	const UWeaponDefinition* GetOffhandWeaponData();

	
	UFUNCTION()
	void HandleItemAddedToSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, FTaggedItemBundle PreviousItem, EItemChangeReason Reason);

	UFUNCTION()
	void HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, EItemChangeReason Reason);

	/* Try to perform an attack montage
	 * This will succeed if cooldown is ready The weapon is notified of the attack
	 * @Param UseOffhand - If true, we will use the Offhand weapon
	 * @Param MontageIdOverride - If >= 0, we will use this montage id instead of cycling
	 * Multicasts the attack to all clients
	 */
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void TryAttack_Server(FVector AimLocation = FVector(0,0,0), bool ForceOffHand = false, int32 MontageIdOverride = -1);

	UFUNCTION(Reliable, NetMulticast, BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void Attack_Multicast(FVector AimLocation, bool UseOffhand = false, int32 MontageIdOverride = -1);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void SelectNextActiveWeapon(bool bPlayMontage = true);

	UFUNCTION(Server,Reliable)
	void SelectNextActiveWeaponServer(bool bPlayMontage = true);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void SelectPreviousWeapon(bool bPlayMontage = true);

	UFUNCTION(Server,Reliable)
	void SelectPreviousActiveWeaponServer(bool bPlayMontage = true);
	
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void SelectActiveWeapon(int32 WeaponIndex, bool bPlayEquipMontage, AWeaponActor* AlreadySpawnedWeapon = nullptr);

	/*
	If bPlayEquipMontage is false then the swap will happen immediately
	Otherwise the swap time will depend on configuration of GearChangeCommitAnimNotifyName
	*/
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void SelectActiveWeapon_Server(int32 WeaponIndex, FGameplayTag ForcedSlot = FGameplayTag(), AWeaponActor* AlreadySpawnedWeapon = nullptr, EGearChangeStep Step = EGearChangeStep::Request);

	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void SelectUnarmed_Server();

	/*
	 * If PlayEquipMontage is false then the swap will happen immediately
	 * This is called internally by HandleItemAddedToSlot
	 */ 
	void EquipGear(FGameplayTag Slot, const UItemStaticData* NewItemData, FTaggedItemBundle PreviousItem, bool SkipAnim, EGearChangeStep Step);

	FGearSlotDefinition* FindGearSlotDefinition(FGameplayTag SlotTag);

	/*
	* If PlayUnequipMontage is false then the swap will happen immediately
	* This is called internally by HandleItemRemovedFromSlot
	*/
	void UnequipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool SkipAnim = false, EGearChangeStep Step = EGearChangeStep::Request);

	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	void CancelGearChange();

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
	
	/*
	Spawns the weapon on the server, and tries to add it to the array.
	Weapon spawning is handled server side, and the weapon actor is set to replicate so it
	will appear to all clients.
	Note the spawn location is OwnerLocation + FVector(0.0f,0.0f,600.0f); 
	I didn't bother exposing it because ESpawnActorCollisionHandlingMethod::AlwaysSpawn, and  
	the spawned actor is immediately passed to AddWeaponToArray(NewWeaponActor)
	*/
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons|Gear")
	AWeaponActor* SpawnWeapon_IfServer(const UItemStaticData* ItemData, const UWeaponDefinition* WeaponType);

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

	const FGearSlotDefinition* GetHandSlotToUse(const UWeaponDefinition* WeaponData) const;
	AWeaponActor* GetWeaponForSlot(const FGearSlotDefinition* Slot) const;
	
	/*
	Attaches the given weapon to the owner mesh at the desired socket.
	*/
	void AttachWeaponToOwner(AWeaponActor* InputWeaponActor,FName SocketName);
		
	/* Adds the weapon to the list of weapons that can be hotswapped to with */
	void AddAndSetSelectedWeapon(const UItemStaticData* WeaponData, FGameplayTag ForcedSlot = FGameplayTag());
	
	
	virtual void GetLifetimeReplicatedProps(TArray < class FLifetimeProperty > & OutLifetimeProps) const override;

	void PlayRecordedAttackSequence(const UWeaponAttackData* AttackData);
	void StartAttackReplay();
	void StopAttackReplay();

	FMontageData GetEquipMontage(const UGearDefinition* WeaponData) const;
	FMontageData GetUnequipMontage(const UGearDefinition* WeaponData) const;
	
	UInventoryComponent* LinkedInventoryComponent = nullptr;

	// Last attack time
	float LastAttackTime = 0.0f;
	
	bool UseOffhandNext = false;

	// Trace replays
	int ReplayCurrentIndex;
	bool bReplayInitialOwnerPositionSaved;
	const UWeaponAttackData* ReplayedAttackData;
	FTimerHandle ReplayTimerHandle;
	FTransform ReplayInitialOwnerPosition;
	ECollisionChannel TraceChannel;
};


