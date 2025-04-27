// Copyright Rancorous Games, 2024

#include "Components\ItemContainerComponent.h"

#include "GameplayTagsManager.h"
#include "LogRancInventorySystem.h"
#include "Data/ItemInstanceData.h"
#include "Core/RISFunctions.h"
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

void UItemContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UItemContainerComponent, ItemsVer, SharedParams);
}

void UItemContainerComponent::OnRep_Items()
{
	// Recalculate the total weight of the inventory after replication.
	UpdateWeightAndSlots();

	DetectAndPublishChanges();
}

int32 UItemContainerComponent::AddItem_IfServer(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial,
                                                 bool SuppressEvents, bool SuppressUpdate)
{
	return AddItem_ServerImpl(ItemSource, ItemId, RequestedQuantity, AllowPartial, SuppressEvents, SuppressUpdate);
}

int32 UItemContainerComponent::AddItem_ServerImpl(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial = false,
												 bool SuppressEvents, bool SuppressUpdate)
{
	

	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddItems called on non-authority!"));
		return 0;
	}

	UObject* ItemSourceObj = ItemSource.GetObjectRef();
	if (!ItemSourceObj)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item source is null!"));
		return 0;
	}

	// Check if the inventory can receive the item and calculate the acceptable quantity. Do NOT call overriden versions here
	const int32 AcceptableQuantity = GetReceivableQuantityImpl(ItemId);

	if (AcceptableQuantity <= 0 || (!AllowPartial && AcceptableQuantity < RequestedQuantity))
	{
		return 0; // No items added
	}


	// get item data
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}


	int32 AmountToAdd = FMath::Min(AcceptableQuantity, RequestedQuantity);

	FItemBundle* ContainedItem = FindItemInstanceMutable(ItemId);
	bool bCreatedNewBundle = false; // Flag if we created a new entry vs finding existing
	if (!ContainedItem)
	{
		ItemsVer.Items.Add(FItemBundle(ItemId));
		ContainedItem = &ItemsVer.Items.Last();
		bCreatedNewBundle = true;
	}
	
	const int32 InstanceCountBeforeExtract = ContainedItem->InstanceData.Num();
	AmountToAdd = Execute_ExtractItem_IfServer(ItemSourceObj, ItemId, AmountToAdd, NoInstances, EItemChangeReason::Transferred, ContainedItem->InstanceData);
	
	if (AmountToAdd <= 0)
	{
		if (bCreatedNewBundle) // Remove the bundle we added if nothing went into it
			ItemsVer.Items.RemoveAt(ItemsVer.Items.Num() - 1);

		return 0;
	}
	
	ContainedItem->Quantity += AmountToAdd;
	
	// Verify that instancedata was transferred, otherwise create it
	if (IsValid(ItemData->DefaultInstanceDataTemplate))
	{
		const int32 InstanceCountAfterExtract = ContainedItem->InstanceData.Num();
		if (InstanceCountAfterExtract > InstanceCountBeforeExtract)
		{
			for (int32 i = InstanceCountBeforeExtract; i < InstanceCountAfterExtract; ++i)
			{
				if (UItemInstanceData* ExtractedInstanceData = ContainedItem->InstanceData[i])
				{
					ExtractedInstanceData->Initialize(true, nullptr, this);
					GetOwner()->AddReplicatedSubObject(ExtractedInstanceData);
				}
			}
		}

		if (ContainedItem->Quantity != ContainedItem->InstanceData.Num())
		{
			// Instance data was not extracted so create it
			for (int i = 0; i < AmountToAdd; ++i)
			{
				UItemInstanceData* NewInstanceData = DuplicateObject<UItemInstanceData>(ItemData->DefaultInstanceDataTemplate, this);
				ContainedItem->InstanceData.Add(NewInstanceData);
				NewInstanceData->Initialize(true, nullptr, this);
				GetOwner()->AddReplicatedSubObject(NewInstanceData);
			}
		}
		
		ensureMsgf(ContainedItem->InstanceData.Num() == 0 || ContainedItem->InstanceData.Num() == ContainedItem->Quantity,
		TEXT("InstanceData count corrupt, found %u, expected %u or 0"), ContainedItem->InstanceData.Num(), ContainedItem->Quantity);
	}
	
	if (!SuppressUpdate)
		UpdateWeightAndSlots();
	if (!SuppressEvents)
		OnItemAddedToContainer.Broadcast(ItemData, AmountToAdd, ContainedItem->InstanceData, EItemChangeReason::Added);

	if (GetOwnerRole() == ROLE_Authority || GetOwnerRole() == ROLE_None)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);
	}

	return AmountToAdd; // Return the actual quantity added
}


int32 UItemContainerComponent::DestroyItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason, bool AllowPartial)
{
	return DestroyItemImpl(ItemId, Quantity, InstancesToDestroy, Reason, AllowPartial, false, false);
}

int32 UItemContainerComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason, bool AllowPartial, bool SuppressEvents, bool SuppressUpdate)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	FItemBundle* ContainedItem = FindItemInstanceMutable(ItemId);

	if (!ContainedItem || (!AllowPartial && ContainedItem->Quantity < Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove item: %s, Instances provided: %d"), *ItemId.ToString(), InstancesToDestroy.Num());
		return 0;
	}

	const int32 QuantityRemoved =
		ContainedItem->DestroyQuantity(Quantity, InstancesToDestroy, GetOwner()); // Also unregisters instance data as subobject

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

int32 UItemContainerComponent::DropItems(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDrop, FVector RelativeDropLocation)
{
	if (GetQuantityTotal(ItemId) < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot drop item: %s"), *ItemId.ToString());
		return 0;
	}

	if (GetOwnerRole() != ROLE_Authority)
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, Quantity));

	DropItemFromContainer_Server(ItemId, Quantity, FItemBundle::ToInstanceIds(InstancesToDrop), RelativeDropLocation);

	// On client the below is just a guess

	return Quantity;
}


void UItemContainerComponent::DropItemFromContainer_Server_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<int32>& InstanceIdsToDrop, FVector RelativeDropLocation)
{
	FItemBundle* Item = FindItemInstanceMutable(ItemId);
	DropItemFromContainer_ServerImpl(Item, Quantity, Item->FromInstanceIds(InstanceIdsToDrop), RelativeDropLocation);
}

void UItemContainerComponent::DropItemFromContainer_ServerImpl(FItemBundle* Item, int32 Quantity,
	const TArray<UItemInstanceData*>& InstancesToDrop, FVector RelativeDropLocation)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("DropItemFromContainer_ServerImpl called on non-authority!"));
		return;
	}
	
	if (!Item || Item->Quantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot drop item"));
		return;
	}

	const auto& ItemId = Item->ItemId;
	
	TArray<UItemInstanceData*> DroppedItemInstancesArray = TArray<UItemInstanceData*>();
	
	ExtractItem_IfServer(ItemId, Quantity, InstancesToDrop, EItemChangeReason::Dropped, DroppedItemInstancesArray);
	
	SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, Quantity, RelativeDropLocation, DroppedItemInstancesArray);
}

void UItemContainerComponent::SpawnItemIntoWorldFromContainer_ServerImpl(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation, TArray<UItemInstanceData*> ItemInstanceData)
{
	FActorSpawnParameters SpawnParams;

	if (RelativeDropLocation.X == 1e+300 && GetOwner()) // special default value
		RelativeDropLocation = 	GetOwner()->GetActorForwardVector() * DefaultDropDistance;

	URISSubsystem::Get(this)->SpawnWorldItem(this, FItemBundle(ItemId, Quantity,ItemInstanceData), GetOwner()->GetActorLocation() + RelativeDropLocation, DropItemClass);
		
	UpdateWeightAndSlots();
}


int32 UItemContainerComponent::UseItem(const FGameplayTag& ItemId, int32 ItemToUseInstanceId)
{
	if (GetOwnerRole() != ROLE_Authority)
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, 1));

	const auto* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0;
	}

	const UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

	if (!UsableItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return 0;
	}
	
	UseItem_Server(ItemId, ItemToUseInstanceId);

	// On client the below is just a guess
	int32 QuantityToRemove = UsableItem->QuantityPerUse;

	return QuantityToRemove;
}

void UItemContainerComponent::UseItem_Server_Implementation(const FGameplayTag& ItemId, int32 ItemToUseInstanceId)
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return;
	}
	
	UUsableItemDefinition* UsableItem = ItemData->GetItemDefinition<UUsableItemDefinition>();

	if (!UsableItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not usable: %s"), *ItemId.ToString());
		return;
	}

	UItemInstanceData* UsedInstance = nullptr;
	TArray<UItemInstanceData*> InstancesToUse;
	if (ItemToUseInstanceId >= 0)
	{
		auto AllInstanceData =  GetItemInstanceData(ItemId);
		for (int i = 0; i < AllInstanceData.Num(); ++i)
		{
			if (AllInstanceData[i]->UniqueInstanceId == ItemToUseInstanceId)
			{
				InstancesToUse.Add(AllInstanceData[i]);
				UsedInstance = AllInstanceData[i];
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
			const int32 DestroyedQuantity = DestroyItem_IfServer(ItemId, UsableItem->QuantityPerUse, InstancesToUse, EItemChangeReason::Consumed, false);
			ensureMsgf(DestroyedQuantity == UsableItem->QuantityPerUse,
				TEXT("Used item %s but destroyed %d, expected %d"), *ItemId.ToString(), DestroyedQuantity, UsableItem->QuantityPerUse);
		}
	}
}

int32 UItemContainerComponent::DropAllItems_IfServer()
{
	return DropAllItems_ServerImpl();
}

int32 UItemContainerComponent::DropAllItems_ServerImpl()
{
    if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
    {
        UE_LOG(LogRISInventory, Warning, TEXT("DropAllItems_ServerImpl called on non-authority!"));
        return 0;
    }

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
            UE_LOG(LogRISInventory, Error, TEXT("DropAllItems_ServerImpl: ItemId %s is invalid or ItemData not found."), *ItemIdToProcess.ToString());
            ItemsVer.Items.Pop();
            continue;
        }
    	
        if (CurrentQuantityOfItem <= 0) {
        	ItemsVer.Items.Pop();
             continue;
        }

        int32 QuantityForThisStack = FMath::Min(CurrentQuantityOfItem, ItemData->MaxStackSize);

        FVector Direction = FVector::ForwardVector.RotateAngleAxis(CurrentAngle, FVector::UpVector);
        FVector Offset = Direction * DefaultDropDistance;
        Offset += FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(0.f, 50.f));
        FVector DropLocation = OwnerActor->GetActorLocation() + OwnerActor->GetActorRotation().RotateVector(Offset); // Ensure offset respects actor rotation
		TArray<UItemInstanceData*> ItemInstancesToDrop;
    	// Grab QuantityForThisStack instances from the end of ItemToProcess.InstanceData
    	if (ItemToProcess.InstanceData.Num() > 0)
			for (int32 i = ItemToProcess.InstanceData.Num() - 1; i >= 0 && ItemInstancesToDrop.Num() < QuantityForThisStack; --i)
				if (UItemInstanceData* InstanceData = ItemToProcess.InstanceData[i])
					ItemInstancesToDrop.Add(InstanceData);
    	
        DropItemFromContainer_ServerImpl(&ItemToProcess, QuantityForThisStack, ItemInstancesToDrop, DropLocation);

        DroppedStacksCount++;
        CurrentAngle += AngleStep;
    }

    UpdateWeightAndSlots(); 
    MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

    return DroppedStacksCount;
}


int32 UItemContainerComponent::ExtractItemFromOtherContainer_IfServer(UItemContainerComponent* ContainerToExtractFrom, const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToExtract, bool AllowPartial)
{		
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}
	
	if (!ContainerToExtractFrom)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractItemFromContainer called with null container!"));
		return 0;
	}

	int32 ExtractableQuantity = ContainerToExtractFrom->GetQuantityTotal(ItemId);
	if (!AllowPartial && ExtractableQuantity < Quantity) return 0;
	
	FItemBundle* LocalItemInstance = FindItemInstanceMutable(ItemId);
	if (!LocalItemInstance)
	{
		ItemsVer.Items.Add(FItemBundle(ItemId, 0));
		LocalItemInstance = &ItemsVer.Items.Last();
	}

	TArray<UItemInstanceData*> ExtractedInstances;
	// Does not allow partial
	int32 QuantityExtracted = ContainerToExtractFrom->ExtractItemImpl_IfServer(ItemId, Quantity, InstancesToExtract, EItemChangeReason::Transferred, ExtractedInstances, false);
	for (int i = 0; i < ExtractedInstances.Num(); ++i)
	{
		if (UItemInstanceData* InstanceData = ExtractedInstances[i])
		{
			InstanceData->Initialize(true, nullptr, this);
			GetOwner()->AddReplicatedSubObject(InstanceData);
			LocalItemInstance->InstanceData.Add(InstanceData);
		}
	}
	
	LocalItemInstance->Quantity += QuantityExtracted;
	
	UpdateWeightAndSlots();
	OnItemAddedToContainer.Broadcast(URISSubsystem::GetItemDataById(ItemId), QuantityExtracted, ExtractedInstances, EItemChangeReason::Transferred);
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return Quantity;
}

int32 UItemContainerComponent::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract,  EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Extract called on non-authority!"));
		return 0;
	}

	return ExtractItemImpl_IfServer(ItemId, Quantity, InstancesToExtract, Reason, StateArrayToAppendTo, false);
}

int32 UItemContainerComponent::ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool SuppressEvents, bool SuppressUpdate)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}
	
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0;
	}

	auto* ContainedInstance = FindItemInstanceMutable(ItemId);

	if (!ContainedInstance)
		return 0;
	
	// Also unregisters instance data as subobject. Ignores quantity if InstancesToExtract is not empty
	int32 ExtractCount = ContainedInstance->Extract(Quantity, InstancesToExtract, StateArrayToAppendTo, GetOwner());

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

float UItemContainerComponent::GetCurrentWeight() const
{
	return CurrentWeight;
}

float UItemContainerComponent::GetMaxWeight() const
{
	return MaxWeight <= 0.f ? MAX_flt : MaxWeight;
}

const FItemBundle& UItemContainerComponent::FindItemById(const FGameplayTag& ItemId) const
{
    for (const auto& Item : ItemsVer.Items)
    {
        if (Item.ItemId == ItemId)
        {
            return Item;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Item with ID %s not found."), *ItemId.ToString());
    
    static const FItemBundle EmptyItem;
    return EmptyItem;
}

bool UItemContainerComponent::CanContainerReceiveItems(const FGameplayTag& ItemId, int32 Quantity) const
{
	return (OnValidateAddItemToContainer.IsBound() ? OnValidateAddItemToContainer.Execute(ItemId, Quantity) : true) &&
		GetReceivableQuantity(ItemId) >= Quantity;
}

int32 UItemContainerComponent::GetReceivableQuantity(const FGameplayTag& ItemId) const
{
	return GetReceivableQuantityImpl(ItemId);
}

int32 UItemContainerComponent::GetReceivableQuantityImpl(const FGameplayTag& ItemId) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	const int32 AcceptableQuantityByWeight = GetQuantityContainerCanReceiveByWeight(ItemData);

	int32 FinalAcceptableQuantity = FMath::Min(GetQuantityContainerCanReceiveBySlots(ItemData), AcceptableQuantityByWeight);

	if (OnValidateAddItemToContainer.IsBound())
		FinalAcceptableQuantity = FMath::Min(FinalAcceptableQuantity, OnValidateAddItemToContainer.Execute(ItemId, 5));

	return FinalAcceptableQuantity;
}

int32 UItemContainerComponent::GetQuantityContainerCanReceiveBySlots(const UItemStaticData* ItemData) const
{
	const int32 ContainedQuantity = GetQuantityTotal(ItemData->ItemId);
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
	const int32 AcceptableQuantityBySlotCount = (AvailableSlots / SlotsTakenPerStack) * ItemData->MaxStackSize + ItemQuantityTillNextFullSlot;

	return AcceptableQuantityBySlotCount;
}


int32 UItemContainerComponent::GetQuantityContainerCanReceiveByWeight(const UItemStaticData* ItemData) const
{
	// Calculate how many items can be added without exceeding the max weight
	if (ItemData->ItemWeight <= 0) return INT32_MAX;
	
	int32 AcceptableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	AcceptableQuantityByWeight = AcceptableQuantityByWeight > 0 ? AcceptableQuantityByWeight : 0;

	return AcceptableQuantityByWeight;
}

// check just weight

bool UItemContainerComponent::HasWeightCapacityForItems(const FGameplayTag& ItemId, int32 Quantity) const
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return false;
	}

	const int32 AcceptableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	return AcceptableQuantityByWeight >= Quantity;
}

bool UItemContainerComponent::Contains(const FGameplayTag& ItemId, int32 Quantity) const
{
	return ContainsImpl(ItemId, Quantity, NoInstances);
}

bool UItemContainerComponent::ContainsInstances(const FGameplayTag& ItemId, int32 Quantity,
                                                const TArray<UItemInstanceData*>& InstancesToLookFor) const
{
	return ContainsImpl(ItemId, Quantity, InstancesToLookFor);
}

bool UItemContainerComponent::ContainsImpl(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToLookFor) const
{
	auto ContainedInstance = FindItemInstance(ItemId);
	return ContainedInstance && ContainedInstance->Contains(Quantity, InstancesToLookFor);
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
    if (!ItemData)
    {
         UE_LOG(LogRISInventory, Warning, TEXT("ContainsByPredicate: Could not find ItemStaticData for %s"), *ItemId.ToString());
         return false; // Cannot proceed without item data
    }

    if (!ItemData->DefaultInstanceDataTemplate) // Check if the Class pointer itself is null/invalid
    {
        UE_LOG(LogRISInventory, Verbose, TEXT("ContainsByPredicate: Item %s does not use Instance Data. Checking total quantity."), *ItemId.ToString());
        return false;
    }

    if (FoundItemBundle->Quantity != FoundItemBundle->InstanceData.Num() && FoundItemBundle->InstanceData.Num() > 0) {
         // Another inconsistency: Quantity doesn't match instance count.
         UE_LOG(LogRISInventory, Warning, TEXT("ContainsByPredicate: Item %s Quantity (%d) does not match InstanceData count (%d). Predicate check might be unreliable."),
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
        else {
             UE_LOG(LogRISInventory, Warning, TEXT("ContainsByPredicate: Found null pointer in InstanceData array for item %s."), *ItemId.ToString());
        }
    }

    return SatisfyingCount >= Quantity;
}

int32 UItemContainerComponent::GetQuantityTotal(const FGameplayTag& ItemId) const
{
	auto* ContainedInstance = FindItemInstance(ItemId);

	if (!ContainedInstance)
	{
		return 0;
	}

	return ContainedInstance->Quantity;
}


TArray<UItemInstanceData*> UItemContainerComponent::GetItemInstanceData(const FGameplayTag& ItemId)
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData;
	}

	return TArray<UItemInstanceData*>();
}

UItemInstanceData* UItemContainerComponent::GetSingleItemInstanceData(const FGameplayTag& ItemId)
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData.Num() > 0 ? Instance->InstanceData[0] : nullptr;
	}

	return nullptr;
}

TArray<FItemBundle> UItemContainerComponent::GetAllItems() const
{
	return ItemsVer.Items;
}

bool UItemContainerComponent::IsEmpty() const
{
	return ItemsVer.Items.Num() == 0;
}

void UItemContainerComponent::Clear_IfServer()
{
	ClearServerImpl();
}

void UItemContainerComponent::ClearServerImpl()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}

	for (auto& Item : ItemsVer.Items)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(Item.ItemId);
		OnItemRemovedFromContainer.Broadcast(ItemData, Item.Quantity, Item.InstanceData, EItemChangeReason::ForceDestroyed);

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

void UItemContainerComponent::SetAddItemValidationCallback_IfServer(const FAddItemValidationDelegate& ValidationDelegate)
{
	OnValidateAddItemToContainer = ValidationDelegate;
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

			UsedContainerSlotCount += FMath::CeilToInt(ItemInstanceWithState.Quantity / static_cast<float>(ItemData->MaxStackSize)) * SlotsTakenPerStack;

			CurrentWeight += ItemData->ItemWeight * ItemInstanceWithState.Quantity;
		}
	}

	// We can't ensure here because child class inventory will call this and purposefully violate the constraint temporarily
	// ensureMsgf(UsedContainerSlotCount <= MaxContainerSlotCount, TEXT("Used slot count is higher than max slot count!"));
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

void UItemContainerComponent::RebuildItemsToCache()
{
	CachedItemsVer.Items.Empty();

	// Copy ItemsVer to CachedItemsVer using a memcopy
	CachedItemsVer.Items.Append(ItemsVer.Items);
	CachedItemsVer.Version = ItemsVer.Version;
}

void UItemContainerComponent::DetectAndPublishChanges()
{
	// if (CachedItemsVer.Version == ItemsVer.Version)	return;

	// Compare ItemsVer and CachedItemsVer
	for (FItemBundle NewItem : ItemsVer.Items)
	{
		if (FItemBundle* OldItem = CachedItemsVer.Items.FindByPredicate([&NewItem](const FItemBundle& Item)
		{
			return Item.ItemId == NewItem.ItemId;
		}))
		{
			const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);
			TArray<UItemInstanceData*> AddedInstances;
			TArray<UItemInstanceData*> RemovedInstances;

			// Only perform detailed instance check if the item type actually uses instance data
			if (NewItem.InstanceData.Num() > 0 || OldItem->InstanceData.Num() > 0)
			{
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
					OnItemAddedToContainer.Broadcast(ItemData, AddedInstances.Num(), AddedInstances, EItemChangeReason::Synced);
				if (RemovedInstances.Num() > 0)
					OnItemRemovedFromContainer.Broadcast(ItemData, RemovedInstances.Num(), RemovedInstances, EItemChangeReason::Synced);
			}
			else if (OldItem->Quantity != NewItem.Quantity)
			{
				// Item exists, check for quantity change
				if (OldItem->Quantity < NewItem.Quantity)
				{
					OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity - OldItem->Quantity, NoInstances, EItemChangeReason::Synced);
				}
				else // if (OldItem->Quantity > NewItem.Quantity)
				{
					OnItemRemovedFromContainer.Broadcast(ItemData, OldItem->Quantity - NewItem.Quantity, NoInstances, EItemChangeReason::Synced);
				}
			}
						
			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -abs(NewItem.Quantity);
		}
		else
		{
			// New item
			const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);
			OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity, NewItem.InstanceData, EItemChangeReason::Synced);
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
			OnItemRemovedFromContainer.Broadcast(ItemData, CachedItemsVer.Items[i].Quantity, CachedItemsVer.Items[i].InstanceData, EItemChangeReason::Synced);
			CachedItemsVer.Items.RemoveAt(i);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			CachedItemsVer.Items[i].Quantity = -CachedItemsVer.Items[i].Quantity;
		}
	}
}