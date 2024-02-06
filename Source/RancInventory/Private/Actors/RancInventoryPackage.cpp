// Author: Lucas Vilas-Boas
// Year: 2023

#include "Actors/RancInventoryPackage.h"
#include "Components/RancInventoryComponent.h"
#include "Management/RancInventorySettings.h"
#include "Management/RancInventoryFunctions.h"
#include "Management/RancInventoryData.h"
#include "LogRancInventory.h"
#include <Net/UnrealNetwork.h>
#include <Net/Core/PushModel/PushModel.h>

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RancInventoryPackage)
#endif

ARancInventoryPackage::ARancInventoryPackage(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    bNetStartup = false;
    bNetLoadOnClient = false;
    bReplicates = true;

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    PackageInventory = CreateDefaultSubobject<URancInventoryComponent>(TEXT("PackageInventory"));
    PackageInventory->SetIsReplicated(true);

    if (const URancInventorySettings* const Settings = URancInventorySettings::Get())
    {
        bDestroyWhenInventoryIsEmpty = Settings->bDestroyWhenInventoryIsEmpty;
    }
}

void ARancInventoryPackage::BeginPlay()
{
    Super::BeginPlay();

    SetDestroyOnEmpty(bDestroyWhenInventoryIsEmpty);

    if (bDestroyWhenInventoryIsEmpty && PackageInventory->IsEmpty())
    {
        Destroy();
    }
}

void ARancInventoryPackage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams;
    SharedParams.bIsPushBased = true;

    DOREPLIFETIME_WITH_PARAMS_FAST(ARancInventoryPackage, PackageInventory, SharedParams);
}

void ARancInventoryPackage::PutItemIntoPackage(const TArray<FRancItemInfo> ItemInfo, URancInventoryComponent* FromInventory)
{
    URancInventoryFunctions::TradeRancItem(ItemInfo, FromInventory, PackageInventory);
    MARK_PROPERTY_DIRTY_FROM_NAME(ARancInventoryPackage, PackageInventory, this);
}

void ARancInventoryPackage::GetItemFromPackage(const TArray<FRancItemInfo> ItemInfo, URancInventoryComponent* ToInventory)
{
    URancInventoryFunctions::TradeRancItem(ItemInfo, PackageInventory, ToInventory);
    MARK_PROPERTY_DIRTY_FROM_NAME(ARancInventoryPackage, PackageInventory, this);
}

void ARancInventoryPackage::SetDestroyOnEmpty(const bool bDestroy)
{
    if (bDestroyWhenInventoryIsEmpty == bDestroy)
    {
        return;
    }

    bDestroyWhenInventoryIsEmpty = bDestroy;
   /* FRancInventoryEmpty Delegate = PackageInventory->OnInventoryEmpty;

    if (const bool bIsAlreadyBound = Delegate.IsAlreadyBound(this, &ARancInventoryPackage::BeginPackageDestruction); bDestroy && !bIsAlreadyBound)
    {
        Delegate.AddDynamic(this, &ARancInventoryPackage::BeginPackageDestruction);
    }
    else if (!bDestroy && bIsAlreadyBound)
    {
        Delegate.RemoveDynamic(this, &ARancInventoryPackage::BeginPackageDestruction);
    }*/
}

bool ARancInventoryPackage::GetDestroyOnEmpty() const
{
    return bDestroyWhenInventoryIsEmpty;
}

void ARancInventoryPackage::BeginPackageDestruction_Implementation()
{
    // Check if this option is still active before the destruction
    if (bDestroyWhenInventoryIsEmpty)
    {
        Destroy();
    }
    else
    {
        UE_LOG(LogRancInventory_Internal, Warning, TEXT("RancInventory - %s: Package %s was not destroyed because the " "option 'bDestroyWhenInventoryIsEmpty' was disabled"), *FString(__func__), *GetName());
    }
}