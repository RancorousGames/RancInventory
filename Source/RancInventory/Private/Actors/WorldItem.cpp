﻿// Copyright Rancorous Games, 2024

#include "Actors\WorldItem.h"

#include "Components/StaticMeshComponent.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Data/ItemStaticData.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"


void AWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (RepresentedItem.ItemId.IsValid())
	{
		Initialize();
	}
}

void AWorldItem::SetItem(const FItemBundleWithInstanceData& NewItem)
{
	RepresentedItem = NewItem;
	Initialize();
}

void AWorldItem::OnRep_Item()
{
	if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || !GetClass()->HasAnyClassFlags(CLASS_Native))
	{
		Initialize();
	}
}

void AWorldItem::Initialize()
{	
	ItemData = URISSubsystem::GetItemDataById(RepresentedItem.ItemId);

	SetMobility(EComponentMobility::Movable);
	auto* mesh = GetStaticMeshComponent();

	mesh->SetSimulatePhysics(true);
	mesh->SetEnableGravity(true);

	if (ItemData && ItemData->ItemWorldMesh)
	{
		mesh->SetStaticMesh(ItemData->ItemWorldMesh);
		mesh->SetWorldScale3D(ItemData->ItemWorldScale);
	}
	else
	{
		// cube
		const FString CubePath = TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'");
		mesh->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, *CubePath)));
		mesh->SetWorldScale3D(FVector(0.2f, 0.2f, 0.2f));
	}
	
	ReceiveInitialize();
}

void AWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWorldItem, RepresentedItem, COND_InitialOnly);
}

void AWorldItem::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
}


int32 AWorldItem::ExtractItem_IfServer_Implementation(
    const FGameplayTag& ItemId, 
    int32 Quantity, 
    EItemChangeReason Reason, 
    TArray<UItemInstanceData*>& StateArrayToAppendTo)
{
    // Validate the input parameters
    if (!ItemId.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ExtractItem_IfServer failed: Invalid ItemId."));
        return 0;
    }

    if (Quantity <= 0)
        return 0;

    // Check if the ItemId matches the represented item
    if (RepresentedItem.ItemId != ItemId)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExtractItem_IfServer failed: ItemId does not match the represented item."));
        return 0;
    }

    // Determine how much can be extracted
    int32 AvailableQuantity = RepresentedItem.Quantity;
    int32 QuantityToExtract = FMath::Min(Quantity, AvailableQuantity);

    if (QuantityToExtract <= 0)
    {
        return 0;
    }

    // Adjust the represented item's quantity
    RepresentedItem.Quantity -= QuantityToExtract;

    // Create instance data if needed
    if (RepresentedItem.InstanceData.Num() > 0 && QuantityToExtract > 0)
    {
        for (int32 i = 0; i < QuantityToExtract; ++i)
        {
            if (RepresentedItem.InstanceData.Num() > 0)
            {
                // Transfer the last instance from RepresentedItem to the output array
                int32 LastIndex = RepresentedItem.InstanceData.Num() - 1;
                StateArrayToAppendTo.Add(RepresentedItem.InstanceData[LastIndex]);
                RepresentedItem.InstanceData.RemoveAt(LastIndex);
            }
        }
    }

    // Notify replication system if necessary
    if (HasAuthority())
    {
        OnRep_Item();
    }

    return QuantityToExtract;
}

int32 AWorldItem::GetContainedQuantity_Implementation(const FGameplayTag& ItemId)
{
	return RepresentedItem.ItemId == ItemId ? RepresentedItem.Quantity : 0;
}