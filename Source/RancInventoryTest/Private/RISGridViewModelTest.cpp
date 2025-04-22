// Copyright Rancorous Games, 2024

#include "RISGridViewModelTest.h" // header empty

#include "NativeGameplayTags.h"
#include "Components\InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "ViewModels\InventoryGridViewModel.h"
#include "RISInventoryTestSetup.cpp"
#include "Core/RISSubsystem.h"
#include "Framework/DebugTestResult.h"
#include "MockClasses/ItemHoldingCharacter.h"

#define TestNameGVM "GameTests.RIS.GridViewModel"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRISGridViewModelTest, TestNameGVM, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class GridViewModelTestContext
{
public:
	GridViewModelTestContext(float CarryCapacity, int32 NumSlots, bool PreferUniversalSlots)
		: TestFixture(FName(*FString(TestNameGVM)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		World = TestFixture.GetWorld();
		TempActor = World->SpawnActor<AItemHoldingCharacter>();
		InventoryComponent = NewObject<UInventoryComponent>(TempActor);
		TempActor->AddInstanceComponent(InventoryComponent);
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, ItemTypeTwoHanded));
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(LeftHandSlot, RightHandSlot,ItemTypeTwoHandedOffhand, ItemTypeOffHandOnly));
		InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);
		InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);
		InventoryComponent->MaxSlotCount = NumSlots;
		InventoryComponent->MaxWeight = CarryCapacity;
		InventoryComponent->RegisterComponent();

		ViewModel = NewObject<UInventoryGridViewModel>();
		ViewModel->InitializeInventory(InventoryComponent, NumSlots, PreferUniversalSlots);
		TestFixture.InitializeTestItems();
	}

	~GridViewModelTestContext()
	{
		if (TempActor)
		{
			TempActor->Destroy();
		}
	}

	FTestFixture TestFixture;
	UWorld* World;
	AActor* TempActor;
	UInventoryComponent* InventoryComponent;
	UInventoryGridViewModel* ViewModel;
};
	
class FGridViewModelTestScenarios
{
public:
	FRISGridViewModelTest* Test;
	
	FGridViewModelTestScenarios(FRISGridViewModelTest* InTest)
		: Test(InTest)
	{
	};


	bool TestInitializeViewModel()
	{
		GridViewModelTestContext Context(100, 9, false);
		auto* ViewModel = Context.ViewModel;
		
		FDebugTestResult Res = true;
	    
		// Test if the linked inventory component is correctly set
		Res &= Test->TestNotNull(TEXT("InventoryComponent should not be null after initialization"), ViewModel->LinkedInventoryComponent.Get());
	    
		// Test if the number of slots is correctly set
		Res &= Test->TestEqual(TEXT("ViewModel should have the correct number of slots"), ViewModel->NumberOfGridSlots, 9);

		// Verify that the slot mapper has correctly initialized empty slots
		for (int32 Index = 0; Index < ViewModel->NumberOfGridSlots; ++Index)
		{
			bool IsGridSlotEmpty = ViewModel->IsGridSlotEmpty(Index);
			Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be initialized as empty"), Index), IsGridSlotEmpty);
		}

		// Verify tagged slots are initialized and empty
		Res &= Test->TestTrue(TEXT("LeftHandSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("RightHandSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestTrue(TEXT("HelmetSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));
		Res &= Test->TestTrue(TEXT("ChestSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(ChestSlot));

		return Res;
	}

	bool TestReactionToInventoryEvents()
	{
	    GridViewModelTestContext Context(99.0f, 9, false);
	    auto* InventoryComponent = Context.InventoryComponent;
	    auto* ViewModel = Context.ViewModel;
	    auto* Subsystem = Context.TestFixture.GetSubsystem();
		
	    FDebugTestResult Res = true;
	    // Test adding items
	    InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks);
		Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks added to the first slot"), ViewModel->GetGridItem(0).Quantity, 5);
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 5);
		
	    // Test adding items to a tagged slot
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestEqual(TEXT("ViewModel should reflect the helmet added to the tagged slot"), ViewModel->GetItemForTaggedSlot(HelmetSlot).Quantity, 1);
	    
	    // Test removing items from a generic slot
	    InventoryComponent->DestroyItem_IfServer(FiveRocks, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("First slot should be empty after removing rocks"), ViewModel->IsGridSlotEmpty(0));
	    
	    // Test removing items from a tagged slot
	    InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(HelmetSlot, 1, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("HelmetSlot should be empty after removing the helmet"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));
	    
	    // Test adding more items to an existing stack
	    InventoryComponent->AddItem_IfServer(Subsystem, ThreeRocks);
	    InventoryComponent->AddItem_IfServer(Subsystem, TwoRocks);
		Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks added to the first slot again"), ViewModel->GetGridItem(0).Quantity, 5);
	    
	    // Test exceeding max stack
	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 10);
		Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestTrue(TEXT("ViewModel should handle exceeding max stack correctly"), ViewModel->GetGridItem(0).Quantity == 5 && ViewModel->GetGridItem(1).Quantity == 5 && ViewModel->GetGridItem(2).Quantity == 5);
	    
	    // Test partial removal of items
	    InventoryComponent->DestroyItem_IfServer(ThreeRocks, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 2 rocks remaining in first slot after partial removal"), ViewModel->GetGridItem(0).Quantity, 2);

		// The next 3 test blocks use ViewModel->MoveItems where i meant to use InventoryComponent->MoveItems_IfServer
		// but it ended up catching some bugs so i'll leave them here
		
	    // Test move 2 rocks from slot 0 to slot to hand making a full stack in hand
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeRocks);
	    ViewModel->MoveItem(NoTag, 0, LeftHandSlot, -1);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 5 rocks in LeftHandSlot and empty slot 0"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).Quantity == 5 && ViewModel->IsGridSlotEmpty(0));

		// Test moving item from a tagged slot to an empty generic slot
		ViewModel->MoveItem(LeftHandSlot, -1, NoTag, 0);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("After moving rocks from LeftHandSlot to slot 0, slot 0 should have 2 rocks"), ViewModel->GetGridItem(0).Quantity == 5 && ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestFalse(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		
		// Test splitting stack from tagged slot to empty generic slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, TwoRocks); // Reset LeftHandSlot with 2 rocks
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity, 2);
		ViewModel->SplitItem(LeftHandSlot, -1, NoTag, 3, 1); // Split 1 rock to an empty slot 3
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity, 1);
		Res &= Test->TestEqual(TEXT("After splitting, LeftHandSlot should have 1 rock"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).Quantity, 1);
		Res &= Test->TestEqual(TEXT("After splitting, slot 3 should have 1 rock"), ViewModel->GetGridItem(3).Quantity, 1);

		// Now lets actually use InventoryComponent->MoveItems_IfServer
		// We have 1 rock in LeftHandSlot, 1 rock in slot 3, 2 rocks in slot 0, 5 rocks in slot 1 and 2
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 16);
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity, 1);
		
	    // Test moving items from a generic slot to a tagged slot that is not empty
	    InventoryComponent->MoveItem(FiveRocks, NoTag, LeftHandSlot); // Move 4/5 rocks from container to LeftHandSlot
		// slot 0 should now have no rocks, slot 1 should have 2 rocks, LeftHandSlot should have 5 rocks
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks in LeftHandSlot"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).Quantity, 5);
		Res &= Test->TestEqual(TEXT("Slot 0 should have 1 rock left after moving 4 rocks to LeftHandSlot"), ViewModel->GetGridItem(0).Quantity, 1);

	    // Test moving items from a tagged slot to a generic slot when both have items
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, FiveRocks); // Ensure RightHandSlot has items for moving
	    InventoryComponent->MoveItem(FiveRocks, RightHandSlot, NoTag); // Move items from RightHandSlot to container
	    Res &= Test->TestEqual(TEXT("ViewModel should reflect moved items from RightHandSlot to slot 0"), ViewModel->GetGridItem(0).Quantity, 5);
	    Res &= Test->TestEqual(TEXT("ViewModel should reflect moved items from RightHandSlot to slot 3"), ViewModel->GetGridItem(3).Quantity, 2);
	    Res &= Test->TestTrue(TEXT("RightHandSlot should be empty after moving items to slot 0"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= ViewModel->AssertViewModelSettled();
		
	    return Res;
	}

	bool TestAddItemsToViewModel()
	{
		GridViewModelTestContext Context(15.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;
	    
		// Simulate adding a rock to the inventory
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeRocks);
		
		FItemBundle Item = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 3 rocks added to the first slot"), Item.ItemId == ItemIdRock && Item.Quantity == 3);

		InventoryComponent->AddItem_IfServer(Subsystem, ThreeRocks);
		Item = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 5 rocks added the first"), Item.ItemId == ItemIdRock && Item.Quantity == 5);
		Item = ViewModel->GetGridItem(1);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 1 rock added to the second slot"), Item.ItemId == ItemIdRock && Item.Quantity == 1);

		Res &= Test->TestTrue(TEXT("HelmetSlot should be empty"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));
		Res &= ViewModel->AssertViewModelSettled();
		
		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot);

		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the tagged slot"), ViewModel->IsTaggedSlotEmpty(HelmetSlot) == false);

		// Add another helmet, which should go to generic slots
		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot);
		Item = ViewModel->GetGridItem(2);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the third slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);

		// Add another helmet to hand slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet);
		auto TaggedItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the left hand slot"), TaggedItem.ItemId == ItemIdHelmet && TaggedItem.Quantity == 1);
		Res &= ViewModel->AssertViewModelSettled();
		
		return Res;
	}

	bool TestAddItemsToPartialStacks()
	{
		GridViewModelTestContext Context(99.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		// Add 5 sticks to slot 0, 3 sticks to slot 5 and 4 sticks to slot 8
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 12, EPreferredSlotPolicy::PreferGenericInventory);
		ViewModel->MoveItem(NoTag, 1, NoTag, 5);
		ViewModel->MoveItem(NoTag, 2, NoTag, 8);
		ViewModel->SplitItem(NoTag, 5, NoTag, 8, 2);
		// Now add sticks one at a time and ensure that they fill up in the order slot: 5, 8, 1
		InventoryComponent->AddItem_IfServer(Subsystem, OneStick);
		Res &= Test->TestTrue(TEXT("Slot 5 should have 4 sticks after adding 1 stick"), ViewModel->GetGridItem(5).Quantity == 4);
		InventoryComponent->AddItem_IfServer(Subsystem, OneStick);
		Res &= Test->TestTrue(TEXT("Slot 5 should have 5 sticks after adding 1 stick"), ViewModel->GetGridItem(5).Quantity == 5);
		Res &= Test->TestTrue(TEXT("Slot 0 should have 5 sticks unchanged"), ViewModel->GetGridItem(0).Quantity == 5);
		Res &= Test->TestTrue(TEXT("Slot 1 should have 0 unchanged"), ViewModel->GetGridItem(1).Quantity == 0);
		InventoryComponent->AddItem_IfServer(Subsystem, OneStick);
		Res &= Test->TestTrue(TEXT("Slot 8 should have 5 sticks after adding 1 stick"), ViewModel->GetGridItem(8).Quantity == 5);
		Res &= Test->TestTrue(TEXT("Slot 1 should have 0 unchanged"), ViewModel->GetGridItem(1).Quantity == 0);
		for (int i = 0; i < 5; i++)
		{
			InventoryComponent->AddItem_IfServer(Subsystem, OneStick);
			Res &= Test->TestTrue(TEXT("Slot 1 should have") + FString::FromInt(i + 1) + TEXT("sticks after adding 1 stick"), ViewModel->GetGridItem(1).Quantity == i + 1);
		}
		
		return Res;
	}

	bool TestMoveAndSwap()
	{
		GridViewModelTestContext Context(20.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;
	    
		// Add initial items to inventory for setup
		InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks);
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);

		// Move rock from slot 0 to slot 1, where sticks are, and expect them to swap
		ViewModel->MoveItem(NoTag, 0, NoTag, 1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot0AfterMove = ViewModel->GetGridItem(0);
		FItemBundle ItemInSlot1AfterMove = ViewModel->GetGridItem(1);
		Res &= Test->TestTrue(TEXT("Slot 0 should now contain sticks after swap"), ItemInSlot0AfterMove.ItemId == ItemIdSticks && ItemInSlot0AfterMove.Quantity == 3);
		Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap"), ItemInSlot1AfterMove.ItemId == ItemIdRock && ItemInSlot1AfterMove.Quantity == 5);

		// Add helmet to inventory and attempt to swap with rocks in slot 1
		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferGenericInventory);
		ViewModel->MoveItem(NoTag, 2, NoTag, 1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot1AfterHelmetSwap = ViewModel->GetGridItem(1);
		FItemBundle ItemInSlot2AfterHelmetSwap = ViewModel->GetGridItem(2);
		Res &= Test->TestTrue(TEXT("Slot 1 should now contain a helmet after swap"), ItemInSlot1AfterHelmetSwap.ItemId == ItemIdHelmet && ItemInSlot1AfterHelmetSwap.Quantity == 1);
		Res &= Test->TestTrue(TEXT("Slot 2 should now contain rocks after swap"), ItemInSlot2AfterHelmetSwap.ItemId == ItemIdRock && ItemInSlot2AfterHelmetSwap.Quantity == 5);

		// Test moving item 3 sticks from a generic slot to a universal tagged slot (LeftHandSlot) with 3 sticks
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeSticks);
		ViewModel->MoveItem(NoTag, 0, LeftHandSlot, -1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInLeftHandAfterMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 5 sticks after move"), ItemInLeftHandAfterMove.ItemId == ItemIdSticks && ItemInLeftHandAfterMove.Quantity == 5);
		// generic slots should contain the remaining 1 sticks
		Res &= Test->TestTrue(TEXT("Slot 0 should contain 1 stick after move"), ViewModel->GetGridItem(0).ItemId == ItemIdSticks && ViewModel->GetGridItem(0).Quantity == 1);

		if (Context.TestFixture.AreGameplayTagsCorrupt())
		{
			UE_LOG(LogTemp, Warning, TEXT("Gameplay tags are corrupt caused by weird unreal gameplay tag handling. Skipping further tests."));
			return true;
		}
		
		// move helmet from slot 1 to expectedly helmet slot
		ViewModel->MoveItemToAnyTaggedSlot(NoTag, 1);
		Res &= ViewModel->AssertViewModelSettled();
		// slot 1 should now be empty and helmet slot should have helmet
		Res &= Test->TestTrue(TEXT("Slot 1 should be empty after moving helmet to HelmetSlot"), ViewModel->IsGridSlotEmpty(1));
		Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 helmet after move"), ViewModel->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet && ViewModel->GetItemForTaggedSlot(HelmetSlot).Quantity == 1);
		
		// Test moving helmet to LeftHandSlot which would swap except Helmet slot can't hold the sticks in the hand so it should fail
	    ViewModel->MoveItem(HelmetSlot, -1, LeftHandSlot, -1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInLeftHandAfterHelmetMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
	    FItemBundle HelmetInHelmetSlotAfterMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestTrue(TEXT("LeftHandSlot should still  contain sticks after failed move"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSticks && ItemInLeftHandAfterHelmetMove.Quantity == 5);
		Res &= Test->TestTrue(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet && HelmetInHelmetSlotAfterMove.Quantity == 1);

		// Now try the same thing but other direction which should also fail as it would cause an invalid swap
		ViewModel->MoveItem(LeftHandSlot, -1, HelmetSlot, -1);
		Res &= ViewModel->AssertViewModelSettled();
		ItemInLeftHandAfterHelmetMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		HelmetInHelmetSlotAfterMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestTrue(TEXT("LeftHandSlot should still  contain sticks after failed move"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSticks && ItemInLeftHandAfterHelmetMove.Quantity == 5);
		Res &= Test->TestTrue(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet && HelmetInHelmetSlotAfterMove.Quantity == 1);
		
		
	    // Attempt to move a non-helmet item from generic slot HelmetSlot, which should fail and no swap occurs
	    ViewModel->MoveItem(NoTag, 2, HelmetSlot, -1); // Assuming rocks are at slot 2 now
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInHelmetSlotAfterInvalidMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    FItemBundle ItemInSlot2AfterInvalidMove = ViewModel->GetGridItem(2);
	    Res &= Test->TestTrue(TEXT("HelmetSlot should not accept non-helmet item, should remain helmet"), ItemInHelmetSlotAfterInvalidMove.ItemId == ItemIdHelmet && ItemInHelmetSlotAfterInvalidMove.Quantity == 1);
	    Res &= Test->TestTrue(TEXT("Slot 2 should remain unchanged after invalid move attempt"), ItemInSlot2AfterInvalidMove.ItemId == ItemIdRock && ItemInSlot2AfterInvalidMove.Quantity == 5);

	    // Move item from a universal tagged slot to a specialized tagged slot that accepts it
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, EItemChangeReason::Removed);
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneSpecialHelmet); // Override spear with another helmet
	    ViewModel->MoveItem(LeftHandSlot, -1, HelmetSlot, -1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInHelmetSlotAfterSwapBack = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    FItemBundle ItemInLeftHandAfterSwapBack = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 special helmet"), ItemInHelmetSlotAfterSwapBack.ItemId == ItemIdSpecialHelmet && ItemInHelmetSlotAfterSwapBack.Quantity == 1);
		Res &= Test->TestTrue(TEXT("LeftHandSlot should contain 1 helmet"), ItemInLeftHandAfterSwapBack.ItemId == ItemIdHelmet && ItemInLeftHandAfterSwapBack.Quantity == 1);
		
	    // Test moving item to an already occupied generic slot to ensure they swap
	    InventoryComponent->AddItem_IfServer(Subsystem, OneSpear); // Add spear to inventory
	    ViewModel->MoveItem(NoTag, 1, NoTag, 2); // Assuming spear is at slot 1 now, and rocks at slot 2
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot1AfterSwap = ViewModel->GetGridItem(1);
	    FItemBundle ItemInSlot2AfterSwap = ViewModel->GetGridItem(2);
	    Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap with spear"), ItemInSlot1AfterSwap.ItemId == ItemIdRock && ItemInSlot1AfterSwap.Quantity == 5);
	    Res &= Test->TestTrue(TEXT("Slot 2 should now contain the spear after swap"), ItemInSlot2AfterSwap.ItemId == ItemIdSpear && ItemInSlot2AfterSwap.Quantity == 1);

	    return Res;
	}

	bool TestSwappingMoves()
	{
		GridViewModelTestContext Context(999.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;
		
		// Now lets test some full inventory cases
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 10*5); // Fill up rest of generic inventory and both hands
		Res &= Test->TestTrue(TEXT("Left and Right hand should have rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));
		bool Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, 0, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as left hand is occupied and cannot be cleared"), Moved, false);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(
					TEXT("Right hand slot should still have a rock"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));
		Res &= Test->TestTrue(
			TEXT("Inventory should still contain 10 rocks"),
			InventoryComponent->GetContainedQuantity(ItemIdRock) == 10*5);

		// Now the other direction, rock -> righthand
		Moved = ViewModel->MoveItem( RightHandSlot, -1, FGameplayTag::EmptyTag, 0);
		Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as left hand is occupied and cannot be cleared"), Moved, false);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Right hand slot should still have a rock"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));
		Res &= ViewModel->AssertViewModelSettled();
		
		// Now remove rock from left hand and try again, rock and spear should swap
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, EItemChangeReason::Removed, true);
		Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, 0, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move spear to right hand slot"), Moved, true);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
			ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(TEXT("Left hand should be empty"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Inventory should now contain 9 rocks"),
			InventoryComponent->GetContainedQuantity(ItemIdRock) == 9*5);

		// Move spear back to generic inventory swapping with a rock
		Moved = ViewModel->MoveItem(RightHandSlot, -1, FGameplayTag::EmptyTag, 3);
		Res &= Test->TestEqual(TEXT("Should move spear to generic inventory"), Moved, true);
		Res &= Test->TestTrue(TEXT("Generic inventory should contain the spear"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Res &= Test->TestTrue(TEXT("Right hand should contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(TEXT("Right hand should contain 5 rocks"),
			ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && ViewModel->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		
		return Res;
	}

	bool TestSplitItems()
	{
	    GridViewModelTestContext Context(99, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
	    FDebugTestResult Res = true;
	    
	    // Add initial items to slots to prepare for split tests
	    InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks); // Added to first generic slot

	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet); // Added to HelmetSlot

	    // Valid split in generic slots
	    ViewModel->SplitItem(NoTag, 0, NoTag, 1, 2);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestEqual(TEXT("After splitting, first slot should have 3 rocks"), ViewModel->GetGridItem(0).Quantity, 3);
	    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 2 rocks"), ViewModel->GetGridItem(1).Quantity, 2);

	    // Invalid split due to insufficient quantity in source slot
	    ViewModel->SplitItem(NoTag, 0, NoTag, 1, 4); // Trying to split more than available
	    Res &= Test->TestEqual(TEXT("Attempt to split more rocks than available should fail"), ViewModel->GetGridItem(0).Quantity, 3);

	    // Split between a generic slot and a tagged slot
	    ViewModel->SplitItem(NoTag, 1, RightHandSlot, -1, 1); // Splitting 1 rock to RightHandSlot
	    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 1 rock"), ViewModel->GetGridItem(1).Quantity, 1);
	    FItemBundle RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestTrue(TEXT("RightHandSlot should now contain 1 rock"), RightHandItem.ItemId == ItemIdRock && RightHandItem.Quantity == 1);

	    // Invalid split to a different item type slot
	    ViewModel->SplitItem(RightHandSlot, -1, HelmetSlot, -1, 1); // Trying to split rock into helmet slot
	    RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot); // Re-fetch to check for changes
	    Res &= Test->TestTrue(TEXT("Attempting to split into a different item type slot should fail"), RightHandItem.Quantity == 1 && ViewModel->GetItemForTaggedSlot(HelmetSlot).Quantity == 1);

	    // Exceeding max stack size
	    InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 8); // Add more rocks to force a stack size check
		// First as a sanity check try to split from an empty slot
		Res &= !ViewModel->SplitItem(NoTag, 2, NoTag, 1, 2); // Attempt to overflow slot 1 with rocks
	    // Now try to split from a slot with items but would overflow
		ViewModel->SplitItem(NoTag, 1, NoTag, 0, 2); // Attempt to overflow slot 1 with rocks
	    Res &= Test->TestEqual(TEXT("Splitting that exceeds max stack size should fail"), ViewModel->GetGridItem(0).Quantity, 5); 
	    Res &= Test->TestEqual(TEXT("Splitting that exceeds max stack size should fail"), ViewModel->GetGridItem(1).Quantity, 3); 
	    // Attempt split from tagged to generic slot with valid quantities
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, FiveRocks);
	    ViewModel->SplitItem(LeftHandSlot, -1, NoTag, 1, 1); // Splitting 1 rock to a new generic slot
	    FItemBundle NewGenericSlotItem = ViewModel->GetGridItem(1);
	    Res &= Test->TestEqual(TEXT("After splitting from tagged to generic, new slot should contain 4 rocks total"), NewGenericSlotItem.Quantity, 4);
		FItemBundle LeftHandItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should now contain 4 rocks"), LeftHandItem.Quantity, 4);
		Res &= ViewModel->AssertViewModelSettled();
		
	    // split from generic to tagged slot
	    ViewModel->SplitItem(NoTag, 1, LeftHandSlot, -1, 1); // Attempt to add another helmet to HelmetSlot
	    FItemBundle ItemInLeftHandAfterSplit = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should now contain 5 rocks"), ItemInLeftHandAfterSplit.Quantity, 5);
		Res &= Test->TestEqual(TEXT("Slot 1 should now contain 3 rocks"), ViewModel->GetGridItem(1).Quantity, 3);
		
		// Status: lefthand 5 rocks, right hand 1 rock, slots 0, 5 rocks, slot 1 3 rocks, helmetSlot 1 helmet
		
	    // Split with empty/invalid indices and tags
		ViewModel->SplitItem(NoTag, 5, NoTag, 6, 1); // Invalid indices
		Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes"), ViewModel->IsGridSlotEmpty(5) && ViewModel->IsGridSlotEmpty(6));
	    ViewModel->SplitItem(NoTag, 10, NoTag, 11, 1); // Invalid indices
	    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes"), ViewModel->IsGridSlotEmpty(10) && ViewModel->IsGridSlotEmpty(11));

	    ViewModel->SplitItem(NoTag, -1, ChestSlot, -1, 1); // Invalid source tag
	    Res &= Test->TestTrue(TEXT("Invalid source tag should result in no changes"), ViewModel->IsTaggedSlotEmpty(ChestSlot));

	    // Attempt to split into a slot with a different item type
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, EItemChangeReason::Removed, true);
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneStick); // Adding spear to RightHandSlot
	    ViewModel->SplitItem(NoTag, 0, RightHandSlot, -1, 1); // Attempt to split rock into RightHandSlot containing a stick
	    RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestTrue(TEXT("Attempting to split into a slot with a different item type should fail"), RightHandItem.ItemId == ItemIdSticks && RightHandItem.Quantity == 1);
		Res &= Test->TestTrue(TEXT("Source slot should remain unchanged after failed split"), ViewModel->GetGridItem(0).Quantity == 5);
		
	    return Res;
	}

	bool TestMoveItemToAnyTaggedSlot()
	{
	    GridViewModelTestContext Context(25, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
	    FDebugTestResult Res = true;
	    
	    // Add a mix of items to test moving to tagged slots
	    InventoryComponent->AddItemToAnySlot(Subsystem, ThreeRocks,  EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet,EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear,EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneChestArmor, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		
		if (Context.TestFixture.AreGameplayTagsCorrupt())
		{
			UE_LOG(LogTemp, Warning, TEXT("Gameplay tags are corrupt caused by weird unreal gameplay tag handling. Skipping further tests."));
			return true;
		}
		
	    // Move rock to any tagged slot (should go to a universal slot)
	    Res &= Test->TestTrue(TEXT("Move rock to any tagged slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
	    Res &= Test->TestTrue(TEXT("Rock should be in the first universal tagged slot, right hand"), ViewModel->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0].Slot).ItemId == ItemIdRock);
		Res &= ViewModel->AssertViewModelSettled();
		
	    // Move helmet to its specialized slot
	    Res &= Test->TestTrue(TEXT("Move helmet to its specialized slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 1));
	    Res &= Test->TestTrue(TEXT("Helmet should be in HelmetSlot"), ViewModel->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
		Res &= ViewModel->AssertViewModelSettled();
		
	    // Move spear to any tagged slot (should go to a universal slot)
	    Res &= Test->TestTrue(TEXT("Move spear to any tagged slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 2));
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Spear should be in right hand tagged slot"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);
		int32 SlotWithRock = ViewModel->GetGridItem(0).ItemId == ItemIdRock ? 0 : 2; // Rock should have been either swapped or moved to 0
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0"), ViewModel->GetGridItem(SlotWithRock).ItemId == ItemIdRock);
		Res &= ViewModel->AssertViewModelSettled();
		
	    // Attempt to move item that is already in its correct tagged slot (helmet), should result in no action
	    Res &= Test->TestFalse(TEXT("Attempting to move helmet already in HelmetSlot should do nothing"), ViewModel->MoveItemToAnyTaggedSlot(HelmetSlot, -1));
		Res &= ViewModel->AssertViewModelSettled();
		
		if (Context.TestFixture.AreGameplayTagsCorrupt())
		{
			UE_LOG(LogTemp, Warning, TEXT("Gameplay tags are corrupt caused by weird unreal gameplay tag handling. Skipping further tests."));
			return true;
		}
		
	    // Move chest armor to its specialized slot
	    Res &= Test->TestTrue(TEXT("Move chest armor to its specialized slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 3));	
	    Res &= Test->TestTrue(TEXT("Chest armor should be in ChestSlot"), !ViewModel->IsTaggedSlotEmpty(ChestSlot));

	    // Attempt to move an item to a tagged slot when all suitable slots are occupied
	    Res &= Test->TestFalse(TEXT("Attempt to move extra rock to tagged should fail as no slots are available"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, SlotWithRock));
		
	    InventoryComponent->AddItem_IfServer(Subsystem, OneSpecialHelmet); // goes to slot 0 or 1
		int32 SlotWithSpecialHelmet = ViewModel->GetGridItem(0).ItemId == ItemIdSpecialHelmet ? 0 : 1;
		Res &= Test->TestTrue(TEXT("A different helmet should swap into the helmet slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, SlotWithSpecialHelmet));
		Res &= Test->TestTrue(TEXT("Special helmet should be in HelmetSlot"), ViewModel->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdSpecialHelmet);
		Res &= Test->TestTrue(TEXT("Helmet should be in generic slot 0"), ViewModel->GetGridItem(SlotWithSpecialHelmet).ItemId == ItemIdHelmet);

	    // Attempt to move an item to a tagged slot when the source index is invalid
	    Res &= Test->TestFalse(TEXT("Attempting to move item from invalid source index should fail"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 100));

	    // Attempt to move an item from a tagged slot that is empty
	    Res &= Test->TestFalse(TEXT("Attempting to move item from an empty tagged slot should fail"), ViewModel->MoveItemToAnyTaggedSlot(ChestSlot, -1));
		Res &= ViewModel->AssertViewModelSettled();

		// Clear, move rock to one hand then movespear to any slot and verify it unequips rock automatically
		InventoryComponent->Clear_IfServer();
		for (int i = 0; i < 9; i++)
		{
			Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be empty"), i), ViewModel->IsGridSlotEmpty(i));
		}
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);
		// verify spear in left hand, nothing in right hand and rock in generic slot
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0"), ViewModel->GetGridItem(0).ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		
		// Rock in left hand
		InventoryComponent->Clear_IfServer();
		// Check that all generic and tagged slots are empty
		for (int i = 0; i < 9; i++)
		{
			Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be empty"), i), ViewModel->IsGridSlotEmpty(i));
		}
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);
		// verify spear in right hand, nothing in left hand and rock in generic slot
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0 or 1"), ViewModel->GetGridItem(0).ItemId == ItemIdRock || ViewModel->GetGridItem(1).ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		// Now verify the same again but that the spear will replace the rock when first picked up from a world item
		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock);
		// Creat a world item
		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundleWithInstanceData(OneSpear), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);

		Res &= Test->TestTrue(TEXT("Rock should still be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be generic slot 0 or 1"), ViewModel->GetGridItem(0).ItemId == ItemIdSpear || ViewModel->GetGridItem(1).ItemId == ItemIdSpear);
		// Logic was changed so we dont want a pickup to move items out
		//Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0 or 1"), ViewModel->GetGridItem(0).ItemId == ItemIdRock);
		//Res &= Test->TestTrue(TEXT("Spear should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);
		//Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		// Test a previous duplication issue. Add spear to right hand and 2 sticks to generic inventory, then move sticks to right hand then to left hand
		InventoryComponent->Clear_IfServer();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear);
		InventoryComponent->AddItem_IfServer(Subsystem, ItemIdSticks, 2);
		Res &= ViewModel->MoveItem(NoTag, 0, RightHandSlot, -1);
		Res &= Test->TestTrue(TEXT("Sticks should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSticks && ViewModel->GetItemForTaggedSlot(RightHandSlot).Quantity == 2);
		Res &= Test->TestTrue(TEXT("Spear should be in slot 0"), ViewModel->GetGridItem(0).ItemId == ItemIdSpear);
		Res &= ViewModel->MoveItem(RightHandSlot, -1, LeftHandSlot, -1);
		Res &= Test->TestTrue(TEXT("Sticks should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdSticks && ViewModel->GetItemForTaggedSlot(LeftHandSlot).Quantity == 2);
		Res &= Test->TestTrue(TEXT("Spear should be in slot 0"), ViewModel->GetGridItem(0).ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		
	    return Res;
	}

	bool TestMakeshiftWeapons()
	{
		GridViewModelTestContext Context(50.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Add makeshift weapons to the inventory
		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundleWithInstanceData(ItemIdBrittleCopperKnife, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);

		// Since rock is a makeshift weapon, it should add itself to right hand
		Res &= Test->TestTrue(TEXT("Knife should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdBrittleCopperKnife);

		// Add another makeshift weapon to the inventory
		WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundleWithInstanceData(ItemIdBrittleCopperKnife, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);

		// Since stick is a dual wieldable makeshift weapon, it should add itself to left hand
		Res &= Test->TestTrue(TEXT("Second knife should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdBrittleCopperKnife);

		// Test that a spear will move itself to the right hand
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);

		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), ViewModel->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= ViewModel->AssertViewModelSettled();
		
		return Res;
	}

	bool TestLeftHandHeldBows()
	{
		// We have two bows, shortbow and longbow. Both can only be held in offhand, but only the longbow is a two-handed weapon.
		
		GridViewModelTestContext Context(50.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Add the shortbow to the inventory
		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundleWithInstanceData(ItemIdShortbow, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);

		// Should add itself to the left hand
		Res &= Test->TestTrue(TEXT("Shortbow should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdShortbow);

		// Ensure the right hand is empty but that the right hand is not blocked
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestFalse(TEXT("Right hand should not be blocked"), InventoryComponent->IsTaggedSlotBlocked(RightHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should not be blocked"), ViewModel->CanTaggedSlotReceiveItem(OneRock, RightHandSlot));
		
		// Try (and fail) to move the bow to the right hand
		Res &= Test->TestFalse(TEXT("Should not be able to move shortbow to right hand"), ViewModel->MoveItemToAnyTaggedSlot(LeftHandSlot, -1));
		Res &= Test->TestTrue(TEXT("Shortbow should still be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdShortbow);

		// Move it to generic and then move it to anytaggedslot and verify it moves to left hand
		Res &= Test->TestTrue(TEXT("Move shortbow to generic slot"), ViewModel->MoveItem(LeftHandSlot, -1, NoTag, 0));
		Res &= Test->TestTrue(TEXT("Shortbow should be in generic slot 0"), ViewModel->GetGridItem(0).ItemId == ItemIdShortbow);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		Res &= Test->TestTrue(TEXT("Move shortbow to left hand"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
		Res &= Test->TestTrue(TEXT("Shortbow should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdShortbow);

		// Now remove the shortbow and add the longbow
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, EItemChangeReason::Removed, true);
		Res &= ViewModel->AssertViewModelSettled();
		WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundleWithInstanceData(ItemIdLongbow, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();

		Res &= Test->TestTrue(TEXT("Longbow should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdLongbow);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(RightHandSlot));
		Res &= Test->TestFalse(TEXT("Right hand should be blocked"), ViewModel->CanTaggedSlotReceiveItem(OneRock, RightHandSlot));

		Res &= Test->TestFalse(TEXT("Should not be able to move longbow to right hand"), ViewModel->MoveItemToAnyTaggedSlot(LeftHandSlot, -1));
		Res &= Test->TestTrue(TEXT("Longbow should still be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdLongbow);

		// Now move the longbow to generic and then move it to anytaggedslot and verify it moves to left hand and unblocking as needed
		InventoryComponent->MoveItem(ItemIdLongbow, 1, LeftHandSlot, NoTag);
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock); // indirectly blocking two-handed items
		Res &= Test->TestTrue(TEXT("Move longbow to any slot, which should be left. It should move blocking rock."), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));

		Res &= Test->TestTrue(TEXT("Longbow should be in left hand"), ViewModel->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdLongbow);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestTrue(TEXT("Generic slot should contain the rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 1);
		Res &= ViewModel->AssertViewModelSettled();
		
		return Res;
	}

	bool TestSlotReceiveItem()
	{
	    GridViewModelTestContext Context(10.0f, 5, false); // Setup with max weight capacity of 50
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
	    FDebugTestResult Res = true;
	    
	    // Adding item to an empty slot
	    Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), ViewModel->CanGridSlotReceiveItem(ThreeRocks, 0));

		// Adding more of that item up to maxstack for the same slot
		Res &= Test->TestTrue(TEXT("Can add more rocks to slot with same item type"), ViewModel->CanGridSlotReceiveItem(TwoRocks, 0));
		InventoryComponent->AddItem_IfServer(Subsystem, TwoRocks); 
		
	    // Trying to add item to a slot with a different item type
	    Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), ViewModel->CanGridSlotReceiveItem(OneHelmet, 0));

		// Add helmet to a different slot
		Res &= Test->TestTrue(TEXT("Can add helmet to a different slot"), ViewModel->CanGridSlotReceiveItem(OneHelmet, 1));
		
	    // SAdding item exceeding max stack size
	    Res &= Test->TestFalse(TEXT("Cannot add rocks exceeding max stack size"), ViewModel->CanGridSlotReceiveItem(FiveRocks, 0));

	    // Adding item to an out-of-bounds slot
	    Res &= Test->TestFalse(TEXT("Cannot add item to an out-of-bounds slot"), ViewModel->CanGridSlotReceiveItem(ThreeRocks, 10));

	    // Weight based test, add a giant boulder with weight 10
	    Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), ViewModel->CanGridSlotReceiveItem(GiantBoulder, 1));

		// Testing CanTaggedSlotReceiveItem
		Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), ViewModel->CanTaggedSlotReceiveItem(ThreeRocks, LeftHandSlot));
		Res &= Test->TestTrue(TEXT("Cannot add rocks to helmet slot"), ViewModel->CanTaggedSlotReceiveItem(ThreeRocks, HelmetSlot) == false);
		Res &= Test->TestTrue(TEXT("Can add helmet to a matching specialized slot"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, HelmetSlot));
		Res &= Test->TestTrue(TEXT("Can add helmet to a universal slot"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, FiveRocks);
		Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
		Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), ViewModel->CanTaggedSlotReceiveItem(GiantBoulder, RightHandSlot));
		
	    return Res;
	}

	bool TestDrop()
	{
		GridViewModelTestContext Context(100.0f, 9, false); // Setup with max weight capacity of 50
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;
		
		// Add 9 spears to generic inventory
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 9, EPreferredSlotPolicy::PreferGenericInventory);
		// Now drop slot index 8 and verify only 8 is removed
		ViewModel->DropItemFromGrid(8, 1);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestEqual(TEXT("After dropping 1 spear, there should be 8 spears left"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear), 8);
		Res &= Test->TestTrue(TEXT("Slot 8 should be empty after dropping 1 spear"), ViewModel->IsGridSlotEmpty(8));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Res &= Test->TestFalse(FString::Printf(TEXT("Slot %d should not be empty"), Index), ViewModel->IsGridSlotEmpty(Index));
		}

		return Res;
	}
};

	
bool FRISGridViewModelTest::RunTest(const FString& Parameters)
{
	FDebugTestResult Res = true;
	FGridViewModelTestScenarios TestScenarios(this);
	Res &= TestScenarios.TestInitializeViewModel();
	Res &= TestScenarios.TestReactionToInventoryEvents();
	Res &= TestScenarios.TestAddItemsToViewModel();
	Res &= TestScenarios.TestAddItemsToPartialStacks();
	Res &= TestScenarios.TestMoveAndSwap();
	Res &= TestScenarios.TestSwappingMoves();
	Res &= TestScenarios.TestSplitItems();
	Res &= TestScenarios.TestMoveItemToAnyTaggedSlot();
	Res &= TestScenarios.TestMakeshiftWeapons();
	Res &= TestScenarios.TestLeftHandHeldBows();
	Res &= TestScenarios.TestSlotReceiveItem();
	Res &= TestScenarios.TestDrop();

	return Res;
}