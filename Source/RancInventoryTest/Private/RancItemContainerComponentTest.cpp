#include "RancItemContainerComponentTest.h"

#include "NativeGameplayTags.h"

#include "Management/RancInventoryData.h"
#include "Components/RancItemContainerComponent.h"
#include "Misc/AutomationTest.h"
#include "InventorySetup.cpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancItemContainerComponentTest, "GameTests.FRancItemContainerComponentTest.Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#define SETUP_RANCITEMCONTAINER(MaxItems, CarryCapacity) \
URancItemContainerComponent* ItemContainerComponent = NewObject<URancItemContainerComponent>(); \
ItemContainerComponent->MaxNumItemsInContainer = MaxItems; \
ItemContainerComponent->MaxWeight = CarryCapacity; \
InitializeTestItems();

bool TestAddItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 10); // Setup with max 10 items and max weight of 10
    bool Res = true;

    // Test adding an item within weight and item count limits
    FRancItemInstance RockInstance(ItemIdRock, 5); // Assume each rock has a weight of 1
    int32 AddedQuantity = ItemContainerComponent->AddItems_IfServer(RockInstance, false);
    Res &= Test->TestEqual(TEXT("Should add 5 rocks"), AddedQuantity, 5);
    Res &= Test->TestEqual(TEXT("Total weight should be 5 after adding rocks"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    // Test adding another item that exceeds the weight limit
    FRancItemInstance BoulderInstance(ItemIdGiantBoulder, 1); // Assume the boulder has a weight of 10
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(BoulderInstance, false);
    Res &= Test->TestEqual(TEXT("Should not add Giant Boulder due to weight limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total weight should remain 5 after attempting to add Giant Boulder"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    // Test adding item partially when exceeding the max weight
    FRancItemInstance StickInstance(ItemIdSticks, 6); // Assume each stick has a weight of 1
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(StickInstance, true);
    Res &= Test->TestEqual(TEXT("Should add only 5 sticks due to weight limit"), AddedQuantity, 5);
    Res &= Test->TestEqual(TEXT("Total weight should be 10 after partially adding sticks"), ItemContainerComponent->GetCurrentWeight(), 10.0f);

    // Test adding an item when item count exceeds the limit but under weight limit
    ItemContainerComponent->DropAllItems_IfServer(); // Clear the inventory
    ItemContainerComponent->MaxNumItemsInContainer = 2;
    FRancItemInstance SmallRockInstance(ItemIdRock, 2); // Add one small item
    ItemContainerComponent->AddItems_IfServer(SmallRockInstance, false);
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(SmallRockInstance, false); // Try adding one more
    Res &= Test->TestEqual(TEXT("Should not add another rock due to item count limit"), AddedQuantity, 0);

    // Clear the inventory and reset item count and weight capacity for unstackable items test
    ItemContainerComponent->ClearContainer_IfServer();
    ItemContainerComponent->MaxNumItemsInContainer = 10; // Reset to a higher value for this section of tests
    ItemContainerComponent->MaxWeight = 20;

    // Test adding an unstackable item (Spear)
    FRancItemInstance SpearInstance(ItemIdSpear, 1); // Spear is unstackable and has a weight of 3
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(SpearInstance, false);
    Res &= Test->TestEqual(TEXT("Should add 1 spear"), AddedQuantity, 1);
    Res &= Test->TestEqual(TEXT("Total weight should be 3 after adding spear"), ItemContainerComponent->GetCurrentWeight(), 3.0f);

    // Test adding another unstackable item (Helmet), which also should not stack
    FRancItemInstance HelmetInstance(ItemIdHelmet, 1); // Assume Helmet is unstackable and has a weight of 2
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(HelmetInstance, false);
    Res &= Test->TestEqual(TEXT("Should add 1 helmet"), AddedQuantity, 1);
    Res &= Test->TestEqual(TEXT("Total weight should be 5 after adding helmet"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    // Test attempting to add another unstackable item when it would exceed the item count limit
    ItemContainerComponent->MaxNumItemsInContainer = 2; // Set item count limit to 2
    FRancItemInstance AnotherSpearInstance(ItemIdSpear, 1); // Another spear to test limit
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(AnotherSpearInstance, false);
    Res &= Test->TestEqual(TEXT("Should not add another spear due to item count limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total item count should remain 2 after attempting to add another spear"), ItemContainerComponent->GetAllItems().Num(), 2);

    // Test attempting to add an unstackable item that exceeds the weight limit
    ItemContainerComponent->MaxWeight = 7; // Adjust max weight for this test
    FRancItemInstance HeavyHelmetInstance(ItemIdHelmet, 1); // Assume this helmet instance has a weight of 3
    AddedQuantity = ItemContainerComponent->AddItems_IfServer(HeavyHelmetInstance, false);
    Res &= Test->TestEqual(TEXT("Should not add heavy helmet due to weight limit"), AddedQuantity, 0);
    Res &= Test->TestEqual(TEXT("Total weight should remain 5 after attempting to add heavy helmet"), ItemContainerComponent->GetCurrentWeight(), 5.0f);

    return Res;
}

bool TestRemoveItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 20); // Setup with max 10 items and max weight of 20
    bool Res = true;
    
    // Add initial items for removal tests
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 5), false); // 5 Rocks
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdSpear, 1), false); // 1 Spear
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdHelmet, 1), false); // 1 Helmet

    // Test removing a stackable item partially
    int32 RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdRock, 2), true); // Try removing 2 Rocks
    Res &= Test->TestEqual(TEXT("Should remove 2 rocks"), RemovedQuantity, 2);
    Res &= Test->TestEqual(TEXT("Total rocks should be 3 after removal"), ItemContainerComponent->GetItemCount(ItemIdRock), 3);

    // Test removing a stackable item completely
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdRock, 3), true); // Remove remaining Rocks
    Res &= Test->TestEqual(TEXT("Should remove 3 rocks"), RemovedQuantity, 3);
    Res &= Test->TestTrue(TEXT("Rocks should be completely removed"), !ItemContainerComponent->ContainsItems(ItemIdRock, 1));

    // Test removing an unstackable item (Spear)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdSpear, 1), true);
    Res &= Test->TestEqual(TEXT("Should remove 1 spear"), RemovedQuantity, 1);
    Res &= Test->TestTrue(TEXT("Spear should be completely removed"), !ItemContainerComponent->ContainsItems(ItemIdSpear, 1));

    // Test attempting to remove more items than are available (Helmet)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdHelmet, 2), false); // Try removing 2 Helmets
    Res &= Test->TestEqual(TEXT("Should not remove any helmets as quantity exceeds available"), RemovedQuantity, 0);
    Res &= Test->TestTrue(TEXT("Helmet should remain after failed removal attempt"), ItemContainerComponent->ContainsItems(ItemIdHelmet, 1));

    // Test partial removal when not allowed (Helmet)
    RemovedQuantity = ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdHelmet, 1), false); // Properly remove Helmet
    Res &= Test->TestEqual(TEXT("Should remove helmet"), RemovedQuantity, 1);
    Res &= Test->TestFalse(TEXT("Helmet should be removed after successful removal"), ItemContainerComponent->ContainsItems(ItemIdHelmet, 1));

    return Res;
}

bool TestCanReceiveItems(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(5, 15); // Setup with max 5 items and max weight of 15
    bool Res = true;

    // Initially, the container should be able to receive any item within its capacity
    Res &= Test->TestTrue(TEXT("Container should initially be able to receive rocks"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdRock, 5)));

    // Add some items to change the container's capacity state
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 3), false); // 3 Rocks, assuming each rock weighs 1 unit

    // Now, the container should report its reduced capacity accurately
    Res &= Test->TestTrue(TEXT("Container should still be able to receive more rocks"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdRock, 2)));
    Res &= Test->TestFalse(TEXT("Container should not be able to receive more rocks than its weight limit"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdRock, 4)));

    // Add unstackable item (Helmet)
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdHelmet, 5), false); // adding 5x2=10 units worth of weight

    // Check if the container can receive another unstackable item without exceeding item count and weight limits
    Res &= Test->TestTrue(TEXT("Container should be able to receive a spear"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdSpear, 1)));
    
    // Fill up the container to its limits
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 2), false); // Adding more rocks to reach the weight limit

    // Test the container's capacity with various items now that it's full
    Res &= Test->TestFalse(TEXT("Container should not be able to receive any more items due to weight limit"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdRock, 1)));
    Res &= Test->TestFalse(TEXT("Container should not be able to receive any more unstackable items due to item count limit"), ItemContainerComponent->CanContainerReceiveItems(FRancItemInstance(ItemIdHelmet, 1)));

    return Res;
}

bool TestItemCountsAndPresence(FRancItemContainerComponentTest* Test)
{
    SETUP_RANCITEMCONTAINER(10, 20); // Setup with max 10 items and max weight of 20
    bool Res = true;

    // Add items to the inventory
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 5), false); // Add 5 Rocks
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdHelmet, 1), false); // Add 1 Helmet

    // Test GetItemCount for stackable and unstackable items
    Res &= Test->TestEqual(TEXT("Inventory should report 5 rocks"), ItemContainerComponent->GetItemCount(ItemIdRock), 5);
    Res &= Test->TestEqual(TEXT("Inventory should report 1 helmet"), ItemContainerComponent->GetItemCount(ItemIdHelmet), 1);

    // Test ContainsItems for exact and over-quantities
    Res &= Test->TestTrue(TEXT("Inventory should contain at least 5 rocks"), ItemContainerComponent->ContainsItems(ItemIdRock, 5));
    Res &= Test->TestFalse(TEXT("Inventory should not falsely report more rocks than it contains"), ItemContainerComponent->ContainsItems(ItemIdRock, 6));
    Res &= Test->TestTrue(TEXT("Inventory should confirm the presence of the helmet"), ItemContainerComponent->ContainsItems(ItemIdHelmet, 1));
    Res &= Test->TestFalse(TEXT("Inventory should not report more helmets than it contains"), ItemContainerComponent->ContainsItems(ItemIdHelmet, 2));

    // Test GetAllItems to ensure it returns all added items correctly
    TArray<FRancItemInstance> AllItems = ItemContainerComponent->GetAllItems();
    Res &= Test->TestTrue(TEXT("GetAllItems should include rocks"), AllItems.ContainsByPredicate([](const FRancItemInstance& Item) { return Item.ItemId == ItemIdRock && Item.Quantity == 5; }));
    Res &= Test->TestTrue(TEXT("GetAllItems should include the helmet"), AllItems.ContainsByPredicate([](const FRancItemInstance& Item) { return Item.ItemId == ItemIdHelmet && Item.Quantity == 1; }));
    
    // Remove some items and test counts again
    ItemContainerComponent->RemoveItems_IfServer(FRancItemInstance(ItemIdRock, 3), true); // Remove 3 Rocks
    Res &= Test->TestEqual(TEXT("After removal, inventory should report 2 rocks"), ItemContainerComponent->GetItemCount(ItemIdRock), 2);
    
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
    
    // Assuming InitializeTestItems() initializes the items including a "Rock" and "Helmet" with their respective tags.
    InitializeTestItems();

    // Test FindItemById - Initially, the container should not contain a "Rock" item.
    FRancItemInstance RockItemInstance = ItemContainerComponent->FindItemById(ItemIdRock);
    bool bTestFindBeforeAdd = Test->TestTrue(TEXT("FindItemById should not find an item before it's added"), RockItemInstance.ItemId != ItemIdRock);
    
    // Add a "Rock" item to the container.
    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 1));
    
    // Test FindItemById - Now, the container should contain the "Rock" item.
    RockItemInstance = ItemContainerComponent->FindItemById(ItemIdRock);
    bool bTestFindAfterAdd = Test->TestTrue(TEXT("FindItemById should find the item after it's added"), RockItemInstance.ItemId == ItemIdRock);

    // Test ContainsItems for "Rock".
    bool bTestContainsItems = Test->TestTrue(TEXT("ContainsItems should return true for items present in the container"), ItemContainerComponent->ContainsItems(ItemIdRock, 1));

    // Test IsEmpty before and after adding items.
    bool bTestIsEmptyBefore = Test->TestFalse(TEXT("IsEmpty should return false when items are present"), ItemContainerComponent->IsEmpty());
    ItemContainerComponent->DropAllItems_IfServer(); // Assuming this clears all items.
    bool bTestIsEmptyAfter = Test->TestTrue(TEXT("IsEmpty should return true when no items are present"), ItemContainerComponent->IsEmpty());


    ItemContainerComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 1));
    ItemContainerComponent->DropItems(FRancItemInstance(ItemIdRock, 1));
    
    return bTestFindBeforeAdd && bTestFindAfterAdd && bTestContainsItems && bTestIsEmptyBefore && bTestIsEmptyAfter;
}

bool FRancItemContainerComponentTest::RunTest(const FString& Parameters)
{
    bool Res = true;
    Res &= TestAddItems(this);
    Res &= TestRemoveItems(this);
    Res &= TestCanReceiveItems(this);
    Res &= TestItemCountsAndPresence(this);
    Res &= TestMiscFunctions(this);
    // Can't currently test drop because it tries to get world from the component, which tries to get owning actor (null) to call getworld on
    
    return Res;
}