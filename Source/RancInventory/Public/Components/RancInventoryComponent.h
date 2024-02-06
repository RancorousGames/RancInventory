#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Components/ActorComponent.h>
#include "Management/RancInventoryData.h"
#include "RancInventoryComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRancInventoryUpdate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRancInventoryEmpty);

UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API URancInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    explicit URancInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    float GetCurrentWeight() const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    float GetMaxWeight() const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    int32 GetCurrentItemCount() const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    const FRancItemInfo& FindItemById(const FPrimaryRancItemId& ItemId) const;

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    void AddItems(const FRancItemInfo& ItemInfo);

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    bool RemoveItems(const FRancItemInfo& ItemInfo);

    UFUNCTION(BlueprintCallable, Category="Ranc Inventory")
    bool CanReceiveItem(const FRancItemInfo& ItemInfo) const;
    
    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    bool ContainsItem(const FPrimaryRancItemId& ItemId, int32 Quantity = 1) const;

    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    TArray<FRancItemInfo> GetAllItems() const;
    
    UFUNCTION(BlueprintPure, Category="Ranc Inventory")
    bool IsEmpty();

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);
    UPROPERTY(BlueprintAssignable, Category="Ranc Inventory")
    FOnInventoryUpdated OnInventoryUpdated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryEmptied);
    UPROPERTY(BlueprintAssignable, Category="Ranc Inventory")
    FOnInventoryEmptied OnInventoryEmptied;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_Items, EditAnywhere, Category="Ranc Inventory")
    TArray<FRancItemInfo> Items;

    /* Max weight allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "0", UIMin = "0"))
    float MaxWeight;

    /* Max num of items allowed for this inventory */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ranc Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
    int32 MaxNumItems;
private:
    float CurrentWeight;

    UFUNCTION()
    void OnRep_Items();

    void UpdateWeight();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};