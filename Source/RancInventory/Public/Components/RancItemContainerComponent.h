#pragma once
#include "Actors/AWorldItem.h"
#include "Management/RancInventoryData.h"
#include "RancItemContainerComponent.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class URancItemContainerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    
    explicit URancItemContainerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void InitializeComponent() override;
    
	UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    float GetCurrentWeight() const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    float GetMaxWeight() const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    int32 GetCurrentItemCount() const;

    
    void A();

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    const FRancItemInstance& FindItemById(const FGameplayTag& ItemId) const;

    /* Add items to the inventory
     * Should only be called on server as we can't trust client to provide trustworthy ItemInstance
     * Instead have the client send an input like CraftItem or PickupItem to the server which results in server call to AddItem  */
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    void AddItems_IfServer(const FRancItemInstance& ItemInstance);

    /* For most games we could probably trust the client to specify ItemInstance but for e.g. a hot potato we can't
     * Instead have the client send an input like UseItem or DropItem  */
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    bool RemoveItems_IfServer(const FRancItemInstance& ItemInstance);
    
    /* Attempts to drop the item from the inventory, attempting to spawn an Item object in the world
     * Specify DropItemClass and DropDistance properties to customize the drop
     * Called on client it will request the drop on the server
     * Returns the quantity actually dropped, on client this is only a "guess" */
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    int32 DropItems(const FRancItemInstance& ItemInstance, float DropAngle = 0);

    /* Useful for e.g. Death, drops items evenly spaced in a circle with radius DropDistance */
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    int32 DropAllItems_IfServer();
    
    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    bool CanReceiveItem(const FRancItemInstance& ItemInstance) const;
    
    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    bool ContainsItems(const FGameplayTag& ItemId, int32 Quantity = 1) const;
    
    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    int32 GetItemCount(const FGameplayTag& ItemId) const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    TArray<FRancItemInstance> GetAllItems() const;
    
    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    bool IsEmpty() const;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemAdded, const FRancItemInstance&, ItemInstance);
    UPROPERTY(BlueprintAssignable, Category="Ranc Inventory")
    FOnInventoryItemAdded OnItemAdded;
    
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemRemoved, const FRancItemInstance&, ItemInstance);
    UPROPERTY(BlueprintAssignable, Category="Ranc Inventory")
    FOnInventoryItemRemoved OnItemRemoved;
    
    /* Distance away from the owning actor to drop items. Only used on server */ 
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DropDistance = 100;

    /* Class to spawn when dropping items. Only used on server */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AWorldItem> DropItemClass = AWorldItem::StaticClass();

    /* Max weight allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
    float MaxWeight;

    /* Max num of items allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
    int32 MaxNumItems;

protected:

    UPROPERTY(EditAnywhere, Category="Ranc Inventory")
    TArray<FRancInitialItem> InitialItems;
    
    UPROPERTY(ReplicatedUsing=OnRep_Items, BlueprintReadOnly, Category="Ranc Inventory")
    TArray<FRancItemInstance> Items;


    UFUNCTION(Server, Reliable)
    void DropItems_Server(const FRancItemInstance& ItemInstance, float DropAngle = 0);
    
    // virtual drop implementation for override in subclasses
    virtual int32 DropAllItems_ServerImpl();

    AWorldItem* SpawnDroppedItem_IfServer(const FRancItemInstance& ItemInstance, float DropAngle = 0) const;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    
    virtual void UpdateWeight();
    float CurrentWeight;
    
    void CopyItemsToCache();
    
private:
    TArray<FGameplayTag> _KeysToRemove; // Could be a local value but just slight optimization to avoid creating a new array every time.
    
    // The cache is a copy of Items that is not replicated, used to detect changes after replication, only used on client
    TMap<FGameplayTag, int32> ItemsCache; // Id to quantity;
    void DetectAndPublishChanges();
    
    UFUNCTION()
    void OnRep_Items();
    
};
