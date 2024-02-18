
#include "NativeGameplayTags.h"

#include "Management/RancInventoryData.h"
#include "Components/RancItemContainerComponent.h"
#include "Components/RancInventoryComponent.h"
#include "Engine/AssetManager.h"
#include "Management/RancInventoryFunctions.h"
#include "Misc/AutomationTest.h"

#include "InventorySetup.cpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, "GameTests.RancInventoryComponent.BasicTests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#define SETUP_RANCINVENTORY(MaxItems, CarryCapacity) \
URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); \
InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot.GetTag()); \
InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot.GetTag()); \
InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot.GetTag()); \
InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot.GetTag()); \
InventoryComponent->MaxNumItems = MaxItems; \
InventoryComponent->MaxWeight = CarryCapacity; \
InitializeTestItems();

FRancInventoryComponentTest* RITest;


bool TestAddingTaggedSlotItems()
{
    SETUP_RANCINVENTORY(9, 100);

    bool Res = true;
    
    // Ensure the left hand slot is initially empty
    Res &= RITest->TestTrue(TEXT("No item should be in the left hand slot before addition"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).IsValid());
    
    // Add an unstackable item to the left hand slot
    InventoryComponent->AddItemToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), false);
    Res &= RITest->TestTrue(TEXT("Unstackable Item should be in the left hand slot after addition"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

    // Attempt to add another unstackable item to the same slot without override - should fail
    InventoryComponent->AddItemToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), false);
    Res &= RITest->TestTrue(TEXT("Second unstackable item should not replace the first one without override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.Quantity == 1);
    
    // Attempt to add another unstackable item to the same slot with override - should succeed
    InventoryComponent->AddItemToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    Res &= RITest->TestEqual(TEXT("Second unstackable item should replace the first one with override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.Quantity, 1);

    // Test adding to a specialized slot that should only accept specific items
    // Assuming HelmetSlot only accepts items with HelmetTag
    InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdSpear.GetTag(), 1), true);
    Res &= RITest->TestTrue(TEXT("Non-helmet item should not be added to the helmet slot"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).IsValid());

    // Test adding a correct item to a specialized slot
    InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    Res &= RITest->TestTrue(TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));
	
	// Test adding a stackable item to an empty slot and then adding a different stackable item to the same slot without override
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), false); // Assuming this is reset from previous tests
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 2), false);
	Res &= RITest->TestFalse(TEXT("Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock) without override"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSticks));

	// Test adding an item to a slot that is not designated as either universal or specialized (invalid slot)
	InventoryComponent->AddItemToTaggedSlot_IfServer(FGameplayTag::EmptyTag, FRancItemInstance(ItemIdRock.GetTag(), 1), false);
	Res &= RITest->TestFalse(TEXT("Item should not be added to an invalid slot"), InventoryComponent->GetItemForTaggedSlot(FGameplayTag::EmptyTag).IsValid());

	// Test adding a stackable item to the max stack size and then attempting to add more with override, which should return 0
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 5), true); // Reset to max stack size
	int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), true); // Override with less than max stack size
	Res &= RITest->TestEqual(TEXT("Stackable Item (Rock) amount added should be none as already full stack"), AmountAdded, 0);

	// Test adding a stackable item to a slot that has a different stackable item with override enabled
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 4), true); // Assuming different item than before
	Res &= RITest->TestTrue(TEXT("Different stackable item (Sticks) should replace existing item (Rock) in slot with override"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSticks) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.Quantity == 4);
	
    return Res;
}

bool TestRemovingTaggedSlotItems()
{
    SETUP_RANCINVENTORY(9, 100);
    InitializeTestItems(); // Assuming this properly sets up the item data

    bool Res = true;

    // Add stackable item to a slot
    InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), false);

    // Remove a portion of the stackable item
    int32 RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 2, true);
    Res &= RITest->TestTrue(TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
    Res &= RITest->TestTrue(TEXT("Right hand slot should have 1 Rock remaining after partial removal"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.Quantity == 1);

    // Attempt to remove more items than are present without allowing partial removals
    RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 2, false);
    Res &= RITest->TestTrue(TEXT("Should not remove any items if attempting to remove more than present without allowing partial removal"), RemovedQuantity == 0);

    // Add an unstackable item to a slot and then remove it
    InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(HelmetSlot.GetTag(), 1, true);
    Res &= RITest->TestTrue(TEXT("Should successfully remove unstackable item (Helmet)"), RemovedQuantity == 1);
    Res &= RITest->TestFalse(TEXT("Helmet slot should be empty after removing the item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).IsValid());

    // Attempt to remove an item from an empty slot
    RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(LeftHandSlot.GetTag(), 1, true);
    Res &= RITest->TestTrue(TEXT("Should not remove any items from an empty slot"), RemovedQuantity == 0);

    // Attempt to remove an item from a non-existent slot
    RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(FGameplayTag::EmptyTag, 1, true);
    Res &= RITest->TestTrue(TEXT("Should not remove any items from a non-existent slot"), RemovedQuantity == 0);

    return Res;
}

bool TestMoveTaggedSlotItems()
{
    SETUP_RANCINVENTORY(9, 100);
    InitializeTestItems(); // Prepare test items

    bool Res = true;

    // Add item to a tagged slot directly
    InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    Res &= RITest->TestTrue(TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

    // Move item from tagged slot to generic inventory (cannot directly verify generic inventory, so just ensure removal)
    int32 MovedQuantity = InventoryComponent->MoveItemsFromTaggedSlot_ServerImpl(FRancItemInstance(ItemIdHelmet.GetTag(), 1), HelmetSlot.GetTag());
    Res &= RITest->TestEqual(TEXT("Should move the helmet item from the tagged slot to generic inventory"), MovedQuantity, 1);
    
    // Assume generic inventory now has the helmet item
    
    // Move item back to a different tagged slot from generic inventory
    MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdHelmet.GetTag(), 1), RightHandSlot.GetTag());
    Res &= RITest->TestEqual(TEXT("Should move the helmet item from generic inventory to right hand slot"), MovedQuantity, 1);
    Res &= RITest->TestTrue(TEXT("Right hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

    // Move item from one tagged slot to another directly
    MovedQuantity = InventoryComponent->MoveItemsFromAndToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdHelmet.GetTag(), 1), RightHandSlot.GetTag(), LeftHandSlot.GetTag());
    Res &= RITest->TestEqual(TEXT("Should move the helmet item from right hand slot to left hand slot"), MovedQuantity, 1);
    Res &= RITest->TestTrue(TEXT("Left hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

    // Attempt to move an item that doesn't exist in the source tagged slot
    MovedQuantity = InventoryComponent->MoveItemsFromTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 1), HelmetSlot.GetTag());
    Res &= RITest->TestEqual(TEXT("Should not move an item that doesn't exist in the source tagged slot"), MovedQuantity, 0);

	// Add an item compatible with RightHandSlot but not with HelmetSlot
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSpear.GetTag(), 1), true);
	Res &= RITest->TestTrue(TEXT("Spear item should be added to the right hand slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSpear));

	// Attempt to move the Spear (Weapon) to HelmetSlot (Armor) directly
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move the spear item to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Helmet slot should not contain the spear item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).IsValid());

	// Move the Spear back to an empty RightHandSlot from HelmetSlot
	InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Ensure HelmetSlot is occupied with a compatible item
	MovedQuantity = InventoryComponent->MoveItemsFromAndToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), RightHandSlot.GetTag(), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from right hand slot to helmet slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSpear));
	Res &= RITest->TestTrue(TEXT("Helmet slot should remain unchanged"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to move stackable items to a non-stackable slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 1), true);
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 5), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move stackable item to a non-stackable slot"), MovedQuantity, 0);

	// Move item to a slot with a different, incompatible item type
	InventoryComponent->AddItemToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Reset for clarity
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move item to a slot with a different item type"), MovedQuantity, 0);

	// Attempt to move items from an empty or insufficient source slot
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 1, true);
	MovedQuantity = InventoryComponent->MoveItemsFromTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 2), RightHandSlot.GetTag()); // Assuming RightHandSlot is empty or has less than 2 Spears
	Res &= RITest->TestEqual(TEXT("Should not move items from an empty or insufficient source slot"), MovedQuantity, 0);

	// Move item to a slot with specific item type restrictions not met
	InventoryComponent->AddItemToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), true); // Reset with a stackable item
	MovedQuantity = InventoryComponent->MoveItemsFromAndToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 3), RightHandSlot.GetTag(), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move item to a slot with unmet item type restrictions"), MovedQuantity, 0);

    return Res;
}

/*
bool TestAddingUnstackableItem()
{
	//SETUP_INVENTORY(9, 100);
	URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); 
	InventoryComponent->MaxNumItems = 7; 
	InventoryComponent->MaxWeight = 55;

	// Assuming FRancItemInstance constructor takes an FGameplayTag for the item ID and an int32 for the quantity
	FRancItemInstance UnstackableItemInstance(UnstackableItem1.GetTag(), 1);

	// Adding an unstackable item to the inventory. 
	// Since this is a test, assume we're simulating server authority or running in an environment where role checks are bypassed.
	InventoryComponent->AddItems_IfServer(UnstackableItemInstance);

	// Verify the item was added correctly
	// Since GetAllItems returns an array of all items in the inventory, we can check if our unstackable item is among them.
	bool bItemFound = false;
	TArray<FRancItemInstance> AllItems = InventoryComponent->GetAllItems();
	for (const FRancItemInstance& Item : AllItems)
	{
		if (Item.ItemId == UnstackableItem1.GetTag() && Item.Quantity == 1)
		{
			bItemFound = true;
			break;
		}
	}

	// Assert that the unstackable item was found in the inventory
	RITest->TestTrue(TEXT("Unstackable Item should be added to the inventory"), bItemFound);

	// Optionally, you can also verify that the item does not stack by trying to add another of the same item
	// and checking that it increases the number of items in the inventory rather than the quantity of an existing item.
	InventoryComponent->AddItems_IfServer(UnstackableItemInstance);
	bool bCorrectItemCount = InventoryComponent->GetCurrentItemCount() == 2; // Expecting 2 since unstackable items don't stack

	// Assert that adding another unstackable item increases the item count
	return RITest->TestTrue(TEXT("Adding another unstackable item should increase the item count"), bCorrectItemCount);
}*/
/*
bool TestAddingStackableItems()
{
    SETUP_INVENTORY(9, 100);
    auto Item1Data = URancInventoryFunctions::GetItemDataById(StackableItem1.GetTag());

    // Add items below the max stack size and verify it's added correctly.
    int32 belowMaxStack = Item1Data->MaxStackSize - 1;
    int32 addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), belowMaxStack));
    bool test1 = Test->TestEqual("Adding below max stack should succeed with no remainder", addResult, 0);
    FRancItemInstance itemInSlot = SlotMapper->GetItem(0);
    bool test2 = Test->TestEqual("Slot 0 should have correct quantity below max stack", itemInSlot.Quantity, belowMaxStack);

    // Add items to reach max stack size.
    addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), 1));
    itemInSlot = SlotMapper->GetItem(0);
    bool test3 = Test->TestEqual("Adding to max stack should succeed with no remainder", addResult, 0);
    bool test4 = Test->TestEqual("Slot 0 should now be at max stack size", itemInSlot.Quantity, Item1Data->MaxStackSize);

    // Attempt to add more items than max stack size in a single operation.
   //int32 aboveMaxStackAdd = Item1Data->MaxStackSize + 5;
   //addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), aboveMaxStackAdd));
   //bool test5 = Test->TestEqual("Adding above max stack should return correct remainder", addResult, 5);

   //// Ensure no overflow to other slots when adding above max stack in initial add.
   //bool overflowTest = true;
   //for (int32 i = 2; i < 9; ++i) {
   //    overflowTest &= SlotMapper->IsSlotEmpty(i);
   //}
   //bool test6 = Test->TestTrue("No overflow to other slots for initial above max stack add", overflowTest);

    return test1 && test2 && test3 && test4;// && test5 && test6;
}

bool TestItemSwapBetweenSlots()
{
    SETUP_INVENTORY(9, 100);
    SlotMapper->AddItemToSlot(FRancItemInstance(StackableItem1.GetTag(), 1), 1);
    SlotMapper->AddItemToSlot(FRancItemInstance(StackableItem2.GetTag(), 1), 5);
    SlotMapper->MoveItem(FGameplayTag(), 1, FGameplayTag(), 5);
    bool test1 = Test->TestTrue("After moving, slot 1 should have StackableItem2.", SlotMapper->GetItem(1).ItemId.MatchesTag(StackableItem2));
    bool test2 = Test->TestTrue("After moving, slot 5 should have StackableItem1.", SlotMapper->GetItem(5).ItemId.MatchesTag(StackableItem1));
    return test1 && test2;
}

bool TestDroppingPartialStack()
{
    SETUP_INVENTORY(9, 100);
    SlotMapper->AddItemToTaggedSlot(FRancItemInstance(StackableItem1.GetTag(), 5), LeftHandSlot);
    const int32 droppedQuantity = 3;
    SlotMapper->DropItem(LeftHandSlot, -1, droppedQuantity);
    FRancTaggedItemInstance remainingItem = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    return Test->TestEqual("Remaining quantity should match expected value after dropping part of the stack.", remainingItem.ItemInstance.Quantity, 2);
}

bool TestAddingItemsBeyondCapacity()
{
    SETUP_INVENTORY(9, 100);
    FRancItemInstance extraItem(StackableItem2.GetTag(), 100);
    const int32 addResult = SlotMapper->AddItems(extraItem);
    return Test->TestTrue("Adding items beyond inventory capacity should fail.", addResult > 0);
}
*/

bool FRancInventoryComponentTest::RunTest(const FString& Parameters)
{
	//if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
	//{
	//	if (const TSharedPtr<FStreamableHandle> StreamableHandle = AssetManager->LoadPrimaryAssetsWithType(TEXT("RancInventory_ItemData")))
	//	{
	//		StreamableHandle->WaitUntilComplete(5.f);
	//	}
	//}
//
	//URancInventoryFunctions::AllItemsLoadedCallback();
    
	RITest = this;
	bool Res = true;
	Res &= TestAddingTaggedSlotItems();
	Res &= TestRemovingTaggedSlotItems();
	Res &= TestMoveTaggedSlotItems();
	
	//bool test2 = TestAddingStackableItems();
	//bool test3 = TestItemSwapBetweenSlots();
	//bool test5 = TestDroppingPartialStack();
	//bool test6 = TestAddingItemsBeyondCapacity();
    
	return Res;// test1; && test2 && test3 && test5 && test6;
}