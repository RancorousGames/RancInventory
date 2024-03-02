// Copyright Rancorous Games, 2024

#include "..\..\Public\Components\RISItemContainerComponent.h"
#include "..\..\Public\Management\RISInventoryFunctions.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

URISItemContainerComponent::URISItemContainerComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer), MaxWeight(0.f), MaxContainerSlotCount(MAX_int32), CurrentWeight(0.f)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void URISItemContainerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Merge initial items based on item ids
	for (int i = InitialItems.Num() - 1; i >= 0; --i)
	{
		for (int j = i - 1; j >= 0; --j)
		{
			if (InitialItems[i].ItemId == InitialItems[j].ItemId)
			{
				InitialItems[j].Quantity += InitialItems[i].Quantity;
				InitialItems.RemoveAt(i);
				break;
			}
		}
	}
	
	// add all initial items to items
	for (const FRancInitialItem& InitialItem : InitialItems)
	{
		const URISItemData* Data = URISInventoryFunctions::GetSingleItemDataById(InitialItem.ItemId, {}, false);

		if (Data && Data->ItemId.IsValid())
		{
			auto ItemInstance = FRISItemInstance(Data->ItemId, InitialItem.Quantity);
			ItemsVer.Items.Add(ItemInstance);
		}
	}
	UpdateWeightAndSlots();
	CopyItemsToCache();

	if (DropItemClass == nullptr)
	{
		DropItemClass = ARISWorldItem::StaticClass();
	}
}

void URISItemContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(URISItemContainerComponent, ItemsVer, SharedParams);
}


void URISItemContainerComponent::OnRep_Items()
{
	// Recalculate the total weight of the inventory after replication.
	UpdateWeightAndSlots();

	DetectAndPublishChanges();
}

int32 URISItemContainerComponent::AddItems_IfServer(const FRISItemInstance& ItemInstance, bool AllowPartial)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None) // none needed for tests
	{
		UE_LOG(LogTemp, Warning, TEXT("AddItems called on non-authority!"));
		return 0;
	}

	// Check if the inventory can receive the item and calculate the acceptable quantity
	const int32 AcceptableQuantity = GetQuantityOfItemContainerCanReceive(ItemInstance.ItemId);

	if (AcceptableQuantity <= 0 || (!AllowPartial && AcceptableQuantity < ItemInstance.Quantity))
	{
		return 0; // No items added
	}

	const int32 AmountToAdd = FMath::Min(AcceptableQuantity, ItemInstance.Quantity);
	for (auto& ExistingItem : ItemsVer.Items)
	{
		// If item exists, increase the quantity up to the acceptable amount
		if (ExistingItem.ItemId == ItemInstance.ItemId)
		{
			ExistingItem.Quantity += AmountToAdd;
			goto Finish;
		}
	}

	ItemsVer.Items.Add(FRISItemInstance(ItemInstance.ItemId, AmountToAdd));
	
Finish:
	UpdateWeightAndSlots();

	OnItemAddedToContainer.Broadcast(FRISItemInstance(ItemInstance.ItemId, AmountToAdd));

	MARK_PROPERTY_DIRTY_FROM_NAME(URISItemContainerComponent, ItemsVer, this);

	return AmountToAdd; // Return the actual quantity added
}

int32 URISItemContainerComponent::RemoveItems_IfServer(const FRISItemInstance& ItemInstance, bool AllowPartial)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	int32 ContainedAmount = GetContainerItemCount(ItemInstance.ItemId);
	if (!AllowPartial && !DoesContainerContainItems(ItemInstance.ItemId, ItemInstance.Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove item: %s"), *ItemInstance.ItemId.ToString());
		return 0;
	}

	int32 AmountRemoved = 0;

	for (int i = ItemsVer.Items.Num() - 1; i >= 0; --i)
	{
		auto& ExistingItem = ItemsVer.Items[i];
		if (ExistingItem.ItemId == ItemInstance.ItemId)
		{
			AmountRemoved += FMath::Min(ExistingItem.Quantity, ItemInstance.Quantity);
			ExistingItem.Quantity -= ItemInstance.Quantity;

			// If the quantity drops to zero or below, remove the item from the inventory
			if (ExistingItem.Quantity <= 0)
			{
				ItemsVer.Items.RemoveAt(i);
				break; // Assuming ItemId is unique and only one instance exists in the inventory
			}
		}
	}

	// Update the current weight of the inventory
	UpdateWeightAndSlots();

	OnItemRemovedFromContainer.Broadcast(ItemInstance);

	// Mark the Items array as dirty to ensure replication
	MARK_PROPERTY_DIRTY_FROM_NAME(URISItemContainerComponent, ItemsVer, this);

	return AmountRemoved;
}

ARISWorldItem* URISItemContainerComponent::SpawnDroppedItem_IfServer(const FRISItemInstance& ItemInstance,
                                                                   float DropAngle) const
{
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		const FVector DropSpot = DropAngle == 0
			                         ? GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() *
			                         DropDistance
			                         : GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector().
			                                                                        RotateAngleAxis(DropAngle, FVector::UpVector) * DropDistance;
		ARISWorldItem* WorldItem = World->SpawnActorDeferred<ARISWorldItem>(DropItemClass, FTransform(DropSpot));
		if (WorldItem)
		{
			WorldItem->SetItem(ItemInstance);
			WorldItem->FinishSpawning(FTransform(DropSpot));
		}
		return WorldItem;
	}
	return nullptr;
}

FRISItemInstance* URISItemContainerComponent::FindContainerItemInstance(const FGameplayTag& ItemId)
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

int32 URISItemContainerComponent::DropItems(const FRISItemInstance& ItemInstance, float DropAngle)
{
	DropItems_Server(ItemInstance, DropAngle);

	// On client the below is just a guess
	const auto ContainedItemInstance = FindItemById(ItemInstance.ItemId);
	int32 QuantityToDrop = FMath::Min(ItemInstance.Quantity, ContainedItemInstance.Quantity);


	return QuantityToDrop;
}

void URISItemContainerComponent::DropItems_Server_Implementation(const FRISItemInstance& ItemInstance,
                                                                 float DropAngle)
{
	auto ContainedItemInstance = FindItemById(ItemInstance.ItemId);
	const int32 QuantityToDrop = FMath::Min(ItemInstance.Quantity, ContainedItemInstance.Quantity);

	if (QuantityToDrop <= 0 || !ContainedItemInstance.ItemId.IsValid())
	{
		return;
	}

	ARISWorldItem* DroppedItem = SpawnDroppedItem_IfServer(FRISItemInstance(ItemInstance.ItemId, QuantityToDrop),
	                                                    DropAngle);
	if (DroppedItem)
	{
		ContainedItemInstance.Quantity -= QuantityToDrop;
		if (ContainedItemInstance.Quantity <= 0)
		{
			ItemsVer.Items.Remove(ContainedItemInstance);
		}
		OnItemRemovedFromContainer.Broadcast(ItemInstance);
		UpdateWeightAndSlots();
	}
}

int32 URISItemContainerComponent::DropAllItems_IfServer()
{
	return DropAllItems_ServerImpl();
}

int32 URISItemContainerComponent::DropAllItems_ServerImpl()
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	// drop with incrementing angle
	int32 DroppedCount = 0;
	float AngleStep = 360.f / ItemsVer.Items.Num();

	for (int i = ItemsVer.Items.Num() - 1; i >= 0; --i)
	{
		DropItems(ItemsVer.Items[i], AngleStep * DroppedCount);
		DroppedCount++;
	}

	UpdateWeightAndSlots();

	return DroppedCount;
}


float URISItemContainerComponent::GetCurrentWeight() const
{
	return CurrentWeight;
}

float URISItemContainerComponent::GetMaxWeight() const
{
	return MaxWeight <= 0.f ? MAX_flt : MaxWeight;
}

const FRISItemInstance& URISItemContainerComponent::FindItemById(const FGameplayTag& ItemId) const
{
	for (const auto& Item : ItemsVer.Items)
	{
		if (Item.ItemId == ItemId)
		{
			return Item;
		}
	}

	// If the item is not found, throw an error or return a reference to a static empty item info
	UE_LOG(LogTemp, Warning, TEXT("Item with ID %s not found."), *ItemId.ToString());
	return FRISItemInstance::EmptyItemInstance;
}

bool URISItemContainerComponent::CanContainerReceiveItems(const FRISItemInstance& ItemInstance) const
{
	return GetQuantityOfItemContainerCanReceive(ItemInstance.ItemId) >= ItemInstance.Quantity;
}

int32 URISItemContainerComponent::GetQuantityOfItemContainerCanReceive(const FGameplayTag& ItemId) const
{
	const URISItemData* ItemData = URISInventoryFunctions::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	// Calculate how many items can be added without exceeding the max weight
	int32 AcceptableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	AcceptableQuantityByWeight = AcceptableQuantityByWeight > 0 ? AcceptableQuantityByWeight : 0;
	
	const int32 ItemQuantityTillNextFullSlot = ItemData->bIsStackable ? GetContainerItemCount(ItemId) % ItemData->MaxStackSize : 0;
	const int32 AvailableSlots = MaxContainerSlotCount - UsedContainerSlotCount;
	const int32 AcceptableQuantityBySlotCount = AvailableSlots * ItemData->MaxStackSize + ItemQuantityTillNextFullSlot;

	return FMath::Min(AcceptableQuantityByWeight, AcceptableQuantityBySlotCount);
}

// check just weight

bool URISItemContainerComponent::HasWeightCapacityForItems(const FRISItemInstance& ItemInstance) const
{
	const URISItemData* ItemData = URISInventoryFunctions::GetItemDataById(ItemInstance.ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemInstance.ItemId.ToString());
		return false;
	}

	const int32 AcceptableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	return AcceptableQuantityByWeight >= ItemInstance.Quantity;
}

bool URISItemContainerComponent::DoesContainerContainItems(const FGameplayTag& ItemId, int32 Quantity) const
{
	return ContainsItemsImpl(ItemId, Quantity);
}

bool URISItemContainerComponent::ContainsItemsImpl(const FGameplayTag& ItemId, int32 Quantity) const
{
	return GetContainerItemCount(ItemId) >= Quantity;
}

int32 URISItemContainerComponent::GetContainerItemCount(const FGameplayTag& ItemId) const
{
	for (const auto& Item : ItemsVer.Items)
	{
		if (Item.ItemId == ItemId)
		{
			return Item.Quantity;
		}
	}

	return 0;
}

TArray<FRISItemInstance> URISItemContainerComponent::GetAllContainerItems() const
{
	return ItemsVer.Items;
}

bool URISItemContainerComponent::IsEmpty() const
{
	return ItemsVer.Items.Num() == 0;
}

void URISItemContainerComponent::ClearContainer_IfServer()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}

	ItemsVer.Items.Reset();
	OnRep_Items();
}

void URISItemContainerComponent::UpdateWeightAndSlots()
{
	CurrentWeight = 0.0f; // Reset weight
	UsedContainerSlotCount = 0;
	for (const auto& ItemInstance : ItemsVer.Items)
	{
		if (const URISItemData* const ItemData = URISInventoryFunctions::GetItemDataById(ItemInstance.ItemId))
		{
			CurrentWeight += ItemData->ItemWeight * ItemInstance.Quantity;
			UsedContainerSlotCount += FMath::CeilToInt(ItemInstance.Quantity / static_cast<float>(ItemData->MaxStackSize));
		}
	}

	// This example does not handle the case where item data is not found, which might be important for ensuring accuracy.
}

void URISItemContainerComponent::CopyItemsToCache()
{
	ItemsCache.Reset();
	ItemsCache.Reserve(ItemsVer.Items.Num());
	for (int i = 0; i < ItemsVer.Items.Num(); ++i)
	{
		ItemsCache[ItemsVer.Items[i].ItemId] = ItemsVer.Items[i].Quantity;
	}
}

void URISItemContainerComponent::DetectAndPublishChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	for (FRISItemInstance& NewItem : ItemsVer.Items)
	{
		int32* OldQuantity = ItemsCache.Find(NewItem.ItemId);
		if (OldQuantity)
		{
			// Item exists, check for quantity change
			if (*OldQuantity != NewItem.Quantity)
			{
				if (*OldQuantity < NewItem.Quantity)
				{
					OnItemAddedToContainer.Broadcast(FRISItemInstance(NewItem.ItemId, NewItem.Quantity - *OldQuantity));
				}
				else if (*OldQuantity > NewItem.Quantity)
				{
					OnItemRemovedFromContainer.Broadcast(FRISItemInstance(NewItem.ItemId, *OldQuantity - NewItem.Quantity));
				}
			}
			// Mark this item as processed by temporarily setting its value to its own negative
			*OldQuantity = -abs(NewItem.Quantity);
		}
		else
		{
			// New item
			OnItemAddedToContainer.Broadcast(NewItem);
			ItemsCache.Add(NewItem.ItemId, -abs(NewItem.Quantity)); // Mark as processed
		}
	}

	// Second pass: Remove unmarked items (those not set to negative) and revert marks for processed items
	_KeysToRemove.Reset(ItemsCache.Num());
	for (auto& Pair : ItemsCache)
	{
		if (Pair.Value >= 0)
		{
			// Item was not processed (not found in Items), so it has been removed
			OnItemRemovedFromContainer.Broadcast(FRISItemInstance(Pair.Key, Pair.Value));
			_KeysToRemove.Add(Pair.Key);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			Pair.Value = -Pair.Value;
		}
	}

	// Remove items that were not found in the current Items array
	for (const FGameplayTag& Key : _KeysToRemove)
	{
		ItemsCache.Remove(Key);
	}
}
