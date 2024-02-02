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

URancInventoryComponent::URancInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), CurrentWeight(0.f), MaxWeight(0.f), MaxNumItems(0)
{
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    SetIsReplicatedByDefault(true);

    if (const URancInventorySettings* const Settings = URancInventorySettings::Get())
    {
        bAllowEmptySlots = Settings->bAllowEmptySlots;
        MaxWeight = Settings->MaxWeight;
        MaxNumItems = Settings->MaxNumItems;
    }
}

float URancInventoryComponent::GetCurrentWeight() const
{
    return CurrentWeight;
}

float URancInventoryComponent::GetMaxWeight() const
{
    return MaxWeight <= 0.f ? MAX_flt : MaxWeight;
}

int32 URancInventoryComponent::GetCurrentNumItems() const
{
    return RancItems.Num();
}

int32 URancInventoryComponent::GetMaxNumItems() const
{
    return MaxNumItems <= 0 ? MAX_int32 : MaxNumItems;
}

TArray<FRancItemInfo> URancInventoryComponent::GetItemsArray() const
{
    return RancItems;
}

FRancItemInfo& URancInventoryComponent::GetItemReferenceAt(const int32 Index)
{
    return RancItems[Index];
}

FRancItemInfo URancInventoryComponent::GetItemCopyAt(const int32 Index) const
{
    return RancItems[Index];
}

bool URancInventoryComponent::CanReceiveItem(const FRancItemInfo InItemInfo) const
{
    if (!URancInventoryFunctions::IsItemValid(InItemInfo))
    {
        return false;
    }

    bool bOutput = RancItems.Num() <= GetMaxNumItems();

    if (const URancItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(InItemInfo.ItemId, { "Data" }))
    {
        bOutput = bOutput && ((GetCurrentWeight() + (ItemData->ItemWeight * InItemInfo.Quantity)) <= GetMaxWeight());
    }

    if (!bOutput)
    {
        UE_LOG(LogRancInventory, Warning, TEXT("%s: Actor %s cannot receive %d item(s) with name '%s'"), *FString(__func__), *GetOwner()->GetName(), InItemInfo.Quantity, *InItemInfo.ItemId.ToString());
    }

    return bOutput;
}

bool URancInventoryComponent::CanGiveItem(const FRancItemInfo InItemInfo) const
{
    if (!URancInventoryFunctions::IsItemValid(InItemInfo))
    {
        return false;
    }

    if (TArray<int32> InIndex; FindAllItemIndexesWithInfo(InItemInfo, InIndex, FGameplayTagContainer::EmptyContainer))
    {
        int32 Quantity = 0u;
        for (const int32& Index : InIndex)
        {
            Quantity += RancItems[Index].Quantity;
        }

        return Quantity >= InItemInfo.Quantity;
    }

    UE_LOG(LogRancInventory, Warning, TEXT("%s: Actor %s cannot give %d item(s) with name '%s'"), *FString(__func__), *GetOwner()->GetName(), InItemInfo.Quantity, *InItemInfo.ItemId.ToString());

    return false;
}

void URancInventoryComponent::SortInventory(const ERancInventorySortingMode Mode, const ERancInventorySortingOrientation Orientation)
{
    const auto SortByOrientation = [Orientation](const auto A, const auto B) {
        switch (Orientation)
        {
        case ERancInventorySortingOrientation::Ascending:
            return A < B;

        case ERancInventorySortingOrientation::Descending:
            return A > B;

        default:
            return false;
        }

        return false;
        };

    switch (Mode)
    {
    case ERancInventorySortingMode::ID:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.ItemId, B.ItemId);
            }
        );
        break;

    case ERancInventorySortingMode::Name:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemName.ToString(), ItemDataB->ItemName.ToString());
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::Type:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemType, ItemDataB->ItemType);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::IndividualValue:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemValue, ItemDataB->ItemValue);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::StackValue:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemValue * A.Quantity, ItemDataB->ItemValue * B.Quantity);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::IndividualWeight:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemWeight, ItemDataB->ItemWeight);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::StackWeight:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const URancItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const URancItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemWeight * A.Quantity, ItemDataB->ItemWeight * B.Quantity);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::Quantity:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.Quantity, B.Quantity);
            }
        );
        break;

    case ERancInventorySortingMode::Level:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.Level, B.Level);
            }
        );
        break;

    case ERancInventorySortingMode::Tags:
        RancItems.Sort(
            [SortByOrientation](const FRancItemInfo& A, const FRancItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.Tags.Num(), B.Tags.Num());
            }
        );
        break;

    default:
        break;
    }
}

void URancInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    RefreshInventory();
}

void URancInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(URancInventoryComponent, RancItems, SharedParams);
}

void URancInventoryComponent::RefreshInventory()
{
    ForceWeightUpdate();
    ForceInventoryValidation();
}

void URancInventoryComponent::ForceWeightUpdate()
{
    float NewWeigth = 0.f;
    for (const FRancItemInfo& Iterator : RancItems)
    {
        if (const URancItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(Iterator.ItemId, { "Data" }))
        {
            NewWeigth += ItemData->ItemWeight * Iterator.Quantity;
        }
    }

    CurrentWeight = NewWeigth;
}

void URancInventoryComponent::ForceInventoryValidation()
{
    TArray<FRancItemInfo> NewItems;
    TArray<int32> IndexesToRemove;

    for (int32 i = 0; i < RancItems.Num(); ++i)
    {
        if (RancItems[i].Quantity <= 0)
        {
            IndexesToRemove.Add(i);
        }

        else if (RancItems[i].Quantity > 1)
        {
            if (!URancInventoryFunctions::IsItemStackable(RancItems[i]))
            {
                for (int32 j = 0; j < RancItems[i].Quantity; ++j)
                {
                    NewItems.Add(FRancItemInfo(RancItems[i].ItemId, 1, RancItems[i].Tags));
                }

                IndexesToRemove.Add(i);
            }
        }
    }

    if (!URancInventoryFunctions::HasEmptyParam(IndexesToRemove))
    {
        for (const int32& Iterator : IndexesToRemove)
        {
            if (bAllowEmptySlots)
            {
                RancItems[Iterator] = FRancItemInfo::EmptyItemInfo;
            }
            else
            {
                RancItems.RemoveAt(Iterator, 1, false);
            }
        }
    }
    if (!URancInventoryFunctions::HasEmptyParam(NewItems))
    {
        RancItems.Append(NewItems);
    }

    NotifyInventoryChange();
}

bool URancInventoryComponent::FindFirstItemIndexWithInfo(const FRancItemInfo InItemInfo, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset) const
{
    for (int32 Iterator = Offset; Iterator < RancItems.Num(); ++Iterator)
    {
        FRancItemInfo InParamCopy = InItemInfo;
        InParamCopy.Tags.RemoveTags(IgnoreTags);

        FRancItemInfo InExistingCopy = RancItems[Iterator];
        InExistingCopy.Tags.RemoveTags(IgnoreTags);

        if (InExistingCopy == InParamCopy)
        {
            OutIndex = Iterator;
            return true;
        }
    }

    OutIndex = INDEX_NONE;
    return false;
}

bool URancInventoryComponent::FindFirstItemIndexWithTags(const FGameplayTagContainer WithTags, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset) const
{
    for (int32 Iterator = Offset; Iterator < RancItems.Num(); ++Iterator)
    {
        FRancItemInfo InExistingCopy = RancItems[Iterator];
        InExistingCopy.Tags.RemoveTags(IgnoreTags);

        if (InExistingCopy.Tags.HasAllExact(WithTags))
        {
            OutIndex = Iterator;
            return true;
        }
    }

    OutIndex = INDEX_NONE;
    return false;
}

bool URancInventoryComponent::FindFirstItemIndexWithId(const FPrimaryRancItemId InId, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset) const
{
    for (int32 Iterator = Offset; Iterator < RancItems.Num(); ++Iterator)
    {
        if (!RancItems[Iterator].Tags.HasAny(IgnoreTags) && RancItems[Iterator].ItemId == InId)
        {
            OutIndex = Iterator;
            return true;
        }
    }

    OutIndex = INDEX_NONE;
    return false;
}

bool URancInventoryComponent::FindAllItemIndexesWithInfo(const FRancItemInfo InItemInfo, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const
{
    for (auto Iterator = RancItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        if (IgnoreTags.IsEmpty() && *Iterator == InItemInfo)
        {
            OutIndexes.Add(Iterator.GetIndex());
            continue;
        }

        FRancItemInfo InItCopy(*Iterator);
        InItCopy.Tags.RemoveTags(IgnoreTags);

        FRancItemInfo InParamCopy(InItemInfo);
        InParamCopy.Tags.RemoveTags(IgnoreTags);

        if (InItCopy == InParamCopy)
        {
            OutIndexes.Add(Iterator.GetIndex());
        }
    }

    return !URancInventoryFunctions::HasEmptyParam(OutIndexes);
}

bool URancInventoryComponent::FindAllItemIndexesWithTags(const FGameplayTagContainer WithTags, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const
{
    for (auto Iterator = RancItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        FRancItemInfo InCopy(*Iterator);
        if (!IgnoreTags.IsEmpty())
        {
            InCopy.Tags.RemoveTags(IgnoreTags);
        }

        if (InCopy.Tags.HasAll(WithTags))
        {
            OutIndexes.Add(Iterator.GetIndex());
        }
    }

    return !URancInventoryFunctions::HasEmptyParam(OutIndexes);
}

bool URancInventoryComponent::FindAllItemIndexesWithId(const FPrimaryRancItemId InId, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const
{
    for (auto Iterator = RancItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        if (!Iterator->Tags.HasAll(IgnoreTags) && Iterator->ItemId == InId)
        {
            OutIndexes.Add(Iterator.GetIndex());
        }
    }

    return !URancInventoryFunctions::HasEmptyParam(OutIndexes);
}

bool URancInventoryComponent::ContainsItem(const FRancItemInfo InItemInfo, const bool bIgnoreTags) const
{
    return RancItems.FindByPredicate(
        [&InItemInfo, &bIgnoreTags](const FRancItemInfo& InInfo)
        {
            if (bIgnoreTags)
            {
                return InInfo.ItemId == InItemInfo.ItemId;
            }

            return InInfo == InItemInfo;
        }
    ) != nullptr;
}

bool URancInventoryComponent::IsInventoryEmpty() const
{
    bool bOutput = true;

    for (const FRancItemInfo& Iterator : RancItems)
    {
        if (Iterator.Quantity > 0)
        {
            bOutput = false;
            break;
        }
    }

    return bOutput;
}

void URancInventoryComponent::DebugInventory()
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("%s"), *FString(__func__));
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Owning Actor: %s"), *GetOwner()->GetName());

    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Weight: %d"), CurrentWeight);
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Num: %d"), RancItems.Num());
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Size: %d"), RancItems.GetAllocatedSize());

    for (const FRancItemInfo& Iterator : RancItems)
    {
        UE_LOG(LogRancInventory_Internal, Warning, TEXT("Item: %s"), *Iterator.ItemId.ToString());
        UE_LOG(LogRancInventory_Internal, Warning, TEXT("Quantity: %d"), Iterator.Quantity);

        for (const FGameplayTag& Tag : Iterator.Tags)
        {
            UE_LOG(LogRancInventory_Internal, Warning, TEXT("Tag: %s"), *Tag.ToString());
        }
    }

    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Component Memory Size: %d"), GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal));
#endif
}

void URancInventoryComponent::ClearInventory_Implementation()
{
    UE_LOG(LogRancInventory, Display, TEXT("%s: Cleaning %s's inventory"), *FString(__func__), *GetOwner()->GetName());

    RancItems.Empty();
    CurrentWeight = 0.f;
}

void URancInventoryComponent::GetItemIndexesFrom_Implementation(URancInventoryComponent* OtherInventory, const TArray<int32>& ItemIndexes)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    TArray<FRancItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (OtherInventory->RancItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(OtherInventory->RancItems[Iterator]);
        }
    }

    GetItemsFrom_Implementation(OtherInventory, Modifiers);
}

void URancInventoryComponent::GiveItemIndexesTo_Implementation(URancInventoryComponent* OtherInventory, const TArray<int32>& ItemIndexes)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    TArray<FRancItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (OtherInventory->RancItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(RancItems[Iterator]);
        }
    }

    GiveItemsTo_Implementation(OtherInventory, Modifiers);
}

void URancInventoryComponent::GetItemsFrom_Implementation(URancInventoryComponent* OtherInventory, const TArray<FRancItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    if (!IsValid(OtherInventory))
    {
        return;
    }

    const TArray<FRancItemInfo> TradeableItems = URancInventoryFunctions::FilterTradeableItems(OtherInventory, this, Items);

    OtherInventory->UpdateRancItems(TradeableItems, ERancInventoryUpdateOperation::Remove);
    UpdateRancItems(TradeableItems, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::GiveItemsTo_Implementation(URancInventoryComponent* OtherInventory, const TArray<FRancItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    if (!IsValid(OtherInventory))
    {
        return;
    }

    const TArray<FRancItemInfo> TradeableItems = URancInventoryFunctions::FilterTradeableItems(this, OtherInventory, Items);

    UpdateRancItems(TradeableItems, ERancInventoryUpdateOperation::Remove);
    OtherInventory->UpdateRancItems(TradeableItems, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::DiscardItemIndexes_Implementation(const TArray<int32>& ItemIndexes)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(ItemIndexes))
    {
        return;
    }

    TArray<FRancItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (RancItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(RancItems[Iterator]);
        }
    }

    DiscardItems(Modifiers);
}

void URancInventoryComponent::DiscardItems_Implementation(const TArray<FRancItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(Items))
    {
        return;
    }

    UpdateRancItems(Items, ERancInventoryUpdateOperation::Remove);
}

void URancInventoryComponent::AddItems_Implementation(const TArray<FRancItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(Items))
    {
        return;
    }

    UpdateRancItems(Items, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::UpdateRancItems(const TArray<FRancItemInfo>& Modifiers, const ERancInventoryUpdateOperation Operation)
{
    TArray<FItemModifierData> ModifierDataArr;

    const FString OpStr = Operation == ERancInventoryUpdateOperation::Add ? "Add" : "Remove";
    const FString OpPred = Operation == ERancInventoryUpdateOperation::Add ? "to" : "from";

    uint32 SearchOffset = 0;
    FRancItemInfo LastCheckedItem;
    for (const FRancItemInfo& Iterator : Modifiers)
    {
        UE_LOG(LogRancInventory_Internal, Display, TEXT("%s: %s %d item(s) with name '%s' %s inventory"), *FString(__func__), *OpStr, Iterator.Quantity, *Iterator.ItemId.ToString(), *OpPred);

        if (Iterator != LastCheckedItem)
        {
            SearchOffset = 0u;
        }

        int32 Index;
        if (FindFirstItemIndexWithInfo(Iterator, Index, FGameplayTagContainer::EmptyContainer, SearchOffset) && Operation == ERancInventoryUpdateOperation::Remove)
        {
            SearchOffset = Index + 1u;
        }

        ModifierDataArr.Add(FItemModifierData(Iterator, Index));
        LastCheckedItem = Iterator;
    }

    switch (Operation)
    {
    case ERancInventoryUpdateOperation::Add:
        Server_ProcessInventoryAddition_Internal(ModifierDataArr);
        break;

    case ERancInventoryUpdateOperation::Remove:
        Server_ProcessInventoryRemoval_Internal(ModifierDataArr);
        break;

    default:
        break;
    }
}

void URancInventoryComponent::Server_ProcessInventoryAddition_Internal_Implementation(const TArray<FItemModifierData>& Modifiers)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    for (const FItemModifierData& Iterator : Modifiers)
    {
        if (const bool bIsStackable = URancInventoryFunctions::IsItemStackable(Iterator.ItemInfo);
            bIsStackable && Iterator.Index != INDEX_NONE)
        {
            RancItems[Iterator.Index].Quantity += Iterator.ItemInfo.Quantity;
        }
        else if (!bIsStackable)
        {
            for (int32 i = 0u; i < Iterator.ItemInfo.Quantity; ++i)
            {
                const FRancItemInfo ItemInfo{ Iterator.ItemInfo.ItemId, 1, Iterator.ItemInfo.Tags };

                RancItems.Add(ItemInfo);
            }
        }
        else
        {
            RancItems.Add(Iterator.ItemInfo);
        }
    }

    NotifyInventoryChange();
}

void URancInventoryComponent::Server_ProcessInventoryRemoval_Internal_Implementation(const TArray<FItemModifierData>& Modifiers)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    for (const FItemModifierData& Iterator : Modifiers)
    {
        if (Iterator.Index == INDEX_NONE || Iterator.Index > RancItems.Num())
        {
            UE_LOG(LogRancInventory_Internal, Warning, TEXT("%s: Item with name '%s' not found in inventory"), *FString(__func__), *Iterator.ItemInfo.ItemId.ToString());

            continue;
        }

        RancItems[Iterator.Index].Quantity -= Iterator.ItemInfo.Quantity;
    }

    if (bAllowEmptySlots)
    {
        Algo::ForEach(RancItems, [](FRancItemInfo& InInfo)
            {
                if (InInfo.Quantity <= 0)
                {
                    InInfo = FRancItemInfo::EmptyItemInfo;
                }
            });
    }
    else
    {
        RancItems.RemoveAll(
            [](const FRancItemInfo& InInfo)
            {
                return InInfo.Quantity <= 0;
            }
        );
    }

    NotifyInventoryChange();
}

void URancInventoryComponent::OnRep_RancItems()
{
    if (const int32 LastValidIndex = RancItems.FindLastByPredicate([](const FRancItemInfo& Item) { return URancInventoryFunctions::IsItemValid(Item); }); LastValidIndex != INDEX_NONE && RancItems.IsValidIndex(LastValidIndex + 1))
    {
        RancItems.RemoveAt(LastValidIndex + 1, RancItems.Num() - LastValidIndex - 1, false);
    }
    else if (LastValidIndex == INDEX_NONE && !URancInventoryFunctions::HasEmptyParam(RancItems))
    {
        RancItems.Empty();
    }

    RancItems.Shrink();

    if (IsInventoryEmpty())
    {
        RancItems.Empty();

        CurrentWeight = 0.f;
        OnInventoryEmpty.Broadcast();
    }
    else
    {
        UpdateWeight();
    }

    OnInventoryUpdate.Broadcast();
}

void URancInventoryComponent::NotifyInventoryChange()
{
    if (GetOwnerRole() == ROLE_Authority)
    {
        OnRep_RancItems();
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, RancItems, this);
}

void URancInventoryComponent::UpdateWeight_Implementation()
{
    float NewWeight = 0.f;
    for (const FRancItemInfo& Iterator : RancItems)
    {
        if (const URancItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(Iterator.ItemId, { "Data" }))
        {
            NewWeight += ItemData->ItemWeight * Iterator.Quantity;
        }
    }

    CurrentWeight = FMath::Clamp(NewWeight, 0.f, MAX_FLT);
}