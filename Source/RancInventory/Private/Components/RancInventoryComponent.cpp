// Rancorous Games, 2024

#include "Components/RancInventoryComponent.h"
#include <GameFramework/Actor.h>

#include <Engine/AssetManager.h>
#include "Management/RancInventoryFunctions.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"


URancInventoryComponent::URancInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void URancInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Subscribe to base class inventory events
	OnItemAdded.AddDynamic(this, &URancInventoryComponent::OnInventoryItemAddedHandler);
	OnItemRemoved.AddDynamic(this, &URancInventoryComponent::OnInventoryItemRemovedHandler);

	// Initialize available recipes based on initial inventory and recipes
	CheckAndUpdateRecipeAvailability();
}

void URancInventoryComponent::UpdateWeight()
{
	Super::UpdateWeight();

	for (const FRancTaggedItemInstance& TaggedInstance : TaggedSlotItemInstances)
	{
		if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemDataById(
			TaggedInstance.ItemInstance.ItemId))
		{
			CurrentWeight += ItemData->ItemWeight * TaggedInstance.ItemInstance.Quantity;
		}
	}
}

void URancInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(URancInventoryComponent, TaggedSlotItemInstances, SharedParams);

	DOREPLIFETIME(URancInventoryComponent, AllUnlockedRecipes);
}


////////////////////////////////////////////////////// TAGGED SLOTS ///////////////////////////////////////////////////////
int32 URancInventoryComponent::AddItemToTaggedSlot_IfServer(const FGameplayTag& SlotTag,
                                                            const FRancItemInstance& ItemInfo,
                                                            bool OverrideExistingItem)
{
    if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
    {
        return 0;
    }

    if ((!UniversalTaggedSlots.Contains(SlotTag) && !SpecializedTaggedSlots.Contains(SlotTag)) ||
		(SpecializedTaggedSlots.Contains(SlotTag) && !IsItemCompatibleWithSlot(ItemInfo.ItemId, SlotTag)))
    {
        return 0;
    }

    FRancTaggedItemInstance& ExistingItem = const_cast<FRancTaggedItemInstance&>(GetItemForTaggedSlot(SlotTag));
    URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemInfo.ItemId);

    if (ExistingItem.IsValid() && !OverrideExistingItem && !(ExistingItem.ItemInstance.ItemId == ItemInfo.ItemId && ItemData->bIsStackable))
    {
        return 0;
    }

    int32 QuantityToAdd = ItemInfo.Quantity;

    if (ExistingItem.IsValid())
    {
        if (ExistingItem.ItemInstance.ItemId == ItemInfo.ItemId && ItemData->bIsStackable)
        {
            QuantityToAdd = FMath::Min(QuantityToAdd, ItemData->MaxStackSize - ExistingItem.ItemInstance.Quantity);
            ExistingItem.ItemInstance.Quantity += QuantityToAdd;
        }
        else if (OverrideExistingItem)
        {
            RemoveItemsFromTaggedSlot_IfServer(SlotTag, MAX_int32, true);
            TaggedSlotItemInstances.Add(FRancTaggedItemInstance(SlotTag, FRancItemInstance(ItemInfo.ItemId, QuantityToAdd)));
        }
    }
    else
    {
        TaggedSlotItemInstances.Add(FRancTaggedItemInstance(SlotTag, FRancItemInstance(ItemInfo.ItemId, QuantityToAdd)));
    }

    UpdateWeight();
    OnItemAddedToTaggedSlot.Broadcast(SlotTag, FRancItemInstance(ItemInfo.ItemId, QuantityToAdd));
    MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, TaggedSlotItemInstances, this);

    return QuantityToAdd;
}

int32 URancInventoryComponent::RemoveItemsFromTaggedSlot_IfServer(const FGameplayTag& SlotTag, int32 QuantityToRemove,
                                                                  bool AllowPartial)
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
	const FRancItemInstance Removed(InstanceToRemoveFrom->ItemInstance.ItemId, ActualRemovedQuantity);
	InstanceToRemoveFrom->ItemInstance.Quantity -= ActualRemovedQuantity;
	if (InstanceToRemoveFrom->ItemInstance.Quantity <= 0)
	{
		TaggedSlotItemInstances.RemoveAt(IndexToRemoveAt);
	}

	UpdateWeight();
	OnItemRemovedFromTaggedSlot.Broadcast(SlotTag, Removed);
	MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, TaggedSlotItemInstances, this);
	return ActualRemovedQuantity;
}

void URancInventoryComponent::MoveItemsToTaggedSlot_Server_Implementation(
	const FRancItemInstance& ItemInstance, FGameplayTag TargetTaggedSlot)
{
	MoveItemsToTaggedSlot_ServerImpl(ItemInstance, TargetTaggedSlot);
}

void URancInventoryComponent::MoveItemsFromTaggedSlot_Server_Implementation(
	const FRancItemInstance& ItemInstance, FGameplayTag SourceTaggedSlot)
{
	MoveItemsFromTaggedSlot_ServerImpl(ItemInstance, SourceTaggedSlot);
}

void URancInventoryComponent::MoveItemsFromAndToTaggedSlot_Server_Implementation(const FRancItemInstance& ItemInstance,
																				 FGameplayTag SourceTaggedSlot, FGameplayTag TargetTaggedSlot)
{
	MoveItemsFromAndToTaggedSlot_ServerImpl(ItemInstance, SourceTaggedSlot, TargetTaggedSlot);
}


int32 URancInventoryComponent::MoveItemsToTaggedSlot_ServerImpl(
	const FRancItemInstance& ItemInstance, FGameplayTag TargetTaggedSlot)
{
	int32 MaxStackSize = URancInventoryFunctions::GetItemDataById(ItemInstance.ItemId)->MaxStackSize;
	int32 ActualQuantity = FMath::Min(GetItemCount(ItemInstance.ItemId),
	                                  FMath::Min(MaxStackSize, ItemInstance.Quantity));

	if (ActualQuantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempted to move item that does not exist or insufficient quantity"));
		return 0;
	}

	if (!IsItemCompatibleWithSlot(ItemInstance.ItemId, TargetTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
		return 0;
	}

	// Attempt to add the item to the targeted slot.

	FRancTaggedItemInstance& ExistingItemAtSlot = const_cast<FRancTaggedItemInstance&>(GetItemForTaggedSlot(
		TargetTaggedSlot));

	if (ExistingItemAtSlot.IsValid())
	{
		if (ExistingItemAtSlot.ItemInstance.ItemId == ItemInstance.ItemId)
		{
			ActualQuantity = FMath::Min(ActualQuantity, MaxStackSize - ExistingItemAtSlot.ItemInstance.Quantity);
			ExistingItemAtSlot.ItemInstance.Quantity += ActualQuantity;
		}
		else
		{
			if (MoveItemsFromTaggedSlot_ServerImpl(ExistingItemAtSlot.ItemInstance, ExistingItemAtSlot.Tag) <
				ExistingItemAtSlot.ItemInstance.Quantity)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to move existing item from tagged slot"));
				return 0;
			}
		}
	}

	const FRancItemInstance ActualItemToMove = FRancItemInstance(ItemInstance.ItemId, ActualQuantity);
	RemoveItems_IfServer(ActualItemToMove);
	AddItemToTaggedSlot_IfServer(TargetTaggedSlot, ActualItemToMove, false);

	return ActualQuantity;
}



int32 URancInventoryComponent::MoveItemsFromTaggedSlot_ServerImpl(
	const FRancItemInstance& ItemInstance, FGameplayTag SourceTaggedSlot)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveItemsFromTaggedSlot_ServerImpl called on non-authority!"));
		return 0;
	}

	// This call will handle validation, ensuring the slot contains the specified item in sufficient quantity.
	int32 QuantityRemoved = RemoveItemsFromTaggedSlot_IfServer(SourceTaggedSlot, ItemInstance.Quantity, true);

	if (QuantityRemoved <= 0)
	{
		// If no items were removed, log and exit.
		UE_LOG(LogTemp, Warning, TEXT("Failed to remove any items from tagged slot or invalid slot/tag."));
		return 0;
	}

	// Add the removed quantity back to the general inventory.
	FRancItemInstance ActualItemToMove(ItemInstance.ItemId, QuantityRemoved);
	AddItems_IfServer(ActualItemToMove);

	// Return the actual quantity of items moved from the tagged slot to the general inventory.
	return QuantityRemoved;
}


int32 URancInventoryComponent::MoveItemsFromAndToTaggedSlot_ServerImpl(const FRancItemInstance& ItemInstance,
                                                                       FGameplayTag SourceTaggedSlot,
                                                                       FGameplayTag TargetTaggedSlot)
{
	if (GetOwnerRole() < ROLE_Authority && GetOwnerRole() != ROLE_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveItemsFromAndToTaggedSlot_ServerImpl called on non-authority!"));
		return 0;
	}
	
	if (!IsItemCompatibleWithSlot(ItemInstance.ItemId, TargetTaggedSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item is not compatible with the target slot"));
		return 0;
	}

	int32 QuantityRemoved = RemoveItemsFromTaggedSlot_IfServer(SourceTaggedSlot, ItemInstance.Quantity, true);
	
	
	if (QuantityRemoved <= 0)
	{
		// If no items were removed, log and exit.
		UE_LOG(LogTemp, Warning, TEXT("Failed to remove any items from tagged slot or invalid slot/tag."));
		return 0;
	}

	const FRancItemInstance ActualItemToMove = FRancItemInstance(ItemInstance.ItemId, QuantityRemoved);
	AddItemToTaggedSlot_IfServer(TargetTaggedSlot, ActualItemToMove, false);

	return QuantityRemoved;
}

int32 URancInventoryComponent::DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, float DropAngle)
{
	DropFromTaggedSlot_Server_Implementation(SlotTag, Quantity, DropAngle);

	// On client the below is just a guess
	const FRancTaggedItemInstance Item = GetItemForTaggedSlot(SlotTag);
	int32 QuantityToDrop = FMath::Min(Quantity, Item.ItemInstance.Quantity);

	return QuantityToDrop;
}

void URancInventoryComponent::DropFromTaggedSlot_Server_Implementation(const FGameplayTag& SlotTag, int32 Quantity,
                                                                       float DropAngle)
{
	const FRancTaggedItemInstance Item = GetItemForTaggedSlot(SlotTag);
	if (Item.Tag.IsValid())
	{
		const int32 QuantityToDrop = FMath::Min(Quantity, Item.ItemInstance.Quantity);

		if (QuantityToDrop == Item.ItemInstance.Quantity)
		{
			RemoveItemsFromTaggedSlot_IfServer(SlotTag, QuantityToDrop);
		}

		if (QuantityToDrop > 0)
		{
			const FRancItemInstance ItemToDrop(Item.ItemInstance.ItemId, QuantityToDrop);
			// ReSharper disable once CppExpressionWithoutSideEffects
			SpawnDroppedItem_IfServer(ItemToDrop);
		}
	}
}


const FRancTaggedItemInstance& URancInventoryComponent::GetItemForTaggedSlot(const FGameplayTag& SlotTag) const
{
	// loop over SpecialSlotItems
	for (int i = 0; i < TaggedSlotItemInstances.Num(); i++)
	{
		if (TaggedSlotItemInstances[i].Tag == SlotTag)
			return TaggedSlotItemInstances[i];
	}

	return FRancTaggedItemInstance::EmptyItemInstance;
}


TArray<FRancTaggedItemInstance> URancInventoryComponent::GetAllTaggedItems() const
{
	return TaggedSlotItemInstances;
}

void URancInventoryComponent::DetectAndPublishChanges()
{
	// First pass: Update existing items or add new ones, mark them by setting quantity to negative.
	for (FRancTaggedItemInstance& NewItem : TaggedSlotItemInstances)
	{
		FRancItemInstance* OldItem = TaggedItemsCache.Find(NewItem.Tag);

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
						                                  FRancItemInstance(
							                                  NewItem.ItemInstance.ItemId,
							                                  NewItem.ItemInstance.Quantity - OldItem->Quantity));
					}
					else if (OldItem->Quantity > NewItem.ItemInstance.Quantity)
					{
						OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag,
						                                      FRancItemInstance(
							                                      NewItem.ItemInstance.ItemId,
							                                      OldItem->Quantity - NewItem.ItemInstance.Quantity));
					}
				}
				else // Item has changed
				{
					OnItemRemovedFromTaggedSlot.Broadcast(NewItem.Tag,
					                                      FRancItemInstance(OldItem->ItemId, OldItem->Quantity));
					OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,
					                                  FRancItemInstance(NewItem.ItemInstance.ItemId,
					                                                    NewItem.ItemInstance.Quantity));
				}
			}

			// Mark this item as processed by temporarily setting its value to its own negative
			OldItem->Quantity = -NewItem.ItemInstance.Quantity;
		}
		else // New slot has been added to
		{
			OnItemAddedToTaggedSlot.Broadcast(NewItem.Tag,
			                                  FRancItemInstance(NewItem.ItemInstance.ItemId,
			                                                    NewItem.ItemInstance.Quantity));
			TaggedItemsCache.Add(NewItem.Tag,
			                     FRancItemInstance(NewItem.ItemInstance.ItemId, -NewItem.ItemInstance.Quantity));
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

bool URancInventoryComponent::IsItemCompatibleWithSlot(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const
{
	if (UniversalTaggedSlots.Contains(SlotTag))
		return true;
	
	const URancItemData* ItemData = URancInventoryFunctions::GetItemDataById(ItemId);
	if (!ItemData)
	{
		return false; // Item data not found
	}

	// Check if the item's categories contain the slot tag
	return ItemData->ItemCategories.HasTag(SlotTag);
}

void URancInventoryComponent::OnRep_Slots()
{
	UpdateWeight();
	DetectAndPublishChanges();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// CRAFTING /////////////////////////////////////////////////////////////////

bool URancInventoryComponent::CanCraftRecipeId(const FPrimaryAssetId& RecipeId) const
{
	const URancRecipe* Recipe = Cast<URancRecipe>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(Recipe);
}

bool URancInventoryComponent::CanCraftRecipe(const URancRecipe* Recipe) const
{
	if (!Recipe) return false;

	for (const auto& Component : Recipe->Components)
	{
		if (!ContainsItems(Component.ItemId, Component.Quantity))
		{
			return false;
		}
	}
	return true;
}

bool URancInventoryComponent::CanCraftCraftingRecipe(const FPrimaryAssetId& RecipeId) const
{
	URancItemRecipe* CraftingRecipe = Cast<URancItemRecipe>(
		UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	return CanCraftRecipe(CraftingRecipe);
}

void URancInventoryComponent::CraftRecipeId_Server_Implementation(const FPrimaryAssetId& RecipeId)
{
	const URancRecipe* Recipe = Cast<URancRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
	CraftRecipe_IfServer(Recipe);
}

bool URancInventoryComponent::CraftRecipe_IfServer(const URancRecipe* Recipe)
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
			RemoveItems_IfServer(Component);
		}
		bSuccess = true;

		OnCraftConfirmed.Broadcast(Recipe->ResultingObject, Recipe->QuantityCreated);
	}
	return bSuccess;
}

void URancInventoryComponent::SetRecipeLock_Server_Implementation(const FPrimaryAssetId& RecipeId, bool LockState)
{
	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
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
}

URancRecipe* URancInventoryComponent::GetRecipeById(const FPrimaryAssetId& RecipeId)
{
	return Cast<URancRecipe>(UAssetManager::GetIfInitialized()->GetPrimaryAssetObject(RecipeId));
}

TArray<URancRecipe*> URancInventoryComponent::GetAvailableRecipes(FGameplayTag TagFilter)
{
	return CurrentAvailableRecipes.Contains(TagFilter) ? CurrentAvailableRecipes[TagFilter] : TArray<URancRecipe*>();
}

void URancInventoryComponent::CheckAndUpdateRecipeAvailability()
{
	// Clear current available recipes
	CurrentAvailableRecipes.Empty();

	// Iterate through all available recipes and check if they can be crafted
	for (const FPrimaryAssetId& RecipeId : AllUnlockedRecipes)
	{
		URancRecipe* Recipe = GetRecipeById(RecipeId);
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

int32 URancInventoryComponent::DropAllItems_ServerImpl()
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


void URancInventoryComponent::OnInventoryItemAddedHandler(const FRancItemInstance& ItemInfo)
{
	CheckAndUpdateRecipeAvailability();
}

void URancInventoryComponent::OnInventoryItemRemovedHandler(const FRancItemInstance& ItemInfo)
{
	CheckAndUpdateRecipeAvailability();
}

void URancInventoryComponent::OnRep_Recipes()
{
	CheckAndUpdateRecipeAvailability();
}
