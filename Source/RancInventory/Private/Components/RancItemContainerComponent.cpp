#include "Components/RancItemContainerComponent.h"

#include "Management/RancInventoryFunctions.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

URancItemContainerComponent::URancItemContainerComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer), MaxWeight(0.f), MaxNumItemsInContainer(0), CurrentWeight(0.f)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void URancItemContainerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// add all initial items to items
	for (const FRancInitialItem& InitialItem : InitialItems)
	{
		const URancItemData* Data = URancInventoryFunctions::GetSingleItemDataById(InitialItem.ItemId, {}, false);

		if (Data && Data->ItemId.IsValid())
			Items.Add(FRancItemInstance(Data->ItemId, InitialItem.Quantity));
	}
	CopyItemsToCache();

	if (DropItemClass == nullptr)
	{
		DropItemClass = AWorldItem::StaticClass();
	}
}


void URancItemContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(URancItemContainerComponent, Items, SharedParams);
}


void URancItemContainerComponent::OnRep_Items()
{
	// Recalculate the total weight of the inventory after replication.
	UpdateWeight();

	DetectAndPublishChanges();
}

int32 URancItemContainerComponent::AddItems_IfServer(const FRancItemInstance& ItemInstance, bool AllowPartial)
{
    if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None) // none needed for tests
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItems called on non-authority!"));
        return 0;
    }

    // Check if the inventory can receive the item and calculate the acceptable quantity
    int32 AcceptableQuantity = GetAmountOfItemContainerCanReceive(ItemInstance.ItemId);

    if (AcceptableQuantity <= 0 || (!AllowPartial && AcceptableQuantity < ItemInstance.Quantity))
    {
        return 0; // No items added
    }

	int32 AmountToAdd = FMath::Min(AcceptableQuantity, ItemInstance.Quantity);
    for (auto& ExistingItem : Items)
    {
        // If item exists, increase the quantity up to the acceptable amount
        if (ExistingItem.ItemId == ItemInstance.ItemId)
        {
            ExistingItem.Quantity += AmountToAdd;
        	goto Finish;
        }
    }
	
    Items.Add(FRancItemInstance(ItemInstance.ItemId, AmountToAdd));
	
	Finish:
    UpdateWeight();

    OnItemAdded.Broadcast(FRancItemInstance(ItemInstance.ItemId, AmountToAdd));

    MARK_PROPERTY_DIRTY_FROM_NAME(URancItemContainerComponent, Items, this);

    return AmountToAdd; // Return the actual quantity added
}

int32 URancItemContainerComponent::RemoveItems_IfServer(const FRancItemInstance& ItemInstance, bool AllowPartial)
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	int32 ContainedAmount = GetItemCount(ItemInstance.ItemId);
	if (!AllowPartial && !ContainsItems(ItemInstance.ItemId, ItemInstance.Quantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot remove item: %s"), *ItemInstance.ItemId.ToString());
		return 0;
	}

	int32 AmountRemoved = 0;

	for (int i = Items.Num() - 1; i >= 0; --i)
	{
		auto& ExistingItem = Items[i];
		if (ExistingItem.ItemId == ItemInstance.ItemId)
		{
			AmountRemoved += FMath::Min(ExistingItem.Quantity, ItemInstance.Quantity);
			ExistingItem.Quantity -= ItemInstance.Quantity;

			// If the quantity drops to zero or below, remove the item from the inventory
			if (ExistingItem.Quantity <= 0)
			{
				Items.RemoveAt(i);
				break; // Assuming ItemId is unique and only one instance exists in the inventory
			}
		}
	}

	// Update the current weight of the inventory
	UpdateWeight();

	OnItemRemoved.Broadcast(ItemInstance);

	// Mark the Items array as dirty to ensure replication
	MARK_PROPERTY_DIRTY_FROM_NAME(URancItemContainerComponent, Items, this);

	return AmountRemoved;
}

AWorldItem* URancItemContainerComponent::SpawnDroppedItem_IfServer(const FRancItemInstance& ItemInstance,
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
		AWorldItem* WorldItem = World->SpawnActorDeferred<AWorldItem>(DropItemClass, FTransform(DropSpot));
		if (WorldItem)
		{
			WorldItem->SetItem(ItemInstance);
			WorldItem->FinishSpawning(FTransform(DropSpot));
		}
		return WorldItem;
	}
	return nullptr;
}

int32 URancItemContainerComponent::DropItems(const FRancItemInstance& ItemInstance, float DropAngle)
{
	DropItems_Server(ItemInstance, DropAngle);

	// On client the below is just a guess
	const auto ContainedItemInstance = FindItemById(ItemInstance.ItemId);
	int32 QuantityToDrop = FMath::Min(ItemInstance.Quantity, ContainedItemInstance.Quantity);


	return QuantityToDrop;
}

void URancItemContainerComponent::DropItems_Server_Implementation(const FRancItemInstance& ItemInstance,
                                                                  float DropAngle)
{
	auto ContainedItemInstance = FindItemById(ItemInstance.ItemId);
	const int32 QuantityToDrop = FMath::Min(ItemInstance.Quantity, ContainedItemInstance.Quantity);

	if (QuantityToDrop <= 0 || !ContainedItemInstance.ItemId.IsValid())
	{
		return;
	}

	AWorldItem* DroppedItem = SpawnDroppedItem_IfServer(FRancItemInstance(ItemInstance.ItemId, QuantityToDrop),
	                                                    DropAngle);
	if (DroppedItem)
	{
		ContainedItemInstance.Quantity -= QuantityToDrop;
		if (ContainedItemInstance.Quantity <= 0)
		{
			Items.Remove(ContainedItemInstance);
		}
		OnItemRemoved.Broadcast(ItemInstance);
		UpdateWeight();
	}
}

int32 URancItemContainerComponent::DropAllItems_IfServer()
{
	return DropAllItems_ServerImpl();
}

int32 URancItemContainerComponent::DropAllItems_ServerImpl()
{
	if (GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
		return 0;
	}

	// drop with incrementing angle
	int32 DroppedCount = 0;
	float AngleStep = 360.f / Items.Num();

	for (int i = Items.Num() - 1; i >= 0; --i)
	{
		DropItems(Items[i], AngleStep * DroppedCount);
		DroppedCount++;
	}

	UpdateWeight();

	return DroppedCount;
}


float URancItemContainerComponent::GetCurrentWeight() const
{
	return CurrentWeight;
}

float URancItemContainerComponent::GetMaxWeight() const
{
	return MaxWeight <= 0.f ? MAX_flt : MaxWeight;
}

const FRancItemInstance& URancItemContainerComponent::FindItemById(const FGameplayTag& ItemId) const
{
	for (const auto& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			return Item;
		}
	}

	// If the item is not found, throw an error or return a reference to a static empty item info
	UE_LOG(LogTemp, Warning, TEXT("Item with ID %s not found."), *ItemId.ToString());
	return FRancItemInstance::EmptyItemInstance;
}

bool URancItemContainerComponent::CanContainerReceiveItems(const FRancItemInstance& ItemInstance) const
{
	return GetAmountOfItemContainerCanReceive(ItemInstance.ItemId) >= ItemInstance.Quantity;
}

int32 URancItemContainerComponent::GetAmountOfItemContainerCanReceive(const FGameplayTag& ItemId) const
{
	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemId.ToString());
		return 0; // Item data not found
	}

	// Calculate how many items can be added without exceeding the max weight
	int32 AcceptableQuantityByWeight = FMath::FloorToInt((MaxWeight - CurrentWeight) / ItemData->ItemWeight);
	AcceptableQuantityByWeight = AcceptableQuantityByWeight > 0 ? AcceptableQuantityByWeight : 0;
	
	int32 AcceptableQuantityByNumItems = 0;
	// Sum up quantities in all items
	for (const auto& Item : Items)
	{
		AcceptableQuantityByNumItems += Item.Quantity;
	}
	
	AcceptableQuantityByNumItems = MaxNumItemsInContainer - AcceptableQuantityByNumItems;

	return FMath::Min(AcceptableQuantityByWeight, AcceptableQuantityByNumItems);
}


bool URancItemContainerComponent::ContainsItems(const FGameplayTag& ItemId, int32 Quantity) const
{
	return ContainsItemsImpl(ItemId, Quantity);
}

bool URancItemContainerComponent::ContainsItemsImpl(const FGameplayTag& ItemId, int32 Quantity) const
{
	return GetItemCount(ItemId) >= Quantity;
}

int32 URancItemContainerComponent::GetItemCount(const FGameplayTag& ItemId) const
{
	for (const auto& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			return Item.Quantity;
		}
	}

	return 0;
}

TArray<FRancItemInstance> URancItemContainerComponent::GetAllItems() const
{
	return Items;
}

bool URancItemContainerComponent::IsEmpty() const
{
	return Items.Num() == 0;
}

void URancItemContainerComponent::ClearContainer_IfServer()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory called on non-authority!"));
		return;
	}
	
	Items.Reset();
	OnRep_Items();
}

void URancItemContainerComponent::UpdateWeight()
{
	CurrentWeight = 0.0f; // Reset weight
	for (const auto& ItemInstance : Items)
	{
		if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemDataById(ItemInstance.ItemId))
		{
			CurrentWeight += ItemData->ItemWeight * ItemInstance.Quantity;
		}
	}

	// This example does not handle the case where item data is not found, which might be important for ensuring accuracy.
}

void URancItemContainerComponent::CopyItemsToCache()
{
	ItemsCache.Reset();
	ItemsCache.Reserve(Items.Num());
	for (int i = 0; i < Items.Num(); ++i)
	{
		ItemsCache[Items[i].ItemId] = Items[i].Quantity;
	}
}

void URancItemContainerComponent::DetectAndPublishChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	for (FRancItemInstance& NewItem : Items)
	{
		int32* OldQuantity = ItemsCache.Find(NewItem.ItemId);
		if (OldQuantity)
		{
			// Item exists, check for quantity change
			if (*OldQuantity != NewItem.Quantity)
			{
				if (*OldQuantity < NewItem.Quantity)
				{
					OnItemAdded.Broadcast(FRancItemInstance(NewItem.ItemId, NewItem.Quantity - *OldQuantity));
				}
				else if (*OldQuantity > NewItem.Quantity)
				{
					OnItemRemoved.Broadcast(FRancItemInstance(NewItem.ItemId, *OldQuantity - NewItem.Quantity));
				}
			}
			// Mark this item as processed by temporarily setting its value to its own negative
			*OldQuantity = -abs(NewItem.Quantity);
		}
		else
		{
			// New item
			OnItemAdded.Broadcast(NewItem);
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
			OnItemRemoved.Broadcast(FRancItemInstance(Pair.Key, Pair.Value));
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
