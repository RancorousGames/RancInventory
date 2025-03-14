// Copyright Rancorous Games, 2024


#include "GearManagerComponent.h"

#include "LogRancInventorySystem.h"
#include "WeaponActor.h"
#include "Engine/EngineTypes.h"
#include "Async/IAsyncTask.h"
#include "Core/RISFunctions.h"
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
			AddAndSetSelectedWeapon(DefaultUnarmedWeaponData);
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
	EquipGear(SlotTag, Data, PreviousItem, false, EGearChangeStep::Request);
}

void UGearManagerComponent::HandleItemRemovedFromSlot(const FGameplayTag& SlotTag, const UItemStaticData* Data, int32 Quantity, EItemChangeReason Reason)
{
	if (LinkedInventoryComponent->GetItemForTaggedSlot(SlotTag).ItemId != Data->ItemId)
	{
		UnequipGear(SlotTag, Data, false);
	}
}

void UGearManagerComponent::AddAndSetSelectedWeapon(const UItemStaticData* ItemData, FGameplayTag ForcedSlot)
{
	if (!IsValid(ItemData))
	{
		return;
	}

	int32 ExistingEntry = SelectableWeaponsData.Find(ItemData);
	if (ExistingEntry == INDEX_NONE)
	{
		if (SelectableWeaponsData.Num() == MaxSelectableWeaponCount)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("NumberOfWeaponsAcquired >= WeaponSlots, replaced earliest weapon"))

			SelectableWeaponsData.RemoveAt(0);
		}

		SelectableWeaponsData.Add(ItemData);
		ExistingEntry = SelectableWeaponsData.Num() - 1;
	}

	const auto* WeaponData = ItemData->GetItemDefinition<UWeaponDefinition>();

	if (!WeaponData)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponData is nullptr."))
		return;
	}

	
	//if (WeaponData == 

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
		if (!WeaponToReplace->ItemData)
		{
			UE_LOG(LogRISInventory, Warning, TEXT("WeaponToReplace->ItemData is nullptr. EquipWeapon() Failed"))
			return;
		}
		
		if (WeaponToReplace->ItemData == ItemData)
			return;
	}
	
	AWeaponActor* WeaponActor = SpawnWeapon_IfServer(ItemData, WeaponData);

	AttachWeaponToOwner(WeaponActor, HandSlot->AttachSocketName);
	if (HandSlot == MainHandSlot)
		MainhandSlotWeapon = WeaponActor;
	else if (HandSlot == OffhandSlot)
		OffhandSlotWeapon = WeaponActor;

	WeaponActor->Equip();

	OnWeaponSelected.Broadcast(HandSlot->SlotTag, WeaponActor);

	ActiveWeaponIndex = ExistingEntry;
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

AWeaponActor* UGearManagerComponent::GetWeaponForSlot(const FGearSlotDefinition* Slot) const
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

	const FGearSlotDefinition* HandSlot;
	if (ForcedSlot.IsValid())
	{
		HandSlot = FindGearSlotDefinition(ForcedSlot);
	}
	else
	{
		HandSlot = GetHandSlotToUse(ItemData->GetItemDefinition<UWeaponDefinition>());
	}

	LinkedInventoryComponent->MoveItem(ItemData->ItemId, 1, FGameplayTag(), HandSlot->SlotTag);
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
	for (FGearSlotDefinition& GearSlot : GearSlots)
	{
		if (GearSlot.SlotTag == SlotTag)
		{
			return &GearSlot;
		}
	}

	return nullptr;
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
                if (GearSlot->SlotTag == MainHandSlot->SlotTag)
                    MainhandSlotWeapon = nullptr;
                else if (GearSlot->SlotTag == OffhandSlot->SlotTag)
                    OffhandSlotWeapon = nullptr;

                WeaponToUnequip->Holster();
                OnWeaponHolstered.Broadcast(Slot, WeaponToUnequip);

                if (!DefaultUnarmedWeaponData || WeaponToUnequip->ItemData != DefaultUnarmedWeaponData)
                {
                    WeaponToUnequip->Destroy();
                    if (!MainhandSlotWeapon && !OffhandSlotWeapon && UnarmedWeaponActor)
                    {
                        SelectUnarmed_Server();
                    }
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
	        if (UWeaponDefinition* WeaponData = NewItemData->GetItemDefinition<UWeaponDefinition>())
        	{
        		AddAndSetSelectedWeapon(NewItemData, Slot);
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

bool UGearManagerComponent::PlayEquipMontage(AWeaponActor* WeaponActor)
{
	if (!Check(WeaponActor) || !WeaponActor->WeaponData)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("WeaponActor or WeaponData is nullptr."))
		return false;
	}

	UAnimMontage* EquipMontage = GetEquipMontage(WeaponActor->WeaponData).Montage.Get();
	const float PlayLength = PlayMontage(Owner, EquipMontage, GetEquipMontage(WeaponActor->WeaponData).PlayRate, FName(""));

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

			if (bRecordAttackTraces)
			{
				if (UWeaponAttackRecorderComponent* RecorderComponent = NewObject<UWeaponAttackRecorderComponent>(Owner, UWeaponAttackRecorderComponent::StaticClass()))
				{
					RecorderComponent->RegisterComponent();
				}
			}

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


FMontageData UGearManagerComponent::GetUnequipMontage(const UGearDefinition* WeaponData) const
{
	if (!WeaponData || !WeaponData->HolsterMontage.Montage.IsValid())
		return DefaultUnequipMontage;
	return WeaponData->HolsterMontage;
}

FMontageData UGearManagerComponent::GetEquipMontage(const UGearDefinition* WeaponData) const
{
	if (!WeaponData || !WeaponData->EquipMontage.Montage.IsValid())
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


//////////////////////// BEHAVIOR ////////////////////////


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