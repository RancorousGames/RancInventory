// Copyright Rancorous Games, 2024

#pragma once
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
    
    explicit UItemContainerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void InitializeComponent() override;
	
    UFUNCTION(BlueprintPure, Category=RIS)
    float GetCurrentWeight() const;

    UFUNCTION(BlueprintPure, Category=RIS)
    float GetMaxWeight() const;

    UFUNCTION(BlueprintPure, Category=RIS)
    const FItemBundleWithInstanceData& FindItemById(const FGameplayTag& ItemId) const;

    /* Add items to the inventory
     * Typically only called on server but if called on client it can be used as a form of client prediction
     * This will extract the item count from the source, if you don't want to take the items from anywhere specific, use the GetInfiniteItemSource
     * Returns the amount added */
	UFUNCTION(BlueprintCallable, Category=RIS)
	int32 AddItem_IfServer(TScriptInterface<IItemSource> ItemSource,  const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial = false, bool SuppressUpdate = false);

    /* For most games we could probably trust the client to specify ItemInstance but for e.g. a hot potato we can't
     * Instead have the client send an input like UseItem or DropItem
     * Returns the amount removed, if AllowPartial is false and there is insufficient quantity then removes 0*/
    UFUNCTION(BlueprintCallable, Category=RIS)
    int32 DestroyItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial = false);

    /* Attempts to drop the item from the inventory, attempting to spawn an Item object in the world
     * Specify DropItemClass and DropDistance properties to customize the drop
     * Called on client it will request the drop on the server
     * Returns the quantity dropped, on client this is only a "guess"
     * Does not partial, will not do a drop if not enough quantity is available
     */
    UFUNCTION(BlueprintCallable, Category=RIS)
    int32 DropItems(const FGameplayTag& ItemId, int32 Quantity,  FVector RelativeDropLocation = FVector(1e+300, 0,0));

	// Attempt to activate the item, e.g. use a potion, activate a magic item, etc.
	UFUNCTION(BlueprintCallable, Category=RIS)
    int32 UseItem(const FGameplayTag& ItemId);

    /* Useful for e.g. Death, drops items evenly spaced in a circle with radius DropDistance */
    UFUNCTION(BlueprintCallable, Category=RIS)
    int32 DropAllItems_IfServer();
    
	/* Checks weight and container slot count */
    UFUNCTION(BlueprintPure, Category=RIS)
    bool CanContainerReceiveItems(const FGameplayTag& ItemId, int32 Quantity) const;

	/* Checks weight and container slot count */
    UFUNCTION(BlueprintPure, Category=RIS)
    int32 GetReceivableQuantity(const FGameplayTag& ItemId) const;
	
	UFUNCTION(BlueprintPure, Category=RIS)
    bool HasWeightCapacityForItems(const FGameplayTag& ItemId, int32 Quantity) const;

	/* Returns an item id and at least a quantity exists among ALL items in the container */
    UFUNCTION(BlueprintPure, Category=RIS)
    bool Contains(const FGameplayTag& ItemId, int32 Quantity = 1) const;

	/* Returns an item id and quantity exists among the items in the containers generic slots
	 * This is equivalent to GetContainedQuantity if this is an Itemcontainer but NOT an InventoryComponent */
    UFUNCTION(BlueprintPure, Category=RIS)
    int32 GetContainerOnlyItemQuantity(const FGameplayTag& ItemId) const;
	
	UFUNCTION(BlueprintPure, Category=RIS)
	TArray<UItemInstanceData*> GetItemState(const FGameplayTag& ItemId);
	
	UFUNCTION(BlueprintPure, Category=RIS)
	UItemInstanceData* GetSingleItemState(const FGameplayTag& ItemId);

    UFUNCTION(BlueprintPure, Category=RIS)
    TArray<FItemBundleWithInstanceData> GetAllContainerItems() const;
    
    UFUNCTION(BlueprintPure, Category=RIS)
    bool IsEmpty() const;

    // Removes all items and publishes the removals
    UFUNCTION(BlueprintCallable, Category=RIS)
    void Clear_IfServer();
	
	/* This allows you to set a delegate that will be called to validate if/how many of an item can be added to the container
	 * Items can still be added to tagged slots even if this returns false which results in it also existing in the container items
	* TODO: consider whether it might want to know the item state */
	DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(int32, FAddItemValidationDelegate, const FGameplayTag&, ItemId, int32, Quantity);
	UFUNCTION(BlueprintCallable, Category=RIS)
	void SetAddItemValidationCallback_IfServer(const FAddItemValidationDelegate& ValidationDelegate);
	
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryItemAdded, const UItemStaticData*, ItemData, int32, Quantity, EItemChangeReason, Reason);
    UPROPERTY(BlueprintAssignable, Category=RIS)
    FOnInventoryItemAdded OnItemAddedToContainer;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInventoryItemRemoved,  const UItemStaticData*, ItemData, int32, Quantity, EItemChangeReason, Reason);
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
    int32 MaxContainerSlotCount = MAX_int32;
	
	/* Set whether the container should be configured to a diablo-like jigsaw style
	 * Note that the inventory/container doesn't actually respect the jigsaw puzzle, only the number of slots.
	 * This is so that we dont need to replicate the players setup to the server
	 * The client only viewmodel will handle keeping track of jigsaw configuration displayed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory")
	bool JigsawMode = false;
	
	UPROPERTY(BlueprintReadOnly, Category=RIS)
	int32 UsedContainerSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category=RIS)
	float CurrentWeight = 0;
	
	/*  Whether to write highly detailed debug information to the log */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RIS|Debug")
	bool DebugLoggingEnabled = false;

	UFUNCTION(BlueprintCallable, Category=RIS)
	int32 ExtractItemFromContainer_IfServer(const FGameplayTag& ItemId, int32 Quantity, UItemContainerComponent* ContainerToExtractFrom, bool AllowPartial = false);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
	int32 ExtractItem_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo);
	
	virtual int32 ExtractItem_IfServer_Implementation(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo) override;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory") // ReSharper disable once CppHidingFunction
    int32 GetContainedQuantity(const FGameplayTag& ItemId);
	
	
protected:

    UPROPERTY(EditAnywhere, Category=RIS)
    TArray<FInitialItem> InitialItems;
    
    UPROPERTY(ReplicatedUsing=OnRep_Items, BlueprintReadOnly, Category=RIS)
	FVersionedItemInstanceArray ItemsVer;
//   TArray<FItemBundle> Items;


	UPROPERTY(ReplicatedUsing=OnRep_Items, BlueprintReadOnly, Category=RIS)
	FVersionedItemInstanceArray CachedItemsVer;
	
	FAddItemValidationDelegate OnValidateAddItemToContainer;

    UFUNCTION(Server, Reliable)
    void DropItemFromContainer_Server(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation = FVector(1e+300, 0,0));
	void SpawnItemIntoWorldFromContainer_ServerImpl(const FGameplayTag& ItemId, int32 Quantity, FVector RelativeDropLocation, TArray<UItemInstanceData*> ItemInstanceData);
	
	UFUNCTION(Server, Reliable)
	void UseItem_Server(const FGameplayTag& ItemId);
    
    // virtual implementations for override in subclasses
	virtual int32 AddItem_ServerImpl(TScriptInterface<IItemSource> ItemSource, const FGameplayTag& ItemId, int32 RequestedQuantity, bool AllowPartial,
												 bool SuppressUpdate = false);
    virtual int32 DropAllItems_ServerImpl();
	virtual int32 GetContainerOnlyItemQuantityImpl(const FGameplayTag& ItemId) const;
	virtual bool ContainsImpl(const FGameplayTag& ItemId, int32 Quantity = 1) const;
	virtual void ClearImpl();
	virtual int32 DestroyItemImpl(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, bool AllowPartial = false, bool UpdateAfter = true, bool SendEventAfter = true);
	virtual int32 GetReceivableQuantityImpl(const FGameplayTag& ItemId) const;
    virtual int32 GetQuantityContainerCanReceiveByWeight(const UItemStaticData* ItemData) const;
	virtual int32 ExtractItemImpl_IfServer(const FGameplayTag& ItemId, int32 Quantity, EItemChangeReason Reason, TArray<UItemInstanceData*>& StateArrayToAppendTo, bool SuppressUpdate = false);
	
    virtual int32 GetContainedQuantity_Implementation(const FGameplayTag& ItemId) override;

	// Helper functions
	int32 GetQuantityContainerCanReceiveBySlots(const UItemStaticData* ItemData) const;
    FItemBundleWithInstanceData* FindItemInstance(const FGameplayTag& ItemId);
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
    virtual void UpdateWeightAndSlots();
    
    void RebuildItemsToCache();
	
	//// Client Prediction and Rollback ////

	// The last known state of items, used to detect changes after replication, only used on client
	

	// Operations we sent to the server that have not yet been confirmed
	UPROPERTY(VisibleAnywhere, Category=RIS)
	TArray<FRISExpectedOperation> RequestedOperationsToServer;
    
private:
    TArray<FGameplayTag> _KeysToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.
    
	
    void DetectAndPublishChanges();
	
    UFUNCTION()
    void OnRep_Items();
};