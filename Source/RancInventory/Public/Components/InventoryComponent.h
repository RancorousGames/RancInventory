// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>
#include "ItemContainerComponent.h"
#include "Core/RISSubsystem.h"
#include "InventoryComponent.generated.h"

// Forward declarations
class UObjectRecipeData;
struct FPrimaryRISRecipeId;

USTRUCT(Blueprintable)
struct FUniversalTaggedSlot
{
	GENERATED_BODY()

	FUniversalTaggedSlot() : Slot(FGameplayTag::EmptyTag), UniversalSlotToBlock(FGameplayTag::EmptyTag), RequiredItemCategoryToActivateBlocking(FGameplayTag::EmptyTag) {}
	FUniversalTaggedSlot(FGameplayTag InSlot) : Slot(InSlot), UniversalSlotToBlock(FGameplayTag::EmptyTag), RequiredItemCategoryToActivateBlocking(FGameplayTag::EmptyTag) {}
	FUniversalTaggedSlot(FGameplayTag InSlot, FGameplayTag InSlotToBlock, FGameplayTag InRequiredItemCategoryToBlock)
		: Slot(InSlot), UniversalSlotToBlock(InSlotToBlock), RequiredItemCategoryToActivateBlocking(InRequiredItemCategoryToBlock) {}
	FUniversalTaggedSlot(FGameplayTag InSlot, FGameplayTag InSlotToBlock, FGameplayTag InRequiredItemCategoryToBlock, FGameplayTag InExclusiveToSlotCategory)
		: Slot(InSlot), UniversalSlotToBlock(InSlotToBlock), RequiredItemCategoryToActivateBlocking(InRequiredItemCategoryToBlock), ExclusiveToSlotCategory(InExclusiveToSlotCategory) {}


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	FGameplayTag Slot;

	// If this is set then the given slot will be blocked when this slot is filled, depending on the condition below
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	FGameplayTag UniversalSlotToBlock;

	// This is a conditional item category to look for in THIS slots item to determine whether we should block SlotToBlock
	// If it is not set and SlotToBlock is, then we will block SlotToBlock always
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
	FGameplayTag RequiredItemCategoryToActivateBlocking;


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
	
	// == QUERY ==
	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	const FTaggedItemBundle& GetItemForTaggedSlot(const FGameplayTag& SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "RIS | Equipment")
	bool IsTaggedSlotBlocked(const FGameplayTag& Slot) const;

	UFUNCTION(BlueprintPure, Category="RIS|Validation", Meta = (DisplayName = "ContainsInTaggedSlot"))
	bool ContainsInTaggedSlot_BP(const FGameplayTag& SlotTag,	const FGameplayTag& ItemId,	int32 Quantity) const;

	// Quantity is ignored if InstancesToLookFor is not empty
	virtual bool ContainsInTaggedSlot(const FGameplayTag& SlotTag,	const FGameplayTag& ItemId,
		int32 Quantity,	const TArray<UItemInstanceData*>& InstancesToLookFor) const;

	UFUNCTION(BlueprintPure, Category = "RIS")
	TArray<FTaggedItemBundle> GetAllTaggedItems() const;

	UFUNCTION(BlueprintPure, Category=RIS)
	int32 GetContainerOnlyItemQuantity(const FGameplayTag& ItemId) const;
	
	UFUNCTION(BlueprintPure, Category="RIS|Validation") 
	bool CanReceiveItemInTaggedSlot(const FGameplayTag& ItemId, int32 QuantityToReceive, const FGameplayTag& TargetTaggedSlot, bool SwapBackAllowed) const;

	// Includes tagged slots
	virtual int32 GetReceivableQuantity(const UItemStaticData* ItemData, int32 RequestedQuantity = 0x7fffffff, bool AllowPartial = true, bool SwapBackAllowed = false) const override;
	
	int32 GetReceivableQuantityContainerOnly(const UItemStaticData* ItemData, int32 RequestedQuantity = 0x7fffffff, bool AllowPartial = true, bool SwapBackAllowed = false) const;

	int32 GetReceivableQuantityForTaggedSlot(const UItemStaticData* ItemData, const FGameplayTag& TargetTaggedSlot, int32 RequestedQuantity = 0x7fffffff, bool AllowPartial = true, bool AllowSwapback = false) const;

	const FUniversalTaggedSlot* WouldItemMoveIndirectlyViolateBlocking(const FGameplayTag& TaggedSlot, const UItemStaticData* ItemData) const;
	
	// == ADD ITEMS (Tagged Slots) ==

	/* Adds an item to a specified tagged slot */
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment", Meta = (HidePin="OverrideExistingItem"))
	int32 AddItemToTaggedSlot_IfServer(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& SlotTag, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial = true, bool PushOutExistingItem = true);

	// == REMOVE/USE/MOVE ITEMS (Tagged Slots - Client Interface) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 UseItemFromTaggedSlot(const FGameplayTag& SlotTag, int32 ItemToUseInstanceId = -1);
	
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 DropFromTaggedSlot(const FGameplayTag& SlotTag, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));

	// == REMOVE/MODIFY ITEMS (Tagged Slots - Server-Side Execution) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment", meta = (AutoCreateRefTerm = "MyOptionalParam")) // InstancesToRemove was MyOptionalParam
	int32 RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag SlotTag, int32 QuantityToRemove, const TArray<UItemInstanceData*>& InstancesToRemove, EItemChangeReason Reason, bool AllowPartial = true, bool DestroyFromContainer = true, bool SuppressEvents = false, bool SuppressUpdate = false);

	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 RemoveItemFromAnyTaggedSlots_IfServer(FGameplayTag ItemId, int32 QuantityToRemove, TArray<UItemInstanceData*> InstancesToRemove, EItemChangeReason Reason, bool DestroyFromContainer = true, bool SuppressEvents = false, bool SuppressUpdate = false);
	
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 RemoveAnyItemFromTaggedSlot_IfServer(FGameplayTag SlotTag);

	// == MANAGEMENT (Tagged Slots) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	void SetTaggedSlotBlocked(FGameplayTag Slot, bool IsBlocked);


	// == GENERAL INVENTORY OPERATIONS (Combined Generic & Tagged Slots) ==

	// --- Add Items ---
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	int32 AddItemToAnySlot(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool AllowPartial = false, bool SuppressEvents = false, bool SuppressUpdate = false);
	
	virtual int32 AddItemWithInstances_IfServer(TScriptInterface<IItemSource> ItemSource,
		const FGameplayTag& ItemId,
		int32 RequestedQuantity,
		const TArray<UItemInstanceData*>& InstancesToExtract,
		bool AllowPartial = false,
		bool SuppressEvents = false,
		bool SuppressUpdate = false) override;

	// --- Remove/Move Items (Client Interface) ---

	/*
	 * Moves an item from tagged or generic slot to another tagged or generic slot
	 * Always allows partial
	 */
	UFUNCTION(BlueprintCallable, Category = "RIS")
	int32 MoveItem(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToMove,
					const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
					const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
					const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);


	////////////////// CRAFTING ///////////////////
	// == QUERY (Crafting) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftRecipeId(const FPrimaryRISRecipeId& RecipeId) const;

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftRecipe(const UObjectRecipeData* Recipe) const;

	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CanCraftCraftingRecipe(const FPrimaryRISRecipeId& RecipeId) const;
	
	UFUNCTION(BlueprintCallable, Category = "RIS | Recipes")
	UObjectRecipeData* GetRecipeById(const FPrimaryRISRecipeId& RecipeId);

	UFUNCTION(BlueprintCallable, Category = "RIS | Recipes")
	TArray<UObjectRecipeData*> GetAvailableRecipes(FGameplayTag TagFilter);

	// == ACTIONS (Crafting - Client Interface calls Server RPCs) ==
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RIS | Crafting")
	void CraftRecipeId_Server(const FPrimaryRISRecipeId& RecipeId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "RIS | Recipes")
	void SetRecipeLock_Server(const FPrimaryRISRecipeId& RecipeId, bool LockState);

	// == ACTIONS (Crafting - Server-Side Execution) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Crafting")
	bool CraftRecipe_IfServer(const UObjectRecipeData* Recipe);


	////////////////// ACTIONS ///////////////////
	// == CLIENT INTERFACE (Other Actions) ==
	UFUNCTION(BlueprintCallable, Category = "RIS | Equipment")
	void PickupItem(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool DestroyAfterPickup = true);
	

	////////////////// VALIDATION ///////////////////
	// == OTHER VALIDATION ==
	int32 ValidateMoveItem(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToMove,
					const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
					const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
					const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);


	// == BASE CONTAINER EVENT HANDLERS ==
	UFUNCTION()
	void OnInventoryItemAddedHandler(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesAdded, EItemChangeReason Reason);
	UFUNCTION()
	void OnInventoryItemRemovedHandler(const UItemStaticData* ItemData, int32 Quantity, const TArray<UItemInstanceData*>& InstancesRemoved, EItemChangeReason Reason);

public: // For Events and Vars section, following base class structure
	// === EVENTS AND VARS ===
	
	// --- PUBLIC DELEGATES & ASSIGNABLE UPROPERTIES ---
	// Tagged Slot Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnItemAddedToTaggedSlot, const FGameplayTag&, SlotTag, const UItemStaticData*, ItemData, int32, Quantity, const TArray<UItemInstanceData*>&, InstancesAdded, FTaggedItemBundle, PreviousItem, EItemChangeReason, Reason);
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnItemAddedToTaggedSlot OnItemAddedToTaggedSlot;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnItemRemovedFromTaggedSlot, const FGameplayTag&, SlotTag, const UItemStaticData*, ItemData, int32, Quantity, const TArray<UItemInstanceData*>&, InstancesRemoved, EItemChangeReason, Reason);
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnItemRemovedFromTaggedSlot OnItemRemovedFromTaggedSlot;

	// Crafting Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftConfirmed, TSubclassOf<UObject>, CraftedClass, int32, QuantityCrafted);
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnCraftConfirmed OnCraftConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAvailableRecipesUpdated);
	UPROPERTY(BlueprintAssignable, Category = "RIS | Equipment")
	FOnAvailableRecipesUpdated OnAvailableRecipesUpdated;

	// --- PUBLIC CONFIGURABLE UPROPERTIES ---
	// Tagged Slot Configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	TArray<FUniversalTaggedSlot> UniversalTaggedSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS | Equipment")
	TArray<FGameplayTag> SpecializedTaggedSlots; // E.g., head, feet

	// Crafting Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS | Recipes")
	FGameplayTagContainer RecipeTagFilters;

	// --- PUBLIC REPLICATED STATE UPROPERTIES ---
	// Crafting State
	UPROPERTY(ReplicatedUsing=OnRep_Recipes, EditAnywhere, BlueprintReadOnly, Category = "RIS | Recipes")
	TArray<FPrimaryRISRecipeId> AllUnlockedRecipes;

protected:
	// == SERVER RPC IMPLEMENTATIONS (for public UFUNCTION(Server, Reliable) stubs) ==
	UFUNCTION(Server, Reliable, Category = "RIS | Equipment")
	void PickupItem_Server(AWorldItem* WorldItem, EPreferredSlotPolicy PreferTaggedSlots = EPreferredSlotPolicy::PreferGenericInventory, bool DestroyAfterPickup = true);

	UFUNCTION(Server, Reliable, Category = "RIS")
	void MoveItem_Server(const FGameplayTag& ItemId, int32 Quantity,
	                     const TArray<int32>& InstanceIdsToMove,
						 const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
						 const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
						 const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0);
	
	UFUNCTION(Server, Reliable)
	void DropFromTaggedSlot_Server(const FGameplayTag& SlotTag, int32 Quantity, const TArray<int32>& InstanceIdsToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));

	UFUNCTION(Server, Reliable)
	void UseItemFromTaggedSlot_Server(const FGameplayTag& SlotTag, int32 ItemToUseInstanceId = -1);
	// Note: CraftRecipeId_Server and SetRecipeLock_Server RPC stubs are already public.

	// == SERVER-SIDE IMPLEMENTATION LOGIC ==

	// Always allows partial move
	static void MoveBetweenContainers_ServerImpl(UItemContainerComponent* SourceComponent,
										         UItemContainerComponent* TargetComponent,
										         const FGameplayTag& ItemId,
										         int32 Quantity,
										         const TArray<int32>& InstanceIdsToMove,
										         const FGameplayTag& SourceTaggedSlot,
										         const FGameplayTag& TargetTaggedSlot);
	
	int32 MoveItem_ServerImpl(const FGameplayTag& ItemId, int32 RequestedQuantity,
	                          TArray<UItemInstanceData*> InstancesToMove,
							  const FGameplayTag& SourceTaggedSlot = FGameplayTag(),
							  const FGameplayTag& TargetTaggedSlot = FGameplayTag(),
							  bool AllowAutomaticSwapping = true,
							  const FGameplayTag& SwapItemId = FGameplayTag(), int32 SwapQuantity = 0,
							  bool SuppressEvents = false, bool SuppressUpdate = false, bool SimulateMoveOnly = false);

	void DropFromTaggedSlot_ServerImpl(const FGameplayTag& SlotTag, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));
	
	// Tagged Slot Specific Logic
	void UpdateBlockingState(FGameplayTag SlotTag, const UItemStaticData* ItemData, bool IsEquip);
	
	// Crafting Logic
	void CheckAndUpdateRecipeAvailability();

	// Internal Query Helpers (used by server logic)
	bool ContainedInUniversalSlot(const FGameplayTag& TagToFind) const;
	int32 GetIndexForTaggedSlot(const FGameplayTag& SlotTag) const;

	// == OVERRIDES OF BASE PROTECTED VIRTUALS ==
	virtual void UpdateWeightAndSlots() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual int32 DropAllItems_ServerImpl() override;
	virtual int32 DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason, bool AllowPartial = false, bool SuppressEvents = false, bool SuppressUpdate = false) override;
	virtual void ClearServerImpl() override;
	virtual int32 ExtractItem_ServerImpl(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& InstanceArrayToAppendTo, bool AllowPartial, bool SuppressEvents, bool SuppressUpdate) override;
	
	// Unlike other extract methods, this Does NOT allow partial extraction, will return 0
	int32 ExtractItemFromTaggedSlot_IfServer(const FGameplayTag& TaggedSlot, const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& InstanceArrayToAppendTo);


	// == REPLICATION HANDLERS (OnRep) ==
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_Recipes();


	// == INTERNAL STATE CHANGE DETECTION ==
	void DetectAndPublishContainerChanges();


	// --- PROTECTED REPLICATED STATE UPROPERTIES ---
	UPROPERTY(ReplicatedUsing=OnRep_Slots, BlueprintReadOnly, Category = "RIS")
	TArray<FTaggedItemBundle> TaggedSlotItems;

	// --- PROTECTED NON-REPLICATED INTERNAL STATE ---
	TMap<FGameplayTag, TArray<UObjectRecipeData*>> CurrentAvailableRecipes;
	TMap<FGameplayTag, FItemBundle> CachedTaggedSlotItems; // For client-side change detection for tagged slots

private:
	UPROPERTY()
	URISSubsystem* Subsystem;
	
	TArray<FGameplayTag> _SlotsToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.
	
	// The cache is a copy of Items that is not replicated, used to detect changes after replication, only used on client
	TMap<FGameplayTag, FItemBundle> TaggedItemsCache; // Slot to quantity;

	TArray<std::tuple<FGameplayTag, int32>> GetItemDistributionPlan(const UItemStaticData* ItemData, int32 Quantity, EPreferredSlotPolicy PreferTaggedSlots);
	void SortUniversalTaggedSlots();

public: // Boilerplate - Friend classes
	friend class UGridInventoryViewModel;
	friend class UItemContainerComponent;
};