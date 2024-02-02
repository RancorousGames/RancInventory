// Author: Lucas Vilas-Boas
// Year: 2023

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Components/ActorComponent.h>
#include "Management/RancInventoryData.h"
#include "RancInventoryComponent.generated.h"

UENUM(Category = "Ranc Inventory | Enumerations")
enum class ERancInventoryUpdateOperation : uint8
{
    None,
    Add,
    Remove
};

UENUM(Category = "Ranc Inventory | Enumerations")
enum class ERancInventorySortingMode : uint8
{
    ID,
    Name,
    Type,
    IndividualValue,
    StackValue,
    IndividualWeight,
    StackWeight,
    Quantity,
    Level,
    Tags
};

UENUM(Category = "Ranc Inventory | Enumerations")
enum class ERancInventorySortingOrientation : uint8
{
    Ascending,
    Descending
};

USTRUCT(Category = "Ranc Inventory | Structures")
struct FItemModifierData
{
    GENERATED_BODY()

    FItemModifierData() = default;

    explicit FItemModifierData(const FRancItemInfo& InItemInfo) : ItemInfo(InItemInfo)
    {
    }

    explicit FItemModifierData(const FRancItemInfo& InItemInfo, const int32& InIndex) : ItemInfo(InItemInfo), Index(InIndex)
    {
    }

    FRancItemInfo ItemInfo = FRancItemInfo();
    int32 Index = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRancInventoryUpdate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRancInventoryEmpty);

UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API URancInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    explicit URancInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    /* Experimental parameter to assist using empty slots in the inventory: If true, will replace empty slots with empty item info */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory")
    bool bAllowEmptySlots;

    /* Get the current inventory weight */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    float GetCurrentWeight() const;

    /* Get the max inventory weight */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    float GetMaxWeight() const;

    /* Get the current num of items in this inventory */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    int32 GetCurrentNumItems() const;

    /* Get the current max num of items in this inventory */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    int32 GetMaxNumItems() const;

    /* Called on every inventory update */
    UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory")
    FRancInventoryUpdate OnInventoryUpdate;

    /* Called when the inventory is empty */
    UPROPERTY(BlueprintAssignable, Category = "Ranc Inventory")
    FRancInventoryEmpty OnInventoryEmpty;

    /* Get the items that this inventory have */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    TArray<FRancItemInfo> GetItemsArray() const;

    /* Get a reference of the item at given index */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    FRancItemInfo& GetItemReferenceAt(const int32 Index);

    /* Get a copy of the item at given index */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    FRancItemInfo GetItemCopyAt(const int32 Index) const;

    /* Check if this inventory can receive the item */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    virtual bool CanReceiveItem(const FRancItemInfo InItemInfo) const;

    /* Check if this inventory can give the item */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    virtual bool CanGiveItem(const FRancItemInfo InItemInfo) const;

    /* Find the first item that matches the specified info */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindFirstItemIndexWithInfo(const FRancItemInfo InItemInfo, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset = 0) const;

    /* Find the first item that matches the specified tag container */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindFirstItemIndexWithTags(const FGameplayTagContainer WithTags, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset = 0) const;

    /* Find the first item that matches the specified id */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindFirstItemIndexWithId(const FPrimaryRancItemId InId, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset = 0) const;

    /* Find the first item that matches the specified info */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindAllItemIndexesWithInfo(const FRancItemInfo InItemInfo, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const;

    /* Find the first item that matches the specified tag container */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindAllItemIndexesWithTags(const FGameplayTagContainer WithTags, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const;

    /* Find the first item that matches the specified id */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory", meta = (AutoCreateRefTerm = "IgnoreTags"))
    bool FindAllItemIndexesWithId(const FPrimaryRancItemId InId, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const;

    /* Check if the inventory stack contains a item that matches the specified info */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    bool ContainsItem(const FRancItemInfo InItemInfo, const bool bIgnoreTags = false) const;

    /* Check if the inventory is empty */
    UFUNCTION(BlueprintPure, Category = "Ranc Inventory")
    bool IsInventoryEmpty() const;

    /* Print debug informations in the log about this inventory */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    virtual void DebugInventory();

    /* Remove all items from this inventory */
    UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void ClearInventory();

    /* Update the current weight of this inventory */
    UFUNCTION(Client, Unreliable, BlueprintCallable, Category = "Ranc Inventory")
    void UpdateWeight();

    /* Get items from another inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void GetItemIndexesFrom(URancInventoryComponent* OtherInventory, const TArray<int32>& ItemIndexes);

    /* Give items to another inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void GiveItemIndexesTo(URancInventoryComponent* OtherInventory, const TArray<int32>& ItemIndexes);

    /* Get items from another inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void GetItemsFrom(URancInventoryComponent* OtherInventory, const TArray<FRancItemInfo>& Items);

    /* Give items to another inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void GiveItemsTo(URancInventoryComponent* OtherInventory, const TArray<FRancItemInfo>& Items);

    /* Discard items from this inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void DiscardItemIndexes(const TArray<int32>& ItemIndexes);

    /* Discard items from this inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void DiscardItems(const TArray<FRancItemInfo>& Items);

    /* Add items to this inventory */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ranc Inventory")
    void AddItems(const TArray<FRancItemInfo>& Items);

    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    void SortInventory(const ERancInventorySortingMode Mode, const ERancInventorySortingOrientation Orientation);

protected:
    /* Items that this inventory have */
    UPROPERTY(ReplicatedUsing = OnRep_RancItems, EditAnywhere, BlueprintReadOnly, Category = "Ranc Inventory", meta = (Getter = "GetItemsArray", ArrayClamp = "MaxNumItems"))
    TArray<FRancItemInfo> RancItems;

    /* Current weight of this inventory */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true"))
    float CurrentWeight;

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void RefreshInventory();

private:
    /* Max weight allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
    float MaxWeight;

    /* Max num of items allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
    int32 MaxNumItems;

    void ForceWeightUpdate();
    void ForceInventoryValidation();

public:
    /* Add a item to this inventory */
    void UpdateRancItems(const TArray<FRancItemInfo>& Modifiers, const ERancInventoryUpdateOperation Operation);

private:
    UFUNCTION(Server, Reliable)
    void Server_ProcessInventoryAddition_Internal(const TArray<FItemModifierData>& Modifiers);

    UFUNCTION(Server, Reliable)
    void Server_ProcessInventoryRemoval_Internal(const TArray<FItemModifierData>& Modifiers);

    UFUNCTION(Category = "Ranc Inventory")
    void OnRep_RancItems();

protected:
    /* Mark the inventory as dirty to update the replicated data and broadcast the events */
    UFUNCTION(BlueprintCallable, Category = "Ranc Inventory")
    void NotifyInventoryChange();
};
