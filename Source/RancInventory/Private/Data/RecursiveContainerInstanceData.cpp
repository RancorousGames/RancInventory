#include "Data/RecursiveContainerInstanceData.h"

#include "Actors/WorldItem.h"
#include "Components/ItemContainerComponent.h"
#include "Net/UnrealNetwork.h"

URecursiveContainerInstanceData::URecursiveContainerInstanceData()
	: ContainerClassToSpawn(UItemContainerComponent::StaticClass())
{
	
}

URecursiveContainerInstanceData::~URecursiveContainerInstanceData()
{
	OnDestroy();
}

inline void URecursiveContainerInstanceData::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URecursiveContainerInstanceData, RepresentedContainer);
}

void URecursiveContainerInstanceData::Initialize_Implementation(bool OwnedByContainer, AWorldItem* OwningWorldItem,
	UItemContainerComponent* OwningContainer)
{
	Super::Initialize_Implementation(OwnedByContainer, OwningWorldItem, OwningContainer);

	if (!IsValid(ContainerClassToSpawn))
	{
		ContainerClassToSpawn = UItemContainerComponent::StaticClass();
	}
	
	TObjectPtr<UItemContainerComponent> OldContainer = RepresentedContainer;
	
	ensureMsgf((OwnedByContainer && IsValid(OwningContainer)) || (!OwnedByContainer && IsValid(OwningWorldItem)),
		TEXT("RecursiveContainerInstanceData::Initialize_Implementation: OwningWorldItem or OwningContainer and match OwnedByComponent."));
	
	AActor* OwningActor = OwningWorldItem ? OwningWorldItem : OwningContainer->GetOwner();
	
	if (IsValid(OwningActor))
	{
		if (UItemContainerComponent* SubContainer = NewObject<UItemContainerComponent>(
			OwningActor, ContainerClassToSpawn))
		{
			SubContainer->MaxSlotCount = MaxSlotCount;
			SubContainer->MaxWeight = MaxWeight;

			UItemContainerComponent* TemplateContainer = OwningContainer ? OwningContainer : OldContainer.Get();
			if (IsValid(TemplateContainer))
			{
				SubContainer->JigsawMode = TemplateContainer->JigsawMode;
				SubContainer->DefaultDropDistance = TemplateContainer->DefaultDropDistance;
				SubContainer->DropItemClass = TemplateContainer->DropItemClass;
			}
			
			if (IsValid(OldContainer) && OwningActor->HasAuthority())
			{
				auto OldContainerItems = OldContainer->GetAllItems();
				for (int32 Index = OldContainerItems.Num() - 1; Index >= 0; --Index)
				{
					const FItemBundle& Item = OldContainerItems[Index];
					SubContainer->AddItem_IfServer(OldContainer, Item.ItemId, Item.Quantity, false);
					for (UItemInstanceData* InstanceData : Item.InstanceData)
					{
						if (IsValid(InstanceData))
						{
							RepresentedContainer->GetOwner()->RemoveReplicatedSubObject(InstanceData);
							OwningActor->AddReplicatedSubObject(InstanceData);

							// Potential recursive call
							InstanceData->Initialize_Implementation(IsValid(SubContainer), OwningWorldItem, SubContainer);
						}
					}
				}

				// Grab reference to the raw array you want to iterate
				TArray<FItemBundle> Bundles = OldContainer->GetAllItems();

				// Iterate backwards by index
				for (int32 Index = Bundles.Num() - 1; Index >= 0; --Index)
				{
					const FItemBundle& Item = Bundles[Index];
					SubContainer->AddItem_IfServer(OldContainer, Item.ItemId, Item.Quantity, false);
					for (UItemInstanceData* InstanceData : Item.InstanceData)
					{
						if (IsValid(InstanceData))
						{
							RepresentedContainer->GetOwner()->RemoveReplicatedSubObject(InstanceData);
							OwningActor->AddReplicatedSubObject(InstanceData);

							// Potential recursive call
							InstanceData->Initialize_Implementation(IsValid(OwningWorldItem), OwningWorldItem, SubContainer);
						}
					}
				}
				
				ensureMsgf(OldContainer->UsedContainerSlotCount == 0,
					TEXT("RecursiveContainerInstanceData::Initialize_Implementation: Old subcontainer should be empty after transfer."));
				
				OldContainer->DestroyComponent();
			}
			
			OwningActor->AddOwnedComponent(SubContainer);
			SubContainer->RegisterComponent();
			RepresentedContainer = SubContainer;
		}
	}
}

void URecursiveContainerInstanceData::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	if (IsValid(RepresentedContainer))
	{
		RepresentedContainer->GetOwner()->RemoveReplicatedSubObject(this);
		RepresentedContainer->DestroyComponent();
		RepresentedContainer = nullptr;
	}
}
