#include "RancInventoryComponentTest.h"

#include "NativeGameplayTags.h"

#include "Management/RancInventoryData.h"
#include "Components/RancItemContainerComponent.h"
#include "Components/RancInventoryComponent.h"
#include "Engine/AssetManager.h"
#include "Management/RancInventoryFunctions.h"
#include "Misc/AutomationTest.h"

#include "InventorySetup.cpp"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, "GameTests.RancInventoryComponent.Tests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

#define SETUP_RANCINVENTORY(CarryCapacity) \
URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); \
InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot.GetTag()); \
InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot.GetTag()); \
InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot.GetTag()); \
InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot.GetTag()); \
InventoryComponent->MaxNumItemsInContainer = 999; \
InventoryComponent->MaxWeight = CarryCapacity; \
InitializeTestItems();

FRancInventoryComponentTest* RITest;

bool TestAddingTaggedSlotItems()
{
    SETUP_RANCINVENTORY(100);

    bool Res = true;
    
    // Ensure the left hand slot is initially empty
    Res &= RITest->TestTrue(TEXT("No item should be in the left hand slot before addition"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).IsValid());
    
    // Add an unstackable item to the left hand slot
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), false);
    Res &= RITest->TestTrue(TEXT("Unstackable Item should be in the left hand slot after addition"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

    // Attempt to add another unstackable item to the same slot without override - should fail
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), false);
    Res &= RITest->TestTrue(TEXT("Second unstackable item should not replace the first one without override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.Quantity == 1);
    
    // Attempt to add another unstackable item to the same slot with override - should succeed
    InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    Res &= RITest->TestEqual(TEXT("Second unstackable item should replace the first one with override"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot.GetTag()).ItemInstance.Quantity, 1);

    // Test adding to a specialized slot that should only accept specific items
    // Assuming HelmetSlot only accepts items with HelmetTag
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdSpear.GetTag(), 1), true);
    Res &= RITest->TestTrue(TEXT("Non-helmet item should not be added to the helmet slot"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).IsValid());

    // Test adding a correct item to a specialized slot
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
    Res &= RITest->TestTrue(TEXT("Helmet item should be added to the helmet slot"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));
	
	// Test adding a stackable item to an empty slot and then adding a different stackable item to the same slot without override
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), false); // Assuming this is reset from previous tests
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 2), false);
	Res &= RITest->TestFalse(TEXT("Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock) without override"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSticks));

	// Test adding an item to a slot that is not designated as either universal or specialized (invalid slot)
	InventoryComponent->AddItemsToTaggedSlot_IfServer(FGameplayTag::EmptyTag, FRancItemInstance(ItemIdRock.GetTag(), 1), false);
	Res &= RITest->TestFalse(TEXT("Item should not be added to an invalid slot"), InventoryComponent->GetItemForTaggedSlot(FGameplayTag::EmptyTag).IsValid());

	// Test adding a stackable item to the max stack size and then attempting to add more with override, which should return 0
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 5), true); // Reset to max stack size
	int32 AmountAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), true); // Override with less than max stack size
	Res &= RITest->TestEqual(TEXT("Stackable Item (Rock) amount added should be none as already full stack"), AmountAdded, 0);

	// Test adding a stackable item to a slot that has a different stackable item with override enabled
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 4), true); // Assuming different item than before
	Res &= RITest->TestTrue(TEXT("Different stackable item (Sticks) should replace existing item (Rock) in slot with override"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSticks) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.Quantity == 4);
	
    return Res;
}

bool TestRemovingTaggedSlotItems()
{
    SETUP_RANCINVENTORY(100);

    bool Res = true;

    // Add stackable item to a slot
    InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), false);

    // Remove a portion of the stackable item
    int32 RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 2, true);
    Res &= RITest->TestTrue(TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
    Res &= RITest->TestTrue(TEXT("Right hand slot should have 1 Rock remaining after partial removal"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.Quantity == 1);

    // Attempt to remove more items than are present without allowing partial removals
    RemovedQuantity = InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 2, false);
    Res &= RITest->TestTrue(TEXT("Should not remove any items if attempting to remove more than present without allowing partial removal"), RemovedQuantity == 0);

    // Add an unstackable item to a slot and then remove it
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
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
    SETUP_RANCINVENTORY(100);

    bool Res = true;

    // Add item to a tagged slot directly
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true);
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
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSpear.GetTag(), 1), true);
	Res &= RITest->TestTrue(TEXT("Spear item should be added to the right hand slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSpear));

	// Attempt to move the Spear (Weapon) to HelmetSlot (Armor) directly
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move the spear item to helmet slot"), MovedQuantity, 0);
	Res &= RITest->TestFalse(TEXT("Helmet slot should not contain the spear item"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).IsValid());

	// Move the Spear back to an empty RightHandSlot from HelmetSlot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Ensure HelmetSlot is occupied with a compatible item
	MovedQuantity = InventoryComponent->MoveItemsFromAndToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), RightHandSlot.GetTag(), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move the spear item from right hand slot to helmet slot directly"), MovedQuantity, 0);
	Res &= RITest->TestTrue(TEXT("Right hand slot should still contain the spear item"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdSpear));
	Res &= RITest->TestTrue(TEXT("Helmet slot should remain unchanged"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdHelmet));

	// Attempt to move stackable items to a non-stackable slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 1), true);
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 5), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move stackable item to a non-stackable slot"), MovedQuantity, 0);

	// Move item to a slot with a different, incompatible item type
	InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Reset for clarity
	MovedQuantity = InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 1), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move item to a slot with a different item type"), MovedQuantity, 0);

	// Attempt to move items from an empty or insufficient source slot
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 1, true);
	MovedQuantity = InventoryComponent->MoveItemsFromTaggedSlot_ServerImpl(FRancItemInstance(ItemIdSpear.GetTag(), 2), RightHandSlot.GetTag()); // Assuming RightHandSlot is empty or has less than 2 Spears
	Res &= RITest->TestEqual(TEXT("Should not move items from an empty or insufficient source slot"), MovedQuantity, 0);

	// Move item to a slot with specific item type restrictions not met
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), true); // Reset with a stackable item
	MovedQuantity = InventoryComponent->MoveItemsFromAndToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 3), RightHandSlot.GetTag(), HelmetSlot.GetTag());
	Res &= RITest->TestEqual(TEXT("Should not move item to a slot with unmet item type restrictions"), MovedQuantity, 0);

    return Res;
}

bool TestDroppingFromTaggedSlot()
{
    SETUP_RANCINVENTORY(100);

    bool Res = true;

    // Step 1: Add an item to a tagged slot
    InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 3), true);
    Res &= RITest->TestTrue(TEXT("Rocks should be added to the right hand slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot.GetTag()).ItemInstance.Quantity == 3);

    // Step 2: Drop a portion of the stackable item from the tagged slot
    int32 DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot.GetTag(), 2, 0.0f);
    Res &= RITest->TestEqual(TEXT("Should set to drop a portion of the stackable item (2 Rocks)"), DroppedQuantity, 2);

    // Step 3: Attempt to drop more items than are present in the tagged slot
    DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot.GetTag(), 5, 0.0f); // Trying to drop 5 rocks, but only 1 remains
    Res &= RITest->TestEqual(TEXT("Should set to drop the remaining quantity of the item (1 Rock)"), DroppedQuantity, 1);

    // Step 4: Attempt to drop an item from an empty tagged slot
    DroppedQuantity = InventoryComponent->DropFromTaggedSlot(LeftHandSlot.GetTag(), 1, 0.0f); // Assuming LeftHandSlot is empty
    Res &= RITest->TestEqual(TEXT("Should not drop any items from an empty tagged slot"), DroppedQuantity, 0);

    // Step 5: Attempt to drop items from a tagged slot with a non-stackable item
    InventoryComponent->AddItemsToTaggedSlot_IfServer(HelmetSlot.GetTag(), FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Add a non-stackable item
    DroppedQuantity = InventoryComponent->DropFromTaggedSlot(HelmetSlot.GetTag(), 1, 0.0f);
    Res &= RITest->TestEqual(TEXT("Should set to drop the non-stackable item (Helmet)"), DroppedQuantity, 1);

    return Res;
}

bool TestCanCraftRecipe()
{
	SETUP_RANCINVENTORY(100); // Setup with a weight capacity for the test

	bool Res = true;

	// Create a recipe for crafting
	URancRecipe* TestRecipe = NewObject<URancRecipe>();
	TestRecipe->Components.Add(FRancItemInstance(ItemIdRock.GetTag(), 2)); // Requires 2 Rocks
	TestRecipe->Components.Add(FRancItemInstance(ItemIdSticks.GetTag(), 3)); // Requires 3 Sticks

	// Step 1: Inventory has all required components in the correct quantities
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 2), true);
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 3), true);
	Res &= RITest->TestTrue(TEXT("CanCraftRecipe should return true when all components are present in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 2: Inventory is missing one component
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(LeftHandSlot.GetTag(), 3, true); // Remove Sticks
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when a component is missing"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 3: Inventory has insufficient quantity of one component
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 1), true); // Add only 1 Stick
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when components are present but in insufficient quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 4: Crafting with an empty or null recipe reference
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when the recipe is null"), InventoryComponent->CanCraftRecipe(nullptr));

	// Step 5: Clear tagged slots before adding new test scenarios
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 99, true); // Clear Rocks
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(LeftHandSlot.GetTag(), 99, true); // Clear Sticks

	// Step 6: Inventory has all required components in the generic inventory

	InventoryComponent->AddItems_IfServer( FRancItemInstance(ItemIdRock.GetTag(), 2));
	InventoryComponent->AddItems_IfServer( FRancItemInstance(ItemIdSticks.GetTag(), 3));
	Res &= RITest->TestTrue(TEXT("CanCraftRecipe should return true when all components are present in generic inventory in correct quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));

	// Step 7: Generic inventory has insufficient quantity of one component
	// First, simulate removing items from generic inventory by moving them to a tagged slot and then removing
	InventoryComponent->MoveItemsToTaggedSlot_ServerImpl(FRancItemInstance(ItemIdRock.GetTag(), 1), RightHandSlot); // Simulate removing 1 Rock from generic inventory
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot, 1, true); // Actually remove the moved item
	Res &= RITest->TestFalse(TEXT("CanCraftRecipe should return false when components in generic inventory are present but in insufficient quantities"), InventoryComponent->CanCraftRecipe(TestRecipe));
	
	return Res;
}

bool TestCraftRecipe()
{
	SETUP_RANCINVENTORY(100); // Setup with a sufficient weight capacity for the test

	bool Res = true;

	// Create a test recipe
	URancRecipe* TestRecipe = NewObject<URancRecipe>();
	TestRecipe->ResultingObject = UObject::StaticClass(); // Assuming UMyCraftedObject is a valid class
	TestRecipe->QuantityCreated = 1;
	TestRecipe->Components.Add(FRancItemInstance(ItemIdRock.GetTag(), 2)); // Requires 2 Rocks
	TestRecipe->Components.Add(FRancItemInstance(ItemIdSticks.GetTag(), 3)); // Requires 3 Sticks

	// Step 1: Crafting success
	InventoryComponent->AddItems_IfServer( FRancItemInstance(ItemIdRock.GetTag(), 5));
	InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdSticks.GetTag(), 3));
	Res &= RITest->TestTrue(TEXT("CraftRecipe_IfServer should return true when all components are present"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Would be nice to test if we could confirm OnCraftConfirmed gets called but haven't found a nice way

	// Check that correct quantity of components were removed
	Res &= RITest->TestEqual(TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdRock.GetTag()), 3);
	Res &= RITest->TestEqual(TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdSticks.GetTag()), 0);

	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 99, true); // Clear Rocks from RightHandSlot
	
	// Step 2: Crafting failure due to insufficient components
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when a component is missing"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	
	// Step 4: Crafting with a null recipe
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when the recipe is null"), InventoryComponent->CraftRecipe_IfServer(nullptr));
	
	
	// Step 5: Crafting success with components spread between generic inventory and tagged slots
	InventoryComponent->AddItems_IfServer( FRancItemInstance(ItemIdRock.GetTag(), 1)); // Add 1 Rock to generic inventory
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 1), true); // Add another Rock to a tagged slot
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 3), true); // Add 3 Sticks to a tagged slot
	Res &= RITest->TestTrue(TEXT("CraftRecipe_IfServer should return true when components are spread between generic and tagged slots"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));
	// Assuming a way to verify the resulting object was created successfully

	// Step 6: Reset environment for next test
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(LeftHandSlot.GetTag(), 99, true); // Clear Rocks from LeftHandSlot
	InventoryComponent->RemoveItemsFromTaggedSlot_IfServer(RightHandSlot.GetTag(), 99, true); // Clear Sticks from HelmetSlot

	// Step 7: Crafting failure when tagged slots contain all necessary components but in insufficient quantities
	InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot.GetTag(), FRancItemInstance(ItemIdRock.GetTag(), 1), true); // Add only 1 Rock to a tagged slot, insufficient for the recipe
	InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot.GetTag(), FRancItemInstance(ItemIdSticks.GetTag(), 2), true); // Add only 2 Sticks to a tagged slot, insufficient for the recipe
	Res &= RITest->TestFalse(TEXT("CraftRecipe_IfServer should return false when not all components are present in sufficient quantities"), InventoryComponent->CraftRecipe_IfServer(TestRecipe));

	return Res;
}

bool TestInventoryMaxCapacity()
{
    SETUP_RANCINVENTORY(5); // Setup with a weight capacity of 5

    bool Res = true;

    // Step 1: Adding Stackable Items to Generic Slots
	InventoryComponent->AddItems_IfServer( FRancItemInstance(ItemIdRock.GetTag(), 3)); 
    Res &= RITest->TestEqual(TEXT("Should successfully add rocks within capacity"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdRock.GetTag()), 3);
    InventoryComponent->AddItems_IfServer(FRancItemInstance(ItemIdSticks.GetTag(), 3)); // Trying to add more rocks, total weight would be 6 but capacity is 5
	Res &= RITest->TestEqual(TEXT("Should fail to add sticks beyond capacity"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdSticks.GetTag()), 0);
	
    // Step 2: Adding Unstackable Items to Tagged Slots
    int32 QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(LeftHandSlot, FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Weight = 2
    Res &= RITest->TestEqual(TEXT("Should successfully add a helmet within capacity"), QuantityAdded, 1);
    QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FRancItemInstance(ItemIdHelmet.GetTag(), 1), true); // Trying to add another helmet, total weight would be 4
    Res &= RITest->TestEqual(TEXT("Should fail to add a second helmet beyond capacity"), QuantityAdded, 0);

    // Step 3: Adding Stackable items
    InventoryComponent->RemoveItemsFromAnyTaggedSlots_IfServer(ItemIdHelmet, 1); // Reset tagged slot
	QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FRancItemInstance(ItemIdSticks, 5)); // Try Adding 5 sticks, which should fail
    Res &= RITest->TestEqual(TEXT("AddItemsToTaggedSlot_IfServer does not do partial adding and weight exceeds capacity"), QuantityAdded, 0);
	QuantityAdded = InventoryComponent->AddItemsToTaggedSlot_IfServer(RightHandSlot, FRancItemInstance(ItemIdRock, 2));
	Res &= RITest->TestEqual(TEXT("Should successfully add 2 rocks within capacity"), QuantityAdded, 2);

	URancItemRecipe* BoulderRecipe = NewObject<URancItemRecipe>();
	BoulderRecipe->ResultingItemId = ItemIdGiantBoulder; // a boulder weighs 10
	BoulderRecipe->QuantityCreated = 1;
	BoulderRecipe->Components.Add(FRancItemInstance(ItemIdRock, 5)); // Requires 2 Rocks
	
    // Step 4: Crafting Items That Exceed Capacity
    bool CraftSuccess = InventoryComponent->CraftRecipe_IfServer(BoulderRecipe);
    Res &= RITest->TestTrue(TEXT("Crafting should succeed"), CraftSuccess);
	// Check if the crafted helmets are in inventory
	Res &= RITest->TestEqual(TEXT("Crafted boulder should not be in inventory"), InventoryComponent->GetItemCountIncludingTaggedSlots(ItemIdGiantBoulder), 0);
	
    return Res;
}

bool TestAddItemToAnySlots()
{
    SETUP_RANCINVENTORY(15); 
	InventoryComponent->MaxNumItemsInContainer = 5;
	
    bool Res = true;

    // Create item instances with specified quantities and weights
    FRancItemInstance RockInstance(ItemIdRock.GetTag(), 5);
    FRancItemInstance StickInstance(ItemIdSticks.GetTag(), 2);

	// PreferTaggedSlots = true, adding items directly to tagged slots first
	int32 Added = InventoryComponent->AddItemToAnySlots_IfServer(RockInstance, true);
	Res &= RITest->TestEqual(TEXT("Should add rocks to right hand slot"), Added, 5); // weight 5

	// remove from right hand slot
	InventoryComponent->RemoveItemsFromAnyTaggedSlots_IfServer(ItemIdRock, 5); // weight 0

    // PreferTaggedSlots = false, adding items to generic slots first
    Added = InventoryComponent->AddItemToAnySlots_IfServer(RockInstance, false);
    Res &= RITest->TestEqual(TEXT("Should add all rocks"), Added, 5); // weight 5
	Res &= RITest->TestFalse(TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	Res &= RITest->TestFalse(TEXT("Left hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	
    // Exceeding generic slot capacity, items should spill over to tagged slots if available
    Added = InventoryComponent->AddItemToAnySlots_IfServer(StickInstance, false); // weight 7
    Res &= RITest->TestEqual(TEXT("Should add sticks to left hand slot after generic slots are full"), Added, 2);
	Res &= RITest->TestEqual(TEXT("Left hand slot should contain sticks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemInstance.Quantity, 2);
    
    // Capacity limit reached, no more items should be added
    FRancItemInstance HeavyItem(ItemIdGiantBoulder, 1); // Weight 10 exceeding capacity
    Added = InventoryComponent->AddItemToAnySlots_IfServer(HeavyItem, true);
    Res &= RITest->TestEqual(TEXT("Should not add heavy items beyond weight capacity"), Added, 0);

    // Adding items back to generic slots if there's still capacity after attempting tagged slots
    InventoryComponent->MaxWeight = 30; // Increase weight capacity for this test
    Added = InventoryComponent->AddItemToAnySlots_IfServer(HeavyItem, true);
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