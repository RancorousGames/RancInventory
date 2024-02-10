#include "RancInventory/Public/Actors/AWorldItem.h"

#include "Management/RancInventoryFunctions.h"
#include "Net/UnrealNetwork.h"


void AWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetMobility(EComponentMobility::Movable);
	SetReplicates(true);
	SetReplicateMovement(true);
	GetStaticMeshComponent()->SetEnableGravity(true);
	GetStaticMeshComponent()->SetSimulatePhysics(true);
}

void AWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (Item.ItemId.IsValid())
	{
		Initialize();
	}
}

void AWorldItem::SetItem(const FRancItemInfo& NewItem)
{
	Item = NewItem;
	Initialize();
}

void AWorldItem::OnRep_Item()
{
	Initialize();
}

void AWorldItem::Initialize()
{
	ItemData = URancInventoryFunctions::GetItemById(Item.ItemId);

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
	}
}

void AWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWorldItem, Item, COND_InitialOnly);
}

void AWorldItem::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
}
