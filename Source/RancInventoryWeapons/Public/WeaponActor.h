// Copyright Rancorous Games, 2024

#pragma once

#include "CoreMinimal.h"
#include "WeaponDefinition.h"
#include "Templates/SharedPointer.h"
#include "Actors/WorldItem.h"
#include "Engine/StreamableManager.h"
#include "Templates/SharedPointer.h"
#include "WeaponActor.generated.h"

class UWeaponFiringComponent;
class UNetConnection;
class UMeleeMontageComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponStateChange);

/**
 * Base class for a replicated weapon, meant to be attached to character skeletal mesh.
 * When spawning, set the ItemId and PlacedInWorld properties.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom))
class RANCINVENTORYWEAPONS_API AWeaponActor : public AStaticMeshActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray < class FLifetimeProperty > & OutLifetimeProps) const override;

	virtual UNetConnection * GetNetConnection() const override;

public:

	UPROPERTY(BlueprintAssignable)
	FWeaponStateChange OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable)
	FWeaponStateChange OnWeaponHolstered;

	AWeaponActor(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	const UWeaponDefinition* WeaponData;
	
	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	FGameplayTag ItemId;

	// The hand slot index that this weapon is currently in, if not in world. Is only replicated and modified on spawn.
	UPROPERTY(Replicated, BlueprintReadOnly, VisibleAnywhere, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	int HandSlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	const UItemStaticData* ItemData;
	
	// Ensure to call base Initialize when overriding
	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void Initialize(bool InitializeWeaponData = true, bool InitializeStaticMesh = true);
	
	/* Note: Cooldown is handled by GearManagerComponent, returns true in base class but can be overriden
	 * @return true if the weapon can attack, false otherwise */
	UFUNCTION(BlueprintPure, BlueprintCallable, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	bool CanAttack();

	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void OnAttackPerformed();

	/* Returns a transform representing an offset of how the weapon should attach
	 * By default this will try to get a matching socket on the static mesh
	* @param SocketName the socket that the weapon is being attached to on the holding actor */
	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	FTransform GetAttachTransform(FName SocketName);
	virtual FTransform GetAttachTransform_Impl(FName SocketName);
	
	UFUNCTION(BlueprintAuthorityOnly, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon", meta=(DisplayName = "GetAttackMontage"))
	int32 GetAttackMontageId(int32 MontageIdOverride = -1);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon", meta=(DisplayName = "GetAttackMontage"))
	FAttackMontageData GetAttackMontage(int32 MontageId);

	UFUNCTION(Server, Reliable)
	void Equip_Server();
	
	UFUNCTION(NetMulticast, Reliable)
	void Equip_Multicast();
	
	UFUNCTION(BlueprintInternalUseOnly, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void Equip_Impl();
	

	void Holster();

	UFUNCTION(NetMulticast, Reliable)
	void HolsterMulticast();

	UFUNCTION(Server, Reliable)
	void HolsterServer();
protected:
	
	/* Index of the next attack montage to play, only managed on server */
	int32 MontageCycleIndex = -1;
	TArray<TSharedPtr<FStreamableHandle>> AnimationHandles;
};