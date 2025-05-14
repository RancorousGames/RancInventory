// Copyright Rancorous Games, 2024

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
	bReplicateUsingRegisteredSubObjectList = true;
}

void AWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (RepresentedItem.ItemId.IsValid())
	{
		Initialize();
	}
}

void AWorldItem::SetItem(const FItemBundle& NewItem)
{	
	RepresentedItem = NewItem;
	Initialize();
}

void AWorldItem::OnRep_Item()
{
	if (IsValid(ItemData))
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldItem::OnRep_Item: WorldItem being changed after initialization from %s to %s"),
			*ItemData->ItemId.ToString(), *RepresentedItem.ItemId.ToString());
	}

	// Not sure why I added this check, remove if nothing is broken
	//if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || !GetClass()->HasAnyClassFlags(CLASS_Native))
	//{ 
		Initialize();
	//}
}

void AWorldItem::Initialize_Implementation()
{	
	ItemData = URISSubsystem::GetItemDataById(RepresentedItem.ItemId);

	if (!ItemData)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldItem::Initialize: ItemData is null for ItemId: %s"), *RepresentedItem.ItemId.ToString());
		return;
	}

	if (RepresentedItem.InstanceData.IsEmpty() && IsValid(ItemData->DefaultInstanceDataTemplate))
	{
		for (int32 i = 0; i < RepresentedItem.Quantity; ++i)
		{
			UItemInstanceData* InstanceData = NewObject<UItemInstanceData>(ItemData->DefaultInstanceDataTemplate);
			RepresentedItem.InstanceData.Add(InstanceData);
		}
	}

	for (UItemInstanceData* InstanceData : RepresentedItem.InstanceData)
	{
		if (InstanceData)
		{
			InstanceData->Initialize(false, this, nullptr);
			AddReplicatedSubObject(InstanceData);
		}
	}

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
		const FString CubePath = TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'");
		mesh->SetStaticMesh(Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, *CubePath)));
		mesh->SetWorldScale3D(FVector(0.2f, 0.2f, 0.2f));
	}
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
    const TArray<UItemInstanceData*>& InstancesToExtract, // Added const reference
    EItemChangeReason Reason,
    TArray<UItemInstanceData*>& StateArrayToAppendTo,
    bool AllowPartial)
{
    if (!ItemId.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ExtractItem_IfServer failed: Invalid ItemId."));
        return 0;
    }

    if (Quantity <= 0)
        return 0;

    if (RepresentedItem.ItemId != ItemId)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExtractItem_IfServer failed: ItemId does not match the represented item."));
        return 0;
    }

    // Call the bundle's ExtractQuantity method
    // 'this' actor acts as the owner for removing replicated subobjects
    int32 ExtractCount = RepresentedItem.Extract(Quantity, InstancesToExtract, StateArrayToAppendTo, this);

    if (ExtractCount > 0)
    {
        if (HasAuthority())
        {
            // Optional: Destroy the world item if it becomes empty
            if (RepresentedItem.Quantity <= 0)
            {
                 // Start destruction, important this doesn't happen synchronously as we might need to access its recursively created containers
                 ConditionalBeginDestroy();
            }
        }
    }

    return ExtractCount;
}

int32 AWorldItem::GetQuantityTotal_Implementation(const FGameplayTag& ItemId) const
{
	return RepresentedItem.ItemId == ItemId ? RepresentedItem.Quantity : 0;
}