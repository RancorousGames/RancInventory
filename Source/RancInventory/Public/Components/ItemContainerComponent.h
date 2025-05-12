// Copyright Rancorous Games, 2024

#pragma once
#include "LogRancInventorySystem.h"
#include "Actors/WorldItem.h"
#include "Data/ItemBundle.h"
#include "Core/IItemSource.h"
#include "Data/RISDataTypes.h"
#include "Data/ItemStaticData.h"
#include "ViewModels/RISNetworkingData.h"
#include "ItemContainerComponent.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), Category = "RIS | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API UItemContainerComponent : public UActorComponent, public IItemSource
{
    GENERATED_BODY()
public:

	static const TArray<UItemInstanceData*> NoInstances;
    
    explicit UItemContainerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void InitializeComponent() override;

	// == QUERY ==
	
	/* Returns an item id and at least a quantity exists among ALL items in the container.
     * @param ItemId The ID of the item to search for.
     * @param Quantity The minimum number of items required.
     * @return True if the condition is met, false otherwise.
     */
    UFUNCTION(BlueprintPure, Category=RIS, meta = (DisplayName = "Contains Instances"))
    bool Contains(const FGameplayTag& ItemId, int32 Quantity = 1) const;
	
	virtual bool ContainsInstances(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToLookFor) const;
	
	virtual int32 GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const override;
	
	/**
	 * Checks if the container holds at least 'Quantity' items of 'ItemId' where
	 * each counted item's instance data satisfies the provided Blueprint Predicate.
	 * If the item type does not use instance data, this behaves like the regular Contains check.
	 * If the Predicate is unbound, it's considered satisfied by any instance data.
	 *
	 * @param ItemId The ID of the item to search for.
	 * @param Predicate A Blueprint delegate (const UItemInstanceData*) -> bool. Must return true for an item instance to be counted.
	 * @param Quantity The minimum number of item instances satisfying the predicate required.
	 * @return True if the condition is met, false otherwise.
	 */
	DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FBPItemInstancePredicate, const UItemInstanceData*, InstanceData);
	UFUNCTION(BlueprintPure, Category="RIS", meta = (DisplayName = "Contains By Predicate (Blueprint)"))
	bool ContainsByPredicate(const FGameplayTag& ItemId, const FBPItemInstancePredicate& Predicate, int32 Quantity = 1) const;
	
	UFUNCTION(BlueprintPure, Category=RIS)
	bool IsEmpty() const;
	
	/**
	 * Validates if the specified item/quantity can be received by the target (either a tagged slot or the generic container).
	 * Checks compatibility, blocking, stacking, weight, and slot limits of the *target*.
	 * @param ItemId The item ID to check.
	 * @param QuantityToReceive The desired quantity.
	 * @param SwapBackAllowed Whether a potential swap is being considered (affects capacity checks for tagged slots).
	 * @return The actual quantity that CAN be received (can be less than requested due to limits, 0 if invalid/impossible).
	 */
	UFUNCTION(BlueprintPure, Category="RIS|Validation") // Make BP accessible if needed for UI checks
	virtual bool CanReceiveItem(const FGameplayTag& ItemId, int32 QuantityToReceive, bool SwapBackAllowed = false) const;

	/*
	 * Validates if the specified item/quantity can be received by the target (either a tagged slot or the generic container).
	 * Checks compatibility, weight and slot limits of the *target*.
	 * NOTE: That slot limit is purely based on max slot count and max stack size of item
	 * @param ItemData The item data to check.
	 * @param RequestedQuantity if AllowPartial is false then this returns 0 if we cant contain all of RequestedQuantity
	 * @param AllowPartial Whether partial quantities are allowed.
	 * @param SwapBackAllowed Whether a potential swap is being considered (affects capacity checks for tagged slots).
	 * @return The actual quantity that CAN be received (can be less than requested due to limits, 0 if invalid/impossible).
	 */
	virtual int32 GetReceivableQuantity(const UItemStaticData* ItemData, int32 RequestedQuantity = 0x7fffffff, bool AllowPartial = true, bool SwapBackAllowed = false) const;
	
	virtual int32 GetQuantityContainerCanReceiveByWeight(const UItemStaticData* ItemData) const;
	int32 GetQuantityContainerCanReceiveBySlots(const UItemStaticData* ItemData) const;
	
	/* Returns copy of all items of the given type */
	UFUNCTION(BlueprintPure, Category=RIS)
	TArray<FItemBundle> GetAllItems() const;
	
	/* Returns copy of all items instance data of the given type */
	UFUNCTION(BlueprintPure, Category=RIS)
	TArray<UItemInstanceData*> GetItemInstanceData(const FGameplayTag& ItemId) const;

	/* Returns reference to the "last" item among all instances of the given type or nullptr if none is found or the item doesnt have instance data */
	UFUNCTION(BlueprintPure, Category=RIS)
	UItemInstanceData* GetSingleItemInstanceData(const FGameplayTag& ItemId) const;

	// == ADD ITEMS TO THIS CONTAINER ==
	
	/* Adds items to this container, extracting them from the specified source.
	 * Cannot be called on clients, clients must use e.g. pickup or otherwise indirectly add item
	 * Prioritizes extracting specific InstancesToExtract if provided, otherwise extracts by quantity.
	 * @param ItemSource The source to extract items from (e.g., another container, world item, subsystem).
	 * @param ItemId The ID of the item to add.
	 * @param RequestedQuantity The quantity to add (ignored if InstancesToExtract is not empty).
	 * @param AllowPartial If true, allows adding fewer items than requested if capacity or source is limited.
	 * @param SuppressEvents If true, suppresses broadcasting OnItemAddedToContainer event.
	 * @param SuppressUpdate If true, suppresses updating weight/slots after adding.
	 * @return The actual quantity of items successfully added to this container. */
	UFUNCTION(BlueprintCallable, Category = RIS, meta = (AutoCreateRefTerm = "InstancesToExtract"))
	int32 AddItem_IfServer(
		TScriptInterface<IItemSource> ItemSource,
		const FGameplayTag& ItemId,
		int32 RequestedQuantity,
		bool AllowPartial = false,
		bool SuppressEvents = false,
		bool SuppressUpdate = false
	);

	/* Adds items to this container, extracting them from the specified source.
	 * Cannot be called on clients, clients must use e.g. pickup or otherwise indirectly add item
	 * Prioritizes extracting specific InstancesToExtract if provided, otherwise extracts by quantity.
	 * @param ItemSource The source to extract items from (e.g., another container, world item, subsystem).
	 * @param ItemId The ID of the item to add.
	 * @param RequestedQuantity The quantity to add (ignored if InstancesToExtract is not empty).
	 * @param InstancesToExtract Specific instances to extract from the source and add to this container, usually not specified.
	 * @param AllowPartial If true, allows adding fewer items than requested if capacity or source is limited.
	 * @param SuppressEvents If true, suppresses broadcasting OnItemAddedToContainer event.
	 * @param SuppressUpdate If true, suppresses updating weight/slots after adding.
	 * @return The actual quantity of items successfully added to this container. */
	UFUNCTION(BlueprintCallable, Category = RIS, meta = (AutoCreateRefTerm = "InstancesToExtract"))
	virtual int32 AddItemWithInstances_IfServer(
		TScriptInterface<IItemSource> ItemSource,
		const FGameplayTag& ItemId,
		int32 RequestedQuantity,
		const TArray<UItemInstanceData*>& InstancesToExtract,
		bool AllowPartial = false,
		bool SuppressEvents = false,
		bool SuppressUpdate = false
	);

	/* This allows you to set a delegate that will be called to validate if/how many of an item can be added to the container
	 * For example a container or inventory might only be able to hold one quest item
	 * The slot refers to a tagged slot of an inventory or container if empty
	 */
	DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(int32, FAddItemValidationDelegate, const FGameplayTag&, ItemId, int32, Quantity, const FGameplayTag&, Slot);
	UFUNCTION(BlueprintCallable, Category=RIS)
	void SetAddItemValidationCallback(const FAddItemValidationDelegate& ValidationDelegate);

	// == REMOVE ITEMS FROM CONTAINER ==

	// = Client interface =
	
	/* Attempt to activate the item, e.g. use a potion, activate a magic item, etc.
	 * Note we do not specify a quantity or array of instance data here
	 * As a consequence, usable items must either not be instanced OR have QuantityPerUse <= 1 */
	UFUNCTION(BlueprintCallable, Category=RIS)
	int32 UseItem(const FGameplayTag& ItemId, int32 ItemToUseInstanceId = -1);
	
	/* Attempts to drop the item from the inventory, attempting to spawn an Item object in the world
	 * Specify DropItemClass and DropDistance properties to customize the drop
	 * Ignores quantity if InstancesToExtract is not empty
	 * Called on client it will request the drop on the server
	 * Returns the quantity dropped, on client this is only a "guess"
	 * Does not partial, will not do a drop if not enough quantity is available
	 */
	UFUNCTION(BlueprintCallable, Category=RIS, meta = (AutoCreateRefTerm = "InstancesToDrop"))
	int32 DropItem(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));
	
	/**
	 * Server RPC to handle moving items from this container to another.
	 * Called by the client-side ViewModel after prediction.
	 * Always allows partial move
	 * @param TargetComponent The destination container/inventory component.
	 * @param ItemId The ID of the item being moved.
	 * @param Quantity The amount of the item to move. (ignored if InstanceIdsToMove is not empty)
	 * @param InstancesToMove Specific instance IDs being moved (if any).
	 * @param SourceTaggedSlot Tag of the source slot IF moving FROM a tagged slot in THIS component.
	 * @param TargetTaggedSlot Tag of the target slot IF moving TO a tagged slot in the TargetComponent.
	 */
	UFUNCTION(BlueprintCallable)
	void RequestMoveItemToOtherContainer(
		UItemContainerComponent* TargetComponent,
		const FGameplayTag& ItemId,
		int32 Quantity,
		const TArray<UItemInstanceData*>& InstancesToMove,
		const FGameplayTag& SourceTaggedSlot,
		const FGameplayTag& TargetTaggedSlot
	);
	
	// = Server only =
	
	/* Cannot be called on clients, clients must call e.g. use/drop or otherwise indirectly destroy items */
	UFUNCTION(BlueprintCallable, Category=RIS)
	int32 DestroyItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason, bool AllowPartial = false);
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial) override;

	/* Useful for e.g. Death, drops items evenly spaced in a circle with radius DropDistance */
	UFUNCTION(BlueprintCallable, Category=RIS)
	int32 DropAllItems_IfServer();
	
    // Removes all items and publishes the removals
    UFUNCTION(BlueprintCallable, Category=RIS)
    void Clear_IfServer();

protected:

	// Protected Add

	// Allows partial move
	UFUNCTION(Server, Reliable)
	void RequestMoveItemToOtherContainer_Server(
		UItemContainerComponent* TargetComponent,
		const FGameplayTag& ItemId,
		int32 Quantity,
		const TArray<int32>& InstanceIdsToMove,
		const FGameplayTag& SourceTaggedSlot,
		const FGameplayTag& TargetTaggedSlot
	);

	// Protected Remove:
	
	UFUNCTION(Server, Reliable)
	void DropItemFromContainer_Server(const FGameplayTag& ItemId, int32 Quantity, const TArray<int32>& InstanceIdsToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));
	void DropItemFromContainer_ServerImpl(const FItemBundle* Item, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToDrop, FVector RelativeDropLocation = FVector(1e+300, 0,0));

	// Drops all items, respecting stack sizes. Returns number of worlditems created			
	virtual int32 DropAllItems_ServerImpl();
	
	UFUNCTION(Server, Reliable)
	void UseItem_Server(const FGameplayTag& ItemId, int32 ItemToUseUniqueId = -1);
	
	void SpawnItemIntoWorldFromContainer_ServerImpl(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation, const TArray<UItemInstanceData*>& ItemInstanceData);
	
	virtual void ClearServerImpl();
	virtual int32 DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, TArray<UItemInstanceData*> InstancesToDestroy, EItemChangeReason Reason, bool AllowPartial = false, bool SuppressEvents = false, bool SuppressUpdate = false);

	// Interface version of ExtractItem_IfServer_Implementation
	virtual int32 ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& InstancesToExtract, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool AllowPartial, bool SuppressEvents = false, bool SuppressUpdate = false);

	// == OTHER ==
	
	const FItemBundle* FindItemInstance(const FGameplayTag& ItemId) const;
	
	FItemBundle* FindItemInstanceMutable(const FGameplayTag& ItemId);
    
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void UpdateWeightAndSlots();
    
	void RebuildItemsToCache();
	
	void DetectAndPublishChanges();
	
	UFUNCTION()
	void OnRep_Items();

	/* Internal helper to receive items that have already been extracted from another source.
	 * Takes ownership of the provided instance data, registers them as subobjects,
	 * updates quantity, weight, and slots, and broadcasts the OnItemAdded event.
	 * Called only by MoveBetweenContainers_ServerImpl
	 */
	virtual int32 ReceiveExtractedItems_IfServer(const FGameplayTag& ItemId, int32 Quantity, const TArray<UItemInstanceData*>& ReceivedInstances, bool SuppressEvents = false);
	
	// === EVENTS AND VARS ===
	
public:
	
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnInventoryItemAdded, const UItemStaticData*, ItemData, int32, Quantity, const TArray<UItemInstanceData*>&, InstancesAdded, EItemChangeReason, Reason);
    UPROPERTY(BlueprintAssignable, Category=RIS)
    FOnInventoryItemAdded OnItemAddedToContainer;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnInventoryItemRemoved,  const UItemStaticData*, ItemData, int32, Quantity, const TArray<UItemInstanceData*>&, InstancesRemoved, EItemChangeReason, Reason);
    UPROPERTY(BlueprintAssignable, Category=RIS)
    FOnInventoryItemRemoved OnItemRemovedFromContainer;
    
    /* Distance away from the owning actor to drop items. Only used on server and only if no drop location is supplied to the drop call */ 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RIS)
    float DefaultDropDistance = 100;

    /* Class to spawn when dropping items. Only used on server */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RIS)
    TSubclassOf<AWorldItem> DropItemClass = AWorldItem::StaticClass();

    /* Max weight allowed for this item container, this also applies to any child classes */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (ClampMin = "1", UIMin = "1"))
    float MaxWeight = 999999;
	
    /* Max number of slots contained items are allowed to take up.
     * Note: The inventory doesn't actually store items in slots, it just uses it to calculate whether items can be added */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (ClampMin = "1", UIMin = "1"))
    int32 MaxSlotCount = MAX_int32;
	
	/* Set whether the container should be configured to a diablo-like jigsaw style
	 * Note that the inventory/container doesn't actually respect the jigsaw puzzle, only the number of slots.
	 * This is so that we dont need to replicate the players setup to the server
	 * The client only viewmodel will handle keeping track of jigsaw configuration displayed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory")
	bool JigsawMode = false;
	
	UPROPERTY(BlueprintReadOnly, Category=RIS)
	int32 UsedContainerSlotCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=RIS)
	float CurrentWeight = 0;
	
	/*  Whether to write highly detailed debug information to the log */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS|Debug")
	bool DebugLoggingEnabled = false;

protected:

    UPROPERTY(EditAnywhere, Category=RIS)
    TArray<FInitialItem> InitialItems;
    
    UPROPERTY(ReplicatedUsing=OnRep_Items, BlueprintReadOnly, Category=RIS)
	FVersionedItemInstanceArray ItemsVer;
	
	// The last known state of items, used to detect changes after replication, only used on client
	UPROPERTY(ReplicatedUsing=OnRep_Items, BlueprintReadOnly, Category=RIS)
	FVersionedItemInstanceArray CachedItemsVer;
	
	FAddItemValidationDelegate OnValidateAddItem;

	

	// Operations we sent to the server that have not yet been confirmed
	UPROPERTY(VisibleAnywhere, Category=RIS)
	TArray<FRISExpectedOperation> RequestedOperationsToServer;
    
private:
    TArray<FGameplayTag> _KeysToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.

	
	// == BOILERPLATE ==
public:

	FORCEINLINE bool IsClient(const char* FunctionName = nullptr) const;

    static bool BadItemData(const UItemStaticData* ItemData, const FGameplayTag& ItemId = FGameplayTag::EmptyTag);

    FORCEINLINE TArray<FItemBundle>::RangedForIteratorType begin()
	{
		return ItemsVer.Items.begin();
	}

	FORCEINLINE TArray<FItemBundle>::RangedForConstIteratorType begin() const
	{
		return ItemsVer.Items.begin();
	}

	FORCEINLINE TArray<FItemBundle>::RangedForIteratorType end()
	{
		return ItemsVer.Items.end();
	}

	FORCEINLINE TArray<FItemBundle>::RangedForConstIteratorType end() const
	{
		return ItemsVer.Items.end();
	}

	friend class UInventoryComponent; // Necessary for MoveBetweenContainers_ServerImpl as protected doesnt work for static functions
	friend class FInventoryComponentTestScenarios;
};