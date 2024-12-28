
#include "Components/ActorComponent.h"
#include "WeaponDataStructures.h"
#include "WeaponStaticData.h"
#include "Components/InventoryComponent.h"
#include "GearManagerComponent.generated.h"

class UWeaponStaticData;
class AWeaponActor;
class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponState, AWeaponActor*, WeaponActor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGearUpdated, FGameplayTag, Slot, FGameplayTag, ItemId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponEquipState, FName, WeaponType, bool , bEquipState);


USTRUCT(BlueprintType)
struct FGearSlotDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory")
	FGameplayTag SlotTag;

	// The name of the socket OR bone on the parent mesh where we want to attach the item
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory")
	EGearSlotType SlotType;
};


UENUM(BlueprintType)
enum class EGearSlotType : uint8 {MainHand, OffHand, Armor};

// enum
UENUM(BlueprintType)
enum class EPendingGearChangeType : uint8
{
	NoChange,
	Equip,
	Unequip,
	WeaponSelect
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

// not using this as i dont want a mirror when i have gearslotdefinition as mutable due to mesh
	//	// Mirror of GearSlots, but with the SlotTag as the key
//	TMap<FGameplayTag, FGearSlotDefinition> GearDefinitionsPerSlot;

	// Shortcuts into the GearDefinitionsPerSlot
	const FGearSlotDefinition* MainHandSlot;
	const FGearSlotDefinition* OffHandSlot;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRISWSimpleEvent);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FRISWSimpleEvent OnEquippedWeaponChange;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FRISWSimpleEvent OnSelectableWeaponsChanged;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponChangeEvent, AWeaponActor*, WeaponActor);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FWeaponChangeEvent OnWeaponSelected;
	
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FWeaponChangeEvent OnWeaponHolstered;

	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGearOrWeaponUpdated, FGameplayTag, Slot, FGameplayTag, ItemId, AWeaponActor*, WeaponActor);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FGearOrWeaponUpdated OnGearEquipped;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGearUpdated, FGameplayTag, Slot, FGameplayTag, ItemId);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FGearUpdated OnGearUnequipped;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponAttackEvent, FMontageData, MontageData);
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear ")
	FWeaponAttackEvent OnAttackPerformed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitDetected, AActor*, HitActor, const FHitResult&, HitResult);
	// Delegate to broadcast when a hit is detected during the attack replay
	UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory Weapons | Gear | Attack")
	FOnHitDetected OnHitDetected;

	/*
	Called after SpawnWeaponsFromSavedData() is done
	*/
	UPROPERTY(BlueprintAssignable)
	FRISWSimpleEvent OnFinishSpawningWeapons;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	bool bAnimateGearChanges = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	TArray<FGearSlotDefinition> GearSlots;

	/* The tag that are a category of all two handed weapons, if any */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	FGameplayTag TwoHandedCategory;
	
	/*
	If true, we will resize the weapon to it's original world scale after attaching
	*/	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	bool bUseWeaponScaleInsteadOfSocketScale = true;
	
	/* Max number of weapons to be at the ready	*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	int32 MaxSelectableWeaponCount = 9;
	
	/* If true, the actor will automatically rotate smoothly in the given direction of attack	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	bool bRotateToAttackDirection = true;

	/* Rotation speed in degrees per second	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float RotateToAttackDirectionSpeed = 12.0f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	FMontageData DefaultEquipMontage = FMontageData();

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	FMontageData DefaultUnequipMontage = FMontageData();
	
	/* The data definition for unarmed attacks */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	UWeaponStaticData* DefaultUnarmedWeaponData = nullptr;
	
	/*
	 * Delay before gear equip actually applies (to give a chance for the animation to play partially first)
	 * If this is not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float EquipDelay = 0.3f;

	/*
	 * Delay before gear unequip actually applies (to give a chance for the animation to play partially first)
	 * If this is not set then the equipment change will happen immediately
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float UnequipDelay = 0.3f;

	/*
	 * Delay before gear weapon select actually applies (to give a chance for the animation to play partially first)
	 * If this is not set then the equipment change will happen immediately
	 */
	//UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	//float WeaponSelectDelay = 0.3f;

	/* When replacing an item and playing both an unequip and equip montage, this determine how long until the equip starts blending in
	 * This might have unintended consequences when used with anim notify */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Configuration")
	float EquipUnequipAnimBlendDelay = 0.5f;
	
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSlot,VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	int32 ActiveWeaponIndex = 0;

	// Note: Not replicated so clients need to use SelectableWeaponsCount
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Ranc Inventory Weapons | Gear | Internal")
	TArray<const UWeaponStaticData*> SelectableWeaponsData;

	UPROPERTY(Replicated, VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	int32 SelectableWeaponsCount = 0;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	AWeaponActor* MainhandSlotWeapon = nullptr;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | State")
	AWeaponActor* OffhandSlotWeapon = nullptr;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	bool bMainhandSlotIsInOffhand = false;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	ACharacter* Owner = nullptr;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	AWeaponActor* UnarmedWeaponActor = nullptr;
	
	// Delayed gear change state. Note that the delayed change is triggered by the client as we dont rely on the server playing animations

	TArray<FDelayedGearChange> DelayedGearChangeQueue = TArray<FDelayedGearChange>(); // Using array as queue, last element is the next to be processed
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Ranc Inventory Weapons | Gear | Internal")
	double TimeWhenReadyForNextMontage = 0;


	/*
	FUNCTIONS. Note Initialize is not in alphabetical order because its important.
	PlayMontage is also out of order because it is the only static function here.
	*/

	/*	Initialized variables. Called from Begin play	*/
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void Initialize();


	/* Returns the weapon currently equipped, if two weapons are equipped, returns the mainhand weapon	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear ")
	AWeaponActor* GetActiveWeapon();
	
	/* Convenience function to get the mainhand weapons data.
	 * Equivalent to WeaponPerSlot[MainHandSlot]*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear ")
	const UWeaponStaticData* GetMainhandWeaponSlotData();

	/* Convenience function to get the Offhand weapons data.
	 * Equivalent to WeaponPerSlot[OffhandSlot] */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear ")
	const UWeaponStaticData* GetOffhandSlotWeaponData();

	/* Returns if we have no weapon or have the "unarmed" weapon equipped */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ranc Inventory Weapons | Gear ")
	bool IsUnarmed() const;
	
	UFUNCTION()
	void HandleItemAddedToSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity);

	UFUNCTION()
	void HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, EItemChangeReason Reason);

	/* Try to perform an attack montage
	 * This will succeed if cooldown is ready The weapon is notified of the attack
	 * @Param UseOffhand - If true, we will use the Offhand weapon
	 * @Param MontageIdOverride - If >= 0, we will use this montage id instead of cycling
	 * Multicasts the attack to all clients
	 */
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void TryAttack_Server(FVector AimLocation = FVector(0,0,0), bool ForceOffHand = false, int32 MontageIdOverride = -1);

	UFUNCTION(Reliable, NetMulticast, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void Attack_Multicast(FVector AimLocation, bool UseOffhand = false, int32 MontageIdOverride = -1);

	/*
	Attaches the given weapon to the owner mesh at the desired socket.
	*/
	void AttachWeaponToOwner(AWeaponActor* InputWeaponActor,FName SocketName);
		
	/* Adds the weapon to the SelectableWeaponsData array that can be hotswapped to with
	 * SelectNextActiveWeapon, SelectPreviousWeapon and SelectActiveWeapon*/
	void EquipWeapon_IfServer(const FGameplayTag& Slot, const UWeaponStaticData* WeaponData, bool bPlayEquipMontage, AWeaponActor* AlreadySpawnedWeapon = nullptr);

	
	
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void SelectNextActiveWeapon();
	
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void SelectPreviousWeapon();
	
	/*
	If bPlayEquipMontage is false then the swap will happen immediately
	*/
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void SelectActiveWeapon_Server(int32 WeaponIndex, AWeaponActor* AlreadySpawnedWeapon = nullptr);

	/* Remove a weapon from SelectableWeaponsData so that it can no longer be selected */
	UFUNCTION(Reliable, Server, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	void RemoveWeaponFromSelectableWeapons_Server(const UWeaponStaticData* WeaponData);
	
	// Initiates the process of replaying an attack trace sequence recorded by a weapon
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Attack")
	void PlayAttackTraceSequence(const UWeaponAttackData* AttackData, ECollisionChannel TraceChannel, FName TraceProfile);

	// Helper function for executing the trace at each recorded timestamp
	void ExecuteAttackTrace(const FWeaponAttackTimestamp& AttackTimestamp, ECollisionChannel TraceChannel, FName TraceProfile);
	
	// not needed as selectweapon just tells inventory to change item and that syncs everything
//UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
//void SelectWeapon_Multicast(int32 WeaponIndex);
	
	/*
	 * If PlayEquipMontage is false then the swap will happen immediately
	 * This is called internally by HandleItemAddedToSlot
	 */ 
	void EquipGear_IfServer(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayEquipMontage, bool bPlayHolsterMontage = false, AWeaponActor* AlreadySpawnedWeapon = nullptr);

	const FGearSlotDefinition* FindGearSlotDefinition(FGameplayTag SlotTag) const;
	
	UFUNCTION()
	void OnRep_ActiveWeaponSlot();

	UFUNCTION()
	void OnRep_ActiveWeapon();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	/*
	Plays montage safely, and handles nullptr, negative playrate. 
	return OwnerChar->PlayAnimMontage(Montage,PlayRate,StartSectionName);
	returns 0.0f if can't play the montage cause of nullptrs
	*/
	float PlayMontage( ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate = 1.0f,  FName StartSectionName = FName(""), bool bShowDebugWarnings = false);
	
	bool IsWeaponVisible(AWeaponActor* InputWeaponActor);
	bool PlayWeaponEquipMontage(AWeaponActor* WeaponActor);
	bool PlayWeaponHolsterMontage(AWeaponActor* InputWeaponActor);
	
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory Weapons | Gear ")
	AWeaponActor* SpawnWeaponLocal(const UWeaponStaticData* WeaponType);
	
	/*
	* If PlayUnequipMontage is false then the swap will happen immediately
	* This is called internally by HandleItemRemovedFromSlot
	*/
	void UnequipGear_IfServer(FGameplayTag Slot, const UItemStaticData* ItemData, bool PlayUnequipMontage, bool bForceQueue);
	
	void UnequipWeapon_IfServer(FGameplayTag Slot, bool JustPlayUnequipAnim);
	void ForceUnequipFromEquip_IfServer(FGameplayTag Slot, const UItemStaticData* ItemData, bool JustPlayUnequipAnim);
	void UnequipWeapon_ServerImpl(AWeaponActor* WeaponToUnequip, bool JustPlayUnequipAnim);

	void SortWeaponOrder();

	/* Asks the linked inventory to drop the item, converting it to a WorldItem */
	void DropGearFromSlot_IfServer(FGameplayTag Slot) const;
	
	
	// Block below is used for rotating towards the attack direction
	float TargetYaw;
	FTimerHandle TimerHandle_RotationUpdate;
	
	FTimerHandle TimerHandle_DelayedGearChange;
	
	FTimerHandle TimerHandle_UnequipEquipBlendDelay;
	
	const FGearSlotDefinition* GetHandSocketToUse(const UWeaponStaticData* WeaponData, FGameplayTag RequestedSlot = FGameplayTag::EmptyTag) const;
	const AWeaponActor* GetWeaponForSlot(const FGearSlotDefinition* Slot, bool IncludeTwoHanded) const;
	
	EDelayReason SetupDelayedGearChange(EPendingGearChangeType InPendingGearChangeType, const FGameplayTag& GearChangeSlot, const UItemStaticData* ItemData, bool bForceQueue = false, AWeaponActor* SpawnedWeapon = nullptr);

	UFUNCTION()
	void DelayedGearChangeTriggered();
	
	
	void RotateToAimLocation(FVector AimLocation);
	void UpdateRotation();

	
	virtual void GetLifetimeReplicatedProps(TArray < class FLifetimeProperty > & OutLifetimeProps) const override;

	UInventoryComponent* LinkedInventoryComponent = nullptr;

	// Last attack time
	float LastAttackTime = 0.0f;
};

