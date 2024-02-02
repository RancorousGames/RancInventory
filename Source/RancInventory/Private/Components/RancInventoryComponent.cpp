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
    return ElementusItems.Num();
}

int32 URancInventoryComponent::GetMaxNumItems() const
{
    return MaxNumItems <= 0 ? MAX_int32 : MaxNumItems;
}

TArray<FElementusItemInfo> URancInventoryComponent::GetItemsArray() const
{
    return ElementusItems;
}

FElementusItemInfo& URancInventoryComponent::GetItemReferenceAt(const int32 Index)
{
    return ElementusItems[Index];
}

FElementusItemInfo URancInventoryComponent::GetItemCopyAt(const int32 Index) const
{
    return ElementusItems[Index];
}

bool URancInventoryComponent::CanReceiveItem(const FElementusItemInfo InItemInfo) const
{
    if (!URancInventoryFunctions::IsItemValid(InItemInfo))
    {
        return false;
    }

    bool bOutput = ElementusItems.Num() <= GetMaxNumItems();

    if (const UElementusItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(InItemInfo.ItemId, { "Data" }))
    {
        bOutput = bOutput && ((GetCurrentWeight() + (ItemData->ItemWeight * InItemInfo.Quantity)) <= GetMaxWeight());
    }

    if (!bOutput)
    {
        UE_LOG(LogRancInventory, Warning, TEXT("%s: Actor %s cannot receive %d item(s) with name '%s'"), *FString(__func__), *GetOwner()->GetName(), InItemInfo.Quantity, *InItemInfo.ItemId.ToString());
    }

    return bOutput;
}

bool URancInventoryComponent::CanGiveItem(const FElementusItemInfo InItemInfo) const
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
            Quantity += ElementusItems[Index].Quantity;
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
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.ItemId, B.ItemId);
            }
        );
        break;

    case ERancInventorySortingMode::Name:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemName.ToString(), ItemDataB->ItemName.ToString());
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::Type:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemType, ItemDataB->ItemType);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::IndividualValue:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemValue, ItemDataB->ItemValue);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::StackValue:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemValue * A.Quantity, ItemDataB->ItemValue * B.Quantity);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::IndividualWeight:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemWeight, ItemDataB->ItemWeight);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::StackWeight:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                if (!URancInventoryFunctions::IsItemValid(A))
                {
                    return false;
                }

                if (const UElementusItemData* const ItemDataA = URancInventoryFunctions::GetSingleItemDataById(A.ItemId, { "Data" }))
                {
                    if (const UElementusItemData* const ItemDataB = URancInventoryFunctions::GetSingleItemDataById(B.ItemId, { "Data" }))
                    {
                        return SortByOrientation(ItemDataA->ItemWeight * A.Quantity, ItemDataB->ItemWeight * B.Quantity);
                    }
                }

                return false;
            }
        );
        break;

    case ERancInventorySortingMode::Quantity:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.Quantity, B.Quantity);
            }
        );
        break;

    case ERancInventorySortingMode::Level:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
            {
                return URancInventoryFunctions::IsItemValid(A) && SortByOrientation(A.Level, B.Level);
            }
        );
        break;

    case ERancInventorySortingMode::Tags:
        ElementusItems.Sort(
            [SortByOrientation](const FElementusItemInfo& A, const FElementusItemInfo& B)
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

    DOREPLIFETIME_WITH_PARAMS_FAST(URancInventoryComponent, ElementusItems, SharedParams);
}

void URancInventoryComponent::RefreshInventory()
{
    ForceWeightUpdate();
    ForceInventoryValidation();
}

void URancInventoryComponent::ForceWeightUpdate()
{
    float NewWeigth = 0.f;
    for (const FElementusItemInfo& Iterator : ElementusItems)
    {
        if (const UElementusItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(Iterator.ItemId, { "Data" }))
        {
            NewWeigth += ItemData->ItemWeight * Iterator.Quantity;
        }
    }

    CurrentWeight = NewWeigth;
}

void URancInventoryComponent::ForceInventoryValidation()
{
    TArray<FElementusItemInfo> NewItems;
    TArray<int32> IndexesToRemove;

    for (int32 i = 0; i < ElementusItems.Num(); ++i)
    {
        if (ElementusItems[i].Quantity <= 0)
        {
            IndexesToRemove.Add(i);
        }

        else if (ElementusItems[i].Quantity > 1)
        {
            if (!URancInventoryFunctions::IsItemStackable(ElementusItems[i]))
            {
                for (int32 j = 0; j < ElementusItems[i].Quantity; ++j)
                {
                    NewItems.Add(FElementusItemInfo(ElementusItems[i].ItemId, 1, ElementusItems[i].Tags));
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
                ElementusItems[Iterator] = FElementusItemInfo::EmptyItemInfo;
            }
            else
            {
                ElementusItems.RemoveAt(Iterator, 1, false);
            }
        }
    }
    if (!URancInventoryFunctions::HasEmptyParam(NewItems))
    {
        ElementusItems.Append(NewItems);
    }

    NotifyInventoryChange();
}

bool URancInventoryComponent::FindFirstItemIndexWithInfo(const FElementusItemInfo InItemInfo, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset) const
{
    for (int32 Iterator = Offset; Iterator < ElementusItems.Num(); ++Iterator)
    {
        FElementusItemInfo InParamCopy = InItemInfo;
        InParamCopy.Tags.RemoveTags(IgnoreTags);

        FElementusItemInfo InExistingCopy = ElementusItems[Iterator];
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
    for (int32 Iterator = Offset; Iterator < ElementusItems.Num(); ++Iterator)
    {
        FElementusItemInfo InExistingCopy = ElementusItems[Iterator];
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

bool URancInventoryComponent::FindFirstItemIndexWithId(const FPrimaryElementusItemId InId, int32& OutIndex, const FGameplayTagContainer& IgnoreTags, const int32 Offset) const
{
    for (int32 Iterator = Offset; Iterator < ElementusItems.Num(); ++Iterator)
    {
        if (!ElementusItems[Iterator].Tags.HasAny(IgnoreTags) && ElementusItems[Iterator].ItemId == InId)
        {
            OutIndex = Iterator;
            return true;
        }
    }

    OutIndex = INDEX_NONE;
    return false;
}

bool URancInventoryComponent::FindAllItemIndexesWithInfo(const FElementusItemInfo InItemInfo, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const
{
    for (auto Iterator = ElementusItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        if (IgnoreTags.IsEmpty() && *Iterator == InItemInfo)
        {
            OutIndexes.Add(Iterator.GetIndex());
            continue;
        }

        FElementusItemInfo InItCopy(*Iterator);
        InItCopy.Tags.RemoveTags(IgnoreTags);

        FElementusItemInfo InParamCopy(InItemInfo);
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
    for (auto Iterator = ElementusItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        FElementusItemInfo InCopy(*Iterator);
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

bool URancInventoryComponent::FindAllItemIndexesWithId(const FPrimaryElementusItemId InId, TArray<int32>& OutIndexes, const FGameplayTagContainer& IgnoreTags) const
{
    for (auto Iterator = ElementusItems.CreateConstIterator(); Iterator; ++Iterator)
    {
        if (!Iterator->Tags.HasAll(IgnoreTags) && Iterator->ItemId == InId)
        {
            OutIndexes.Add(Iterator.GetIndex());
        }
    }

    return !URancInventoryFunctions::HasEmptyParam(OutIndexes);
}

bool URancInventoryComponent::ContainsItem(const FElementusItemInfo InItemInfo, const bool bIgnoreTags) const
{
    return ElementusItems.FindByPredicate(
        [&InItemInfo, &bIgnoreTags](const FElementusItemInfo& InInfo)
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

    for (const FElementusItemInfo& Iterator : ElementusItems)
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
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Num: %d"), ElementusItems.Num());
    UE_LOG(LogRancInventory_Internal, Warning, TEXT("Size: %d"), ElementusItems.GetAllocatedSize());

    for (const FElementusItemInfo& Iterator : ElementusItems)
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

    ElementusItems.Empty();
    CurrentWeight = 0.f;
}

void URancInventoryComponent::GetItemIndexesFrom_Implementation(URancInventoryComponent* OtherInventory, const TArray<int32>& ItemIndexes)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    TArray<FElementusItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (OtherInventory->ElementusItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(OtherInventory->ElementusItems[Iterator]);
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

    TArray<FElementusItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (OtherInventory->ElementusItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(ElementusItems[Iterator]);
        }
    }

    GiveItemsTo_Implementation(OtherInventory, Modifiers);
}

void URancInventoryComponent::GetItemsFrom_Implementation(URancInventoryComponent* OtherInventory, const TArray<FElementusItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    if (!IsValid(OtherInventory))
    {
        return;
    }

    const TArray<FElementusItemInfo> TradeableItems = URancInventoryFunctions::FilterTradeableItems(OtherInventory, this, Items);

    OtherInventory->UpdateElementusItems(TradeableItems, ERancInventoryUpdateOperation::Remove);
    UpdateElementusItems(TradeableItems, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::GiveItemsTo_Implementation(URancInventoryComponent* OtherInventory, const TArray<FElementusItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    if (!IsValid(OtherInventory))
    {
        return;
    }

    const TArray<FElementusItemInfo> TradeableItems = URancInventoryFunctions::FilterTradeableItems(this, OtherInventory, Items);

    UpdateElementusItems(TradeableItems, ERancInventoryUpdateOperation::Remove);
    OtherInventory->UpdateElementusItems(TradeableItems, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::DiscardItemIndexes_Implementation(const TArray<int32>& ItemIndexes)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(ItemIndexes))
    {
        return;
    }

    TArray<FElementusItemInfo> Modifiers;
    for (const int32& Iterator : ItemIndexes)
    {
        if (ElementusItems.IsValidIndex(Iterator))
        {
            Modifiers.Add(ElementusItems[Iterator]);
        }
    }

    DiscardItems(Modifiers);
}

void URancInventoryComponent::DiscardItems_Implementation(const TArray<FElementusItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(Items))
    {
        return;
    }

    UpdateElementusItems(Items, ERancInventoryUpdateOperation::Remove);
}

void URancInventoryComponent::AddItems_Implementation(const TArray<FElementusItemInfo>& Items)
{
    if (GetOwnerRole() != ROLE_Authority || URancInventoryFunctions::HasEmptyParam(Items))
    {
        return;
    }

    UpdateElementusItems(Items, ERancInventoryUpdateOperation::Add);
}

void URancInventoryComponent::UpdateElementusItems(const TArray<FElementusItemInfo>& Modifiers, const ERancInventoryUpdateOperation Operation)
{
    TArray<FItemModifierData> ModifierDataArr;

    const FString OpStr = Operation == ERancInventoryUpdateOperation::Add ? "Add" : "Remove";
    const FString OpPred = Operation == ERancInventoryUpdateOperation::Add ? "to" : "from";

    uint32 SearchOffset = 0;
    FElementusItemInfo LastCheckedItem;
    for (const FElementusItemInfo& Iterator : Modifiers)
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
            ElementusItems[Iterator.Index].Quantity += Iterator.ItemInfo.Quantity;
        }
        else if (!bIsStackable)
        {
            for (int32 i = 0u; i < Iterator.ItemInfo.Quantity; ++i)
            {
                const FElementusItemInfo ItemInfo{ Iterator.ItemInfo.ItemId, 1, Iterator.ItemInfo.Tags };

                ElementusItems.Add(ItemInfo);
            }
        }
        else
        {
            ElementusItems.Add(Iterator.ItemInfo);
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
        if (Iterator.Index == INDEX_NONE || Iterator.Index > ElementusItems.Num())
        {
            UE_LOG(LogRancInventory_Internal, Warning, TEXT("%s: Item with name '%s' not found in inventory"), *FString(__func__), *Iterator.ItemInfo.ItemId.ToString());

            continue;
        }

        ElementusItems[Iterator.Index].Quantity -= Iterator.ItemInfo.Quantity;
    }

    if (bAllowEmptySlots)
    {
        Algo::ForEach(ElementusItems, [](FElementusItemInfo& InInfo)
            {
                if (InInfo.Quantity <= 0)
                {
                    InInfo = FElementusItemInfo::EmptyItemInfo;
                }
            });
    }
    else
    {
        ElementusItems.RemoveAll(
            [](const FElementusItemInfo& InInfo)
            {
                return InInfo.Quantity <= 0;
            }
        );
    }

    NotifyInventoryChange();
}

void URancInventoryComponent::OnRep_ElementusItems()
{
    if (const int32 LastValidIndex = ElementusItems.FindLastByPredicate([](const FElementusItemInfo& Item) { return URancInventoryFunctions::IsItemValid(Item); }); LastValidIndex != INDEX_NONE && ElementusItems.IsValidIndex(LastValidIndex + 1))
    {
        ElementusItems.RemoveAt(LastValidIndex + 1, ElementusItems.Num() - LastValidIndex - 1, false);
    }
    else if (LastValidIndex == INDEX_NONE && !URancInventoryFunctions::HasEmptyParam(ElementusItems))
    {
        ElementusItems.Empty();
    }

    ElementusItems.Shrink();

    if (IsInventoryEmpty())
    {
        ElementusItems.Empty();

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
        OnRep_ElementusItems();
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(URancInventoryComponent, ElementusItems, this);
}

void URancInventoryComponent::UpdateWeight_Implementation()
{
    float NewWeight = 0.f;
    for (const FElementusItemInfo& Iterator : ElementusItems)
    {
        if (const UElementusItemData* const ItemData = URancInventoryFunctions::GetSingleItemDataById(Iterator.ItemId, { "Data" }))
        {
            NewWeight += ItemData->ItemWeight * Iterator.Quantity;
        }
    }

    CurrentWeight = FMath::Clamp(NewWeight, 0.f, MAX_FLT);
}