#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "RancItemContainerComponent.h"
#include "RancInventoryComponent.generated.h"


UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API URancInventoryComponent : public URancItemContainerComponent
{
	GENERATED_BODY()

public:
	explicit URancInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeComponent() override;

	////////////////// TAGGED SLOTS ///////////////////

	// Add an item to a tagged slot, if the slot is already occupied it will return the quantity that was added. Allows partial when stacking
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Equipment", Meta = (HidePin="OverrideExistingItem"))
	int32 AddItemsToTaggedSlot_IfServer(const FGameplayTag& SlotTag, const FRancItemInstance& ItemsToAdd,
	                                    bool OverrideExistingItem = false);

	/* Attempts to add an item to a generic or tagged slot, PreferTaggedSlots determines which is tried first.
	 * It may distribute items over several slots, e.g. 5 rocks in left hand and the remaining 3 in generic inventory
	 * If PreferTaggedSlots is true, an item with category e.g. HelmetSlot will go into HelmetSlot first
	 * Returns amount added, and always allows partial adding */
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Equipment")
	int32 AddItemsToAnySlots_IfServer(FRancItemInstance ItemsToAdd, bool PreferTaggedSlots = true);

	// Remove up to Quantity item from a tagged slot, will return the count that was removed
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Equipment")
	int32 RemoveQuantityFromTaggedSlot_IfServer(const FGameplayTag& SlotTag, int32 QuantityToRemove, bool AllowPartial = true);

	// Remove up to Quantity item from any tagged slot, will return the count that was removed, always allows partial removal
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Equipment")
	int32 RemoveItemsFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove);


	/* Moves items from one tagged slot to another, always allows partial moves but source must contain right amount
	 * Leave SourceTaggedSlot as empty tag to move from container or leave TargetTaggedSlot empty to move to container */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Ranc Inventory")
	void MoveItems_Server(const FRancItemInstance& ItemInstance,
							   const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
							   const FGameplayTag& TargetTaggedSlot = FGameplayTag());
	
	int32 MoveItems_ServerImpl(const FRancItemInstance& ItemInstance,
	                           const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
	                           const FGameplayTag& TargetTaggedSlot = FGameplayTag());
	
	/* Attempts to drop the item from the inventory, attempting to spawn an Item object in the world
	 * Specify DropItemClass and DropDistance properties to customize the drop
	 * Called on client it will request the drop on the server
	 * Returns the quantity actually dropped, on client this is only a "guess" */
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Equipment")
	int32 DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, float DropAngle = 0);

	// Checks weight and count limits and compatability
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool CanTaggedSlotReceiveItem(const FRancItemInstance& ItemInfo, const FGameplayTag& SlotTag) const;
	
	UFUNCTION(BlueprintPure, Category="Ranc Inventory")
	int32 GetItemCountIncludingTaggedSlots(const FGameplayTag& ItemId) const;

	//  Returns if the item can be added to teh slot, does NOT check if the slot is occupied
	UFUNCTION(BlueprintPure, Category="Ranc Inventory | Equipment")
	bool IsTaggedSlotCompatible(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const;

	UFUNCTION(BlueprintPure, Category="Ranc Inventory | Equipment")
	const FRancTaggedItemInstance& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;

	// Removes all items from generic and tagged slots, and publishes the removals
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
	void ClearInventory_IfServer();
	
	// New events for slot equipment changes, this also gets called if an already held stackable item has its stack quantity increased
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAddedToTaggedSlot, const FGameplayTag&, SlotTag, const FRancItemInstance&, ItemInfo);

	UPROPERTY(BlueprintAssignable, Category="Ranc Inventory | Equipment")
	FOnItemAddedToTaggedSlot OnItemAddedToTaggedSlot;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemovedFromTaggedSlot, const FGameplayTag&, SlotTag, const FRancItemInstance&, ItemInfo);

	// Note: This also gets called for partial removing, e.g. moving 1 apple in a stack of 5
	UPROPERTY(BlueprintAssignable, Category="Ranc Inventory | Equipment")
	FOnItemRemovedFromTaggedSlot OnItemRemovedFromTaggedSlot;

	// Which special but universal slots are available, e.g. left/right hand
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ranc Inventory | Equipment")
	TArray<FGameplayTag> UniversalTaggedSlots; // E.g., hands

	/* Which specialized slots are available that hold only specific items, e.g. helmet
	 * These are not actually different from UniversalSpecialSlots in this component but are used by e.g. SlotMapper 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ranc Inventory | Equipment")
	TArray<FGameplayTag> SpecializedTaggedSlots; // E.g., head, feet

	UFUNCTION()
	void OnInventoryItemAddedHandler(const FRancItemInstance& ItemInfo);
	UFUNCTION()
	void OnInventoryItemRemovedHandler(const FRancItemInstance& ItemInfo);

	////////////////// CRAFTING ///////////////////

	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
	bool CanCraftRecipeId(const FPrimaryAssetId& RecipeId) const;

	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
	bool CanCraftRecipe(const URancRecipe* Recipe) const;

	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
	bool CanCraftCraftingRecipe(const FPrimaryAssetId& RecipeId) const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Ranc Inventory | Crafting")
	void CraftRecipeId_Server(const FPrimaryAssetId& RecipeId);

	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Crafting")
	bool CraftRecipe_IfServer(const URancRecipe* Recipe);


	UFUNCTION(BlueprintPure, Category="Ranc Inventory")
	TArray<FRancTaggedItemInstance> GetAllTaggedItems() const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Ranc Inventory | Recipes")
	void SetRecipeLock_Server(const FPrimaryAssetId& RecipeId, bool LockState);

	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Recipes")
	URancRecipe* GetRecipeById(const FPrimaryAssetId& RecipeId);

	// Available recipes are ones that are unlocked and for which we have the necessary materials
	UFUNCTION(BlueprintCallable, Category="Ranc Inventory | Recipes")
	TArray<URancRecipe*> GetAvailableRecipes(FGameplayTag TagFilter);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftConfirmed, TSubclassOf<UObject>, CraftedClass, int32, QuantityCrafted);

	// Note: This does NOT get called for item crafting, those are automatically added to inventory
	UPROPERTY(BlueprintAssignable, Category="Ranc Inventory | Equipment")
	FOnCraftConfirmed OnCraftConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAvailableRecipesUpdated);

	// Note: This does NOT get called for item crafting, those are automatically added to inventory
	UPROPERTY(BlueprintAssignable, Category="Ranc Inventory | Equipment")
	FOnAvailableRecipesUpdated OnAvailableRecipesUpdated;

	// The groups of recipes that are relevant for this inventory component, e.g. "Items" and "Buildings"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ranc Inventory | Recipes")
	FGameplayTagContainer RecipeTagFilters;

	// All possible recipes, not just ones we can craft
	UPROPERTY(ReplicatedUsing=OnRep_Recipes, EditAnywhere, BlueprintReadOnly, Category="Ranc Inventory | Recipes")
	TArray<FPrimaryAssetId> AllUnlockedRecipes;

protected:
	void CheckAndUpdateRecipeAvailability();

	virtual int32 DropAllItems_ServerImpl() override;

	UFUNCTION(Server, Reliable)
	void DropFromTaggedSlot_Server(const FGameplayTag& SlotTag, int32 Quantity, float DropAngle = 0);

	virtual void UpdateWeightAndSlots() override;

	virtual bool ContainsItemsImpl(const FGameplayTag& ItemId, int32 Quantity) const override;

	int32 GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_Slots, BlueprintReadOnly, Category="Ranc Inventory")
	TArray<FRancTaggedItemInstance> TaggedSlotItemInstances;

	TMap<FGameplayTag, TArray<URancRecipe*>> CurrentAvailableRecipes;

private:
	TArray<FGameplayTag> _SlotsToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.
	// The cache is a copy of Items that is not replicated, used to detect changes after replication, only used on client
	TMap<FGameplayTag, FRancItemInstance> TaggedItemsCache; // Slot to quantity;
	void DetectAndPublishChanges();

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_Recipes();

	friend class URancInventorySlotMapper;
};
