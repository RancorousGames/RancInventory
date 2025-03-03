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
 * 
 * Base class for a replicated weapon, meant to be attached to character skeletal mesh.
 * Notably overrides GetNetConnection() to return the net connection of GetAttachParentActor() , so firing components attached to this actor can
 * call RPCs.
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	const UItemStaticData* ItemData;
	
	/* Index of the next attack montage to play */
	UPROPERTY(BlueprintReadWrite, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	int32 MontageCycleIndex = -1;

	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void Initialize(bool InitializeWeaponData = true, bool InitializeStaticMesh = true);
	
	virtual void Initialize_Impl(bool InitializeWeaponData = true, bool InitializeStaticMesh = true);

	/* Function to notify the weapon an attack is requested
	 * @return true if the weapon can attack, false otherwise */
	UFUNCTION(BlueprintPure, BlueprintCallable, BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	bool CanAttack();
	virtual bool CanAttack_Impl();

	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	void PerformAttack();
	virtual void PerformAttack_Impl();

	/* Returns a transform representing an offset of how the weapon should attach
	 * By default this will try to get a matching socket on the static mesh
	* @param SocketName the socket that the weapon is being attached to on the holding actor */
	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon")
	FTransform GetAttachTransform(FName SocketName);
	virtual FTransform GetAttachTransform_Impl(FName SocketName);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Ranc Inventory | Weapon", meta=(DisplayName = "GetAttackMontage"))
	FMontageData GetAttackMontage(int32 MontageIdOverride = -1);

	virtual FMontageData GetAttackMontage_Impl(int32 MontageIdOverride = -1);

	
	void Equip();

	UFUNCTION(NetMulticast, Reliable)
	void EquipMulticast();

	// Called on all clients and server, lets C++ override
	virtual void EquipMulticastImpl();

	UFUNCTION(Server, Reliable)
	void EquipServer();
	
	void Holster();

	UFUNCTION(NetMulticast, Reliable)
	void HolsterMulticast();

	UFUNCTION(Server, Reliable)
	void HolsterServer();
	
	/*
	Weapon no longer inventory. This is what you call when a weapon is dropped from ACharacter to clean up
	*/
	void Remove();

	UFUNCTION(NetMulticast, Reliable)
	void RemoveMulticast();

	UFUNCTION(Server, Reliable)
	void RemoveServer();
	
	UPROPERTY(BlueprintReadWrite, Category = "Ranc Inventory | Weapon", meta = (ExposeOnSpawn))
	bool PlacedInWorld = true;
	
	UFUNCTION(BlueprintCallable, Category = "Ranc Inventory | Weapon", meta = (BlueprintGetter = "IsPlacedInWorld"))
	virtual bool IsPlacedInWorld_Implementation()
	{
		return PlacedInWorld;
	}

private:
	TArray<TSharedPtr<FStreamableHandle>> AnimationHandles;
	
};