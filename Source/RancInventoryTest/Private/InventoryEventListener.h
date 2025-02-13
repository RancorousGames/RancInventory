#pragma once

// GlobalInventoryEventListener.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/InventoryComponent.h"
#include "Data/ItemBundle.h"
#include "UObject/Object.h"
#include "InventoryEventListener.generated.h"

UCLASS()
class UGlobalInventoryEventListener : public UObject
{
    GENERATED_BODY()

public:
    // --- Event Data for Item Added ---
    UPROPERTY()
    bool bItemAddedTriggered = false;
    UPROPERTY()
    UItemStaticData* AddedItemStaticData = nullptr;
    UPROPERTY()
    int32 AddedQuantity = 0;
    UPROPERTY()
    EItemChangeReason AddedChangeReason;
    
    UPROPERTY()
    bool bItemAddedToTaggedTriggered = false;
    UPROPERTY()
    FGameplayTag AddedSlotTag;
    UPROPERTY()
    UItemStaticData* AddedToTaggedItemStaticData = nullptr;
    UPROPERTY()
    int32 AddedToTaggedQuantity = 0;
    UPROPERTY()
    FTaggedItemBundle AddedToTaggedPreviousItem;
    UPROPERTY()
    EItemChangeReason AddedToTaggedChangeReason;

    // --- Event Data for Item Removed ---
    UPROPERTY()
    bool bItemRemovedTriggered = false;
    UPROPERTY()
    UItemStaticData* RemovedItemStaticData = nullptr;
    UPROPERTY()
    int32 RemovedQuantity = 0;
    UPROPERTY()
    EItemChangeReason RemovedChangeReason;

    
    UPROPERTY()
    bool bItemRemovedFromTaggedTriggered = false;
    UPROPERTY()
    FGameplayTag RemovedSlotTag;
    UPROPERTY()
    UItemStaticData* RemovedFromTaggedItemStaticData = nullptr;
    UPROPERTY()
    int32 RemovedFromTaggedQuantity = 0;
    UPROPERTY()
    EItemChangeReason RemovedFromTaggedChangeReason;
    

    // --- Event Data for Craft Confirmed ---
    UPROPERTY()
    bool bCraftConfirmedTriggered = false;
    UPROPERTY()
    TSubclassOf<UObject> CraftConfirmedObject;
    UPROPERTY()
    int32 CraftConfirmedQuantity = 0;

    // --- Event Data for Available Recipes Updated ---
    UPROPERTY()
    bool bAvailableRecipesUpdatedTriggered = false;

    // ---
    // This function subscribes the listener to all events on the provided inventory component.
    // ---
    UFUNCTION()
    void SubscribeToInventoryComponent(UInventoryComponent* InventoryComponent);

    void Clear();


    UFUNCTION()
    void HandleItemAddedToTaggedSlot(const FGameplayTag& InSlotTag, const UItemStaticData* InItemStaticData, int32 InQuantity, FTaggedItemBundle PreviousItem, EItemChangeReason InChangeReason);

    UFUNCTION()
    void HandleItemRemovedFromTaggedSlot(const FGameplayTag& InSlotTag, const UItemStaticData* InItemStaticData, int32 InQuantity, EItemChangeReason InChangeReason);

    UFUNCTION()
    void HandleItemAddedToContainer(const UItemStaticData* InItemStaticData, int32 InQuantity, EItemChangeReason InChangeReason);

    UFUNCTION()
    void HandleItemRemovedFromContainer(const UItemStaticData* InItemStaticData, int32 InQuantity, EItemChangeReason InChangeReason);

    UFUNCTION()
    void OnCraftConfirmed(TSubclassOf<UObject> InObject, int32 InQuantity);

    UFUNCTION()
    void OnAvailableRecipesUpdated();
};