#include "TestClass.h"

#include "Components/RancInventoryComponent.h"

void UTestClass::A()
{
	UE_LOG(LogTemp, Warning, TEXT("A"));

	URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>();
//	URancItemContainerComponent* InventoryComponentX = NewObject<URancItemContainerComponent>(); 
//	InventoryComponent->MaxNumItems = 7; 
//	InventoryComponent->MaxWeight = 55;
//	
//	//FRancItemInstance UnstackableItemInstance(UnstackableItem1.GetTag(), 1);
//	InventoryComponentX->AddItems_IfServer(FRancItemInstance::EmptyItemInstance);
	
}
