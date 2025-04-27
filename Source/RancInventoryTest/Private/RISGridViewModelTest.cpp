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
		TestFixture.InitializeTestItems(); // Ensures items, including Egg, are available
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

// Helper to compare instance data arrays (by pointer)
bool CompareInstanceArrays(FRISGridViewModelTest* Test, const FString& Context, const TArray<UItemInstanceData*>& ArrayA, const TArray<UItemInstanceData*>& ArrayB)
{
	if (ArrayA.Num() != ArrayB.Num())
	{
		Test->AddError(FString::Printf(TEXT("%s: Instance array counts differ (%d vs %d)"), *Context, ArrayA.Num(), ArrayB.Num()));
		return false;
	}
	for (int32 i = 0; i < ArrayA.Num(); ++i)
	{
		if (ArrayA[i] != ArrayB[i])
		{
			// Using TestTrue format for pointer comparison
			bool bMatch = ArrayA[i] == ArrayB[i];
			Test->TestTrue(FString::Printf(TEXT("%s: Instance pointer mismatch at index %d"), *Context, i), bMatch);
			if (!bMatch) return false; // Return on first mismatch for brevity
		}
	}
	return true;
}
	
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
	    
		Res &= Test->TestNotNull(TEXT("InventoryComponent should not be null after initialization"), ViewModel->LinkedInventoryComponent.Get());
		Res &= Test->TestEqual(TEXT("ViewModel should have the correct number of slots"), ViewModel->NumberOfGridSlots, 9);

		for (int32 Index = 0; Index < ViewModel->NumberOfGridSlots; ++Index)
		{
			bool IsGridSlotEmpty = ViewModel->IsGridSlotEmpty(Index);
			Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be initialized as empty"), Index), IsGridSlotEmpty);
            // Check instance data is empty
            FItemBundle Item = ViewModel->GetGridItem(Index);
            Res &= Test->TestEqual(FString::Printf(TEXT("Slot %d instance data should be empty on init"), Index), Item.InstanceData.Num(), 0);
		}

		Res &= Test->TestTrue(TEXT("LeftHandSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("RightHandSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestTrue(TEXT("HelmetSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));
		Res &= Test->TestTrue(TEXT("ChestSlot should be initialized and empty"), ViewModel->IsTaggedSlotEmpty(ChestSlot));
        // Check tagged instance data
        FItemBundle TaggedItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestEqual(TEXT("LeftHandSlot instance data empty on init"), TaggedItem.InstanceData.Num(), 0);
        TaggedItem = ViewModel->GetItemForTaggedSlot(HelmetSlot);
        Res &= Test->TestEqual(TEXT("HelmetSlot instance data empty on init"), TaggedItem.InstanceData.Num(), 0);

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
		InventoryComponent->AddItemToAnySlot(Subsystem, FiveRocks);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks added to the first slot"), ItemSlot0.Quantity, 5);
		Res &= Test->TestEqual(TEXT("Inventory component should match ViewModel"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 5);

		// Test adding items to a tagged slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemHelmetSlot = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect the helmet added to the tagged slot"), ItemHelmetSlot.Quantity, 1);

		// Test removing items from a generic slot
		InventoryComponent->DestroyItem_IfServer(FiveRocks, FItemBundle::NoInstances, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("First slot should be empty after removing rocks"), ViewModel->IsGridSlotEmpty(0));

		// Test removing items from a tagged slot
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(HelmetSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("HelmetSlot should be empty after removing the helmet"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));

		// Test adding more items to an existing stack
		InventoryComponent->AddItemToAnySlot(Subsystem, ThreeRocks);
		InventoryComponent->AddItemToAnySlot(Subsystem, TwoRocks);
		Res &= ViewModel->AssertViewModelSettled();
		ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks added to the first slot again"), ItemSlot0.Quantity, 5);

		// Test exceeding max stack
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 10);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemSlot1 = ViewModel->GetGridItem(1);
		FItemBundle ItemSlot2 = ViewModel->GetGridItem(2);
		Res &= Test->TestTrue(TEXT("ViewModel should handle exceeding max stack correctly"), ItemSlot0.Quantity == 5 && ItemSlot1.Quantity == 5 && ItemSlot2.Quantity == 5);

		// Test partial removal of items
		InventoryComponent->DestroyItem_IfServer(ThreeRocks, FItemBundle::NoInstances, EItemChangeReason::Removed); // Removes from slot 0
		Res &= ViewModel->AssertViewModelSettled();
		ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 2 rocks remaining in first slot after partial removal"), ItemSlot0.Quantity, 2);

		// Test moving items from a generic slot to a tagged slot
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeRocks);
		Res &= ViewModel->AssertViewModelSettled();
		// Use InventoryComponent move, VM should react
		InventoryComponent->MoveItem(TwoRocks, FItemBundle::NoInstances, NoTag, LeftHandSlot); // Move 2 rocks from slot 0
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemLeftHand = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks in LeftHandSlot"), ItemLeftHand.Quantity, 5);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect empty slot 0"), ViewModel->IsGridSlotEmpty(0));

		// Test moving item from a tagged slot to an empty generic slot
		InventoryComponent->MoveItem(FiveRocks, FItemBundle::NoInstances, LeftHandSlot, NoTag); // Move 5 rocks back
		Res &= ViewModel->AssertViewModelSettled();
		ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("After moving rocks from LeftHandSlot to slot 0, slot 0 should have 5 rocks"), ItemSlot0.Quantity, 5);
		Res &= Test->TestTrue(TEXT("LeftHandSlot should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		// Test splitting stack from tagged slot to empty generic slot - Via Component
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, TwoRocks); // Reset LeftHandSlot with 2 rocks
		Res &= ViewModel->AssertViewModelSettled();
		// We can't directly "split" via the component, only move a quantity. Let's move 1.
		InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, LeftHandSlot, NoTag); // Move 1 rock to generic
		Res &= ViewModel->AssertViewModelSettled();
		ItemLeftHand = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		FItemBundle ItemSlot3 = ViewModel->GetGridItem(3); // Should land in first available empty slot (3)
		Res &= Test->TestEqual(TEXT("After moving 1, LeftHandSlot should have 1 rock"), ItemLeftHand.Quantity, 1);
		Res &= Test->TestEqual(TEXT("After moving 1, slot 3 should have 1 rock"), ItemSlot3.Quantity, 1);
		Res &= Test->TestEqual(TEXT("Inventory component container should have 16 rocks"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 16);
		Res &= Test->TestEqual(TEXT("Inventory component tagged should have 1 rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity, 1);

		// Test moving items from a generic slot to a tagged slot that is not empty
		// slot 0 = 5, slot 1 = 5, slot 2 = 5, slot 3 = 1, lefthand = 1
		InventoryComponent->MoveItem(FiveRocks, FItemBundle::NoInstances, NoTag, LeftHandSlot); // Move from slot 0 (4 rocks)
		Res &= ViewModel->AssertViewModelSettled();
		ItemLeftHand = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect 5 rocks in LeftHandSlot"), ItemLeftHand.Quantity, 5);
		Res &= Test->TestEqual(TEXT("Slot 0 should have 1 rock left after moving 4 rocks to LeftHandSlot"), ItemSlot0.Quantity, 1);

		// Test moving items from a tagged slot to a generic slot when both have items
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, FiveRocks); // Ensure RightHandSlot has items for moving
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->MoveItem(FiveRocks, FItemBundle::NoInstances, RightHandSlot, NoTag); // Move items from RightHandSlot to container
		Res &= ViewModel->AssertViewModelSettled();
		ItemSlot0 = ViewModel->GetGridItem(0); // Slot 0 had 1, receives 4 -> 5
		ItemSlot3 = ViewModel->GetGridItem(3); // Slot 3 had 1, receives 1 -> 2
		Res &= Test->TestEqual(TEXT("ViewModel should reflect moved items from RightHandSlot to slot 0"), ItemSlot0.Quantity, 5);
		Res &= Test->TestEqual(TEXT("ViewModel should reflect moved items from RightHandSlot to slot 3"), ItemSlot3.Quantity, 2);
		Res &= Test->TestTrue(TEXT("RightHandSlot should be empty after moving items to slot 0"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));


		// --- Instance Data Event Tests ---
		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();

		// Add knife via component
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle GridKnife = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Grid slot 0 has knife"), GridKnife.ItemId == ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Grid slot 0 instance count"), GridKnife.InstanceData.Num(), 1);
		UItemInstanceData* InstancePtr = GridKnife.InstanceData.Num() > 0 ? GridKnife.InstanceData[0] : nullptr;
		Res &= Test->TestNotNull(TEXT("[Instance][Event] Instance pointer valid"), InstancePtr);

		// Move knife to tagged via component
		InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, {InstancePtr}, NoTag, RightHandSlot);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("[Instance][Event] Grid slot 0 empty after move"), ViewModel->IsGridSlotEmpty(0));
		FItemBundle TaggedKnife = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Right hand has knife after move"), TaggedKnife.ItemId == ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Right hand instance count"), TaggedKnife.InstanceData.Num(), 1);
		if (TaggedKnife.InstanceData.Num() == 1)
		{
			Res &= Test->TestTrue(TEXT("[Instance][Event] Right hand instance pointer correct"), TaggedKnife.InstanceData[0] == InstancePtr);
		}

		// Remove knife from tagged via component
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, {InstancePtr}, EItemChangeReason::Removed, false, true); // Destroy
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("[Instance][Event] Right hand empty after removal"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestEqual(TEXT("[Instance][Event] Right hand instance count zero"), ViewModel->GetItemForTaggedSlot(RightHandSlot).InstanceData.Num(), 0);

		// Add stackable instances via component
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 2, EPreferredSlotPolicy::PreferGenericInventory); // Add 2 eggs
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle GridEggs = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Grid slot 0 has eggs"), GridEggs.ItemId == ItemIdBrittleEgg);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Grid slot 0 has 2 eggs"), GridEggs.Quantity, 2);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Grid slot 0 has 2 instances"), GridEggs.InstanceData.Num(), 2);
		UItemInstanceData* EggInstanceA = GridEggs.InstanceData[0];
		UItemInstanceData* EggInstanceB = GridEggs.InstanceData[1];

		// Remove one egg instance via component (Use)
		InventoryComponent->UseItem(ItemIdBrittleEgg); // Use and destroy
		Res &= ViewModel->AssertViewModelSettled();
		GridEggs = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Grid slot 0 has 1 egg after use"), GridEggs.Quantity, 1);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Grid slot 0 has 1 instance after use"), GridEggs.InstanceData.Num(), 1);
		// Check which instance remains (depends on UseItem implementation - assume it removes last)
		if (GridEggs.InstanceData.Num() == 1)
		{
			Res &= Test->TestTrue(TEXT("[Instance][Event] Remaining instance should be A"), GridEggs.InstanceData[0] == EggInstanceA);
		}

		return Res;
	}

	bool TestAddItemsToViewModel()
	{
		GridViewModelTestContext Context(15.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		InventoryComponent->AddItemToAnySlot(Subsystem, ThreeRocks);
		FItemBundle Item = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 3 rocks added to the first slot"), Item.ItemId == ItemIdRock && Item.Quantity == 3);

		InventoryComponent->AddItemToAnySlot(Subsystem, ThreeRocks);
		Item = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 5 rocks added the first"), Item.ItemId == ItemIdRock && Item.Quantity == 5);
		Item = ViewModel->GetGridItem(1);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect 1 rock added to the second slot"), Item.ItemId == ItemIdRock && Item.Quantity == 1);

		Res &= Test->TestTrue(TEXT("HelmetSlot should be empty"), ViewModel->IsTaggedSlotEmpty(HelmetSlot));
		Res &= ViewModel->AssertViewModelSettled();

		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot);
		FItemBundle HelmetItem = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the tagged slot"), !ViewModel->IsTaggedSlotEmpty(HelmetSlot));
        Res &= Test->TestTrue(TEXT("Helmet item ID correct"), HelmetItem.ItemId == ItemIdHelmet);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot);
		Item = ViewModel->GetGridItem(2);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the third slot"), Item.ItemId == ItemIdHelmet && Item.Quantity == 1);

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet);
		FItemBundle TaggedItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("ViewModel should reflect the helmet added to the left hand slot"), TaggedItem.ItemId == ItemIdHelmet && TaggedItem.Quantity == 1);
		Res &= ViewModel->AssertViewModelSettled();

        // Instance Data Tests
        InventoryComponent->Clear_IfServer();
        Res &= ViewModel->AssertViewModelSettled();

        // Add Knife to grid
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridKnife = ViewModel->GetGridItem(0);
        Res &= Test->TestTrue(TEXT("[Instance] Grid slot 0 has knife"), GridKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Grid slot 0 has 1 instance"), GridKnife.InstanceData.Num(), 1);

        // Add Eggs to grid (stackable instances)
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 2);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridEggs = ViewModel->GetGridItem(1);
        Res &= Test->TestTrue(TEXT("[Instance] Grid slot 1 has eggs"), GridEggs.ItemId == ItemIdBrittleEgg);
        Res &= Test->TestEqual(TEXT("[Instance] Grid slot 1 has quantity 2"), GridEggs.Quantity, 2);
        Res &= Test->TestEqual(TEXT("[Instance] Grid slot 1 has 2 instances"), GridEggs.InstanceData.Num(), 2);

        // Add Knife to Tagged Slot
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle TaggedKnife = ViewModel->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] Right hand has knife"), TaggedKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Right hand has 1 instance"), TaggedKnife.InstanceData.Num(), 1);


		return Res;
	}

	bool TestAddItemsToPartialStacks()
	{
		GridViewModelTestContext Context(99.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 12, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		// Should be 5, 5, 2 in slots 0, 1, 2
		ViewModel->MoveItem(NoTag, 1, NoTag, 5); // Move 5 from slot 1 -> 5
		Res &= ViewModel->AssertViewModelSettled();
		ViewModel->MoveItem(NoTag, 2, NoTag, 8); // Move 2 from slot 2 -> 8
		Res &= ViewModel->AssertViewModelSettled();
        // Now state is: Slot 0: 5, Slot 5: 5, Slot 8: 2
        // Split 2 from slot 5 -> slot 8, making slot 8 full (4)
        ViewModel->SplitItem(NoTag, 5, NoTag, 8, 2);
        Res &= ViewModel->AssertViewModelSettled();
         // State: Slot 0: 5, Slot 5: 3, Slot 8: 4
        FItemBundle ItemSlot5 = ViewModel->GetGridItem(5);
        FItemBundle ItemSlot8 = ViewModel->GetGridItem(8);
		Res &= Test->TestEqual(TEXT("Slot 5 should have 3 sticks after split setup"), ItemSlot5.Quantity, 3);
        Res &= Test->TestEqual(TEXT("Slot 8 should have 4 sticks after split setup"), ItemSlot8.Quantity, 4);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneStick); // Adds 1 stick
        Res &= ViewModel->AssertViewModelSettled();
		ItemSlot5 = ViewModel->GetGridItem(5); // Re-fetch
		Res &= Test->TestEqual(TEXT("Slot 5 should have 4 sticks after adding 1 stick"), ItemSlot5.Quantity, 4);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneStick);
        Res &= ViewModel->AssertViewModelSettled();
        ItemSlot5 = ViewModel->GetGridItem(5); // Re-fetch
		Res &= Test->TestEqual(TEXT("Slot 5 should have 5 sticks after adding 1 stick"), ItemSlot5.Quantity, 5);
		FItemBundle ItemSlot0 = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("Slot 0 should have 5 sticks unchanged"), ItemSlot0.Quantity, 5);
		FItemBundle ItemSlot1 = ViewModel->GetGridItem(1);
		Res &= Test->TestEqual(TEXT("Slot 1 should have 0 unchanged"), ItemSlot1.Quantity, 0);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneStick);
        Res &= ViewModel->AssertViewModelSettled();
        ItemSlot8 = ViewModel->GetGridItem(8); // Re-fetch
		Res &= Test->TestEqual(TEXT("Slot 8 should have 5 sticks after adding 1 stick"), ItemSlot8.Quantity, 5);
		ItemSlot1 = ViewModel->GetGridItem(1); // Re-fetch
		Res &= Test->TestEqual(TEXT("Slot 1 should have 0 unchanged"), ItemSlot1.Quantity, 0);

		for (int i = 0; i < 5; i++)
		{
			InventoryComponent->AddItemToAnySlot(Subsystem, OneStick);
            Res &= ViewModel->AssertViewModelSettled();
			ItemSlot1 = ViewModel->GetGridItem(1); // Re-fetch
			Res &= Test->TestEqual(FString::Printf(TEXT("Slot 1 should have %d sticks after adding 1 stick"), i + 1), ItemSlot1.Quantity, i + 1);
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

		InventoryComponent->AddItemToAnySlot(Subsystem, FiveRocks);
		InventoryComponent->AddItemToAnySlot(Subsystem, ThreeSticks);
		Res &= ViewModel->AssertViewModelSettled();

		ViewModel->MoveItem(NoTag, 0, NoTag, 1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot0AfterMove = ViewModel->GetGridItem(0);
		FItemBundle ItemInSlot1AfterMove = ViewModel->GetGridItem(1);
		Res &= Test->TestTrue(TEXT("Slot 0 should now contain sticks after swap"), ItemInSlot0AfterMove.ItemId == ItemIdSticks && ItemInSlot0AfterMove.Quantity == 3);
		Res &= Test->TestTrue(TEXT("Slot 1 should now contain rocks after swap"), ItemInSlot1AfterMove.ItemId == ItemIdRock && ItemInSlot1AfterMove.Quantity == 5);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		ViewModel->MoveItem(NoTag, 2, NoTag, 1);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot1AfterHelmetSwap = ViewModel->GetGridItem(1);
		FItemBundle ItemInSlot2AfterHelmetSwap = ViewModel->GetGridItem(2);
		Res &= Test->TestTrue(TEXT("Slot 1 should now contain a helmet after swap"), ItemInSlot1AfterHelmetSwap.ItemId == ItemIdHelmet && ItemInSlot1AfterHelmetSwap.Quantity == 1);
		Res &= Test->TestTrue(TEXT("Slot 2 should now contain rocks after swap"), ItemInSlot2AfterHelmetSwap.ItemId == ItemIdRock && ItemInSlot2AfterHelmetSwap.Quantity == 5);

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeSticks);
		Res &= ViewModel->AssertViewModelSettled();
		ViewModel->MoveItem(NoTag, 0, LeftHandSlot, -1); // Move 3 sticks from grid 0 to LeftHand (which has 3) -> 5 LeftHand, 1 Grid 0
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInLeftHandAfterMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		FItemBundle ItemSlot0AfterMove = ViewModel->GetGridItem(0);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should contain 5 sticks after move"), ItemInLeftHandAfterMove.Quantity, 5);
        Res &= Test->TestTrue(TEXT("LeftHandSlot ItemID correct"), ItemInLeftHandAfterMove.ItemId == ItemIdSticks);
		Res &= Test->TestEqual(TEXT("Slot 0 should contain 1 stick after move"), ItemSlot0AfterMove.Quantity, 1);
        Res &= Test->TestTrue(TEXT("Slot 0 ItemID correct"), ItemSlot0AfterMove.ItemId == ItemIdSticks);

		if (Context.TestFixture.AreGameplayTagsCorrupt()) { /* ... */ return true; }

		ViewModel->MoveItemToAnyTaggedSlot(NoTag, 1); // Move helmet from grid 1 to HelmetSlot
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Slot 1 should be empty after moving helmet to HelmetSlot"), ViewModel->IsGridSlotEmpty(1));
		FItemBundle HelmetItem = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestTrue(TEXT("HelmetSlot should contain 1 helmet after move"), HelmetItem.ItemId == ItemIdHelmet && HelmetItem.Quantity == 1);

		ViewModel->MoveItem(HelmetSlot, -1, LeftHandSlot, -1); // Fail: Swap Helmet <-> Sticks (Invalid slot for Sticks)
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInLeftHandAfterHelmetMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
	    FItemBundle HelmetInHelmetSlotAfterMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should still contain sticks after failed move"), ItemInLeftHandAfterHelmetMove.Quantity, 5);
        Res &= Test->TestTrue(TEXT("LeftHandSlot ItemID correct"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSticks);
		Res &= Test->TestEqual(TEXT("HelmetSlot should still contain helmet after failed move"), HelmetInHelmetSlotAfterMove.Quantity, 1);
        Res &= Test->TestTrue(TEXT("HelmetSlot ItemID correct"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet);

		ViewModel->MoveItem(LeftHandSlot, -1, HelmetSlot, -1); // Fail: Swap Sticks <-> Helmet (Invalid slot for Sticks)
		Res &= ViewModel->AssertViewModelSettled();
		ItemInLeftHandAfterHelmetMove = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		HelmetInHelmetSlotAfterMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should still contain sticks after failed move (2)"), ItemInLeftHandAfterHelmetMove.Quantity, 5);
        Res &= Test->TestTrue(TEXT("LeftHandSlot ItemID correct"), ItemInLeftHandAfterHelmetMove.ItemId == ItemIdSticks);
		Res &= Test->TestEqual(TEXT("HelmetSlot should still contain helmet after failed move (2)"), HelmetInHelmetSlotAfterMove.Quantity, 1);
        Res &= Test->TestTrue(TEXT("HelmetSlot ItemID correct"), HelmetInHelmetSlotAfterMove.ItemId == ItemIdHelmet);

	    ViewModel->MoveItem(NoTag, 2, HelmetSlot, -1); // Fail: Move Rock (grid 2) -> HelmetSlot
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInHelmetSlotAfterInvalidMove = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    FItemBundle ItemInSlot2AfterInvalidMove = ViewModel->GetGridItem(2);
	    Res &= Test->TestEqual(TEXT("HelmetSlot should not accept non-helmet item, should remain helmet"), ItemInHelmetSlotAfterInvalidMove.Quantity, 1);
        Res &= Test->TestTrue(TEXT("HelmetSlot ItemID correct"), ItemInHelmetSlotAfterInvalidMove.ItemId == ItemIdHelmet);
	    Res &= Test->TestEqual(TEXT("Slot 2 should remain unchanged after invalid move attempt"), ItemInSlot2AfterInvalidMove.Quantity, 5);
        Res &= Test->TestTrue(TEXT("Slot 2 ItemID correct"), ItemInSlot2AfterInvalidMove.ItemId == ItemIdRock);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, FItemBundle::NoInstances, EItemChangeReason::Removed);
		Res &= ViewModel->AssertViewModelSettled();
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneSpecialHelmet); // Add SpecialHelmet to LeftHand
		Res &= ViewModel->AssertViewModelSettled();
	    ViewModel->MoveItem(LeftHandSlot, -1, HelmetSlot, -1); // Swap SpecialHelmet (Left) <-> Helmet (Helmet)
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInHelmetSlotAfterSwapBack = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    FItemBundle ItemInLeftHandAfterSwapBack = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestEqual(TEXT("HelmetSlot should contain 1 special helmet"), ItemInHelmetSlotAfterSwapBack.Quantity, 1);
        Res &= Test->TestTrue(TEXT("HelmetSlot ItemID correct"), ItemInHelmetSlotAfterSwapBack.ItemId == ItemIdSpecialHelmet);
		Res &= Test->TestEqual(TEXT("LeftHandSlot should contain 1 helmet"), ItemInLeftHandAfterSwapBack.Quantity, 1);
        Res &= Test->TestTrue(TEXT("LeftHandSlot ItemID correct"), ItemInLeftHandAfterSwapBack.ItemId == ItemIdHelmet);

	    InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear); // Add spear to grid (slot 1 likely)
		Res &= ViewModel->AssertViewModelSettled();
		int SpearSlot = ViewModel->GetGridItem(1).ItemId == ItemIdSpear ? 1 : 3; // Find spear slot
		ViewModel->MoveItem(NoTag, SpearSlot, NoTag, 2); // Swap Spear (grid SpearSlot) <-> Rock (grid 2)
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle ItemInSlot1AfterSwap = ViewModel->GetGridItem(SpearSlot);
	    FItemBundle ItemInSlot2AfterSwap = ViewModel->GetGridItem(2);
	    Res &= Test->TestEqual(TEXT("Slot SpearSlot should now contain rocks after swap with spear"), ItemInSlot1AfterSwap.Quantity, 5);
        Res &= Test->TestTrue(TEXT("Slot SpearSlot ItemID correct"), ItemInSlot1AfterSwap.ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Slot 2 should now contain the spear after swap"), ItemInSlot2AfterSwap.Quantity, 1);
        Res &= Test->TestTrue(TEXT("Slot 2 ItemID correct"), ItemInSlot2AfterSwap.ItemId == ItemIdSpear);

		return Res;
	}

	bool TestSwappingMoves()
	{
		GridViewModelTestContext Context(999.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 10*5); // Fill up rest
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle LHItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		FItemBundle RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Left and Right hand should have rocks"), LHItem.ItemId.MatchesTag(ItemIdRock) && RHItem.ItemId.MatchesTag(ItemIdRock));

		int32 SpearSlot = ViewModel->GetGridItem(0).ItemId == ItemIdSpear ? 0 : -1;
        if (SpearSlot == -1) SpearSlot = ViewModel->GetGridItem(1).ItemId == ItemIdSpear ? 1 : -1; // Find spear
		Res &= Test->TestNotEqual(TEXT("Spear should be in a grid slot"), SpearSlot, -1);

		bool Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, SpearSlot, RightHandSlot); // Fail: Swap Spear(Grid) <-> Rock(RH), but LH Rock blocks Spear equip
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestFalse(TEXT("Should not move spear to right hand slot as left hand is occupied"), Moved);
		RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Right hand slot should still have a rock"), RHItem.ItemId.MatchesTag(ItemIdRock));
		Res &= Test->TestEqual(TEXT("Inventory should still contain 10*5 rocks"), InventoryComponent->GetQuantityTotal(ItemIdRock), 10*5);

		Moved = ViewModel->MoveItem( RightHandSlot, -1, FGameplayTag::EmptyTag, SpearSlot); // Fail: Swap Rock(RH) <-> Spear(Grid)
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestFalse(TEXT("Should not move rock to grid slot as spear cannot swap into RH"), Moved);
		RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Right hand slot should still have a rock (2)"), RHItem.ItemId.MatchesTag(ItemIdRock));
		FItemBundle GridSpearItem = ViewModel->GetGridItem(SpearSlot);
        Res &= Test->TestTrue(TEXT("Grid slot should still have spear"), GridSpearItem.ItemId == ItemIdSpear);


		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		Res &= ViewModel->AssertViewModelSettled();
		Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, SpearSlot, RightHandSlot); // Success: Swap Spear(Grid) <-> Rock(RH)
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Should move spear to right hand slot"), Moved);
		RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"), RHItem.ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestEqual(TEXT("Inventory should now contain 9*5 rocks"), InventoryComponent->GetQuantityTotal(ItemIdRock), 9*5);
        FItemBundle GridRockItem = ViewModel->GetGridItem(SpearSlot);
        Res &= Test->TestTrue(TEXT("Grid slot should contain rock after swap"), GridRockItem.ItemId == ItemIdRock);


		// Move spear back to generic inventory swapping with a rock
		Moved = ViewModel->MoveItem(RightHandSlot, -1, FGameplayTag::EmptyTag, 3); // Swap Spear(RH) <-> Rock(Grid 3)
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Should move spear to generic inventory"), Moved);
		GridSpearItem = ViewModel->GetGridItem(3);
		Res &= Test->TestTrue(TEXT("Generic slot 3 should contain the spear"), GridSpearItem.ItemId == ItemIdSpear);
		RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Right hand should contain a rock"), RHItem.ItemId.MatchesTag(ItemIdRock) && RHItem.Quantity == 5);

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
	    InventoryComponent->AddItemToAnySlot(Subsystem, FiveRocks); // Added to first generic slot (0)
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet); // Added to HelmetSlot
	    Res &= ViewModel->AssertViewModelSettled();

	    // Valid split in generic slots
	    ViewModel->SplitItem(NoTag, 0, NoTag, 1, 2);
	    Res &= ViewModel->AssertViewModelSettled();
	    FItemBundle ItemSlot0 = ViewModel->GetGridItem(0);
	    FItemBundle ItemSlot1 = ViewModel->GetGridItem(1);
	    Res &= Test->TestEqual(TEXT("After splitting, first slot should have 3 rocks"), ItemSlot0.Quantity, 3);
	    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 2 rocks"), ItemSlot1.Quantity, 2);

	    // Invalid split due to insufficient quantity in source slot
	    ViewModel->SplitItem(NoTag, 0, NoTag, 1, 4); // Trying to split more than available (only 3)
	    Res &= ViewModel->AssertViewModelSettled(); // Re-assert settled state
	    ItemSlot0 = ViewModel->GetGridItem(0); // Re-fetch
	    Res &= Test->TestEqual(TEXT("Attempt to split more rocks than available should fail, Slot 0 remains 3"), ItemSlot0.Quantity, 3);

	    // Split between a generic slot and a tagged slot
	    ViewModel->SplitItem(NoTag, 1, RightHandSlot, -1, 1); // Splitting 1 rock from Grid(1) -> RightHandSlot
	    Res &= ViewModel->AssertViewModelSettled();
	    ItemSlot1 = ViewModel->GetGridItem(1); // Re-fetch
	    Res &= Test->TestEqual(TEXT("After splitting, second slot should have 1 rock"), ItemSlot1.Quantity, 1);
	    FItemBundle RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestTrue(TEXT("RightHandSlot should now contain 1 rock"), RightHandItem.ItemId == ItemIdRock && RightHandItem.Quantity == 1);

	    // Invalid split to a different item type slot
	    ViewModel->SplitItem(RightHandSlot, -1, HelmetSlot, -1, 1); // Trying to split rock into helmet slot
	    Res &= ViewModel->AssertViewModelSettled();
	    RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot); // Re-fetch to check for changes
	    FItemBundle HelmetItem = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    Res &= Test->TestEqual(TEXT("Attempting to split into incompatible slot should fail (RH unchanged)"), RightHandItem.Quantity, 1);
	    Res &= Test->TestTrue(TEXT("RH ItemID still Rock"), RightHandItem.ItemId == ItemIdRock);
	    Res &= Test->TestEqual(TEXT("Attempting to split into incompatible slot should fail (Helmet unchanged)"), HelmetItem.Quantity, 1);
	    Res &= Test->TestTrue(TEXT("Helmet ItemID still Helmet"), HelmetItem.ItemId == ItemIdHelmet);

	    // Add 11 rocks. State before add: Slot0(3R), Slot1(1R), RH(1R), Helmet(1H). Total 5 Rocks.
	    // Adding 11 Rocks (Weight 11). Capacity 99.
	    // Fills Slot0 (3->5, adds 2), Slot1 (1->5, adds 4), RH (1->5, adds 4). Total added = 10. 1 remaining.
	    // Adds 1 to next empty grid slot (Slot 2).
	    InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 11);
	    Res &= ViewModel->AssertViewModelSettled();
	    // Expected State: Slot0(5R), Slot1(5R), Slot2(1R), RH(5R), Helmet(1H)

	    ItemSlot0 = ViewModel->GetGridItem(0); // Re-fetch state after add
	    ItemSlot1 = ViewModel->GetGridItem(1);
	    FItemBundle ItemSlot2 = ViewModel->GetGridItem(2);
	    RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestEqual(TEXT("Slot 0 after add 11"), ItemSlot0.Quantity, 5);
	    Res &= Test->TestEqual(TEXT("Slot 1 after add 11"), ItemSlot1.Quantity, 5);
	    Res &= Test->TestEqual(TEXT("Slot 2 after add 11"), ItemSlot2.Quantity, 1);
	    Res &= Test->TestEqual(TEXT("RH after add 11"), RightHandItem.Quantity, 5);

	    // Res &= !ViewModel->SplitItem(NoTag, 2, NoTag, 1, 2); // Fail: This tries to split *from* slot 2, which only has 1 rock. Correctly returns false.
	    bool bSplitFromSlot2Failed = !ViewModel->SplitItem(NoTag, 2, NoTag, 1, 2);
	    Res &= Test->TestTrue(TEXT("Splitting 2 from slot with 1 should fail"), bSplitFromSlot2Failed);
	    Res &= ViewModel->AssertViewModelSettled(); // Should still be settled

	    // Now try to split from a slot with items but would overflow
	    // Res &= !ViewModel->SplitItem(NoTag, 1, NoTag, 0, 1); // Fail: Split Grid(1)[5R] -> Grid(0)[5R] (Overflow Grid 0)
	    bool bSplitOverflowFailed = !ViewModel->SplitItem(NoTag, 1, NoTag, 0, 1);
	    Res &= Test->TestTrue(TEXT("Splitting into full slot should fail"), bSplitOverflowFailed);
	    Res &= ViewModel->AssertViewModelSettled();
	    ItemSlot0 = ViewModel->GetGridItem(0); // Re-fetch
	    ItemSlot1 = ViewModel->GetGridItem(1);
	    Res &= Test->TestEqual(TEXT("Splitting that exceeds max stack size should fail (Slot 0 unchanged)"), ItemSlot0.Quantity, 5);
	    Res &= Test->TestEqual(TEXT("Splitting that exceeds max stack size should fail (Slot 1 unchanged)"), ItemSlot1.Quantity, 5);

	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, FiveRocks);
	    Res &= ViewModel->AssertViewModelSettled();

	    // **** Failing Section Debugged ****
	    // State before split: Slot1(5R), LeftHandSlot(5R)
	    // ViewModel->SplitItem(LeftHandSlot, -1, NoTag, 1, 1); // Attempt Split LH[5R] -> Grid(1)[5R]
	    bool bSplitLHToGrid1 = ViewModel->SplitItem(LeftHandSlot, -1, NoTag, 1, 1);
	    Res &= Test->TestFalse(TEXT("Splitting LH(5R) to Grid1(5R) should fail (target full)"), bSplitLHToGrid1);
	    Res &= ViewModel->AssertViewModelSettled();
	    ItemSlot1 = ViewModel->GetGridItem(1); // Re-fetch after failed split
	    FItemBundle LeftHandItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot); // Re-fetch
	    // Check that quantities remain unchanged due to failed split
	    Res &= Test->TestEqual(TEXT("After FAILED split, grid slot 1 should still contain 5 rocks"), ItemSlot1.Quantity, 5); // << CORRECTED EXPECTATION
	    Res &= Test->TestEqual(TEXT("After FAILED split, LeftHandSlot should still contain 5 rocks"), LeftHandItem.Quantity, 5); // << ADDED CHECK

	    // **** End of Corrected Section ****

	    // Continue with remaining tests...
	    ViewModel->SplitItem(NoTag, 1, LeftHandSlot, -1, 1); // Split Grid(1)[5R] -> LH[5R] - Should Fail (LH full)
	    Res &= ViewModel->AssertViewModelSettled();
	    FItemBundle ItemInLeftHandAfterSplit = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
	    Res &= Test->TestEqual(TEXT("LeftHandSlot should still contain 5 rocks (split failed)"), ItemInLeftHandAfterSplit.Quantity, 5);
	    ItemSlot1 = ViewModel->GetGridItem(1); // Re-fetch
	    Res &= Test->TestEqual(TEXT("Slot 1 should still contain 5 rocks (split failed)"), ItemSlot1.Quantity, 5);

	    // Test splitting into an empty slot again to ensure logic is sound after failed attempts
	    ViewModel->SplitItem(NoTag, 0, NoTag, 3, 2); // Split Slot0[5R] -> Slot3[Empty]
	    Res &= ViewModel->AssertViewModelSettled();
	    ItemSlot0 = ViewModel->GetGridItem(0);
	    FItemBundle ItemSlot3 = ViewModel->GetGridItem(3);
	    Res &= Test->TestEqual(TEXT("After splitting to empty slot, Slot 0 should have 3 rocks"), ItemSlot0.Quantity, 3);
	    Res &= Test->TestEqual(TEXT("After splitting to empty slot, Slot 3 should have 2 rocks"), ItemSlot3.Quantity, 2);


	    ViewModel->SplitItem(NoTag, 5, NoTag, 6, 1); // Fail: Invalid source index
	    Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes (5)"), ViewModel->IsGridSlotEmpty(5));
	    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes (6)"), ViewModel->IsGridSlotEmpty(6));
	    ViewModel->SplitItem(NoTag, 10, NoTag, 11, 1); // Fail: Invalid source index
	    Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes (10)"), ViewModel->IsGridSlotEmpty(10));
	    Res &= Test->TestTrue(TEXT("Invalid split indices should result in no changes (11)"), ViewModel->IsGridSlotEmpty(11));

	    ViewModel->SplitItem(NoTag, -1, ChestSlot, -1, 1); // Fail: Invalid source grid index (-1 needs tag)
	    Res &= ViewModel->AssertViewModelSettled();
	    Res &= Test->TestTrue(TEXT("Invalid source grid index should result in no changes"), ViewModel->IsTaggedSlotEmpty(ChestSlot));

	    InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
	    Res &= ViewModel->AssertViewModelSettled();
	    InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneStick);
	    Res &= ViewModel->AssertViewModelSettled();
	    ViewModel->SplitItem(NoTag, 0, RightHandSlot, -1, 1); // Fail: Split Rock(Grid 0) -> RH(Stick)
	    Res &= ViewModel->AssertViewModelSettled();
	    RightHandItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
	    Res &= Test->TestTrue(TEXT("Attempting to split into a slot with a different item type should fail"), RightHandItem.ItemId == ItemIdSticks && RightHandItem.Quantity == 1);
	    ItemSlot0 = ViewModel->GetGridItem(0); // Re-fetch
	    Res &= Test->TestEqual(TEXT("Source slot should remain unchanged after failed split"), ItemSlot0.Quantity, 3); // Was 3 after previous split

	    // --- Instance Data Split Tests ---
	    InventoryComponent->Clear_IfServer();
	    Res &= ViewModel->AssertViewModelSettled();

	    // Add 3 eggs to grid slot 0
	    InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 3);
	    Res &= ViewModel->AssertViewModelSettled();
	    FItemBundle EggSlot0 = ViewModel->GetGridItem(0);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Grid slot 0 has 3 eggs"), EggSlot0.Quantity, 3);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Grid slot 0 has 3 instances"), EggSlot0.InstanceData.Num(), 3);
	    TArray<UItemInstanceData*> OriginalInstances = EggSlot0.InstanceData; // Copy for later check

	    // Split 1 egg from slot 0 to slot 1
	    ViewModel->SplitItem(NoTag, 0, NoTag, 1, 1);
	    Res &= ViewModel->AssertViewModelSettled();
	    EggSlot0 = ViewModel->GetGridItem(0);
	    FItemBundle EggSlot1 = ViewModel->GetGridItem(1);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 0 has 2 eggs after split"), EggSlot0.Quantity, 2);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 0 has 2 instances after split"), EggSlot0.InstanceData.Num(), 2);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 1 has 1 egg after split"), EggSlot1.Quantity, 1);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 1 has 1 instance after split"), EggSlot1.InstanceData.Num(), 1);

	    // Verify the *correct* instance was moved (assuming last one)
	    if (EggSlot0.InstanceData.Num() == 2 && EggSlot1.InstanceData.Num() == 1 && OriginalInstances.Num() == 3)
	    {
	        Res &= Test->TestTrue(TEXT("[InstanceSplit] Slot 1 instance matches last original instance"), EggSlot1.InstanceData[0] == OriginalInstances[2]);
	        Res &= Test->TestTrue(TEXT("[InstanceSplit] Slot 0 instance 0 matches original 0"), EggSlot0.InstanceData[0] == OriginalInstances[0]);
	        Res &= Test->TestTrue(TEXT("[InstanceSplit] Slot 0 instance 1 matches original 1"), EggSlot0.InstanceData[1] == OriginalInstances[1]);
	    }

	    // Split 1 egg from slot 0 to LeftHand
	    ViewModel->SplitItem(NoTag, 0, LeftHandSlot, -1, 1);
	    Res &= ViewModel->AssertViewModelSettled();
	    EggSlot0 = ViewModel->GetGridItem(0);
	    FItemBundle EggLeftHand = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 0 has 1 egg after split to tag"), EggSlot0.Quantity, 1);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] Slot 0 has 1 instance after split to tag"), EggSlot0.InstanceData.Num(), 1);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] LeftHand has 1 egg after split"), EggLeftHand.Quantity, 1);
	    Res &= Test->TestEqual(TEXT("[InstanceSplit] LeftHand has 1 instance after split"), EggLeftHand.InstanceData.Num(), 1);
	    if (EggSlot0.InstanceData.Num() == 1 && EggLeftHand.InstanceData.Num() == 1 && OriginalInstances.Num() == 3)
	    {
	         Res &= Test->TestTrue(TEXT("[InstanceSplit] LeftHand instance matches original 1"), EggLeftHand.InstanceData[0] == OriginalInstances[1]); // Original[1] was the second one left in slot 0
	         Res &= Test->TestTrue(TEXT("[InstanceSplit] Slot 0 instance matches original 0"), EggSlot0.InstanceData[0] == OriginalInstances[0]);
	    }

	    return Res;
	}

	bool TestMoveItemToAnyTaggedSlot()
	{
	    GridViewModelTestContext Context(25, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

	    FDebugTestResult Res = true;

	    InventoryComponent->AddItemToAnySlot(Subsystem, ThreeRocks,  EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneHelmet,EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear,EPreferredSlotPolicy::PreferGenericInventory);
	    InventoryComponent->AddItemToAnySlot(Subsystem, OneChestArmor, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();

		if (Context.TestFixture.AreGameplayTagsCorrupt()) { /*...*/ return true; }

	    Res &= Test->TestTrue(TEXT("Move rock to any tagged slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle TaggedRock = ViewModel->GetItemForTaggedSlot(RightHandSlot); // Assuming RightHand is first universal available
	    Res &= Test->TestTrue(TEXT("Rock should be in the first universal tagged slot, right hand"), TaggedRock.ItemId == ItemIdRock);

	    Res &= Test->TestTrue(TEXT("Move helmet to its specialized slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 1));
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle TaggedHelmet = ViewModel->GetItemForTaggedSlot(HelmetSlot);
	    Res &= Test->TestTrue(TEXT("Helmet should be in HelmetSlot"), TaggedHelmet.ItemId == ItemIdHelmet);

	    Res &= Test->TestTrue(TEXT("Move spear to any tagged slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 2));
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle TaggedSpear = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand tagged slot"), TaggedSpear.ItemId == ItemIdSpear);
		int32 SlotWithRock = ViewModel->GetGridItem(0).ItemId == ItemIdRock ? 0 : 2; // Rock was swapped by spear move
		FItemBundle GridRock = ViewModel->GetGridItem(SlotWithRock);
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0 or 2"), GridRock.ItemId == ItemIdRock);

	    Res &= Test->TestFalse(TEXT("Attempting to move helmet already in HelmetSlot should do nothing"), ViewModel->MoveItemToAnyTaggedSlot(HelmetSlot, -1));
		Res &= ViewModel->AssertViewModelSettled();

		if (Context.TestFixture.AreGameplayTagsCorrupt()) { /*...*/ return true; }

	    Res &= Test->TestTrue(TEXT("Move chest armor to its specialized slot"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 3));
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle TaggedChest = ViewModel->GetItemForTaggedSlot(ChestSlot);
	    Res &= Test->TestTrue(TEXT("Chest armor should be in ChestSlot"), TaggedChest.ItemId == ItemIdChestArmor);

	    Res &= Test->TestFalse(TEXT("Attempt to move extra rock to tagged should fail"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, SlotWithRock));
		Res &= ViewModel->AssertViewModelSettled();

		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpecialHelmet, EPreferredSlotPolicy::PreferGenericInventory); // goes to grid slot (likely 1 or 3)
		Res &= ViewModel->AssertViewModelSettled();
		auto moved = ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);
		Res &= Test->TestTrue(TEXT("A different helmet should swap into the helmet slot"), moved);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle HelmetSlotItem = ViewModel->GetItemForTaggedSlot(HelmetSlot);
		Res &= Test->TestTrue(TEXT("Special helmet should be in HelmetSlot"), HelmetSlotItem.ItemId == ItemIdSpecialHelmet);
		FItemBundle GridHelmet = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("Helmet should be in generic slot"), GridHelmet.ItemId == ItemIdHelmet);

	    Res &= Test->TestFalse(TEXT("Attempting to move item from invalid source index should fail"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 100));
		Res &= ViewModel->AssertViewModelSettled();

	    Res &= Test->TestFalse(TEXT("Attempting to move item from an empty tagged slot should fail"), ViewModel->MoveItemToAnyTaggedSlot(LeftHandSlot, -1)); // Assuming LeftHand is empty
		Res &= ViewModel->AssertViewModelSettled();

		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();
		for (int i = 0; i < 9; i++) Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be empty"), i), ViewModel->IsGridSlotEmpty(i));
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0); // Move Spear from grid 0 to Any Tagged (RightHand)
		Res &= ViewModel->AssertViewModelSettled();
		GridRock = ViewModel->GetGridItem(0);
		TaggedSpear = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot 0"), GridRock.ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), TaggedSpear.ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();
		for (int i = 0; i < 9; i++) Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be empty"), i), ViewModel->IsGridSlotEmpty(i));
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0); // Move Spear from grid 0 to Any Tagged (RightHand)
		Res &= ViewModel->AssertViewModelSettled();
		int32 RockSlot = ViewModel->GetGridItem(0).ItemId == ItemIdRock ? 0 : 1; // Rock moved to slot 0 or 1
		GridRock = ViewModel->GetGridItem(RockSlot);
		TaggedSpear = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Rock should be in generic slot"), GridRock.ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), TaggedSpear.ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock);
		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundle(OneSpear), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();

		FItemBundle LeftHandItem = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		int32 SpearGridSlot = ViewModel->GetGridItem(0).ItemId == ItemIdSpear ? 0 : (ViewModel->GetGridItem(1).ItemId == ItemIdSpear ? 1 : -1);
		FItemBundle SpearGridItem = ViewModel->GetGridItem(SpearGridSlot);
		Res &= Test->TestTrue(TEXT("Rock should still be in left hand"), LeftHandItem.ItemId == ItemIdRock);
		Res &= Test->TestTrue(TEXT("Spear should be generic slot"), SpearGridItem.ItemId == ItemIdSpear);

		InventoryComponent->Clear_IfServer();
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear);
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 2);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= ViewModel->MoveItem(NoTag, 0, RightHandSlot, -1); // Swap Sticks(Grid 0) <-> Spear(RH)
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle RHSticks = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		FItemBundle GridSpear = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("Sticks should be in right hand"), RHSticks.ItemId == ItemIdSticks && RHSticks.Quantity == 2);
		Res &= Test->TestTrue(TEXT("Spear should be in slot 0"), GridSpear.ItemId == ItemIdSpear);
		Res &= ViewModel->MoveItem(RightHandSlot, -1, LeftHandSlot, -1); // Move Sticks RH -> LH
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle LHSticks = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Sticks should be in left hand"), LHSticks.ItemId == ItemIdSticks && LHSticks.Quantity == 2);
		GridSpear = ViewModel->GetGridItem(0); // Recheck
		Res &= Test->TestTrue(TEXT("Spear should be in slot 0"), GridSpear.ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));

        // --- Instance Data Tests ---
        InventoryComponent->Clear_IfServer();
        Res &= ViewModel->AssertViewModelSettled();

        // Add Knife to Grid 0
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridKnifeStart = ViewModel->GetGridItem(0);
        UItemInstanceData* KnifeInstancePtr = GridKnifeStart.InstanceData.Num() > 0 ? GridKnifeStart.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Knife instance ptr valid"), KnifeInstancePtr);

        // Move Knife (Grid 0) to Any Tagged (should go to RH)
        Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);
        Res &= ViewModel->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[Instance] Grid 0 should be empty after move"), ViewModel->IsGridSlotEmpty(0));
        FItemBundle TaggedKnifeEnd = ViewModel->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] RH has knife"), TaggedKnifeEnd.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] RH has 1 instance"), TaggedKnifeEnd.InstanceData.Num(), 1);
        if(TaggedKnifeEnd.InstanceData.Num() == 1)
        {
            Res &= Test->TestTrue(TEXT("[Instance] RH instance pointer matches"), TaggedKnifeEnd.InstanceData[0] == KnifeInstancePtr);
        }

        // Add Egg (stackable instance) to Grid 0
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 1);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridEggStart = ViewModel->GetGridItem(0);
        UItemInstanceData* EggInstancePtr = GridEggStart.InstanceData.Num() > 0 ? GridEggStart.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Egg instance ptr valid"), EggInstancePtr);

        // Move Egg (Grid 0) to Any Tagged (should go to LH, as RH has knife)
        Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0);
        Res &= ViewModel->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[Instance] Grid 0 empty after egg move"), ViewModel->IsGridSlotEmpty(0));
        FItemBundle TaggedEggEnd = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] LH has egg"), TaggedEggEnd.ItemId == ItemIdBrittleEgg);
        Res &= Test->TestEqual(TEXT("[Instance] LH has 1 egg instance"), TaggedEggEnd.InstanceData.Num(), 1);
        if(TaggedEggEnd.InstanceData.Num() == 1)
        {
             Res &= Test->TestTrue(TEXT("[Instance] LH egg instance pointer matches"), TaggedEggEnd.InstanceData[0] == EggInstancePtr);
        }

		return Res;
	}

	bool TestMakeshiftWeapons()
	{
		GridViewModelTestContext Context(50.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundle(ItemIdBrittleCopperKnife, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle RHKnife = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Knife should be in right hand"), RHKnife.ItemId == ItemIdBrittleCopperKnife);

		WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundle(ItemIdBrittleCopperKnife, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle LHKnife = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Second knife should be in left hand"), LHKnife.ItemId == ItemIdBrittleCopperKnife);

		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0); // Move Spear from Grid 0 to Tagged (RH)
		Res &= ViewModel->AssertViewModelSettled();

		FItemBundle RHSpear = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), RHSpear.ItemId == ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		return Res;
	}

	bool TestLeftHandHeldBows()
	{
		GridViewModelTestContext Context(50.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		AWorldItem* WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundle(ItemIdShortbow, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle LHShortbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Shortbow should be in left hand"), LHShortbow.ItemId == ItemIdShortbow);

		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestFalse(TEXT("Right hand should not be blocked"), InventoryComponent->IsTaggedSlotBlocked(RightHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should not be blocked"), ViewModel->CanTaggedSlotReceiveItem(OneRock, RightHandSlot));

		Res &= Test->TestFalse(TEXT("Should not be able to move shortbow to right hand"), ViewModel->MoveItemToAnyTaggedSlot(LeftHandSlot, -1));
		Res &= ViewModel->AssertViewModelSettled();
		LHShortbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Shortbow should still be in left hand"), LHShortbow.ItemId == ItemIdShortbow);

		Res &= Test->TestTrue(TEXT("Move shortbow to generic slot"), ViewModel->MoveItem(LeftHandSlot, -1, NoTag, 0));
		Res &= ViewModel->AssertViewModelSettled();
		FItemBundle GridShortbow = ViewModel->GetGridItem(0);
		Res &= Test->TestTrue(TEXT("Shortbow should be in generic slot 0"), GridShortbow.ItemId == ItemIdShortbow);
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

		Res &= Test->TestTrue(TEXT("Move shortbow to left hand"), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
		Res &= ViewModel->AssertViewModelSettled();
		LHShortbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Shortbow should be in left hand"), LHShortbow.ItemId == ItemIdShortbow);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		Res &= ViewModel->AssertViewModelSettled();
		WorldItem = Subsystem->SpawnWorldItem(InventoryComponent, FItemBundle(ItemIdLongbow, 1), FVector::Zero(), AWorldItem::StaticClass());
		ViewModel->PickupItem(WorldItem, EPreferredSlotPolicy::PreferSpecializedTaggedSlot, false);
		Res &= ViewModel->AssertViewModelSettled();

		FItemBundle LHLongbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Longbow should be in left hand"), LHLongbow.ItemId == ItemIdLongbow);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestTrue(TEXT("Right hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(RightHandSlot));
		Res &= Test->TestFalse(TEXT("Right hand should be blocked"), ViewModel->CanTaggedSlotReceiveItem(OneRock, RightHandSlot));

		Res &= Test->TestFalse(TEXT("Should not be able to move longbow to right hand"), ViewModel->MoveItemToAnyTaggedSlot(LeftHandSlot, -1));
		Res &= ViewModel->AssertViewModelSettled();
		LHLongbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Longbow should still be in left hand"), LHLongbow.ItemId == ItemIdLongbow);

		InventoryComponent->MoveItem(ItemIdLongbow, 1, FItemBundle::NoInstances, LeftHandSlot, NoTag);
		Res &= ViewModel->AssertViewModelSettled();
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Move longbow to any slot, which should be left. It should move blocking rock."), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
		Res &= ViewModel->AssertViewModelSettled();

		LHLongbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Longbow should be in left hand"), LHLongbow.ItemId == ItemIdLongbow);
		Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		Res &= Test->TestEqual(TEXT("Generic slot should contain the rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 1);

		return Res;
	}

	bool TestSlotReceiveItem()
	{
	    GridViewModelTestContext Context(10.0f, 5, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();

	    FDebugTestResult Res = true;

	    Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), ViewModel->CanGridSlotReceiveItem(ThreeRocks, 0));

		Res &= Test->TestTrue(TEXT("Can add more rocks to slot with same item type"), ViewModel->CanGridSlotReceiveItem(TwoRocks, 0));
		InventoryComponent->AddItemToAnySlot(Subsystem, TwoRocks);
		Res &= ViewModel->AssertViewModelSettled();

	    Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), ViewModel->CanGridSlotReceiveItem(OneHelmet, 0));

		Res &= Test->TestTrue(TEXT("Can add helmet to a different slot"), ViewModel->CanGridSlotReceiveItem(OneHelmet, 1));

	    Res &= Test->TestFalse(TEXT("Cannot add rocks exceeding max stack size"), ViewModel->CanGridSlotReceiveItem(FiveRocks, 0));

	    Res &= Test->TestFalse(TEXT("Cannot add item to an out-of-bounds slot"), ViewModel->CanGridSlotReceiveItem(ThreeRocks, 10));

	    Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), ViewModel->CanGridSlotReceiveItem(GiantBoulder, 1));

		Res &= Test->TestTrue(TEXT("Can add rocks to empty slot"), ViewModel->CanTaggedSlotReceiveItem(ThreeRocks, LeftHandSlot));
		Res &= Test->TestFalse(TEXT("Cannot add rocks to helmet slot"), ViewModel->CanTaggedSlotReceiveItem(ThreeRocks, HelmetSlot));
		Res &= Test->TestTrue(TEXT("Can add helmet to a matching specialized slot"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, HelmetSlot));
		Res &= Test->TestTrue(TEXT("Can add helmet to a universal slot"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, FiveRocks);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestFalse(TEXT("Cannot add a helmet to a slot with rocks"), ViewModel->CanTaggedSlotReceiveItem(OneHelmet, LeftHandSlot));
		Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), ViewModel->CanTaggedSlotReceiveItem(GiantBoulder, RightHandSlot));

		return Res;
	}

	bool TestDrop()
	{
		GridViewModelTestContext Context(100.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 9, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();

		ViewModel->DropItemFromGrid(8, 1);
		Res &= ViewModel->AssertViewModelSettled(); // Wait for potential server correction
		Res &= Test->TestEqual(TEXT("After dropping 1 spear, there should be 8 spears left"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear), 8);
		Res &= Test->TestTrue(TEXT("Slot 8 should be empty after dropping 1 spear"), ViewModel->IsGridSlotEmpty(8));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			FItemBundle SlotItem = ViewModel->GetGridItem(Index);
			Res &= Test->TestFalse(FString::Printf(TEXT("Slot %d should not be empty"), Index), ViewModel->IsGridSlotEmpty(Index));
            Res &= Test->TestTrue(FString::Printf(TEXT("Slot %d should be spear"), Index), SlotItem.ItemId == ItemIdSpear);
		}

        // Instance Data Drop Test
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 3); // 3 eggs in slot 0
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridEggs = ViewModel->GetGridItem(0);
        TArray<UItemInstanceData*> InitialEggInstances = GridEggs.InstanceData;
        Res &= Test->TestEqual(TEXT("[InstanceDrop] Initial instance count"), InitialEggInstances.Num(), 3);

        // Drop 1 egg from grid slot 0
        ViewModel->DropItemFromGrid(0, 1);
        Res &= ViewModel->AssertViewModelSettled();
        GridEggs = ViewModel->GetGridItem(0);
        Res &= Test->TestEqual(TEXT("[InstanceDrop] Quantity after drop 1"), GridEggs.Quantity, 2);
        Res &= Test->TestEqual(TEXT("[InstanceDrop] Instance count after drop 1"), GridEggs.InstanceData.Num(), 2);
        // Verify the correct instance was dropped (assuming last one)
        if(InitialEggInstances.Num() == 3 && GridEggs.InstanceData.Num() == 2)
        {
            Res &= Test->TestTrue(TEXT("[InstanceDrop] Remaining instance 0 correct"), GridEggs.InstanceData[0] == InitialEggInstances[0]);
            Res &= Test->TestTrue(TEXT("[InstanceDrop] Remaining instance 1 correct"), GridEggs.InstanceData[1] == InitialEggInstances[1]);
        }

		return Res;
	}

    // New test specifically for using items with instance data
    bool TestUseInstanceDataItems()
    {
        GridViewModelTestContext Context(100.0f, 9, false);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* ViewModel = Context.ViewModel;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

        // Add 3 eggs to grid slot 0
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 3);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle GridEggs = ViewModel->GetGridItem(0);
        TArray<UItemInstanceData*> InitialEggInstances = GridEggs.InstanceData;
        Res &= Test->TestEqual(TEXT("[InstanceUse] Initial egg instance count"), InitialEggInstances.Num(), 3);

        // Use 1 egg from grid slot 0
        ViewModel->UseItemFromGrid(0);
        Res &= ViewModel->AssertViewModelSettled();
        GridEggs = ViewModel->GetGridItem(0);
        Res &= Test->TestEqual(TEXT("[InstanceUse] Quantity after use 1"), GridEggs.Quantity, 2);
        Res &= Test->TestEqual(TEXT("[InstanceUse] Instance count after use 1"), GridEggs.InstanceData.Num(), 2);
        // Verify the correct instance was used (assuming last one)
        if(InitialEggInstances.Num() == 3 && GridEggs.InstanceData.Num() == 2)
        {
            Res &= Test->TestTrue(TEXT("[InstanceUse] Remaining instance 0 correct"), GridEggs.InstanceData[0] == InitialEggInstances[0]);
            Res &= Test->TestTrue(TEXT("[InstanceUse] Remaining instance 1 correct"), GridEggs.InstanceData[1] == InitialEggInstances[1]);
        }

        // Add 2 eggs to LeftHandSlot
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdBrittleEgg, 2);
        Res &= ViewModel->AssertViewModelSettled();
        FItemBundle TaggedEggs = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
        TArray<UItemInstanceData*> InitialTaggedInstances = TaggedEggs.InstanceData;
        Res &= Test->TestEqual(TEXT("[InstanceUse] Initial tagged egg instance count"), InitialTaggedInstances.Num(), 2);

        // Use 1 egg from LeftHandSlot
        ViewModel->UseItemFromTaggedSlot(LeftHandSlot);
        Res &= ViewModel->AssertViewModelSettled();
        TaggedEggs = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestEqual(TEXT("[InstanceUse] Tagged quantity after use 1"), TaggedEggs.Quantity, 1);
        Res &= Test->TestEqual(TEXT("[InstanceUse] Tagged instance count after use 1"), TaggedEggs.InstanceData.Num(), 1);
        // Verify correct instance used (assuming last)
        if(InitialTaggedInstances.Num() == 2 && TaggedEggs.InstanceData.Num() == 1)
        {
             Res &= Test->TestTrue(TEXT("[InstanceUse] Tagged remaining instance 0 correct"), TaggedEggs.InstanceData[0] == InitialTaggedInstances[0]);
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
    Res &= TestScenarios.TestUseInstanceDataItems(); // Add new test

	return Res;
}