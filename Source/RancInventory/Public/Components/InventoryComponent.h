// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "ItemContainerComponent.h"
#include "Core/RISSubsystem.h"
#include "InventoryComponent.generated.h"

USTRUCT(Blueprintable)
struct FUniversalTaggedSlot
{
	GENERATED_BODY()

	FUniversalTaggedSlot() : Slot(FGameplayTag::EmptyTag), UniversalSlotToBlock(FGameplayTag::EmptyTag), RequiredItemCategoryToBlock(FGameplayTag::EmptyTag) {}
	FUniversalTaggedSlot(FGameplayTag InSlot) : Slot(InSlot), UniversalSlotToBlock(FGameplayTag::EmptyTag), RequiredItemCategoryToBlock(FGameplayTag::EmptyTag) {}
	FUniversalTaggedSlot(FGameplayTag InSlot, FGameplayTag InSlotToBlock, FGameplayTag InRequiredItemCategoryToBlock)
		: Slot(InSlot), UniversalSlotToBlock(InSlotToBlock), RequiredItemCategoryToBlock(InRequiredItemCategoryToBlock) {}
	FUniversalTaggedSlot(FGameplayTag InSlot, FGameplayTag InSlotToBlock, FGameplayTag InRequiredItemCategoryToBlock, FGameplayTag InExclusiveToSlotCategory)
		: Slot(InSlot), UniversalSlotToBlock(InSlotToBlock), RequiredItemCategoryToBlock(InRequiredItemCategoryToBlock), ExclusiveToSlotCategory(InExclusiveToSlotCategory) {}
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	FGameplayTag Slot;
	
	// If this is set then the given slot will be blocked when this slot is filled, depending on the condition below
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	FGameplayTag UniversalSlotToBlock;

	// This is a conditional item category to look for in THIS slots item to determine whether we should block SlotToBlock
	// If it is not set and SlotToBlock is, then we will block SlotToBlock always
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	FGameplayTag RequiredItemCategoryToBlock;

	
	// If an item has this category then it can only enter this specific universal slot and not any other universal slots
	// E.g. Items.Categories.TwoHanded might be exclusive to the right hand slot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	FGameplayTag ExclusiveToSlotCategory;
	
	bool IsValid() const
	{
		return Slot.IsValid();
	}
};

UENUM(BlueprintType)
enum class EPreferredSlotPolicy : uint8
{
	PreferGenericInventory,
	PreferSpecializedTaggedSlot,
	PreferAnyTaggedSlot
};


/* The high level idea is to NOT store where specific items are, we only store the total quantity of each item
 * A "Viewmodel" class is then responsible for displaying items in the players chosen slots. This is for network and server performance reasons.
 * To support jigsaw/diablo style inventories, i looked into various ways validate whether an item can be added but its a NP-hard problem
 * Solution 1: The client send a solution to the server when requesting but disallow server initiated additions
 * Solution 2: Synchronize specific layout only for jigsaw inventories, but requires a lot of RPCs and state
 * Solution 3: Run the expensive perfect algos on server async but the algos can take several seconds
 * Chosen solution: "Liquify" all items for validation purposes so we only need to check slot counts which potentially allows some insignficant cheating
 */
/* A container of items containing a number of uniform "slots" that can hold items, e.g. a backpack or a chest */
UCLASS(Blueprintable, ClassGroup = (Custom), Category = "RIS | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API UInventoryComponent : public UItemContainerComponent
{
	GENERATED_BODY()

public:
	explicit UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitializeComponent() override;

	////////////////// TAGGED SLOTS ///////////////////

	// Add an item to a tagged slot, if the slot is already occupied it will return the quantity that was added. Allows partial
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment", Meta = (HidePin="OverrideExistingItem"))
	int32 AddItemToTaggedSlot_IfServer(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& SlotTag, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial = true);

	/* Attempts to add an item to a generic or tagged slot, PreferTaggedSlots determines which is tried first.
	 * Typically only called on server but if called on client it can be used as a form of client prediction
	 * It may distribute items over several slots, e.g. 5 rocks in left hand and the remaining 3 in generic inventory
	 * If PreferTaggedSlots is true, an item with category e.g. HelmetSlot will go into HelmetSlot first
	 * Returns amount added, and always allows partial adding */
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 AddItemToAnySlot(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool AllowPartial = false);
	
	// Remove up to Quantity item from a tagged slot, will return the count that was removed
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag SlotTag, int32 QuantityToRemove, EItemChangeReason Reason, bool AllowPartial = true, bool DestroyFromContainer = true, bool SkipWeightUpdate = false);

	// Remove up to Quantity item from any tagged slot, will return the count that was removed, always allows partial removal
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 RemoveItemFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove, EItemChangeReason Reason, bool DestroyFromContainer = true);

	/**
	 * Attempts to clear a specific tagged slot by moving its contents to the generic inventory container.
	 * This will only succeed if the generic container has enough space (weight/slots) to accept the item.
	 * Does nothing if the slot is already empty. Only executes fully on the server.
	 * @param SlotTag The tagged slot to clear.
	 * @return The quantity of the item successfully moved out of the tagged slot and into the generic container. Returns 0 if the slot was empty or the move failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 RemoveAnyItemFromTaggedSlot_IfServer(FGameplayTag SlotTag);
	
	/* Attempts to activate the item from the inventory, e.g. use a potion or active a magical item	 */
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 UseItemFromTaggedSlot(const FGameplayTag& SlotTag);

	// Checks weight and count limits and compatability
	UFUNCTION(BlueprintCallable, Category="Inventory Mapping")
	bool CanTaggedSlotReceiveItem(const FItemBundle& ItemInfo, const FGameplayTag& SlotTag) const;

	UFUNCTION(BlueprintPure, Category="Inventory Mapping")
	int32 GetQuantityOfItemTaggedSlotCanReceive(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "RIS")
	int32 GetItemQuantityTotal(const FGameplayTag& ItemId) const;

	//  Returns if the item can be added to teh slot, does NOT check if the slot is occupied
	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	bool IsTaggedSlotCompatible(const FGameplayTag& ItemId, const FGameplayTag& SlotTag) const;

	//  Returns if the item can be added to teh slot, does NOT check if the slot is occupied
	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	bool IsItemInTaggedSlotValid(const FGameplayTag SlotTag) const;
	
	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	const FTaggedItemBundle& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;

	// Allows you to set a certain tagged slot to be blocked, e.g. a two handed weapon might block the offhand slot
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	void SetTaggedSlotBlocked(FGameplayTag Slot, bool IsBlocked);

	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	bool CanItemBeEquippedInUniversalSlot(const FGameplayTag& ItemId, const FUniversalTaggedSlot& Slot, bool IgnoreBlocking = false) const;

	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	bool IsTaggedSlotBlocked(const FGameplayTag& Slot) const;

	// New events for slot equipment changes, this also gets called if an already held stackable item has its stack quantity increased
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnItemAddedToTaggedSlot, const FGameplayTag&, SlotTag, const UItemStaticData*, ItemData, int32, Quantity, FTaggedItemBundle, PreviousItem, EItemChangeReason, Reason);
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnItemAddedToTaggedSlot OnItemAddedToTaggedSlot;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnItemRemovedFromTaggedSlot, const FGameplayTag&, SlotTag, const UItemStaticData*, ItemData, int32, Quantity, EItemChangeReason, Reason);
	// Note: This also gets called for partial removing, e.g. moving 1 apple in a stack of 5
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnItemRemovedFromTaggedSlot OnItemRemovedFromTaggedSlot;

	// Which tagged but universal slots are available, e.g. left/right hand
	// These can be configured to block other universal slots, e.g. a two handed weapon in mainhand might block offhand
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	TArray<FUniversalTaggedSlot> UniversalTaggedSlots;
	
	/* Which specialized slots are available that hold only specific items, e.g. helmet
	 * These are not actually different from UniversalSpecialSlots in this component but are used by e.g. SlotMapper 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	TArray<FGameplayTag> SpecializedTaggedSlots; // E.g., head, feet

	UFUNCTION()
	void OnInventoryItemAddedHandler(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);
	UFUNCTION()
	void OnInventoryItemRemovedHandler(const UItemStaticData* ItemData, int32 Quantity, EItemChangeReason Reason);

	////////////////// CRAFTING ///////////////////

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftRecipeId(const FPrimaryRISRecipeId& RecipeId) const;

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftRecipe(const UObjectRecipeData* Recipe) const;

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftCraftingRecipe(const FPrimaryRISRecipeId& RecipeId) const;
	
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RIS | Crafting")
	void CraftRecipeId_Server(const FPrimaryRISRecipeId& RecipeId);

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CraftRecipe_IfServer(const UObjectRecipeData* Recipe);


	UFUNCTION(BlueprintPure, Category = "RIS")
	TArray<FTaggedItemBundle> GetAllTaggedItems() const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RIS | Recipes")
	void SetRecipeLock_Server(const FPrimaryRISRecipeId& RecipeId, bool LockState);

	UFUNCTION(BlueprintCallable, Category = "RIS | Recipes")
	UObjectRecipeData* GetRecipeById(const FPrimaryRISRecipeId& RecipeId);

	// Available recipes are ones that are unlocked and for which we have the necessary materials
	UFUNCTION(BlueprintCallable, Category = "RIS | Recipes")
	TArray<UObjectRecipeData*> GetAvailableRecipes(FGameplayTag TagFilter);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftConfirmed, TSubclassOf<UObject>, CraftedClass, int32, QuantityCrafted);

	// Note: This does NOT get called for item crafting, those are automatically added to inventory
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnCraftConfirmed OnCraftConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAvailableRecipesUpdated);

	// Note: This does NOT get called for item crafting, those are automatically added to inventory
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnAvailableRecipesUpdated OnAvailableRecipesUpdated;

	// The groups of recipes that are relevant for this inventory component, e.g. "Items" and "Buildings"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS | Recipes")
	FGameplayTagContainer RecipeTagFilters;

	// All possible recipes, not just ones we can craft
	UPROPERTY(ReplicatedUsing=OnRep_Recipes, EditAnywhere, BlueprintReadOnly, Category = "RIS | Recipes")
	TArray<FPrimaryRISRecipeId> AllUnlockedRecipes;
	
	
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	void PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool DestroyAfterPickup = true);
	
	UFUNCTION(BlueprintCallable, Category = "RIS")
	int32 MoveItem(const FGameplayTag& ItemId, int32 Quantity,
					const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
					const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
					const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);


	// Runs the code of MoveItem but does not actually move the item, useful for validations pre-move
	int32 ValidateMoveItem(const FGameplayTag& ItemId, int32 Quantity,
					const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
					const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
					const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);

	/* Attempts to drop the item from the inventory, attempting to spawn an Item object in the world
	 * Specify DropItemClass and DropDistance properties to customize the drop
	 * Called on client it will request the drop on the server
	 * Returns the quantity actually dropped, on client this is only a "guess" */
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, FVector RelativeDropLocation = FVector(1e+300, 0,0));

	bool ContainedInUniversalSlot(const FGameplayTag& TagToFind) const;

	// Returns null if no blocking or returns the slot causing the block if blocking is occuring
	const FUniversalTaggedSlot* WouldItemMoveIndirectlyViolateBlocking(const FGameplayTag& TaggedSlot, const UItemStaticData* ItemData) const;

protected:
	void CheckAndUpdateRecipeAvailability();
	
	/* Attempts to pickup a world item, if successfull and fully picked up the object is destroyed
	 * If AllowPartial is true, will only pick up as much as possible. */
	UFUNCTION(Server, Reliable, Category = "RIS | Equipment")
	void PickupItem_Server(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool DestroyAfterPickup = true);

	
	
	/* Do NOT call this directly
	 * Moves items from one tagged slot to another, always allows partial moves but source must contain right amount
	 * Leave SourceTaggedSlot as empty tag to move from container or leave TargetTaggedSlot empty to move to container */
	UFUNCTION(Server, Reliable, Category = "RIS")
	void MoveItem_Server(const FGameplayTag& ItemId, int32 Quantity,
						  const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
						  const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
						  const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);
	


	/*
	 * Move items from one slot to another
	 * @param ItemId The item id to move
	 * @param RequestedQuantity The quantity to move
	 * @param SourceTaggedSlot The slot to move from, if empty will move from container
	 * @param TargetTaggedSlot The slot to move to, if empty will move to container
	 * @param AllowAutomaticSwapping If true, will attempt to swap with another item if the target slot is occupied
	 * @param SwapItemId Item that will swap back into source, must match the target item if specified
	 * @param SwapQuantity Quantity to swap back, must match the target item quantity if specified
	 * @return The quantity actually moved
	 */
	int32 MoveItem_ServerImpl(const FGameplayTag& ItemId, int32 RequestedQuantity,
							   const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
							   const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
							   bool AllowAutomaticSwapping = true,
							   const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0,
							   bool SuppressUpdate = false, bool SimulateMoveOnly = false);
	
	UFUNCTION(Server, Reliable)
	void DropFromTaggedSlot_Server(const FGameplayTag& SlotTag, int32 Quantity, FVector RelativeDropLocation = FVector(1e+300, 0,0));

	UFUNCTION(Server, Reliable)
	void UseItemFromTaggedSlot_Server(const FGameplayTag& SlotTag);

	virtual void UpdateWeightAndSlots() override;

	int32 GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const;

	// Container overrides
	virtual int32 AddItem_ServerImpl(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial,
												 bool SuppressUpdate = false) override;
	virtual int32 DropAllItems_ServerImpl() override;
	virtual int32 DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial = false, bool UpdateAfter = true, bool SendEventAfter = true) override;
	virtual int32 GetContainerOnlyItemQuantityImpl(const FGameplayTag& ItemId) const override;
	virtual bool ContainsImpl(const FGameplayTag& ItemId, int32 Quantity = 1) const override;
	virtual void ClearImpl() override;
	virtual int32 GetReceivableQuantityImpl(const FGameplayTag& ItemId) const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual int32 ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool SuppressUpdate) override;
	int32 ExtractItemFromTaggedSlot_IfServer(const FGameplayTag& TaggedSlot, const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);

	void UpdateBlockingState(FGameplayTag SlotTag, const UItemStaticData* ItemData, bool IsEquip);


	UPROPERTY(ReplicatedUsing=OnRep_Slots, BlueprintReadOnly, Category = "RIS")
	TArray<FTaggedItemBundle> TaggedSlotItemInstances;
	
	TMap<FGameplayTag, TArray<UObjectRecipeData*>> CurrentAvailableRecipes;

	void DetectAndPublishContainerChanges();


	//// Client Prediction and Rollback ////
	
	TMap<FGameplayTag, FItemBundle> CachedTaggedSlotItems;
	
private:
	UPROPERTY()
	URISSubsystem* Subsystem;
	TArray<FGameplayTag> _SlotsToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.
	// The cache is a copy of Items that is not replicated, used to detect changes after replication, only used on client
	TMap<FGameplayTag, FItemBundle> TaggedItemsCache; // Slot to quantity;
	
	TArray<std::tuple<FGameplayTag, int32>> GetItemDistributionPlan(const UItemStaticData* ItemData, int32 Quantity, EPreferredSlotPolicy PreferTaggedSlots);

	void SortUniversalTaggedSlots();
	
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_Recipes();

	friend class UGridInventoryViewModel;
};