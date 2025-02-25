// Copyright Rancorous Games, 2024

#include "GearManagerComponent.h"
#include "NativeGameplayTags.h"
#include "Components/InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp" // Include for test item and tag definitions
#include "WeaponActor.h"
#include "Framework/DebugTestResult.h"
#include "GameFramework/Character.h"

#define TestName "GameTests.RIS.RancGearManager"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancGearManagerComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class GearManagerComponentTestContext
{
public:
	GearManagerComponentTestContext(float CarryCapacity, int32 NumSlots)
		: TestFixture(FName(*FString(TestName)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		World = TestFixture.GetWorld();
		TempActor = World->SpawnActor<ACharacter>();
		InventoryComponent = NewObject<UInventoryComponent>(TempActor);
		TempActor->AddInstanceComponent(InventoryComponent);
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(LeftHandSlot));
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, ItemTypeTwoHanded));
		InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);
		InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);
		InventoryComponent->MaxContainerSlotCount = NumSlots;
		InventoryComponent->MaxWeight = CarryCapacity;
		InventoryComponent->RegisterComponent();

		GearManager = NewObject<UGearManagerComponent>(TempActor);
		TempActor->AddInstanceComponent(GearManager);
		GearManager->RegisterComponent();

		FGearSlotDefinition MainHandSlotDef;
		MainHandSlotDef.SlotTag = RightHandSlot;
		MainHandSlotDef.AttachSocketName = FName("MainHandSocket");
		MainHandSlotDef.SlotType = EGearSlotType::MainHand;
		MainHandSlotDef.bVisibleOnCharacter = true;
		GearManager->GearSlots.Add(MainHandSlotDef);

		FGearSlotDefinition OffHandSlotDef;
		OffHandSlotDef.SlotTag = LeftHandSlot;
		OffHandSlotDef.AttachSocketName = FName("OffHandSocket");
		OffHandSlotDef.SlotType = EGearSlotType::OffHand;
		OffHandSlotDef.bVisibleOnCharacter = true;
		GearManager->GearSlots.Add(OffHandSlotDef);

		GearManager->Initialize();
		TestFixture.InitializeTestItems(); // Initialize test items
	}

	~GearManagerComponentTestContext()
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
	UGearManagerComponent* GearManager;
};


class GearManagerTestScenarios
{
public:
	FRancGearManagerComponentTest* Test;

	GearManagerTestScenarios(FRancGearManagerComponentTest* InTest)
		: Test(InTest)
	{
	};

	bool TestEquippingWeapon()
	{
		GearManagerComponentTestContext Context(100, 9); // No blocking for this test
		auto* GearManager = Context.GearManager;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be null initially"), GearManager->MainhandSlotWeapon == nullptr);
		Res &= Test->TestTrue(
			TEXT("OffhandSlotWeapon should be null initially"), GearManager->OffhandSlotWeapon == nullptr);

		GearManager->AddAndSelectWeapon(SpearData, FGameplayTag());

		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be valid after equipping the spear"),
			GearManager->MainhandSlotWeapon != nullptr);

		return Res;
	}

	bool TestBlockedSlotBehavior()
	{
		GearManagerComponentTestContext Context(100, 9);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* GearManager = Context.GearManager;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, SpearData->ItemId, 1, false);

		int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, false);
		Res &= Test->TestEqual(TEXT("Adding an item to a blocked slot should add 0 items"), AmountAdded, 0);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Removed, true);

		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, false);
		Res &= Test->TestNotEqual(
			TEXT("After unblocking, adding an item to the slot should succeed (non-zero amount)"), AmountAdded, 0);

		return Res;
	}

	bool TestUnequippingWeapon()
	{
		GearManagerComponentTestContext Context(100, 9);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* GearManager = Context.GearManager;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, SpearData->ItemId, 1, false);

		Res &= Test->TestTrue(
			TEXT("Weapon should be equipped before unequipping"), GearManager->MainhandSlotWeapon != nullptr);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Removed, true);

		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be null after dropping the weapon"),
			GearManager->MainhandSlotWeapon == nullptr);

		return Res;
	}

	bool TestInvalidWeaponSelection()
	{
		GearManagerComponentTestContext Context(100, 9);
		auto* GearManager = Context.GearManager;
		FDebugTestResult Res = true;

		const int32 InvalidIndex = 999;
		GearManager->SelectActiveWeapon(InvalidIndex, false, nullptr);

		Res &= Test->TestTrue(
			TEXT("ActiveWeaponIndex should remain 0 after an invalid selection"), GearManager->ActiveWeaponIndex == 0);
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should still be null after an invalid selection"),
			GearManager->MainhandSlotWeapon == nullptr);

		return Res;
	}

	bool TestWeaponSelectionDeselectSequences()
	{
	    // Setup a test context with enough capacity and slots.
	    GearManagerComponentTestContext Context(100, 9);
	    auto* InventoryComponent = Context.InventoryComponent;
	    auto* GearManager = Context.GearManager;
	    auto* Subsystem = Context.TestFixture.GetSubsystem();
	    FDebugTestResult Res = true;

	    // ***********************************************
	    // Step 0: Verify initial state: both weapon slots are empty.
	    // ***********************************************
	    Res &= Test->TestTrue(TEXT("Step 0: MainhandSlotWeapon should be null initially"), GearManager->MainhandSlotWeapon == nullptr);
	    Res &= Test->TestTrue(TEXT("Step 0: OffhandSlotWeapon should be null initially"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 1: Equip Spear (a two-handed weapon) into mainhand (RightHandSlot).
	    // ***********************************************
	    UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
	    Res &= Test->TestTrue(TEXT("Step 1: Spear item data should be valid"), SpearData != nullptr);
	    int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, SpearData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 1: Adding Spear to RightHandSlot should succeed"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 1: MainhandSlotWeapon should be valid after equipping Spear"), GearManager->MainhandSlotWeapon != nullptr);
	    Res &= Test->TestTrue(TEXT("Step 1: MainhandSlotWeapon has correct ItemData (Spear)"),
	        GearManager->MainhandSlotWeapon->ItemData &&
	        GearManager->MainhandSlotWeapon->ItemData->ItemId == SpearData->ItemId);

	    // ***********************************************
	    // Step 2: Attempt to equip Rock (a one-handed weapon) into offhand (LeftHandSlot)
	    // while a two-handed Spear is equipped in mainhand. Expect failure.
	    // ***********************************************
	    UItemStaticData* RockData = Subsystem->GetItemDataById(ItemIdRock);
	    Res &= Test->TestTrue(TEXT("Step 2: Rock item data should be valid"), RockData != nullptr);
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 2: Adding Rock to LeftHandSlot should fail due to two-handed spear"), AmountAdded == 0);
	    Res &= Test->TestTrue(TEXT("Step 2: OffhandSlotWeapon should remain null"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 3: Attempt to equip another Spear into offhand; expect failure.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, SpearData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 3: Adding Spear to LeftHandSlot should fail due to two-handed restriction"), AmountAdded == 0);
	    Res &= Test->TestTrue(TEXT("Step 3: OffhandSlotWeapon should still be null"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 4: Remove the Spear from mainhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Removed, true, true);
	    Res &= Test->TestTrue(TEXT("Step 4: After removal, MainhandSlotWeapon should be null"), GearManager->MainhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 5: With no two-handed weapon, equip Rock into offhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 5: Adding Rock to LeftHandSlot should succeed"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 5: OffhandSlotWeapon should be valid after equipping Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 6: With Rock still equipped in offhand, attempt to equip Spear into mainhand.
	    // Expect success with rock moved to generic inventory
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, SpearData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 6: Adding Spear to RightHandSlot should succeed even with offhand occupied"), AmountAdded == 1);
	    Res &= Test->TestTrue(TEXT("Step 6: MainhandSlotWeapon is now valid"), GearManager->MainhandSlotWeapon != nullptr);

	    // ***********************************************
	    // Step 9: Attempt to equip Rock into offhand while spear is equipped. Expect failure.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 9: Adding Rock to LeftHandSlot should fail with spear equipped"), AmountAdded == 0);
	    Res &= Test->TestTrue(TEXT("Step 9: OffhandSlotWeapon remains null"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 10: Remove Spear from mainhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Removed, true, true);
	    Res &= Test->TestTrue(TEXT("Step 10: After removal, MainhandSlotWeapon should be null"), GearManager->MainhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 11: Equip Rock into offhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 11: Adding Rock to LeftHandSlot should succeed"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 11: OffhandSlotWeapon should hold Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 12: Attempt to equip Spear into offhand directly.
	    // A two-handed weapon should never be allowed in offhand even if mainhand is empty.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, SpearData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 12: Adding Spear to LeftHandSlot should fail"), AmountAdded == 0);
	    Res &= Test->TestTrue(TEXT("Step 12: OffhandSlotWeapon remains holding Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 13: clear offhand, then MOVE Spear into mainhand;
	    // ***********************************************
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Removed, true, true);
		AmountAdded = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 1, EPreferredSlotPolicy::PreferGenericInventory);
		int32 MoveResult = InventoryComponent->MoveItem(SpearData->ItemId, 1, FGameplayTag::EmptyTag, RightHandSlot, FGameplayTag::EmptyTag, 1);
	    Res &= Test->TestTrue(TEXT("Step 13: Adding Spear to RightHandSlot should succeed"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 13: MainhandSlotWeapon should hold Spear"),
	        GearManager->MainhandSlotWeapon &&
	        GearManager->MainhandSlotWeapon->ItemData &&
	        GearManager->MainhandSlotWeapon->ItemData->ItemId == SpearData->ItemId);
	    Res &= Test->TestTrue(TEXT("Step 13: OffhandSlotWeapon should be auto-cleared"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 14: Attempt to equip Rock into offhand; should fail with spear equipped.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 14: Adding Rock to LeftHandSlot should fail with spear equipped"), AmountAdded == 0);
	    Res &= Test->TestTrue(TEXT("Step 14: OffhandSlotWeapon remains null"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 15: Remove Spear from mainhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Removed, true, true);
	    Res &= Test->TestTrue(TEXT("Step 15: After removal, MainhandSlotWeapon should be null"), GearManager->MainhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 16: Equip Rock into offhand.
	    // ***********************************************
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 16: Adding Rock to LeftHandSlot should succeed"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 16: OffhandSlotWeapon should hold Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 17: Lock the LeftHandSlot explicitly.
	    // Simulate a locked slot.
	    // ***********************************************
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Removed, true, true);
	    InventoryComponent->SetTaggedSlotBlocked(LeftHandSlot, true);
	    // Attempt to equip Rock again into the locked slot should fail.
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 17: Adding Rock to a locked LeftHandSlot should fail"), AmountAdded == 0);
	    
	    // ***********************************************
	    // Step 18: Unlock the LeftHandSlot.
	    // ***********************************************
	    InventoryComponent->SetTaggedSlotBlocked(LeftHandSlot, false);
	    AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
	    Res &= Test->TestTrue(TEXT("Step 18: Adding Rock to LeftHandSlot should now succeed after unlocking"), AmountAdded > 0);
	    Res &= Test->TestTrue(TEXT("Step 18: OffhandSlotWeapon should hold Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 19: Attempt an invalid swap using MoveItem with an empty source tag.
	    // Expect failure.
	    // ***********************************************
	    MoveResult = InventoryComponent->MoveItem(SpearData->ItemId, 1, FGameplayTag::EmptyTag, LeftHandSlot, RockData->ItemId, 1);
	    Res &= Test->TestTrue(TEXT("Step 19: Invalid swap should fail (result 0)"), MoveResult == 0);
	    Res &= Test->TestTrue(TEXT("Step 19: LeftHandSlotWeapon remains Rock"),
	        GearManager->OffhandSlotWeapon &&
	        GearManager->OffhandSlotWeapon->ItemData &&
	        GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

	    // ***********************************************
	    // Step 20: Finally, remove all items via DropAllItems_IfServer.
	    // ***********************************************
	    int32 TotalDropped = InventoryComponent->DropAllItems_IfServer();
	    Res &= Test->TestTrue(TEXT("Step 20: Dropping all items should drop at least 1 item"), TotalDropped >= 1);

	    // ***********************************************
	    // Step 21: Verify final state: both weapon slots should be empty.
	    // ***********************************************
	    Res &= Test->TestTrue(TEXT("Step 21: Final state - MainhandSlotWeapon should be null"), GearManager->MainhandSlotWeapon == nullptr);
	    Res &= Test->TestTrue(TEXT("Step 21: Final state - OffhandSlotWeapon should be null"), GearManager->OffhandSlotWeapon == nullptr);

	    // ***********************************************
	    // Step 22: (Optional additional check) Verify ActiveWeaponIndex is valid (if managed).
	    // ***********************************************
	    Res &= Test->TestTrue(TEXT("Step 22: ActiveWeaponIndex should be set (>= 0)"), GearManager->ActiveWeaponIndex >= 0);

	    return Res;
	}

	bool TestUnequippingPartially()
	{
		// Setup a test context with enough capacity and slots.
		GearManagerComponentTestContext Context(100, 9);
		auto* InventoryComponent = Context.InventoryComponent;
		auto* GearManager = Context.GearManager;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		// Equip 3 rocks to right hand, verify we get it back when asking for active weapon, them remove just one rock anmd verify we still have weapon
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 3, false);
		AWeaponActor* ActiveWeapon = GearManager->GetActiveWeapon();
		Res &= Test->TestTrue(TEXT("Active weapon should be valid after equipping 3 rocks"), ActiveWeapon != nullptr);

		for (int i = 0; i < 2; i++)
		{
			InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Removed, true);
			ActiveWeapon = GearManager->GetActiveWeapon();
			Res &= Test->TestTrue(TEXT("Active weapon should still be valid after removing 1 rock"), ActiveWeapon != nullptr);
		}

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Removed, true);
		ActiveWeapon = GearManager->GetActiveWeapon();
		Res &= Test->TestTrue(TEXT("Active weapon should be null after removing all rocks"), ActiveWeapon == nullptr);		
		
		return Res;
	}
};

bool FRancGearManagerComponentTest::RunTest(const FString& Parameters)
{
	FDebugTestResult Res = true;
	GearManagerTestScenarios TestScenarios(this);
	Res &= TestScenarios.TestEquippingWeapon();
	Res &= TestScenarios.TestBlockedSlotBehavior();
	Res &= TestScenarios.TestUnequippingWeapon();
	Res &= TestScenarios.TestInvalidWeaponSelection();
	Res &= TestScenarios.TestWeaponSelectionDeselectSequences();
	Res &= TestScenarios.TestUnequippingPartially();

	return Res;
}
