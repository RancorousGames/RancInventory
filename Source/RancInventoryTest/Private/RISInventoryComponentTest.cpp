// Copyright Rancorous Games, 2024

#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/InventoryComponent.h"
#include "Data/RecipeData.h"

#define TestName "GameTests.RIS.RancInventory"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define SETUP_RISINVENTORY(CarryCapacity) \
URISSubsystem* Subsystem = FindSubsystem(TestName); \
UWorld* World = FindWorld(nullptr, TestName); \
AActor* TempActor = World->SpawnActor<AActor>(); \
UInventoryComponent* InventoryComponent = NewObject<UInventoryComponent>(TempActor); \
TempActor->AddInstanceComponent(InventoryComponent); \
InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot); \
InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot); \
InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot); \
InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot); \
InventoryComponent->MaxContainerSlotCount = 9; \
InventoryComponent->MaxWeight = CarryCapacity; \
InventoryComponent->RegisterComponent(); \
InventoryComponent->InitializeComponent(); \
InitializeTestItems(TestName);

FRancInventoryComponentTest* RITest;

bool TestAddingTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);
	bool Res = true;

	// Ensure the left hand slot is initially empty
	Res &= RITest->TestTrue(TEXT("No item should be in the left hand slot before addition"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

	// Add an unstackable item to the left hand slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, false);
	Res &= RITest->TestTrue(
		TEXT("Unstackable Item should be in the left hand slot after addition"),
		InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to add another unstackable item to the same slot - should fail
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true);
	Res &= RITest->TestTrue(
		TEXT("Second unstackable item should not replace the first one"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 1);

	// Test adding to a specialized slot that should only accept specific items
	// Assuming HelmetSlot only accepts items with HelmetTag
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneSpear, true);
	Res &= RITest->TestTrue(TEXT("Non-helmet item should not be added to the helmet slot"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Test adding a correct item to a specialized slot
	// NOTE This test sometimes FAILS when recompiling
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
	Res &= RITest->TestTrue(
		TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

	// Test adding a stackable item to an empty slot and then adding a different stackable item to the same slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, false);
	Res &= RITest->TestFalse(
		TEXT("Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock)"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks));

	// Test adding an item to a slot that is not designated as either universal or specialized (invalid slot)
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, FGameplayTag::EmptyTag, OneRock, false);
	Res &= RITest->TestFalse(TEXT("Item should not be added to an invalid slot"), InventoryComponent->GetItemForTaggedSlot(FGameplayTag::EmptyTag).IsValid());

	// Test adding a stackable item to the max stack size and then attempting to add more with allowpartial, which should return 0
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, FiveRocks, true);
	int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
	Res &= RITest->TestEqual(TEXT("Stackable Item (Rock) amount added should be none as already full stack"), AmountAdded, 0);
	Res &= RITest->TestEqual(TEXT("Right hand slot should have 5 Rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity, 5);

	// Set right hand to have 1 rock
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 4, EItemChangeReason::Removed, true);
	
	// Test adding a stackable item to a slot that has a different stackable item with partial true
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 4, true); // Assuming different item than before
	// Should not have added
	Res &= RITest->TestFalse(TEXT("Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock) without override"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks));
	Res &= RITest->TestEqual(TEXT("Right hand slot should have 1 Rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity, 1);

	return Res;
}

bool TestRemovingTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Add stackable item to a slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);

	// Remove a portion of the stackable item
	int32 RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, EItemChangeReason::Removed, true);
	Res &= RITest->TestTrue(TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should have 1 Rock remaining after partial removal"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 1);

	// Attempt to remove more items than are present without allowing partial removals
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, EItemChangeReason::Removed,false);
	Res &= RITest->TestTrue(TEXT("Should not remove any items if attempting to remove more than present without allowing partial removal"), RemovedQuantity == 0);

	// Add an unstackable item to a slot and then remove it
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(HelmetSlot, 1, EItemChangeReason::Removed,true);
	Res &= RITest->TestTrue(TEXT("Should successfully remove unstackable item (Helmet)"), RemovedQuantity == 1);
	Res &= RITest->TestFalse(TEXT("Helmet slot should be empty after removing the item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Attempt to remove an item from an empty slot
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, EItemChangeReason::Removed,true);
	Res &= RITest->TestTrue(TEXT("Should not remove any items from an empty slot"), RemovedQuantity == 0);

	// Attempt to remove an item from a non-existent slot
	RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(FGameplayTag::EmptyTag, 1,EItemChangeReason::Removed, true);
	Res &= RITest->TestTrue(TEXT("Should not remove any items from a non-existent slot"), RemovedQuantity == 0);

	return Res;
}

bool TestMoveTaggedSlotItems()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Add item to a tagged slot directly
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
	Res &= RITest->TestTrue(
		TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));
	Res &= RITest->TestTrue(TEXT("Container should be empty"), InventoryComponent->GetContainedQuantity(HelmetSlot) == 0);

	// Move item from tagged slot to generic inventory (cannot directly verify generic inventory, so just ensure removal)
	int32 MovedQuantity = InventoryComponent->MoveItem(OneHelmet, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from the tagged slot to generic inventory"), MovedQuantity, 1);
	Res &= RITest->TestTrue(TEXT("Helmet slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain the helmet item"), InventoryComponent->GetContainedQuantity(ItemIdHelmet) == 1);
	// generic inventory now has the helmet item

	// Try to move item to invalid (chest) slot
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FGameplayTag::EmptyTag, ChestSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item to the chest slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Chest slot should not contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

	// Move item back to a different tagged slot from generic inventory
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from generic inventory to right hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));

	// Move item from one hand to the other
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from right hand slot to left hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to move an item that doesn't exist in the source tagged slot
	MovedQuantity = InventoryComponent->MoveItem(OneRock, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move an item that doesn't exist in the source tagged slot"), MovedQuantity, 0);

	// Status: Inventory contains 1 item, helmet in left hand slot

	// Add an item compatible with RightHandSlot but not with HelmetSlot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear, true);
	Res &= RITest->TestTrue(
		TEXT("Spear item should be added to the right hand slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

	// Attempt to move the Spear (Weapon) to HelmetSlot (Armor) directly, should fail
	MovedQuantity = InventoryComponent->MoveItem(OneSpear, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Helmet slot should still be empty"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

	// Try to Move a (non existent) Spear back to an empty RightHandSlot from HelmetSlot
	MovedQuantity = InventoryComponent->MoveItem(OneSpear, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from right hand slot to helmet slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

	// Try again but other way which should also fail because helmet slot is empty AND spear is incompatible
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, HelmetSlot, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item from left hand slot to right hand slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
	
	// Status: Helmet in left hand, Spear in right hand

	// attempt to swap the items in the hands
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, LeftHandSlot, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should Swap the two items"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Right hand slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should now contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSpear));

	// move helmet from right hand to helmet slot
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, RightHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should move the helmet item from right hand slot to helmet slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(
		TEXT("Helmet slot should now contain the helmet item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

	// Now try moving spear from left hand to helmet slot which should fail
	MovedQuantity = InventoryComponent->MoveItem(OneSpear, LeftHandSlot, HelmetSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from left hand slot to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSpear));

	// Try again the other direction, which should also fail because the spear would get swapped to helmet slot 
	MovedQuantity = InventoryComponent->MoveItem(OneHelmet, HelmetSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move the helmet item from helmet slot to left hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(
		TEXT("Left hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSpear));
	
	// Remove spear, Attempt to move items from an empty or insufficient source slot
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1,EItemChangeReason::Removed, true);
	MovedQuantity = InventoryComponent->MoveItem(OneSpear, LeftHandSlot); // Assuming RightHandSlot is empty
	Res &= RITest->TestEqual(TEXT("Should not move items from an empty or insufficient source slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("No items in generic inventory"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear)	== 0);

	// Reset
	InventoryComponent->Clear_IfServer();
	Res &= RITest->TestTrue(TEXT("Left hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Right hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

	// Test moving stackable rocks
	InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 8);
	MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks to right hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 5 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);
	// Try to move remaining 5, expecting 2 to be moved
	MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

	// Try again and verify none are moved
	MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move any rocks to right hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

	// Move to other hand in several steps
	MovedQuantity = InventoryComponent->MoveItem(TwoRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks from right hand slot to left hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);

	// Decided to allow this for now // Attempt to move more than exists to verify that it fails
	// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 5), RightHandSlot, LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot shou<ld still contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);

	// Now move the remaining 3 rocks to the left hand slot
	MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Right hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

	// Now we test the same kind of rock moving but to and then from generic inventory
	MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from left hand slot to generic inventory"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should now hold 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 6);

	// Decided to allow this for now // Move more than exists
	// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 5), LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks from left hand slot to generic inventory"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot should still hold 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
	// Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainedQuantity(ItemIdRock) == 6);

	MovedQuantity = InventoryComponent->MoveItem(TwoRocks, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks from left hand slot to generic inventory"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Left hand slot should now be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 8 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 8);

	// move more than exists
	// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 10), FGameplayTag::EmptyTag, LeftHandSlot);
	// Res &= RITest->TestEqual(TEXT("Should not move any rocks to left hand slot"), MovedQuantity, 0);
	// Res &= RITest->TestTrue(TEXT("Left hand slot should still be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	// Res &= RITest->TestTrue(TEXT("Generic inventory should contain 8 rocks"), InventoryComponent->GetContainedQuantity(ItemIdRock) == 8);

	// Move back to right hand
	MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 2);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 6);

	// Try moving just 1 more rock to Right hand
	MovedQuantity = InventoryComponent->MoveItem(OneRock, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 1 rock to right hand slot"), MovedQuantity, 1);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 5 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);

	// move 2 more to get full stack
	MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
	Res &= RITest->TestTrue(TEXT("Right hand slot should contain 5 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
	Res &= RITest->TestTrue(TEXT("Generic inventory should contain 3 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

	// remove two rocks from right hand, leaving three, then add a spear to left hand
	// Then we try to swap the hand contents but with only 1 rock, which is invalid as it would leave 2 rocks behind making the swap impossible
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, EItemChangeReason::Removed,true);
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneSpear, true);
	MovedQuantity = InventoryComponent->MoveItem(OneRock, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain the spear"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSpear));

	// Now move all 3 rocks expecting a swap
	MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
	Res &= RITest->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
	Res &= RITest->TestTrue(TEXT("Left hand slot should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 3);


	return Res;
}

bool TestDroppingFromTaggedSlot()
{
	SETUP_RISINVENTORY(100);

	bool Res = true;

	// Step 1: Add an item to a tagged slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
	Res &= RITest->TestTrue(
		TEXT("Rocks should be added to the right hand slot"),
		InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).
		Quantity == 3);

	// Step 2: Drop a portion of the stackable item from the tagged slot
	int32 DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 2, FVector());
	Res &= RITest->TestEqual(TEXT("Should set to drop a portion of the stackable item (2 Rocks)"), DroppedQuantity, 2);

	// Step 3: Attempt to drop more items than are present in the tagged slot
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 5, FVector()); // Trying to drop 5 rocks, but only 1 remains
	Res &= RITest->TestEqual(TEXT("Should set to drop the remaining quantity of the item (1 Rock)"), DroppedQuantity, 1);

	// Step 4: Attempt to drop an item from an empty tagged slot
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(LeftHandSlot, 1, FVector()); // Assuming LeftHandSlot is empty
	Res &= RITest->TestEqual(TEXT("Should not drop any items from an empty tagged slot"), DroppedQuantity, 0);

	// Step 5: Attempt to drop items from a tagged slot with a non-stackable item
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true); // Add a non-stackable item
	DroppedQuantity = InventoryComponent->DropFromTaggedSlot(HelmetSlot, 1, FVector());
	Res &= RITest->TestEqual(TEXT("Should set to drop the non-stackable item (Helmet)"), DroppedQuantity, 1);

	return Res;
}

bool TestCanCraftRecipe()
{
	SETUP_RISINVENTORY(100); // Setup with a weight capacity for the test

	bool Res = true;

	// Create a recipe for crafting
	UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
	TestRecipe->Components.Add(FItemBundle(TwoRocks)); // Requires 2 Rocks
	TestRecipe->Components.Add(FItemBundle(ThreeSticks)); // Requires 3 Sticks

	// Step 1: Inventory has all required components in the correct quantities
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, TwoRocks, true);
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeSticks, true);
	Res &= RITest->TestTrue(TEXT("CanCraftRecipe should return true when all components are present in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 2: Inventory is missing one component
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 3, EItemChangeReason::Removed,true); // Remove Sticks
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when a component is missing"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 3: Inventory has insufficient quantity of one component
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 1, true); // Add only 1 Stick
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when components are present but in insufficient quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 4: Crafting with an empty or null recipe reference
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when the recipe is null"), InventoryComponent->CanCraftRecipe(nullptr));

	// Step 5: Clear tagged slots before adding new test scenarios
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed,true); // Clear Rocks
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, EItemChangeReason::Removed,true); // Clear Sticks

	// Step 6: Inventory has all required components in the generic inventory

	InventoryComponent->AddItem_IfServer(Subsystem, TwoRocks);
	InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
	Res &= RITest->TestTrue(
		TEXT("CanCraftRecipe should return true when all components are present in generic inventory in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 7: Generic inventory has insufficient quantity of one component
	// First, simulate removing items from generic inventory by moving them to a tagged slot and then removing
	InventoryComponent->MoveItem(OneRock, FGameplayTag::EmptyTag, RightHandSlot); // Simulate removing 1 Rock from generic inventory
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Removed,true); // Actually remove the moved item
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
	TestRecipe->Components.Add(FItemBundle(TwoRocks)); // Requires 2 Rocks
	TestRecipe->Components.Add(FItemBundle(ThreeSticks)); // Requires 3 Sticks

	// Step 1: Crafting success
	InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks);
	InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
	Res &= RITest->TestTrue(TEXT("CraftRecipe_IfServer should return true when all components are present"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Would be nice to test if we could confirm OnCraftConfirmed gets called but haven't found a nice way

	// Check that correct quantity of components were removed
	Res &= RITest->TestEqual(
		TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemQuantityTotal(ItemIdRock), 3);
	Res &= RITest->TestEqual(
		TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemQuantityTotal(ItemIdSticks), 0);

	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed,true); // Clear Rocks from RightHandSlot

	// Step 2: Crafting failure due to insufficient components
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when a component is missing"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));

	// Step 4: Crafting with a null recipe
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when the recipe is null"), InventoryComponent->CraftRecipe_IfServer(nullptr));


	// Step 5: Crafting success with components spread between generic inventory and tagged slots
	InventoryComponent->AddItem_IfServer(Subsystem, OneRock); // Add 1 Rock to generic inventory
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true); // Add another Rock to a tagged slot
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeSticks, true); // Add 3 Sticks to a tagged slot
	Res &= RITest->TestTrue(
		TEXT("CraftRecipe_IfServer should return true when components are spread between generic and tagged slots"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Assuming a way to verify the resulting object was created successfully

	// Step 6: Reset environment for next test
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, EItemChangeReason::Removed,true); // Clear Rocks from LeftHandSlot
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed,true); // Clear Sticks from HelmetSlot

	// Step 7: Crafting failure when tagged slots contain all necessary components but in insufficient quantities
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true); // Add only 1 Rock to a tagged slot, insufficient for the recipe
	InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, true); // Add only 2 Sticks to a tagged slot, insufficient for the recipe
	Res &= RITest->TestFalse(
		TEXT("CraftRecipe_IfServer should return false when not all components are present in sufficient quantities"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));

	return Res;
}

bool TestInventoryMaxCapacity()
{
	SETUP_RISINVENTORY(5); // Setup with a weight capacity of 5

	bool Res = true;

	// Step 1: Adding Stackable Items to Generic Slots
	InventoryComponent->AddItem_IfServer(Subsystem, ThreeRocks);
	Res &= RITest->TestEqual(TEXT("Should successfully add rocks within capacity"), InventoryComponent->GetItemQuantityTotal(ItemIdRock), 3);
	InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks); // Trying to add more rocks, total weight would be 6 but capacity is 5
	Res &= RITest->TestEqual(TEXT("Should fail to add sticks beyond capacity"), InventoryComponent->GetItemQuantityTotal(ItemIdSticks), 0);

	// Step 2: Adding Unstackable Items to Tagged Slots
	int32 QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true); // Weight = 2
	Res &= RITest->TestEqual(TEXT("Should successfully add a helmet within capacity"), QuantityAdded, 1);
	QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneHelmet, true);
	// Trying to add another helmet, total weight would be 4
	Res &= RITest->TestEqual(TEXT("Should fail to add a second helmet beyond capacity"), QuantityAdded, 0);

	// Step 3: Adding Stackable items
	InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdHelmet, 1, EItemChangeReason::Removed); // Reset tagged slot
	QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 5, false); // Try Adding 5 sticks, which should fail
	Res &= RITest->TestEqual(TEXT("AddItemToTaggedSlot_IfServer does not do partial adding and weight exceeds capacity"), QuantityAdded, 0);
	QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, TwoRocks);
	Res &= RITest->TestEqual(TEXT("Should successfully add 2 rocks within capacity"), QuantityAdded, 2);

	UItemRecipeData* BoulderRecipe = NewObject<UItemRecipeData>();
	BoulderRecipe->ResultingItemId = ItemIdGiantBoulder; // a boulder weighs 10
	BoulderRecipe->QuantityCreated = 1;
	BoulderRecipe->Components.Add(FItemBundle(ItemIdRock, 5)); // Requires 2 Rocks

	// Step 4: Crafting Items That Exceed Capacity
	bool CraftSuccess = InventoryComponent->CraftRecipe_IfServer(BoulderRecipe);
	// Whether this should succeed or not is up to the game design, but it should not be added to inventory if it exceeds capacity
	// Res &= RITest->TestFalse(TEXT("Crafting should/should not succeed"), CraftSuccess); 
	// Check that the crafted item is not in inventory
	Res &= RITest->TestEqual(TEXT("Crafted boulder should not be in inventory"), InventoryComponent->GetItemQuantityTotal(ItemIdGiantBoulder), 0);

	return Res;
}

bool TestAddItemToAnySlots()
{
	SETUP_RISINVENTORY(20);
	InventoryComponent->MaxContainerSlotCount = 2;

	bool Res = true;
	// Create item instances with specified quantities and weights

	// PreferTaggedSlots = true, adding items directly to tagged slots first
	int32 Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, true);
	Res &= RITest->TestEqual(TEXT("Should add rocks to right hand slot"), Added, 5); // weight 5

	// remove from right hand slot
	InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdRock, 5, EItemChangeReason::Removed); // weight 0
	Res &= RITest->TestFalse(TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	
	// PreferTaggedSlots = false, adding items to generic slots first
	Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, false);
	Res &= RITest->TestEqual(TEXT("Should add all rocks"), Added, 5); // weight 5
	Res &= RITest->TestFalse(TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	Res &= RITest->TestFalse(TEXT("Left hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	
	// Exceeding generic slot count, items should spill over to tagged slots if available
	InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, false); // take up last slot
	Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 2, false); // weight 12
	Res &= RITest->TestEqual(TEXT("Should add sticks to the first universal tagged slot after generic slots are full"), Added, 2);
	Res &= RITest->TestEqual(TEXT("First universal tagged slot (left hand) should contain sticks"), InventoryComponent->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0]).Quantity, 2);

	// Weight limit almost reached, no heavy items should be added despite right hand being available
	// Add a boulder with weight 22 exceeding capacity
	Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdGiantBoulder, 1, true);
	Res &= RITest->TestEqual(TEXT("Should not add heavy items beyond weight capacity"), Added, 0);
	
	InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);
	
	// Adding items back to generic slots if there's still capacity after attempting tagged slots
	InventoryComponent->MaxWeight = 25; // Increase weight capacity for this test
	Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdGiantBoulder, 1, true);
	Res &= RITest->TestEqual(TEXT("Should add heavy items to generic slots after trying tagged slots"), Added, 1);

	return Res;
}

bool FRancInventoryComponentTest::RunTest(const FString& Parameters)
{
	RITest = this;
	bool Res = true;
	Res &= TestAddingTaggedSlotItems();
	Res &= TestRemovingTaggedSlotItems();
	Res &= TestMoveTaggedSlotItems();
	Res &= TestDroppingFromTaggedSlot();
	Res &= TestCanCraftRecipe();
	Res &= TestCraftRecipe();
	Res &= TestInventoryMaxCapacity();
	Res &= TestAddItemToAnySlots();

	return Res;
}
