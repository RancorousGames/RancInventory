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
    // --- Event Data for Item Added to Container ---
    UPROPERTY()
    bool bItemAddedTriggered = false;
    UPROPERTY()
    TObjectPtr<const UItemStaticData> AddedItemStaticData = nullptr; // Use const pointer
    UPROPERTY()
    int32 AddedQuantity = 0;
    UPROPERTY()
    TArray<TObjectPtr<UItemInstanceData>> AddedInstances; // Capture added instances
    UPROPERTY()
    EItemChangeReason AddedChangeReason;

    // --- Event Data for Item Added to Tagged Slot ---
    UPROPERTY()
    bool bItemAddedToTaggedTriggered = false;
    UPROPERTY()
    FGameplayTag AddedSlotTag;
    UPROPERTY()
    TObjectPtr<const UItemStaticData> AddedToTaggedItemStaticData = nullptr; // Use const pointer
    UPROPERTY()
    int32 AddedToTaggedQuantity = 0;
    UPROPERTY()
    TArray<TObjectPtr<UItemInstanceData>> AddedToTaggedInstances; // Capture added instances for tagged slot
    UPROPERTY()
    FTaggedItemBundle AddedToTaggedPreviousItem;
    UPROPERTY()
    EItemChangeReason AddedToTaggedChangeReason;

    // --- Event Data for Item Removed from Container ---
    UPROPERTY()
    bool bItemRemovedTriggered = false;
    UPROPERTY()
    TObjectPtr<const UItemStaticData> RemovedItemStaticData = nullptr; // Use const pointer
    UPROPERTY()
    int32 RemovedQuantity = 0;
    UPROPERTY()
    TArray<TObjectPtr<UItemInstanceData>> RemovedInstances; // Capture removed instances
    UPROPERTY()
    EItemChangeReason RemovedChangeReason;

    // --- Event Data for Item Removed from Tagged Slot ---
    UPROPERTY()
    bool bItemRemovedFromTaggedTriggered = false;
    UPROPERTY()
    FGameplayTag RemovedSlotTag;
    UPROPERTY()
    TObjectPtr<const UItemStaticData> RemovedFromTaggedItemStaticData = nullptr; // Use const pointer
    UPROPERTY()
    int32 RemovedFromTaggedQuantity = 0;
    UPROPERTY()
    TArray<TObjectPtr<UItemInstanceData>> RemovedFromTaggedInstances; // Capture removed instances for tagged slot
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

    // --- Subscription ---
    UFUNCTION()
    void SubscribeToInventoryComponent(UInventoryComponent* InventoryComponent)
    {
        if (!InventoryComponent) return;

        // Bind Container Events
        InventoryComponent->OnItemAddedToContainer.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemAddedToContainer);
        InventoryComponent->OnItemRemovedFromContainer.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemRemovedFromContainer);

        // Bind Tagged Slot Events
        InventoryComponent->OnItemAddedToTaggedSlot.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemAddedToTaggedSlot);
        InventoryComponent->OnItemRemovedFromTaggedSlot.AddDynamic(this, &UGlobalInventoryEventListener::HandleItemRemovedFromTaggedSlot);

        // Bind Crafting Events
        InventoryComponent->OnCraftConfirmed.AddDynamic(this, &UGlobalInventoryEventListener::OnCraftConfirmed);
        InventoryComponent->OnAvailableRecipesUpdated.AddDynamic(this, &UGlobalInventoryEventListener::OnAvailableRecipesUpdated);
    }

    // --- Reset State ---
    void Clear()
    {
        // Reset Added Container flags and data
        bItemAddedTriggered = false;
        AddedItemStaticData = nullptr;
        AddedQuantity = 0;
        AddedInstances.Empty();
        AddedChangeReason = EItemChangeReason::Added; // Default or choose appropriate

        // Reset Added Tagged flags and data
        bItemAddedToTaggedTriggered = false;
        AddedSlotTag = FGameplayTag::EmptyTag;
        AddedToTaggedItemStaticData = nullptr;
        AddedToTaggedQuantity = 0;
        AddedToTaggedInstances.Empty();
        AddedToTaggedPreviousItem = FTaggedItemBundle::EmptyItemInstance;
        AddedToTaggedChangeReason = EItemChangeReason::Added;

        // Reset Removed Container flags and data
        bItemRemovedTriggered = false;
        RemovedItemStaticData = nullptr;
        RemovedQuantity = 0;
        RemovedInstances.Empty();
        RemovedChangeReason = EItemChangeReason::Removed;

        // Reset Removed Tagged flags and data
        bItemRemovedFromTaggedTriggered = false;
        RemovedSlotTag = FGameplayTag::EmptyTag;
        RemovedFromTaggedItemStaticData = nullptr;
        RemovedFromTaggedQuantity = 0;
        RemovedFromTaggedInstances.Empty();
        RemovedFromTaggedChangeReason = EItemChangeReason::Removed;

        // Reset Crafting flags and data
        bCraftConfirmedTriggered = false;
        CraftConfirmedObject = nullptr;
        CraftConfirmedQuantity = 0;

        // Reset Recipe Update flag
        bAvailableRecipesUpdatedTriggered = false;
    }

    // --- Event Handlers ---
    UFUNCTION()
    void HandleItemAddedToTaggedSlot(const FGameplayTag& InSlotTag, const UItemStaticData* InItemStaticData, int32 InQuantity, const TArray<UItemInstanceData*>& InInstancesAdded, FTaggedItemBundle PreviousItem, EItemChangeReason InChangeReason)
    {
        bItemAddedToTaggedTriggered = true;
        AddedSlotTag = InSlotTag;
        AddedToTaggedItemStaticData = InItemStaticData;
        AddedToTaggedQuantity = InQuantity;
        AddedToTaggedInstances = InInstancesAdded; // Capture instances
        AddedToTaggedPreviousItem = PreviousItem;
        AddedToTaggedChangeReason = InChangeReason;
    }

    UFUNCTION()
    void HandleItemRemovedFromTaggedSlot(const FGameplayTag& InSlotTag, const UItemStaticData* InItemStaticData, int32 InQuantity, const TArray<UItemInstanceData*>& InInstancesRemoved, EItemChangeReason InChangeReason)
    {
        bItemRemovedFromTaggedTriggered = true;
        RemovedSlotTag = InSlotTag;
        RemovedFromTaggedItemStaticData = InItemStaticData;
        RemovedFromTaggedQuantity = InQuantity;
        RemovedFromTaggedInstances = InInstancesRemoved; // Capture instances
        RemovedFromTaggedChangeReason = InChangeReason;
    }

    UFUNCTION()
    void HandleItemAddedToContainer(const UItemStaticData* InItemStaticData, int32 InQuantity,  const TArray<UItemInstanceData*>& InInstancesAdded, EItemChangeReason InChangeReason)
    {
        bItemAddedTriggered = true;
        AddedItemStaticData = InItemStaticData;
        AddedQuantity = InQuantity;
        AddedInstances = InInstancesAdded; // Capture instances
        AddedChangeReason = InChangeReason;
    }

    UFUNCTION()
    void HandleItemRemovedFromContainer(const UItemStaticData* InItemStaticData, int32 InQuantity, const TArray<UItemInstanceData*>& InInstancesRemoved, EItemChangeReason InChangeReason)
    {
        bItemRemovedTriggered = true;
        RemovedItemStaticData = InItemStaticData;
        RemovedQuantity = InQuantity;
        RemovedInstances = InInstancesRemoved; // Capture instances
        RemovedChangeReason = InChangeReason;
    }

    UFUNCTION()
    void OnCraftConfirmed(TSubclassOf<UObject> InObject, int32 InQuantity)
    {
        bCraftConfirmedTriggered = true;
        CraftConfirmedObject = InObject;
        CraftConfirmedQuantity = InQuantity;
    }

    UFUNCTION()
    void OnAvailableRecipesUpdated()
    {
        bAvailableRecipesUpdatedTriggered = true;
    }
};