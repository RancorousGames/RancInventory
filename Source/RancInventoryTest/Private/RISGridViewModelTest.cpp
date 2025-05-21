// Copyright Rancorous Games, 2024

#include "RISGridViewModelTest.h" // header empty

#include "EngineUtils.h"
#include "NativeGameplayTags.h"
#include "Components\InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "ViewModels\InventoryGridViewModel.h"
#include "RISInventoryTestSetup.cpp"
#include "Core/RISSubsystem.h"
#include "Framework/DebugTestResult.h"
#include "MockClasses/ItemHoldingCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#define TestNameGVM "GameTests.RIS.3_GridViewModel"
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
		ViewModel->Initialize(InventoryComponent);
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
		ViewModel->MoveItem(NoTag, 0, LeftHandSlot); // Move 3 sticks from grid 0 to LeftHand (which has 3) -> 5 LeftHand, 1 Grid 0
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

		// Its not entirely clear how i want to handle the automatic swapbacking with direct and indirect blocking here
		//bool Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, SpearSlot, RightHandSlot); // Fail: Swap Spear(Grid) <-> Rock(RH), but LH Rock blocks Spear equip
		//Res &= ViewModel->AssertViewModelSettled();
		//Res &= Test->TestFalse(TEXT("Should not move spear to right hand slot as left hand is occupied"), Moved);
		//RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		//Res &= Test->TestTrue(TEXT("Right hand slot should still have a rock"), RHItem.ItemId.MatchesTag(ItemIdRock));
		//Res &= Test->TestEqual(TEXT("Inventory should still contain 10*5 rocks"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock), 10*5);

		//Moved = ViewModel->MoveItem( RightHandSlot, -1, FGameplayTag::EmptyTag, SpearSlot); // Fail: Swap Rock(RH) <-> Spear(Grid)
		//Res &= ViewModel->AssertViewModelSettled();
		//Res &= Test->TestFalse(TEXT("Should not move rock to grid slot as spear cannot swap into RH"), Moved);
		//RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		//Res &= Test->TestTrue(TEXT("Right hand slot should still have a rock (2)"), RHItem.ItemId.MatchesTag(ItemIdRock));
		//FItemBundle GridSpearItem = ViewModel->GetGridItem(SpearSlot);
        //Res &= Test->TestTrue(TEXT("Grid slot should still have spear"), GridSpearItem.ItemId == ItemIdSpear);


		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		Res &= ViewModel->AssertViewModelSettled();
		bool Moved = ViewModel->MoveItem(FGameplayTag::EmptyTag, SpearSlot, RightHandSlot); // Success: Swap Spear(Grid) <-> Rock(RH)
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Should move spear to right hand slot"), Moved);
		RHItem = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"), RHItem.ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));
		Res &= Test->TestEqual(TEXT("Inventory should now contain 9*5 rocks"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock), 9*5);
        FItemBundle GridRockItem = ViewModel->GetGridItem(SpearSlot);
        Res &= Test->TestTrue(TEXT("Grid slot should contain rock after swap"), GridRockItem.ItemId == ItemIdRock);


		// Move spear back to generic inventory swapping with a rock
		Moved = ViewModel->MoveItem(RightHandSlot, -1, FGameplayTag::EmptyTag, 3); // Swap Spear(RH) <-> Rock(Grid 3)
		Res &= ViewModel->AssertViewModelSettled();
		Res &= Test->TestTrue(TEXT("Should move spear to generic inventory"), Moved);
		auto GridSpearItem = ViewModel->GetGridItem(3);
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

		// TODO: We dont currently support indirect automatic unblocking
		//InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock);
		InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= ViewModel->AssertViewModelSettled();
		Res &= ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0); // Move Spear from grid 0 to Any Tagged (RightHand)
		Res &= ViewModel->AssertViewModelSettled();
		int32 RockSlot = ViewModel->GetGridItem(0).ItemId == ItemIdRock ? 0 : 1; // Rock moved to slot 0 or 1
		GridRock = ViewModel->GetGridItem(RockSlot);
		TaggedSpear = ViewModel->GetItemForTaggedSlot(RightHandSlot);
		//Res &= Test->TestTrue(TEXT("Rock should be in generic slot"), GridRock.ItemId == ItemIdRock);
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
		Res &= ViewModel->MoveItem(RightHandSlot, -1, LeftHandSlot); // Move Sticks RH -> LH
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
		// TODO	: We dont currently support indirect automatic unblocking
		//Res &= Test->TestTrue(TEXT("Spear should be in right hand"), RHSpear.ItemId == ItemIdSpear);
		//Res &= Test->TestTrue(TEXT("Left hand should be empty"), ViewModel->IsTaggedSlotEmpty(LeftHandSlot));

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

		// TODO: We dont currently support indirect automatic unblocking
		//Res &= Test->TestTrue(TEXT("Move longbow to any slot, which should be left. It should move blocking rock."), ViewModel->MoveItemToAnyTaggedSlot(NoTag, 0));
		Res &= ViewModel->AssertViewModelSettled();

		//LHLongbow = ViewModel->GetItemForTaggedSlot(LeftHandSlot);
		//Res &= Test->TestTrue(TEXT("Longbow should be in left hand"), LHLongbow.ItemId == ItemIdLongbow);
		//Res &= Test->TestTrue(TEXT("Right hand should be empty"), ViewModel->IsTaggedSlotEmpty(RightHandSlot));
		//Res &= Test->TestEqual(TEXT("Generic slot should contain the rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 1);

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
		Res &= Test->TestFalse(TEXT("Cannot add Giant Boulder due to weight restrictions"), ViewModel->CanTaggedSlotReceiveItem(GiantBoulder, RightHandSlot, false));

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

		ViewModel->DropItem(NoTag, 8, 1);
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
        ViewModel->DropItem(NoTag, 0, 1);
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
        ViewModel->UseItem(NoTag, 0);
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
        ViewModel->UseItem(LeftHandSlot);
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

		bool TestMoveItemToOtherViewModel()
    {
        // --- Setup ---
        GridViewModelTestContext ContextA(100.0f, 9, false); ContextA.TempActor->Rename(TEXT("ActorA"));
        GridViewModelTestContext ContextB(100.0f, 9, false); ContextB.TempActor->Rename(TEXT("ActorB"));

        auto* InvA = ContextA.InventoryComponent;
        auto* VMA = ContextA.ViewModel;
        auto* InvB = ContextB.InventoryComponent;
        auto* VMB = ContextB.ViewModel;
        auto* Subsystem = ContextA.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;
        bool bMoveSuccess = false;
        int32 ExpectedQuantity = 0;

        auto ClearInventories = [&]() {
            InvA->Clear_IfServer();
            InvB->Clear_IfServer();
            Res &= VMA->AssertViewModelSettled();
            Res &= VMB->AssertViewModelSettled();
        };

        // --- Test Group 1: Grid <-> Grid (No Instances) ---
        UE_LOG(LogTemp, Log, TEXT("--- TestMoveItemToOtherViewModel: Grid <-> Grid (No Instances) ---"));
        ClearInventories();

        // 1a. Full Stack Move (A->B)
        InvA->AddItemToAnySlot(Subsystem, FiveRocks); // A:0 = 5R
        Res &= VMA->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, NoTag, 0); // Move A:0 -> B:0 (Full)
        Res &= Test->TestTrue(TEXT("[1a] Move Grid->Grid FullStack Initiated"), bMoveSuccess);
        // Check state immediately after the synchronous call
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[1a] VMA Slot 0 empty settled"), VMA->IsGridSlotEmpty(0));
        Res &= Test->TestTrue(TEXT("[1a] VMB Slot 0 has 5R settled"), VMB->GetGridItem(0).ItemId == ItemIdRock && VMB->GetGridItem(0).Quantity == 5);
        Res &= Test->TestEqual(TEXT("[1a] InvA Rocks"), InvA->GetQuantityTotal_Implementation(ItemIdRock), 0);
        Res &= Test->TestEqual(TEXT("[1a] InvB Rocks"), InvB->GetQuantityTotal_Implementation(ItemIdRock), 5);
		UE_LOG(LogTemp, Log, TEXT("State after 1a: VMA Empty || VMB Grid[0]=5R"));

        // 1b. Partial Stack Move (Split B->A)
        // Context: B:0=5R, A is empty
        bMoveSuccess = VMB->MoveItemToOtherViewModel(NoTag, 0, VMA, NoTag, 0, 2); // Move 2 Rocks B:0 -> A:0
        Res &= Test->TestTrue(TEXT("[1b] Move Grid->Grid Split Initiated"), bMoveSuccess);
        // Check state immediately after
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestEqual(TEXT("[1b] VMB Slot 0 has 3R settled"), VMB->GetGridItem(0).Quantity, 3);
        Res &= Test->TestTrue(TEXT("[1b] VMA Slot 0 has 2R settled"), VMA->GetGridItem(0).ItemId == ItemIdRock && VMA->GetGridItem(0).Quantity == 2);
        Res &= Test->TestEqual(TEXT("[1b] InvA Rocks"), InvA->GetQuantityTotal_Implementation(ItemIdRock), 2);
        Res &= Test->TestEqual(TEXT("[1b] InvB Rocks"), InvB->GetQuantityTotal_Implementation(ItemIdRock), 3);
		UE_LOG(LogTemp, Log, TEXT("State after 1b: VMA Grid[0]=2R || VMB Grid[0]=3R"));

        // --- Test Group 2: Tagged <-> Grid (No Instances) ---
        UE_LOG(LogTemp, Log, TEXT("--- TestMoveItemToOtherViewModel: Tagged <-> Grid (No Instances) ---"));
        ClearInventories();

        // 2a. Tagged (A) -> Grid (B) (Sticks)
        InvA->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ThreeSticks); // A:LH = 3S
        Res &= VMA->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(LeftHandSlot, -1, VMB, NoTag, 0); // Move A:LH -> B:0 (Full)
        Res &= Test->TestTrue(TEXT("[2a] Move Tagged->Grid Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[2a] VMA LH empty settled"), VMA->IsTaggedSlotEmpty(LeftHandSlot));
        Res &= Test->TestTrue(TEXT("[2a] VMB Slot 0 has 3S settled"), VMB->GetGridItem(0).ItemId == ItemIdSticks && VMB->GetGridItem(0).Quantity == 3);
        Res &= Test->TestEqual(TEXT("[2a] InvA Sticks"), InvA->GetQuantityTotal_Implementation(ItemIdSticks), 0);
        Res &= Test->TestEqual(TEXT("[2a] InvB Sticks"), InvB->GetQuantityTotal_Implementation(ItemIdSticks), 3);
		UE_LOG(LogTemp, Log, TEXT("State after 2a: VMA Empty || VMB Grid[0]=3S"));

        // 2b. Grid (B) -> Tagged (A) (Helmet)
        InvB->AddItemToAnySlot(Subsystem, OneHelmet); // B:1 = 1H (assuming B:0 has sticks)
        Res &= VMB->AssertViewModelSettled();
        bMoveSuccess = VMB->MoveItemToOtherViewModel(NoTag, 1, VMA, HelmetSlot, -1); // Move B:1 -> A:Helmet
        Res &= Test->TestTrue(TEXT("[2b] Move Grid->Tagged Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[2b] VMB Slot 1 empty settled"), VMB->IsGridSlotEmpty(1));
        Res &= Test->TestTrue(TEXT("[2b] VMA HelmetSlot has 1H settled"), VMA->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
        Res &= Test->TestEqual(TEXT("[2b] InvA Helmets"), InvA->GetQuantityTotal_Implementation(ItemIdHelmet), 1);
        Res &= Test->TestEqual(TEXT("[2b] InvB Helmets"), InvB->GetQuantityTotal_Implementation(ItemIdHelmet), 0);
		UE_LOG(LogTemp, Log, TEXT("State after 2b: VMA Tags[Helmet]=1H || VMB Grid[0]=3S"));


        // --- Test Group 3: Tagged <-> Tagged (No Instances) ---
        UE_LOG(LogTemp, Log, TEXT("--- TestMoveItemToOtherViewModel: Tagged <-> Tagged (No Instances) ---"));
        ClearInventories();

        // 3a. Tagged (A) -> Tagged (B) (Chest Armor)
        InvA->AddItemToTaggedSlot_IfServer(Subsystem, ChestSlot, OneChestArmor); // A:Chest = 1C
        Res &= VMA->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(ChestSlot, -1, VMB, ChestSlot, -1); // Move A:Chest -> B:Chest
        Res &= Test->TestTrue(TEXT("[3a] Move Tagged->Tagged Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[3a] VMA Chest empty settled"), VMA->IsTaggedSlotEmpty(ChestSlot));
        Res &= Test->TestTrue(TEXT("[3a] VMB Chest has 1C settled"), VMB->GetItemForTaggedSlot(ChestSlot).ItemId == ItemIdChestArmor);
        Res &= Test->TestEqual(TEXT("[3a] InvA Armor"), InvA->GetQuantityTotal_Implementation(ItemIdChestArmor), 0);
        Res &= Test->TestEqual(TEXT("[3a] InvB Armor"), InvB->GetQuantityTotal_Implementation(ItemIdChestArmor), 1);
		// State after 3a: VMA Empty || VMB Tags[Chest]=1C

        // 3b. Tagged (B) -> Tagged (A) (Swap Scenario - Add Helmet to A:Helmet)
        InvA->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet); // A:Helmet = 1H
        Res &= VMA->AssertViewModelSettled();
        // Context: B:Chest=1C, A:Helmet=1H
        bMoveSuccess = VMB->MoveItemToOtherViewModel(ChestSlot, -1, VMA, HelmetSlot, -1, 0); // Try Move B:Chest -> A:Helmet (Invalid)
        Res &= Test->TestFalse(TEXT("[3b] Move Tagged->Tagged Swap with disallowed Item"), bMoveSuccess); // Should initiate swap
        // Server logic handles the swap back internally
        Res &= Test->TestTrue(TEXT("[3b] VMB Chest has 1H settled (Unchanged)"), VMB->GetItemForTaggedSlot(ChestSlot).ItemId == ItemIdChestArmor);
		auto AHelmetSlotItem = VMA->GetItemForTaggedSlot(HelmetSlot);
        Res &= Test->TestTrue(TEXT("[3b] VMA Helmet has 1H Unchanged"), AHelmetSlotItem.ItemId == ItemIdHelmet);
        Res &= Test->TestEqual(TEXT("[3b] InvA unchanged Armor"), InvA->GetQuantityTotal_Implementation(ItemIdChestArmor), 0);
        Res &= Test->TestEqual(TEXT("[3b] InvB unchanged Armor"), InvB->GetQuantityTotal_Implementation(ItemIdChestArmor), 1);
        Res &= Test->TestEqual(TEXT("[3b] InvA unchanged Helmets"), InvA->GetQuantityTotal_Implementation(ItemIdHelmet), 1);
		Res &= Test->TestEqual(TEXT("[3b] InvB unchanged Helmets"), InvB->GetQuantityTotal_Implementation(ItemIdHelmet), 0);
		Res &= VMA->AssertViewModelSettled();
		Res &= VMB->AssertViewModelSettled();
		// State after 3b: VMA Tags[Helmet]=1H || VMB Tags[Chest]=1C

		// 3c. MoveToUniversalSlot -> From Universal slot to disallowed slot
        UE_LOG(LogTemp, Log, TEXT("--- Test 3c: Tagged(Specialized B) -> Tagged(Universal A) ---"));
        bMoveSuccess = VMB->MoveItemToOtherViewModel(ChestSlot, -1, VMA, RightHandSlot, -1); // B:Chest -> A:RH
        Res &= Test->TestTrue(TEXT("[3c] Move Chest->RH Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[3c] VMA RH has 1C settled"), VMA->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdChestArmor);
        Res &= Test->TestTrue(TEXT("[3c] VMB Chest empty settled"), VMB->IsTaggedSlotEmpty(ChestSlot));
        Res &= Test->TestEqual(TEXT("[3c] InvA Armor"), InvA->GetQuantityTotal_Implementation(ItemIdChestArmor), 1);
        Res &= Test->TestEqual(TEXT("[3c] InvB Armor"), InvB->GetQuantityTotal_Implementation(ItemIdChestArmor), 0);
        // Helmet remains in A:Helmet
        Res &= Test->TestEqual(TEXT("[3c] InvA Helmets"), InvA->GetQuantityTotal_Implementation(ItemIdHelmet), 1);
        Res &= Test->TestEqual(TEXT("[3c] InvB Helmets"), InvB->GetQuantityTotal_Implementation(ItemIdHelmet), 0);
		UE_LOG(LogTemp, Log, TEXT("State after 3c: VMA Tags[Helmet]=1H, Tags[RH]=1C || VMB Empty"));

        // 3d. Attempt Move Chest Armor (A:RH) -> Helmet Slot (A:Helmet) (Disallowed - Swap Back Check)
        // Context: A:Helmet=1H, A:RH=1C
        UE_LOG(LogTemp, Log, TEXT("--- Test 3d: Tagged(Universal A) -> Tagged(Specialized A - Occupied/Incompatible Swap) ---"));
        // We need to use VMA->MoveItem for internal moves
        bMoveSuccess = VMA->MoveItem(RightHandSlot, -1, HelmetSlot, -1); // Try move Chest A:RH -> A:Helmet (Occupied by Helmet)
        Res &= Test->TestFalse(TEXT("[3d] Move Chest->Helmet(Occupied) returned false (Incompatible Swap)"), bMoveSuccess);
        // No server simulation needed for internal VM move failure based on prediction/validation
        Res &= VMA->AssertViewModelSettled(); // Ensure state is still consistent
        Res &= Test->TestTrue(TEXT("[3d] VMA RH still has 1C settled"), VMA->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdChestArmor);
        Res &= Test->TestTrue(TEXT("[3d] VMA Helmet still has 1H settled"), VMA->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
        Res &= Test->TestEqual(TEXT("[3d] InvA Armor"), InvA->GetQuantityTotal_Implementation(ItemIdChestArmor), 1);
        Res &= Test->TestEqual(TEXT("[3d] InvA Helmets"), InvA->GetQuantityTotal_Implementation(ItemIdHelmet), 1);
        // B remains empty
		UE_LOG(LogTemp, Log, TEXT("State after 3d: VMA Tags[Helmet]=1H, Tags[RH]=1C || VMB Empty"));

        // 3e. Attempt Move Helmet (A:Helmet) -> Hand Slot (A:RH) (Occupied by Chest - Illegal Swap Back)
        // Context: A:Helmet=1H, A:RH=1C
        UE_LOG(LogTemp, Log, TEXT("--- Test 3e: Tagged(Specialized A) -> Tagged(Universal A - Occupied/Incompatible Swap) ---"));
        bMoveSuccess = VMA->MoveItem(HelmetSlot, -1, RightHandSlot, -1); // Try move Helmet A:Helmet -> A:RH (Occupied by Chest)
        Res &= Test->TestFalse(TEXT("[3e] Move Helmet->RH(Occupied) returned false (Incompatible Swap Back)"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[3e] VMA RH still has 1C settled"), VMA->GetItemForTaggedSlot(RightHandSlot).ItemId == ItemIdChestArmor);
        Res &= Test->TestTrue(TEXT("[3e] VMA Helmet still has 1H settled"), VMA->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
        Res &= Test->TestEqual(TEXT("[3e] InvA Armor"), InvA->GetQuantityTotal_Implementation(ItemIdChestArmor), 1);
        Res &= Test->TestEqual(TEXT("[3e] InvA Helmets"), InvA->GetQuantityTotal_Implementation(ItemIdHelmet), 1);
		UE_LOG(LogTemp, Log, TEXT("State after 3e: VMA Tags[Helmet]=1H, Tags[RH]=1C || VMB Empty"));

        // --- Test Group 4: Instance Data Transfers ---
        ClearInventories();

        // 4a. Grid (A) -> Grid (B) - Stackable Instances (Eggs)
        InvA->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 2); // A:0 = 2E
        Res &= VMA->AssertViewModelSettled();
        FItemBundle EggBundleA_Start = VMA->GetGridItem(0);
        TArray<UItemInstanceData*> EggInstancesA = EggBundleA_Start.InstanceData;
        Res &= Test->TestEqual(TEXT("[4a] VMA Slot 0 starts with 2 egg instances"), EggInstancesA.Num(), 2);
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, NoTag, 0); // Move A:0 -> B:0 (Full)
        Res &= Test->TestTrue(TEXT("[4a] Move InstGrid->Grid Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[4a] VMA Slot 0 empty settled"), VMA->IsGridSlotEmpty(0));
        Res &= CompareInstanceArrays(Test, TEXT("[4a] VMB Slot 0 has correct instances settled"), VMB->GetGridItem(0).InstanceData, EggInstancesA);
        Res &= Test->TestEqual(TEXT("[4a] InvA Eggs"), InvA->GetQuantityTotal_Implementation(ItemIdBrittleEgg), 0);
        Res &= Test->TestEqual(TEXT("[4a] InvB Eggs"), InvB->GetQuantityTotal_Implementation(ItemIdBrittleEgg), 2);
        if(EggInstancesA.Num() > 0) {
            Res &= Test->TestFalse(TEXT("[4a] Instance 0 Unregistered from A"), ContextA.TempActor->IsReplicatedSubObjectRegistered(EggInstancesA[0]));
            Res &= Test->TestTrue(TEXT("[4a] Instance 0 Registered with B"), ContextB.TempActor->IsReplicatedSubObjectRegistered(EggInstancesA[0]));
        }
		UE_LOG(LogTemp, Log, TEXT("State after 4a: VMA Empty || VMB Grid[0]=2E"));

        // 4b. Tagged (A) -> Grid (B) - Single Instance (Knife)
        InvA->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1); // A:RH = 1K
        Res &= VMA->AssertViewModelSettled();
        FItemBundle KnifeBundleA_Start = VMA->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstanceA = KnifeBundleA_Start.InstanceData.Num() > 0 ? KnifeBundleA_Start.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[4b] Knife Instance A valid"), KnifeInstanceA);
        bMoveSuccess = VMA->MoveItemToOtherViewModel(RightHandSlot, -1, VMB, NoTag, 1); // Move A:RH -> B:1
        Res &= Test->TestTrue(TEXT("[4b] Move InstTagged->Grid Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[4b] VMA RH empty settled"), VMA->IsTaggedSlotEmpty(RightHandSlot));
        Res &= CompareInstanceArrays(Test, TEXT("[4b] VMB Slot 1 has correct instance settled"), VMB->GetGridItem(1).InstanceData, {KnifeInstanceA});
        Res &= Test->TestEqual(TEXT("[4b] InvA Knives"), InvA->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife), 0);
        Res &= Test->TestEqual(TEXT("[4b] InvB Knives"), InvB->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife), 1);
         if(KnifeInstanceA) {
            Res &= Test->TestFalse(TEXT("[4b] Instance Unregistered from A"), ContextA.TempActor->IsReplicatedSubObjectRegistered(KnifeInstanceA));
            Res &= Test->TestTrue(TEXT("[4b] Instance Registered with B"), ContextB.TempActor->IsReplicatedSubObjectRegistered(KnifeInstanceA));
        }
		UE_LOG(LogTemp, Log, TEXT("State after 4b: VMA Empty || VMB Grid[0]=2E, Grid[1]=1K"));

        // 4c. Grid (B) -> Tagged (A) - Partial Stackable Instances (Eggs back to A)
        // Context: B:0=2E, B:1=1K. A is empty.
        FItemBundle EggBundleB_Start = VMB->GetGridItem(0);
        TArray<UItemInstanceData*> EggInstancesB = EggBundleB_Start.InstanceData;
        UItemInstanceData* EggToMove = EggInstancesB.Num() > 0 ? EggInstancesB.Last() : nullptr; // Get last egg instance
        Res &= Test->TestNotNull(TEXT("[4c] Egg Instance B valid"), EggToMove);
        bMoveSuccess = VMB->MoveItemToOtherViewModel(NoTag, 0, VMA, LeftHandSlot, -1, 1); // Move 1 Egg B:0 -> A:LH
        Res &= Test->TestTrue(TEXT("[4c] Move InstGrid->Tagged Split Initiated"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        Res &= Test->TestEqual(TEXT("[4c] VMB Slot 0 has 1E settled"), VMB->GetGridItem(0).Quantity, 1);
        Res &= CompareInstanceArrays(Test, TEXT("[4c] VMA LH has correct instance settled"), VMA->GetItemForTaggedSlot(LeftHandSlot).InstanceData, {EggToMove});
        Res &= Test->TestEqual(TEXT("[4c] InvA Eggs"), InvA->GetQuantityTotal_Implementation(ItemIdBrittleEgg), 1);
        Res &= Test->TestEqual(TEXT("[4c] InvB Eggs"), InvB->GetQuantityTotal_Implementation(ItemIdBrittleEgg), 1);
        if(EggToMove) {
            Res &= Test->TestFalse(TEXT("[4c] Instance Unregistered from B"), ContextB.TempActor->IsReplicatedSubObjectRegistered(EggToMove));
            Res &= Test->TestTrue(TEXT("[4c] Instance Registered with A"), ContextA.TempActor->IsReplicatedSubObjectRegistered(EggToMove));
        }
		UE_LOG(LogTemp, Log, TEXT("State after 4c: VMA Tags[LH]=1E || VMB Grid[0]=1E, Grid[1]=1K"));

        // --- Test Group 5: Failure Cases ---
        UE_LOG(LogTemp, Log, TEXT("--- TestMoveItemToOtherViewModel: Failure Cases ---"));
        ClearInventories();

        // 5a. Source Empty
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, NoTag, 0, 0); // A:0 (Empty) -> B:0
        Res &= Test->TestFalse(TEXT("[5a] Move from empty grid returned false"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();

        // 5b. Target Incompatible
        InvA->AddItemToAnySlot(Subsystem, OneRock); // A:0 = 1R
        InvB->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet); // B:Helmet = 1H
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, HelmetSlot, -1, 0); // A:0 (Rock) -> B:Helmet
        Res &= Test->TestFalse(TEXT("[5b] Move Rock to HelmetSlot returned false"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled(); // State should remain unchanged
        Res &= Test->TestTrue(TEXT("[5b] VMA Slot 0 unchanged settled"), VMA->GetGridItem(0).ItemId == ItemIdRock);
        Res &= Test->TestTrue(TEXT("[5b] VMB HelmetSlot unchanged settled"), VMB->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);

        // 5c. Target Full (Stacking)
        InvA->AddItemToAnySlot(Subsystem, FiveRocks); // A:0 = 5R, A1: 1R
        InvB->Clear_IfServer(); // Clear B
        InvB->AddItemToAnySlot(Subsystem, FiveRocks); // B:0 = 5R
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 1, VMB, NoTag, 0, 1); // Try split 1 Rock A:1 -> B:0 (Full)
        Res &= Test->TestFalse(TEXT("[5c] Move Rock to full Rock slot returned false"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        Res &= Test->TestEqual(TEXT("[5c] VMA Slot 1 unchanged"), VMA->GetGridItem(1).Quantity, 1);
        Res &= Test->TestEqual(TEXT("[5c] VMB Slot 0 unchanged"), VMB->GetGridItem(0).Quantity, 5);

        // 5d. Target Full (Non-Stacking)
        InvA->Clear_IfServer(); // Clear A
        InvA->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferGenericInventory); // A:0 = 1H
        InvB->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot); // B:H = 1H
		Res &= Test->TestTrue(TEXT("[5d] VMB HelmetSlot has 1H"), VMB->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);
		Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
		// TODO: Same itemid move not yet properly defined
        //bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, HelmetSlot, -1); // Try move Helmet A:0 -> B:Helmet (Full)
        //Res &= Test->TestTrue(TEXT("[5d] Move Helmet to full Helmet slot"), bMoveSuccess);
        //Res &= VMA->AssertViewModelSettled();
		//Res &= VMB->AssertViewModelSettled();
        //Res &= Test->TestTrue(TEXT("[5d] VMA Slot 0 changed"), VMA->GetGridItem(0).ItemId != ItemIdHelmet);
        //Res &= Test->TestTrue(TEXT("[5d] VMB HelmetSlot changed"), VMB->GetItemForTaggedSlot(HelmetSlot).ItemId == ItemIdHelmet);

        // 5e. Target Blocked
        InvA->Clear_IfServer(); // Clear A
        InvA->AddItemToAnySlot(Subsystem, OneSpear); // A:0 = 1S
        InvB->Clear_IfServer(); // Clear B
        InvB->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock); // B:LH = 1R (Blocks B:RH for spear)
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, RightHandSlot, -1, 0); // A:0(Spear) -> B:RH(Blocked)
        Res &= Test->TestFalse(TEXT("[5e] Move Spear to blocked RH returned false"), bMoveSuccess);
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        Res &= Test->TestTrue(TEXT("[5e] VMA Slot 0 unchanged"), VMA->GetGridItem(0).ItemId == ItemIdSpear);
        Res &= Test->TestTrue(TEXT("[5e] VMB RH unchanged"), VMB->IsTaggedSlotEmpty(RightHandSlot));
        Res &= Test->TestTrue(TEXT("[5e] VMB LH unchanged"), VMB->GetItemForTaggedSlot(LeftHandSlot).ItemId == ItemIdRock);

        // 5f. Target cannot receive due to weight/slot limits (server-side rejection)
        ClearInventories();
        InvA->AddItemToAnySlot(Subsystem, FiveRocks, EPreferredSlotPolicy::PreferGenericInventory); // A:0 = 5R
        InvB->MaxWeight = 2; // B can only hold 2 rocks
        Res &= VMA->AssertViewModelSettled() && VMB->AssertViewModelSettled();
        bMoveSuccess = VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, NoTag, 0); // Try move 5 Rocks A:0 -> B:0
        Res &= Test->TestFalse(TEXT("[5f] Move Rock to limited Inv failed as the full stack could not be moved"), bMoveSuccess);
        // Check visual state immediately after prediction
        Res &= Test->TestFalse(TEXT("[5f] VMA Slot 0 unchanged in vm"), VMA->IsGridSlotEmpty(0));
        Res &= Test->TestFalse(TEXT("[5f] VMB Slot 0 has 0 rocks"), VMB->GetGridItem(0).IsValid());
        // Perform partial call on underlying inventory (should transfer partial
        InvA->RequestMoveItemToOtherContainer(InvB, ItemIdRock, 5, {}, NoTag, NoTag);
		Res &= VMA->AssertViewModelSettled();
		Res &= VMB->AssertViewModelSettled();
        // Check final settled state after server correction
        Res &= Test->TestTrue(TEXT("[5f] VMA Slot 0 has 3R settled (Returned)"), VMA->GetGridItem(0).ItemId == ItemIdRock && VMA->GetGridItem(0).Quantity == 3);
        Res &= Test->TestTrue(TEXT("[5f] VMB Slot 0 has 2R settled (Accepted)"), VMB->GetGridItem(0).ItemId == ItemIdRock && VMB->GetGridItem(0).Quantity == 2);
        Res &= Test->TestEqual(TEXT("[5f] InvA rocks"), InvA->GetQuantityTotal_Implementation(ItemIdRock), 3);
        Res &= Test->TestEqual(TEXT("[5f] InvB rocks"), InvB->GetQuantityTotal_Implementation(ItemIdRock), 2);


        return Res;
    }

	bool TestRecursiveContainers()
    {
        GridViewModelTestContext ContextA(200.0f, 10, false); ContextA.TempActor->Rename(TEXT("ActorA_Rec"));
        auto* InvA = ContextA.InventoryComponent;
        auto* VMA = ContextA.ViewModel;
        auto* Subsystem = ContextA.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;
        const float KnifeDurability = 88.f;

        // --- Phase 1: Basic Backpack Operations (VMA) ---
        // VMA: Empty
        InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Backpack(1)
        FItemBundle BackpackBundleA_VMA_Grid0 = VMA->GetGridItem(0);
        bool BackpackInGrid0_VMA = BackpackBundleA_VMA_Grid0.ItemId == ItemIdBackpack && BackpackBundleA_VMA_Grid0.Quantity == 1;
        Res &= Test->TestTrue(TEXT("[Rec P1] VMA Grid[0] has Backpack"), BackpackInGrid0_VMA);
        int32 BackpackInstanceCount_VMA = BackpackBundleA_VMA_Grid0.InstanceData.Num();
        Res &= Test->TestTrue(TEXT("[Rec P1] VMA Backpack instance count is 1"), BackpackInstanceCount_VMA == 1);

        URecursiveContainerInstanceData* RCI_A_Backpack = nullptr;
        if (BackpackBundleA_VMA_Grid0.InstanceData.Num() > 0) {
            RCI_A_Backpack = Cast<URecursiveContainerInstanceData>(BackpackBundleA_VMA_Grid0.InstanceData[0]);
        }
        Res &= Test->TestTrue(TEXT("[Rec P1] Backpack instance data is URecursiveContainerInstanceData"), RCI_A_Backpack != nullptr);
        if (!RCI_A_Backpack) return false; 

        UItemContainerComponent* SubInv_A_Backpack = RCI_A_Backpack->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P1] Backpack (A) has a valid sub-container"), SubInv_A_Backpack != nullptr);
        if (!SubInv_A_Backpack) return false;
        AActor* SubInv_A_BackpackOwner = SubInv_A_Backpack->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P1] Backpack (A) sub-container owner is ActorA"), SubInv_A_BackpackOwner == ContextA.TempActor);

        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Empty]
        int32 AddedToSubA_Rocks = SubInv_A_Backpack->AddItem_IfServer(Subsystem, ItemIdRock, 2, false);
        Res &= Test->TestTrue(TEXT("[Rec P1] Added 2 Rocks to SubInv_A_Backpack"), AddedToSubA_Rocks == 2);
        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Contains Rock(2)]
        int32 RocksInSubInvA = SubInv_A_Backpack->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P1] SubInv_A_Backpack contains 2 Rocks"), RocksInSubInvA == 2);

        int32 AddedToSubA_Sticks = SubInv_A_Backpack->AddItem_IfServer(Subsystem, ItemIdSticks, 3, false);
        Res &= Test->TestTrue(TEXT("[Rec P1] Added 3 Sticks to SubInv_A_Backpack"), AddedToSubA_Sticks == 3);
        int32 SticksInSubInvA = SubInv_A_Backpack->GetQuantityTotal_Implementation(ItemIdSticks);
        Res &= Test->TestTrue(TEXT("[Rec P1] SubInv_A_Backpack contains 3 Sticks"), SticksInSubInvA == 3);
        
        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Contains Rock(2), Sticks(3)]
        int32 AddedToSubA_Purse = SubInv_A_Backpack->AddItem_IfServer(Subsystem, ItemIdCoinPurse, 1, false);
        Res &= Test->TestTrue(TEXT("[Rec P1] Added 1 CoinPurse to SubInv_A_Backpack"), AddedToSubA_Purse == 1);
        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Contains Rock(2), Sticks(3), CoinPurse(1)]
        int32 PurseInSubInvA = SubInv_A_Backpack->GetQuantityTotal_Implementation(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Rec P1] SubInv_A_Backpack contains CoinPurse"), PurseInSubInvA == 1);
        
        URecursiveContainerInstanceData* RCI_A_Purse = nullptr;
        TArray<UItemInstanceData*> PurseInstanceDatasInSubA = SubInv_A_Backpack->GetItemInstanceData(ItemIdCoinPurse);
        if(PurseInstanceDatasInSubA.Num() > 0) {
            RCI_A_Purse = Cast<URecursiveContainerInstanceData>(PurseInstanceDatasInSubA[0]);
        }
        Res &= Test->TestTrue(TEXT("[Rec P1] CoinPurse instance data in SubInv_A_Backpack is URecursiveContainerInstanceData"), RCI_A_Purse != nullptr);
        if (!RCI_A_Purse) return false;

        UItemContainerComponent* SubInv_A_Purse = RCI_A_Purse->RepresentedContainer; 
        Res &= Test->TestTrue(TEXT("[Rec P1] CoinPurse (A) has a valid sub-sub-container"), SubInv_A_Purse != nullptr);
        if (!SubInv_A_Purse) return false;
        AActor* SubInv_A_PurseOwner = SubInv_A_Purse->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P1] CoinPurse (A) sub-sub-container owner is ActorA"), SubInv_A_PurseOwner == ContextA.TempActor);

        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Rock(2),Sticks(3),CoinPurse(1)[SubInv_A_Purse:Empty]]
        int32 AddedToSubSubA_Knife = SubInv_A_Purse->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 1, false);
        Res &= Test->TestTrue(TEXT("[Rec P1] Added 1 Knife to SubInv_A_Purse"), AddedToSubSubA_Knife == 1);
        TArray<UItemInstanceData*> KnifeInstanceDatasInPurseA = SubInv_A_Purse->GetItemInstanceData(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeDurabilityInstanceA = nullptr;
        if (KnifeInstanceDatasInPurseA.Num() > 0) KnifeDurabilityInstanceA = Cast<UItemDurabilityTestInstanceData>(KnifeInstanceDatasInPurseA[0]);
        Res &= Test->TestTrue(TEXT("[Rec P1] Knife instance data in purse is valid"), KnifeDurabilityInstanceA != nullptr);
        if(KnifeDurabilityInstanceA) KnifeDurabilityInstanceA->Durability = KnifeDurability;

        int32 AddedToSubSubA_Eggs = SubInv_A_Purse->AddItem_IfServer(Subsystem, ItemIdBrittleEgg, 2, false);
        Res &= Test->TestTrue(TEXT("[Rec P1] Added 2 Eggs to SubInv_A_Purse"), AddedToSubSubA_Eggs == 2);
        // VMA: Grid[0]=Backpack(1) [SubInv_A_Backpack: Rock(2),Sticks(3),CoinPurse(1)[SubInv_A_Purse:Knife(1),Eggs(2)]]
        int32 KnivesInPurseA = SubInv_A_Purse->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife);
        Res &= Test->TestTrue(TEXT("[Rec P1] SubInv_A_Purse contains 1 Knife"), KnivesInPurseA == 1);
        int32 EggsInPurseA = SubInv_A_Purse->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P1] SubInv_A_Purse contains 2 Eggs"), EggsInPurseA == 2);

        // --- Phase 2: Drop and Pickup (VMA -> World -> VMB) ---
        UItemInstanceData* BackpackInstancePtrBeforeDrop = RCI_A_Backpack;
        int32 DroppedQtyFromVMA = VMA->DropItem(NoTag, 0, 1); 
        Res &= Test->TestTrue(TEXT("[Rec P2] DropItem from VMA returned 1"), DroppedQtyFromVMA == 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        bool VMA_Grid0_EmptyAfterDrop = VMA->IsGridSlotEmpty(0);
        Res &= Test->TestTrue(TEXT("[Rec P2] VMA Grid[0] empty after dropping Backpack"), VMA_Grid0_EmptyAfterDrop);
        bool InvA_NoBackpack = !InvA->Contains(ItemIdBackpack);
        Res &= Test->TestTrue(TEXT("[Rec P2] InvA no longer contains Backpack"), InvA_NoBackpack);
        bool BackpackInstanceUnregisteredFromA = !ContextA.TempActor->IsReplicatedSubObjectRegistered(BackpackInstancePtrBeforeDrop);
        Res &= Test->TestTrue(TEXT("[Rec P2] Backpack instance unregistered from ActorA"), BackpackInstanceUnregisteredFromA);

        AWorldItem* DroppedBackpackWorldItem = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdBackpack) { DroppedBackpackWorldItem = *It; break; }
        }
        Res &= Test->TestTrue(TEXT("[Rec P2] Dropped Backpack WorldItem found"), DroppedBackpackWorldItem != nullptr);
        if (!DroppedBackpackWorldItem) return false;

        FItemBundle BackpackBundle_World = DroppedBackpackWorldItem->RepresentedItem;
        bool WorldBackpackValid = BackpackBundle_World.IsValid();
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack bundle is valid"), WorldBackpackValid);
        int32 WorldBackpackInstanceCount = BackpackBundle_World.InstanceData.Num();
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack instance count is 1"), WorldBackpackInstanceCount == 1);

        URecursiveContainerInstanceData* RCI_World_Backpack = nullptr;
        if(BackpackBundle_World.InstanceData.Num() > 0) { RCI_World_Backpack = Cast<URecursiveContainerInstanceData>(BackpackBundle_World.InstanceData[0]); }
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack instance is URecursiveContainerInstanceData"), RCI_World_Backpack != nullptr);
        if (!RCI_World_Backpack) return false;
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack instance is the same object as original"), RCI_World_Backpack == BackpackInstancePtrBeforeDrop);
        
        UItemContainerComponent* SubInv_World_Backpack = RCI_World_Backpack->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack has valid sub-container"), SubInv_World_Backpack != nullptr);
        if (!SubInv_World_Backpack) return false;
        AActor* SubInv_World_BackpackOwner = SubInv_World_Backpack->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack sub-container owner is WorldItem"), SubInv_World_BackpackOwner == DroppedBackpackWorldItem);
        
        int32 RocksInSubInvWorld = SubInv_World_Backpack->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack's sub-container contains 2 Rocks"), RocksInSubInvWorld == 2);
        int32 SticksInSubInvWorld = SubInv_World_Backpack->GetQuantityTotal_Implementation(ItemIdSticks);
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack's sub-container contains 3 Sticks"), SticksInSubInvWorld == 3);

        TArray<UItemInstanceData*> PurseInstanceDatasInWorldSub = SubInv_World_Backpack->GetItemInstanceData(ItemIdCoinPurse);
        int32 PurseInSubInvWorld = SubInv_World_Backpack->GetQuantityTotal_Implementation(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem Backpack's sub-container contains CoinPurse"), PurseInSubInvWorld == 1);
        URecursiveContainerInstanceData* RCI_World_Purse = nullptr;
        if(PurseInstanceDatasInWorldSub.Num() > 0) { RCI_World_Purse = Cast<URecursiveContainerInstanceData>(PurseInstanceDatasInWorldSub[0]); }
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem CoinPurse instance is URecursiveContainerInstanceData"), RCI_World_Purse != nullptr);
        if (!RCI_World_Purse) return false;

        UItemContainerComponent* SubInv_World_Purse = RCI_World_Purse->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem CoinPurse has valid sub-sub-container"), SubInv_World_Purse != nullptr);
        if (!SubInv_World_Purse) return false;
        AActor* SubInv_World_PurseOwner = SubInv_World_Purse->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem CoinPurse sub-sub-container owner is WorldItem"), SubInv_World_PurseOwner == DroppedBackpackWorldItem);
        
        int32 KnivesInWorldPurse = SubInv_World_Purse->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife);
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem CoinPurse's sub-sub-container contains 1 Knife"), KnivesInWorldPurse == 1);
        TArray<UItemInstanceData*> KnifeInstanceDatasInWorldPurse = SubInv_World_Purse->GetItemInstanceData(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeDurabilityInstanceWorld = nullptr;
        if(KnifeInstanceDatasInWorldPurse.Num() > 0) KnifeDurabilityInstanceWorld = Cast<UItemDurabilityTestInstanceData>(KnifeInstanceDatasInWorldPurse[0]);
        Res &= Test->TestTrue(TEXT("[Rec P2] Knife instance in world purse is valid"), KnifeDurabilityInstanceWorld != nullptr);
        if(KnifeDurabilityInstanceWorld) {
            float DurabilityInWorld = KnifeDurabilityInstanceWorld->Durability;
            Res &= Test->TestTrue(TEXT("[Rec P2] Knife durability preserved in WorldItem"), DurabilityInWorld == KnifeDurability);
        }
        int32 EggsInWorldPurse = SubInv_World_Purse->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P2] WorldItem CoinPurse's sub-sub-container contains 2 Eggs"), EggsInWorldPurse == 2);

        GridViewModelTestContext ContextB(200.0f, 10, false); ContextB.TempActor->Rename(TEXT("ActorB_Rec"));
        auto* InvB = ContextB.InventoryComponent;
        auto* VMB = ContextB.ViewModel;
        // VMB: Empty

        VMB->PickupItem(DroppedBackpackWorldItem, EPreferredSlotPolicy::PreferGenericInventory, true); 
        Res &= VMB->AssertViewModelSettled();
        // VMB: Grid[0]=Backpack(1)
        FItemBundle BackpackBundleB_VMB_Grid0 = VMB->GetGridItem(0);
        bool BackpackInGrid0_VMB = BackpackBundleB_VMB_Grid0.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P2] VMB Grid[0] has Backpack after pickup"), BackpackInGrid0_VMB);
        
        URecursiveContainerInstanceData* RCI_B_Backpack = nullptr;
        if (BackpackBundleB_VMB_Grid0.InstanceData.Num() > 0) { RCI_B_Backpack = Cast<URecursiveContainerInstanceData>(BackpackBundleB_VMB_Grid0.InstanceData[0]); }
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack instance data is valid"), RCI_B_Backpack != nullptr);
        if (!RCI_B_Backpack) return false;
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack instance is same object as original"), RCI_B_Backpack == BackpackInstancePtrBeforeDrop);
        bool BackpackInstanceRegisteredToB = ContextB.TempActor->IsReplicatedSubObjectRegistered(RCI_B_Backpack);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack instance re-registered to ActorB"), BackpackInstanceRegisteredToB);

        UItemContainerComponent* SubInv_B_Backpack = RCI_B_Backpack->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack (B) has valid sub-container"), SubInv_B_Backpack != nullptr);
        if (!SubInv_B_Backpack) return false;
        AActor* SubInv_B_BackpackOwner = SubInv_B_Backpack->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack (B) sub-container owner is ActorB"), SubInv_B_BackpackOwner == ContextB.TempActor);

        int32 RocksInSubInvB = SubInv_B_Backpack->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack's sub-container (B) contains 2 Rocks"), RocksInSubInvB == 2);
        int32 SticksInSubInvB = SubInv_B_Backpack->GetQuantityTotal_Implementation(ItemIdSticks);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack's sub-container (B) contains 3 Sticks"), SticksInSubInvB == 3);
        
        TArray<UItemInstanceData*> PurseInstanceDatasInSubB = SubInv_B_Backpack->GetItemInstanceData(ItemIdCoinPurse);
        int32 PurseInSubInvB = SubInv_B_Backpack->GetQuantityTotal_Implementation(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Backpack's sub-container (B) contains CoinPurse"), PurseInSubInvB == 1);
        URecursiveContainerInstanceData* RCI_B_Purse = nullptr;
        if(PurseInstanceDatasInSubB.Num() > 0) { RCI_B_Purse = Cast<URecursiveContainerInstanceData>(PurseInstanceDatasInSubB[0]); }
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up CoinPurse instance data in SubInv_B_Backpack is valid"), RCI_B_Purse != nullptr);
        if(!RCI_B_Purse) return false;
        
        UItemContainerComponent* SubInv_B_Purse = RCI_B_Purse->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up CoinPurse (B) has valid sub-sub-container"), SubInv_B_Purse != nullptr);
        if(!SubInv_B_Purse) return false;
        AActor* SubInv_B_PurseOwner = SubInv_B_Purse->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up CoinPurse (B) sub-sub-container owner is ActorB"), SubInv_B_PurseOwner == ContextB.TempActor);
        
        int32 KnivesInPurseB = SubInv_B_Purse->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up CoinPurse's sub-sub-container (B) contains 1 Knife"), KnivesInPurseB == 1);
        TArray<UItemInstanceData*> KnifeInstanceDatasInPurseB = SubInv_B_Purse->GetItemInstanceData(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeDurabilityInstanceB = nullptr;
        if(KnifeInstanceDatasInPurseB.Num() > 0) KnifeDurabilityInstanceB = Cast<UItemDurabilityTestInstanceData>(KnifeInstanceDatasInPurseB[0]);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Knife instance in purse B is valid"), KnifeDurabilityInstanceB != nullptr);
        if(KnifeDurabilityInstanceB) {
            float DurabilityInB = KnifeDurabilityInstanceB->Durability;
            Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up Knife durability preserved in Container B"), DurabilityInB == KnifeDurability);
        }
        int32 EggsInPurseB = SubInv_B_Purse->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P2] Picked-up CoinPurse's sub-sub-container (B) contains 2 Eggs"), EggsInPurseB == 2);

        // --- Phase 3: Operations within VMB ---
        // VMB: Grid[0]=Backpack(1) [Rock(2),Sticks(3),Purse(1)[Knife(1),Eggs(2)]]
        bool MovedToTagged_VMB = VMB->MoveItem(NoTag, 0, RightHandSlot, -1); 
        Res &= Test->TestTrue(TEXT("[Rec P3] Moved Backpack from Grid to RightHandSlot in VMB"), MovedToTagged_VMB);
        Res &= VMB->AssertViewModelSettled();
        // VMB: RH=Backpack(1) [Rock(2),Sticks(3),Purse(1)[Knife(1),Eggs(2)]]
        bool VMB_Grid0_EmptyAfterInternalMove = VMB->IsGridSlotEmpty(0);
        Res &= Test->TestTrue(TEXT("[Rec P3] VMB Grid[0] empty after internal move"), VMB_Grid0_EmptyAfterInternalMove);
        FItemBundle BackpackBundleB_VMB_RH = VMB->GetItemForTaggedSlot(RightHandSlot);
        bool BackpackInRH_VMB = BackpackBundleB_VMB_RH.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P3] VMB RightHandSlot has Backpack after internal move"), BackpackInRH_VMB);
        
        int32 RocksInSubB_AfterInternalMove_Qty = SubInv_B_Backpack->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P3] Rocks still in Backpack's sub-container after internal move in VMB"), RocksInSubB_AfterInternalMove_Qty == 2);

        InvB->AddItemToAnySlot(Subsystem, OneHelmet, EPreferredSlotPolicy::PreferSpecializedTaggedSlot);
        // VMB: RH=Backpack(1)[...], HelmetSlot=Helmet(1)
        bool MovedToHelmet_VMB = VMB->MoveItem(RightHandSlot, -1, HelmetSlot, -1); // Try move Backpack (RH) to HelmetSlot (occupied by Helmet)
        Res &= Test->TestFalse(TEXT("[Rec P3] Attempt to move Backpack to occupied, incompatible HelmetSlot should fail"), MovedToHelmet_VMB);
        Res &= VMB->AssertViewModelSettled();
        // VMB: RH=Backpack(1)[...], HelmetSlot=Helmet(1) (State should be unchanged)
        FItemBundle BackpackBundleB_VMB_RH_AfterFail = VMB->GetItemForTaggedSlot(RightHandSlot);
        bool BackpackStillInRH_VMB = BackpackBundleB_VMB_RH_AfterFail.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P3] Backpack still in VMB RH after failed incompatible move"), BackpackStillInRH_VMB);
        FItemBundle HelmetBundleB_VMB_Helmet_AfterFail = VMB->GetItemForTaggedSlot(HelmetSlot);
        bool HelmetStillInHelmet_VMB = HelmetBundleB_VMB_Helmet_AfterFail.ItemId == ItemIdHelmet;
        Res &= Test->TestTrue(TEXT("[Rec P3] Helmet still in VMB HelmetSlot after failed incompatible move"), HelmetStillInHelmet_VMB);

        // --- Phase 4: Transfer Backpack VMB -> VMA ---
        InvA->Clear_IfServer(); // VMA: Empty
        Res &= VMA->AssertViewModelSettled();
        // VMB: RH=Backpack(1)[...], HelmetSlot=Helmet(1) || VMA: Empty
        bool MovedToVMA = VMB->MoveItemToOtherViewModel(RightHandSlot, -1, VMA, NoTag, 0);
        Res &= Test->TestTrue(TEXT("[Rec P4] Moved Backpack from VMB (RH) to VMA (Grid[0])"), MovedToVMA);
        Res &= VMA->AssertViewModelSettled();
        Res &= VMB->AssertViewModelSettled();
        // VMB: HelmetSlot=Helmet(1) || VMA: Grid[0]=Backpack(1)[...]
        bool VMB_RH_EmptyAfterExternalMove = VMB->IsTaggedSlotEmpty(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Rec P4] VMB RightHandSlot empty after external move to VMA"), VMB_RH_EmptyAfterExternalMove);
        FItemBundle BackpackBundleA_VMA_AfterExternalMove = VMA->GetGridItem(0);
        bool BackpackInGrid0_VMA_AfterExternalMove = BackpackBundleA_VMA_AfterExternalMove.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P4] VMA Grid[0] has Backpack after external move"), BackpackInGrid0_VMA_AfterExternalMove);

        URecursiveContainerInstanceData* RCI_A_Backpack_AfterExternalMove = nullptr;
        if (BackpackBundleA_VMA_AfterExternalMove.InstanceData.Num() > 0) {
            RCI_A_Backpack_AfterExternalMove = Cast<URecursiveContainerInstanceData>(BackpackBundleA_VMA_AfterExternalMove.InstanceData[0]);
        }
        Res &= Test->TestTrue(TEXT("[Rec P4] Backpack instance in VMA after external move is valid"), RCI_A_Backpack_AfterExternalMove != nullptr);
        if (!RCI_A_Backpack_AfterExternalMove) return false;
        bool BackpackInstanceA_ReRegToA = ContextA.TempActor->IsReplicatedSubObjectRegistered(RCI_A_Backpack_AfterExternalMove);
        Res &= Test->TestTrue(TEXT("[Rec P4] Backpack instance re-registered to ActorA after external move"), BackpackInstanceA_ReRegToA);
        
        UItemContainerComponent* SubInv_A_Backpack_AfterExternalMove = RCI_A_Backpack_AfterExternalMove->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P4] Backpack sub-container in VMA after external move is valid"), SubInv_A_Backpack_AfterExternalMove != nullptr);
        if (!SubInv_A_Backpack_AfterExternalMove) return false;
        AActor* SubInv_A_Backpack_AfterExternalMove_Owner = SubInv_A_Backpack_AfterExternalMove->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P4] Backpack sub-container owner is ActorA after external move"), SubInv_A_Backpack_AfterExternalMove_Owner == ContextA.TempActor);
        
        int32 RocksInSubA_AfterExternalMove_Qty = SubInv_A_Backpack_AfterExternalMove->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P4] Rocks still in Backpack's sub-container after move to VMA"), RocksInSubA_AfterExternalMove_Qty == 2);
        
        TArray<UItemInstanceData*> PurseInstanceDatasInSubA_AfterExternal = SubInv_A_Backpack_AfterExternalMove->GetItemInstanceData(ItemIdCoinPurse);
        URecursiveContainerInstanceData* RCI_A_Purse_AfterExternalMove = nullptr;
        if(PurseInstanceDatasInSubA_AfterExternal.Num() > 0) {RCI_A_Purse_AfterExternalMove = Cast<URecursiveContainerInstanceData>(PurseInstanceDatasInSubA_AfterExternal[0]); }
        Res &= Test->TestTrue(TEXT("[Rec P4] Purse instance in Backpack (A) after external move is valid"), RCI_A_Purse_AfterExternalMove != nullptr);
        if(!RCI_A_Purse_AfterExternalMove) return false;

        UItemContainerComponent* SubInv_A_Purse_AfterExternalMove = RCI_A_Purse_AfterExternalMove->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P4] Purse sub-sub-container in VMA after external move is valid"), SubInv_A_Purse_AfterExternalMove != nullptr);
        if(!SubInv_A_Purse_AfterExternalMove) return false;

        int32 EggsInPurse_AfterExternalMove_Qty = SubInv_A_Purse_AfterExternalMove->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P4] 2 Eggs still in CoinPurse's sub-sub-container after move to VMA"), EggsInPurse_AfterExternalMove_Qty == 2);
        int32 KnivesInPurse_AfterExternalMove_Qty = SubInv_A_Purse_AfterExternalMove->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife);
        Res &= Test->TestTrue(TEXT("[Rec P4] Knife still in CoinPurse after move to VMA"), KnivesInPurse_AfterExternalMove_Qty == 1);


        // --- Phase 5: Complex Drop Scenario (Drop Backpack containing another Backpack) ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferGenericInventory); // Backpack1
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Backpack1(1)
        FItemBundle B1_VMA_Grid0 = VMA->GetGridItem(0);
        URecursiveContainerInstanceData* RCI_A_B1 = Cast<URecursiveContainerInstanceData>(B1_VMA_Grid0.InstanceData[0]);
        UItemContainerComponent* SubInv_A_B1 = RCI_A_B1->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_A_B1 valid"), SubInv_A_B1 != nullptr);
        if(!SubInv_A_B1) return false;
        
        SubInv_A_B1->AddItem_IfServer(Subsystem, ItemIdBackpack, 1, false); // Backpack2 inside Backpack1
        // VMA: Grid[0]=Backpack1(1) [SubInv_A_B1: Grid[0]=Backpack2(1)]
        TArray<UItemInstanceData*> B2_InstanceDatas_In_SubInv_A_B1 = SubInv_A_B1->GetItemInstanceData(ItemIdBackpack);
        URecursiveContainerInstanceData* RCI_A_B2 = Cast<URecursiveContainerInstanceData>(B2_InstanceDatas_In_SubInv_A_B1[0]);
        UItemContainerComponent* SubInv_A_B2 = RCI_A_B2->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_A_B2 valid"), SubInv_A_B2 != nullptr);
        if(!SubInv_A_B2) return false;

        SubInv_A_B2->AddItem_IfServer(Subsystem, ItemIdRock, 5, false); // Rocks inside Backpack2
        // VMA: Grid[0]=Backpack1(1) [SubInv_A_B1: Grid[0]=Backpack2(1) [SubInv_A_B2: Grid[0]=Rock(5)]]
        int32 RocksInB2_A = SubInv_A_B2->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P5] Rocks in Backpack2 (A) correct"), RocksInB2_A == 5);
        
        VMA->DropItem(NoTag, 0, 1); // Drop Backpack1
        Res &= VMA->AssertViewModelSettled();
        // VMA: Empty

		AWorldItem* DroppedB1_WorldItem = nullptr;
		UWorld* World = ContextA.TestFixture.GetWorld();
		for (TActorIterator<AWorldItem> It(World); It; ++It)
		{
			if (It->RepresentedItem.ItemId == ItemIdBackpack && It->RepresentedItem.InstanceData[0] == RCI_A_B1)
			{
				DroppedB1_WorldItem = *It;
				break;
			}
		}
        Res &= Test->TestTrue(TEXT("[Rec P5] Dropped Backpack1 WorldItem found"), DroppedB1_WorldItem != nullptr);
        if (!DroppedB1_WorldItem) return false;

        FItemBundle B1_Bundle_World = DroppedB1_WorldItem->RepresentedItem;
        URecursiveContainerInstanceData* RCI_W_B1 = Cast<URecursiveContainerInstanceData>(B1_Bundle_World.InstanceData[0]);
        UItemContainerComponent* SubInv_W_B1 = RCI_W_B1->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_W_B1 valid"), SubInv_W_B1 != nullptr);
        if(!SubInv_W_B1) return false;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_W_B1 owner is WorldItem"), SubInv_W_B1->GetOwner() == DroppedB1_WorldItem);

        TArray<UItemInstanceData*> B2_InstanceDatas_In_SubInv_W_B1 = SubInv_W_B1->GetItemInstanceData(ItemIdBackpack);
        int32 B2_Qty_In_SubInv_W_B1 = SubInv_W_B1->GetQuantityTotal_Implementation(ItemIdBackpack);
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_W_B1 contains Backpack2"), B2_Qty_In_SubInv_W_B1 == 1);
        URecursiveContainerInstanceData* RCI_W_B2 = Cast<URecursiveContainerInstanceData>(B2_InstanceDatas_In_SubInv_W_B1[0]);
        UItemContainerComponent* SubInv_W_B2 = RCI_W_B2->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_W_B2 valid"), SubInv_W_B2 != nullptr);
        if(!SubInv_W_B2) return false;
        Res &= Test->TestTrue(TEXT("[Rec P5] SubInv_W_B2 owner is WorldItem"), SubInv_W_B2->GetOwner() == DroppedB1_WorldItem);
        
        int32 RocksInB2_W = SubInv_W_B2->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P5] Rocks in Backpack2 (World) correct"), RocksInB2_W == 5);

        InvB->Clear_IfServer(); Res &= VMB->AssertViewModelSettled();
        // VMB: Empty
        VMB->PickupItem(DroppedB1_WorldItem, EPreferredSlotPolicy::PreferGenericInventory, true);
        Res &= VMB->AssertViewModelSettled();
        // VMB: Grid[0]=Backpack1(1) [SubInv_B_B1: Grid[0]=Backpack2(1) [SubInv_B_B2: Grid[0]=Rock(5)]]
        FItemBundle B1_VMB_Grid0 = VMB->GetGridItem(0);
        URecursiveContainerInstanceData* RCI_B_B1 = Cast<URecursiveContainerInstanceData>(B1_VMB_Grid0.InstanceData[0]);
        UItemContainerComponent* SubInv_B_B1 = RCI_B_B1->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] Picked up SubInv_B_B1 valid"), SubInv_B_B1 != nullptr);
        if(!SubInv_B_B1) return false;
        Res &= Test->TestTrue(TEXT("[Rec P5] Picked up SubInv_B_B1 owner is ActorB"), SubInv_B_B1->GetOwner() == ContextB.TempActor);
        
        TArray<UItemInstanceData*> B2_InstanceDatas_In_SubInv_B_B1 = SubInv_B_B1->GetItemInstanceData(ItemIdBackpack);
        URecursiveContainerInstanceData* RCI_B_B2 = Cast<URecursiveContainerInstanceData>(B2_InstanceDatas_In_SubInv_B_B1[0]);
        UItemContainerComponent* SubInv_B_B2 = RCI_B_B2->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P5] Picked up SubInv_B_B2 valid"), SubInv_B_B2 != nullptr);
        if(!SubInv_B_B2) return false;
        Res &= Test->TestTrue(TEXT("[Rec P5] Picked up SubInv_B_B2 owner is ActorB"), SubInv_B_B2->GetOwner() == ContextB.TempActor);
        int32 RocksInB2_B = SubInv_B_B2->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P5] Rocks in picked up Backpack2 (B) correct"), RocksInB2_B == 5);
        
        // --- Phase 6: Use/Destroy items within a recursive container ---
        // VMA: Empty. VMB: Grid[0]=Backpack1 [SubInv_B_B1: Grid[0]=Backpack2 [SubInv_B_B2: Grid[0]=Rock(5)]]
        int32 DestroyedFromSubInvB2 = SubInv_B_B2->DestroyItem_IfServer(ItemIdRock, 2, {}, EItemChangeReason::Consumed);
        Res &= Test->TestTrue(TEXT("[Rec P6] Destroyed 2 Rocks from SubInv_B_B2"), DestroyedFromSubInvB2 == 2);
        FItemBundle Backpack1_VMB_AfterDestroy = VMB->GetGridItem(0); // ViewModel should still show Backpack1
        bool Backpack1_StillInVMB = Backpack1_VMB_AfterDestroy.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P6] Backpack1 still in VMB Grid[0]"), Backpack1_StillInVMB);
        int32 RocksInSubInvB2_AfterDestroy = SubInv_B_B2->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P6] SubInv_B_B2 now contains 3 Rocks"), RocksInSubInvB2_AfterDestroy == 3);

        // --- Phase 8: Full inventory scenarios ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        InvA->MaxSlotCount = 1; // InvA generic can only hold 1 item type (stack)
        // VMA: Empty, MaxSlotCount=1
        InvA->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory); // Fills InvA generic
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Rock(5)
        
        int32 AddedBackpackToFullInvA = InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        Res &= Test->TestTrue(TEXT("[Rec P8] Added Backpack to VMA (should go to Tagged)"), AddedBackpackToFullInvA == 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Rock(5), RH=Backpack(1) (assuming RH is first available universal)
        FItemBundle Backpack_VMA_RH = VMA->GetItemForTaggedSlot(RightHandSlot);
        bool BackpackInVMA_RH = Backpack_VMA_RH.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P8] Backpack in VMA RightHandSlot"), BackpackInVMA_RH);

        bool MovedBackpackToLH_VMA = VMA->MoveItem(RightHandSlot, -1, LeftHandSlot, -1);
        Res &= Test->TestTrue(TEXT("[Rec P8] Moved Backpack from RH to LH in VMA"), MovedBackpackToLH_VMA);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Rock(5), LH=Backpack(1)
        FItemBundle Backpack_VMA_LH = VMA->GetItemForTaggedSlot(LeftHandSlot);
        bool BackpackInVMA_LH = Backpack_VMA_LH.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P8] Backpack in VMA LeftHandSlot after move"), BackpackInVMA_LH);
		
        // VMA: Grid[0]=Rock(5), LH=Backpack(1)
        Backpack_VMA_LH = VMA->GetItemForTaggedSlot(LeftHandSlot);
        BackpackInVMA_LH = Backpack_VMA_LH.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P8] Backpack in VMA LeftHandSlot after move"), BackpackInVMA_LH);

        // --- Phase 9: Dropping Items FROM within a Recursive Container (held in a Tagged Slot) ---
        // VMA: Grid[0]=Rock(5), LH=Backpack(1)[SubInv_A_B1: B2[SubInv_A_B2: Rock(5)]] (Conceptual state from P5, re-using logic)
        // Get the sub-container of the Backpack in VMA's LeftHandSlot
        URecursiveContainerInstanceData* RCI_A_LH_Backpack = nullptr;
        if (Backpack_VMA_LH.InstanceData.Num() > 0) {
            RCI_A_LH_Backpack = Cast<URecursiveContainerInstanceData>(Backpack_VMA_LH.InstanceData[0]);
        }
        Res &= Test->TestTrue(TEXT("[Rec P9] RCI for Backpack in VMA LH is valid"), RCI_A_LH_Backpack != nullptr);
        if (!RCI_A_LH_Backpack) return false;

        UItemContainerComponent* SubInv_A_LH_Backpack = RCI_A_LH_Backpack->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P9] Sub-container for Backpack in VMA LH is valid"), SubInv_A_LH_Backpack != nullptr);
        if (!SubInv_A_LH_Backpack) return false;
        
        // Let's assume Backpack1 still contains Backpack2 which contains Rock(5) from phase 5 logic re-application.
        // For clarity, we'll add a distinct item directly to SubInv_A_LH_Backpack first.
        SubInv_A_LH_Backpack->Clear_IfServer(); // Clear its contents first
        int32 AddedSticksToLHBackpack = SubInv_A_LH_Backpack->AddItem_IfServer(Subsystem, ItemIdSticks, 3, false);
        Res &= Test->TestTrue(TEXT("[Rec P9] Added 3 Sticks to Backpack in VMA LH"), AddedSticksToLHBackpack == 3);
        // VMA: Grid[0]=Rock(5), LH=Backpack(1)[SubInv_A_LH_Backpack: Sticks(3)]

        // Now, "drop" an item *from* SubInv_A_LH_Backpack.
        // This operation isn't directly exposed via ViewModel for sub-containers.
        // We test the underlying component logic and expect VMA to remain unchanged for the Backpack itself.
        int32 DroppedSticksFromSubInv = SubInv_A_LH_Backpack->DropItem(ItemIdSticks, 1, FItemBundle::NoInstances);
        Res &= Test->TestTrue(TEXT("[Rec P9] DropItem call on SubInv_A_LH_Backpack for 1 Stick returned 1"), DroppedSticksFromSubInv == 1);
        // The WorldItem for the Stick should be created. VMA's view of the Backpack in LH should not change.
        Res &= VMA->AssertViewModelSettled(); // Ensure VMA is settled if any indirect events occurred.
        // VMA: Grid[0]=Rock(5), LH=Backpack(1)[SubInv_A_LH_Backpack: Sticks(2)]
        
        Backpack_VMA_LH = VMA->GetItemForTaggedSlot(LeftHandSlot); // Re-fetch VMA's view
        BackpackInVMA_LH = Backpack_VMA_LH.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P9] Backpack still in VMA LeftHandSlot after dropping item from its sub-inventory"), BackpackInVMA_LH);
        int32 SticksInLHBackpack_AfterDrop = SubInv_A_LH_Backpack->GetQuantityTotal_Implementation(ItemIdSticks);
        Res &= Test->TestTrue(TEXT("[Rec P9] Backpack in VMA LH now contains 2 Sticks"), SticksInLHBackpack_AfterDrop == 2);

        AWorldItem* DroppedStickWorldItem = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdSticks) { DroppedStickWorldItem = *It; break; }
        }
        Res &= Test->TestTrue(TEXT("[Rec P9] Dropped Stick WorldItem found"), DroppedStickWorldItem != nullptr);
        if (DroppedStickWorldItem) {
            DroppedStickWorldItem->Destroy(); // Clean up this dropped item
        }

        // --- Phase 10: Dropping a Recursive Container that contains another Recursive Container (WorldItem check) ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferGenericInventory); // Backpack1 (B1)
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=B1(1)
        FItemBundle B1_VMA_Grid0_P10 = VMA->GetGridItem(0);
        URecursiveContainerInstanceData* RCI_A_B1_P10 = Cast<URecursiveContainerInstanceData>(B1_VMA_Grid0_P10.InstanceData[0]);
        UItemContainerComponent* SubInv_A_B1_P10 = RCI_A_B1_P10->RepresentedContainer;
        
        SubInv_A_B1_P10->AddItem_IfServer(Subsystem, ItemIdCoinPurse, 1, false); // CoinPurse (CP1) inside B1
        Res &= VMA->AssertViewModelSettled(); // VMA only sees B1, no direct update from sub-inv add.
        // VMA: Grid[0]=B1(1)[SubInv_A_B1_P10: CP1(1)]
        TArray<UItemInstanceData*> CP1_Datas_In_SubInv_A_B1_P10 = SubInv_A_B1_P10->GetItemInstanceData(ItemIdCoinPurse);
        URecursiveContainerInstanceData* RCI_A_CP1_P10 = Cast<URecursiveContainerInstanceData>(CP1_Datas_In_SubInv_A_B1_P10[0]);
        UItemContainerComponent* SubInv_A_CP1_P10 = RCI_A_CP1_P10->RepresentedContainer;

        SubInv_A_CP1_P10->AddItem_IfServer(Subsystem, ItemIdRock, 2, false); // Rock(2) inside CP1
        // VMA: Grid[0]=B1(1)[SubInv_A_B1_P10: CP1(1)[SubInv_A_CP1_P10: Rock(2)]]

        UItemInstanceData* B1_InstancePtr_P10 = RCI_A_B1_P10;
        int32 DroppedB1_P10 = VMA->DropItem(NoTag, 0, 1); // Drop B1 (which contains CP1, which contains Rocks)
        Res &= Test->TestTrue(TEXT("[Rec P10] DropItem for B1 (P10) returned 1"), DroppedB1_P10 == 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        
        AWorldItem* WorldB1_P10 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdBackpack && It->RepresentedItem.InstanceData[0] == B1_InstancePtr_P10) {
                WorldB1_P10 = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P10] WorldItem for B1 (P10) found"), WorldB1_P10 != nullptr);
        if (!WorldB1_P10) return false;
        
        FItemBundle B1_Bundle_World_P10 = WorldB1_P10->RepresentedItem;
        URecursiveContainerInstanceData* RCI_W_B1_P10 = Cast<URecursiveContainerInstanceData>(B1_Bundle_World_P10.InstanceData[0]);
        UItemContainerComponent* SubInv_W_B1_P10 = RCI_W_B1_P10->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P10] SubInv_W_B1_P10 valid"), SubInv_W_B1_P10 != nullptr);
        if (!SubInv_W_B1_P10) return false;
        AActor* Owner_SubInv_W_B1_P10 = SubInv_W_B1_P10->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P10] SubInv_W_B1_P10 owner is WorldB1_P10"), Owner_SubInv_W_B1_P10 == WorldB1_P10);

        TArray<UItemInstanceData*> CP1_Datas_In_SubInv_W_B1_P10 = SubInv_W_B1_P10->GetItemInstanceData(ItemIdCoinPurse);
        int32 CP1_Qty_In_SubInv_W_B1_P10 = SubInv_W_B1_P10->GetQuantityTotal_Implementation(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Rec P10] SubInv_W_B1_P10 contains CP1"), CP1_Qty_In_SubInv_W_B1_P10 == 1);
        URecursiveContainerInstanceData* RCI_W_CP1_P10 = Cast<URecursiveContainerInstanceData>(CP1_Datas_In_SubInv_W_B1_P10[0]);
        UItemContainerComponent* SubInv_W_CP1_P10 = RCI_W_CP1_P10->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P10] SubInv_W_CP1_P10 valid"), SubInv_W_CP1_P10 != nullptr);
        if(!SubInv_W_CP1_P10) return false;
        AActor* Owner_SubInv_W_CP1_P10 = SubInv_W_CP1_P10->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P10] SubInv_W_CP1_P10 owner is WorldB1_P10"), Owner_SubInv_W_CP1_P10 == WorldB1_P10);
        
        int32 RocksInCP1_W_P10 = SubInv_W_CP1_P10->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P10] Rocks in CP1 (World) correct"), RocksInCP1_W_P10 == 2);

        if (WorldB1_P10) WorldB1_P10->Destroy();


        // --- Phase 11: Pickup a Recursive Container that had items dropped from its sub-inventory ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        InvB->Clear_IfServer(); Res &= VMB->AssertViewModelSettled();
        // VMA: Empty, VMB: Empty
        
        InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferGenericInventory); // B1 in InvA
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=B1(1)
        FItemBundle B1_VMA_Grid0_P11 = VMA->GetGridItem(0);
        URecursiveContainerInstanceData* RCI_A_B1_P11 = Cast<URecursiveContainerInstanceData>(B1_VMA_Grid0_P11.InstanceData[0]);
        UItemContainerComponent* SubInv_A_B1_P11 = RCI_A_B1_P11->RepresentedContainer;

        SubInv_A_B1_P11->AddItem_IfServer(Subsystem, ItemIdRock, 3, false); // 3 Rocks in B1
        SubInv_A_B1_P11->AddItem_IfServer(Subsystem, ItemIdSticks, 2, false); // 2 Sticks in B1
        // VMA: Grid[0]=B1(1) [SubInv_A_B1_P11: Rock(3), Sticks(2)]

        int32 DroppedRocksFromSubB1_P11 = SubInv_A_B1_P11->DropItem(ItemIdRock, 1, FItemBundle::NoInstances); // Drop 1 Rock from B1
        Res &= Test->TestTrue(TEXT("[Rec P11] Dropped 1 Rock from B1's sub-inventory"), DroppedRocksFromSubB1_P11 == 1);
        // VMA: Grid[0]=B1(1) [SubInv_A_B1_P11: Rock(2), Sticks(2)] (Conceptual, VMA itself isn't directly updated by sub-inv drop)
        Res &= VMA->AssertViewModelSettled(); 
        int32 RocksInSubB1_P11_AfterDrop = SubInv_A_B1_P11->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P11] B1's sub-inventory now has 2 Rocks"), RocksInSubB1_P11_AfterDrop == 2);

        AWorldItem* DroppedRockWorldItem_P11 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdRock) { DroppedRockWorldItem_P11 = *It; break; }
        }
        Res &= Test->TestTrue(TEXT("[Rec P11] Dropped Rock WorldItem (P11) found"), DroppedRockWorldItem_P11 != nullptr);

        // Now drop the Backpack (B1) itself
        UItemInstanceData* B1_InstancePtr_P11 = RCI_A_B1_P11;
        VMA->DropItem(NoTag, 0, 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        AWorldItem* DroppedB1_WorldItem_P11 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdBackpack && It->RepresentedItem.InstanceData[0] == B1_InstancePtr_P11) {
                DroppedB1_WorldItem_P11 = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P11] Dropped B1 WorldItem (P11) found"), DroppedB1_WorldItem_P11 != nullptr);
        if (!DroppedB1_WorldItem_P11) return false;

        // Pickup B1 into InvB
        VMB->PickupItem(DroppedB1_WorldItem_P11, EPreferredSlotPolicy::PreferGenericInventory, true);
        Res &= VMB->AssertViewModelSettled();
        // VMB: Grid[0]=B1(1) [SubInv_B_B1_P11: Rock(2), Sticks(2)]
        FItemBundle B1_VMB_Grid0_P11 = VMB->GetGridItem(0);
        bool B1_In_VMB_Grid0_P11 = B1_VMB_Grid0_P11.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P11] Picked up B1 into VMB Grid[0]"), B1_In_VMB_Grid0_P11);
        URecursiveContainerInstanceData* RCI_B_B1_P11 = Cast<URecursiveContainerInstanceData>(B1_VMB_Grid0_P11.InstanceData[0]);
        UItemContainerComponent* SubInv_B_B1_P11 = RCI_B_B1_P11->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P11] SubInv_B_B1_P11 valid after pickup"), SubInv_B_B1_P11 != nullptr);
        if (!SubInv_B_B1_P11) return false;

        int32 RocksInPickedUpB1 = SubInv_B_B1_P11->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P11] Picked up B1 contains 2 Rocks"), RocksInPickedUpB1 == 2);
        int32 SticksInPickedUpB1 = SubInv_B_B1_P11->GetQuantityTotal_Implementation(ItemIdSticks);
        Res &= Test->TestTrue(TEXT("[Rec P11] Picked up B1 contains 2 Sticks"), SticksInPickedUpB1 == 2);
        
        if(DroppedRockWorldItem_P11) DroppedRockWorldItem_P11->Destroy(); // Clean up the individual rock

        // --- Phase 12: Dropping from a Tagged Recursive Container, check WorldItem's sub-container owner ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        // VMA: Empty
        InvA->AddItemToAnySlot(Subsystem, ItemIdBackpack, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        Res &= VMA->AssertViewModelSettled();
        // VMA: RH=Backpack(1) (assuming RH is available)
        FItemBundle Backpack_VMA_RH_P12 = VMA->GetItemForTaggedSlot(RightHandSlot);
        bool BackpackInVMA_RH_P12 = Backpack_VMA_RH_P12.ItemId == ItemIdBackpack;
        Res &= Test->TestTrue(TEXT("[Rec P12] Backpack in VMA RH"), BackpackInVMA_RH_P12);
        URecursiveContainerInstanceData* RCI_A_RH_Backpack_P12 = Cast<URecursiveContainerInstanceData>(Backpack_VMA_RH_P12.InstanceData[0]);
        UItemContainerComponent* SubInv_A_RH_Backpack_P12 = RCI_A_RH_Backpack_P12->RepresentedContainer;

        SubInv_A_RH_Backpack_P12->AddItem_IfServer(Subsystem, ItemIdRock, 1, false);
        // VMA: RH=Backpack(1)[SubInv_A_RH_Backpack_P12: Rock(1)]
        
        UItemInstanceData* B_RH_InstancePtr_P12 = RCI_A_RH_Backpack_P12;
        VMA->DropItem(RightHandSlot, -1, 1); // Drop Backpack from RH
        Res &= VMA->AssertViewModelSettled();
        // VMA: RH is Empty
        bool VMA_RH_Empty_P12 = VMA->IsTaggedSlotEmpty(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Rec P12] VMA RH is empty after drop"), VMA_RH_Empty_P12);

        AWorldItem* DroppedB_RH_WorldItem_P12 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdBackpack && It->RepresentedItem.InstanceData[0] == B_RH_InstancePtr_P12) {
                DroppedB_RH_WorldItem_P12 = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P12] WorldItem for Backpack from RH found"), DroppedB_RH_WorldItem_P12 != nullptr);
        if(!DroppedB_RH_WorldItem_P12) return false;

        FItemBundle B_RH_Bundle_World_P12 = DroppedB_RH_WorldItem_P12->RepresentedItem;
        URecursiveContainerInstanceData* RCI_W_B_RH_P12 = Cast<URecursiveContainerInstanceData>(B_RH_Bundle_World_P12.InstanceData[0]);
        UItemContainerComponent* SubInv_W_B_RH_P12 = RCI_W_B_RH_P12->RepresentedContainer;
        Res &= Test->TestTrue(TEXT("[Rec P12] SubInv for WorldItem (from RH) valid"), SubInv_W_B_RH_P12 != nullptr);
        if(!SubInv_W_B_RH_P12) return false;
        AActor* Owner_SubInv_W_B_RH_P12 = SubInv_W_B_RH_P12->GetOwner();
        Res &= Test->TestTrue(TEXT("[Rec P12] SubInv for WorldItem (from RH) owner is WorldItem"), Owner_SubInv_W_B_RH_P12 == DroppedB_RH_WorldItem_P12);
        int32 RocksInSub_World_P12 = SubInv_W_B_RH_P12->GetQuantityTotal_Implementation(ItemIdRock);
        Res &= Test->TestTrue(TEXT("[Rec P12] Rocks in SubInv of WorldItem (from RH) correct"), RocksInSub_World_P12 == 1);
        Res &= VMA->AssertViewModelSettled(); // Final ensure after all ops

		// --- Phase 13: Complex Interactions with Pouch, Drops, and Swaps ---
        InvA->Clear_IfServer(); Res &= VMA->AssertViewModelSettled();
        InvB->Clear_IfServer(); Res &= VMB->AssertViewModelSettled();
        // VMA: Empty, VMB: Empty
        InvA->MaxSlotCount = 5; // Set specific slot count for InvA

        // Add initial items to InvA (generic slots)
        InvA->AddItemToAnySlot(Subsystem, ItemIdSticks, 3, EPreferredSlotPolicy::PreferGenericInventory);
        InvA->AddItemToAnySlot(Subsystem, ItemIdRock, 2, EPreferredSlotPolicy::PreferGenericInventory);
        InvA->AddItemToAnySlot(Subsystem, ItemIdBrittleEgg, 2, EPreferredSlotPolicy::PreferGenericInventory); // 2 Eggs
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Sticks(3), Grid[1]=Rock(2), Grid[2]=Egg(2)

        FItemBundle Sticks_VMA_Grid0 = VMA->GetGridItem(0);
        bool Sticks_VMA_Grid0_Correct = Sticks_VMA_Grid0.ItemId == ItemIdSticks && Sticks_VMA_Grid0.Quantity == 3;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[0] has 3 Sticks"), Sticks_VMA_Grid0_Correct);
        FItemBundle Rocks_VMA_Grid1 = VMA->GetGridItem(1);
        bool Rocks_VMA_Grid1_Correct = Rocks_VMA_Grid1.ItemId == ItemIdRock && Rocks_VMA_Grid1.Quantity == 2;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[1] has 2 Rocks"), Rocks_VMA_Grid1_Correct);
        FItemBundle Eggs_VMA_Grid2 = VMA->GetGridItem(2);
        bool Eggs_VMA_Grid2_Correct = Eggs_VMA_Grid2.ItemId == ItemIdBrittleEgg && Eggs_VMA_Grid2.Quantity == 2;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] has 2 Eggs"), Eggs_VMA_Grid2_Correct);
        TArray<UItemInstanceData*> EggInstances_VMA_Original = Eggs_VMA_Grid2.InstanceData;
        int32 EggInstances_VMA_Original_Count = EggInstances_VMA_Original.Num();
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] Eggs have 2 instances"), EggInstances_VMA_Original_Count == 2);
        UItemInstanceData* EggInstance1_VMA = EggInstances_VMA_Original.IsValidIndex(0) ? EggInstances_VMA_Original[0] : nullptr;
        UItemInstanceData* EggInstance2_VMA = EggInstances_VMA_Original.IsValidIndex(1) ? EggInstances_VMA_Original[1] : nullptr;

        // Create a Coin Purse and add it to InvA (generic slot)
        InvA->AddItemToAnySlot(Subsystem, ItemIdCoinPurse, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Sticks(3), Grid[1]=Rock(2), Grid[2]=Egg(2), Grid[3]=Purse(1)
        FItemBundle Purse_VMA_Grid3 = VMA->GetGridItem(3);
        bool PurseInGrid3_VMA = Purse_VMA_Grid3.ItemId == ItemIdCoinPurse;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[3] has CoinPurse"), PurseInGrid3_VMA);
        URecursiveContainerInstanceData* RCI_A_Purse_P13 = nullptr;
        if(Purse_VMA_Grid3.InstanceData.IsValidIndex(0)) RCI_A_Purse_P13 = Cast<URecursiveContainerInstanceData>(Purse_VMA_Grid3.InstanceData[0]);
        UItemContainerComponent* SubInv_A_Purse_P13 = RCI_A_Purse_P13 ? RCI_A_Purse_P13->RepresentedContainer : nullptr;
        Res &= Test->TestTrue(TEXT("[Rec P13] SubInv_A_Purse_P13 valid"), SubInv_A_Purse_P13 != nullptr);
        if (!SubInv_A_Purse_P13) return false;

        // Create a ViewModel for the Coin Purse's sub-inventory
        auto* PurseViewModel_A = NewObject<UInventoryGridViewModel>();
        PurseViewModel_A->Initialize(SubInv_A_Purse_P13);
        Res &= PurseViewModel_A->AssertViewModelSettled();

        // Move 1 Egg from VMA Grid[2] (Instance2) to the Coin Purse (PurseViewModel_A Grid[0])
        bool MovedEggToPurseVM = VMA->MoveItemToOtherViewModel(NoTag, 2, PurseViewModel_A, NoTag, 0, 1); // Move 1 egg
        Res &= Test->TestTrue(TEXT("[Rec P13] MoveItemToOtherViewModel for Egg (VMA to PurseVM_A) initiated"), MovedEggToPurseVM);
        Res &= VMA->AssertViewModelSettled();
        Res &= PurseViewModel_A->AssertViewModelSettled();
        // VMA: Grid[0]=Sticks(3), Grid[1]=Rock(2), Grid[2]=Egg(1){Instance1}, Grid[3]=Purse(1)[SubInv_A_Purse_P13: Egg(1){Instance2}]
        
        Eggs_VMA_Grid2 = VMA->GetGridItem(2);
        bool Eggs_VMA_Grid2_AfterMove_Correct = Eggs_VMA_Grid2.ItemId == ItemIdBrittleEgg && Eggs_VMA_Grid2.Quantity == 1;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] now has 1 Egg"), Eggs_VMA_Grid2_AfterMove_Correct);
        int32 Eggs_VMA_Grid2_Instances_AfterMove = Eggs_VMA_Grid2.InstanceData.Num();
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] Egg has 1 instance"), Eggs_VMA_Grid2_Instances_AfterMove == 1);
        if(Eggs_VMA_Grid2.InstanceData.Num() == 1) {
             bool EggInstance1_StillInVMA = Eggs_VMA_Grid2.InstanceData[0] == EggInstance1_VMA;
             Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] Egg instance is Instance1"), EggInstance1_StillInVMA);
        }
        
        FItemBundle EggInPurse_P13_VM = PurseViewModel_A->GetGridItem(0);
        bool EggInPurse_P13_VM_Correct = EggInPurse_P13_VM.ItemId == ItemIdBrittleEgg && EggInPurse_P13_VM.Quantity == 1;
        Res &= Test->TestTrue(TEXT("[Rec P13] PurseViewModel_A Grid[0] contains 1 Egg"), EggInPurse_P13_VM_Correct);
        int32 EggInstancesInPurse_VM = EggInPurse_P13_VM.InstanceData.Num();
        Res &= Test->TestTrue(TEXT("[Rec P13] Egg in purse (VM) has 1 instance"), EggInstancesInPurse_VM == 1);
        if(EggInPurse_P13_VM.InstanceData.Num() == 1) {
            bool EggInstance2_InPurseVM = EggInPurse_P13_VM.InstanceData[0] == EggInstance2_VMA;
            Res &= Test->TestTrue(TEXT("[Rec P13] Egg in purse (VM) instance is Instance2"), EggInstance2_InPurseVM);
        }

        // Drop the Coin Purse (from VMA Grid[3])
        UItemInstanceData* PurseInstancePtr_P13 = RCI_A_Purse_P13;
        int32 DroppedPurse_P13 = VMA->DropItem(NoTag, 3, 1);
        Res &= Test->TestTrue(TEXT("[Rec P13] Dropped CoinPurse from VMA Grid[3]"), DroppedPurse_P13 == 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Sticks(3), Grid[1]=Rock(2), Grid[2]=Egg(1){Instance1}, Grid[3]=Empty
        bool PurseNoLongerInGrid3_VMA = VMA->IsGridSlotEmpty(3);
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[3] is empty after dropping Purse"), PurseNoLongerInGrid3_VMA);
        bool PurseUnregisteredFromA_P13 = !ContextA.TempActor->IsReplicatedSubObjectRegistered(PurseInstancePtr_P13);
        Res &= Test->TestTrue(TEXT("[Rec P13] Purse instance unregistered from ActorA"), PurseUnregisteredFromA_P13);
        
        AWorldItem* DroppedPurseWorldItem_P13 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdCoinPurse && It->RepresentedItem.InstanceData.IsValidIndex(0) && It->RepresentedItem.InstanceData[0] == PurseInstancePtr_P13) {
                DroppedPurseWorldItem_P13 = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P13] Dropped CoinPurse WorldItem found"), DroppedPurseWorldItem_P13 != nullptr);
        if (!DroppedPurseWorldItem_P13) return false;

        // Pickup the Coin Purse into InvB
        VMB->PickupItem(DroppedPurseWorldItem_P13, EPreferredSlotPolicy::PreferGenericInventory, true);
        Res &= VMB->AssertViewModelSettled();
        // VMB: Grid[0]=Purse(1)[SubInv_B_Purse_P13: Egg(1){Instance2}]
        FItemBundle Purse_VMB_Grid0 = VMB->GetGridItem(0);
        bool PurseInGrid0_VMB = Purse_VMB_Grid0.ItemId == ItemIdCoinPurse;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMB Grid[0] has CoinPurse after pickup"), PurseInGrid0_VMB);
        URecursiveContainerInstanceData* RCI_B_Purse_P13 = nullptr;
        if(Purse_VMB_Grid0.InstanceData.IsValidIndex(0)) RCI_B_Purse_P13 = Cast<URecursiveContainerInstanceData>(Purse_VMB_Grid0.InstanceData[0]);
        bool RCI_B_Purse_P13_IsSame = RCI_B_Purse_P13 == PurseInstancePtr_P13;
        Res &= Test->TestTrue(TEXT("[Rec P13] Picked up Purse instance is same object"), RCI_B_Purse_P13_IsSame);
        UItemContainerComponent* SubInv_B_Purse_P13 = RCI_B_Purse_P13 ? RCI_B_Purse_P13->RepresentedContainer : nullptr;
        Res &= Test->TestTrue(TEXT("[Rec P13] SubInv_B_Purse_P13 is valid"), SubInv_B_Purse_P13 != nullptr);
        if(!SubInv_B_Purse_P13) return false;

        int32 EggsInPickedUpPurse = SubInv_B_Purse_P13->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P13] Picked up Purse contains 1 Egg"), EggsInPickedUpPurse == 1);
        TArray<UItemInstanceData*> EggInstancesInPickedUpPurse = SubInv_B_Purse_P13->GetItemInstanceData(ItemIdBrittleEgg);
        bool EggInstance2_InPickedUpPurse = EggInstancesInPickedUpPurse.Num() == 1 && EggInstancesInPickedUpPurse[0] == EggInstance2_VMA;
        Res &= Test->TestTrue(TEXT("[Rec P13] Egg in picked up purse is Instance2"), EggInstance2_InPickedUpPurse);

        // Move Purse from VMB Grid[0] to VMA Grid[0] (which currently has Sticks) - Expects a swap
        // VMB: Grid[0]=Purse(1)[Egg(1){I2}] || VMA: Grid[0]=Sticks(3), Grid[1]=Rock(2), Grid[2]=Egg(1){I1}

		// TODO: Swapback not currently supported, multistep workaround for now:

		VMB->MoveItemToOtherViewModel(NoTag, 0, VMA, NoTag, 5);
		VMA->MoveItemToOtherViewModel(NoTag, 0, VMB, NoTag, 0);
		VMA->MoveItem(NoTag, 5, NoTag, 0);
		
        //bool MovedPurseToVMA_Swap = VMB->MoveItemToOtherViewModel(NoTag, 0, VMA, NoTag, 0);
        //Res &= Test->TestTrue(TEXT("[Rec P13] Moved Purse from VMB to VMA (Swap with Sticks)"), MovedPurseToVMA_Swap);
        //Res &= VMA->AssertViewModelSettled();
        //Res &= VMB->AssertViewModelSettled();
        //// VMB: Grid[0]=Sticks(3) || VMA: Grid[0]=Purse(1)[Egg(1){I2}], Grid[1]=Rock(2), Grid[2]=Egg(1){I1}
        //FItemBundle Sticks_VMB_Grid0_AfterSwap = VMB->GetGridItem(0);
        //bool Sticks_VMB_Grid0_AfterSwap_Correct = Sticks_VMB_Grid0_AfterSwap.ItemId == ItemIdSticks && Sticks_VMB_Grid0_AfterSwap.Quantity == 3;
        //Res &= Test->TestTrue(TEXT("[Rec P13] VMB Grid[0] now has Sticks"), Sticks_VMB_Grid0_AfterSwap_Correct);
        
        Purse_VMA_Grid3 = VMA->GetGridItem(0); // Purse should be in Grid[0] of VMA now
        bool Purse_VMA_Grid0_AfterSwap_Correct = Purse_VMA_Grid3.ItemId == ItemIdCoinPurse;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[0] now has CoinPurse"), Purse_VMA_Grid0_AfterSwap_Correct);
        URecursiveContainerInstanceData* RCI_A_Purse_P13_AfterSwap = nullptr;
        if(Purse_VMA_Grid3.InstanceData.IsValidIndex(0)) RCI_A_Purse_P13_AfterSwap = Cast<URecursiveContainerInstanceData>(Purse_VMA_Grid3.InstanceData[0]);
        UItemContainerComponent* SubInv_A_Purse_P13_AfterSwap = RCI_A_Purse_P13_AfterSwap ? RCI_A_Purse_P13_AfterSwap->RepresentedContainer : nullptr;
        Res &= Test->TestTrue(TEXT("[Rec P13] SubInv_A_Purse_P13_AfterSwap is valid"), SubInv_A_Purse_P13_AfterSwap != nullptr);
        if(!SubInv_A_Purse_P13_AfterSwap) return false;
        int32 EggsInPurse_A_AfterSwap = SubInv_A_Purse_P13_AfterSwap->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P13] Purse in VMA still contains 1 Egg"), EggsInPurse_A_AfterSwap == 1);

        // Drop Rocks and Sticks from VMA (Grid[1] and VMB Grid[0] respectively)
        VMA->DropItem(NoTag, 1, 2); // Drop 2 Rocks from VMA Grid[1]
        Res &= VMA->AssertViewModelSettled();
        VMB->DropItem(NoTag, 0, 3); // Drop 3 Sticks from VMB Grid[0]
        Res &= VMB->AssertViewModelSettled();
        // VMA: Grid[0]=Purse(1)[Egg(1){I2}], Grid[1]=Empty, Grid[2]=Egg(1){I1} || VMB: Empty
        bool RocksDroppedFromVMA = VMA->IsGridSlotEmpty(1);
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[1] (Rocks) is empty after drop"), RocksDroppedFromVMA);
        bool SticksDroppedFromVMB = VMB->IsGridSlotEmpty(0);
        Res &= Test->TestTrue(TEXT("[Rec P13] VMB Grid[0] (Sticks) is empty after drop"), SticksDroppedFromVMB);

        // Drop the Coin Purse from VMA Grid[0] again
        UItemInstanceData* PurseInstancePtr_P13_Again = RCI_A_Purse_P13_AfterSwap;
        VMA->DropItem(NoTag, 0, 1);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Empty, Grid[1]=Empty, Grid[2]=Egg(1){I1}
        AWorldItem* DroppedPurseWorldItem_P13_Again = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdCoinPurse && It->RepresentedItem.InstanceData.IsValidIndex(0) && It->RepresentedItem.InstanceData[0] == PurseInstancePtr_P13_Again) {
                DroppedPurseWorldItem_P13_Again = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P13] Dropped CoinPurse WorldItem (again) found"), DroppedPurseWorldItem_P13_Again != nullptr);
        if(!DroppedPurseWorldItem_P13_Again) return false;

        // Pickup Rocks (World) then Coin Purse (World) into VMA
        AWorldItem* DroppedRocksWorldItem_P13 = nullptr;
        for (TActorIterator<AWorldItem> It(ContextA.World); It; ++It) {
            if (It->RepresentedItem.ItemId == ItemIdRock) {
                DroppedRocksWorldItem_P13 = *It; break;
            }
        }
        Res &= Test->TestTrue(TEXT("[Rec P13] Dropped Rocks WorldItem found"), DroppedRocksWorldItem_P13 != nullptr);
        if(DroppedRocksWorldItem_P13) {
            VMA->PickupItem(DroppedRocksWorldItem_P13, EPreferredSlotPolicy::PreferGenericInventory, true);
            Res &= VMA->AssertViewModelSettled();
        }
        // VMA: Grid[0]=Rock(2), Grid[1]=Empty, Grid[2]=Egg(1){I1}
        Rocks_VMA_Grid1 = VMA->GetGridItem(0); // Should go to first empty slot, Grid[0]
        bool Rocks_VMA_Grid0_AfterPickup_Correct = Rocks_VMA_Grid1.ItemId == ItemIdRock && Rocks_VMA_Grid1.Quantity == 2;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[0] has Rocks after pickup"), Rocks_VMA_Grid0_AfterPickup_Correct);

        VMA->PickupItem(DroppedPurseWorldItem_P13_Again, EPreferredSlotPolicy::PreferGenericInventory, true);
        Res &= VMA->AssertViewModelSettled();
        // VMA: Grid[0]=Rock(2), Grid[1]=Purse(1)[Egg(1){I2}], Grid[2]=Egg(1){I1}
        Purse_VMA_Grid3 = VMA->GetGridItem(1); // Should go to next empty slot, Grid[1]
        bool Purse_VMA_Grid1_AfterPickup_Correct = Purse_VMA_Grid3.ItemId == ItemIdCoinPurse;
        Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[1] has CoinPurse after pickup (again)"), Purse_VMA_Grid1_AfterPickup_Correct);
        URecursiveContainerInstanceData* RCI_A_Purse_P13_PickedUp = nullptr;
        if(Purse_VMA_Grid3.InstanceData.IsValidIndex(0)) RCI_A_Purse_P13_PickedUp = Cast<URecursiveContainerInstanceData>(Purse_VMA_Grid3.InstanceData[0]);
        UItemContainerComponent* SubInv_A_Purse_P13_PickedUp = RCI_A_Purse_P13_PickedUp ? RCI_A_Purse_P13_PickedUp->RepresentedContainer : nullptr;
        Res &= Test->TestTrue(TEXT("[Rec P13] SubInv_A_Purse_P13_PickedUp is valid"), SubInv_A_Purse_P13_PickedUp != nullptr);
        if(!SubInv_A_Purse_P13_PickedUp) return false;

        int32 EggsInPurse_A_PickedUp = SubInv_A_Purse_P13_PickedUp->GetQuantityTotal_Implementation(ItemIdBrittleEgg);
        Res &= Test->TestTrue(TEXT("[Rec P13] Picked up Purse in VMA contains 1 Egg"), EggsInPurse_A_PickedUp == 1);

        // Swap Egg (VMA Grid[2]{Instance1}) with Rocks (VMA Grid[0])
        // VMA: Grid[0]=Rock(2), Grid[1]=Purse(1)[Egg(1){I2}], Grid[2]=Egg(1){I1}


		// TODO: Swapback not currently supported, multistep workaround for now:
		VMA->MoveItemToOtherViewModel(NoTag, 2, VMB, NoTag, 5);
		VMB->MoveItemToOtherViewModel(NoTag, 0, VMA, NoTag, 2);
		VMB->MoveItem(NoTag, 5, NoTag, 0);

		// TODO: Swap
        //bool SwappedEggAndRock = VMA->MoveItem(NoTag, 2, NoTag, 0);
        //Res &= Test->TestTrue(TEXT("[Rec P13] Swapped Egg (Grid[2]) with Rocks (Grid[0]) in VMA"), SwappedEggAndRock);
        //Res &= VMA->AssertViewModelSettled();
        //// VMA: Grid[0]=Egg(1){I1}, Grid[1]=Purse(1)[Egg(1){I2}], Grid[2]=Rock(2)
        //Eggs_VMA_Grid2 = VMA->GetGridItem(0); // Egg is now in Grid[0]
        //bool Eggs_VMA_Grid0_AfterSwap_Correct = Eggs_VMA_Grid2.ItemId == ItemIdBrittleEgg && Eggs_VMA_Grid2.Quantity == 1;
        //Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[0] has Egg after swap"), Eggs_VMA_Grid0_AfterSwap_Correct);
        //bool EggInstance1_InGrid0_VMA = Eggs_VMA_Grid2.InstanceData.Num()==1 && Eggs_VMA_Grid2.InstanceData[0] == EggInstance1_VMA;
        //Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[0] Egg instance is Instance1"), EggInstance1_InGrid0_VMA);
        //Rocks_VMA_Grid1 = VMA->GetGridItem(2); // Rocks are now in Grid[2]
        //bool Rocks_VMA_Grid2_AfterSwap_Correct = Rocks_VMA_Grid1.ItemId == ItemIdRock && Rocks_VMA_Grid1.Quantity == 2;
        //Res &= Test->TestTrue(TEXT("[Rec P13] VMA Grid[2] has Rocks after swap"), Rocks_VMA_Grid2_AfterSwap_Correct);
		
        
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
    Res &= TestScenarios.TestUseInstanceDataItems();
	Res &= TestScenarios.TestMoveItemToOtherViewModel();
	Res &= TestScenarios.TestRecursiveContainers();

	/* Things to test:
	 * Container filled with 1/5 rocks -> add sticks
	 * Have 3 brittle knives, do operations on the middle one to check specified instances work
	 * MoveItemtoothervm with weight limit
	 */

	return Res;
}

#endif // #if WITH_DEV_AUTOMATION_TESTS