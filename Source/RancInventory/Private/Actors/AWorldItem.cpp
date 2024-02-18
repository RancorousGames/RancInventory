#include "RancInventory/Public/Actors/AWorldItem.h"

#include "Management/RancInventoryFunctions.h"
#include "Net/UnrealNetwork.h"


void AWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AWorldItem::BeginPlay()
{
	Super::BeginPlay();

	if (Item.ItemId.IsValid())
	{
		Initialize();
	}
}

void AWorldItem::SetItem(const FRancItemInstance& NewItem)
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
	ItemData = URancInventoryFunctions::GetItemDataById(Item.ItemId);

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

void AWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AWorldItem, Item, COND_InitialOnly);
}

void AWorldItem::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
}
