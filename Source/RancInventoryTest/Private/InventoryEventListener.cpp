#include "InventoryEventListener.h"

void UGlobalInventoryEventListener::SubscribeToInventoryComponent(UInventoryComponent* InventoryComponent)
{
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryComponent is null!"));
		return;
	}
        
	InventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemAddedToTaggedSlot);
	InventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemRemovedFromTaggedSlot);
	InventoryComponent->OnItemAddedToContainer.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemAddedToContainer);
	InventoryComponent->OnItemRemovedFromContainer.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemRemovedFromContainer);
	InventoryComponent->OnCraftConfirmed.AddDynamic(this, &UGlobalInventoryEventListener::OnCraftConfirmed);
	InventoryComponent->OnAvailableRecipesUpdated.AddDynamic(this, &UGlobalInventoryEventListener::OnAvailableRecipesUpdated);
}

void UGlobalInventoryEventListener::Clear()
{
	bItemAddedTriggered = false;
	bItemRemovedTriggered = false;
	bItemAddedToTaggedTriggered = false;
	bItemRemovedFromTaggedTriggered = false;
	bCraftConfirmedTriggered = false;
	bAvailableRecipesUpdatedTriggered = false;
}

void UGlobalInventoryEventListener::HandleItemAddedToTaggedSlot(const FGameplayTag& InSlotTag,
	const UItemStaticData* InItemStaticData, int32 InQuantity, FTaggedItemBundle PreviousItem,
	EItemChangeReason InChangeReason)
{
	bItemAddedToTaggedTriggered = true;
	AddedSlotTag = InSlotTag;
	// Casting away const for test purposes—ensure this is safe in your context
	AddedToTaggedItemStaticData = const_cast<UItemStaticData*>(InItemStaticData);
	AddedToTaggedQuantity = InQuantity;
	AddedToTaggedPreviousItem = PreviousItem;
	AddedToTaggedChangeReason = InChangeReason;
}

void UGlobalInventoryEventListener::HandleItemRemovedFromTaggedSlot(const FGameplayTag& InSlotTag,
	const UItemStaticData* InItemStaticData, int32 InQuantity, EItemChangeReason InChangeReason)
{
	bItemRemovedFromTaggedTriggered = true;
	RemovedSlotTag = InSlotTag;
	// Casting away const for test purposes—ensure this is safe in your context
	RemovedFromTaggedItemStaticData = const_cast<UItemStaticData*>(InItemStaticData);
	RemovedFromTaggedQuantity = InQuantity;
	RemovedFromTaggedChangeReason = InChangeReason;
}

void UGlobalInventoryEventListener::HandleItemAddedToContainer(const UItemStaticData* InItemStaticData,
	int32 InQuantity, EItemChangeReason InChangeReason)
{
	bItemAddedTriggered = true;
	// Casting away const for test purposes—ensure this is safe in your context
	AddedItemStaticData = const_cast<UItemStaticData*>(InItemStaticData);
	AddedQuantity = InQuantity;
	AddedChangeReason = InChangeReason;
}

void UGlobalInventoryEventListener::HandleItemRemovedFromContainer(const UItemStaticData* InItemStaticData,
                                                                   int32 InQuantity, EItemChangeReason InChangeReason)
{
	bItemRemovedTriggered = true;
	// Casting away const for test purposes—ensure this is safe in your context
	RemovedItemStaticData = const_cast<UItemStaticData*>(InItemStaticData);
	RemovedQuantity = InQuantity;
	RemovedChangeReason = InChangeReason;
}

void UGlobalInventoryEventListener::OnCraftConfirmed(TSubclassOf<UObject> InObject, int32 InQuantity)
{
	bCraftConfirmedTriggered = true;
	CraftConfirmedObject = InObject;
	CraftConfirmedQuantity = InQuantity;
}

void UGlobalInventoryEventListener::OnAvailableRecipesUpdated()
{
	bAvailableRecipesUpdatedTriggered = true;
}
