// Copyright Rancorous Games, 2024


#include "GearManagerComponent.h"

#include "LogRancInventorySystem.h"
#include "WeaponActor.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "RecordingSystem/WeaponAttackRecorderComponent.h"
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

	
	for (int32 i = 0; i < GearSlots.Num(); ++i)
	{
		FGearSlotDefinition& GearSlot = GearSlots[i];
		// if mainhand or offhand then save reference MainHandSlot OffhandSlot
		if (GearSlot.SlotType == EGearSlotType::MainHand)
		{
			MainHandSlotIndex = i;
		}
		else if (GearSlot.SlotType == EGearSlotType::OffHand)
		{
			OffhandSlotIndex = i;
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
			AddAndSetSelectedWeapon_IfServer(DefaultUnarmedWeaponData);
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
	EquipGear(SlotTag, Data, PreviousItem, false, EGearChangeStep::Request);
}

void UGearManagerComponent::HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, EItemChangeReason Reason)
{
	if (LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag).ItemId != Data->ItemId)
	{
		UnequipGear(SlotTag, Data, false);
	}
}

bool UGearManagerComponent::CanAttack_Implementation(FVector AimLocation, bool ForceOffHand)
{
	auto* WeaponActor =  MainhandSlotWeapon;
	if (!WeaponActor || !WeaponActor->CanAttack() || ForceOffHand) WeaponActor = OffhandSlotWeapon;
	
	if (!IsValid(WeaponActor))
		return false;

	const float WeaponCooldown = WeaponActor->WeaponData->Cooldown;

	if (GetWorld()->GetTimeSeconds() - LastAttackTime > WeaponCooldown)
	{
		LastAttackTime = GetWorld()->GetTimeSeconds();

		if (WeaponActor->CanAttack())
		{
			return true;
		}
	}
	return false;
}

// Called only on Server
void UGearManagerComponent::AddAndSetSelectedWeapon_IfServer(const UItemStaticData* ItemData, FGameplayTag ForcedSlot)
{
	if (!IsValid(ItemData) || !Owner->HasAuthority())
	{
		return;
	}
	
	const auto* WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>();

	if (!WeaponData)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponData is nullptr."))
		return;
	}
	
	int32 HandSlotIndex;
	if (ForcedSlot.IsValid())
	{
		HandSlotIndex = FindGearSlotIndex(ForcedSlot);
	}
	else
	{
		HandSlotIndex = GetHandSlotIndexToUse(WeaponData);
	}
	
	if (const AWeaponActor* WeaponToReplace = GetWeaponForSlot(&GearSlots[HandSlotIndex]))
	{
		if (!WeaponToReplace->ItemData)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("WeaponToReplace->ItemData is nullptr. EquipWeapon() Failed"))
			return;
		}
		
		if (WeaponToReplace->ItemData == ItemData)
			return;
	}
	
	
	SpawnWeapon_IfServer(ItemData, WeaponData, HandSlotIndex);
	
	// Weapon will call back into RegisterSpawnedWeapon on server and client
}

void UGearManagerComponent::RegisterSpawnedWeapon(AWeaponActor* WeaponActor)
{
	int32 HandSlotIndex = WeaponActor->HandSlotIndex;
	if (GearSlots.IsValidIndex(HandSlotIndex))
	{
		const FGearSlotDefinition* HandSlot = &GearSlots[HandSlotIndex];
		// Use the pointer safely
	}
	
	if (HandSlotIndex == MainHandSlotIndex)
		MainhandSlotWeapon = WeaponActor;
	else if (HandSlotIndex == OffhandSlotIndex)
		OffhandSlotWeapon = WeaponActor;

	WeaponActor->Equip_Server();

	OnWeaponSelected.Broadcast(GearSlots[HandSlotIndex].SlotTag, WeaponActor);

	int32 ExistingEntry = SelectableWeaponsData.Find(WeaponActor->ItemData);
	if (ExistingEntry == INDEX_NONE)
	{
		// Note: This also modifies the local version of replicated SelectableWeaponsData on clients. The server will override it (but usually without changes)
		if (SelectableWeaponsData.Num() == MaxSelectableWeaponCount)
		{
			UE_LOG(LogRISInventory, Display, TEXT("NumberOfWeaponsAcquired >= WeaponSlots, replaced earliest weapon"))

			SelectableWeaponsData.RemoveAt(0);
		}

		SelectableWeaponsData.Add(WeaponActor->ItemData);
	}
	
	ActiveWeaponIndex = ExistingEntry;

	AttachWeaponToOwner(WeaponActor, GearSlots[HandSlotIndex].AttachSocketName);

	// Finally load any montage attack recording data
	TArray<FSoftObjectPath> PathsToLoad;
	for (const FAttackMontageData& MontageData : WeaponActor->WeaponData->AttackMontages)
	{
		if (!MontageData.RecordedTraceSequence.IsNull())
		{
			// Convert the soft reference to a soft object path
			PathsToLoad.Add(MontageData.RecordedTraceSequence.ToSoftObjectPath());
		}
	}

	// Get the Streamable Manager from the Asset Manager
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	// Request an asynchronous load for all the gathered paths
	Streamable.RequestAsyncLoad(PathsToLoad, FStreamableDelegate::CreateLambda([this, PathsToLoad]()
	{
		// Once all assets are loaded, iterate over the paths and retrieve the loaded objects
		for (const FSoftObjectPath& Path : PathsToLoad)
		{
			if (UObject* LoadedObject = Path.ResolveObject())
				LoadedAttackAssets.Add(LoadedObject);
		}
	}));
}

int32 UGearManagerComponent::GetHandSlotIndexToUse(const UWeaponDefinition* WeaponData) const
{
	switch (WeaponData->HandCompatability)
	{
	case EHandCompatibility::OnlyMainHand:
	case EHandCompatibility::TwoHanded:
		return MainHandSlotIndex;
	case EHandCompatibility::OnlyOffhand:
		if (MainhandSlotWeapon && MainhandSlotWeapon->WeaponData->HandCompatability == EHandCompatibility::TwoHanded)
			return -1;
		return OffhandSlotIndex;
	case EHandCompatibility::BothHands:
		{
			if (MainhandSlotWeapon && 
				!MainhandSlotWeapon->WeaponData->IsLowPriority && 
				MainhandSlotWeapon->WeaponData->HandCompatability != EHandCompatibility::TwoHanded && 
				!OffhandSlotWeapon)
				return OffhandSlotIndex;
			return MainHandSlotIndex;
		}
	}

	UE_LOG(LogRISInventory, Warning, TEXT("GetHandSlotIndexToUse() failed."))
	return -1;
}

AWeaponActor* UGearManagerComponent::GetWeaponForSlot(const FGearSlotDefinition* Slot) const
{
	int32 SlotIndex = FindGearSlotIndex(Slot->SlotTag);
	
	if (SlotIndex == MainHandSlotIndex && MainhandSlotWeapon)
	{
		return MainhandSlotWeapon;
	}
	else if (SlotIndex == OffhandSlotIndex && OffhandSlotWeapon)
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

void UGearManagerComponent::SelectActiveWeapon(int32 WeaponIndex, bool bPlayEquipMontage, AWeaponActor* AlreadySpawnedWeapon)
{
	SelectActiveWeapon_Server(WeaponIndex, FGameplayTag(), AlreadySpawnedWeapon, bPlayEquipMontage ? EGearChangeStep::Request : EGearChangeStep::Apply);
}

void UGearManagerComponent::SelectActiveWeapon_Server_Implementation(int32 WeaponIndex, FGameplayTag ForcedSlot, AWeaponActor* AlreadySpawnedWeapon, EGearChangeStep Step)
{
	const UItemStaticData* ItemData = SelectableWeaponsData.IsValidIndex(WeaponIndex)
		                                    ? SelectableWeaponsData[WeaponIndex]
		                                    : nullptr;

	if (!ItemData)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponData is nullptr."))
		return;
	}

	int32 HandSlotIndex;
	if (ForcedSlot.IsValid())
	{
		HandSlotIndex = FindGearSlotIndex(ForcedSlot);
	}
	else
	{
		HandSlotIndex = GetHandSlotIndexToUse(ItemData->GetItemDefinition<UWeaponDefinition>());
	}

	LinkedInventoryComponent->MoveItem(ItemData->ItemId, 1, FGameplayTag(), GearSlots[HandSlotIndex].SlotTag);
}

void UGearManagerComponent::SelectUnarmed_Server_Implementation()
{
	if (DefaultUnarmedWeaponData)
	{
		AddAndSetSelectedWeapon_IfServer(DefaultUnarmedWeaponData);
	}
}

void UGearManagerComponent::OnGearChangeAnimNotify(FName NotifyName, const FBranchingPointNotifyPayload& _)
{
	if (NotifyName.Compare(GearChangeCommitAnimNotifyName) == 0)
	{
		ProcessNextGearChange();
	}
}


void UGearManagerComponent::CancelGearChange()
{
	HandleInterruption();
}

FGearSlotDefinition* UGearManagerComponent::FindGearSlotDefinition(FGameplayTag SlotTag)
{
	return &GearSlots[FindGearSlotIndex(SlotTag)];
}

int32 UGearManagerComponent::FindGearSlotIndex(FGameplayTag SlotTag) const
{
	for (int32 i = 0; i < GearSlots.Num(); ++i)
	{
		if (GearSlots[i].SlotTag == SlotTag)
		{
			return i;
		}
	}
	return -1;
}

void UGearManagerComponent::UnequipGear(FGameplayTag Slot, const UItemStaticData* ItemData, bool SkipAnim, EGearChangeStep Step)
{
    UGearDefinition* GearData = ItemData->GetItemDefinition<UWeaponDefinition>();

    switch (Step)
    {
        case EGearChangeStep::Request:
        {
            FGearChangeTransaction Transaction;
            Transaction.ChangeType = EPendingGearChangeType::Unequip;
            Transaction.NextStep = SkipAnim ? EGearChangeStep::Apply : EGearChangeStep::PlayAnim;
            Transaction.Slot = Slot;
            Transaction.OldItemData = ItemData;
            QueueGearChange(Transaction);
            break;
        }

        case EGearChangeStep::PlayAnim:
        {
            auto animDefinition = GetUnequipMontage(GearData);
            if (auto* anim = animDefinition.Montage.Get())
            {
                PlayMontage(Owner, anim, animDefinition.PlayRate, FName(""));
            }
            break;
        }

        case EGearChangeStep::Apply:
        {
            const UWeaponDefinition* WeaponData = Cast<UWeaponDefinition>(GearData);
            FGearSlotDefinition* GearSlot = FindGearSlotDefinition(Slot);

            if (AWeaponActor* WeaponToUnequip = GetWeaponForSlot(GearSlot))
            {
                if (GearSlot->SlotTag == GearSlots[MainHandSlotIndex].SlotTag)
                    MainhandSlotWeapon = nullptr;
                else if (GearSlot->SlotTag == GearSlots[OffhandSlotIndex].SlotTag)
                    OffhandSlotWeapon = nullptr;

                WeaponToUnequip->Holster();
                OnWeaponHolstered.Broadcast(Slot, WeaponToUnequip);

                WeaponToUnequip->Destroy();
                if ((!DefaultUnarmedWeaponData || WeaponToUnequip->ItemData != DefaultUnarmedWeaponData) &&
                	!MainhandSlotWeapon && !OffhandSlotWeapon && UnarmedWeaponActor)
                {
                    SelectUnarmed_Server();
                }

                OnEquippedWeaponsChange.Broadcast();
            }
            else
            {
                if (GearSlot->MeshComponent && GearSlot->MeshComponent->GetStaticMesh() != nullptr)
                {
                    GearSlot->MeshComponent->SetVisibility(false);
                    GearSlot->MeshComponent->SetStaticMesh(nullptr);
                }
            }
            break;
        }

        default:
            UE_LOG(LogRISInventory, Error, TEXT("Invalid GearChangeStep."));
            break;
    }
}


void UGearManagerComponent::EquipGear(FGameplayTag Slot, const UItemStaticData* NewItemData, FTaggedItemBundle PreviousItem, bool SkipAnim, EGearChangeStep Step)
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
	
	UGearDefinition* GearData = NewItemData->GetItemDefinition<UWeaponDefinition>();

	switch (Step)
    {
        case EGearChangeStep::Request:
        {
            FGearChangeTransaction Transaction;
            Transaction.ChangeType = EPendingGearChangeType::Equip;
            Transaction.NextStep = SkipAnim ? EGearChangeStep::Apply : EGearChangeStep::PlayAnim;
            Transaction.Slot = Slot;
            Transaction.NewItemData = NewItemData;
            QueueGearChange(Transaction);
            break;
        }

        case EGearChangeStep::PlayAnim:
        {
            auto animDefinition = GetEquipMontage(GearData);
            if (auto* anim = animDefinition.Montage.Get())
            {
                PlayMontage(Owner, anim, animDefinition.PlayRate, FName(""));
            }
            break;
        }

        case EGearChangeStep::Apply:
        {
        	// If the active weapon is unarmed then unequip it
        	if (GetActiveWeapon() == UnarmedWeaponActor)
			{
				UnequipGear(GearSlots[MainHandSlotIndex].SlotTag, DefaultUnarmedWeaponData, true, EGearChangeStep::Apply);
			}
        		
	        if (UWeaponDefinition* WeaponData = NewItemData->GetItemDefinition<UWeaponDefinition>())
        	{
        		AddAndSetSelectedWeapon_IfServer(NewItemData, Slot);
                OnEquippedWeaponsChange.Broadcast();
        	}
        	else
        	{
        		GearSlot->MeshComponent->SetStaticMesh(NewItemData->ItemWorldMesh);
        		GearSlot->MeshComponent->SetWorldScale3D(NewItemData->ItemWorldScale);
        		GearSlot->MeshComponent->SetVisibility(true);
        	}
        	OnGearEquipped.Broadcast(Slot, NewItemData->ItemId);
            break;
        }

        default:
            UE_LOG(LogRISInventory, Error, TEXT("Invalid GearChangeStep."));
            break;
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
	
	SelectActiveWeapon_Server(NextWeaponIndex, FGameplayTag(), nullptr, bPlayMontage ? EGearChangeStep::Request : EGearChangeStep::Apply);
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

AWeaponActor* UGearManagerComponent::SpawnWeapon_IfServer(const UItemStaticData* ItemData, const UWeaponDefinition* WeaponData, int32 HandSlotIndex)
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
	
	if (AWeaponActor* NewWeaponActor = Cast<AWeaponActor>(
			GetWorld()->SpawnActorDeferred<AWeaponActor>(
				WeaponClass,
				FTransform(FRotator(0.0f, 0.0f, 0.0f), SpawnLocation, FVector::OneVector),
				GetOwner())))  // This sets the owner.
	{
		NewWeaponActor->ItemId = ItemData->ItemId;
		NewWeaponActor->ItemData = ItemData;
		NewWeaponActor->HandSlotIndex = HandSlotIndex;
		NewWeaponActor->SetOwner(Owner);

		if (bRecordAttackTraces)
		{
			if (UWeaponAttackRecorderComponent* RecorderComponent = NewObject<UWeaponAttackRecorderComponent>(
					NewWeaponActor, UWeaponAttackRecorderComponent::StaticClass()))
			{
				NewWeaponActor->AddOwnedComponent(RecorderComponent);
				RecorderComponent->RegisterComponent();
			}
		}

		NewWeaponActor->FinishSpawning(FTransform(FRotator(0.0f, 0.0f, 0.0f), SpawnLocation, FVector::OneVector));
		return NewWeaponActor;
	}

	UE_LOG(LogRISInventory, Warning, TEXT("Failed to spawn weapon actor!"))
	return nullptr;
}

float UGearManagerComponent::PlayMontage(ACharacter* OwnerChar, UAnimMontage* Montage, float PlayRate, FName StartSectionName)
{
	if (!OwnerChar)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("UGearManagerComponent::PlayMontage: OwnerChar is a nullptr, PlayMontage() returning 0.0f "))

		return 0.0f;
	}

	if (!Montage)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("UGearManagerComponent::PlayMontage: Montage is a nullptr, PlayMontage() returning 0.0f "))

		return 0.0f;
	}

	if (PlayRate < 0.0f)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("UGearManagerComponent::PlayMontage: Playrate was < 0, setting Playrate = 1!"))
		PlayRate = 1.0f;
	}
	
	return OwnerChar->PlayAnimMontage(Montage, PlayRate, StartSectionName);
}


void UGearManagerComponent::RotateToAimLocation(FVector AimLocation)
{
	if (bRotateToAttackDirection && !AimLocation.IsZero())
	{
		RotateToAttackTargetYaw = FRotationMatrix::MakeFromX(AimLocation - Owner->GetActorLocation()).Rotator().Yaw;

		// Start rotation immediately on the next tick
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_RotationUpdate, this, &UGearManagerComponent::UpdateRotation, GetWorld()->GetDeltaSeconds(), true);
	}
}


FMontageData UGearManagerComponent::GetUnequipMontage(const UGearDefinition* WeaponData) const
{
	if (!WeaponData || !WeaponData->HolsterMontage.Montage.Get())
		return DefaultUnequipMontage;
	return WeaponData->HolsterMontage;
}


FMontageData UGearManagerComponent::GetEquipMontage(const UGearDefinition* WeaponData) const
{
	if (!WeaponData || !WeaponData->EquipMontage.Montage.Get())
		return DefaultEquipMontage;
	return WeaponData->EquipMontage;
}

void UGearManagerComponent::QueueGearChange(const FGearChangeTransaction& Transaction)
{
    // Remove any pending transactions for the same slot and same type
    PendingGearChanges.RemoveAll([&](const FGearChangeTransaction& PendingTx) {
        return PendingTx.Slot == Transaction.Slot && PendingTx.ChangeType == Transaction.ChangeType;
    });
    
    // Add the new transaction
    PendingGearChanges.Add(Transaction);
    
    // Start processing if no active transaction
    if (!bHasActiveTransaction)
    {
        ProcessNextGearChange();
    }
}

void UGearManagerComponent::ProcessNextGearChange()
{
    // If queue is empty, we're done
    if (PendingGearChanges.Num() == 0)
    {
        bHasActiveTransaction = false;
        return;
    }
    
    // Get the next transaction
    ActiveGearChange = &PendingGearChanges[0];
    bHasActiveTransaction = true;
	float ChangeDelay = 0.0f;

	switch (ActiveGearChange->ChangeType)
	{
	case EPendingGearChangeType::Equip:
		EquipGear(ActiveGearChange->Slot, ActiveGearChange->NewItemData, FTaggedItemBundle(), false, ActiveGearChange->NextStep);
		ChangeDelay = ActiveGearChange->NextStep == EGearChangeStep::PlayAnim ? EquipDelay : 0.0f;
		break;
	case EPendingGearChangeType::Unequip:
		UnequipGear(ActiveGearChange->Slot, ActiveGearChange->OldItemData, false, ActiveGearChange->NextStep);
		ChangeDelay = ActiveGearChange->NextStep == EGearChangeStep::PlayAnim ? UnequipDelay : 0.0f;
		break;
	default:
		break;
	}
	
	if (ActiveGearChange->NextStep == EGearChangeStep::Apply)
		PendingGearChanges.RemoveAt(0);
	else
		// Increment step and leave it at top of queue
		ActiveGearChange->NextStep = static_cast<EGearChangeStep>(static_cast<int>(ActiveGearChange->NextStep) + 1);
	
	if (ChangeDelay > 0)
	{
		if (GearChangeCommitAnimNotifyName.IsNone())
		{
			GetWorld()->GetTimerManager().SetTimer(
				GearChangeCommitHandle, 
				this, 
				&UGearManagerComponent::ProcessNextGearChange, 
				ChangeDelay, 
				false
			);
		}
	}
	else
	{
		ProcessNextGearChange();
	}
	
}

void UGearManagerComponent::HandleInterruption()
{
    // Cancel timer
    if (GearChangeCommitHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(GearChangeCommitHandle);
    }
    
    bIsInterrupted = true;
    
    // Reset state
    bHasActiveTransaction = false;
    
    // Clear transaction queue if needed
    PendingGearChanges.Empty();
}

void UGearManagerComponent::OnAttackTraceStateBeginEnd_Implementation(bool Started)
{
	if (!IsRunningDedicatedServer() && IsValid(ReplayedAttackData))
		SendAttackTraceAimRPC_Client();
}

FAttackAimParams UGearManagerComponent::GetAttackTraceAimParams_Implementation()
{
	auto Forward = Owner->GetActorForwardVector();
	float AimYaw = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
	float AimPitch = FMath::RadiansToDegrees(FMath::Asin(Forward.Z));
	
	return FAttackAimParams{AimYaw, AimPitch};
}

//////////////////////// BEHAVIOR ////////////////////////


void UGearManagerComponent::TryAttack_Server_Implementation(FVector AimLocation, bool ForceOffHand, int32 MontageIdOverride)
{
	if (CanAttack(AimLocation, ForceOffHand))
	{
		auto* WeaponActor =  MainhandSlotWeapon;
		if (!WeaponActor || !WeaponActor->CanAttack() || ForceOffHand) WeaponActor = OffhandSlotWeapon;
	
		int32 MontageId = MontageIdOverride >= 0 ? MontageIdOverride : WeaponActor->GetAttackMontageId();
		Attack_Multicast(AimLocation, MontageId, WeaponActor == OffhandSlotWeapon);
	}
}

void UGearManagerComponent::Attack_Multicast_Implementation(FVector AimLocation, int32 MontageId, bool UseOffhand)
{
	AWeaponActor* WeaponActor = UseOffhand ? OffhandSlotWeapon : MainhandSlotWeapon;

	if (!WeaponActor)
		return;

	RotateToAimLocation(AimLocation);
	WeaponActor->OnAttackPerformed();
	FAttackMontageData AttackMontage = WeaponActor->GetAttackMontage(MontageId);
	OnAttackPerformed.Broadcast(AttackMontage);

	if (auto* RecordedAttack = AttackMontage.RecordedTraceSequence.Get())
	{
		PlayRecordedAttackSequence(RecordedAttack);
	}

	PlayMontage(Owner, AttackMontage.Montage, AttackMontage.PlayRate, FName(""));
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
	FQuat TargetQuat = FQuat(FRotator(0.f, RotateToAttackTargetYaw, 0.f));

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
    ReplayedAttackData = AttackData;

	AttackStartTime = GetWorld()->GetTimeSeconds();
}

void UGearManagerComponent::SendAttackTraceAimRPC_Client()
{
	ensureMsgf(!IsRunningDedicatedServer(), TEXT("SendAttackTraceAimRPC_Client() called on dedicated server!"));
	
	double MaxTimeUntilNextUpdate = ReplayedAttackData->FirstTraceDelay - (GetWorld()->GetTimeSeconds() - AttackStartTime);
	
	if (MaxTimeUntilNextUpdate < 0.05f)
	{
		GetWorld()->GetTimerManager().ClearTimer(SendAimDirectionRPC_TimerHandle);
		
		ReceivedReplayAttackAimParams = GetAttackTraceAimParams();
		SetAimInformationRPC_Server(ReceivedReplayAttackAimParams, true);
		// We also want to trace on client
		ContinueAttackReplay();
	}
	else
	{
		// Not currently active code, but built to ensure we call the RPC around the time of FirstTraceDelay. can be adjusted to call multiple times during the trace as well
		float TimeUntilNextUpdate = FMath::Min(MaxTimeUntilNextUpdate, 0.1f);
		GetWorld()->GetTimerManager().SetTimer(SendAimDirectionRPC_TimerHandle, this, &UGearManagerComponent::SendAttackTraceAimRPC_Client, TimeUntilNextUpdate, false);
	}
}


void UGearManagerComponent::SetAimInformationRPC_Server_Implementation(FAttackAimParams AimParams, bool FinalAimUpdate)
{
	ensureMsgf(GetOwner()->HasAuthority(), TEXT("SendAttackTraceAimRPC_Client() called on non authority!"));
	
	ReceivedReplayAttackAimParams = AimParams;

	if (FinalAimUpdate)
	{
		if ((GetWorld()->GetTimeSeconds() - AttackStartTime) > ReplayedAttackData->FirstTraceDelay + 0.4f)
		{
			// Final attack arrived too late, drop it
			return;
		}
		
		ContinueAttackReplay();
	}
}

void UGearManagerComponent::ContinueAttackReplay()
{
    if (!IsValid(ReplayedAttackData) || ReplayedAttackData->AttackSequence.Num() == 0)
    {
        StopAttackReplay();
        return;
    }


	FTransform PivotOffsetTransform;
	PivotOffsetTransform.SetRotation(FQuat(Owner->GetActorRotation()));
    ReplayOwnerAttackOrigin.SetLocation(Owner->GetActorLocation() + PivotOffsetTransform.TransformPosition(ReplayAttackPivotLocationOffset));
    if (ReceivedReplayAttackAimParams.AimPitch != 0 || ReceivedReplayAttackAimParams.AimYaw != 0)
    {
    	FQuat YawQuat = FQuat(FRotator(0.f, ReceivedReplayAttackAimParams.AimYaw, 0.f));
    	FQuat PitchQuat = FQuat(FRotator(ReceivedReplayAttackAimParams.AimPitch, 0.f, 0.f));
    	ReplayOwnerAttackOrigin.SetRotation(YawQuat * PitchQuat);  // Yaw then Pitch
    }
    else
		ReplayOwnerAttackOrigin.SetRotation(FQuat(Owner->GetActorRotation()));
	
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
    	FVector StartPosition = ReplayOwnerAttackOrigin.TransformPosition(CurrentTimestamp.SocketPositions[SocketIndex]);
    	FVector EndPosition = ReplayOwnerAttackOrigin.TransformPosition(NextTimestamp.SocketPositions[SocketIndex]);
    	
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(GetOwner());

	    if (bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartPosition, EndPosition, TraceChannel, QueryParams))
        {
	        if (AActor* HitActor = HitResult.GetActor())
            {
                // Broadcast an event when a hit is detected
                OnHitDetected.Broadcast(HitActor, HitResult);
            }
        }

        #if WITH_EDITOR
        DrawDebugLine(GetWorld(), StartPosition, EndPosition, FColor::Red, false, TimeDelta, 0, 2.0f);
        #endif
    }

    // Schedule the next trace step
    ReplayCurrentIndex++;
    GetWorld()->GetTimerManager().SetTimer(AttackTrace_TimerHandle, this, &UGearManagerComponent::ContinueAttackReplay, TimeDelta, false);
}

void UGearManagerComponent::StopAttackReplay()
{
    GetWorld()->GetTimerManager().ClearTimer(AttackTrace_TimerHandle);
    ReplayedAttackData = nullptr;
	OnAttackAnimNotifyEndEvent.Broadcast();
}