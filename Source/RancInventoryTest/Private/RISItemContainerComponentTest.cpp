﻿// Copyright Rancorous Games, 2024

#include "RISItemContainerComponentTest.h"

#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "TestDelegateForwardHelper.h"
#include "Components/ItemContainerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancItemContainerComponentTest, "GameTests.RIS.RancItemContainer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define SETUP_RANCITEMCONTAINER(MaxItems, CarryCapacity) \
UItemContainerComponent* ItemContainerComponent = NewObject<UItemContainerComponent>(); \
ItemContainerComponent->MaxContainerSlotCount = MaxItems; \
ItemContainerComponent->MaxWeight = CarryCapacity; \
InitializeTestItems();

// TODO RECOVERY:
/*
bool TestAddItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 10); // Setup with max 10 slots and max weight of 10
    bool Res = true;

    // Test adding an item within weight and item count limits
    int32 AddedQuantity = ItemContainerComponent->AddItems_IfServer(FiveRocks, false); // Assume each rock has a weight of 1
    Res &= Test->TestEqual(TEXT("Should add 5 rocks"), AddedQuantity, 5);
    Res &= Test->TestEqual(TEXT("Total weight should be 5 after adding rocks"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    // Test adding another item that exceeds the weight limit
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(GiantBoulder, false);
    Res &= Test->TestEqual(TEXT("Should not add Giant Boulder due to weight limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total weight should remain 5 after attempting to add Giant Boulder"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    // Test adding item partially when exceeding the max weight
    FItemBundle StickInstance(ItemIdSticks, 6); // Assume each stick has a weight of 1
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(StickInstance, true);
    Res &= Test->TestEqual(TEXT("Should add only 5 sticks due to weight limit"), AddedQuantity, 5);
    Res &= Test->TestEqual(TEXT("Total weight should be 10 after partially adding sticks"), ItemContainerComponent->GetCurrentWeight(), 10.0f);

    // Test adding an item when not enough slots are available but under weight limit
    ItemContainerComponent->ClearContainer_IfServer(); // Clear the inventory
    ItemContainerComponent->MaxContainerSlotCount = 2;
    FItemBundle TenRocks(ItemIdRock, 10); 
    ItemContainerComponent->AddItems_IfServer(TenRocks, false); // Fill two slots with 5 rocks each
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneRock, false); // Try adding one more
    Res &= Test->TestEqual(TEXT("Should not add another rock due to slot limit"), AddedQuantity, 0);

    // Clear the inventory and reset item count and weight capacity for unstackable items test
    ItemContainerComponent->ClearContainer_IfServer();
    ItemContainerComponent->MaxWeight = 10;

    // Test adding an unstackable item. Spear is unstackable and has a weight of 3
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneSpear, false);
    Res &= Test->TestEqual(TEXT("Should add 1 spear"), AddedQuantity, 1);
    Res &= Test->TestEqual(TEXT("Total weight should be 3 after adding spear"), ItemContainerComponent->GetCurrentWeight(), 3.0f);
    Res &= Test->TestEqual(TEXT("Total used slot count should be 1"), ItemContainerComponent->UsedContainerSlotCount, 1);

    // Test adding another unstackable item (Helmet), which also should not stack. // Assume Helmet is unstackable and has a weight of 2
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneHelmet, false);
    Res &= Test->TestEqual(TEXT("Should add 1 helmet"), AddedQuantity, 1);
    Res &= Test->TestEqual(TEXT("Total weight should be 5 after adding helmet"), ItemContainerComponent->GetCurrentWeight(), 5.0f);
    Res &= Test->TestEqual(TEXT("Total used slot count should be 2"), ItemContainerComponent->UsedContainerSlotCount, 2);

    // Test attempting to add another unstackable item when it would exceed the slot limit
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneSpear, false);
    Res &= Test->TestEqual(TEXT("Should not add another spear due to item count limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total item count should remain 2 after attempting to add another spear"), ItemContainerComponent->GetAllContainerItems().Num(), 2);
    Res &= Test->TestEqual(TEXT("Total used slot count should be 2"), ItemContainerComponent->UsedContainerSlotCount, 2);

    ItemContainerComponent->MaxContainerSlotCount = 3; // add one more slot
    // Test attempting to add an unstackable item that exceeds the weight limit
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(GiantBoulder, false);
    Res &= Test->TestEqual(TEXT("Should not add heavy item due to weight limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total weight should remain 5 after attempting to add heavy helmet"), ItemContainerComponent->GetCurrentWeight(), 5.0f);
    Res &= Test->TestEqual(TEXT("Total used slot count should be 2"), ItemContainerComponent->UsedContainerSlotCount, 2);
    
    // but a small rock we have weight capacity for
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneRock, false);
    Res &= Test->TestEqual(TEXT("Should add another rock"), AddedQuantity, 1);
    Res &= Test->TestEqual(TEXT("Total weight should be 6 after adding another rock"), ItemContainerComponent->GetCurrentWeight(), 6.0f);
    Res &= Test->TestEqual(TEXT("Total used slot count should be 3 after adding another rock"), ItemContainerComponent->UsedContainerSlotCount, 3);
    
    return Res;
}

bool TestRemoveItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 20); // Setup with max 10 items and max weight of 20
    bool Res = true;
    
    // Add initial items for removal tests
    ItemContainerComponent->AddItems_IfServer(FiveRocks, false); // 5 Rocks
    ItemContainerComponent->AddItems_IfServer(OneSpear, false); // 1 Spear
    ItemContainerComponent->AddItems_IfServer(OneHelmet, false); // 1 Helmet

    // Test removing a stackable item partially
    int32 RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(TwoRocks, true); // Try removing 2 Rocks
    Res &= Test->TestEqual(TEXT("Should remove 2 rocks"), RemovedQuantity, 2);
    Res &= Test->TestEqual(TEXT("Total rocks should be 3 after removal"), ItemContainerComponent->GetContainerItemCount(ItemIdRock), 3);

    // Test removing a stackable item completely
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(ThreeRocks, true); // Remove remaining Rocks
    Res &= Test->TestEqual(TEXT("Should remove 3 rocks"), RemovedQuantity, 3);
    Res &= Test->TestTrue(TEXT("Rocks should be completely removed"), !ItemContainerComponent->DoesContainerContainItems(ItemIdRock, 1));

    // Test removing an unstackable item (Spear)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(OneSpear, true);
    Res &= Test->TestEqual(TEXT("Should remove 1 spear"), RemovedQuantity, 1);
    Res &= Test->TestTrue(TEXT("Spear should be completely removed"), !ItemContainerComponent->DoesContainerContainItems(ItemIdSpear, 1));

    // Test attempting to remove more items than are available (Helmet)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FItemBundle(ItemIdHelmet, 2), false); // Try removing 2 Helmets
    Res &= Test->TestEqual(TEXT("Should not remove any helmets as quantity exceeds available"), RemovedQuantity, 0);
    Res &= Test->TestTrue(TEXT("Helmet should remain after failed removal attempt"), ItemContainerComponent->DoesContainerContainItems(ItemIdHelmet, 1));

    // Test partial removal when not allowed (Helmet)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(OneHelmet, false); // Properly remove Helmet
    Res &= Test->TestEqual(TEXT("Should remove helmet"), RemovedQuantity, 1);
    Res &= Test->TestFalse(TEXT("Helmet should be removed after successful removal"), ItemContainerComponent->DoesContainerContainItems(ItemIdHelmet, 1));

    return Res;
}

bool TestCanReceiveItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(7, 15); // Setup with max slots and max weight
    bool Res = true;

    // Initially, the container should be able to receive any item within its capacity
    Res &= Test->TestTrue(TEXT("Container should initially be able to receive rocks"), ItemContainerComponent->CanContainerReceiveItems(FiveRocks));

    // Add some items to change the container's capacity state
    ItemContainerComponent->AddItems_IfServer(ThreeRocks, false); // 3 Rocks, assuming each rock weighs 1 unit

    // Now, the container should report its reduced capacity accurately
    Res &= Test->TestTrue(TEXT("Container should still be able to receive more rocks"), ItemContainerComponent->CanContainerReceiveItems(TwoRocks));
    Res &= Test->TestFalse(TEXT("Container should not be able to receive more rocks than its weight limit"), ItemContainerComponent->CanContainerReceiveItems(FItemBundle(ItemIdRock, 13)));

    // Add unstackable item (Helmet)
    ItemContainerComponent->AddItems_IfServer(FItemBundle(ItemIdHelmet, 5), false); // adding 5x2=10 units worth of weight, totalling 13

    // Check if the container can receive another unstackable item without exceeding item count and weight limits
    Res &= Test->TestTrue(TEXT("Container should be able to receive a helmet"), ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
    Res &= Test->TestFalse(TEXT("Container should not be able to receive a spear"), ItemContainerComponent->CanContainerReceiveItems(OneSpear));
    
    // Fill up the container to its limits
    ItemContainerComponent->AddItems_IfServer(TwoRocks, false); // Adding more rocks to reach the weight limit

    // Test the container's capacity with various items now that it's full
    Res &= Test->TestFalse(TEXT("Container should not be able to receive any more items due to weight limit"), ItemContainerComponent->CanContainerReceiveItems(FItemBundle(ItemIdRock, 1)));
    Res &= Test->TestFalse(TEXT("Container should not be able to receive any more unstackable items due to item count limit"), ItemContainerComponent->CanContainerReceiveItems(OneHelmet));

    // 6 slots should taken lets increase the weight limit, then we should be able to fill 1 slot but not two
    ItemContainerComponent->MaxWeight = 20;
    Res &= Test->TestTrue(TEXT("Container should now be able to receive 1 more item"), ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
    ItemContainerComponent->AddItems_IfServer(OneHelmet, false); // Adding more rocks to reach the weight limit
    Res &= Test->TestFalse(TEXT("Container should not be able to receive any more unstackable items due to slot count limit"), ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
    return Res;
}

bool TestItemCountsAndPresence(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 20); // Setup with max 10 items and max weight of 20
    bool Res = true;

    // Add items to the inventory
    ItemContainerComponent->AddItems_IfServer(FiveRocks, false); // Add 5 Rocks
    ItemContainerComponent->AddItems_IfServer(OneHelmet, false); // Add 1 Helmet

    // Test GetItemCount for stackable and unstackable items
    Res &= Test->TestEqual(TEXT("Inventory should report 5 rocks"), ItemContainerComponent->GetContainerItemCount(ItemIdRock), 5);
    Res &= Test->TestEqual(TEXT("Inventory should report 1 helmet"), ItemContainerComponent->GetContainerItemCount(ItemIdHelmet), 1);

    // Test ContainsItems for exact and over-quantities
    Res &= Test->TestTrue(TEXT("Inventory should contain at least 5 rocks"), ItemContainerComponent->DoesContainerContainItems(ItemIdRock, 5));
    Res &= Test->TestFalse(TEXT("Inventory should not falsely report more rocks than it contains"), ItemContainerComponent->DoesContainerContainItems(ItemIdRock, 6));
    Res &= Test->TestTrue(TEXT("Inventory should confirm the presence of the helmet"), ItemContainerComponent->DoesContainerContainItems(ItemIdHelmet, 1));
    Res &= Test->TestFalse(TEXT("Inventory should not report more helmets than it contains"), ItemContainerComponent->DoesContainerContainItems(ItemIdHelmet, 2));

    // Test GetAllItems to ensure it returns all added items correctly
    TArray<FItemBundle> AllItems = ItemContainerComponent->GetAllContainerItems();
    Res &= Test->TestTrue(TEXT("GetAllItems should include rocks"), AllItems.ContainsByPredicate([](const FItemBundle& Item) { return Item.ItemId == ItemIdRock && Item.Quantity == 5; }));
    Res &= Test->TestTrue(TEXT("GetAllItems should include the helmet"), AllItems.ContainsByPredicate([](const FItemBundle& Item) { return Item.ItemId == ItemIdHelmet && Item.Quantity == 1; }));
    
    // Remove some items and test counts again
    ItemContainerComponent->RemoveItems_IfServer(ThreeRocks, true); // Remove 3 Rocks
    Res &= Test->TestEqual(TEXT("After removal, inventory should report 2 rocks"), ItemContainerComponent->GetContainerItemCount(ItemIdRock), 2);
    
    // Test inventory is not empty
    Res &= Test->TestFalse(TEXT("Inventory should not be empty"), ItemContainerComponent->IsEmpty());

    // Clear the container and test again
    ItemContainerComponent->ClearContainer_IfServer();
    Res &= Test->TestTrue(TEXT("After clearing, inventory should be empty"), ItemContainerComponent->IsEmpty());

    return Res;
}

bool TestMiscFunctions(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 50); // Setup with a capacity for 10 items and 50 weight capacity.
    
    bool Res = true;
    // Assuming InitializeTestItems() initializes the items including a "Rock" and "Helmet" with their respective tags.
    InitializeTestItems();

    // Test FindItemById - Initially, the container should not contain a "Rock" item.
    FItemBundle RockItemInstance = ItemContainerComponent->FindItemById(ItemIdRock);
    Res &=  Test->TestTrue(TEXT("FindItemById should not find an item before it's added"), RockItemInstance.ItemId != ItemIdRock);
    
    // Add a "Rock" item to the container.
    ItemContainerComponent->AddItems_IfServer(FItemBundle(ItemIdRock, 1));
    
    // Test FindItemById - Now, the container should contain the "Rock" item.
    RockItemInstance = ItemContainerComponent->FindItemById(ItemIdRock);
    Res &=  Test->TestTrue(TEXT("FindItemById should find the item after it's added"), RockItemInstance.ItemId == ItemIdRock);

    // Test ContainsItems for "Rock".
    Res &=  Test->TestTrue(TEXT("ContainsItems should return true for items present in the container"), ItemContainerComponent->DoesContainerContainItems(ItemIdRock, 1));

    // Test IsEmpty before and after adding items.
    Res &=  Test->TestFalse(TEXT("IsEmpty should return false when items are present"), ItemContainerComponent->IsEmpty());
 
    ItemContainerComponent->ClearContainer_IfServer();

    ItemContainerComponent->AddItems_IfServer(FItemBundle(ItemIdRock, 1));
    ItemContainerComponent->DropItems(FItemBundle(ItemIdRock, 1));
    
    return Res;
}

bool TestSetAddItemValidationCallback(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 50); // Assuming max items and weight capacity are sufficient for the test
    bool Res = true;

    // We have to do this weird workaround because we can't bind a lambda to a delegate directly
    const auto DelegateHelper = NewObject<UTestDelegateForwardHelper>();
    UItemContainerComponent::FAddItemValidationDelegate MyDelegateInstance;
    MyDelegateInstance.BindUFunction(DelegateHelper, FName("DispatchItemToBool"));
    ItemContainerComponent->SetAddItemValidationCallback_IfServer(MyDelegateInstance);
    // we can then set DelegateHelper->CallFuncItemToBool to whatever we want the callback to be

    // Define a lambda function as the validation callback which only allows adding rocks
    DelegateHelper->CallFuncItemToBool = [](const FItemBundle& ItemInstance)
    {
        return ItemInstance.ItemId.MatchesTag(ItemIdRock);
    };
  
    // Attempt to add a rock, which should be allowed
    int32 AddedQuantity = ItemContainerComponent->AddItems_IfServer(FiveRocks, false);
    Res &= Test->TestEqual(TEXT("Should add 5 rocks since rocks are allowed"), AddedQuantity, 5);

    // Attempt to add a helmet, which should be denied
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneHelmet, false);
    Res &= Test->TestEqual(TEXT("Should not add the helmet since only rocks are allowed"), AddedQuantity, 0);

    // Change the validation callback to allow all items
    DelegateHelper->CallFuncItemToBool = [&](const FItemBundle& ItemInstance){return true;};

    // Now, adding a helmet should be allowed
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(OneHelmet, false);
    Res &= Test->TestEqual(TEXT("Should add the helmet now that all items are allowed"), AddedQuantity, 1);

    return Res;
}*/

bool FRancItemContainerComponentTest::RunTest(const FString& Parameters)
{
    bool Res = true;
   // Res &= TestAddItems(this);
   // Res &= TestRemoveItems(this);
   // Res &= TestCanReceiveItems(this);
   // Res &= TestItemCountsAndPresence(this);
   // Res &= TestMiscFunctions(this);
   // Res &= TestSetAddItemValidationCallback(this);
    // Can't currently test drop because it tries to get world from the component, which tries to get owning actor (null) to call getworld on
    
    return Res;
}