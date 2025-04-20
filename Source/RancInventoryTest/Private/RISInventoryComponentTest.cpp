// Copyright Rancorous Games, 2024

#include "InventoryEventListener.h"
#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/InventoryComponent.h"
#include "Data/RecipeData.h"
#include "Framework/DebugTestResult.h"
#include "MockClasses/ItemHoldingCharacter.h"

#define TestName "GameTests.RIS.InventoryComponent"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class InventoryComponentTestContext
{
public:
	InventoryComponentTestContext(float CarryCapacity)
		: TestFixture(FName(*FString(TestName)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		TempActor = TestFixture.GetWorld()->SpawnActor<AItemHoldingCharacter>();
		InventoryComponent = NewObject<UInventoryComponent>(TempActor);
		TempActor->AddInstanceComponent(InventoryComponent);
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(LeftHandSlot));
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, ItemTypeTwoHanded));
		InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);
		InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);
		InventoryComponent->MaxSlotCount = 9;
		InventoryComponent->MaxWeight = CarryCapacity;
		InventoryComponent->RegisterComponent();
		TestFixture.InitializeTestItems();
	}

	~InventoryComponentTestContext()
	{
		if (TempActor)
		{
			TempActor->Destroy();
		}
	}

	FTestFixture TestFixture;
	AActor* TempActor;
	UInventoryComponent* InventoryComponent;
};

class FInventoryComponentTestScenarios
{
public:
	FRancInventoryComponentTest* Test;

	FInventoryComponentTestScenarios(FRancInventoryComponentTest* InTest)
		: Test(InTest)
	{
	};

	bool TestAddingTaggedSlotItems() const
	{
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Ensure the left hand slot is initially empty
		Res &= Test->TestTrue(
			TEXT("No item should be in the left hand slot before addition"),
			!InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Add an unstackable item to the left hand slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, false);
		Res &= Test->TestTrue(
			TEXT("Unstackable Item should be in the left hand slot after addition"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

		// Attempt to add another unstackable item to the same slot - should fail
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Second unstackable item should not replace the first one"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 1);

		// Test adding to a specialized slot that should only accept specific items
		// Assuming HelmetSlot only accepts items with HelmetTag
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneSpear, true);
		Res &= Test->TestTrue(
			TEXT("Non-helmet item should not be added to the helmet slot"),
			!InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

		// Test adding a correct item to a specialized slot
		// NOTE This test sometimes FAILS when recompiling
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Helmet item should be added to the helmet slot"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

		// Test adding a stackable item to an empty slot and then adding a different stackable item to the same slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, false);
		Res &= Test->TestFalse(
			TEXT(
				"Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock)"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks));

		// Test adding an item to a slot that is not designated as either universal or specialized (invalid slot)
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, FGameplayTag::EmptyTag, OneRock, false);
		Res &= Test->TestFalse(
			TEXT("Item should not be added to an invalid slot"),
			InventoryComponent->GetItemForTaggedSlot(FGameplayTag::EmptyTag).IsValid());

		// Test adding a stackable item to the max stack size and then attempting to add more with allowpartial, which should return 0
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, FiveRocks, true);
		int32 AmountAdded = InventoryComponent->
			AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
		Res &= Test->TestEqual(
			TEXT("Stackable Item (Rock) amount added should be none as already full stack"), AmountAdded, 0);
		Res &= Test->TestEqual(
			TEXT("Right hand slot should have 5 Rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity, 5);

		// Set right hand to have 1 rock
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 4, EItemChangeReason::Removed, true);

		// Test adding a stackable item to a slot that has a different stackable item with partial true
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 4, true);
		// Assuming different item than before
		// Should not have added
		Res &= Test->TestFalse(
			TEXT(
				"Different stackable item (Sticks) should not be added to a slot already containing a stackable item (Rock) without override"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks));
		Res &= Test->TestEqual(
			TEXT("Right hand slot should have 1 Rock"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity, 1);

		return Res;
	}

	bool TestRemovingTaggedSlotItems() const
	{
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Add stackable item to a slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);

		// Remove a portion of the stackable item
		int32 RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
			RightHandSlot, 2, EItemChangeReason::Removed, true);
		Res &= Test->TestTrue(
			TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should have 1 Rock remaining after partial removal"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 1);

		// Attempt to remove more items than are present without allowing partial removals
		RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
			RightHandSlot, 2, EItemChangeReason::Removed, false);
		Res &= Test->TestTrue(
			TEXT(
				"Should not remove any items if attempting to remove more than present without allowing partial removal"),
			RemovedQuantity == 0);

		// Add an unstackable item to a slot and then remove it
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
			HelmetSlot, 1, EItemChangeReason::Removed, true);
		Res &= Test->TestTrue(TEXT("Should successfully remove unstackable item (Helmet)"), RemovedQuantity == 1);
		Res &= Test->TestFalse(
			TEXT("Helmet slot should be empty after removing the item"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

		// Attempt to remove an item from an empty slot
		RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
			LeftHandSlot, 1, EItemChangeReason::Removed, true);
		Res &= Test->TestTrue(TEXT("Should not remove any items from an empty slot"), RemovedQuantity == 0);

		// Attempt to remove an item from a non-existent slot
		RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
			FGameplayTag::EmptyTag, 1, EItemChangeReason::Removed, true);
		Res &= Test->TestTrue(TEXT("Should not remove any items from a non-existent slot"), RemovedQuantity == 0);

		return Res;
	}

	bool TestRemoveAnyItemFromTaggedSlot()
	{
		InventoryComponentTestContext Context(100); // Sufficient capacity initially
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		InventoryComponent->MaxSlotCount = 9; // Reset slots for clarity
		FDebugTestResult Res = true;
		UGlobalInventoryEventListener* Listener = NewObject<UGlobalInventoryEventListener>();
		Listener->SubscribeToInventoryComponent(InventoryComponent);

		// --- Test Case 1: Basic Move Success (Stackable Item) ---
		InventoryComponent->Clear_IfServer();
		Listener->Clear();
		// Add 3 rocks to right hand
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Right hand should have 3 rocks before clear"),
		                      InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic inventory should be empty before clear"),
		                       InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 0);

		// Clear the slot
		int32 MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(RightHandSlot);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Should return 3 as the moved quantity"), MovedQuantity, 3);
		Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Right hand slot should be empty after clear"),
		                       InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic inventory should now have 3 rocks"),
		                       InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 3);
		// Verify events
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Remove event should fire"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Add event should fire"), Listener->bItemAddedTriggered);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Correct removed quantity in event"), Listener->RemovedFromTaggedQuantity, 3);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Correct added quantity in event"), Listener->AddedQuantity, 3);

		// --- Test Case 2: Basic Move Success (Unstackable Item) ---
		InventoryComponent->Clear_IfServer();
		Listener->Clear();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Helmet slot should have helmet before clear"),
		                      InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

		MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(HelmetSlot);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Should return 1 for unstackable move"), MovedQuantity, 1);
		Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Helmet slot should be empty after clear"),
		                       InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic inventory should now have 1 helmet"),
		                       InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet), 1);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Remove event should fire for helmet"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Add event should fire for helmet"), Listener->bItemAddedTriggered);

		// --- Test Case 3: Failure - Clearing an Empty Slot ---
		Listener->Clear();
		MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(LeftHandSlot); // Assuming empty
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Clearing an empty slot should return 0"), MovedQuantity, 0);
		Res &= Test->TestFalse(TEXT("[RemoveAnyItem] No remove event should fire for empty slot"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestFalse(TEXT("[RemoveAnyItem] No add event should fire for empty slot"), Listener->bItemAddedTriggered);

        // --- Test Case 4: Success - Clearing a Blocked Slot (Item present) ---
		// Moving *from* a blocked slot should succeed if the target (generic) is okay.
		InventoryComponent->Clear_IfServer();
		Listener->Clear();
		InventoryComponent->MaxWeight = 100;
		InventoryComponent->MaxSlotCount = 9;
        // Setup: Spear to RightHand (blocking LeftHand)
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear, true);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Left hand should be blocked (with item)"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

        // Try to clear the blocked slot containing an item.
		MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(RightHandSlot);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Clearing a blocked slot (with item) should succeed"), MovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Left hand slot should be empty after clearing blocked slot"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Right hand slot should be empty after clearing blocked slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		// The slot itself remains marked as blocked because the Spear is still equipped
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic should have the spear after clearing blocked slot"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear), 1);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Remove event should fire for clearing blocked"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Add event should fire for clearing blocked"), Listener->bItemAddedTriggered);

		return Res;
	}
	
	bool TestMoveTaggedSlotItems()
	{
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Add item to a tagged slot directly
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Helmet item should be added to the helmet slot"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));
		Res &= Test->TestTrue(
			TEXT("Container should be empty"), InventoryComponent->GetContainedQuantity(HelmetSlot) == 0);

		// Move item from tagged slot to generic inventory (cannot directly verify generic inventory, so just ensure removal)
		int32 SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, HelmetSlot, FGameplayTag::EmptyTag, FGameplayTag(), 0);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the helmet item from the helmet slot to generic inventory"), SimulatedMoveQuantity, 1);
		int32 MovedQuantity = InventoryComponent->MoveItem(OneHelmet, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should move the helmet item from the tagged slot to generic inventory"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Helmet slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain the helmet item"),
			InventoryComponent->GetContainedQuantity(ItemIdHelmet) == 1);
		// generic inventory now has the helmet item

		// Try to move item to invalid (chest) slot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FGameplayTag::EmptyTag, ChestSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the helmet item from the helmet slot to generic inventory"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FGameplayTag::EmptyTag, ChestSlot);
		Res &= Test->TestEqual(TEXT("Should not move the helmet item to the chest slot"), MovedQuantity, 0);
		Res &= Test->TestFalse(
			TEXT("Chest slot should not contain the helmet item"),
			InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

		// Move item back to a different tagged slot from generic inventory
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the helmet item from generic inventory to right hand slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should move the helmet item from generic inventory to right hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should now contain the helmet item"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));

		// Move item from one hand to the other
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the helmet item from right hand slot to left hand slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should move the helmet item from right hand slot to left hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should now contain the helmet item"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

		// Attempt to move an item that doesn't exist in the source tagged slot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving an item that doesn't exist in the source tagged slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move an item that doesn't exist in the source tagged slot"), MovedQuantity, 0);

		// Status: Inventory contains 1 item, helmet in left hand slot

		// Add an item compatible with RightHandSlot but not with HelmetSlot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock, true);
		Res &= Test->TestTrue(
			TEXT("Rock item should be added to the right hand slot"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

		// Attempt to move the Rock to HelmetSlot (Armor) directly, should fail
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, RightHandSlot, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the rock item from right hand slot to helmet slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, RightHandSlot, HelmetSlot);
		Res &= Test->TestEqual(TEXT("Should not move the rock item to helmet slot"), MovedQuantity, 0);
		Res &= Test->TestFalse(
			TEXT("Helmet slot should still be empty"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

		// Try to Move a (non existent) Rock back to a RightHandSlot from HelmetSlot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, HelmetSlot, RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the rock item from helmet slot to right hand slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, HelmetSlot, RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move the rock item from right hand slot to helmet slot directly"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain the rock item"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

		// Try again but other way which should also fail because helmet slot is empty AND spear is incompatible
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneSpear, HelmetSlot, RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should simulate moving the spear item from helmet slot to right hand slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, RightHandSlot, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move the helmet item from left hand slot to right hand slot directly"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain the rock item"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

		// Status: Helmet in left hand, Rock in right hand

		// attempt to swap the items in the hands
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, LeftHandSlot, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving the helmet item from left hand slot to right hand slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, LeftHandSlot, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should Swap the two items"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should now contain the helmet item"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));
		Res &= Test->TestTrue(
			TEXT("Left hand slot should now contain the rock item"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

		// move helmet from right hand to helmet slot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, RightHandSlot, HelmetSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving the helmet item from right hand slot to helmet slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, RightHandSlot, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should move the helmet item from right hand slot to helmet slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Helmet slot should now contain the helmet item"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

		// Now try moving rock from left hand to helmet slot which should fail
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, LeftHandSlot, HelmetSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving the rock item from left hand slot to helmet slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, LeftHandSlot, HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move the rock item from left hand slot to helmet slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should still contain the rock item"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

		// Try again the other direction, which should also fail because the rock would get swapped to helmet slot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, HelmetSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving the rock item from helmet slot to left hand slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneHelmet, HelmetSlot, LeftHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move the helmet item from helmet slot to left hand slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should still contain the rock item"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

		// Remove rock, Attempt to move items from an empty or insufficient source slot
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, EItemChangeReason::Removed, true);
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, LeftHandSlot, HelmetSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving the rock item from left hand slot to helmet slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, LeftHandSlot); // Assuming RightHandSlot is empty
		Res &= Test->TestEqual(
			TEXT("Should not move items from an empty or insufficient source slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("No items in generic inventory"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 0);

		// Reset
		InventoryComponent->Clear_IfServer();
		Res &= Test->TestTrue(
			TEXT("Left hand slot should be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(
			TEXT("Right hand slot should be empty"),
			!InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// Test moving stackable rocks
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 8);
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ItemIdRock, 8, FGameplayTag::EmptyTag, RightHandSlot);
        		Res &= Test->TestEqual(TEXT("Should simulate moving 8 rocks to right hand slot"), SimulatedMoveQuantity, 8);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks to right hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 5 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);
		// Try to move remaining 5, expecting 2 to be moved
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 5 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		// Try again and verify none are moved
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate not moving 3 rocks to right hand slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move any rocks to right hand slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		// Move to other hand in several steps
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks from right hand slot to left hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain 2 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);

		// Decided to allow this for now // Attempt to move more than exists to verify that it fails
		// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 5), RightHandSlot, LeftHandSlot);
		// Res &= Test->TestEqual(TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
		// Res &= Test->TestTrue(TEXT("Left hand slot shou<ld still contain 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);

		// Now move the remaining 3 rocks to the left hand slot
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should be empty"),
			!InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// Now we test the same kind of rock moving but to and then from generic inventory
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from left hand slot to generic inventory"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks from left hand slot to generic inventory"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should now hold 2 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 6 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 6);

		// Decided to allow this for now // Move more than exists
		// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 5), LeftHandSlot);
		// Res &= Test->TestEqual(TEXT("Should not move any rocks from left hand slot to generic inventory"), MovedQuantity, 0);
		// Res &= Test->TestTrue(TEXT("Left hand slot should still hold 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
		// Res &= Test->TestTrue(TEXT("Generic inventory should contain 6 rocks"), InventoryComponent->GetContainedQuantity(ItemIdRock) == 6);

		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks from left hand slot to generic inventory"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks from left hand slot to generic inventory"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should now be empty"),
			!InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 8 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 8);

		// move more than exists
		// MovedQuantity = InventoryComponent->MoveItem(FRancItemBundle(ItemIdRock, 10), FGameplayTag::EmptyTag, LeftHandSlot);
		// Res &= Test->TestEqual(TEXT("Should not move any rocks to left hand slot"), MovedQuantity, 0);
		// Res &= Test->TestTrue(TEXT("Left hand slot should still be empty"), !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		// Res &= Test->TestTrue(TEXT("Generic inventory should contain 8 rocks"), InventoryComponent->GetContainedQuantity(ItemIdRock) == 8);

		// Move back to right hand
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 2 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 2);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 6 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 6);

		// Try moving just 1 more rock to Right hand
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 1 rock to right hand slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 1 rock to right hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 5 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);

		// move 2 more to get full stack
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		// remove two rocks from right hand, leaving three, then add a stick to left hand
		// Then we try to swap the hand contents but with only 1 rock, which is invalid as it would leave 2 rocks behind making the swap impossible
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, EItemChangeReason::Removed, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneStick, true);
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 1 rock from right hand slot to left hand slot"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain the stick"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSticks));

		// Now move all 3 rocks expecting a swap
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 3);
		
		return Res;
	}

	bool TestMoveOperationsWithSwapback()
	{
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;
		
		// Now lets test some full inventory cases
		InventoryComponent->Clear_IfServer();
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 10*5); // Fill up rest of generic inventory and both hands
		Res &= Test->TestTrue(TEXT("Left and Right hand should have rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));
		int32 MovedQuantity = InventoryComponent->MoveItem(OneSpear, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as left hand is occupied and cannot be cleared"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(
			TEXT("Inventory should still contain 10 rocks"),
			InventoryComponent->GetContainedQuantity(ItemIdRock) == 10*5);

		// Now with swapback which should still fail
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 5);
		Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as left hand is occupied and cannot be cleared"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// Now with swapback in opposite direction, rock -> righthand
		MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot, OneSpear);
		Res &= Test->TestEqual(TEXT("Should not move rock to right hand slot as left hand is occupied and cannot be cleared"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		
		// Now remove rock from left hand and try again, rock and spear should swap
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, EItemChangeReason::Removed, true);
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move spear to right hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(TEXT("Left hand should be empty"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Inventory should now contain 9 rocks"),
			InventoryComponent->GetContainedQuantity(ItemIdRock) == 9*5);

		// Move spear back to generic inventory swapping with a rock explicitly
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, RightHandSlot, FGameplayTag::EmptyTag, ItemIdRock, 5);
		Res &= Test->TestEqual(TEXT("Should move spear to generic inventory"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Generic inventory should contain the spear"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Res &= Test->TestTrue(TEXT("Right hand should contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);

		// And back to right hand swapping with rock again
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 5);
		Res &= Test->TestEqual(TEXT("Should move spear to right hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

		// Now swap a rock from generic to right hand, with swapback of spear
		MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot, OneSpear);
		Res &= Test->TestEqual(TEXT("Should move rock to right hand slot"), MovedQuantity, 5);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the rock"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);

		return Res;
	}
	
	bool TestDroppingFromTaggedSlot()
	{
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Step 1: Add an item to a tagged slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
		Res &= Test->TestTrue(
			TEXT("Rocks should be added to the right hand slot"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent
			->GetItemForTaggedSlot(RightHandSlot).
			Quantity == 3);

		// Step 2: Drop a portion of the stackable item from the tagged slot
		int32 DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 2, FVector());
		Res &= Test->
			TestEqual(TEXT("Should set to drop a portion of the stackable item (2 Rocks)"), DroppedQuantity, 2);

		// Step 3: Attempt to drop more items than are present in the tagged slot
		DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 5, FVector());
		// Trying to drop 5 rocks, but only 1 remains
		Res &= Test->TestEqual(
			TEXT("Should set to drop the remaining quantity of the item (1 Rock)"), DroppedQuantity, 1);

		// Step 4: Attempt to drop an item from an empty tagged slot
		DroppedQuantity = InventoryComponent->DropFromTaggedSlot(LeftHandSlot, 1, FVector());
		// Assuming LeftHandSlot is empty
		Res &= Test->TestEqual(TEXT("Should not drop any items from an empty tagged slot"), DroppedQuantity, 0);

		// Step 5: Attempt to drop items from a tagged slot with a non-stackable item
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		// Add a non-stackable item
		DroppedQuantity = InventoryComponent->DropFromTaggedSlot(HelmetSlot, 1, FVector());
		Res &= Test->TestEqual(TEXT("Should set to drop the non-stackable item (Helmet)"), DroppedQuantity, 1);

		return Res;
	}

	bool TestCanCraftRecipe() const
	{
		InventoryComponentTestContext Context(100); // Setup with a weight capacity for the test
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Create a recipe for crafting
		UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
		TestRecipe->Components.Add(FItemBundle(TwoRocks)); // Requires 2 Rocks
		TestRecipe->Components.Add(FItemBundle(ThreeSticks)); // Requires 3 Sticks

		// Step 1: Inventory has all required components in the correct quantities
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, TwoRocks, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeSticks, true);
		Res &= Test->TestTrue(
			TEXT("CanCraftRecipe should return true when all components are present in correct quantities"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		// Step 2: Inventory is missing one component
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 3, EItemChangeReason::Removed, true);
		// Remove Sticks
		Res &= Test->TestFalse(
			TEXT("CanCraftRecipe should return false when a component is missing"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		// Step 3: Inventory has insufficient quantity of one component
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 1, true);
		// Add only 1 Stick
		Res &= Test->TestFalse(
			TEXT("CanCraftRecipe should return false when components are present but in insufficient quantities"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		// Step 4: Crafting with an empty or null recipe reference
		Res &= Test->TestFalse(
			TEXT("CanCraftRecipe should return false when the recipe is null"),
			InventoryComponent->CanCraftRecipe(nullptr));

		// Step 5: Clear tagged slots before adding new test scenarios
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed, true);
		// Clear Rocks
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, EItemChangeReason::Removed, true);
		// Clear Sticks

		// Step 6: Inventory has all required components in the generic inventory

		InventoryComponent->AddItem_IfServer(Subsystem, TwoRocks);
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
		Res &= Test->TestTrue(
			TEXT(
				"CanCraftRecipe should return true when all components are present in generic inventory in correct quantities"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		// Step 7: Generic inventory has insufficient quantity of one component
		// First, simulate removing items from generic inventory by moving them to a tagged slot and then removing
		InventoryComponent->MoveItem(OneRock, FGameplayTag::EmptyTag, RightHandSlot);
		// Simulate removing 1 Rock from generic inventory
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Removed, true);
		// Actually remove the moved item
		Res &= Test->TestFalse(
			TEXT(
				"CanCraftRecipe should return false when components in generic inventory are present but in insufficient quantities"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		return Res;
	}

	bool TestCraftRecipe()
	{
		InventoryComponentTestContext Context(100); // Setup with a sufficient weight capacity for the test
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Create a test recipe
		UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
		TestRecipe->ResultingObject = UObject::StaticClass(); // Assuming UMyCraftedObject is a valid class
		TestRecipe->QuantityCreated = 1;
		TestRecipe->Components.Add(FItemBundle(TwoRocks)); // Requires 2 Rocks
		TestRecipe->Components.Add(FItemBundle(ThreeSticks)); // Requires 3 Sticks

		// Step 1: Crafting success
		InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks);
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
		Res &= Test->TestTrue(
			TEXT("CraftRecipe_IfServer should return true when all components are present"),
			InventoryComponent->CraftRecipe_IfServer(TestRecipe));
		// Would be nice to test if we could confirm OnCraftConfirmed gets called but haven't found a nice way

		// Check that correct quantity of components were removed
		Res &= Test->TestEqual(
			TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"),
			InventoryComponent->GetItemQuantityTotal(ItemIdRock), 3);
		Res &= Test->TestEqual(
			TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"),
			InventoryComponent->GetItemQuantityTotal(ItemIdSticks), 0);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed, true);
		// Clear Rocks from RightHandSlot

		// Step 2: Crafting failure due to insufficient components
		Res &= Test->TestFalse(
			TEXT("CraftRecipe_IfServer should return false when a component is missing"),
			InventoryComponent->CraftRecipe_IfServer(TestRecipe));

		// Step 4: Crafting with a null recipe
		Res &= Test->TestFalse(
			TEXT("CraftRecipe_IfServer should return false when the recipe is null"),
			InventoryComponent->CraftRecipe_IfServer(nullptr));


		// Step 5: Crafting success with components spread between generic inventory and tagged slots
		InventoryComponent->AddItem_IfServer(Subsystem, OneRock); // Add 1 Rock to generic inventory
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		// Add another Rock to a tagged slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeSticks, true);
		// Add 3 Sticks to a tagged slot
		Res &= Test->TestTrue(
			TEXT("CraftRecipe_IfServer should return true when components are spread between generic and tagged slots"),
			InventoryComponent->CraftRecipe_IfServer(TestRecipe));
		// Assuming a way to verify the resulting object was created successfully

		// Step 6: Reset environment for next test
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, EItemChangeReason::Removed, true);
		// Clear Rocks from LeftHandSlot
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed, true);
		// Clear Sticks from HelmetSlot

		// Step 7: Crafting failure when tagged slots contain all necessary components but in insufficient quantities
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		// Add only 1 Rock to a tagged slot, insufficient for the recipe
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, true);
		// Add only 2 Sticks to a tagged slot, insufficient for the recipe
		Res &= Test->TestFalse(
			TEXT(
				"CraftRecipe_IfServer should return false when not all components are present in sufficient quantities"),
			InventoryComponent->CraftRecipe_IfServer(TestRecipe));

		return Res;
	}

	bool TestInventoryMaxCapacity()
	{
		InventoryComponentTestContext Context(5); // Setup with a weight capacity of 5
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Step 1: Adding Stackable Items to Generic Slots
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeRocks);
		Res &= Test->TestEqual(
			TEXT("Should successfully add rocks within capacity"), InventoryComponent->GetItemQuantityTotal(ItemIdRock),
			3);
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
		// Trying to add more rocks, total weight would be 6 but capacity is 5
		Res &= Test->TestTrue(
			TEXT("Should fail to add all 3 sticks due to weight capacity"), InventoryComponent->GetItemQuantityTotal(ItemIdSticks) < 3);

		// Remove any sticks we might have added partially
		InventoryComponent->DestroyItem_IfServer(ItemIdSticks, 99, EItemChangeReason::Removed, true);
		// Verify removal of sticks only
		Res &= Test->TestEqual(
			TEXT("Should remove all sticks"), InventoryComponent->GetItemQuantityTotal(ItemIdSticks), 0);
		Res &= Test->TestEqual(
			TEXT("Should not remove any rocks"), InventoryComponent->GetItemQuantityTotal(ItemIdRock), 3);

		// Step 2: Adding Unstackable Items to Tagged Slots
		int32 QuantityAdded = InventoryComponent->
			AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true); // Weight = 2
		Res &= Test->TestEqual(TEXT("Should successfully add a helmet within capacity"), QuantityAdded, 1);
		QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneHelmet, true);
		// Trying to add another helmet, total weight would be 4
		Res &= Test->TestEqual(TEXT("Should fail to add a second helmet beyond capacity"), QuantityAdded, 0);

		// Step 3: Adding Stackable items
		InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdHelmet, 1, EItemChangeReason::Removed);
		// Reset tagged slot
		QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(
			Subsystem, RightHandSlot, ItemIdSticks, 5, false); // Try Adding 5 sticks, which should fail
		Res &= Test->TestEqual(
			TEXT("AddItemToTaggedSlot_IfServer does not do partial adding and weight exceeds capacity"), QuantityAdded,
			0);
		QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, TwoRocks);
		Res &= Test->TestEqual(TEXT("Should successfully add 2 rocks within capacity"), QuantityAdded, 2);

		UItemRecipeData* BoulderRecipe = NewObject<UItemRecipeData>();
		BoulderRecipe->ResultingItemId = ItemIdGiantBoulder; // a boulder weighs 10
		BoulderRecipe->QuantityCreated = 1;
		BoulderRecipe->Components.Add(FItemBundle(ItemIdRock, 5)); // Requires 2 Rocks

		// Step 4: Crafting Items That Exceed Capacity
		bool CraftSuccess = InventoryComponent->CraftRecipe_IfServer(BoulderRecipe);
		// Whether this should succeed or not is up to the game design, but it should not be added to inventory if it exceeds capacity
		// Res &= Test->TestFalse(TEXT("Crafting should/should not succeed"), CraftSuccess); 
		// Check that the crafted item is not in inventory
		Res &= Test->TestEqual(
			TEXT("Crafted boulder should not be in inventory"),
			InventoryComponent->GetItemQuantityTotal(ItemIdGiantBoulder), 0);

		return Res;
	}

	bool TestAddItemToAnySlots()
	{
		InventoryComponentTestContext Context(20);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		InventoryComponent->MaxSlotCount = 2;

		FDebugTestResult Res = true;
		// Create item instances with specified quantities and weights

		// PreferTaggedSlots = true, adding items directly to tagged slots first
		int32 Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add rocks to right hand slot"), Added, 5); // weight 5

		// remove from right hand slot
		InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdRock, 5, EItemChangeReason::Removed);
		// weight 0
		Res &= Test->TestFalse(
			TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// PreferTaggedSlots = false, adding items to generic slots first
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= Test->TestEqual(TEXT("Should add all rocks"), Added, 5); // weight 5
		Res &= Test->TestFalse(
			TEXT("Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestFalse(
			TEXT("Left hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Exceeding generic slot count, items should spill over to tagged slots if available
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory); // take up last slot
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 2, EPreferredSlotPolicy::PreferGenericInventory); // weight 12
		Res &= Test->TestEqual(
			TEXT("Should add sticks to the first universal tagged slot after generic slots are full"), Added, 2);
		Res &= Test->TestEqual(
			TEXT("First universal tagged slot (left hand) should contain sticks"),
			InventoryComponent->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0].Slot).Quantity, 2);

		// Weight limit almost reached, no heavy items should be added despite right hand being available
		// Add a boulder with weight 22 exceeding capacity
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdGiantBoulder, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should not add heavy items beyond weight capacity"), Added, 0);

		InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);

		// Adding items back to generic slots if there's still capacity after attempting tagged slots
		InventoryComponent->MaxWeight = 25; // Increase weight capacity for this test
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdGiantBoulder, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add heavy items to generic slots after trying tagged slots"), Added, 1);

		return Res;
	}

	bool TestAddStackableItems() const
	{
	    InventoryComponentTestContext Context(100); // Setup with sufficient capacity
	    auto* InventoryComponent = Context.InventoryComponent;
	    auto* Subsystem = Context.TestFixture.GetSubsystem();
	    FDebugTestResult Res = true;
		
	    // **Test 1: Add stackable items to generic inventory**
	    // Add 20 rocks, which should fit entirely in generic slots (9 slots, 5 per slot = 45 max)
	    int32 Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 20, true);
	    Res &= Test->TestEqual(TEXT("Should add 20 rocks"), Added, 20);
	    int32 GenericRockCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 20 rocks"), GenericRockCount, 20);

	    // **Test 2: Add more rocks to fill generic inventory**
	    // Add 25 more rocks, filling the remaining generic capacity (45 - 20 = 25)
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 25, true);
	    Res &= Test->TestEqual(TEXT("Should add 25 more rocks"), Added, 25);
	    GenericRockCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 45 rocks"), GenericRockCount, 45);

	    // **Test 3: Add rocks that spill over to universal tagged slots**
	    // Add 10 rocks, which should go to LeftHandSlot (5) and RightHandSlot (5)
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 10, true);
	    Res &= Test->TestEqual(TEXT("Should add 10 rocks to tagged slots"), Added, 10);
	    FTaggedItemBundle LeftHand = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
	    FTaggedItemBundle RightHand = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Left hand should have 5 rocks"), LeftHand.Quantity, 5);
	    Res &= Test->TestEqual(TEXT("Right hand should have 5 rocks"), RightHand.Quantity, 5);

	    // **Test 4: Try to add more rocks when all slots are full**
	    // Total capacity is 55 (9*5 + 5 + 5), so adding 5 more should fail
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 5, true);
	    Res &= Test->TestEqual(TEXT("Should not add any more rocks"), Added, 0);

	    // **Test 5: Add to a specific tagged slot incrementally**
	    InventoryComponent->Clear_IfServer();
	    Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 3, true);
	    Res &= Test->TestEqual(TEXT("Should add 3 rocks to right hand"), Added, 3);
	    RightHand = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Right hand should have 3 rocks"), RightHand.Quantity, 3);

	    // Add more to the same slot, up to the stack limit
	    Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 2, true);
	    Res &= Test->TestEqual(TEXT("Should add 2 more rocks to right hand"), Added, 2);
	    RightHand = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Right hand should have 5 rocks"), RightHand.Quantity, 5);

	    // Try to exceed the stack limit
	    Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 1, true);
	    Res &= Test->TestEqual(TEXT("Should not add more rocks to right hand"), Added, 0);

	    // **Test 6: Try to add stackable item to a specialized slot that doesn't accept it**
	    Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, ItemIdRock, 1, true);
	    Res &= Test->TestEqual(TEXT("Should not add rock to helmet slot"), Added, 0);
	    FTaggedItemBundle HelmetSlotItem = InventoryComponent->GetItemForTaggedSlot(HelmetSlot);
	    Res &= Test->TestFalse(TEXT("Helmet slot should be empty"), HelmetSlotItem.IsValid());

	    // **Test 7: Add items with AllowPartial = false**
	    InventoryComponent->Clear_IfServer();
	    // Try to add 60 rocks (exceeds total capacity of 55) with AllowPartial = false
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 60, false);
	    Res &= Test->TestEqual(TEXT("Should not add any rocks since 60 > 55"), Added, 0);
	    GenericRockCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock);
	    LeftHand = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
	    RightHand = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 0 rocks"), GenericRockCount, 0);
	    Res &= Test->TestFalse(TEXT("Left hand should be empty"), LeftHand.IsValid());
	    Res &= Test->TestFalse(TEXT("Right hand should be empty"), RightHand.IsValid());

	    // **Test 8: Add different stackable items**
	    InventoryComponent->Clear_IfServer();
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 5, true);
	    Res &= Test->TestEqual(TEXT("Should add 5 rocks"), Added, 5);
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 5, true);
	    Res &= Test->TestEqual(TEXT("Should add 5 sticks"), Added, 5);
	    int32 RockCount = InventoryComponent->GetItemQuantityTotal(ItemIdRock);
	    int32 StickCount = InventoryComponent->GetItemQuantityTotal(ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Total rocks should be 5"), RockCount, 5);
	    Res &= Test->TestEqual(TEXT("Total sticks should be 5"), StickCount, 5);

	    // **Test 9: Fill partial stack in universal tagged slot before adding to generic inventory**
	    InventoryComponent->Clear_IfServer();
	    // Add 3 sticks to LeftHandSlot (partial stack)
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 3, true);
	    // Add 5 sticks with preference for generic inventory
	    Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 5, EPreferredSlotPolicy::PreferGenericInventory);
	    Res &= Test->TestEqual(TEXT("Should add 5 sticks, filling LeftHandSlot first"), Added, 5);
	    // Check LeftHandSlot: should be filled to 5
	    LeftHand = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
	    Res &= Test->TestEqual(TEXT("Left hand should have 5 sticks"), LeftHand.Quantity, 5);
	    // Check generic inventory: should have 3 sticks in one slot
	    int32 GenericStickCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 3 sticks"), GenericStickCount, 3);

	    // **Test 10: Fill partial stack in generic inventory before adding to new slots**
	    InventoryComponent->Clear_IfServer();
	    // Add 2 sticks to a generic slot (partial stack)
	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 2, true);
	    // Add 5 sticks
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 5, true);
	    Res &= Test->TestEqual(TEXT("Should add 5 sticks, filling existing stack first"), Added, 5);
	    // Check generic inventory: should have one stack of 5 and another of 2
	    GenericStickCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 7 sticks"), GenericStickCount, 7);
	    // Assuming the system fills the existing stack first, then creates a new stack
	    // Need to check the individual slot quantities if possible
	    // For now, assume total quantity is correct

	    // **Test 11: Fill multiple partial stacks**
	    InventoryComponent->Clear_IfServer();
	    // Add 2 sticks to LeftHandSlot
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 2, true);
	    // Add 3 sticks to a generic slot
	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 3, true);
	    // Add 10 sticks
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 10, true);
	    Res &= Test->TestEqual(TEXT("Should add 10 sticks, filling partial stacks first"), Added, 10);
	    // Check LeftHandSlot: should be filled to 5 (added 3)
	    LeftHand = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
	    Res &= Test->TestEqual(TEXT("Left hand should have 5 sticks"), LeftHand.Quantity, 5);
	    // Check generic inventory: existing stack filled to 5 (added 2), and new stack of 5
	    GenericStickCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 10 sticks"), GenericStickCount, 10);
	    // Again, assuming the system fills existing stacks first

	    // **Test 12: Respect stack limits and slot capacities**
	    InventoryComponent->Clear_IfServer();
	    // Fill LeftHandSlot with 5 sticks
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 5, true);
	    // Fill RightHandSlot with 5 sticks
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 5, true);
	    // Fill generic slots with full stacks of sticks (9 slots * 5 = 45)
	    for (int i = 0; i < 9; ++i)
	    {
	        InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 5, true);
	    }
	    // Try to add 1 more stick
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 1, true);
	    Res &= Test->TestEqual(TEXT("Should not add any more sticks"), Added, 0);

	    // **Test 13: Add items when partial stacks are in different slot types**
	    InventoryComponent->Clear_IfServer();
	    // Add 2 sticks to LeftHandSlot
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdSticks, 2, true);
	    // Add 2 sticks to a generic slot
	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 2, true);
	    // Add 6 sticks
	    Added = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 6, true);
	    Res &= Test->TestEqual(TEXT("Should add 6 sticks, filling partial stacks"), Added, 6);
	    // Check LeftHandSlot: should be filled to 5 (added 3)
	    LeftHand = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
	    Res &= Test->TestEqual(TEXT("Left hand should have 5 sticks"), LeftHand.Quantity, 5);
	    // Check generic inventory: existing stack filled to 5 (added 3), no new stacks
	    GenericStickCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Generic inventory should have 5 sticks"), GenericStickCount, 5);

	    return Res;
	}

	bool TestAddItem()
	{
		// Inventory component overrides the AddItem_IfServer function to handle item addition logic relating to tagged slots.
		InventoryComponentTestContext Context(100);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		// Ensure starting with a clean slate.
		InventoryComponent->Clear_IfServer();

		// ============================================================================
		// Part 1: Test with a Stackable Item (e.g., Rock)
		// ============================================================================
		// For stackable items (like rocks), our system should fill:
		// - 40 items into the generic container (diablo-style grid slots)
		// - 5 items into the LeftHandSlot (a universal tagged slot)
		// - 5 items into the RightHandSlot (a universal tagged slot)
		// Total expected = 40 + 5 + 5 = 50

		int32 RequestedRocks = 60;
		int32 AddedRocks = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, RequestedRocks, true);
		Res &= Test->TestEqual(
			TEXT("Adding 60 Rocks should return 55 as the added quantity (5*9+5+5)"),
			AddedRocks, 55);

		// Check generic inventory (container) for rocks.
		// Expected: 40 rocks (i.e. 8 full slots at 5 per slot)
		int32 GenericRockCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock);
		Res &= Test->TestEqual(
			TEXT("Generic inventory should hold 45 Rocks"),
			GenericRockCount, 45);

		// Check universal tagged slots.
		FTaggedItemBundle LeftHandBundle = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
		FTaggedItemBundle RightHandBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Left hand slot should hold 5 Rocks"),
			LeftHandBundle.Quantity, 5);
		Res &= Test->TestEqual(
			TEXT("Right hand slot should hold 5 Rocks"),
			RightHandBundle.Quantity, 5);

		// Also verify that other tagged slots (specialized ones) remain empty (Rocks shouldn't go there).
		Res &= Test->TestFalse(
			TEXT("Helmet slot should not contain Rocks"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		Res &= Test->TestFalse(
			TEXT("Chest slot should not contain Rocks"),
			InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

		// ============================================================================
		// Part 2: Test with an Unstackable Item (e.g., Helmet)
		// ============================================================================
		// For unstackable items, each occupies one slot.
		// Valid slots for a Helmet are: the generic container (if the dedicated slot is empty)
		// and the specialized HelmetSlot (which accepts Helmet items).
		// With MaxContainerSlotCount set to 9 and HelmetSlot available,
		// the maximum capacity for Helmet items should be 9 (generic) + 1 (helmet slot) = 10.
		// So, if we request 15 helmets with AllowPartial true, we should get 10 added.

		InventoryComponent->Clear_IfServer();

		int32 RequestedHelmets = 15;
		int32 AddedHelmets = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, RequestedHelmets, true);
		Res &= Test->TestEqual(
			TEXT("Adding 15 Helmets should only add 12 due to slot limits"),
			AddedHelmets, 12);

		// Check generic container for helmets.
		int32 GenericHelmetCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet);
		Res &= Test->TestEqual(
			TEXT("Generic inventory should hold 9 Helmets"),
			GenericHelmetCount, 9);

		// Check specialized tagged slot for Helmet.
		FTaggedItemBundle HelmetSlotBundle = InventoryComponent->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestEqual(
			TEXT("Helmet slot should hold 1 Helmet"),
			HelmetSlotBundle.Quantity, 1);

		// Verify that other tagged slots remain empty (Helmets should not be placed in universal or other specialized slots).
		LeftHandBundle = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
		RightHandBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestEqual(
			TEXT("Left hand slot should hold 1 Helmet"),
			LeftHandBundle.Quantity, 1);
		Res &= Test->TestEqual(
			TEXT("Right hand slot should hold 1 Helmet"),
			RightHandBundle.Quantity, 1);

		Res &= Test->TestFalse(
			TEXT("Chest slot should not contain a Helmet"),
			InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

		// Clear and add a spear
		InventoryComponent->Clear_IfServer();

		// Fill up inventory
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, 5);
		Res &= Test->TestEqual(
			TEXT("Should add 4 helmets to generic and 1 to helmet slot"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet), 4);
		Res &= Test->TestEqual(
			TEXT("Should add 4 helmets to generic and 1 to helmet slot"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).Quantity, 1);
		InventoryComponent->AddItem_IfServer(Subsystem, OneSpear);
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, 5); // generic
		int32 FinalAdded = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, 1); // can't add to any slot so should fail
		FinalAdded += InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, 1); // can't add to any slot so should fail
		Res &= Test->TestEqual(
			TEXT("Should not add additional helmets to any slot"), FinalAdded, 0);
		int32 QuantityOnlyContainer = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet);
		Res &= Test->TestEqual(
			TEXT("Should add 9 helmets to generic and 1 to helmet slot"),QuantityOnlyContainer, 9);
		Res &= Test->TestEqual(
			TEXT("Should still have 1 in helmet slot"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).Quantity, 1);
		Res &= Test->TestTrue(
			TEXT("Should have spear in right hand"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(
			TEXT("Left hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		
		return Res;
	}

	
	bool TestExclusiveUniversalSlots()
	{
		// Set up the test context with a limited capacity.
		InventoryComponentTestContext Context(20);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		// Add a spear to left hand and verify it fails as two handed weapons are exlusive to right hand, then add a spear to any hand and verify it gets added to right hand
		// finally remove the spear again, then add spear to anyslot with prefer tagged false and verify we can add to generic inventory and that both hands are empty

		int32 Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneSpear);
		Res &= Test->TestEqual(TEXT("Should not add a spear to left hand slot as its exclusive to right hand"), Added, 0);

		
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestFalse(TEXT("Should not add a spear to left hand slot as its exclusive to right hand"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestEqual(TEXT("Should add a spear to right hand slot"), Added, 1);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

		InventoryComponent->Clear_IfServer();
		
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= Test->TestFalse(TEXT("Should not add a spear to left hand slot as its exclusive to right hand"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestFalse(TEXT("Should not add a spear to right hand slot as we did not prefer tagged slots"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should contain a spear"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);

		return Res;
	}

	bool TestBlockingSlots()
	{
		// Set up the test context with a limited capacity.
		InventoryComponentTestContext Context(20);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		InventoryComponent->MaxSlotCount = 2; // restrict generic slots

		FDebugTestResult Res = true;

		// Test adding blocking item to right hand and verify we cant add to left hand
		// Test adding item to left hand and verify we cant add blocking item to right hand
		// Test for both adding to tagged slot/moving to tagged slot/adding to any slot

		// Add a blocking item to right hand
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear, true);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain a spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

		// Try adding a rock to left hand, should fail
		Res &= Test->TestTrue(TEXT("Can't add rock to left hand"), InventoryComponent->GetQuantityOfItemTaggedSlotCanReceive(LeftHandSlot, ItemIdRock) == 0);
		int32 Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		Res &= Test->TestEqual(TEXT("Should not add a rock to left hand slot"), Added, 0);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Try to add a rock to any slot preferring tagged, should add to generic
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add a rock to generic inventory"), Added, 1);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain a rock"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 1);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Add rock to generic inventory then try to move to left hand, should fail
		InventoryComponent->AddItem_IfServer(Subsystem, OneRock);
		Added = InventoryComponent->MoveItem(OneRock, FGameplayTag::EmptyTag, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move a rock to left hand slot"), Added, 0);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Now add a helmet to helmetslot and verify we can't move it to left hand
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Helmet slot should contain a helmet"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));
		int32 Moved = InventoryComponent->MoveItem(OneHelmet, FGameplayTag::EmptyTag, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move a helmet to left hand slot"), Moved, 0);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a helmet"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Now clear, add a rock to left hand, then verify we cant add a spear to right hand
		InventoryComponent->Clear_IfServer();
		// Verify clear will clear blocked state
		Res &= Test->TestFalse(TEXT("Left hand should not be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));
		
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		Res &= Test->TestTrue(TEXT("Can't add spear to right hand"), InventoryComponent->GetQuantityOfItemTaggedSlotCanReceive(RightHandSlot, ItemIdSpear) == 0);
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add a spear to generic inventory"), Added, 1);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain a spear"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Res &= Test->TestFalse(TEXT("Right hand slot should not contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		Added = InventoryComponent->MoveItem(ItemIdSpear, 1, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move a spear to right hand slot"), Added, 0);
		Res &= Test->TestFalse(TEXT("Right hand slot should not contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Left hand slot should still contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// We now want to add various ways of setting and clearing the blocked status
		InventoryComponent->Clear_IfServer();
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		// Verify it was added to right hand and that left hand is blocked
		Res &= Test->TestTrue(TEXT("Right hand should contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

		// try to add a rock to left hand, should fail
		Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		Res &= Test->TestEqual(TEXT("Should not add a rock to left hand slot"), Added, 0);
		Res &= Test->TestFalse(TEXT("Left hand slot should not contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		InventoryComponent->Clear_IfServer();
		// Add spear to generic inventory and move it to right hand and validate that will set blocked state
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= Test->TestEqual(TEXT("Should add a spear to generic inventory"), Added, 1);
		Res &= Test->TestTrue(TEXT("Generic inventory should contain a spear"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Added = InventoryComponent->MoveItem(ItemIdSpear, 1, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move a spear to right hand slot"), Added, 1);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

		
		InventoryComponent->Clear_IfServer();
		// Now test adding a spear by FILLING the generic inventory with spears, a rock to right hand and move the rock to the generic inventory, forcing a swap with a spear
		// Add 3 spears to inventory (maximum, 2 to generic, 1 to right hand)
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 3, EPreferredSlotPolicy::PreferGenericInventory);
		// verify hands are clear
		Res &= Test->TestTrue(TEXT("Right hand should have spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestFalse(TEXT("Left hand should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should contain two spears"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 2);
		
		// Remove spear from right hand and Add a rock to right hand
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::ForceDestroyed);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock, true);
		// Move the rock to generic inventory
		int32 MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 1, RightHandSlot, FGameplayTag::EmptyTag, ItemIdSpear, 1);
		// Verify rock is in generic inventory and spear is in right hand
		Res &= Test->TestEqual(TEXT("Should have moved 1 item"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Rock should be in generic inventory"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 1);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(TEXT("Left hand should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));


		InventoryComponent->Clear_IfServer();
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneRock, EPreferredSlotPolicy::PreferGenericInventory);
		// Remove spear and verify unblock
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::ForceDestroyed);
		Res &= Test->TestFalse(TEXT("Right hand should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestFalse(TEXT("Left hand should be unblocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));
		// Re-add spear and test again when moving spear instead of removing
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		InventoryComponent->MoveItem(ItemIdSpear, 1, RightHandSlot, FGameplayTag::EmptyTag);
		Res &= Test->TestFalse(TEXT("Right hand should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestFalse(TEXT("Left hand should be unblocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));
		
		return Res;
	}

	bool TestReceivableQuantity()
	{
		// Set up the test context with a limited capacity.
		InventoryComponentTestContext Context(20);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		InventoryComponent->MaxSlotCount = 2; // restrict generic slots

		FDebugTestResult Res = true;

		int32 ReceivableQuantity = InventoryComponent->GetReceivableQuantity(ItemIdSpear);
		Res &= Test->TestEqual(TEXT("Should be able to receive 3 spears, one righthand, two in generic"), ReceivableQuantity, 3);

		// Remove old right hand and add a new version that does not make two handed items exclusive to right hand
		int32 IndexToReplace = InventoryComponent->UniversalTaggedSlots.IndexOfByPredicate([&](const FUniversalTaggedSlot& Slot) { return Slot.Slot == RightHandSlot; });
		InventoryComponent->UniversalTaggedSlots[IndexToReplace] = FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, FGameplayTag());
		
		// Now verify we can still only receive 3 spears as adding a spear to each hand would violate blocking
		ReceivableQuantity = InventoryComponent->GetReceivableQuantity(ItemIdSpear);
		Res &= Test->TestEqual(TEXT("Should be able to receive 3 spears, one righthand, two in generic"), ReceivableQuantity, 3);
		
		return Res;
	}
	
	bool TestEventBroadcasting()
	{
	    InventoryComponentTestContext Context(20);
	    auto* InventoryComponent = Context.InventoryComponent;
	    auto* Subsystem = Context.TestFixture.GetSubsystem();
	    InventoryComponent->MaxSlotCount = 2;
	    FDebugTestResult Res = true;
	    UGlobalInventoryEventListener* Listener = NewObject<UGlobalInventoryEventListener>();
	    Listener->SubscribeToInventoryComponent(InventoryComponent);

	    int32 Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferAnyTaggedSlot);
	    Res &= Test->TestEqual(TEXT("Should add 5 rocks to a tagged slot"), Added, 5);
	    Res &= Test->TestTrue(TEXT("OnItemAdded event should trigger for rock addition"), Listener->bItemAddedToTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Previous item should be empty"),
	        (!Listener->AddedToTaggedPreviousItem.IsValid() ||
	         (Listener->AddedToTaggedPreviousItem.ItemId == FGameplayTag::EmptyTag && Listener->AddedToTaggedPreviousItem.Quantity == 0)));
	    Listener->bItemAddedTriggered = false;

	    InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdRock, 5, EItemChangeReason::Removed);
	    Res &= Test->TestFalse(TEXT("Right hand slot should be empty after removal"),
	                           InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	    Res &= Test->TestTrue(TEXT("OnItemRemoved event should trigger for rock removal"), Listener->bItemRemovedFromTaggedTriggered);
	    Listener->bItemRemovedTriggered = false;

	    Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
	    Res &= Test->TestEqual(TEXT("Should add 5 rocks to generic slots"), Added, 5);
	    Res &= Test->TestFalse(TEXT("Right hand slot should remain empty"),
	                           InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
	    Res &= Test->TestFalse(TEXT("Left hand slot should remain empty"),
	                           InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
	    Res &= Test->TestTrue(TEXT("OnItemAdded event should trigger for rock addition"), Listener->bItemAddedTriggered);

	    InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
	    Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 2, EPreferredSlotPolicy::PreferGenericInventory);
	    Res &= Test->TestEqual(TEXT("Should add 2 sticks after generic slots are full"), Added, 2);
	    Res &= Test->TestEqual(TEXT("First universal tagged slot (left hand) should contain 2 sticks"),
	                           InventoryComponent->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0].Slot).Quantity, 2);
	    Res &= Test->TestTrue(TEXT("OnItemAddedToTaggedSlot event should trigger for spilled sticks"), Listener->bItemAddedToTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Previous item for spilled sticks should be empty"),
	        (!Listener->AddedToTaggedPreviousItem.IsValid() ||
	         (Listener->AddedToTaggedPreviousItem.ItemId == FGameplayTag::EmptyTag && Listener->AddedToTaggedPreviousItem.Quantity == 0)));

	    InventoryComponent->MaxWeight = 25;
	    InventoryComponent->MoveItem(ItemIdRock, 5, FGameplayTag::EmptyTag, RightHandSlot);

	    InventoryComponent->Clear_IfServer();
	    Listener->Clear();

	    Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
	    Res &= Test->TestEqual(TEXT("Should add rocks to generic inventory"), Added, 5);
	    Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 3, EPreferredSlotPolicy::PreferGenericInventory);
	    Res &= Test->TestEqual(TEXT("Should add sticks to generic inventory"), Added, 3);
	    InventoryComponent->MoveItem(ItemIdRock, 3, FGameplayTag(), RightHandSlot);
	    Res &= Test->TestTrue(TEXT("Right hand slot should contain rocks"),
	                           InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

	    Listener->Clear();
	    InventoryComponent->MoveItem(ItemIdSticks, 2, FGameplayTag(), LeftHandSlot);
	    Res &= Test->TestTrue(TEXT("Event should fire for adding to tagged slot"), Listener->bItemAddedToTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Left hand slot should have received sticks"), Listener->AddedSlotTag == LeftHandSlot);
	    Res &= Test->TestTrue(TEXT("Added item should be sticks"), Listener->AddedToTaggedItemStaticData->ItemId == ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Correct quantity moved"), Listener->AddedToTaggedQuantity, 2);
	    Res &= Test->TestTrue(TEXT("Previous item should be empty"),
	        !Listener->AddedToTaggedPreviousItem.IsValid());

	    Listener->Clear();
	    InventoryComponent->MoveItem(ItemIdRock, 3, RightHandSlot, FGameplayTag());
	    Res &= Test->TestTrue(TEXT("Event should fire for removing from tagged slot"), Listener->bItemRemovedFromTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Removed slot should be RightHandSlot"), Listener->RemovedSlotTag == RightHandSlot);
	    Res &= Test->TestTrue(TEXT("Removed item should be rocks"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Correct quantity removed"), Listener->RemovedFromTaggedQuantity, 3);
	    Res &= Test->TestTrue(TEXT("Event should fire for adding to generic slots"), Listener->bItemAddedTriggered);
	    Res &= Test->TestTrue(TEXT("Added item should be rocks"), Listener->AddedItemStaticData->ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Correct quantity added to generic slots"), Listener->AddedQuantity, 3);

	    Listener->Clear();
	    InventoryComponent->MoveItem(ItemIdRock, 2, FGameplayTag(), LeftHandSlot, ItemIdSticks, 2);
	    Res &= Test->TestTrue(TEXT("Swap should trigger remove event for previous item in LeftHandSlot"), Listener->bItemRemovedFromTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Correct slot affected"), Listener->RemovedSlotTag == LeftHandSlot);
	    Res &= Test->TestTrue(TEXT("Correct item removed (sticks)"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdSticks);
	    Res &= Test->TestEqual(TEXT("Correct quantity removed"), Listener->RemovedFromTaggedQuantity, 2);
	    Res &= Test->TestTrue(TEXT("Event should fire for adding swapped item"), Listener->bItemAddedToTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Added slot should be LeftHandSlot"), Listener->AddedSlotTag == LeftHandSlot);
	    Res &= Test->TestTrue(TEXT("Added item should be rocks"), Listener->AddedToTaggedItemStaticData->ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Correct quantity moved"), Listener->AddedToTaggedQuantity, 2);
	    Res &= Test->TestTrue(TEXT("Previous item should be sticks"),
	        (Listener->AddedToTaggedPreviousItem.ItemId == ItemIdSticks && Listener->AddedToTaggedPreviousItem.Quantity == 2));

	    Listener->Clear();
	    InventoryComponent->MoveItem(ItemIdRock, 2, LeftHandSlot, FGameplayTag());
	    Res &= Test->TestTrue(TEXT("Event should fire for removing item from tagged slot"), Listener->bItemRemovedFromTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Left hand slot should be affected"), Listener->RemovedSlotTag == LeftHandSlot);
	    Res &= Test->TestTrue(TEXT("Removed item should be rocks"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Correct quantity removed"), Listener->RemovedFromTaggedQuantity, 2);
	    Res &= Test->TestFalse(TEXT("Left hand slot should now be empty"),
	                           InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

	    InventoryComponent->Clear_IfServer();
	    Listener->Clear();
	    int32 Moved = InventoryComponent->MoveItem(ItemIdHelmet, 1, FGameplayTag(), RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Should not move"), Moved, 0);
	    Res &= Test->TestFalse(TEXT("No events should fire"), Listener->bItemAddedTriggered || Listener->bItemRemovedTriggered);

	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 5);
	    int PartialMove = InventoryComponent->MoveItem(ItemIdRock, 99, FGameplayTag::EmptyTag, RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Should move only available quantity"), PartialMove, 5);
	    Res &= Test->TestTrue(TEXT("Event should fire for adding to tagged slot"), Listener->bItemAddedToTaggedTriggered);
	    Res &= Test->TestTrue(TEXT("Previous item for partial move should be empty"), !Listener->AddedToTaggedPreviousItem.IsValid());

	    return Res;
	}
	
	bool TestIndirectOperations()
	{
		InventoryComponentTestContext Context(99);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		InventoryComponent->MaxSlotCount = 9;
		FDebugTestResult Res = true;

		// Add a variety of items to tagged slots
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, ChestSlot, OneChestArmor, true);

		// Destroy from generic inventory (which contains duplicates) and verify they are removed from tagged as well
		InventoryComponent->DestroyItem_IfServer(OneRock, EItemChangeReason::ForceDestroyed);
		Res &= Test->TestFalse(TEXT("Left hand slot should not contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should not contain a rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 0);

		// Repeat for other items
		InventoryComponent->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 1, EItemChangeReason::ForceDestroyed);
		Res &= Test->TestFalse(TEXT("Right hand slot should not contain a knife"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should not contain a knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife) == 0);

		InventoryComponent->DestroyItem_IfServer(OneHelmet, EItemChangeReason::ForceDestroyed);
		Res &= Test->TestFalse(TEXT("Helmet slot should be empty after helmet destroy"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should not contain a helmet"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet) == 0);

		InventoryComponent->DestroyItem_IfServer(OneChestArmor, EItemChangeReason::ForceDestroyed);
		Res &= Test->TestFalse(TEXT("Chest slot should be empty after armor destroy"), InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should not contain chest armor"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdChestArmor) == 0);

		// Now try more complex cases
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdRock, 3);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 5);

		InventoryComponent->DestroyItem_IfServer(OneRock, EItemChangeReason::ForceDestroyed);
		int32 AmountContainedInBothHands = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity + InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity;
		Res &= Test->TestEqual(TEXT("The hands should now contain combined 7 rocks"), AmountContainedInBothHands, 7);
		Res &= Test->TestTrue(TEXT("Left hand should contain at least 2 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity >= 2);
		Res &= Test->TestTrue(TEXT("Right hand should contain at least 4 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity >= 4);

		// Clear
		InventoryComponent->Clear_IfServer();
		
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneHelmet, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);

		InventoryComponent->DestroyItem_IfServer(OneHelmet, EItemChangeReason::ForceDestroyed);
		// Verify we emptied righth or helmet slot. Ideally we should have emptied the hand slot before the specialized helmet slot but this is fine for now
		Res &= Test->TestTrue(TEXT("Right hand or helmet slot should be empty after helmet destroy"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid() || !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Right hand or helmet slot should contain a helmet"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet) || InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));
		
		return Res;
	}
};

bool FRancInventoryComponentTest::RunTest(const FString& Parameters)
{
	FDebugTestResult Res = true;
	FInventoryComponentTestScenarios TestScenarios(this);
	Res &= TestScenarios.TestAddingTaggedSlotItems();
	Res &= TestScenarios.TestAddItem();
	Res &= TestScenarios.TestAddItemToAnySlots();
	Res &= TestScenarios.TestAddStackableItems();
	Res &= TestScenarios.TestRemovingTaggedSlotItems();
	Res &= TestScenarios.TestRemoveAnyItemFromTaggedSlot();
	Res &= TestScenarios.TestMoveTaggedSlotItems();
	Res &= TestScenarios.TestMoveOperationsWithSwapback();
	Res &= TestScenarios.TestDroppingFromTaggedSlot();
	Res &= TestScenarios.TestCanCraftRecipe();
	Res &= TestScenarios.TestCraftRecipe();
	Res &= TestScenarios.TestInventoryMaxCapacity();
	Res &= TestScenarios.TestExclusiveUniversalSlots();
	Res &= TestScenarios.TestBlockingSlots();
	Res &= TestScenarios.TestReceivableQuantity();
	Res &= TestScenarios.TestEventBroadcasting();
	Res &= TestScenarios.TestIndirectOperations();

	return Res;
}
