// Copyright Rancorous Games, 2024


#include "GearManagerComponent.h"

#include "LogRancInventorySystem.h"
#include "WeaponActor.h"
#include "Engine/EngineTypes.h"
#include "Async/IAsyncTask.h"
#include "Core/RISFunctions.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "RecordingSystem/WeaponAttackRecorderDataTypes.h"

// Sets default values for this component's properties
UGearManagerComponent::UGearManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);	
}

void UGearManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	auto* OwningActor = GetOwner();
	Owner = Cast<ACharacter>(OwningActor);
	if (Owner)
	{
		LinkedInventoryComponent = Owner->GetComponentByClass<UInventoryComponent>();

		if (!LinkedInventoryComponent)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("LinkedInventoryComponent is nullptr."))
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
	if (!Owner || !LinkedInventoryComponent)
	{
		UE_LOG(LogRISInventory, Error, TEXT("Owner or LinkedInventoryComponent is nullptr."))
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
		if (GearSlot.SlotType == EGearSlotType::MainHand)
		{
			MainHandSlot = &GearSlot;
		}
		else if (GearSlot.SlotType == EGearSlotType::OffHand)
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
			UnarmedWeaponActor = MainhandSlotWeapon;
		}
	}
}

AWeaponActor* UGearManagerComponent::GetActiveWeapon()
{
	if (MainhandSlotWeapon) return MainhandSlotWeapon;

	return OffhandSlotWeapon;
}

const UWeaponDefinition* UGearManagerComponent::GetMainhandWeaponSlotData()
{
	if (!MainhandSlotWeapon) return nullptr;

	return MainhandSlotWeapon->WeaponData;
}

const UWeaponDefinition* UGearManagerComponent::GetOffhandWeaponData()
{
	if (!OffhandSlotWeapon) return nullptr;

	return OffhandSlotWeapon->WeaponData;
}

void UGearManagerComponent::HandleItemAddedToSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, FTaggedItemBundle PreviousItem, EItemChangeReason Reason)
{
//	if (const UWeaponStaticData* WeaponData = Cast<UWeaponStaticData>(Data))
//	{
//		
//		AddAndSelectWeapon(WeaponData);
//	}
//	else
//	{
		EquipGear(SlotTag, Data, PreviousItem, true);
//	}
}

void UGearManagerComponent::HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, EItemChangeReason Reason)
{
	UnequipGear(SlotTag, Data, true);
}

void UGearManagerComponent::TryAttack_Server_Implementation(FVector AimLocation, bool ForceOffHand, int32 MontageIdOverride)
{
	auto* WeaponActor =  MainhandSlotWeapon;
	if (!WeaponActor || !WeaponActor->CanAttack() || ForceOffHand) WeaponActor = OffhandSlotWeapon;
	
	if (!IsValid(WeaponActor))
		return;

	const float WeaponCooldown = WeaponActor->WeaponData->Cooldown;

	if (GetWorld()->GetTimeSeconds() - LastAttackTime > WeaponCooldown)
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();

		if (WeaponActor->CanAttack())
		{
			Attack_Multicast(AimLocation, WeaponActor == OffhandSlotWeapon, MontageIdOverride);
		}
	}
}

void UGearManagerComponent::Attack_Multicast_Implementation(FVector AimLocation, bool UseOffhand, int32 MontageIdOverride)
{
	AWeaponActor* WeaponActor = UseOffhand ? OffhandSlotWeapon : MainhandSlotWeapon;

	if (!WeaponActor)
		return;

	RotateToAimLocation(AimLocation);
	WeaponActor->PerformAttack();
	FMontageData AttackMontage = WeaponActor->GetAttackMontage(MontageIdOverride);

	PlayMontage(Owner, AttackMontage.Montage.Get(), AttackMontage.PlayRate, FName(""));
}

void UGearManagerComponent::AddAndSelectWeapon(const UItemStaticData* ItemData, FGameplayTag ForcedSlot)
{
	if (!IsValid(ItemData))
	{
		return;
	}

	int32 ExistingEntry = SelectableWeaponsData.Find(ItemData);
	if (ExistingEntry != INDEX_NONE)
	{
		SelectActiveWeapon_Server(ExistingEntry, false, ForcedSlot);
	}
	else
	{
		if (SelectableWeaponsData.Num() == MaxSelectableWeaponCount)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("NumberOfWeaponsAcquired >= WeaponSlots, replaced earliest weapon"))

			SelectableWeaponsData.RemoveAt(0);
		}

		SelectableWeaponsData.Add(ItemData);
		SelectActiveWeapon_Server(SelectableWeaponsData.Num() - 1, false, ForcedSlot, nullptr);
	}
}

const FGearSlotDefinition* UGearManagerComponent::GetHandSlotToUse(const UWeaponDefinition* WeaponData) const
{
	switch (WeaponData->HandCompatability)
	{
	case EHandCompatibility::OnlyMainHand:
	case EHandCompatibility::TwoHanded:
		return MainHandSlot;
	case EHandCompatibility::OnlyOffhand:
		// Return offhand unless mainhand is two handed
		return MainhandSlotWeapon && MainhandSlotWeapon->WeaponData->HandCompatability == EHandCompatibility::TwoHanded ? nullptr : OffhandSlot;
	case EHandCompatibility::BothHands:
		{
			if (MainhandSlotWeapon && !MainhandSlotWeapon->WeaponData->IsLowPriority && MainhandSlotWeapon->WeaponData->HandCompatability != EHandCompatibility::TwoHanded && !OffhandSlotWeapon)
				return OffhandSlot;
			return MainHandSlot;
		}
	}

	UE_LOG(LogRISInventory, Warning, TEXT("GetHandSlotToUse() failed."))

	return nullptr;
}

const AWeaponActor* UGearManagerComponent::GetWeaponForSlot(const FGearSlotDefinition* Slot) const
{
	if (Slot == MainHandSlot && MainhandSlotWeapon)
	{
		return MainhandSlotWeapon;
	}
	else if (Slot == OffhandSlot && OffhandSlotWeapon)
	{
		return OffhandSlotWeapon;
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
		UE_LOG(LogRISInventory, Warning, TEXT("AttachWeaponToOwner() failed, CharMesh is nullptr"))
		return;
	}

	FVector WeaponWorldScale = InputWeaponActor->GetActorScale3D(); //WorldSpace scale

	InputWeaponActor->AttachToComponent(CharMesh, AttachRules, SocketName);
	// print SocketName
	UE_LOG(LogRISInventory, Warning, TEXT("SocketName: %s"), *SocketName.ToString());
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
		UE_LOG(LogRISInventory, Warning, TEXT("Owner a nullptr. Initialize this component propertly!"));

		return false;
	}


	if (!InputWeaponActor)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("InputWeaponActor is nullptr!"))
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


void UGearManagerComponent::SelectActiveWeapon(int32 WeaponIndex, bool bPlayEquipMontage, AWeaponActor* AlreadySpawnedWeapon)
{
	SelectActiveWeapon_Server(WeaponIndex, bPlayEquipMontage, FGameplayTag(), AlreadySpawnedWeapon);
}

void UGearManagerComponent::SelectActiveWeapon_Server_Implementation(int32 WeaponIndex, bool bPlayEquipMontage, FGameplayTag ForcedSlot, AWeaponActor* AlreadySpawnedWeapon)
{

	if (SelectableWeaponsData.Num() == 0 || WeaponIndex < 0 || WeaponIndex > SelectableWeaponsData.Num())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Slot is < 0 || Slot > WeaponSlot. EquipWeapon() Failed"))
		return;
	}

	bool DelayConfigured = false;
	const auto* ItemData = SelectableWeaponsData[WeaponIndex];
	const auto* WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>(UWeaponDefinition::StaticClass());
	if (bPlayEquipMontage)
	{
		// When a delay is configured, we want to play 
		DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Equip, FGameplayTag::EmptyTag, ItemData, WeaponIndex);
	}


	if (!WeaponData)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponData is nullptr."))
		return;
	}

	AWeaponActor* WeaponActor = AlreadySpawnedWeapon;
	if (!DelayConfigured && !WeaponActor)
	{
		WeaponActor = SpawnWeapon_IfServer(ItemData, WeaponData);

		if (!Check(WeaponActor) || !Owner)
		{
			return;
		}
	}

	const FGearSlotDefinition* HandSlot;
	if (ForcedSlot.IsValid())
	{
		HandSlot = FindGearSlotDefinition(ForcedSlot);
	}
	else
	{
		HandSlot = GetHandSlotToUse(WeaponData);
	}
	
	if (const AWeaponActor* WeaponToReplace = GetWeaponForSlot(HandSlot))
	{
		if (bPlayEquipMontage)
		{
			EquipMontageToBlendInto = WeaponData->EquipMontage;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_UnequipEquipBlendDelay, this, &UGearManagerComponent::PlayBlendInEquipMontage, EquipUnequipAnimBlendDelay, false);
		}

		if (!WeaponToReplace->ItemData)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("WeaponToReplace->ItemData is nullptr. EquipWeapon() Failed"))
			return;
		}
		
		UnequipGear(HandSlot->SlotTag, WeaponToReplace->ItemData, bPlayEquipMontage);
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
			MainhandSlotWeapon = WeaponActor;
		}
		else if (HandSlot == OffhandSlot)
		{
			OffhandSlotWeapon = WeaponActor;
		}
		ActiveWeaponIndex = WeaponIndex;
		WeaponActor->Equip();
	}

	if (!DelayConfigured)
		OnWeaponSelected.Broadcast(HandSlot->SlotTag, WeaponActor);
}

void UGearManagerComponent::SelectUnarmed_Server_Implementation()
{
	if (UnarmedWeaponActor)
	{
		OffhandSlotWeapon = nullptr;
		ActiveWeaponIndex = 0;
		MainhandSlotWeapon = UnarmedWeaponActor;
		UnarmedWeaponActor->Equip();
		
		OnWeaponSelected.Broadcast(MainHandSlot->SlotTag, UnarmedWeaponActor);
	}
}

void UGearManagerComponent::EquipGear(FGameplayTag Slot, const UItemStaticData* NewItemData, FTaggedItemBundle PreviousItem, bool PlayEquipMontage, bool PlayUnequipMontage)
{
    if (!NewItemData)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("ItemData is nullptr. EquipGear() Failed"));
        return;
    }

    // Find the gear slot definition for the given slot
    FGearSlotDefinition* GearSlot = FindGearSlotDefinition(Slot);
    if (!GearSlot)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("No gear slot found for slot %s"), *Slot.ToString());
        return;
    }

    // Check if there is an item already equipped in this slot.
    if (PreviousItem.IsValid())
    {
        // There is an item already in the slot. Retrieve its item data.
        const UItemStaticData* PreviousItemData = URISSubsystem::GetItemDataById(PreviousItem.ItemId);
        if (!PreviousItemData)
        {
            UE_LOG(LogRISInventory, Error, TEXT("Existing item data is nullptr. EquipGear() Failed"));
            return;
        }

        // Play the unequip montage for the existing item.
        UnequipGear(Slot, PreviousItemData, PlayUnequipMontage);

        // Return immediately. After the montage has played (or the delay expires),
        // the function is expected to be called again to proceed with equipping.
        return;
    }

    // No existing item in the slot – proceed with equipping the new item.
    // Hide the slot’s mesh so that the new item’s mesh can be set or the montage can blend in.
    if (GearSlot->MeshComponent)
    {
        GearSlot->MeshComponent->SetVisibility(false);
    }

    // Configure a delay if an equip montage is to be played.
    bool DelayConfigured = false;
    if (PlayEquipMontage)
    {
        DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Equip, Slot, NewItemData, 0, PreviousItem);
    }

    // Determine if this is a weapon item or a non-weapon item.
    UWeaponDefinition* WeaponData = NewItemData->GetItemDefinition<UWeaponDefinition>(UWeaponDefinition::StaticClass());
    if (WeaponData)
    {
        // Weapon item: if no delay is configured, spawn/select and attach the weapon.
        if (!DelayConfigured)
        {
            AddAndSelectWeapon(NewItemData, Slot);
        }
        else if (PlayEquipMontage && !EquipMontageToBlendInto.IsValid())
        {
            // If a delay is active, play the default equip montage now.
            PlayMontage(Owner, DefaultEquipMontage.Montage.Get(), DefaultEquipMontage.PlayRate, FName(""));
        }
    }
    else
    {
        // Non-weapon item: if using delays, update the slot mesh.
        if (DelayConfigured)
        {
            GearSlot->MeshComponent->SetStaticMesh(NewItemData->ItemWorldMesh);
            GearSlot->MeshComponent->SetWorldScale3D(NewItemData->ItemWorldScale);
        }
        if (!EquipMontageToBlendInto.IsValid())
        {
            PlayMontage(Owner, DefaultEquipMontage.Montage.Get(), DefaultEquipMontage.PlayRate, FName(""));
        }
    }

    // Finally, if there was no delay then broadcast that the gear has been equipped.
    if (!DelayConfigured)
    {
        OnGearEquipped.Broadcast(Slot, NewItemData->ItemId);
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

bool UGearManagerComponent::SetupDelayedGearChange(EPendingGearChangeType InPendingGearChangeType, const FGameplayTag& GearChangeSlot, const UItemStaticData* ItemData, int32 WeaponSelectionIndex, FTaggedItemBundle PreviousItem)
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
		DelayedPreviousItem = PreviousItem;
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
		EquipGear(DelayedGearChangeSlot, DelayedGearChangeItemData, DelayedPreviousItem, false);
		break;
	case EPendingGearChangeType::Unequip:
		UnequipGear(DelayedGearChangeSlot, DelayedGearChangeItemData, false);
		break;
	case EPendingGearChangeType::WeaponSelect:
		SelectActiveWeapon_Server(DelayedWeaponSelectionIndex, false);
		break;
	default: ;
	}

	DelayedGearChangeSlot = FGameplayTag();
	DelayedPreviousItem = FTaggedItemBundle();
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
	DelayedPreviousItem = FTaggedItemBundle();
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
	UWeaponDefinition* WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>(UWeaponDefinition::StaticClass());

	PlayUnequipMontage = PlayUnequipMontage && WeaponData->HolsterMontage.Montage.IsValid();
	if (PlayUnequipMontage)
	{
		DelayConfigured = SetupDelayedGearChange(EPendingGearChangeType::Unequip, Slot, ItemData);
	}
	
	if (ItemData && WeaponData && Slot == MainHandSlot->SlotTag || Slot == OffhandSlot->SlotTag)
	{
		if (WeaponToUnequip == nullptr)
		{
			// Todo: Note that with this current solution, the unequip is effectively not cancelable
			if (Slot == MainHandSlot->SlotTag && WeaponData->HandCompatability == EHandCompatibility::OnlyOffhand && OffhandSlotWeapon)
			{
				WeaponToUnequip = OffhandSlotWeapon;
				OffhandSlotWeapon = nullptr;
			}
			else if (Slot == OffhandSlot->SlotTag && OffhandSlotWeapon)
			{
				WeaponToUnequip = OffhandSlotWeapon;
				OffhandSlotWeapon = nullptr;
			}
			else
			{
				WeaponToUnequip = MainhandSlotWeapon;
				MainhandSlotWeapon = nullptr;
			}
		}
		
		if (!WeaponToUnequip)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("WeaponToUnequip is nullptr."))
			return;
		}

		if (PlayUnequipMontage)
		{
			PlayWeaponHolsterMontage(WeaponToUnequip);
		}
		if (!DelayConfigured)
		{			
			WeaponToUnequip->Holster(); // TODO: Doesn't make sense to tell the weapon to holster if we are going to destroy it after. 
			OnWeaponHolstered.Broadcast(Slot, WeaponToUnequip);
			if (!DefaultUnarmedWeaponData || WeaponToUnequip->ItemData != DefaultUnarmedWeaponData)
			{
				WeaponToUnequip->Destroy();
				if (!MainhandSlotWeapon && !OffhandSlotWeapon && UnarmedWeaponActor)
				{
					SelectUnarmed_Server();
				}
			}
			WeaponToUnequip = nullptr;
			OnEquippedWeaponsChange.Broadcast();
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

	if (!IsValid(ItemData) || !ItemData->ItemId.IsValid())
	{
		UE_LOG(LogRISInventory, Error, TEXT("ItemData or ItemData->ItemId is invalid. Something went wrong"))
		return;
	}
	
	if (!DelayConfigured)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("OnGearUnequipped.Broadcast slot %s, item %s"), *Slot.ToString(), *ItemData->ItemId.ToString())
		OnGearUnequipped.Broadcast(Slot, ItemData->ItemId);
	}
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
		UE_LOG(LogRISInventory, Warning, TEXT("No weapons to select!"))
		return;
	}

	const int32 NextWeaponIndex = (ActiveWeaponIndex + 1) % SelectableWeaponsData.Num();
	
	SelectActiveWeapon_Server(NextWeaponIndex, bPlayMontage);
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
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponActor or WeaponData is nullptr."))
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

AWeaponActor* UGearManagerComponent::SpawnWeapon_IfServer(const UItemStaticData* ItemData, const UWeaponDefinition* WeaponData)
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
			NewWeaponActor->ItemData = ItemData;

			NewWeaponActor->FinishSpawning(FTransform(FRotator(0.0f, 0.0f, 0.0f), SpawnLocation, FVector::OneVector));
			return NewWeaponActor;
		}

	UE_LOG(LogRISInventory, Warning, TEXT("Failed to spawn weapon actor!"))
	return nullptr;
}

float UGearManagerComponent::PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName, bool bShowDebugWarnings)
{
	if (!OwnerChar)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("OwnerChar is a nullptr, PlayMontage() returning 0.0f "))
		}

		return 0.0f;
	}

	if (!Montage)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Montage is a nullptr, PlayMontage() returning 0.0f "))
		}


		return 0.0f;
	}

	if (PlayRate < 0.0f)
	{
		if (bShowDebugWarnings)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Playrate was < 0, setting Playrate = 1!"))
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
	OnAttackAnimNotifyEndEvent.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("Attack replay stopped."));
}