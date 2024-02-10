// Author: Lucas Vilas-Boas
// Year: 2023

#include "Components/RancInventoryComponent.h"
#include "Management/RancInventoryFunctions.h"
#include "Management/RancInventorySettings.h"
#include "LogRancInventory.h"
#include <Engine/AssetManager.h>
#include <GameFramework/Actor.h>
#include <Algo/ForEach.h>
#include <Net/UnrealNetwork.h>
#include <Net/Core/PushModel/PushModel.h>

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RancInventoryComponent)
#endif

URancInventoryComponent::URancInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), MaxWeight(0.f), MaxNumItems(0), CurrentWeight(0.f)
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    bWantsInitializeComponent = true;
    SetIsReplicatedByDefault(true);

    if (const URancInventorySettings* const Settings = URancInventorySettings::Get())
    {
        MaxWeight = Settings->MaxWeight;
        MaxNumItems = Settings->MaxNumItems;
    }
}

void URancInventoryComponent::InitializeComponent()
{
    Super::InitializeComponent();

    // add all initial items to items
    if (GetOwnerRole() == ROLE_Authority)
    {
        for (const FRancInitialItem& InitialItem : InitialItems)
        {
            const URancItemData* Data = URancInventoryFunctions::GetSingleItemDataById(InitialItem.ItemId, {}, false);
            
            if (Data && Data->ItemId.IsValid())
                Items.Add(FRancItemInfo(Data->ItemId, InitialItem.Quantity));
        }
    }

    if (DropItemClass == nullptr)
    {
        DropItemClass = AWorldItem::StaticClass();
    }
}


void URancInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(URancInventoryComponent, Items, SharedParams);
}


void URancInventoryComponent::OnRep_Items()
{
    // Recalculate the total weight of the inventory after replication.
    UpdateWeight();

    // Notify other systems of the inventory update, possibly updating the UI.
    OnInventoryUpdated.Broadcast();

    // If the inventory is now empty, trigger the OnInventoryEmptied delegate.
    if (Items.IsEmpty())
    {
        OnInventoryEmptied.Broadcast();
    }
}

void URancInventoryComponent::AddItems(const FRancItemInfo& ItemInfo)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItems called on non-authority!"));
        return;
    }

    // Check if the inventory can receive the item
    if (!CanReceiveItem(ItemInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot receive item: %s"), *ItemInfo.ItemId.ToString());
        return;
    }

    bool bItemAdded = false;
    for (auto& ExistingItem : Items)
    {
        // If item exists and is stackable, increase the quantity
        if (ExistingItem.ItemId == ItemInfo.ItemId)
        {
            ExistingItem.Quantity += ItemInfo.Quantity;
            bItemAdded = true;
            break;
        }
    }

    // If item does not exist in the inventory, add it
    if (!bItemAdded)
    {
        Items.Add(ItemInfo);
    }

    // Update the current weight of the inventory
    UpdateWeight();

    OnInventoryItemAdded.Broadcast(ItemInfo);

    // Mark the Items array as dirty to ensure replication
    MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, Items, this);
}

bool URancInventoryComponent::RemoveItems(const FRancItemInfo& ItemInfo)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemoveItems called on non-authority!"));
        return false;
    }

    // Check if the inventory can give the item
    if (!ContainsItem(ItemInfo.ItemId, ItemInfo.Quantity))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot remove item: %s"), *ItemInfo.ItemId.ToString());
        return false;
    }

    for (int i = Items.Num() - 1; i >= 0; --i)
    {
        auto& ExistingItem = Items[i];
        if (ExistingItem.ItemId == ItemInfo.ItemId)
        {
            ExistingItem.Quantity -= ItemInfo.Quantity;

            // If the quantity drops to zero or below, remove the item from the inventory
            if (ExistingItem.Quantity <= 0)
            {
                Items.RemoveAt(i);
                break; // Assuming ItemId is unique and only one instance exists in the inventory
            }
        }
    }
    
    // Update the current weight of the inventory
    UpdateWeight();

    OnInventoryItemRemoved.Broadcast(ItemInfo);
    
    // Mark the Items array as dirty to ensure replication
    MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, Items, this);

    return true;
}

int32 URancInventoryComponent::DropItems(const FRancItemInfo& ItemInfo)
{
    auto ContainedItemInfo = FindItemById(ItemInfo.ItemId);
    int32 CountToDrop = FMath::Min(ItemInfo.Quantity, ContainedItemInfo.Quantity);

    if (CountToDrop <= 0)
    {
        return false;
    }

    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("DropItems called on non-authority!"));
        return false;
    }

    if (UWorld* World = GetWorld())
    {
        FActorSpawnParameters SpawnParams;
        if (AWorldItem* WorldItem = World->SpawnActorDeferred<AWorldItem>(DropItemClass,
                                                                  FTransform( GetOwner()->
                                                                  GetActorLocation() + GetOwner()->GetActorForwardVector() *
                                                                  DropDistance)))
        {
            WorldItem->SetItem(ItemInfo);
            WorldItem->FinishSpawning(FTransform(GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * DropDistance));
            
            ContainedItemInfo.Quantity -= CountToDrop;
            if (ContainedItemInfo.Quantity <= 0)
            {
                Items.Remove(ContainedItemInfo);
            }
            OnInventoryItemRemoved.Broadcast(ItemInfo);

            return CountToDrop;
        }
    }
    
    return 0;
}

float URancInventoryComponent::GetCurrentWeight() const
{
    return CurrentWeight;
}

float URancInventoryComponent::GetMaxWeight() const
{
    return MaxWeight <= 0.f ? MAX_flt : MaxWeight;
}

const FRancItemInfo& URancInventoryComponent::FindItemById(const FGameplayTag& ItemId) const
{
    for (const auto& Item : Items)
    {
        if (Item.ItemId == ItemId)
        {
            return Item;
        }
    }

    // If the item is not found, throw an error or return a reference to a static empty item info
    static FRancItemInfo EmptyItemInfo;
    UE_LOG(LogTemp, Warning, TEXT("Item with ID %s not found."), *ItemId.ToString());
    return EmptyItemInfo;
}

bool URancInventoryComponent::CanReceiveItem(const FRancItemInfo& ItemInfo) const
{
    // Check if adding this item would exceed the max number of unique items
    if (!ContainsItem(ItemInfo.ItemId) && Items.Num() >= MaxNumItems)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot receive item: Inventory is full."));
        return false;
    }

    // Calculate the additional weight this item would add
    if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemById(ItemInfo.ItemId))
    {
        float AdditionalWeight = ItemData->ItemWeight * ItemInfo.Quantity;
        if (CurrentWeight + AdditionalWeight > MaxWeight)
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot receive item: Exceeds max weight."));
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not find item data for item: %s"), *ItemInfo.ItemId.ToString());
        return false;
    }

    return true;
}

bool URancInventoryComponent::ContainsItem(const FGameplayTag& ItemId, int32 Quantity) const
{
    int32 TotalQuantity = 0;
    for (const auto& Item : Items)
    {
        if (Item.ItemId == ItemId)
        {
            TotalQuantity += Item.Quantity;
            if (TotalQuantity >= Quantity)
            {
                return true;
            }
        }
    }
    return false;
}

int32 URancInventoryComponent::GetCurrentItemCount() const
{
    int32 Count = 0;
    for (const auto& Item : Items)
    {
        Count += Item.Quantity;
    }
    return Count;
}

TArray<FRancItemInfo> URancInventoryComponent::GetAllItems() const
{
    return Items;
}

bool URancInventoryComponent::IsEmpty()
{
    return Items.Num() == 0;
}

void URancInventoryComponent::UpdateWeight()
{
    CurrentWeight = 0.0f; // Reset weight
    for (const auto& ItemInfo : Items)
    {
        if (const URancItemData* const ItemData = URancInventoryFunctions::GetItemById(ItemInfo.ItemId))
        {
            CurrentWeight += ItemData->ItemWeight * ItemInfo.Quantity;
        }
    }

    // This example does not handle the case where item data is not found, which might be important for ensuring accuracy.
}
