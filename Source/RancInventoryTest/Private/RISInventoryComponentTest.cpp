// Copyright Rancorous Games, 2024

#include "RISInventoryComponentTest.h"
#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/InventoryComponent.h"
#include "Data/RecipeData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, "GameTests.RIS.RancInventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define SETUP_RISINVENTORY(CarryCapacity) \
UInventoryComponent* InventoryComponent = NewObject<UInventoryComponent>(); \
InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot); \
InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot); \
InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot); \
InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot); \
InventoryComponent->MaxContainerSlotCount = 9; \
InventoryComponent->MaxWeight = CarryCapacity; \
InitializeTestItems();

FRancInventoryComponentTest* RITest;
// TODO RECOVERY:
/*
bool TestAddingTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Ensure the left hand slot is initially empty
	Res &= RITest->TestTrue(TEXT("No item should be in the left hand slot before addition"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

	// Add an unstackable item to the left hand slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, false);
	Res &= RITest->TestTrue(
		TEXT("Unstackable Item should be in the left hand slot after addition"),
		InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to add another unstackable item to the same slot without override - should fail
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, false);
	Res &= RITest->TestTrue(
		TEXT("Second unstackable item should not replace the first one without override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 1);

	// Attempt to add another unstackable item to the same slot with override - should succeed
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, true);
	Res &= RITest->TestEqual(
		TEXT("Second unstackable item should replace the first one with override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity, 1);

	// Test adding to a specialized slot that should only accept specific items
	// Assuming HelmetSlot only accepts items with HelmetTag
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneSpear, true);
	Res &= RITest->TestTrue(TEXT("Non-helmet item should not be added to the helmet slot"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Test adding a correct item to a specialized slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true);
	Res &= RITest->TestTrue(
		TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));

	// Test adding a stackable item to an empty slot and then adding a different stackable item to the same slot without override
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeRocks, false); // Assuming this is reset from previous tests
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FItemBundle(ItemIdSticks, 2), false);
	Res &= RITest->TestFalse(
		TEXT("Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock) without override"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSticks));

	// Test adding an item to a slot that is not designated as either universal or specialized (invalid slot)
	InventoryComponent->AddItemsToTaggedSlot_IfServer(FGameplayTag::EmptyTag, OneRock, false);
	Res &= RITest->TestFalse(TEXT("Item should not be added to an invalid slot"), InventoryComponent->GetItemForTaggedSlot(FGameplayTag::EmptyTag).IsValid());

	// Test adding a stackable item to the max stack size and then attempting to add more with override, which should return 0
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FiveRocks, true); // Reset to max stack size
	int32 AmountAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeRocks, false);
	Res &= RITest->TestEqual(TEXT("Stackable Item (Rock) amount added should be none as already full stack"), AmountAdded, 0);

	// Do it again but with override true
	AmountAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeRocks, true);
	Res &= RITest->TestEqual(TEXT("Stackable Item (Rock) amount added should be 3 with override"), AmountAdded, 3);

	// Test adding a stackable item to a slot that has a different stackable item with override enabled
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FItemBundle(ItemIdSticks, 4), true); // Assuming different item than before
	Res &= RITest->TestTrue(
		TEXT("Different stackable item (Sticks) should replace existing item (Rock) in slot with override"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSticks) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle
		.Quantity == 4);

	return Res;
}

bool TestRemovingTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Add stackable item to a slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeRocks, false);

	// Remove a portion of the stackable item
	int32 RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, true);
	Res &= RITest->TestTrue(TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should have 1 Rock remaining after partial removal"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 1);

	// Attempt to remove more items than are present without allowing partial removals
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, false);
	Res &= RITest->TestTrue(TEXT("Should not remove any items if attempting to remove more than present without allowing partial removal"), RemovedQuantity == 0);

	// Add an unstackable item to a slot and then remove it
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true);
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(HelmetSlot, 1, true);
	Res &= RITest->TestTrue(TEXT("Should successfully remove unstackable item (Helmet)"), RemovedQuantity == 1);
	Res &= RITest->TestFalse(TEXT("Helmet slot should be empty after removing the item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Attempt to remove an item from an empty slot
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, true);
	Res &= RITest->TestTrue(TEXT("Should not remove any items from an empty slot"), RemovedQuantity == 0);

	// Attempt to remove an item from a non-existent slot
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag::EmptyTag, 1, true);
	Res &= RITest->TestTrue(TEXT("Should not remove any items from a non-existent slot"), RemovedQuantity == 0);

	return Res;
}

bool TestMoveTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Add item to a tagged slot directly
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true);
	Res &= RITest->TestTrue(
		TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));
	Res &= RITest->TestTrue(TEXT("Container should be empty"), InventoryComponent->GetAllContainerItems().Num() == 0);

	// Move item from tagged slot to generic inventory (cannot directly verify generic inventory, so just ensure removal)
	int32 MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from the tagged slot to generic inventory"), MovedQuantity, 1);
	Res &= RITest->TestTrue(TEXT("Helmet slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain the helmet item"), InventoryComponent->GetContainerItemCount(ItemIdHelmet) == 1);
	// generic inventory now has the helmet item

	// Try to move item to invalid (chest) slot
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, FGameplayTag::EmptyTag, ChestSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item to the chest slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Chest slot should not contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

	// Move item back to a different tagged slot from generic inventory
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from generic inventory to right hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));

	// Move item from one hand to the other
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from right hand slot to left hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to move an item that doesn't exist in the source tagged slot
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneRock, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move an item that doesn't exist in the source tagged slot"), MovedQuantity, 0);

	// Status: Inventory contains 1 item, helmet in left hand slot

	// Add an item compatible with RightHandSlot but not with HelmetSlot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, OneSpear, true);
	Res &= RITest->TestTrue(
		TEXT("Spear item should be added to the right hand slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));

	// Attempt to move the Spear (Weapon) to HelmetSlot (Armor) directly, should fail
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneSpear, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Helmet slot should still be empty"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Try to Move a (non existent) Spear back to an empty RightHandSlot from HelmetSlot
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneSpear, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from right hand slot to helmet slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));

	// Try again but other way which should also fail because helmet slot is empty AND spear is incompatible
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, HelmetSlot, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item from left hand slot to right hand slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));
	
	// Status: Helmet in left hand, Spear in right hand

	// attempt to swap the items in the hands
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, LeftHandSlot, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should Swap the two items"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should now contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));

	// move helmet from right hand to helmet slot
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from right hand slot to helmet slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Helmet slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemBundle.ItemId.MatchesTag(ItemIdHelmet));

	// Now try moving spear from left hand to helmet slot which should fail
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneSpear, LeftHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from left hand slot to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));

	// Try again the other direction, which should also fail because the spear would get swapped to helmet slot 
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneHelmet, HelmetSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item from helmet slot to left hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));
	
	// Remove spear, Attempt to move items from an empty or insufficient source slot
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, true);
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneSpear, LeftHandSlot); // Assuming RightHandSlot is empty
	Res &= RITest->TestEqual(TEXT("Should not move items from an empty or insufficient source slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("No items in generic inventory"), InventoryComponent->GetAllContainerItems().Num() == 0);

	// Reset
	InventoryComponent->ClearInventory_IfServer();
	Res &= RITest->TestTrue(TEXT("Left hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Right hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

	// Test moving stackable rocks
	InventoryComponent->AddItems_IfServer(FItemBundle(ItemIdRock, 8));
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks to right hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 5 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 5);
	// Try to move remaining 5, expecting 2 to be moved
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(FItemBundle(ItemIdRock, 5), FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 3);

	// Try again and verify none are moved
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move any rocks to right hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 3);

	// Move to other hand in several steps
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(TwoRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks from right hand slot to left hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 3);

	// Decided to allow this for now // Attempt to move more than exists to verify that it fails
	// MovedQuantity = InventoryComponent->MoveItems_ServerImpl(FRancItemBundle(ItemIdRock, 5), RightHandSlot, LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot shou<ld still contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 2);

	// Now move the remaining 3 rocks to the left hand slot
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(ThreeRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Right hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

	// Now we test the same kind of rock moving but to and then from generic inventory
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(ThreeRocks, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from left hand slot to generic inventory"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should now hold 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 6);

	// Decided to allow this for now // Move more than exists
	// MovedQuantity = InventoryComponent->MoveItems_ServerImpl(FRancItemBundle(ItemIdRock, 5), LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks from left hand slot to generic inventory"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot should still hold 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 2);
	// Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 6);

	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(TwoRocks, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks from left hand slot to generic inventory"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Left hand slot should now be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 8 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 8);

	// move more than exists
	// MovedQuantity = InventoryComponent->MoveItems_ServerImpl(FRancItemBundle(ItemIdRock, 10), FGameplayTag::EmptyTag, LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks to left hand slot"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot should still be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	// Res &= RITest->TestTrue(TEXT("Generic inventory should contain 8 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 8);

	// Move back to right hand
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 6);

	// Try moving just 1 more rock to Right hand
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneRock, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 1 rock to right hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 5 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 5);

	// move 2 more to get full stack
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerItemCount(ItemIdRock) == 3);

	// remove two rocks from right hand, leaving three, then add a spear to left hand
	// Then we try to swap the hand contents but with only 1 rock, which is invalid as it would leave 2 rocks behind making the swap impossible
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, true);
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneSpear, true);
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(OneRock, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain the spear"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdSpear));

	// Now move all 3 rocks expecting a swap
	MovedQuantity = InventoryComponent->MoveItems_ServerImpl(ThreeRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemBundle.Quantity == 3);


	return Res;
}

bool TestDroppingFromTaggedSlot()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Step 1: Add an item to a tagged slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeRocks, true);
	Res &= RITest->TestTrue(
		TEXT("Rocks should be added to the right hand slot"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemBundle.
		Quantity == 3);

	// Step 2: Drop a portion of the stackable item from the tagged slot
	int32 DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 2, 0.0f);
	Res &= RITest->TestEqual(TEXT("Should set to drop a portion of the stackable item (2 Rocks)"), DroppedQuantity, 2);

	// Step 3: Attempt to drop more items than are present in the tagged slot
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 5, 0.0f); // Trying to drop 5 rocks, but only 1 remains
	Res &= RITest->TestEqual(TEXT("Should set to drop the remaining quantity of the item (1 Rock)"), DroppedQuantity, 1);

	// Step 4: Attempt to drop an item from an empty tagged slot
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(LeftHandSlot, 1, 0.0f); // Assuming LeftHandSlot is empty
	Res &= RITest->TestEqual(TEXT("Should not drop any items from an empty tagged slot"), DroppedQuantity, 0);

	// Step 5: Attempt to drop items from a tagged slot with a non-stackable item
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot, OneHelmet, true); // Add a non-stackable item
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(HelmetSlot, 1, 0.0f);
	Res &= RITest->TestEqual(TEXT("Should set to drop the non-stackable item (Helmet)"), DroppedQuantity, 1);

	return Res;
}

bool TestCanCraftRecipe()
{
	SETUP_RISINVENTORY(100); // Setup with a weight capacity for the test

	bool Res = true;

	// Create a recipe for crafting
	UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
	TestRecipe->Components.Add(TwoRocks); // Requires 2 Rocks
	TestRecipe->Components.Add(ThreeSticks); // Requires 3 Sticks

	// Step 1: Inventory has all required components in the correct quantities
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, TwoRocks, true);
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, ThreeSticks, true);
	Res &= RITest->TestTrue(TEXT("CanCraftRecipe should return true when all components are present in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 2: Inventory is missing one component
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 3, true); // Remove Sticks
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when a component is missing"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 3: Inventory has insufficient quantity of one component
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, FItemBundle(ItemIdSticks, 1), true); // Add only 1 Stick
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when components are present but in insufficient quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 4: Crafting with an empty or null recipe reference
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when the recipe is null"), InventoryComponent->CanCraftRecipe(nullptr));

	// Step 5: Clear tagged slots before adding new test scenarios
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, true); // Clear Rocks
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, true); // Clear Sticks

	// Step 6: Inventory has all required components in the generic inventory

	InventoryComponent->AddItems_IfServer(TwoRocks);
	InventoryComponent->AddItems_IfServer(ThreeSticks);
	Res &= RITest->TestTrue(
		TEXT("CanCraftRecipe should return true when all components are present in generic inventory in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 7: Generic inventory has insufficient quantity of one component
	// First, simulate removing items from generic inventory by moving them to a tagged slot and then removing
	InventoryComponent->MoveItems_ServerImpl(OneRock, FGameplayTag::EmptyTag, RightHandSlot); // Simulate removing 1 Rock from generic inventory
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, true); // Actually remove the moved item
	Res &= RITest->TestFalse(
		TEXT("CanCraftRecipe should return false when components in generic inventory are present but in insufficient quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	return Res;
}

bool TestCraftRecipe()
{
	SETUP_RISINVENTORY(100); // Setup with a sufficient weight capacity for the test

	bool Res = true;

	// Create a test recipe
	UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
	TestRecipe->ResultingObject = UObject::StaticClass(); // Assuming UMyCraftedObject is a valid class
	TestRecipe->QuantityCreated = 1;
	TestRecipe->Components.Add(TwoRocks); // Requires 2 Rocks
	TestRecipe->Components.Add(ThreeSticks); // Requires 3 Sticks

	// Step 1: Crafting success
	InventoryComponent->AddItems_IfServer(FiveRocks);
	InventoryComponent->AddItems_IfServer(ThreeSticks);
	Res &= RITest->TestTrue(TEXT("CraftRecipe_IfServer should return true when all components are present"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Would be nice to test if we could confirm OnCraftConfirmed gets called but haven't found a nice way

	// Check that correct quantity of components were removed
	Res &= RITest->TestEqual(
		TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdRock), 3);
	Res &= RITest->TestEqual(
		TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdSticks), 0);

	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, true); // Clear Rocks from RightHandSlot

	// Step 2: Crafting failure due to insufficient components
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when a component is missing"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));

	// Step 4: Crafting with a null recipe
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when the recipe is null"), InventoryComponent->CraftRecipe_IfServer(nullptr));


	// Step 5: Crafting success with components spread between generic inventory and tagged slots
	InventoryComponent->AddItems_IfServer(OneRock); // Add 1 Rock to generic inventory
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneRock, true); // Add another Rock to a tagged slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, ThreeSticks, true); // Add 3 Sticks to a tagged slot
	Res &= RITest->TestTrue(
		TEXT("CraftRecipe_IfServer should return true when components are spread between generic and tagged slots"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Assuming a way to verify the resulting object was created successfully

	// Step 6: Reset environment for next test
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, true); // Clear Rocks from LeftHandSlot
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, true); // Clear Sticks from HelmetSlot

	// Step 7: Crafting failure when tagged slots contain all necessary components but in insufficient quantities
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneRock, true); // Add only 1 Rock to a tagged slot, insufficient for the recipe
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FItemBundle(ItemIdSticks, 2), true); // Add only 2 Sticks to a tagged slot, insufficient for the recipe
	Res &= RITest->TestFalse(
		TEXT("CraftRecipe_IfServer should return false when not all components are present in sufficient quantities"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));

	return Res;
}

bool TestInventoryMaxCapacity()
{
	SETUP_RISINVENTORY(5); // Setup with a weight capacity of 5

	bool Res = true;

	// Step 1: Adding Stackable Items to Generic Slots
	InventoryComponent->AddItems_IfServer(ThreeRocks);
	Res &= RITest->TestEqual(TEXT("Should successfully add rocks within capacity"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdRock), 3);
	InventoryComponent->AddItems_IfServer(ThreeSticks); // Trying to add more rocks, total weight would be 6 but capacity is 5
	Res &= RITest->TestEqual(TEXT("Should fail to add sticks beyond capacity"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdSticks), 0);

	// Step 2: Adding Unstackable Items to Tagged Slots
	int32 QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, OneHelmet, true); // Weight = 2
	Res &= RITest->TestEqual(TEXT("Should successfully add a helmet within capacity"), QuantityAdded, 1);
	QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, OneHelmet, true);
	// Trying to add another helmet, total weight would be 4
	Res &= RITest->TestEqual(TEXT("Should fail to add a second helmet beyond capacity"), QuantityAdded, 0);

	// Step 3: Adding Stackable items
	InventoryComponent->RemoveItemsFromAnyTaggedSlots_IfServer(ItemIdHelmet, 1); // Reset tagged slot
	QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FItemBundle(ItemIdSticks, 5)); // Try Adding 5 sticks, which should fail
	Res &= RITest->TestEqual(TEXT("AddItemsToTaggedSlot_IfServer does not do partial adding and weight exceeds capacity"), QuantityAdded, 0);
	QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, TwoRocks);
	Res &= RITest->TestEqual(TEXT("Should successfully add 2 rocks within capacity"), QuantityAdded, 2);

	UItemRecipeData* BoulderRecipe = NewObject<UItemRecipeData>();
	BoulderRecipe->ResultingItemId = ItemIdGiantBoulder; // a boulder weighs 10
	BoulderRecipe->QuantityCreated = 1;
	BoulderRecipe->Components.Add(FItemBundle(ItemIdRock, 5)); // Requires 2 Rocks

	// Step 4: Crafting Items That Exceed Capacity
	bool CraftSuccess = InventoryComponent->CraftRecipe_IfServer(BoulderRecipe);
	Res &= RITest->TestTrue(TEXT("Crafting should succeed"), CraftSuccess);
	// Check if the crafted helmets are in inventory
	Res &= RITest->TestEqual(TEXT("Crafted boulder should not be in inventory"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdGiantBoulder), 0);

	return Res;
}

bool TestAddItemToAnySlots()
{
	SETUP_RISINVENTORY(20);
	InventoryComponent->MaxContainerSlotCount = 2;

	bool Res = true;
	// Create item instances with specified quantities and weights
	FItemBundle RockInstance(ItemIdRock, 5);
	FItemBundle StickInstance(ItemIdSticks, 2);

	// PreferTaggedSlots = true, adding items directly to tagged slots first
	int32 Added = InventoryComponent->AddItemsToAnySlots_IfServer(RockInstance, true);
	Res &= RITest->TestEqual(TEXT("Should add rocks to right hand slot"), Added, 5); // weight 5

	// remove from right hand slot
	InventoryComponent->RemoveItemsFromAnyTaggedSlots_IfServer(ItemIdRock, 5); // weight 0
	Res &= RITest->TestFalse(TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	
	// PreferTaggedSlots = false, adding items to generic slots first
	Added = InventoryComponent->AddItemsToAnySlots_IfServer(RockInstance, false);
	Res &= RITest->TestEqual(TEXT("Should add all rocks"), Added, 5); // weight 5
	Res &= RITest->TestFalse(TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	Res &= RITest->TestFalse(TEXT("Left hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	
	// Exceeding generic slot count, items should spill over to tagged slots if available
	InventoryComponent->AddItemsToAnySlots_IfServer(RockInstance, false); // take up last slot
	Added = InventoryComponent->AddItemsToAnySlots_IfServer(StickInstance, false); // weight 12
	Res &= RITest->TestEqual(TEXT("Should add sticks to the first universal tagged slot after generic slots are full"), Added, 2);
	Res &= RITest->TestEqual(TEXT("First universal tagged slot (left hand) should contain sticks"), InventoryComponent->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0]).ItemBundle.Quantity, 2);

	// Weight limit almost reached, no heavy items should be added despite right hand being available
	FItemBundle HeavyItem(ItemIdGiantBoulder, 1); // Weight 22 exceeding capacity
	Added = InventoryComponent->AddItemsToAnySlots_IfServer(HeavyItem, true);
	Res &= RITest->TestEqual(TEXT("Should not add heavy items beyond weight capacity"), Added, 0);
	
	InventoryComponent->MoveItems_ServerImpl(RockInstance, FGameplayTag::EmptyTag, RightHandSlot);
	
	// Adding items back to generic slots if there's still capacity after attempting tagged slots
	InventoryComponent->MaxWeight = 25; // Increase weight capacity for this test
	Added = InventoryComponent->AddItemsToAnySlots_IfServer(HeavyItem, true);
	Res &= RITest->TestEqual(TEXT("Should add heavy items to generic slots after trying tagged slots"), Added, 1);

	return Res;
}*/

bool FRancInventoryComponentTest::RunTest(const FString& Parameters)
{
	RITest = this;
	bool Res = true;
	/*Res &= TestAddingTaggedSlotItems();
	Res &= TestRemovingTaggedSlotItems();
	Res &= TestMoveTaggedSlotItems();
	Res &= TestDroppingFromTaggedSlot();
	Res &= TestCanCraftRecipe();
	Res &= TestCraftRecipe();
	Res &= TestInventoryMaxCapacity();
	Res &= TestAddItemToAnySlots();*/

	return Res;
}
