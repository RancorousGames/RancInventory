#include "SlotMapperTest.h" // header not needed

#include "NativeGameplayTags.h"
#include "Components/RancInventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "Management/RancInventorySlotMapper.h"
#include "InventorySetup.cpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlotMapperTest, "GameTests.SlotMapper.Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#define SETUP_SLOTMAPPER(CarryCapacity, NumSlots, PreferUniversalSlots) \
    URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); \
    InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot); \
    InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot); \
    InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot); \
    InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot); \
    InventoryComponent->MaxContainerSlotCount = NumSlots; \
    InventoryComponent->MaxWeight = CarryCapacity; \
    URancInventorySlotMapper* SlotMapper = NewObject<URancInventorySlotMapper>(); \
	SlotMapper->Initialize(InventoryComponent, NumSlots, PreferUniversalSlots); \
	InitializeTestItems();
	


bool TestInitializeSlotMapper(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(15.0f, 9, false);

	bool Res = true;
    
	// Test if the linked inventory component is correctly set
	Res &= Test->TestNotNull(TEXT("InventoryComponent should not be null after initialization"), SlotMapper->LinkedInventoryComponent);
    
	// Test if the number of slots is correctly set
	Res &= Test->TestEqual(TEXT("SlotMapper should have the correct number of slots"), SlotMapper->NumberOfSlots, 9);

	// Verify that the slot mapper has correctly initialized empty slots
	for (int32 Index = 0; Index < SlotMapper->NumberOfSlots; ++Index)
	{
		bool IsSlotEmpty = SlotMapper->IsSlotEmpty(Index);
		Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be initialized as empty"), Index), IsSlotEmpty);
	}

	// Verify tagged slots are initialized and empty
	Res &= Test->TestTrue(TEXT("LeftHandSlot should be initialized and empty"), SlotMapper->IsTaggedSlotEmpty(LeftHandSlot));
	Res &= Test->TestTrue(TEXT("RightHandSlot should be initialized and empty"), SlotMapper->IsTaggedSlotEmpty(RightHandSlot));
	Res &= Test->TestTrue(TEXT("HelmetSlot should be initialized and empty"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot));
	Res &= Test->TestTrue(TEXT("ChestSlot should be initialized and empty"), SlotMapper->IsTaggedSlotEmpty(ChestSlot));

	return Res;
}

bool TestReactionToInventoryEvents(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(99.0f, 9, false);

    bool Res = true;
    // Test adding items
    InventoryComponent->AddItems_IfServer(FiveRocks);
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect 5 rocks added to the first slot"), SlotMapper->GetItem(0).Quantity, 5);
	Res &= Test->TestEqual(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetContainerItemCount(ItemIdRock), 5);
	
    // Test adding items to a tagged slot
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true);
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect the helmet added to the tagged slot"), SlotMapper->GetItemForTaggedSlot(HelmetSlot).Quantity, 1);
    
    // Test removing items from a generic slot
    InventoryComponent->RemoveItems_IfServer(FiveRocks);
    Res &= Test->TestTrue(TEXT("First slot should be empty after removing rocks"), SlotMapper->IsSlotEmpty(0));
    
    // Test removing items from a tagged slot
    InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(HelmetSlot, 1);
    Res &= Test->TestTrue(TEXT("HelmetSlot should be empty after removing the helmet"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot));
    
    // Test adding more items to an existing stack
    InventoryComponent->AddItems_IfServer(ThreeRocks);
    InventoryComponent->AddItems_IfServer(TwoRocks);
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect 5 rocks added to the first slot again"), SlotMapper->GetItem(0).Quantity, 5);
    
    // Test exceeding max stack
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 10));
    Res &= Test->TestTrue(TEXT("SlotMapper should handle exceeding max stack correctly"), SlotMapper->GetItem(0).Quantity == 5 && SlotMapper->GetItem(1).Quantity == 5 && SlotMapper->GetItem(2).Quantity == 5);
    
    // Test partial removal of items
    InventoryComponent->RemoveItems_IfServer(ThreeRocks);
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect 2 rocks remaining in first slot after partial removal"), SlotMapper->GetItem(0).Quantity, 2);

	// The next 3 test blocks use SlotMapper->MoveItems where i meant to use InventoryComponent->MoveItems_IfServer
	// but it ended up catching some bugs so i'll leave them here
	
    // Test move 2 rocks from slot 0 to slot to hand making a full stack in hand
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, ThreeRocks, true);
    SlotMapper->MoveItems(NoTag, 0, LeftHandSlot, -1);
    Res &= Test->TestTrue(TEXT("SlotMapper should reflect 5 rocks in LeftHandSlot and empty slot 0"), SlotMapper->GetItemForTaggedSlot(LeftHandSlot).Quantity == 5 && SlotMapper->IsSlotEmpty(0));

	// Test moving item from a tagged slot to an empty generic slot
	SlotMapper->MoveItems(LeftHandSlot, -1, NoTag, 0);
	Res &= Test->TestTrue(TEXT("After moving rocks from LeftHandSlot to slot 0, slot 0 should have 2 rocks"), SlotMapper->GetItem(0).Quantity == 5 && SlotMapper->IsTaggedSlotEmpty(LeftHandSlot));
	Res &= Test->TestFalse(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemInstance.IsValid());
	
	// Test splitting stack from tagged slot to empty generic slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, TwoRocks, true); // Reset LeftHandSlot with 2 rocks
	Res &= Test->TestEqual(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemInstance.Quantity, 2);
	SlotMapper->SplitItems(LeftHandSlot, -1, NoTag, 3, 1); // Split 1 rock to an empty slot 3
	Res &= Test->TestEqual(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemInstance.Quantity, 1);
	Res &= Test->TestEqual(TEXT("After splitting, LeftHandSlot should have 1 rock"), SlotMapper->GetItemForTaggedSlot(LeftHandSlot).Quantity, 1);
	Res &= Test->TestEqual(TEXT("After splitting, slot 3 should have 1 rock"), SlotMapper->GetItem(3).Quantity, 1);

	// Now lets actually use InventoryComponent->MoveItems_IfServer
	// We have 1 rock in LeftHandSlot, 1 rock in slot 3, 2 rocks in slot 0, 5 rocks in slot 1 and 2
	Res &= Test->TestEqual(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetContainerItemCount(ItemIdRock), 16);
	Res &= Test->TestEqual(TEXT("Inventory component should match slotmapper"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemInstance.Quantity, 1);
	
    // Test moving items from a generic slot to a tagged slot that is not empty
    InventoryComponent->MoveItems_ServerImpl(FiveRocks, NoTag, LeftHandSlot); // Move 4/5 rocks from container to LeftHandSlot
	// slot 0 should now have no rocks, slot 1 should have 2 rocks, LeftHandSlot should have 5 rocks
	Res &= Test->TestEqual(TEXT("SlotMapper should reflect 5 rocks in LeftHandSlot"), SlotMapper->GetItemForTaggedSlot(LeftHandSlot).Quantity, 5);
	Res &= Test->TestEqual(TEXT("Slot 0 should have 1 rock left after moving 4 rocks to LeftHandSlot"), SlotMapper->GetItem(0).Quantity, 1);

    // Test moving items from a tagged slot to a generic slot when both have items
    InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FiveRocks, true); // Ensure RightHandSlot has items for moving
    InventoryComponent->MoveItems_ServerImpl(FiveRocks, RightHandSlot, NoTag); // Move items from RightHandSlot to container
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect moved items from RightHandSlot to slot 0"), SlotMapper->GetItem(0).Quantity, 5);
    Res &= Test->TestEqual(TEXT("SlotMapper should reflect moved items from RightHandSlot to slot 3"), SlotMapper->GetItem(3).Quantity, 2);
    Res &= Test->TestTrue(TEXT("RightHandSlot should be empty after moving items to slot 0"), SlotMapper->IsTaggedSlotEmpty(RightHandSlot));

    return Res;
}

bool TestAddItemsToSlotMapper(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(15.0f, 9, false);

	bool Res = true;
    
	// Simulate adding a rock to the inventory
	InventoryComponent->AddItems_IfServer(ThreeRocks);
	
	FRancItemInstance Item = SlotMapper->GetItem(0);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 3 rocks added to the first slot"), Item.ItemId == ItemIdRock && Item.Quantity == 3);

	InventoryComponent->AddItems_IfServer(ThreeRocks);
	Item = SlotMapper->GetItem(0);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 5 rocks added the first"), Item.ItemId == ItemIdRock && Item.Quantity == 5);
	Item = SlotMapper->GetItem(1);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 1 rock added to the second slot"), Item.ItemId == ItemIdRock && Item.Quantity == 1);

	Res &= Test->TestTrue(TEXT("HelmetSlot should be empty"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot));
	
	InventoryComponent->AddItemsToAnySlots_IfServer(OneHelmet, true);

	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the tagged slot"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot) == false);

	// Add another helmet, which should go to generic slots
	InventoryComponent->AddItemsToAnySlots_IfServer(OneHelmet, false);
	Item = SlotMapper->GetItem(2);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the third slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);

	// Add another helmet to hand slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, false);
	Item = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the left hand slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);
	
	return Res;
}

bool TestMoveAndSwap(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(20.0f, 9, false);

	bool Res = true;
    
	// Add initial items to inventory for setup
	InventoryComponent->AddItems_IfServer(FiveRocks);
	InventoryComponent->AddItems_IfServer(ThreeSticks);

	// Move rock from slot 0 to slot 1, where sticks are, and expect them to swap
	SlotMapper->MoveItems(NoTag, 0, NoTag, 1);
	FRancItemInstance ItemInSlot0AfterMove = SlotMapper->GetItem(0);
	FRancItemInstance ItemInSlot1AfterMove = SlotMapper->GetItem(1);
	Res &= Test->TestTrue(TEXT("Slot 0 should now contain sticks after swap"), ItemInSlot0AfterMove.ItemId == ItemIdSticks && ItemInSlot0AfterMove.Quantity == 3);
	Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap"), ItemInSlot1AfterMove.ItemId == ItemIdRock && ItemInSlot1AfterMove.Quantity == 5);

	// Add helmet to inventory and attempt to swap with rocks in slot 1
	InventoryComponent->AddItems_IfServer(OneHelmet);
	SlotMapper->MoveItems(NoTag, 2, NoTag, 1);
	FRancItemInstance ItemInSlot1AfterHelmetSwap = SlotMapper->GetItem(1);
	FRancItemInstance ItemInSlot2AfterHelmetSwap = SlotMapper->GetItem(2);
	Res &= Test->TestTrue(TEXT("Slot 1 should now contain a helmet after swap"), ItemInSlot1AfterHelmetSwap.ItemId == ItemIdHelmet && ItemInSlot1AfterHelmetSwap.Quantity == 1);
	Res &= Test->TestTrue(TEXT("Slot 2 should now contain rocks after swap"), ItemInSlot2AfterHelmetSwap.ItemId == ItemIdRock && ItemInSlot2AfterHelmetSwap.Quantity == 5);

	// Test moving item 3 sticks from a generic slot to a universal tagged slot (LeftHandSlot) with 3 sticks
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, ThreeSticks, false);
	SlotMapper->MoveItems(NoTag, 0, LeftHandSlot, -1);
 	FRancItemInstance ItemInLeftHandAfterMove = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 5 sticks after move"), ItemInLeftHandAfterMove.ItemId == ItemIdSticks && ItemInLeftHandAfterMove.Quantity == 5);
	// generic slots should contain the remaining 1 sticks
	Res &= Test->TestTrue(TEXT("Slot 0 should contain 1 stick after move"), SlotMapper->GetItem(0).ItemId == ItemIdSticks && SlotMapper->GetItem(0).Quantity == 1);

	// move helmet from slot 1 to expectedly helmet slot
	SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 1);
	// slot 1 should now be empty and helmet slot should have helmet
	Res &= Test->TestTrue(TEXT("Slot 1 should be empty after moving helmet to HelmetSlot"), SlotMapper->IsSlotEmpty(1));
	Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 helmet after move"), SlotMapper->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet && SlotMapper->GetItemForTaggedSlot(HelmetSlot).Quantity == 1);
	
	// Test moving helmet to LeftHandSlot which would swap except Helmet slot can't hold spear so it should fail
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneSpear, true); // replacing the 5 sticks with spear
    SlotMapper->MoveItems(HelmetSlot, -1, LeftHandSlot, -1);
    FRancItemInstance ItemInLeftHandAfterHelmetMove = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    FRancItemInstance HelmetInHelmetSlotAfterMove = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should still  contain spear after failed move"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSpear && ItemInLeftHandAfterHelmetMove.Quantity == 1);
	Res &= Test->TestTrue(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet && HelmetInHelmetSlotAfterMove.Quantity == 1);

	// Now try the same thing but other direction which should also fail as it would cause an invalid swap
	SlotMapper->MoveItems(LeftHandSlot, -1, HelmetSlot, -1);
	ItemInLeftHandAfterHelmetMove = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	HelmetInHelmetSlotAfterMove = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should still  contain spear after failed move"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSpear && ItemInLeftHandAfterHelmetMove.Quantity == 1);
	Res &= Test->TestTrue(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet && HelmetInHelmetSlotAfterMove.Quantity == 1);
	
	
    // Attempt to move a non-helmet item from generic slot HelmetSlot, which should fail and no swap occurs
    SlotMapper->MoveItems(NoTag, 2, HelmetSlot, -1); // Assuming rocks are at slot 2 now
    FRancItemInstance ItemInHelmetSlotAfterInvalidMove = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
    FRancItemInstance ItemInSlot2AfterInvalidMove = SlotMapper->GetItem(2);
    Res &= Test->TestTrue(TEXT("HelmetSlot should not accept non-helmet item, should remain helmet"), ItemInHelmetSlotAfterInvalidMove.ItemId == ItemIdHelmet && ItemInHelmetSlotAfterInvalidMove.Quantity == 1);
    Res &= Test->TestTrue(TEXT("Slot 2 should remain unchanged after invalid move attempt"), ItemInSlot2AfterInvalidMove.ItemId == ItemIdRock && ItemInSlot2AfterInvalidMove.Quantity == 5);

    // Move item from a universal tagged slot to a specialized tagged slot that accepts it
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, true); // Override spear with another helmet
    SlotMapper->MoveItems(LeftHandSlot, -1, HelmetSlot, -1);
    FRancItemInstance ItemInHelmetSlotAfterSwapBack = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
    FRancItemInstance ItemInLeftHandAfterSwapBack = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 helmet"), ItemInHelmetSlotAfterSwapBack.ItemId == ItemIdHelmet && ItemInHelmetSlotAfterSwapBack.Quantity == 1);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 1 helmet"), ItemInLeftHandAfterSwapBack.ItemId == ItemIdHelmet && ItemInLeftHandAfterSwapBack.Quantity == 1);
	
    // Test moving item to an already occupied generic slot to ensure they swap
    InventoryComponent->AddItems_IfServer(OneSpear); // Add spear to inventory
    SlotMapper->MoveItems(NoTag, 1, NoTag, 2); // Assuming spear is at slot 1 now, and rocks at slot 2
    FRancItemInstance ItemInSlot1AfterSwap = SlotMapper->GetItem(1);
    FRancItemInstance ItemInSlot2AfterSwap = SlotMapper->GetItem(2);
    Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap with spear"), ItemInSlot1AfterSwap.ItemId == ItemIdRock && ItemInSlot1AfterSwap.Quantity == 5);
    Res &= Test->TestTrue(TEXT("Slot 2 should now contain the spear after swap"), ItemInSlot2AfterSwap.ItemId == ItemIdSpear && ItemInSlot2AfterSwap.Quantity == 1);

    return Res;
}

bool TestSplitItems(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(99, 9, false);

    bool Res = true;
    
    // Add initial items to slots to prepare for split tests
    InventoryComponent->AddItems_IfServer(FiveRocks); // Added to first generic slot

    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true); // Added to HelmetSlot

    // Valid split in generic slots
    SlotMapper->SplitItems(NoTag, 0, NoTag, 1, 2);
    Res &= Test->TestEqual(TEXT("After splitting, first slot should have 3 rocks"), SlotMapper->GetItem(0).Quantity, 3);
    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 2 rocks"), SlotMapper->GetItem(1).Quantity, 2);

    // Invalid split due to insufficient quantity in source slot
    SlotMapper->SplitItems(NoTag, 0, NoTag, 1, 4); // Trying to split more than available
    Res &= Test->TestEqual(TEXT("Attempt to split more rocks than available should fail"), SlotMapper->GetItem(0).Quantity, 3);

    // Split between a generic slot and a tagged slot
    SlotMapper->SplitItems(NoTag, 1, RightHandSlot, -1, 1); // Splitting 1 rock to RightHandSlot
    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 1 rock"), SlotMapper->GetItem(1).Quantity, 1);
    FRancItemInstance RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot);
    Res &= Test->TestTrue(TEXT("RightHandSlot should now contain 1 rock"), RightHandItem.ItemId == ItemIdRock && RightHandItem.Quantity == 1);

    // Invalid split to a different item type slot
    SlotMapper->SplitItems(RightHandSlot, -1, HelmetSlot, -1, 1); // Trying to split rock into helmet slot
    RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot); // Re-fetch to check for changes
    Res &= Test->TestTrue(TEXT("Attempting to split into a different item type slot should fail"), RightHandItem.Quantity == 1 && SlotMapper->GetItemForTaggedSlot(HelmetSlot).Quantity == 1);

    // Exceeding max stack size
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 8)); // Add more rocks to force a stack size check
    SlotMapper->SplitItems(NoTag, 2, NoTag, 1, 2); // Attempt to overflow slot 1 with rocks
    Res &= Test->TestEqual(TEXT("Splitting that exceeds max stack size should fail"), SlotMapper->GetItem(1).Quantity, 5); 
    // Attempt split from tagged to generic slot with valid quantities
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, FiveRocks);
    SlotMapper->SplitItems(LeftHandSlot, -1, NoTag, 2, 1); // Splitting 1 rock to a new generic slot
    FRancItemInstance NewGenericSlotItem = SlotMapper->GetItem(2);
    Res &= Test->TestEqual(TEXT("After splitting from tagged to generic, new slot should contain 3 rocks total"), NewGenericSlotItem.Quantity, 3);
	FRancItemInstance LeftHandItem = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestEqual(TEXT("LeftHandSlot should now contain 4 rocks"), LeftHandItem.Quantity, 4);
	
    // split from generic to tagged slot
    SlotMapper->SplitItems(NoTag, 2, LeftHandSlot, -1, 1); // Attempt to add another helmet to HelmetSlot
    FRancItemInstance ItemInLeftHandAfterSplit = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestEqual(TEXT("LeftHandSlot should now contain 5 rocks"), ItemInLeftHandAfterSplit.Quantity, 5);
	Res &= Test->TestEqual(TEXT("Slot 2 should now contain 2 rocks"), SlotMapper->GetItem(2).Quantity, 2);
	
	// Status: lefthand 5 rocks, right hand 1 rock, slots 0 and 1, 5 rocks, slot 2 2 rocks, helmetSlot 1 helmet
	
    // Split with empty/invalid indices and tags
	SlotMapper->SplitItems(NoTag, 5, NoTag, 6, 1); // Invalid indices
	Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes"), SlotMapper->IsSlotEmpty(5) && SlotMapper->IsSlotEmpty(6));
    SlotMapper->SplitItems(NoTag, 10, NoTag, 11, 1); // Invalid indices
    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes"), SlotMapper->IsSlotEmpty(10) && SlotMapper->IsSlotEmpty(11));

    SlotMapper->SplitItems(NoTag, -1, ChestSlot, -1, 1); // Invalid source tag
    Res &= Test->TestTrue(TEXT("Invalid source tag should result in no changes"), SlotMapper->IsTaggedSlotEmpty(ChestSlot));

    // Attempt to split into a slot with a different item type
    InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, OneSpear, true); // Adding spear to RightHandSlot
    SlotMapper->SplitItems(NoTag, 0, RightHandSlot, -1, 1); // Attempt to split rock into RightHandSlot containing a spear
    RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot);
    Res &= Test->TestTrue(TEXT("Attempting to split into a slot with a different item type should fail"), RightHandItem.ItemId == ItemIdSpear && RightHandItem.Quantity == 1);
	Res &= Test->TestTrue(TEXT("Source slot should remain unchanged after failed split"), SlotMapper->GetItem(0).Quantity == 5);
	
    return Res;
}

bool TestMoveItemToAnyTaggedSlot(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(25.0f, 9, false);

    bool Res = true;
    
    // Add a mix of items to test moving to tagged slots
    InventoryComponent->AddItems_IfServer(ThreeRocks);
    InventoryComponent->AddItems_IfServer(OneHelmet);
    InventoryComponent->AddItems_IfServer(OneSpear);
    InventoryComponent->AddItems_IfServer(OneChestArmor);

    // Move rock to any tagged slot (should go to a universal slot)
    Res &= Test->TestTrue(TEXT("Move rock to any tagged slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 0));
    Res &= Test->TestTrue(TEXT("Rock should be in the first universal tagged slot, left hand"), SlotMapper->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdRock);

    // Move helmet to its specialized slot
    Res &= Test->TestTrue(TEXT("Move helmet to its specialized slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 1));
    Res &= Test->TestTrue(TEXT("Helmet should be in HelmetSlot"), SlotMapper->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);

    // Move spear to any tagged slot (should go to a universal slot)
    Res &= Test->TestTrue(TEXT("Move spear to any tagged slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 2));
    Res &= Test->TestTrue(TEXT("Spear should be in right hand tagged slot"), SlotMapper->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);

    // Attempt to move item that is already in its correct tagged slot (helmet), should result in no action
    Res &= Test->TestFalse(TEXT("Attempting to move helmet already in HelmetSlot should do nothing"), SlotMapper->MoveItemToAnyTaggedSlot(HelmetSlot, -1));

    // Move chest armor to its specialized slot
    Res &= Test->TestTrue(TEXT("Move chest armor to its specialized slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 3));
    Res &= Test->TestTrue(TEXT("Chest armor should be in ChestSlot"), !SlotMapper->IsTaggedSlotEmpty(ChestSlot));

    // Attempt to move an item to a tagged slot when all suitable slots are occupied
    InventoryComponent->AddItems_IfServer(OneRock); // Add another rock
    Res &= Test->TestFalse(TEXT("Attempt to move extra rock to should fail as no slots are available"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 4));
	
    InventoryComponent->AddItems_IfServer(OneSpecialHelmet); // goes to slot 1
    Res &= Test->TestTrue(TEXT("A different helmet should swap into the helmet slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 1));
	Res &= Test->TestTrue(TEXT("Special helmet should be in HelmetSlot"), SlotMapper->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdSpecialHelmet);
	Res &= Test->TestTrue(TEXT("Helmet should be in generic slot 0"), SlotMapper->GetItem(1).ItemId == ItemIdHelmet);

    // Attempt to move an item to a tagged slot when the source index is invalid
    Res &= Test->TestFalse(TEXT("Attempting to move item from invalid source index should fail"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 100));

    // Attempt to move an item from a tagged slot that is empty
    Res &= Test->TestFalse(TEXT("Attempting to move item from an empty tagged slot should fail"), SlotMapper->MoveItemToAnyTaggedSlot(ChestSlot, -1));
	
    return Res;
}


bool TestMoveItemToAnyTaggedSlotPreferUniversal(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(15.0f, 9, /* PreferEmptyUniversalSlot */ true);

    bool Res = true;

	// Add one helmet to slot 0 and one to helmet slot
    InventoryComponent->AddItems_IfServer(OneHelmet);
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneSpecialHelmet, true);

	// Move helmet to any tagged slot, should prefer empty universal slot
	Res &= Test->TestTrue(TEXT("Move helmet to any tagged slot"), SlotMapper->MoveItemToAnyTaggedSlot(NoTag, 0));
	Res &= Test->TestTrue(TEXT("Helmet should be in HelmetSlot"), SlotMapper->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
	Res &= Test->TestTrue(TEXT("Special helmet should be in generic slot 0"), SlotMapper->GetItem(0).ItemId == ItemIdSpecialHelmet);
	
    return Res;
}

bool TestSlotReceiveItem(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(10.0f, 5, false); // Setup with max weight capacity of 50

    bool Res = true;
    
    // Adding item to an empty slot
    Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), SlotMapper->CanSlotReceiveItem(ThreeRocks, 0));

	// Adding more of that item up to maxstack for the same slot
	Res &= Test->TestTrue(TEXT("Can add more rocks to slot with same item type"), SlotMapper->CanSlotReceiveItem(TwoRocks, 0));
	InventoryComponent->AddItems_IfServer(TwoRocks); 
	
    // Trying to add item to a slot with a different item type
    Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), SlotMapper->CanSlotReceiveItem(OneHelmet, 0));

	// Add helmet to a different slot
	Res &= Test->TestTrue(TEXT("Can add helmet to a different slot"), SlotMapper->CanSlotReceiveItem(OneHelmet, 1));
	
    // SAdding item exceeding max stack size
    Res &= Test->TestFalse(TEXT("Cannot add rocks exceeding max stack size"), SlotMapper->CanSlotReceiveItem(FiveRocks, 0));

    // Adding item to an out-of-bounds slot
    Res &= Test->TestFalse(TEXT("Cannot add item to an out-of-bounds slot"), SlotMapper->CanSlotReceiveItem(ThreeRocks, 10));

    // Weight based test, add a giant boulder with weight 10
    Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), SlotMapper->CanSlotReceiveItem(GiantBoulder, 1));

	// Testing CanTaggedSlotReceiveItem
	Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), SlotMapper->CanTaggedSlotReceiveItem(ThreeRocks, LeftHandSlot));
	Res &= Test->TestTrue(TEXT("Cannot add rocks to helmet slot"), SlotMapper->CanTaggedSlotReceiveItem(ThreeRocks, HelmetSlot) == false);
	Res &= Test->TestTrue(TEXT("Can add helmet to a matching specialized slot"), SlotMapper->CanTaggedSlotReceiveItem(OneHelmet, HelmetSlot));
	Res &= Test->TestTrue(TEXT("Can add helmet to a universal slot"), SlotMapper->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, FiveRocks, true);
	Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), SlotMapper->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
	Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), SlotMapper->CanTaggedSlotReceiveItem(GiantBoulder, RightHandSlot));
	
    return Res;
}
	
bool FSlotMapperTest::RunTest(const FString& Parameters)
{
	bool Res = true;
	Res &= TestInitializeSlotMapper(this);
	Res &= TestReactionToInventoryEvents(this);
	Res &= TestAddItemsToSlotMapper(this);
	Res &= TestMoveAndSwap(this);
	Res &= TestSplitItems(this);
	Res &= TestMoveItemToAnyTaggedSlot (this);
	Res &= TestMoveItemToAnyTaggedSlotPreferUniversal (this);
	Res &= TestSlotReceiveItem (this);
	// TestDropItem 
    
	return Res;
}