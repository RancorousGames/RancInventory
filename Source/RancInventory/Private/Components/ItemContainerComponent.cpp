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
			//ItemsVer.Items.Add(FItemBundleWithInstanceData(InitialItem.ItemData->ItemId, InitialItem.Quantity));
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
                                                 bool SuppressUpdate)
{
	return AddItem_ServerImpl(ItemSource, ItemId, RequestedQuantity, AllowPartial, SuppressUpdate);
}

int32 UItemContainerComponent::AddItem_ServerImpl(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial = false,
												 bool SuppressUpdate)
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

	FItemBundleWithInstanceData* ContainedItem = FindItemInstance(ItemId);
	bool bCreatedNewBundle = false; // Flag if we created a new entry vs finding existing
	if (!ContainedItem)
	{
		ItemsVer.Items.Add(FItemBundleWithInstanceData(ItemId));
		ContainedItem = &ItemsVer.Items.Last();
		bCreatedNewBundle = true;
	}
	
	const int32 InstanceCountBeforeExtract = ContainedItem->InstanceData.Num();
	AmountToAdd = Execute_ExtractItem_IfServer(ItemSourceObj, ItemId, AmountToAdd, EItemChangeReason::Transferred, ContainedItem->InstanceData);
	
	if (AmountToAdd <= 0)
	{
		if (bCreatedNewBundle) // Remove the bundle we added if nothing went into it
			ItemsVer.Items.RemoveAt(ItemsVer.Items.Num() - 1);

		return 0;
	}
	
	ContainedItem->Quantity += AmountToAdd;
	
	// Verify that instancedata was transferred, otherwise create it
	if (ItemData->ItemInstanceDataClass->IsValidLowLevel())
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
				auto* NewInstanceData = NewObject<UItemInstanceData>(this, ItemData->ItemInstanceDataClass);
				ContainedItem->InstanceData.Add(NewInstanceData);
				NewInstanceData->Initialize(true, nullptr, this);
				GetOwner()->AddReplicatedSubObject(NewInstanceData);
			}
		}
		
		ensureMsgf(ContainedItem->InstanceData.Num() == 0 || ContainedItem->InstanceData.Num() == ContainedItem->Quantity,
		TEXT("InstanceData count corrupt, found %u, expected %u or 0"), ContainedItem->InstanceData.Num(), ContainedItem->Quantity);
	}
	
	if (!SuppressUpdate)
	{
		UpdateWeightAndSlots();
		OnItemAddedToContainer.Broadcast(ItemData, AmountToAdd, EItemChangeReason::Added);
	}

	if (GetOwnerRole() == ROLE_Authority || GetOwnerRole() == ROLE_None)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);
	}

	return AmountToAdd; // Return the actual quantity added
}


int32 UItemContainerComponent::DestroyItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial)
{
	return DestroyItemImpl(ItemId, Quantity, Reason, AllowPartial, true);
}

int32 UItemContainerComponent::DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial, bool UpdateAfter, bool SendEventAfter)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	FItemBundleWithInstanceData* ContainedItem = FindItemInstance(ItemId);

	if (!ContainedItem || (!AllowPartial && ContainedItem->Quantity < Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove item: %s"), *ItemId.ToString());
		return 0;
	}

	const int32 QuantityRemoved = FMath::Min(ContainedItem->Quantity, Quantity);

	
	ContainedItem->DestroyQuantity(QuantityRemoved, GetOwner()); // Also unregisters instance data as subobject

	if (!ContainedItem->IsValid()) // If the quantity drops to zero or below, remove the item from the inventory
	{
		ItemsVer.Items.RemoveSingle(*ContainedItem);
	}

	// Update the current weight of the inventory
	if (UpdateAfter)
		UpdateWeightAndSlots();

	if (SendEventAfter)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
		OnItemRemovedFromContainer.Broadcast(ItemData, QuantityRemoved, Reason);
	}

	// Mark the Items array as dirty to ensure replication
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return QuantityRemoved;
}

int32 UItemContainerComponent::DropItems(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation)
{
	if (GetContainerOnlyItemQuantity(ItemId) < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot drop item: %s"), *ItemId.ToString());
		return 0;
	}

	if (GetOwnerRole() != ROLE_Authority)
		RequestedOperationsToServer.Add(FRISExpectedOperation(Remove, ItemId, Quantity));

	DropItemFromContainer_Server(ItemId, Quantity, RelativeDropLocation);

	// On client the below is just a guess

	return Quantity;
}


void UItemContainerComponent::DropItemFromContainer_Server_Implementation(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation)
{
	if (FindItemById(ItemId).Quantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot drop item: %s"), *ItemId.ToString());
		return;
	}
	
	TArray<UItemInstanceData*> DroppedItemStateArray;
	ExtractItem_IfServer(ItemId, Quantity, EItemChangeReason::Dropped, DroppedItemStateArray);
	SpawnItemIntoWorldFromContainer_ServerImpl(ItemId, Quantity, RelativeDropLocation, DroppedItemStateArray);
}


void UItemContainerComponent::SpawnItemIntoWorldFromContainer_ServerImpl(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation, TArray<UItemInstanceData*> ItemInstanceData)
{
	FActorSpawnParameters SpawnParams;

	if (RelativeDropLocation.X == 1e+300 && GetOwner()) // special default value
		RelativeDropLocation = 	GetOwner()->GetActorForwardVector() * DefaultDropDistance;

	URISSubsystem::Get(this)->SpawnWorldItem(this, FItemBundleWithInstanceData(ItemId, Quantity,ItemInstanceData), GetOwner()->GetActorLocation() + RelativeDropLocation, DropItemClass);
		
	UpdateWeightAndSlots();
}


int32 UItemContainerComponent::UseItem(const FGameplayTag& ItemId)
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
	
	UseItem_Server(ItemId);

	// On client the below is just a guess
	int32 QuantityToRemove = UsableItem->QuantityPerUse;

	return QuantityToRemove;
}

void UItemContainerComponent::UseItem_Server_Implementation(const FGameplayTag& ItemId)
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

	const int32 ActualQuantity = DestroyItem_IfServer(ItemId, UsableItem->QuantityPerUse, EItemChangeReason::Consumed, false);
	if (ActualQuantity > 0 || UsableItem->QuantityPerUse == 0)
	{
		UsableItem->Use(GetOwner());
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
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	// drop with incrementing angle
	int32 DroppedCount = 0;
	const float AngleStep = 360.f / ItemsVer.Items.Num();

	while (ItemsVer.Items.Num() > 0)
	{
		const FItemBundleWithInstanceData& NextToDrop = ItemsVer.Items.Last();
		FVector DropLocation = GetOwner()->GetActorForwardVector() * DefaultDropDistance + FVector(FMath::FRand() * 100, FMath::FRand() * 100 , 100);
		DropItemFromContainer_Server(NextToDrop.ItemId, NextToDrop.Quantity, DropLocation);
		DroppedCount++;
	}

	return DroppedCount;
}

int32 UItemContainerComponent::ExtractItemFromContainer_IfServer(const FGameplayTag& ItemId, int32 Quantity, UItemContainerComponent* ContainerToExtractFrom, bool AllowPartial)
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

	int32 ExtractableQuantity = ContainerToExtractFrom->GetContainedQuantity(ItemId);
	if (!AllowPartial && ExtractableQuantity < Quantity) return 0;
	
	FItemBundleWithInstanceData* ItemInstance = FindItemInstance(ItemId);

	if (!ItemInstance)
	{
		ItemsVer.Items.Add(FItemBundleWithInstanceData(ItemId, 0));
		ItemInstance = &ItemsVer.Items.Last();
	}
	
	int32 QuantityExtracted = ContainerToExtractFrom->ExtractItemImpl_IfServer(ItemId, Quantity, EItemChangeReason::Transferred, ItemInstance->InstanceData, false);
	for (int i = 0; i < ItemInstance->InstanceData.Num(); ++i)
	{
		if (UItemInstanceData* InstanceData = ItemInstance->InstanceData[i])
		{
			InstanceData->Initialize(true, nullptr, this);
			GetOwner()->AddReplicatedSubObject(InstanceData);
		}
	}
	
	ItemInstance->Quantity += QuantityExtracted;
	
	UpdateWeightAndSlots();
	OnItemAddedToContainer.Broadcast(URISSubsystem::GetItemDataById(ItemId), QuantityExtracted, EItemChangeReason::Transferred);
	MARK_PROPERTY_DIRTY_FROM_NAME(UItemContainerComponent, ItemsVer, this);

	return Quantity;
}

int32 UItemContainerComponent::ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("Extract called on non-authority!"));
		return 0;
	}

	return ExtractItemImpl_IfServer(ItemId, Quantity, Reason, StateArrayToAppendTo, false);
}

int32 UItemContainerComponent::ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool SuppressUpdate)
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
		return 0; // Item data not found
	}

	auto* ContainedInstance = FindItemInstance(ItemId);

	if (!ContainedInstance || ContainedInstance->Quantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot extract item: %s"), *ItemId.ToString());
		return 0;
	}

	int32 ExtractCount = ContainedInstance->ExtractQuantity(Quantity, StateArrayToAppendTo, GetOwner()); // Also unregisters instance data as subobject

	if (!ContainedInstance->IsValid()) // If the quantity drops to zero or below, remove the item from the inventory
	{
		ItemsVer.Items.RemoveSingle(*ContainedInstance);
	}

	if (!SuppressUpdate)
	{
		UpdateWeightAndSlots();
		OnItemRemovedFromContainer.Broadcast(ItemData, ExtractCount, Reason);
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

const FItemBundleWithInstanceData& UItemContainerComponent::FindItemById(const FGameplayTag& ItemId) const
{
    for (const auto& Item : ItemsVer.Items)
    {
        if (Item.ItemId == ItemId)
        {
            return Item;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Item with ID %s not found."), *ItemId.ToString());
    
    static const FItemBundleWithInstanceData EmptyItem;
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
	const int32 ContainedQuantity = GetContainerOnlyItemQuantity(ItemData->ItemId);
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
	return ContainsImpl(ItemId, Quantity);
}

bool UItemContainerComponent::ContainsImpl(const FGameplayTag& ItemId, int32 Quantity) const
{
	return GetContainerOnlyItemQuantity(ItemId) >= Quantity;
}

int32 UItemContainerComponent::GetContainerOnlyItemQuantity(const FGameplayTag& ItemId) const
{
	return GetContainerOnlyItemQuantityImpl(ItemId);
}

int32 UItemContainerComponent::GetContainerOnlyItemQuantityImpl(const FGameplayTag& ItemId) const
{
	return  const_cast<UItemContainerComponent*>(this)->GetContainedQuantity_Implementation(ItemId);
}


int32 UItemContainerComponent::GetContainedQuantity_Implementation(const FGameplayTag& ItemId)
{
	auto* ContainedInstance = FindItemInstance(ItemId);

	if (!ContainedInstance)
	{
		return 0;
	}

	return ContainedInstance->Quantity;
}


TArray<UItemInstanceData*> UItemContainerComponent::GetItemState(const FGameplayTag& ItemId)
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData;
	}

	return TArray<UItemInstanceData*>();
}

UItemInstanceData* UItemContainerComponent::GetSingleItemState(const FGameplayTag& ItemId)
{
	if (auto* Instance = FindItemInstance(ItemId))
	{
		return Instance->InstanceData.Num() > 0 ? Instance->InstanceData[0] : nullptr;
	}

	return nullptr;
}

TArray<FItemBundleWithInstanceData> UItemContainerComponent::GetAllContainerItems() const
{
	return ItemsVer.Items;
}

bool UItemContainerComponent::IsEmpty() const
{
	return ItemsVer.Items.Num() == 0;
}

void UItemContainerComponent::Clear_IfServer()
{
	ClearImpl();
}

void UItemContainerComponent::ClearImpl()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}

	for (auto& Item : ItemsVer.Items)
	{
		const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(Item.ItemId);
		OnItemRemovedFromContainer.Broadcast(ItemData, Item.Quantity, EItemChangeReason::ForceDestroyed);

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

FItemBundleWithInstanceData* UItemContainerComponent::FindItemInstance(const FGameplayTag& ItemId)
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
	for (FItemBundleWithInstanceData NewItem : ItemsVer.Items)
	{
		if (FItemBundleWithInstanceData* OldItem = CachedItemsVer.Items.FindByPredicate([&NewItem](const FItemBundleWithInstanceData& Item)
		{
			return Item.ItemId == NewItem.ItemId;
		}))
		{
			// Item exists, check for quantity change
			if (OldItem->Quantity != NewItem.Quantity)
			{
				const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);
				if (OldItem->Quantity < NewItem.Quantity)
				{
					OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity - OldItem->Quantity, EItemChangeReason::Synced);
				}
				else if (OldItem->Quantity > NewItem.Quantity)
				{
					OnItemRemovedFromContainer.Broadcast(ItemData, OldItem->Quantity - NewItem.Quantity, EItemChangeReason::Synced);
				}
			}
			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -abs(NewItem.Quantity);
		}
		else
		{
			// New item
			const auto* ItemData = URISSubsystem::GetItemDataById(NewItem.ItemId);
			OnItemAddedToContainer.Broadcast(ItemData, NewItem.Quantity, EItemChangeReason::Synced);
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
			OnItemRemovedFromContainer.Broadcast(ItemData, CachedItemsVer.Items[i].Quantity, EItemChangeReason::Synced);
			CachedItemsVer.Items.RemoveAt(i);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			CachedItemsVer.Items[i].Quantity = -CachedItemsVer.Items[i].Quantity;
		}
	}
}