// Copyright Rancorous Games, 2024

#include "Components\ItemContainerComponent.h"

#include "GameplayTagsManager.h"
#include "LogRancInventorySystem.h"
#include "Components/InventoryComponent.h"
#include "Data/ItemInstanceData.h"
#include "Core/RISSubsystem.h"
#include "Data/UsableItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

const TArray<UItemInstanceData*> UItemContainerComponent::NoInstances;

UItemContainerComponent::UItemContainerComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UItemContainerComponent::InitializeComponent()
{
	// This check is necessary because of tests since there's some weirdness going on
	if (!HasBeenInitialized())
		Super::InitializeComponent();


	// Merge initial items based on item ids
	for (int i = InitialItems.Num() - 1; i >= 0; --i)
	{
		for (int j = i - 1; j >= 0; --j)
		{
			if (InitialItems[i].ItemData && InitialItems[i].ItemData->ItemId == InitialItems[j].ItemData->ItemId)
			{
				InitialItems[j].Quantity += InitialItems[i].Quantity;
				InitialItems.RemoveAt(i);
				break;
			}
		}
	}

	// add all initial items to items
	for (const FInitialItem& InitialItem : InitialItems)
	{
		if (InitialItem.ItemData && InitialItem.ItemData->ItemId.IsValid())
		{
			//ItemsVer.Items.Add(FItemBundle(InitialItem.ItemData->ItemId, InitialItem.Quantity));
			// TODO: Remove or implement instancedata creation
		}
	}

	UpdateWeightAndSlots();
	RebuildItemsToCache();

	if (DropItemClass == nullptr)
	{
		DropItemClass = AWorldItem::StaticClass();
	}
}

bool UItemContainerComponent::Contains(const FGameplayTag& ItemId, int32 Quantity) const
{
	return ContainsInstances(ItemId, Quantity, NoInstances);
}

bool UItemContainerComponent::ContainsInstances(const FGameplayTag& ItemId, int32 Quantity,
                                                TArray<UItemInstanceData*> InstancesToLookFor) const
{
	auto ContainedInstance = FindItemInstance(ItemId);
	return ContainedInstance && ContainedInstance->Contains(Quantity, InstancesToLookFor);
}

int32 UItemContainerComponent::GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const
{
	auto* ContainedInstance = FindItemInstance(ItemId);


	if (!ContainedInstance)
		return 0;

	return ContainedInstance->Quantity;
}

bool UItemContainerComponent::ContainsByPredicate(const FGameplayTag& ItemId,
                                                  const FBPItemInstancePredicate& Predicate, int32 Quantity) const
{
	if (Quantity <= 0)
		return true; // Requesting zero or negative quantity is always true


	if (!ItemId.IsValid())
		return false; // Invalid ItemId cannot be contained

	const FItemBundle* FoundItemBundle = FindItemInstance(ItemId);

	if (!FoundItemBundle || !FoundItemBundle->IsValid()) // Check if item exists at all
		return false; // Item not found in the container

	// Check if the item type uses instance data
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return false;

	if (!ItemData->DefaultInstanceDataTemplate) // Check if the Class pointer itself is null/invalid
	{
		UE_LOG(LogRISInventory, Verbose,
		       TEXT("ContainsByPredicate: Item %s does not use Instance Data. Checking total quantity."),
		       *ItemId.ToString());
		return false;
	}

	if (FoundItemBundle->Quantity != FoundItemBundle->InstanceData.Num() && FoundItemBundle->InstanceData.Num() > 0)
	{
		// Another inconsistency: Quantity doesn't match instance count.
		UE_LOG(LogRISInventory, Warning,
		       TEXT(
			       "ContainsByPredicate: Item %s Quantity (%d) does not match InstanceData count (%d). Predicate check might be unreliable."
		       ),
		       *ItemId.ToString(), FoundItemBundle->Quantity, FoundItemBundle->InstanceData.Num());
	}


	int32 SatisfyingCount = 0;
	for (const UItemInstanceData* InstanceData : FoundItemBundle->InstanceData)
	{
		if (InstanceData) // Ensure pointer is valid
		{
			if (Predicate.Execute(InstanceData))
			{
				SatisfyingCount++;

				if (SatisfyingCount >= Quantity)
					return true;
			}
		}
		else
		{
			UE_LOG(LogRISInventory, Warning,
			       TEXT("ContainsByPredicate: Found null pointer in InstanceData array for item %s."),
			       *ItemId.ToString());
		}
	}

	return SatisfyingCount >= Quantity;
}

bool UItemContainerComponent::IsEmpty() const
{
	return ItemsVer.Items.Num() == 0;
}

bool UItemContainerComponent::CanReceiveItem(const FGameplayTag& ItemId, int32 QuantityToReceive,
                                             bool SwapBackAllowed) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData || BadItemData(ItemData, ItemId))
		return false;
	return GetReceivableQuantity(ItemData, QuantityToReceive, false, SwapBackAllowed) >=
		QuantityToReceive;
}

int32 UItemContainerComponent::GetReceivableQuantity(const UItemStaticData* ItemData, int32 RequestedQuantity,
                                                     bool AllowPartial, bool SwapBackAllowed) const
{
	if (BadItemData(ItemData)) return 0;

	const int32 ViableQuantityByWeight = GetQuantityContainerCanReceiveByWeight(ItemData);
	int32 ViableQuantityBySlotCount = GetQuantityContainerCanReceiveBySlots(ItemData);

	if (SwapBackAllowed && ViableQuantityBySlotCount == 0) ViableQuantityBySlotCount = ItemData->MaxStackSize;
	// If we are allowed to swap back (max 1 item) then we will have the slots at least one stack

	int32 FinalViableQuantity = FMath::Min(ViableQuantityBySlotCount, ViableQuantityByWeight);

	if (!AllowPartial && FinalViableQuantity < RequestedQuantity) return 0;

	if (OnValidateAddItem.IsBound())
		FinalViableQuantity = FMath::Min(FinalViableQuantity, OnValidateAddItem.Execute(ItemData->ItemId, FinalViableQuantity, FGameplayTag::EmptyTag));

	return FMath::Min(FinalViableQuantity, RequestedQuantity);
}

int32 UItemContainerComponent::GetQuantityContainerCanReceiveByWeight(const UItemStaticData* ItemData) const
{
	// Calculate how many items can be added without exceeding the max weight
	if (ItemData->ItemWeight <= 0) return INT32_MAX;


	int32 ViableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	ViableQuantityByWeight = ViableQuantityByWeight > 0 ? ViableQuantityByWeight : 0;

	return ViableQuantityByWeight;
}

int32 UItemContainerComponent::GetQuantityContainerCanReceiveBySlots(const UItemStaticData* ItemData) const
{
	const int32 ContainedQuantity = GetQuantityTotal_Implementation(ItemData->ItemId);
	const int32 ItemQuantityTillNextFullSlot = // e.g. 3/5 = 2, 5/5 = 0, 0/5 = 0, 14/5 = 1
		ItemData->MaxStackSize > 1
			? ContainedQuantity > 0 && ContainedQuantity % ItemData->MaxStackSize != 0
				  ? ItemData->MaxStackSize - (ContainedQuantity % ItemData->MaxStackSize)
				  : 0
			: 0;
	int32 SlotsTakenPerStack = 1;
	if (JigsawMode)
	{
		SlotsTakenPerStack = ItemData->JigsawSizeX * ItemData->JigsawSizeY;
	}


	const int32 AvailableSlots = MaxSlotCount - UsedContainerSlotCount;
	const int32 ViableQuantityBySlotCount = (AvailableSlots / SlotsTakenPerStack) * ItemData->MaxStackSize +
		ItemQuantityTillNextFullSlot;

	return ViableQuantityBySlotCount;
}

TArray<FItemBundle> UItemContainerComponent::GetAllItems() const
{
	return ItemsVer.Items;
}

TArray<UItemInstanceData*> UItemContainerComponent::GetItemInstanceData(const FGameplayTag& ItemId) const
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData;
	}


	return TArray<UItemInstanceData*>();
}

UItemInstanceData* UItemContainerComponent::GetSingleItemInstanceData(const FGameplayTag& ItemId) const
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData.Num() > 0 ? Instance->InstanceData[0] : nullptr;
	}


	return nullptr;
}

int32 UItemContainerComponent::AddItem_IfServer(
	TScriptInterface<IItemSource> ItemSource,
	const FGameplayTag& ItemId,
	int32 RequestedQuantity,
	bool AllowPartial,
	bool SuppressEvents,
	bool SuppressUpdate)
{
	return AddItemWithInstances_IfServer(ItemSource, ItemId, RequestedQuantity,
		NoInstances, AllowPartial, SuppressEvents, SuppressUpdate);
}

int32 UItemContainerComponent::AddItemWithInstances_IfServer(
	TScriptInterface<IItemSource> ItemSource,
	const FGameplayTag& ItemId,
	int32 RequestedQuantity,
	const TArray<UItemInstanceData*>& InstancesToExtract, // Now takes instances
	bool AllowPartial,
	bool SuppressEvents,
	bool SuppressUpdate)
{
	if (IsClient("AddItem_IfServer"))
		return 0;


	UObject* ItemSourceObj = ItemSource.GetObjectRef();
	if (!ItemSourceObj || !ItemId.IsValid())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("AddItem_IfServer: Item source is null or ItemId is invalid!"));
		return 0;
	}

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return 0;

	const bool bUseSpecificInstances = InstancesToExtract.Num() > 0;
	const int32 QuantityToValidate = bUseSpecificInstances ? InstancesToExtract.Num() : RequestedQuantity;

	if (QuantityToValidate <= 0)
	{
		return 0; // Requesting zero or negative quantity
	}

	// 1. Validate THIS container's capacity first
	// Use GetReceivableQuantityImpl to avoid virtual calls if overridden in InventoryComponent unnecessarily
	const int32 ViableQuantity = FMath::Min(RequestedQuantity,
		GetReceivableQuantity(ItemData, QuantityToValidate, AllowPartial));

	if (ViableQuantity <= 0 || (ViableQuantity < QuantityToValidate && !AllowPartial))
	{
		UE_LOG(LogRISInventory, Verbose,
		       TEXT("AddItem_IfServer: Target container cannot receive item %s (Capacity: %d, Requested: %d)"),
		       *ItemId.ToString(), ViableQuantity, QuantityToValidate);
		return 0; // Target cannot receive any
	}

	// 2. Attempt to extract from the source
	TArray<UItemInstanceData*> ExtractedInstances; // Temporary array to hold extracted instances
	// Pass the specific instances to extract if they were provided
	int32 ActualExtractedQuantity = Execute_ExtractItem_IfServer(
		ItemSourceObj,
		ItemId,
		ViableQuantity,
		InstancesToExtract, // Pass specific instances if provided
		EItemChangeReason::Transferred,
		ExtractedInstances,
		AllowPartial
	);

	if (ActualExtractedQuantity <= 0)
	{
		UE_LOG(LogRISInventory, Verbose, TEXT("AddItem_IfServer: Source failed to provide item %s (Requested attempt: %d)"),
		       *ItemId.ToString(), ViableQuantity);
		return 0; // Source couldn't provide the item
	}

	// If specific instances were requested, verify we got exactly those (and the correct quantity)
	if (bUseSpecificInstances)
	{
		if (ActualExtractedQuantity != InstancesToExtract.Num())
		{
			UE_LOG(LogRISInventory, Warning,
			       TEXT(
				       "AddItem_IfServer: Source extracted %d instances, but %d specific instances were requested for %s. Aborting add."
			       ), ActualExtractedQuantity, InstancesToExtract.Num(), *ItemId.ToString());
			// TODO: Need a robust way to potentially return the incorrectly extracted items back to the source if possible.
			// For now, aborting is safest to prevent state mismatch.
			return 0;
		}
	}
	// If extracting by quantity, ActualExtractedQuantity might be less than AmountToAttempt if source was limited. This is OK.


	// 3. Add the successfully extracted items to THIS container
	FItemBundle* ContainedItem = FindItemInstanceMutable(ItemId);
	bool bCreatedNewBundle = false; // Flag if we created a new entry vs finding existing
	if (!ContainedItem)
	{
		ItemsVer.Items.Add(FItemBundle(ItemId));
		ContainedItem = &ItemsVer.Items.Last();
		bCreatedNewBundle = true;
	}

	ContainedItem->Quantity += ActualExtractedQuantity;

	// Handle instance data ownership transfer - NO CREATION
	if (IsValid(ItemData->DefaultInstanceDataTemplate)) // Check if item type *should* have instance data
	{
		if (ExtractedInstances.Num() > 0) // Instances were provided by the source
		{
			ensureMsgf(ExtractedInstances.Num() == ActualExtractedQuantity,
			           TEXT(
				           "AddItem_IfServer: Mismatch between ActualExtractedQuantity (%d) and ExtractedInstances count (%d) for %s."
			           ), ActualExtractedQuantity, ExtractedInstances.Num(), *ItemId.ToString());

			for (UItemInstanceData* ExtractedInstanceData : ExtractedInstances)
			{
				if (ExtractedInstanceData)
				{
					// Take ownership: Initialize in context of *this* component and register
					ExtractedInstanceData->Initialize(true, nullptr, this);
					GetOwner()->AddReplicatedSubObject(ExtractedInstanceData);
					ContainedItem->InstanceData.Add(ExtractedInstanceData); // Add to internal array
				}
				else
				{
					UE_LOG(LogRISInventory, Warning,
					       TEXT("AddItem_IfServer: Encountered null pointer in ExtractedInstances array for %s."),
					       *ItemId.ToString());
				}
			}
		}
		// If ExtractedInstances is empty but DefaultInstanceDataTemplate is valid, log a warning because we are NOT creating instances here.
		else if (ActualExtractedQuantity > 0)
		{
			UE_LOG(LogRISInventory, Warning,
			       TEXT(
				       "AddItem_IfServer: Item %s requires instance data, but source did not provide any. Instances will be missing."
			       ), *ItemId.ToString());
		}

		// Final consistency check
		ensureMsgf(
			ContainedItem->InstanceData.Num() == 0 || ContainedItem->InstanceData.Num() == ContainedItem->Quantity,
			TEXT("AddItem_IfServer: InstanceData count corrupt after add for %s. Qty: %d, Inst: %d"),
			*ItemId.ToString(), ContainedItem->Quantity, ContainedItem->InstanceData.Num());
	}

	// Cleanup potentially empty bundle if extraction failed entirely after creation
	if (ActualExtractedQuantity <= 0 && bCreatedNewBundle)
	{
		ItemsVer.Items.RemoveSingleSwap(*ContainedItem); // Use RemoveSingleSwap if order doesn't matter
	}

	// 4. Final Updates and Events
	if (!SuppressUpdate)
		UpdateWeightAndSlots();
	if (!SuppressEvents)
		// Broadcast using the successfully ExtractedInstances array
		OnItemAddedToContainer.Broadcast(ItemData, ActualExtractedQuantity, ExtractedInstances,
		                                 EItemChangeReason::Transferred); // Use Transferred reason?

	if (GetOwnerRole() == ROLE_Authority || GetOwnerRole() == ROLE_None)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);
	}

	return ActualExtractedQuantity; // Return the actual quantity successfully added
}

void UItemContainerComponent::SetAddItemValidationCallback(const FAddItemValidationDelegate& ValidationDelegate)
{
	OnValidateAddItem = ValidationDelegate;
}

int32 UItemContainerComponent::UseItem(const FGameplayTag& ItemId, int32 ItemToUseInstanceId)
{
	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return 0;

	const UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

	if (!UsableItem)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return 0;
	}

	int32 QuantityToRemove = UsableItem->QuantityPerUse;

	if (GetOwnerRole() != ROLE_Authority && QuantityToRemove >= 1)
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, QuantityToRemove));

	UseItem_Server(ItemId, ItemToUseInstanceId);

	// On client this is just a guess
	return QuantityToRemove;
}

int32 UItemContainerComponent::DropItem(const FGameplayTag& ItemId, int32 Quantity,
                                        TArray<UItemInstanceData*> InstancesToDrop, FVector RelativeDropLocation)
{
	if (!ContainsInstances(ItemId, Quantity, InstancesToDrop))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Cannot drop item: %s"), *ItemId.ToString());
		return 0;
	}


	if (GetOwnerRole() != ROLE_Authority)
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, Quantity));

	DropItemFromContainer_Server(ItemId, Quantity, FItemBundle::ToInstanceIds(InstancesToDrop), RelativeDropLocation);

	// On client the below is just a guess

	return Quantity;
}

void UItemContainerComponent::RequestMoveItemToOtherContainer(
	UItemContainerComponent* TargetComponent,
	const FGameplayTag& ItemId,
	int32 Quantity,
	const TArray<UItemInstanceData*>& InstanceToMove,
	const FGameplayTag& SourceTaggedSlot,
	const FGameplayTag& TargetTaggedSlot)
{
	if (!ContainsInstances(ItemId, Quantity, InstanceToMove))
		return;


	// predict and call server
	if (IsClient())
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, Quantity));

	RequestMoveItemToOtherContainer_Server(
		TargetComponent,
		ItemId,
		Quantity,
		FItemBundle::ToInstanceIds(InstanceToMove),
		SourceTaggedSlot,
		TargetTaggedSlot);
}

int32 UItemContainerComponent::DestroyItem_IfServer(const FGameplayTag& ItemId, int32 Quantity,
                                                    TArray<UItemInstanceData*> InstancesToDestroy,
                                                    EItemChangeReason Reason, bool AllowPartial)
{
	return DestroyItemImpl(ItemId, Quantity, InstancesToDestroy, Reason, AllowPartial, false, false);
}

int32 UItemContainerComponent::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity,
                                                                   const TArray<UItemInstanceData*>& InstancesToExtract,
                                                                   EItemChangeReason Reason,
                                                                   TArray<UItemInstanceData*>& StateArrayToAppendTo,
                                                                   bool AllowPartial)
{
	return ExtractItem_ServerImpl(ItemId, Quantity, InstancesToExtract, Reason, StateArrayToAppendTo, AllowPartial,
	                                false, false);
}

int32 UItemContainerComponent::DropAllItems_IfServer()
{
	return DropAllItems_ServerImpl();
}

void UItemContainerComponent::Clear_IfServer()
{
	ClearServerImpl();
}

void UItemContainerComponent::RequestMoveItemToOtherContainer_Server_Implementation(
	UItemContainerComponent* TargetComponent,
	const FGameplayTag& ItemId,
	int32 Quantity,
	const TArray<int32>& InstanceIdsToMove,
	const FGameplayTag& SourceTaggedSlot,
	const FGameplayTag& TargetTaggedSlot)
{
	UInventoryComponent::MoveBetweenContainers_ServerImpl(
		this,
		TargetComponent,
		ItemId,
		Quantity,
		InstanceIdsToMove,
		SourceTaggedSlot,
		TargetTaggedSlot);
}

void UItemContainerComponent::DropItemFromContainer_Server_Implementation(
	const FGameplayTag& ItemId, int32 Quantity, const TArray<int32>& InstanceIdsToDrop, FVector RelativeDropLocation)
{
	FItemBundle* Item = FindItemInstanceMutable(ItemId);
	DropItemFromContainer_ServerImpl(Item->ItemId, Quantity, Item->FromInstanceIds(InstanceIdsToDrop), RelativeDropLocation);
}

void UItemContainerComponent::DropItemFromContainer_ServerImpl(FGameplayTag ItemId, int32 Quantity,
                                                               const TArray<UItemInstanceData*>& InstancesToDrop,
                                                               FVector RelativeDropLocation)
{
	if (IsClient("DropItemFromContainer_ServerImpl"))
		return;
	
	TArray<UItemInstanceData*> DroppedItemInstancesArray = TArray<UItemInstanceData*>();

	int32 Extracted = ExtractItem_IfServer_Implementation(ItemId, Quantity, InstancesToDrop,
	                                                      EItemChangeReason::Dropped, DroppedItemInstancesArray, false);

	if (Extracted <= 0)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("DropItemFromContainer_ServerImpl: Items not found"));
		return;
	}

	SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, Extracted, RelativeDropLocation,
	                                           DroppedItemInstancesArray);
}

int32 UItemContainerComponent::DropAllItems_ServerImpl()
{
	if (IsClient("DropAllItems_ServerImpl"))
		return 0;

	int32 DroppedStacksCount = 0;
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogRISInventory, Error, TEXT("DropAllItems_ServerImpl: Cannot drop items, OwnerActor is null."));
		return 0;
	}

	const int32 InitialItemTypeCount = ItemsVer.Items.Num(); // For angle calculation
	float CurrentAngle = FMath::FRand() * 360.0f; // Start at a random angle
	const float AngleStep = (InitialItemTypeCount > 0) ? (360.0f / InitialItemTypeCount) : 0.0f;

	// Loop while the container still has items. DropItemFromContainer_Server will extract quantity,
	// eventually emptying the bundles and causing them to be removed from ItemsVer.Items.
	while (ItemsVer.Items.Num() > 0)
	{
		FItemBundle& ItemToProcess = ItemsVer.Items.Last();
		FGameplayTag& ItemIdToProcess = ItemToProcess.ItemId;
		int32 CurrentQuantityOfItem = ItemToProcess.Quantity;
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemIdToProcess);

		// 3a. Get Item Data and Stack Size
		if (!ItemIdToProcess.IsValid() || !IsValid(ItemData))
		{
			UE_LOG(LogRISInventory, Error, TEXT("DropAllItems_ServerImpl: ItemId %s is invalid or ItemData not found."),
			       *ItemIdToProcess.ToString());
			ItemsVer.Items.Pop();
			continue;
		}

		if (CurrentQuantityOfItem <= 0)
		{
			ItemsVer.Items.Pop();
			continue;
		}

		int32 QuantityForThisStack = FMath::Min(CurrentQuantityOfItem, ItemData->MaxStackSize);

		FVector Direction = FVector::ForwardVector.RotateAngleAxis(CurrentAngle, FVector::UpVector);
		FVector Offset = Direction * DefaultDropDistance;
		Offset += FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(0.f, 50.f));
		FVector DropLocation = OwnerActor->GetActorLocation() + OwnerActor->GetActorRotation().RotateVector(Offset);
		// Ensure offset respects actor rotation
		TArray<UItemInstanceData*> ItemInstancesToDrop;
		// Grab QuantityForThisStack instances from the end of ItemToProcess.InstanceData
		if (ItemToProcess.InstanceData.Num() > 0)
			for (int32 i = ItemToProcess.InstanceData.Num() - 1; i >= 0 && ItemInstancesToDrop.Num() <
			     QuantityForThisStack; --i)
				if (UItemInstanceData* InstanceData = ItemToProcess.InstanceData[i])
					ItemInstancesToDrop.Add(InstanceData);

		DropItemFromContainer_ServerImpl(ItemToProcess.ItemId, QuantityForThisStack, ItemInstancesToDrop, DropLocation);

		DroppedStacksCount++;
		CurrentAngle += AngleStep;
	}

	UpdateWeightAndSlots();
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return DroppedStacksCount;
}

void UItemContainerComponent::UseItem_Server_Implementation(const FGameplayTag& ItemId, int32 ItemToUseInstanceId)
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId)) return;

	UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

	if (!UsableItem)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return;
	}

	UItemInstanceData* UsedInstance = nullptr;
	TArray<UItemInstanceData*> InstancesToUse;
	if (IsValid(ItemData->DefaultInstanceDataTemplate))
	{
		auto AllInstanceData = GetItemInstanceData(ItemId);
		for (int i = AllInstanceData.Num() - 1; i >= 0; --i)
		{
			if (AllInstanceData[i]->UniqueInstanceId == ItemToUseInstanceId || ItemToUseInstanceId < 0)
			{
				InstancesToUse.Add(AllInstanceData[i]);
				UsedInstance = AllInstanceData[i];
				if (ItemToUseInstanceId >= 0 || InstancesToUse.Num() >= UsableItem->QuantityPerUse)
					break;
			}
		}
	}

	if ((UsableItem->QuantityPerUse == 0 && InstancesToUse.Num() == 0) ||
		ContainsInstances(ItemId, UsableItem->QuantityPerUse, InstancesToUse))
	{
		UsableItem->Use(GetOwner(), ItemData, UsedInstance);

		if (UsableItem->QuantityPerUse > 0)
		{
			const int32 DestroyedQuantity = DestroyItem_IfServer(ItemId, UsableItem->QuantityPerUse, InstancesToUse,
			                                                     EItemChangeReason::Consumed, false);
			ensureMsgf(DestroyedQuantity == UsableItem->QuantityPerUse,
			           TEXT("Used item %s but destroyed %d, expected %d"), *ItemId.ToString(), DestroyedQuantity,
			           UsableItem->QuantityPerUse);
		}
	}
}

void UItemContainerComponent::SpawnItemIntoWorldFromContainer_ServerImpl(
	const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation,
	const TArray<UItemInstanceData*>& ItemInstanceData)
{
	FActorSpawnParameters SpawnParams;


	if (RelativeDropLocation.X == 1e+300 && GetOwner()) // special default value
		RelativeDropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance;

	URISSubsystem::Get(this)->SpawnWorldItem(this, FItemBundle(ItemId, Quantity, ItemInstanceData),
	                                         GetOwner()->GetActorLocation() + RelativeDropLocation, DropItemClass);

	UpdateWeightAndSlots();
}

void UItemContainerComponent::ClearServerImpl()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}


	for (auto& Item : ItemsVer.Items)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(Item.ItemId);
		OnItemRemovedFromContainer.Broadcast(ItemData, Item.Quantity, Item.InstanceData,
		                                     EItemChangeReason::ForceDestroyed);

		for (UItemInstanceData* InstanceData : Item.InstanceData)
		{
			if (InstanceData)
			{
				GetOwner()->RemoveReplicatedSubObject(InstanceData);
				InstanceData->ConditionalBeginDestroy();
			}
		}
	}

	ItemsVer.Items.Reset();
	UpdateWeightAndSlots();
	DetectAndPublishChanges();
}

int32 UItemContainerComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity,
                                               TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason,
                                               bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	if (IsClient("DestroyItemImpl"))
		return 0;

	FItemBundle* ContainedItem = FindItemInstanceMutable(ItemId);

	if (!ContainedItem || (!AllowPartial && ContainedItem->Quantity < Quantity))
	{
		UE_LOG(LogRISInventory, Warning, TEXT("Cannot remove item: %s, Instances provided: %d"), *ItemId.ToString(),
		       InstancesToDestroy.Num());
		return 0;
	}

	const int32 QuantityRemoved =
		ContainedItem->DestroyQuantity(Quantity, InstancesToDestroy, GetOwner());
	// Also unregisters instance data as subobject

	if (!ContainedItem->IsValid()) // If the quantity drops to zero or below, remove the item from the inventory
	{
		ensureMsgf(ContainedItem->InstanceData.Num() == 0,
		           TEXT("InstanceData count corrupt, found %u, expected 0"), ContainedItem->InstanceData.Num());
		ItemsVer.Items.RemoveSingle(*ContainedItem);
	}

	if (!SuppressEvents)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		if (InstancesToDestroy.Num() == QuantityRemoved)
			OnItemRemovedFromContainer.Broadcast(ItemData, QuantityRemoved, InstancesToDestroy, Reason);
		else
		{
			// Figure out which instances was destroyed by taking the elements from InstancesToDestroy that are NOT in ContainedItem->InstanceData
			// If an instance InstancesToDestroy never existed in the container then it will still be included here which is unfortunate but a warning will have already been logged
			TArray<UItemInstanceData*> ActualDestroyedInstances;
			for (UItemInstanceData* Instance : ActualDestroyedInstances)
			{
				if (!ContainedItem->InstanceData.Contains(Instance))
				{
					ActualDestroyedInstances.Add(Instance);
				}
			}
			OnItemRemovedFromContainer.Broadcast(ItemData, QuantityRemoved, ActualDestroyedInstances, Reason);
		}
	}

	// Update the current weight of the inventory
	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	// Mark the Items array as dirty to ensure replication
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return QuantityRemoved;
}

int32 UItemContainerComponent::ExtractItem_ServerImpl(const FGameplayTag& ItemId, int32 Quantity,
                                                        const TArray<UItemInstanceData*>& InstancesToExtract,
                                                        EItemChangeReason Reason,
                                                        TArray<UItemInstanceData*>& StateArrayToAppendTo,
                                                        bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	if (IsClient("ExtractItem_ServerImpl")) return 0;

	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (BadItemData(ItemData, ItemId))
		return 0;

	auto* ContainedInstance = FindItemInstanceMutable(ItemId);

	if (!ContainedInstance)
		return 0;

	// Also unregisters instance data as subobject. Ignores quantity if InstancesToExtract is not empty
	int32 ExtractCount = ContainedInstance->Extract(Quantity, InstancesToExtract, StateArrayToAppendTo, GetOwner(),
	                                                AllowPartial);

	if (ExtractCount <= 0) return 0;

	if (!ContainedInstance->IsValid()) // If the quantity drops to zero or below, remove the item from the inventory
	{
		ItemsVer.Items.RemoveSingle(*ContainedInstance);
	}

	if (!SuppressUpdate)
		UpdateWeightAndSlots();

	if (!SuppressEvents)
	{
		if (StateArrayToAppendTo.Num() > 0)
		{
			TArray<UItemInstanceData*> ExtractedInstances;
			for (int32 i = 0; i < ExtractCount; ++i)
				if (UItemInstanceData* InstanceData = StateArrayToAppendTo[StateArrayToAppendTo.Num() - 1 - i])
					ExtractedInstances.Add(InstanceData);

			OnItemRemovedFromContainer.Broadcast(ItemData, ExtractCount, ExtractedInstances, Reason);
		}
		else
		{
			OnItemRemovedFromContainer.Broadcast(ItemData, ExtractCount, NoInstances, Reason);
		}
	}

	// Mark the Items array as dirty to ensure replication
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return ExtractCount;
}

const FItemBundle* UItemContainerComponent::FindItemInstance(const FGameplayTag& ItemId) const
{
	for (auto& Item : ItemsVer.Items)
	{
		if (Item.ItemId == ItemId)
		{
			return &Item;
		}
	}
	
	return nullptr;
}

FItemBundle* UItemContainerComponent::FindItemInstanceMutable(const FGameplayTag& ItemId)
{
	for (auto& Item : ItemsVer.Items)
	{
		if (Item.ItemId == ItemId)
		{
			return &Item;
		}
	}


	return nullptr;
}

void UItemContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);


	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UItemContainerComponent, ItemsVer, SharedParams);
}

void UItemContainerComponent::UpdateWeightAndSlots()
{
	CurrentWeight = 0.0f; // Reset weight
	UsedContainerSlotCount = 0;
	for (const auto& ItemInstanceWithState : ItemsVer.Items)
	{
		if (const UItemStaticData* const ItemData = URISSubsystem::GetItemDataById(ItemInstanceWithState.ItemId))
		{
			int32 SlotsTakenPerStack = 1;
			if (JigsawMode)
			{
				if (JigsawMode)
				{
					SlotsTakenPerStack = ItemData->JigsawSizeX * ItemData->JigsawSizeY;
				}
			}


			UsedContainerSlotCount += FMath::CeilToInt(
				ItemInstanceWithState.Quantity / static_cast<float>(ItemData->MaxStackSize)) * SlotsTakenPerStack;

			CurrentWeight += ItemData->ItemWeight * ItemInstanceWithState.Quantity;
		}
	}

	// We can't ensure here because child class inventory will call this and purposefully violate the constraint temporarily
	// ensureMsgf(UsedContainerSlotCount <= MaxContainerSlotCount, TEXT("Used slot count is higher than max slot count!"));
}

void UItemContainerComponent::RebuildItemsToCache()
{
	CachedItemsVer.Items.Empty();


	// Copy ItemsVer to CachedItemsVer using a memcopy
	CachedItemsVer.Items.Append(ItemsVer.Items);
	CachedItemsVer.Version = ItemsVer.Version;
}

void UItemContainerComponent::DetectAndPublishChanges()
{
	// if (CachedItemsVer.Version == ItemsVer.Version) return;


	// Compare ItemsVer and CachedItemsVer
	for (FItemBundle NewItem : ItemsVer.Items)
	{
		if (FItemBundle* OldItem = CachedItemsVer.Items.FindByPredicate([&NewItem](const FItemBundle& Item)
		{
			return Item.ItemId == NewItem.ItemId;
		}))
		{
			const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);

			// Only perform detailed instance check if the item type actually uses instance data
			if (NewItem.InstanceData.Num() > 0 || OldItem->InstanceData.Num() > 0)
			{
				TArray<UItemInstanceData*> RemovedInstances;
				TArray<UItemInstanceData*> AddedInstances;
				// Find added instances (present in New, not in Old)
				for (UItemInstanceData* NewInstance : NewItem.InstanceData)
				{
					// Check for null just in case, though it shouldn't happen in a clean state
					if (NewInstance && !OldItem->InstanceData.Contains(NewInstance))
					{
						AddedInstances.Add(NewInstance);
					}
				}

				// Find removed instances (present in Old, not in New)
				for (UItemInstanceData* OldInstance : OldItem->InstanceData)
				{
					// Check for null just in case
					if (OldInstance && !NewItem.InstanceData.Contains(OldInstance))
					{
						RemovedInstances.Add(OldInstance);
					}
				}

				// Broadcast with the instances we *did* detect, even if count is wrong. Quantity comes from AddedInstances.Num().
				if (AddedInstances.Num() > 0)
					OnItemAddedToContainer.Broadcast(ItemData, AddedInstances.Num(), AddedInstances,
					                                 EItemChangeReason::Synced);
				if (RemovedInstances.Num() > 0)
					OnItemRemovedFromContainer.Broadcast(ItemData, RemovedInstances.Num(), RemovedInstances,
					                                     EItemChangeReason::Synced);
			}
			else if (OldItem->Quantity != NewItem.Quantity)
			{
				// Item exists, check for quantity change
				if (OldItem->Quantity < NewItem.Quantity)
				{
					OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity - OldItem->Quantity, NoInstances,
					                                 EItemChangeReason::Synced);
				}
				else // if (OldItem->Quantity > NewItem.Quantity)
				{
					OnItemRemovedFromContainer.Broadcast(ItemData, OldItem->Quantity - NewItem.Quantity, NoInstances,
					                                     EItemChangeReason::Synced);
				}
			}

			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -abs(NewItem.Quantity);
		}
		else
		{
			// New item
			const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);
			OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity, NewItem.InstanceData,
			                                 EItemChangeReason::Synced);
			NewItem.Quantity = -NewItem.Quantity; // Mark as processed
			CachedItemsVer.Items.Add(NewItem);
		}
	}

	// Remove unmarked items (those not set to negative) and revert marks for processed items
	for (int32 i = CachedItemsVer.Items.Num() - 1; i >= 0; --i)
	{
		if (CachedItemsVer.Items[i].Quantity >= 0)
		{
			// Item was not processed (not found in Items), so it has been removed
			const auto* ItemData = URISSubsystem::GetItemDataById(CachedItemsVer.Items[i].ItemId);
			OnItemRemovedFromContainer.Broadcast(ItemData, CachedItemsVer.Items[i].Quantity,
			                                     CachedItemsVer.Items[i].InstanceData, EItemChangeReason::Synced);
			CachedItemsVer.Items.RemoveAt(i);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			CachedItemsVer.Items[i].Quantity = -CachedItemsVer.Items[i].Quantity;
		}
	}
}

void UItemContainerComponent::OnRep_Items()
{
	// Recalculate the total weight of the inventory after replication.
	UpdateWeightAndSlots();


	DetectAndPublishChanges();
}

int32 UItemContainerComponent::ReceiveExtractedItems_IfServer(const FGameplayTag& ItemId, int32 Quantiity,
                                                              const TArray<UItemInstanceData*>& ReceivedInstances, bool SuppressEvents)
{
	if (!ItemId.IsValid())
	{
		UE_LOG(LogRISInventory, Warning, TEXT("ReceiveExtractedItems_IfServer: Invalid ItemId provided."));
		return 0;
	}


	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);

	bool InstancesUsed = IsValid(ItemData->DefaultInstanceDataTemplate);

	if (InstancesUsed && ReceivedInstances.Num() == 0)
	{
		UE_LOG(LogRISInventory, Warning, TEXT("ReceiveExtractedItems_IfServer: No instances to receive for item %s."),
		       *ItemId.ToString());
		return 0;
	}

	if (IsClient("ReceiveExtractedItems_IfServer called on non-authority!")) return 0;

	// Check capacity *before* modifying state
	int32 QuantityToReceive = GetReceivableQuantity(ItemData);
	if (ReceivedInstances.Num() > 0)
		QuantityToReceive = FMath::Min((int32)ReceivedInstances.Num(), QuantityToReceive);
	else
		QuantityToReceive = FMath::Min(QuantityToReceive, Quantiity);

	if (QuantityToReceive <= 0)
	{
		UE_LOG(LogRISInventory, Warning,
		       TEXT("ReceiveExtractedItems_IfServer: Cannot receive item %s, container full or invalid."),
		       *ItemId.ToString());
		return 0;
	}

	if (BadItemData(ItemData, ItemId)) return 0;

	FItemBundle* ContainedItem = FindItemInstanceMutable(ItemId);
	bool bCreatedNewBundle = false;
	if (!ContainedItem)
	{
		ItemsVer.Items.Add(FItemBundle(ItemId));
		ContainedItem = &ItemsVer.Items.Last();
		bCreatedNewBundle = true;
	}

	int32 ActuallyReceivedCount = QuantityToReceive;
	if (InstancesUsed)
	{
		ActuallyReceivedCount = 0;
		for (int32 i = 0; i < QuantityToReceive; ++i)
		{
			if (UItemInstanceData* Instance = ReceivedInstances[i])
			{
				ContainedItem->InstanceData.Add(Instance);
				// Re-initialize and register with the *new* owner (this component's owner)
				Instance->Initialize(true, nullptr, this);
				GetOwner()->AddReplicatedSubObject(Instance);
				ActuallyReceivedCount++;
			}
			else
			{
				UE_LOG(LogRISInventory, Warning,
				       TEXT("ReceiveExtractedItems_IfServer: Encountered null instance pointer during receive for %s."),
				       *ItemId.ToString());
			}
		}
	}

	ContainedItem->Quantity += ActuallyReceivedCount;

	if (ActuallyReceivedCount <= 0 && bCreatedNewBundle)
	{
		ItemsVer.Items.Pop(); // Remove empty bundle if nothing was actually added
	}
	else if (ActuallyReceivedCount > 0)
	{
		UpdateWeightAndSlots();
		// Create a sub-array of only the successfully added instances for the broadcast
		TArray<UItemInstanceData*> AddedInstancesForBroadcast;
		if (InstancesUsed)
		{
			AddedInstancesForBroadcast.Reserve(ActuallyReceivedCount);
			for (int32 i = 0; i < ActuallyReceivedCount; ++i) AddedInstancesForBroadcast.Add(ReceivedInstances[i]);
		}

		if (!SuppressEvents)
			OnItemAddedToContainer.Broadcast(ItemData, ActuallyReceivedCount, AddedInstancesForBroadcast,
			                                 EItemChangeReason::Transferred);
		MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);
	}


	ensureMsgf(ContainedItem->InstanceData.IsEmpty() || ContainedItem->Quantity == ContainedItem->InstanceData.Num(),
	           TEXT("ReceiveExtractedItems_IfServer: Instance count mismatch after receive for %s. Qty: %d, Inst: %d"),
	           *ItemId.ToString(), ContainedItem ? ContainedItem->Quantity : -1,
	           ContainedItem ? ContainedItem->InstanceData.Num() : -1);


	return ActuallyReceivedCount;
}


TArray<UItemInstanceData*> FromInstanceIds(const TArray<UItemInstanceData*>& ContainedInstances,
                                           const TArray<int32>& InstanceIds)
{
	TArray<UItemInstanceData*> MatchingInstanceData;
	if (InstanceIds.Num() == 0 || ContainedInstances.IsEmpty()) return MatchingInstanceData;


	MatchingInstanceData.Reserve(InstanceIds.Num());
	for (UItemInstanceData* Instance : ContainedInstances)
	{
		if (Instance && InstanceIds.Contains(Instance->UniqueInstanceId))
		{
			MatchingInstanceData.Add(Instance);
		}
	}
	return MatchingInstanceData;
}


bool UItemContainerComponent::IsClient(const char* FunctionName) const
{
	bool IsClient = GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None;
	if (FunctionName && IsClient)
	{
		FString FuncName(FunctionName);
		UE_LOG(LogRISInventory, Error,
			   TEXT("%s called from non authority"),
			   *FuncName);
	}
		
	// GetOwnerRole() == ROLE_None is for unit tests which we want to treat as server
	return IsClient;
}

bool UItemContainerComponent::BadItemData(const UItemStaticData* ItemData, const FGameplayTag& ItemId)
{
	if (!ItemData)
	{
		if (ItemId.IsValid())
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		}
		else
		{
			UE_LOG(LogRISInventory, Warning, TEXT("Could not find item data for item"));
		}
			
		return true;
	}
		
	return false;
}
