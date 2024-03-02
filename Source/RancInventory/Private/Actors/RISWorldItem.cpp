// Copyright Rancorous Games, 2024

#include "..\..\Public\Actors\RISWorldItem.h"

#include "..\..\Public\Management\RISInventoryFunctions.h"
#include "Net/UnrealNetwork.h"


void ARISWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void ARISWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (Item.ItemId.IsValid())
	{
		Initialize();
	}
}

void ARISWorldItem::SetItem(const FRISItemInstance& NewItem)
{
	Item = NewItem;
	Initialize();
}

void ARISWorldItem::OnRep_Item()
{
	Initialize();
}

void ARISWorldItem::Initialize()
{
	ItemData = URISInventoryFunctions::GetItemDataById(Item.ItemId);

	auto* mesh = GetStaticMeshComponent();

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
}

void ARISWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ARISWorldItem, Item, COND_InitialOnly);
}

void ARISWorldItem::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
}
