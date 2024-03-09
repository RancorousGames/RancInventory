// Copyright Rancorous Games, 2024

#include "Components\RISInventoryComponent.h"
#include <GameFramework/Actor.h>

#include <Engine/AssetManager.h>
#include "Management\RISInventoryFunctions.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"


URISInventoryComponent::URISInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void URISInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Subscribe to base class inventory events
	OnItemAddedToContainer.AddDynamic(this, &URISInventoryComponent::OnInventoryItemAddedHandler);
	OnItemRemovedFromContainer.AddDynamic(this, &URISInventoryComponent::OnInventoryItemRemovedHandler);

	// Initialize available recipes based on initial inventory and recipes
	CheckAndUpdateRecipeAvailability();
}

int32 URISInventoryComponent::GetItemCountIncludingTaggedSlots(const FGameplayTag& ItemId) const
{
	int32 Quantity = GetContainerItemCount(ItemId);
	for (const FRancTaggedItemInstance& TaggedItem : TaggedSlotItemInstances)
	{
		if (TaggedItem.ItemInstance.ItemId == ItemId)
		{
			Quantity += TaggedItem.ItemInstance.Quantity;
		}
	}
	return Quantity;
}

void URISInventoryComponent::UpdateWeightAndSlots()
{
	Super::UpdateWeightAndSlots();

	for (const FRancTaggedItemInstance& TaggedInstance : TaggedSlotItemInstances)
	{
		if (const URISItemData* const ItemData = URISInventoryFunctions::GetItemDataById(
			TaggedInstance.ItemInstance.ItemId))
		{
			CurrentWeight += ItemData->ItemWeight * TaggedInstance.ItemInstance.Quantity;
		}
	}
}

bool URISInventoryComponent::ContainsItemsImpl(const FGameplayTag& ItemId, int32 Quantity) const
{
	return GetItemCountIncludingTaggedSlots(ItemId) >= Quantity;
}

void URISInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(URISInventoryComponent, TaggedSlotItemInstances, SharedParams);

	DOREPLIFETIME(URISInventoryComponent, AllUnlockedRecipes);
}


////////////////////////////////////////////////////// TAGGED SLOTS ///////////////////////////////////////////////////////
int32 URISInventoryComponent::AddItemsToTaggedSlot_IfServer(const FGameplayTag& SlotTag,
                                                             const FRISItemInstance& ItemsToAdd,
                                                             bool OverrideExistingItem)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddItemsToTaggedSlot_IfServer called on non-authority!"));
		return 0;
	}

	// New check for slot item compatibility and weight capacity
	if (!IsTaggedSlotCompatible(ItemsToAdd.ItemId, SlotTag) || !HasWeightCapacityForItems(ItemsToAdd))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item cannot be added to the tagged slot"));
		return 0; // Either the slot is incompatible
	}


	// Locate the existing item in the tagged slot
	FRancTaggedItemInstance EmptyCopy = FRancTaggedItemInstance::EmptyItemInstance;
	int32 Index = GetIndexForTaggedSlot(SlotTag);

	FRancTaggedItemInstance& ExistingItem = EmptyCopy;
	URISItemData* ItemData = nullptr;
	if (TaggedSlotItemInstances.IsValidIndex(Index))
	{
		ExistingItem = TaggedSlotItemInstances[Index];
		ItemData = URISInventoryFunctions::GetItemDataById(ItemsToAdd.ItemId);
	}

	// Determine the quantity to add
	int32 QuantityToAdd = ItemsToAdd.Quantity;
	if (ExistingItem.IsValid() && ExistingItem.ItemInstance.ItemId == ItemsToAdd.ItemId && ItemData->bIsStackable && !OverrideExistingItem)
	{
		// For stackable items, calculate the available space in the slot
		QuantityToAdd = FMath::Min(QuantityToAdd, ItemData->MaxStackSize - ExistingItem.ItemInstance.Quantity);
		ExistingItem.ItemInstance.Quantity += QuantityToAdd; // Update quantity directly
	}
	else if (OverrideExistingItem || !ExistingItem.IsValid())
	{
		// If overriding or slot is empty, clear existing and add new
		if (ExistingItem.IsValid())
		{
			RemoveQuantityFromTaggedSlot_IfServer(SlotTag, MAX_int32, true); // Remove existing item
		}
		TaggedSlotItemInstances.Add(FRancTaggedItemInstance(SlotTag, ItemsToAdd.ItemId, QuantityToAdd));
	}

	UpdateWeightAndSlots(); // Recalculate the current weight
	OnItemAddedToTaggedSlot.Broadcast(SlotTag, FRISItemInstance(ItemsToAdd.ItemId, QuantityToAdd));
	MARK_PROPERTY_DIRTY_FROM_NAME(URISInventoryComponent, TaggedSlotItemInstances, this);

	return QuantityToAdd;
}


int32 URISInventoryComponent::AddItemsToAnySlots_IfServer(FRISItemInstance ItemsToAdd, bool PreferTaggedSlots)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		return 0; // Ensure this method is called on the server
	}

	if (!HasWeightCapacityForItems(ItemsToAdd))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item cannot be added to the inventory due to weight capacity"));
		return 0; // Either the slot is incompatible
	}

	int32 TotalAdded = 0;
	int32 RemainingQuantity = ItemsToAdd.Quantity;

	// Adjust the flow based on PreferTaggedSlots flag
	if (!PreferTaggedSlots)
	{
		// Try adding to generic slots first if not preferring tagged slots
		int32 QuantityAddedToGeneric = AddItems_IfServer(FRISItemInstance(ItemsToAdd.ItemId, RemainingQuantity), true);
		TotalAdded += QuantityAddedToGeneric;
		RemainingQuantity -= QuantityAddedToGeneric;
	}

	// Proceed to try adding to tagged slots if PreferTaggedSlots is true or if there's remaining quantity
	if (PreferTaggedSlots || RemainingQuantity > 0)
	{
		for (const FGameplayTag& SlotTag : SpecializedTaggedSlots)
		{
			if (RemainingQuantity <= 0) break;
			if (IsTaggedSlotCompatible(ItemsToAdd.ItemId, SlotTag))
			{
				int32 QuantityAdded = AddItemsToTaggedSlot_IfServer(SlotTag, FRISItemInstance(ItemsToAdd.ItemId, RemainingQuantity), false);
				TotalAdded += QuantityAdded;
				RemainingQuantity -= QuantityAdded;
			}
		}

		for (const FGameplayTag& SlotTag : UniversalTaggedSlots)
		{
			if (RemainingQuantity <= 0) break;
			const int32 QuantityAdded = AddItemsToTaggedSlot_IfServer(SlotTag, FRISItemInstance(ItemsToAdd.ItemId, RemainingQuantity), false);
			TotalAdded += QuantityAdded;
			RemainingQuantity -= QuantityAdded;
		}
	}

	// If there's still remaining quantity and we didn't prefer tagged slots first, try adding back to generic slots
	if (RemainingQuantity > 0 && !PreferTaggedSlots)
	{
		int32 QuantityAddedBackToGeneric = AddItems_IfServer(FRISItemInstance(ItemsToAdd.ItemId, RemainingQuantity), true);
		TotalAdded += QuantityAddedBackToGeneric;
		RemainingQuantity -= QuantityAddedBackToGeneric;
	}

	return TotalAdded; // Total quantity successfully added across slots
}

int32 URISInventoryComponent::RemoveQuantityFromTaggedSlot_IfServer(const FGameplayTag& SlotTag, int32 QuantityToRemove, bool AllowPartial)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		return 0;
	}

	int32 IndexToRemoveAt = -1;

	FRancTaggedItemInstance* InstanceToRemoveFrom = nullptr;

	// The above but as a for loop
	for (int32 i = 0; i < TaggedSlotItemInstances.Num(); i++)
	{
		if (TaggedSlotItemInstances[i].Tag == SlotTag)
		{
			InstanceToRemoveFrom = &TaggedSlotItemInstances[i];
			IndexToRemoveAt = i;
			break;
		}
	}

	if (InstanceToRemoveFrom == nullptr || (!AllowPartial && InstanceToRemoveFrom->ItemInstance.Quantity <
		QuantityToRemove))
		return 0;

	const int32 ActualRemovedQuantity = FMath::Min(QuantityToRemove, InstanceToRemoveFrom->ItemInstance.Quantity);
	const FRISItemInstance Removed(InstanceToRemoveFrom->ItemInstance.ItemId, ActualRemovedQuantity);
	InstanceToRemoveFrom->ItemInstance.Quantity -= ActualRemovedQuantity;
	if (InstanceToRemoveFrom->ItemInstance.Quantity <= 0)
	{
		TaggedSlotItemInstances.RemoveAt(IndexToRemoveAt);
	}

	UpdateWeightAndSlots();
	OnItemRemovedFromTaggedSlot.Broadcast(SlotTag, Removed);
	MARK_PROPERTY_DIRTY_FROM_NAME(URISInventoryComponent, TaggedSlotItemInstances, this);
	return ActualRemovedQuantity;
}

int32 URISInventoryComponent::RemoveItemsFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove)
{
	int32 RemovedCount = 0;
	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		if (TaggedSlotItemInstances[i].ItemInstance.ItemId == ItemId)
		{
			RemovedCount += RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag, QuantityToRemove - RemovedCount, true);
			if (RemovedCount >= QuantityToRemove)
			{
				break;
			}
		}
	}

	return RemovedCount;
}

void URISInventoryComponent::MoveItems_Server_Implementation(const FRISItemInstance& ItemInstance,
                                                              const FGameplayTag& SourceTaggedSlot,
                                                              const FGameplayTag& TargetTaggedSlot)
{
	MoveItems_ServerImpl(ItemInstance, SourceTaggedSlot, TargetTaggedSlot);
}


int32 URISInventoryComponent::MoveItems_ServerImpl(const FRISItemInstance& ItemInstance,
                                                    const FGameplayTag& SourceTaggedSlot,
                                                    const FGameplayTag& TargetTaggedSlot)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveItemsToTaggedSlot_ServerImpl called on non-authority!"));
		return 0;
	}

	bool SourceIsTaggedSlot = SourceTaggedSlot.IsValid();
	bool TargetIsTaggedSlot = TargetTaggedSlot.IsValid();

	if (!SourceIsTaggedSlot && !TargetIsTaggedSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moving to and from container is not meaningful"));
		return 0;
	}

	FRISItemInstance* SourceItem = nullptr;
	int32 SourceItemContainerIndex = -1;
	if (SourceIsTaggedSlot)
	{
		const int32 SourceIndex = GetIndexForTaggedSlot(SourceTaggedSlot);
		if (!TaggedSlotItemInstances.IsValidIndex(SourceIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Source tagged slot does not exist"));
			return 0;
		}

		SourceItem = &TaggedSlotItemInstances[SourceIndex].ItemInstance;
	}
	else
	{
		for (int i = 0; i < ItemsVer.Items.Num(); i++)
		{
			if (ItemsVer.Items[i].ItemId == ItemInstance.ItemId)
			{
				SourceItemContainerIndex = i;
				SourceItem = &ItemsVer.Items[i];
				break;
			}
		}
		
		if (SourceItem == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Source container item does not exist"));
			return 0;
		}
	}

	FRISItemInstance* TargetItem;
	if (TargetIsTaggedSlot)
	{
		if (!IsTaggedSlotCompatible(ItemInstance.ItemId, TargetTaggedSlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
			return 0;
		}

		const int32 TargetIndex = GetIndexForTaggedSlot(TargetTaggedSlot);

		if (!TaggedSlotItemInstances.IsValidIndex(TargetIndex))
		{
			if (!UniversalTaggedSlots.Contains(TargetTaggedSlot) && !SpecializedTaggedSlots.Contains(TargetTaggedSlot))
			{
				UE_LOG(LogTemp, Warning, TEXT("Target tagged slot does not exist"));
				return 0;
			}

			TaggedSlotItemInstances.Add(FRancTaggedItemInstance(TargetTaggedSlot, FRISItemInstance::EmptyItemInstance));
			TargetItem = &TaggedSlotItemInstances.Last().ItemInstance;
		}
		else
		{
			TargetItem = &TaggedSlotItemInstances[TargetIndex].ItemInstance;
		}
	}
	else
	{
		TargetItem = FindContainerItemInstance(ItemInstance.ItemId);;
		if (TargetItem == nullptr)
		{
			ItemsVer.Items.Add(FRISItemInstance::EmptyItemInstance);
			TargetItem = &ItemsVer.Items.Last();
		}
	}

	if (TargetItem && SourceIsTaggedSlot && URISInventoryFunctions::ShouldItemsBeSwapped(SourceItem, TargetItem) && !IsTaggedSlotCompatible(TargetItem->ItemId, SourceTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the source slot"));
		return 0;
	}

	const int32 MovedQuantity = URISInventoryFunctions::MoveBetweenSlots(SourceItem, TargetItem, !TargetIsTaggedSlot, ItemInstance.Quantity, true);
	
	if (MovedQuantity > 0)
	{
		if (!SourceItem->IsValid())
		{
			if (SourceIsTaggedSlot)
				TaggedSlotItemInstances.RemoveAt(GetIndexForTaggedSlot(SourceTaggedSlot));
			else
				ItemsVer.Items.RemoveAt(SourceItemContainerIndex);
		}
		
		const FRISItemInstance ActualMovedItem(ItemInstance.ItemId, MovedQuantity);
		MARK_PROPERTY_DIRTY_FROM_NAME(URISInventoryComponent, TaggedSlotItemInstances, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(URISItemContainerComponent, ItemsVer, this);

		if (SourceIsTaggedSlot)
			OnItemRemovedFromTaggedSlot.Broadcast(SourceTaggedSlot, ActualMovedItem);
		else
			OnItemRemovedFromContainer.Broadcast(ActualMovedItem);
		if (TargetIsTaggedSlot)
			OnItemAddedToTaggedSlot.Broadcast(TargetTaggedSlot, ActualMovedItem);
		else
			OnItemAddedToContainer.Broadcast(ActualMovedItem);
	}

	return MovedQuantity;
}

int32 URISInventoryComponent::DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, float DropAngle)
{
	// On client the below is just a guess
	const FRancTaggedItemInstance Item = GetItemForTaggedSlot(SlotTag);
	if (!Item.IsValid()) return 0;
	int32 QuantityToDrop = FMath::Min(Quantity, Item.ItemInstance.Quantity);

	DropFromTaggedSlot_Server_Implementation(SlotTag, Quantity, DropAngle);

	return QuantityToDrop;
}

bool URISInventoryComponent::CanTaggedSlotReceiveItem(const FRISItemInstance& ItemInstance, const FGameplayTag& SlotTag) const
{
	return IsTaggedSlotCompatible(ItemInstance.ItemId, SlotTag) && CanContainerReceiveItems(ItemInstance);
}

void URISInventoryComponent::DropFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag, int32 Quantity,
                                                                       float DropAngle)
{
	const FRancTaggedItemInstance Item = GetItemForTaggedSlot(SlotTag);
	if (Item.Tag.IsValid())
	{
		const int32 QuantityToDrop = FMath::Min(Quantity, Item.ItemInstance.Quantity);

		int32 DroppedCount = RemoveQuantityFromTaggedSlot_IfServer(SlotTag, QuantityToDrop);

		if (DroppedCount > 0)
		{
			const FRISItemInstance ItemToDrop(Item.ItemInstance.ItemId, QuantityToDrop);
			// ReSharper disable once CppExpressionWithoutSideEffects
			SpawnDroppedItem_IfServer(ItemToDrop);
		}
	}
}

const FRancTaggedItemInstance& URISInventoryComponent::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	int32 Index = GetIndexForTaggedSlot(SlotTag);

	if (Index < 0 || Index >= TaggedSlotItemInstances.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("GetItemForTaggedSlot called with invalid slot tag"));
		return FRancTaggedItemInstance::EmptyItemInstance;
	}

	return TaggedSlotItemInstances[Index];
}

int32 URISInventoryComponent::GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const
{
	// loop over SpecialSlotItems
	for (int i = 0; i < TaggedSlotItemInstances.Num(); i++)
	{
		if (TaggedSlotItemInstances[i].Tag == SlotTag)
			return i;
	}

	return -1;
}

void URISInventoryComponent::ClearInventory_IfServer()
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("ClearInventory_IfServer called on non-authority!"));
		return;
	}

	ClearContainer_IfServer();

	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		RemoveQuantityFromTaggedSlot_IfServer(TaggedSlotItemInstances[i].Tag, MAX_int32, true);
	}
}

TArray<FRancTaggedItemInstance> URISInventoryComponent::GetAllTaggedItems() const
{
	return TaggedSlotItemInstances;
}

void URISInventoryComponent::DetectAndPublishChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	for (FRancTaggedItemInstance& NewItem : TaggedSlotItemInstances)
	{
		FRISItemInstance* OldItem = TaggedItemsCache.Find(NewItem.Tag);

		if (OldItem)
		{
			// Item is the same, check for quantity change
			if (OldItem->ItemId != NewItem.ItemInstance.ItemId || OldItem->Quantity != NewItem.ItemInstance.Quantity)
			{
				if (OldItem->ItemId == NewItem.ItemInstance.ItemId)
				{
					if (OldItem->Quantity < NewItem.ItemInstance.Quantity)
					{
						OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,
						                                  FRISItemInstance(
							                                  NewItem.ItemInstance.ItemId,
							                                  NewItem.ItemInstance.Quantity - OldItem->Quantity));
					}
					else if (OldItem->Quantity > NewItem.ItemInstance.Quantity)
					{
						OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag,
						                                      FRISItemInstance(
							                                      NewItem.ItemInstance.ItemId,
							                                      OldItem->Quantity - NewItem.ItemInstance.Quantity));
					}
				}
				else // Item has changed
				{
					OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag,
					                                      FRISItemInstance(OldItem->ItemId, OldItem->Quantity));
					OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,
					                                  FRISItemInstance(NewItem.ItemInstance.ItemId,
					                                                    NewItem.ItemInstance.Quantity));
				}
			}

			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -NewItem.ItemInstance.Quantity;
		}
		else // New slot has been added to
		{
			OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,
			                                  FRISItemInstance(NewItem.ItemInstance.ItemId,
			                                                    NewItem.ItemInstance.Quantity));
			TaggedItemsCache.Add(NewItem.Tag,
			                     FRISItemInstance(NewItem.ItemInstance.ItemId, -NewItem.ItemInstance.Quantity));
			// Mark as processed
		}
	}

	// Second pass: Remove unmarked items (those not set to negative) and revert marks for processed items
	_SlotsToRemove.Reset(TaggedItemsCache.Num());
	for (auto& SlotItemKV : TaggedItemsCache)
	{
		if (SlotItemKV.Value.Quantity >= 0)
		{
			// Item was not processed (not found in Items), so it has been removed
			OnItemRemovedFromTaggedSlot.Broadcast(SlotItemKV.Key, SlotItemKV.Value);
			_SlotsToRemove.Add(SlotItemKV.Key);
		}
		else
		{
			// Revert the mark to reflect the actual quantity
			SlotItemKV.Value.Quantity = -SlotItemKV.Value.Quantity;
		}
	}

	// Remove items that were not found in the current Items array
	for (const FGameplayTag& Key : _SlotsToRemove)
	{
		TaggedItemsCache.Remove(Key);
	}
}

bool URISInventoryComponent::IsTaggedSlotCompatible(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const
{
	const URISItemData* ItemData = URISInventoryFunctions::GetItemDataById(ItemId);
	if (!ItemData)
	{
		return false; // Item data not found, assume slot cannot receive item
	}

	if (!UniversalTaggedSlots.Contains(SlotTag) && !ItemData->ItemCategories.HasTag(SlotTag))
	{
		return false; // Item is not compatible with the slot
	}

	return true; // Slot is compatible
}

void URISInventoryComponent::OnRep_Slots()
{
	UpdateWeightAndSlots();
	DetectAndPublishChanges();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// CRAFTING /////////////////////////////////////////////////////////////////

bool URISInventoryComponent::CanCraftRecipeId(const FPrimaryRISRecipeId& RecipeId) const
{
	const URISObjectRecipeData* Recipe = Cast<URISObjectRecipeData>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(Recipe);
}

bool URISInventoryComponent::CanCraftRecipe(const URISObjectRecipeData* Recipe) const
{
	if (!Recipe) return false;

	for (const auto& Component : Recipe->Components)
	{
		if (!DoesContainerContainItems(Component.ItemId, Component.Quantity))
		{
			return false;
		}
	}
	return true;
}

bool URISInventoryComponent::CanCraftCraftingRecipe(const FPrimaryRISRecipeId& RecipeId) const
{
	URISItemRecipeData* CraftingRecipe = Cast<URISItemRecipeData>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(CraftingRecipe);
}

void URISInventoryComponent::CraftRecipeId_Server_Implementation(const FPrimaryRISRecipeId& RecipeId)
{
	const URISObjectRecipeData* Recipe = Cast<URISObjectRecipeData>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	CraftRecipe_IfServer(Recipe);
}

bool URISInventoryComponent::CraftRecipe_IfServer(const URISObjectRecipeData* Recipe)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		return false;
	}

	bool bSuccess = false;
	if (Recipe && CanCraftRecipe(Recipe))
	{
		for (const auto& Component : Recipe->Components)
		{
			int QuantityToRemoveFromGenericSlots = FMath::Min(GetContainerItemCount(Component.ItemId), Component.Quantity);
			int32 RemovedFromGeneric = RemoveItems_IfServer(FRISItemInstance(Component.ItemId, QuantityToRemoveFromGenericSlots));
			int32 RemovedFromTaggedSlots = RemoveItemsFromAnyTaggedSlots_IfServer(Component.ItemId, Component.Quantity - QuantityToRemoveFromGenericSlots);
			if (RemovedFromGeneric + RemovedFromTaggedSlots < Component.Quantity)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to remove all items for crafting even though they were confirmed"));
				return false;
			}
		}
		bSuccess = true;

		if (const URISItemRecipeData* ItemRecipe = Cast<URISItemRecipeData>(Recipe))
		{
			// If it is an item recipe, add the resulting item to the inventory
			auto CraftedItem = FRISItemInstance(ItemRecipe->ResultingItemId, ItemRecipe->QuantityCreated);
			const int32 AmountAdded = AddItemsToAnySlots_IfServer(CraftedItem, false);
			if (AmountAdded < ItemRecipe->QuantityCreated)
			{
				UE_LOG(LogTemp, Display, TEXT("Failed to add crafted item to inventory, dropping item instead"));
				/*AWorldItem* DroppedItem =*/
				SpawnDroppedItem_IfServer(FRISItemInstance(ItemRecipe->ResultingItemId, ItemRecipe->QuantityCreated - AmountAdded));
			}
		}
		else
		{
			OnCraftConfirmed.Broadcast(Recipe->ResultingObject, Recipe->QuantityCreated);
		}
	}
	return bSuccess;
}

void URISInventoryComponent::SetRecipeLock_Server_Implementation(const FPrimaryRISRecipeId& RecipeId, bool LockState)
{
	if (AllUnlockedRecipes.Contains(RecipeId) != LockState)
	{
		if (LockState)
		{
			AllUnlockedRecipes.Remove(RecipeId);
		}
		else
		{
			AllUnlockedRecipes.Add(RecipeId);
		}

		if (GetNetMode() == NM_ListenServer)
		{
			CheckAndUpdateRecipeAvailability();
		}
	}
}

URISObjectRecipeData* URISInventoryComponent::GetRecipeById(const FPrimaryRISRecipeId& RecipeId)
{
	return Cast<URISObjectRecipeData>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
}

TArray<URISObjectRecipeData*> URISInventoryComponent::GetAvailableRecipes(FGameplayTag TagFilter)
{
	return CurrentAvailableRecipes.Contains(TagFilter) ? CurrentAvailableRecipes[TagFilter] : TArray<URISObjectRecipeData*>();
}

void URISInventoryComponent::CheckAndUpdateRecipeAvailability()
{
	// Clear current available recipes
	CurrentAvailableRecipes.Empty();

	// Iterate through all available recipes and check if they can be crafted
	for (const FPrimaryRISRecipeId& RecipeId : AllUnlockedRecipes)
	{
		URISObjectRecipeData* Recipe = GetRecipeById(RecipeId);
		if (CanCraftRecipe(Recipe))
		{
			for (const FGameplayTag& Category : RecipeTagFilters)
			{
				// If recipe matches a category, add it to the corresponding list
				if (Recipe->Tags.HasTag(Category))
				{
					if (!CurrentAvailableRecipes.Contains(Category))
					{
						CurrentAvailableRecipes.Add(Category, {}); // ensure the value is a valid array
					}
					CurrentAvailableRecipes[Category].Add(Recipe);
				}
			}
		}
	}

	OnAvailableRecipesUpdated.Broadcast();
}

int32 URISInventoryComponent::DropAllItems_ServerImpl()
{
	int32 DropCount = Super::DropAllItems_ServerImpl();

	// Drop all items from tagged slots
	for (int i = TaggedSlotItemInstances.Num() - 1; i >= 0; i--)
	{
		DropFromTaggedSlot_Server(TaggedSlotItemInstances[i].Tag, TaggedSlotItemInstances[i].ItemInstance.Quantity,
		                          FMath::FRand() * 360.0f);
		DropCount++;
	}

	return DropCount;
}


void URISInventoryComponent::OnInventoryItemAddedHandler(const FRISItemInstance& ItemInfo)
{
	CheckAndUpdateRecipeAvailability();
}

void URISInventoryComponent::OnInventoryItemRemovedHandler(const FRISItemInstance& ItemInfo)
{
	CheckAndUpdateRecipeAvailability();
}

void URISInventoryComponent::OnRep_Recipes()
{
	CheckAndUpdateRecipeAvailability();
}
