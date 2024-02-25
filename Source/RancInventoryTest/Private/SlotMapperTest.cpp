/*
 * To learn about tests I used these resources:
 * https://dev.to/goals/unit-testing-private-functionality-in-unreal-engine-c-classes-2fig
 * https://github.com/ibbles/LearningUnrealEngine/blob/master/Unit%20tests%20source%20notes.md
 * https://www.jetbrains.com/help/rider/Unreal_Engine__Tests.html
 * https://www.youtube.com/watch?v=erNW3r9uRH0
 */


#include "SlotMapperTest.h" // header not needed

#include "NativeGameplayTags.h"
#include "Components/RancInventoryComponent.h"
#include "Engine/AssetManager.h"
#include "Management/RancInventoryFunctions.h"
#include "Misc/AutomationTest.h"
#include "Management/RancInventorySlotMapper.h"
#include "InventorySetup.cpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlotMapperTest, "GameTests.SlotMapper.Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#define SETUP_SLOTMAPPER(MaxItemsInGenericSlots, CarryCapacity, NumSlots) \
    URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); \
    InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot); \
    InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot); \
    InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot); \
    InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot); \
    InventoryComponent->MaxNumItemsInContainer = MaxItemsInGenericSlots; \
    InventoryComponent->MaxWeight = CarryCapacity; \
    URancInventorySlotMapper* SlotMapper = NewObject<URancInventorySlotMapper>(); \
	SlotMapper->Initialize(InventoryComponent, NumSlots); \
	InitializeTestItems();
	


bool TestInitializeSlotMapper(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(10, 15.0f, 9);

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

bool TestAddItemsToSlotMapper(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(10, 15.0f, 9);

	bool Res = true;
    
	// Simulate adding a rock to the inventory
	FRancItemInstance RockItemInstance(ItemIdRock, 3);
	InventoryComponent->AddItems_IfServer(RockItemInstance);
	
	FRancItemInstance Item = SlotMapper->GetItem(0);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 3 rocks added to the first slot"), Item.ItemId == ItemIdRock && Item.Quantity == 3);

	InventoryComponent->AddItems_IfServer(RockItemInstance);
	Item = SlotMapper->GetItem(0);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 5 rocks added the first"), Item.ItemId == ItemIdRock && Item.Quantity == 5);
	Item = SlotMapper->GetItem(1);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect 1 rock added to the second slot"), Item.ItemId == ItemIdRock && Item.Quantity == 1);

	Res &= Test->TestTrue(TEXT("HelmetSlot should be empty"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot));
	
	FRancItemInstance HelmetItemInstance(ItemIdHelmet, 1); 
	InventoryComponent->AddItemsToAnySlots_IfServer(HelmetItemInstance, true);

	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the tagged slot"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot) == false);

	// Add another helmet, which should go to generic slots
	InventoryComponent->AddItemsToAnySlots_IfServer(HelmetItemInstance, false);
	Item = SlotMapper->GetItem(2);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the third slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);

	// Add another helmet to hand slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, HelmetItemInstance, false);
	Item = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestTrue(TEXT("SlotMapper should reflect the helmet added to the left hand slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);
	
	return Res;
}

bool TestMoveAndSwap(FSlotMapperTest* Test)
{
	SETUP_SLOTMAPPER(10, 20.0f, 9);

	bool Res = true;
    
	// Add initial items to inventory for setup
	FRancItemInstance RockItemInstance(ItemIdRock, 5);
	InventoryComponent->AddItems_IfServer(RockItemInstance);
	FRancItemInstance StickItemInstance(ItemIdSticks, 3);
	InventoryComponent->AddItems_IfServer(StickItemInstance);

	// Move rock from slot 0 to slot 1, where sticks are, and expect them to swap
	SlotMapper->MoveItem(FGameplayTag(), 0, FGameplayTag(), 1);
	FRancItemInstance ItemInSlot0AfterMove = SlotMapper->GetItem(0);
	FRancItemInstance ItemInSlot1AfterMove = SlotMapper->GetItem(1);
	Res &= Test->TestTrue(TEXT("Slot 0 should now contain sticks after swap"), ItemInSlot0AfterMove.ItemId == ItemIdSticks && ItemInSlot0AfterMove.Quantity == 3);
	Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap"), ItemInSlot1AfterMove.ItemId == ItemIdRock && ItemInSlot1AfterMove.Quantity == 5);

	// Add helmet to inventory and attempt to swap with rocks in slot 1
	FRancItemInstance HelmetItemInstance(ItemIdHelmet, 1);
	InventoryComponent->AddItems_IfServer(HelmetItemInstance);
	SlotMapper->MoveItem(FGameplayTag(), 2, FGameplayTag(), 1);
	FRancItemInstance ItemInSlot1AfterHelmetSwap = SlotMapper->GetItem(1);
	FRancItemInstance ItemInSlot2AfterHelmetSwap = SlotMapper->GetItem(2);
	Res &= Test->TestTrue(TEXT("Slot 1 should now contain a helmet after swap"), ItemInSlot1AfterHelmetSwap.ItemId == ItemIdHelmet && ItemInSlot1AfterHelmetSwap.Quantity == 1);
	Res &= Test->TestTrue(TEXT("Slot 2 should now contain rocks after swap"), ItemInSlot2AfterHelmetSwap.ItemId == ItemIdRock && ItemInSlot2AfterHelmetSwap.Quantity == 5);

	// Test moving item 3 sticks from a generic slot to a universal tagged slot (LeftHandSlot) with 3 sticks
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, StickItemInstance, false);
	SlotMapper->MoveItem(FGameplayTag(), 0, LeftHandSlot, -1);
	FRancItemInstance ItemInLeftHandAfterMove = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 5 sticks after move"), ItemInLeftHandAfterMove.ItemId == ItemIdSticks && ItemInLeftHandAfterMove.Quantity == 5);
	// generic slots should contain the remaining 1 sticks
	Res &= Test->TestTrue(TEXT("Slot 0 should contain 1 stick after move"), SlotMapper->GetItem(0).ItemId == ItemIdSticks && SlotMapper->GetItem(0).Quantity == 1);
	
	// Test moving helmet to LeftHandSlot which would swap except Helmet slot can't hold spear so it should fail
    FRancItemInstance SpearItemInstance(ItemIdSpear, 1);
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, SpearItemInstance, true); // replacing the 5 sticks with spear
    SlotMapper->MoveItem(HelmetSlot, -1, LeftHandSlot, -1);
    FRancItemInstance ItemInLeftHandAfterHelmetMove = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    FRancItemInstance HelmetInHelmetSlotAfterMove = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should still  contain spear after failed move"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSpear && ItemInLeftHandAfterHelmetMove.Quantity == 1);
	Res &= Test->TestTrue(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet && HelmetInHelmetSlotAfterMove.Quantity == 1);

    // Attempt to move a non-helmet item from generic slot HelmetSlot, which should fail and no swap occurs
    SlotMapper->MoveItem(FGameplayTag(), 2, HelmetSlot, -1); // Assuming rocks are at slot 2 now
    FRancItemInstance ItemInHelmetSlotAfterInvalidMove = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
    FRancItemInstance ItemInSlot2AfterInvalidMove = SlotMapper->GetItem(2);
    Res &= Test->TestTrue(TEXT("HelmetSlot should not accept non-helmet item, should remain spear"), ItemInHelmetSlotAfterInvalidMove.ItemId == ItemIdSpear && ItemInHelmetSlotAfterInvalidMove.Quantity == 1);
    Res &= Test->TestTrue(TEXT("Slot 2 should remain unchanged after invalid move attempt"), ItemInSlot2AfterInvalidMove.ItemId == ItemIdRock && ItemInSlot2AfterInvalidMove.Quantity == 5);

    // Move item from a universal tagged slot to a specialized tagged slot that accepts it
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, HelmetItemInstance, true); // Override spear with another helmet
    SlotMapper->MoveItem(LeftHandSlot, -1, HelmetSlot, -1);
    FRancItemInstance ItemInHelmetSlotAfterSwapBack = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
    FRancItemInstance ItemInLeftHandAfterSwapBack = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 helmet"), ItemInHelmetSlotAfterSwapBack.ItemId == ItemIdHelmet && ItemInHelmetSlotAfterSwapBack.Quantity == 1);
	Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 1 helmet"), ItemInLeftHandAfterSwapBack.ItemId == ItemIdHelmet && ItemInLeftHandAfterSwapBack.Quantity == 1);
	
    // Test moving item to an already occupied generic slot to ensure they swap
    FRancItemInstance SpearItemInstanceForSwap(ItemIdSpear, 1);
    InventoryComponent->AddItems_IfServer(SpearItemInstanceForSwap); // Add spear to inventory
    SlotMapper->MoveItem(FGameplayTag(), 3, FGameplayTag(), 2); // Assuming spear is at slot 3 now, and rocks at slot 2
    FRancItemInstance ItemInSlot3AfterSwap = SlotMapper->GetItem(3);
    FRancItemInstance ItemInSlot2AfterSwap = SlotMapper->GetItem(2);
    Res &= Test->TestTrue(TEXT("Slot 3 should now contain rocks after swap with spear"), ItemInSlot3AfterSwap.ItemId == ItemIdRock && ItemInSlot3AfterSwap.Quantity == 5);
    Res &= Test->TestTrue(TEXT("Slot 2 should now contain the spear after swap"), ItemInSlot2AfterSwap.ItemId == ItemIdSpear && ItemInSlot2AfterSwap.Quantity == 1);

    return Res;
}

bool TestSplitItems(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(10, 15.0f, 9);

    bool Res = true;
    
    // Add initial items to slots to prepare for split tests
    FRancItemInstance InitialRockInstance(ItemIdRock, 5);
    InventoryComponent->AddItems_IfServer(InitialRockInstance); // Added to first generic slot

    FRancItemInstance InitialHelmetInstance(ItemIdHelmet, 1);
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, InitialHelmetInstance, true); // Added to HelmetSlot

    // Valid split in generic slots
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 0, FGameplayTag::EmptyTag, 1, 2);
    Res &= Test->TestEqual(TEXT("After splitting, first slot should have 3 rocks"), SlotMapper->GetItem(0).Quantity, 3);
    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 2 rocks"), SlotMapper->GetItem(1).Quantity, 2);

    // Invalid split due to insufficient quantity in source slot
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 0, FGameplayTag::EmptyTag, 1, 4); // Trying to split more than available
    Res &= Test->TestEqual(TEXT("Attempt to split more rocks than available should fail"), SlotMapper->GetItem(0).Quantity, 3);

    // Split between a generic slot and a tagged slot
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 1, RightHandSlot, -1, 1); // Splitting 1 rock to RightHandSlot
    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 1 rock"), SlotMapper->GetItem(1).Quantity, 1);
    FRancItemInstance RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot);
    Res &= Test->TestTrue(TEXT("RightHandSlot should now contain 1 rock"), RightHandItem.ItemId == ItemIdRock && RightHandItem.Quantity == 1);

    // Invalid split to a different item type slot
    SlotMapper->SplitItem(RightHandSlot, -1, HelmetSlot, -1, 1); // Trying to split rock into helmet slot
    RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot); // Re-fetch to check for changes
    Res &= Test->TestTrue(TEXT("Attempting to split into a different item type slot should fail"), RightHandItem.Quantity == 1 && SlotMapper->GetItemForTaggedSlot(HelmetSlot).Quantity == 1);

    // Exceeding max stack size
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 8)); // Add more rocks to force a stack size check
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 2, FGameplayTag::EmptyTag, 1, 4); // Attempt to overflow slot 1 with rocks
    Res &= Test->TestTrue(TEXT("Splitting that exceeds max stack size should fail"), SlotMapper->GetItem(1).Quantity < 5); // Assuming max stack size < 6
  // Attempt split from tagged to generic slot with valid quantities
    SlotMapper->SplitItem(LeftHandSlot, -1, FGameplayTag::EmptyTag, 2, 1); // Splitting 1 helmet to a new generic slot
    FRancItemInstance NewGenericSlotItem = SlotMapper->GetItem(2);
    Res &= Test->TestTrue(TEXT("After splitting from tagged to generic, new slot should contain 1 helmet"), NewGenericSlotItem.ItemId == ItemIdHelmet && NewGenericSlotItem.Quantity == 1);

    // Attempt split from generic to tagged slot exceeding max stack size
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 2, HelmetSlot, -1, 1); // Attempt to add another helmet to HelmetSlot
    FRancItemInstance HelmetSlotItem = SlotMapper->GetItemForTaggedSlot(HelmetSlot);
    Res &= Test->TestTrue(TEXT("HelmetSlot should not exceed max stack size of 1"), HelmetSlotItem.Quantity == 1);

    // Split with invalid indices and tags
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 10, FGameplayTag::EmptyTag, 11, 1); // Invalid indices
    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes"), SlotMapper->IsSlotEmpty(10) && SlotMapper->IsSlotEmpty(11));

    SlotMapper->SplitItem(FGameplayTag(), -1, ChestSlot, -1, 1); // Invalid source tag
    Res &= Test->TestTrue(TEXT("Invalid source tag should result in no changes"), SlotMapper->IsTaggedSlotEmpty(ChestSlot));

    // Attempt to split into a slot with a different item type
    FRancItemInstance SpearItemInstance(ItemIdSpear, 1);
    InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, SpearItemInstance, false); // Adding spear to RightHandSlot
    SlotMapper->SplitItem(FGameplayTag::EmptyTag, 0, RightHandSlot, -1, 1); // Attempt to split rock into RightHandSlot containing a spear
    RightHandItem = SlotMapper->GetItemForTaggedSlot(RightHandSlot); // Re-fetch to check for changes
    Res &= Test->TestTrue(TEXT("Attempting to split into a slot with a different item type should fail"), RightHandItem.ItemId == ItemIdSpear && RightHandItem.Quantity == 1);

    return Res;
}

bool TestMoveItemToAnyTaggedSlot(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(10, 15.0f, 9);

    bool Res = true;
    
    // Add a mix of items to test moving to tagged slots
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 3));
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdHelmet, 1));
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdSpear, 2));
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdChestArmor, 1));

    // Move rock to any tagged slot (should go to a universal slot)
    Res &= Test->TestTrue(TEXT("Move rock to any tagged slot"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 0));
    Res &= Test->TestTrue(TEXT("Rock should be in a universal tagged slot"), !SlotMapper->IsTaggedSlotEmpty(LeftHandSlot) || !SlotMapper->IsTaggedSlotEmpty(RightHandSlot));

    // Move helmet to its specialized slot
    Res &= Test->TestTrue(TEXT("Move helmet to its specialized slot"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 1));
    Res &= Test->TestTrue(TEXT("Helmet should be in HelmetSlot"), !SlotMapper->IsTaggedSlotEmpty(HelmetSlot));

    // Move spear to any tagged slot (should go to a universal slot)
    Res &= Test->TestTrue(TEXT("Move spear to any tagged slot"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 2));
    Res &= Test->TestTrue(TEXT("Spear should be in a universal tagged slot"), !SlotMapper->IsTaggedSlotEmpty(LeftHandSlot) || !SlotMapper->IsTaggedSlotEmpty(RightHandSlot));

    // Attempt to move item that is already in its correct tagged slot (helmet), should result in no action
    Res &= Test->TestFalse(TEXT("Attempting to move helmet already in HelmetSlot should do nothing"), SlotMapper->MoveItemToAnyTaggedSlot(HelmetSlot, -1));

    // Move chest armor to its specialized slot
    Res &= Test->TestTrue(TEXT("Move chest armor to its specialized slot"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 3));
    Res &= Test->TestTrue(TEXT("Chest armor should be in ChestSlot"), !SlotMapper->IsTaggedSlotEmpty(ChestSlot));

    // Attempt to move an item to a tagged slot when all suitable slots are occupied
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdRock, 1)); // Add another rock
    Res &= Test->TestFalse(TEXT("Attempt to move extra rock to occupied universal slot should fail"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 4));

    // Check if moving an item from a tagged slot back to inventory respects the item categories for specialized slots
    Res &= Test->TestTrue(TEXT("Move helmet from HelmetSlot back to inventory"), SlotMapper->MoveItemToAnyTaggedSlot(HelmetSlot, -1));
    Res &= Test->TestTrue(TEXT("Helmet should not move to a non-specialized slot"), SlotMapper->IsTaggedSlotEmpty(HelmetSlot) && SlotMapper->GetItem(4).ItemId != ItemIdHelmet);

    // Fill all slots and attempt to move an item to a tagged slot, expecting failure due to full inventory
    for (int i = 5; i < 9; ++i) {
        InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdSticks, 1)); // Fill the remaining slots
    }
    Res &= Test->TestFalse(TEXT("Attempting to move item to any tagged slot with full inventory should fail"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 5));

    // Move item to a specialized slot when it does not match the slot's item categories
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdGiantBoulder, 1)); // Add item not suitable for any specialized slot
    Res &= Test->TestFalse(TEXT("Move Giant Boulder to any tagged slot should fail"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 9));

    // Attempt to move an item to a tagged slot when the source index is invalid
    Res &= Test->TestFalse(TEXT("Attempting to move item from invalid source index should fail"), SlotMapper->MoveItemToAnyTaggedSlot(FGameplayTag::EmptyTag, 100));

    // Attempt to move an item from a tagged slot that is empty
    Res &= Test->TestFalse(TEXT("Attempting to move item from an empty tagged slot should fail"), SlotMapper->MoveItemToAnyTaggedSlot(ChestSlot, -1));

    // Verify that attempting to move items does not alter the quantity or existence of items not involved in the move
    Res &= Test->TestEqual(TEXT("Quantity of items not involved in move should not change"), SlotMapper->GetItemForTaggedSlot(RightHandSlot).Quantity, 2);

    return Res;
}

bool TestSlotReceiveItem(FSlotMapperTest* Test)
{
    SETUP_SLOTMAPPER(10, 20.0f, 5); // Setup with max weight capacity of 50

    bool Res = true;
    
    // Adding item to an empty slot
    FRancItemInstance RockInstance(ItemIdRock, 3);
    Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), SlotMapper->CanSlotReceiveItem(RockInstance, 0));

	// Adding more of that item up to maxstack for the same slot
	FRancItemInstance RockInstance2(ItemIdRock, 2);
	Res &= Test->TestTrue(TEXT("Can add more rocks to slot with same item type"), SlotMapper->CanSlotReceiveItem(RockInstance2, 0));

    // Trying to add item to a slot with a different item type
    FRancItemInstance HelmetInstance(ItemIdHelmet, 1);
    Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), SlotMapper->CanSlotReceiveItem(HelmetInstance, 0));

	// Add helmet to a different slot
	Res &= Test->TestTrue(TEXT("Can add helmet to a different slot"), SlotMapper->CanSlotReceiveItem(HelmetInstance, 1));
	
    // SAdding item exceeding max stack size
    FRancItemInstance ExcessiveRockInstance(ItemIdRock, 5); 
    Res &= Test->TestFalse(TEXT("Cannot add rocks exceeding max stack size"), SlotMapper->CanSlotReceiveItem(ExcessiveRockInstance, 0));

    // Adding item to an out-of-bounds slot
    Res &= Test->TestFalse(TEXT("Cannot add item to an out-of-bounds slot"), SlotMapper->CanSlotReceiveItem(RockInstance, 10));

    // Weight based test
    FRancItemInstance GiantBoulderInstance(ItemIdGiantBoulder, 1); // Weight is 10
    Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), SlotMapper->CanSlotReceiveItem(GiantBoulderInstance, 1));

    return Res;
}
	
bool FSlotMapperTest::RunTest(const FString& Parameters)
{
	bool Res = true;
	Res &= TestInitializeSlotMapper(this);
	Res &= TestAddItemsToSlotMapper(this);
	Res &= TestMoveAndSwap(this);
	Res &= TestSplitItems(this);
	Res &= TestMoveItemToAnyTaggedSlot (this);
	Res &= TestSlotReceiveItem (this);
	// TestDropItem 
    
	return Res;
}