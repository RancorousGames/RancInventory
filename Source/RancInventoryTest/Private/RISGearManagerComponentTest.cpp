// Copyright Rancorous Games, 2024

#include "GearManagerComponent.h"
#include "NativeGameplayTags.h"
#include "Components/InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp" // Include for test item and tag definitions
#include "WeaponActor.h"
#include "Framework/DebugTestResult.h"
#include "GameFramework/Character.h"
#include "MockClasses/ItemHoldingCharacter.h"

#define TestName "GameTests.RIS.GearManager"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancGearManagerComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Utility functions
TArray<const UItemStaticData*> GetSelectableWeapons(const UGearManagerComponent* GearManager)
{
	// Assuming direct access for testing or use a getter if available
	return GearManager->SelectableWeaponsData;
}

bool ContainsSelectableWeapon(const UGearManagerComponent* GearManager, FGameplayTag ItemId)
{
	const UItemStaticData* ItemData = URISSubsystem::GetItemDataById(ItemId);
	if (!ItemData) return false;
	TArray<const UItemStaticData*> selectables = GetSelectableWeapons(GearManager);
	return selectables.Contains(ItemData);
}

bool CheckMainHandWeaponIsEmptyOrUnarmed(const UGearManagerComponent* GearManager)
{
	return GearManager->MainhandSlotWeapon == nullptr || GearManager->MainhandSlotWeapon == GearManager->UnarmedWeaponActor;
}

class GearManagerComponentTestContext
{
public:
	GearManagerComponentTestContext(float CarryCapacity, int32 NumSlots)
		: TestFixture(FName(*FString(TestName)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		World = TestFixture.GetWorld();
		TimerManager = &World->GetTimerManager();
		TempActor = World->SpawnActor<AItemHoldingCharacter>();
		InventoryComponent = NewObject<UInventoryComponent>(TempActor);
		TempActor->AddInstanceComponent(InventoryComponent);
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(LeftHandSlot));
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, ItemTypeTwoHanded));
		InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);
		InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);
		InventoryComponent->MaxSlotCount = NumSlots;
		InventoryComponent->MaxWeight = CarryCapacity;
		InventoryComponent->RegisterComponent();

		GearManager = NewObject<UGearManagerComponent>(TempActor);

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

		GearManager->EquipDelay = 1.0f;
		GearManager->UnequipDelay = 1.0f;
		
		TestFixture.InitializeTestItems(); // Initialize test items
		GearManager->DefaultUnarmedWeaponData = Subsystem->GetItemDataById(ItemIdUnarmed);
		
		TempActor->AddInstanceComponent(GearManager);
		GearManager->RegisterComponent();
		GearManager->Initialize();
	}

	~GearManagerComponentTestContext()
	{
		if (TempActor)
		{
			TempActor->Destroy();
		}
	}

	void TickTime(double Time) const
	{
		GFrameCounter++;
		TimerManager->Tick(Time + 0.001);
	}

	FTestFixture TestFixture;
	UWorld* World;
	FTimerManager* TimerManager;
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
		auto* Inventory = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be null initially"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));
		Res &= Test->TestTrue(
			TEXT("OffhandSlotWeapon should be null initially"), GearManager->OffhandSlotWeapon == nullptr);

		Inventory->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSpear, 1, false);
		Context.TickTime(0); // Necessary first call to activate pending timers

		
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should not yet be valid after adding the spear"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));

		Context.TickTime(GearManager->EquipDelay);
		
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be valid after equipping the spear"),GearManager->MainhandSlotWeapon != nullptr);
		Res &= Test->TestTrue(
			TEXT("OffhandSlotWeapon should still be null after equipping the spear"), GearManager->OffhandSlotWeapon == nullptr);
	
		// Remove spear and add two daggers
		Inventory->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true);
		Inventory->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, false);
		Inventory->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdBrittleCopperKnife, 1, false);

		// This should have queued up unequip > equip > equip
		
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should still be spear"),GearManager->MainhandSlotWeapon != nullptr && GearManager->MainhandSlotWeapon->ItemData->ItemId == ItemIdSpear);
	
		Context.TickTime(GearManager->UnequipDelay);
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be null after removing the spear"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));
		Res &= Test->TestTrue(
					TEXT("OffhandSlotWeapon should still be null after removing the spear"), GearManager->OffhandSlotWeapon == nullptr);

		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be valid after equipping the  first dagger"), GearManager->MainhandSlotWeapon != nullptr);
		Res &= Test->TestTrue(
			TEXT("OffhandSlotWeapon should still be null after equipping the first dagger"), GearManager->OffhandSlotWeapon == nullptr);

		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be valid after equipping the second dagger"), GearManager->MainhandSlotWeapon != nullptr);
		Res &= Test->TestTrue(
			TEXT("OffhandSlotWeapon should be valid after equipping the second dagger"), GearManager->OffhandSlotWeapon != nullptr);
		
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

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true);

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

		Context.TickTime(0);
		Context.TickTime(GearManager->EquipDelay);
		
		Res &= Test->TestTrue(
			TEXT("Weapon should be equipped before unequipping"), GearManager->MainhandSlotWeapon != nullptr);

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true);

		Context.TickTime(GearManager->UnequipDelay);
		
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should be null after dropping the weapon"),
			GearManager->MainhandSlotWeapon == nullptr || GearManager->MainhandSlotWeapon == GearManager->UnarmedWeaponActor);

		return Res;
	}

	bool TestInvalidWeaponSelection()
	{
		GearManagerComponentTestContext Context(100, 9);
		auto* GearManager = Context.GearManager;
		FDebugTestResult Res = true;

		const int32 InvalidIndex = 999;
		GearManager->SelectActiveWeapon(InvalidIndex, false);

		Res &= Test->TestTrue(
			TEXT("ActiveWeaponIndex should remain 0 after an invalid selection"), GearManager->ActiveWeaponIndex == 0);
		Res &= Test->TestTrue(
			TEXT("MainhandSlotWeapon should still be null after an invalid selection"),
			GearManager->MainhandSlotWeapon == nullptr || GearManager->MainhandSlotWeapon == GearManager->UnarmedWeaponActor);

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
		Res &= Test->TestTrue(TEXT("Step 0: MainhandSlotWeapon should be null initially"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));
		Res &= Test->TestTrue(TEXT("Step 0: OffhandSlotWeapon should be null initially"), GearManager->OffhandSlotWeapon == nullptr);

		// ***********************************************
		// Step 1: Equip Spear (a two-handed weapon) into mainhand (RightHandSlot).
		// ***********************************************
		UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Step 1: Spear item data should be valid"), SpearData != nullptr);
		int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, SpearData->ItemId, 1, false);
		Context.TickTime(0); // Necessary first call to activate pending timers
		Context.TickTime(GearManager->EquipDelay);
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
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 2: Adding Rock to LeftHandSlot should fail due to two-handed spear"), AmountAdded == 0);
		Res &= Test->TestTrue(TEXT("Step 2: OffhandSlotWeapon should remain null"), GearManager->OffhandSlotWeapon == nullptr);

		// ***********************************************
		// Step 3: Attempt to equip another Spear into offhand; expect failure.
		// ***********************************************
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, SpearData->ItemId, 1, false);
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 3: Adding Spear to LeftHandSlot should fail due to two-handed restriction"), AmountAdded == 0);
		Res &= Test->TestTrue(TEXT("Step 3: OffhandSlotWeapon should still be null"), GearManager->OffhandSlotWeapon == nullptr);

		// ***********************************************
		// Step 4: Remove the Spear from mainhand.
		// ***********************************************
		AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true, true);
		Context.TickTime(GearManager->UnequipDelay);
		Res &= Test->TestTrue(TEXT("Step 4: After removal, MainhandSlotWeapon should be null"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));

		// ***********************************************
		// Step 5: With no two-handed weapon, equip Rock into offhand.
		// ***********************************************
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->EquipDelay);
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
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 6: Adding Spear to RightHandSlot should succeed even with offhand occupied"), AmountAdded == 1);
		Res &= Test->TestTrue(TEXT("Step 6: MainhandSlotWeapon is now valid"), GearManager->MainhandSlotWeapon != nullptr);

		// ***********************************************
		// Step 9: Attempt to equip Rock into offhand while spear is equipped. Expect failure.
		// ***********************************************
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 9: Adding Rock to LeftHandSlot should fail with spear equipped"), AmountAdded == 0);
		Res &= Test->TestTrue(TEXT("Step 9: OffhandSlotWeapon remains null"), GearManager->OffhandSlotWeapon == nullptr);

		// ***********************************************
		// Step 10: Remove Spear from mainhand.
		// ***********************************************
		AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true, true);
		Context.TickTime(GearManager->UnequipDelay);
		Res &= Test->TestTrue(TEXT("Step 10: After removal, MainhandSlotWeapon should be null"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));

		// ***********************************************
		// Step 11: Equip Rock into offhand.
		// ***********************************************
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->EquipDelay);
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
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 12: Adding Spear to LeftHandSlot should fail"), AmountAdded == 0);
		Res &= Test->TestTrue(TEXT("Step 12: OffhandSlotWeapon remains holding Rock"),
			GearManager->OffhandSlotWeapon &&
			GearManager->OffhandSlotWeapon->ItemData &&
			GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

		// ***********************************************
		// Step 13: clear offhand, then MOVE Spear into mainhand;
		// ***********************************************
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Moved, true, true);
		AmountAdded = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 1, EPreferredSlotPolicy::PreferGenericInventory);
		int32 MoveResult = InventoryComponent->MoveItem(SpearData->ItemId, 1, FGameplayTag::EmptyTag, RightHandSlot, FGameplayTag::EmptyTag, 1);
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
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
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 14: Adding Rock to LeftHandSlot should fail with spear equipped"), AmountAdded == 0);
		Res &= Test->TestTrue(TEXT("Step 14: OffhandSlotWeapon remains null"), GearManager->OffhandSlotWeapon == nullptr);

		// ***********************************************
		// Step 15: Remove Spear from mainhand.
		// ***********************************************
		AmountAdded = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 999, EItemChangeReason::Moved, true, true);
		Context.TickTime(GearManager->UnequipDelay);
		Res &= Test->TestTrue(TEXT("Step 15: After removal, MainhandSlotWeapon should be null"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));

		// ***********************************************
		// Step 16: Equip Rock into offhand.
		// ***********************************************
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 16: Adding Rock to LeftHandSlot should succeed"), AmountAdded > 0);
		Res &= Test->TestTrue(TEXT("Step 16: OffhandSlotWeapon should hold Rock"),
			GearManager->OffhandSlotWeapon &&
			GearManager->OffhandSlotWeapon->ItemData &&
			GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

		// ***********************************************
		// Step 17: Lock the LeftHandSlot explicitly.
		// Simulate a locked slot.
		// ***********************************************
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Moved, true, true);
		InventoryComponent->SetTaggedSlotBlocked(LeftHandSlot, true);
		// Attempt to equip Rock again into the locked slot should fail.
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 17: Adding Rock to a locked LeftHandSlot should fail"), AmountAdded == 0);
	    
		// ***********************************************
		// Step 18: Unlock the LeftHandSlot.
		// ***********************************************
		InventoryComponent->SetTaggedSlotBlocked(LeftHandSlot, false);
		AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, RockData->ItemId, 1, false);
		Context.TickTime(GearManager->EquipDelay);
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
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestTrue(TEXT("Step 19: Invalid swap should fail (result 0)"), MoveResult == 0);
		Res &= Test->TestTrue(TEXT("Step 19: LeftHandSlotWeapon remains Rock"),
			GearManager->OffhandSlotWeapon &&
			GearManager->OffhandSlotWeapon->ItemData &&
			GearManager->OffhandSlotWeapon->ItemData->ItemId == RockData->ItemId);

		// ***********************************************
		// Step 20: Finally, remove all items via DropAllItems_IfServer.
		// ***********************************************
		int32 TotalDropped = InventoryComponent->DropAllItems_IfServer();
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->UnequipDelay);
		Context.TickTime(GearManager->UnequipDelay);
		Res &= Test->TestTrue(TEXT("Step 20: Dropping all items should drop at least 1 item"), TotalDropped >= 1);

		// Verify final state: both weapon slots should be empty.
		Res &= Test->TestTrue(TEXT("Step 21: Final state - MainhandSlotWeapon should be null"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));
		Res &= Test->TestTrue(TEXT("Step 21: Final state - OffhandSlotWeapon should be null"), GearManager->OffhandSlotWeapon == nullptr);

		// Verify ActiveWeaponIndex is valid (if managed).
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
		Context.TickTime(0);
		Context.TickTime(GearManager->EquipDelay);
		AWeaponActor* ActiveWeapon = GearManager->GetActiveWeapon();
		Res &= Test->TestTrue(TEXT("Active weapon should be valid after equipping 3 rocks"), !CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));

		for (int i = 0; i < 2; i++)
		{
			InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Moved, true);
			Context.TickTime(GearManager->UnequipDelay);
			ActiveWeapon = GearManager->GetActiveWeapon();
			Res &= Test->TestTrue(TEXT("Active weapon should still be valid after removing 1 rock"), !CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));
		}

		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Moved, true);
		Context.TickTime(GearManager->UnequipDelay);
		ActiveWeapon = GearManager->GetActiveWeapon();
		Res &= Test->TestTrue(TEXT("Active weapon should be null after removing all rocks"), CheckMainHandWeaponIsEmptyOrUnarmed(GearManager));	
		
		return Res;
	}

		bool TestSelectableWeaponsLifecycle()
	{
		FDebugTestResult Res = true;
		Test->AddInfo(TEXT("Starting Consolidated SelectableWeapons Lifecycle Test..."));

		// --- Context Setup ---
		GearManagerComponentTestContext Context(100, 10); // Use one context for the whole flow
		auto* GearManager = Context.GearManager;
		auto* Inventory = Context.InventoryComponent;
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		GearManager->MaxSelectableWeaponCount = 3; // Set a limit for testing rollover

		// Get Item Data pointers
		const UItemStaticData* UnarmedData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdUnarmed);
		const UItemStaticData* SpearData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdSpear);
		const UItemStaticData* KnifeData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdBrittleCopperKnife);
		const UItemStaticData* RockData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdRock);
        const UItemStaticData* ShortbowData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdShortbow);
        const UItemStaticData* HelmetData = Context.TestFixture.GetSubsystem()->GetItemDataById(ItemIdHelmet);

        Res &= Test->TestNotNull(TEXT("UnarmedData valid"), UnarmedData);
        Res &= Test->TestNotNull(TEXT("SpearData valid"), SpearData);
        Res &= Test->TestNotNull(TEXT("KnifeData valid"), KnifeData);
        Res &= Test->TestNotNull(TEXT("RockData valid"), RockData);
        Res &= Test->TestNotNull(TEXT("ShortbowData valid"), ShortbowData);
        Res &= Test->TestNotNull(TEXT("HelmetData valid"), HelmetData);


		// --- 1. Initial State & Adding Weapons (via Equipping) ---
		Test->AddInfo(TEXT("Testing Initial State (with Unarmed) and Adding via Equip..."));
        // Verify initial state with Unarmed automatically added by context
		Res &= Test->TestEqual(TEXT("1a. Initial Count = 1 (Unarmed)"), GetSelectableWeapons(GearManager).Num(), 1);
		Res &= Test->TestTrue(TEXT("1b. Initial State Contains Unarmed"), ContainsSelectableWeapon(GearManager, ItemIdUnarmed));
        Res &= Test->TestEqual(TEXT("1c. Initial Item at index 0 is Unarmed"), GetSelectableWeapons(GearManager)[0], UnarmedData);

		// Add Spear (Weapon 2 overall, Index 1)
		Inventory->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSpear, 1, false);
		Context.TickTime(0); Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestEqual(TEXT("1d. Add Spear: Count = 2"), GetSelectableWeapons(GearManager).Num(), 2);
		Res &= Test->TestTrue(TEXT("1e. Add Spear: Contains Unarmed"), ContainsSelectableWeapon(GearManager, ItemIdUnarmed));
        Res &= Test->TestTrue(TEXT("1f. Add Spear: Contains Spear"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
        Res &= Test->TestEqual(TEXT("1g. Add Spear: Item at index 0 is Unarmed"), GetSelectableWeapons(GearManager)[0], UnarmedData);
        Res &= Test->TestEqual(TEXT("1h. Add Spear: Item at index 1 is Spear"), GetSelectableWeapons(GearManager)[1], SpearData);


		// Add Knife to generic inventory and register it (Weapon 3 overall, Index 2) 
        Inventory->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1);
		GearManager->ManualAddSelectableWeapon(Subsystem->GetItemDataById(ItemIdBrittleCopperKnife));
		Res &= Test->TestEqual(TEXT("1i. Add Knife: Count = 3"), GetSelectableWeapons(GearManager).Num(), 3);
		Res &= Test->TestTrue(TEXT("1j. Add Knife: Contains Unarmed"), ContainsSelectableWeapon(GearManager, ItemIdUnarmed));
        Res &= Test->TestTrue(TEXT("1k. Add Knife: Contains Spear"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
		Res &= Test->TestTrue(TEXT("1l. Add Knife: Contains Knife"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));
        // Order check (Unarmed, Spear, Knife)
        Res &= Test->TestEqual(TEXT("1m. Add Knife: Item at index 0 is Unarmed"), GetSelectableWeapons(GearManager)[0], UnarmedData);
        Res &= Test->TestEqual(TEXT("1n. Add Knife: Item at index 1 is Spear"), GetSelectableWeapons(GearManager)[1], SpearData);
        Res &= Test->TestEqual(TEXT("1o. Add Knife: Item at index 2 is Knife"), GetSelectableWeapons(GearManager)[2], KnifeData);


		// Re-Equip Spear (Should not duplicate, count remains 3)
		Inventory->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Moved);
		Context.TickTime(0); Context.TickTime(GearManager->UnequipDelay);
		Inventory->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSpear, 1, false); // Re-add it
		Context.TickTime(0); Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestEqual(TEXT("1p. Re-Equip Spear: Count still 3"), GetSelectableWeapons(GearManager).Num(), 3);
        Res &= Test->TestTrue(TEXT("1q. Re-Equip Spear: Contains Unarmed"), ContainsSelectableWeapon(GearManager, ItemIdUnarmed));
        Res &= Test->TestTrue(TEXT("1r. Re-Equip Spear: Contains Spear"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
        Res &= Test->TestTrue(TEXT("1s. Re-Equip Spear: Contains Knife"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));
        // Order should remain Unarmed(0), Spear(1), Knife(2) because it finds existing Spear
        Res &= Test->TestEqual(TEXT("1t. Re-Equip Spear: Item at index 0 is Unarmed"), GetSelectableWeapons(GearManager)[0], UnarmedData);
        Res &= Test->TestEqual(TEXT("1u. Re-Equip Spear: Item at index 1 is Spear"), GetSelectableWeapons(GearManager)[1], SpearData);
        Res &= Test->TestEqual(TEXT("1v. Re-Equip Spear: Item at index 2 is Knife"), GetSelectableWeapons(GearManager)[2], KnifeData);


		// Add Rock (Weapon 4 overall) - Exceeds limit (3), should remove oldest (Unarmed)
		Inventory->AddItemToAnySlot(Subsystem, ItemIdRock, 1);
        Inventory->MoveItem(ItemIdRock, 1, FGameplayTag::EmptyTag, RightHandSlot); // Replaces Spear equip
        Context.TickTime(0); Context.TickTime(GearManager->UnequipDelay); Context.TickTime(GearManager->EquipDelay);
		Res &= Test->TestEqual(TEXT("1w. Add Rock (Limit): Count still 3"), GetSelectableWeapons(GearManager).Num(), 3);
		Res &= Test->TestFalse(TEXT("1x. Add Rock (Limit): Unarmed (oldest) removed"), ContainsSelectableWeapon(GearManager, ItemIdUnarmed));
		Res &= Test->TestTrue(TEXT("1y. Add Rock (Limit): Spear remains selectable"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
		Res &= Test->TestTrue(TEXT("1z. Add Rock (Limit): Knife remains selectable"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));
        Res &= Test->TestTrue(TEXT("1aa. Add Rock (Limit): Rock added"), ContainsSelectableWeapon(GearManager, ItemIdRock));
        // Order check (Spear, Knife, Rock)
        Res &= Test->TestEqual(TEXT("1ab. Add Rock (Limit): Item at index 0 is Spear"), GetSelectableWeapons(GearManager)[0], SpearData);
        Res &= Test->TestEqual(TEXT("1ac. Add Rock (Limit): Item at index 1 is Knife"), GetSelectableWeapons(GearManager)[1], KnifeData);
        Res &= Test->TestEqual(TEXT("1ad. Add Rock (Limit): Item at index 2 is Rock"), GetSelectableWeapons(GearManager)[2], RockData);


        // Add Shortbow (Weapon 5 overall) - Exceeds limit (3), should remove oldest (Spear)
        Inventory->AddItemToAnySlot(Subsystem, ItemIdShortbow, 1);
        Inventory->MoveItem(ItemIdShortbow, 1, FGameplayTag::EmptyTag, LeftHandSlot); // Replaces Knife equip
        Context.TickTime(0); Context.TickTime(GearManager->UnequipDelay); Context.TickTime(GearManager->EquipDelay);
        Res &= Test->TestEqual(TEXT("1ae. Add Bow (Limit): Count still 3"), GetSelectableWeapons(GearManager).Num(), 3);
        Res &= Test->TestFalse(TEXT("1af. Add Bow (Limit): Spear (oldest) removed"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
        Res &= Test->TestTrue(TEXT("1ag. Add Bow (Limit): Knife remains selectable"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));
        Res &= Test->TestTrue(TEXT("1ah. Add Bow (Limit): Rock remains selectable"), ContainsSelectableWeapon(GearManager, ItemIdRock));
        Res &= Test->TestTrue(TEXT("1ai. Add Bow (Limit): Bow added"), ContainsSelectableWeapon(GearManager, ItemIdShortbow));
        // Order check (Knife, Rock, Bow)
        Res &= Test->TestEqual(TEXT("1aj. Add Bow (Limit): Item at index 0 is Knife"), GetSelectableWeapons(GearManager)[0], KnifeData);
        Res &= Test->TestEqual(TEXT("1ak. Add Bow (Limit): Item at index 1 is Rock"), GetSelectableWeapons(GearManager)[1], RockData);
        Res &= Test->TestEqual(TEXT("1al. Add Bow (Limit): Item at index 2 is Bow"), GetSelectableWeapons(GearManager)[2], ShortbowData);


		// --- 2. Manual Adding & Removing (Simulated Client) ---
		Test->AddInfo(TEXT("Testing Manual Add/Remove..."));
        // Current state: [Knife, Rock, Bow]
        int32 currentCount = 3;

		// Manual Add
        GearManager->ManualAddSelectableWeapon(SpearData, -1); // Append
        currentCount++;
        Res &= Test->TestEqual(TEXT("2a. Manual Add Spear (Append): Count = 4"), GetSelectableWeapons(GearManager).Num(), currentCount);
        Res &= Test->TestEqual(TEXT("2b. Manual Add Spear (Append): Item at index 3 is Spear"), GetSelectableWeapons(GearManager)[currentCount - 1], SpearData);

        GearManager->ManualAddSelectableWeapon(HelmetData, 0); // Insert at 0
        currentCount++;
        Res &= Test->TestEqual(TEXT("2c. Manual Add Helmet (Insert 0): Count = 5"), GetSelectableWeapons(GearManager).Num(), currentCount);
        Res &= Test->TestEqual(TEXT("2d. Manual Add Helmet (Insert 0): Item at index 0 is Helmet"), GetSelectableWeapons(GearManager)[0], HelmetData);
        Res &= Test->TestEqual(TEXT("2e. Manual Add Helmet (Insert 0): Item at index 1 is Knife"), GetSelectableWeapons(GearManager)[1], KnifeData); // Old index 0 shifted

        // Manual Remove
        GearManager->RemoveSelectableWeapon(0); // Remove Helmet
        currentCount--;
        Res &= Test->TestEqual(TEXT("2f. Manual Remove Helmet (Index 0): Count = 4"), GetSelectableWeapons(GearManager).Num(), currentCount);
        Res &= Test->TestFalse(TEXT("2g. Manual Remove Helmet (Index 0): No longer contains Helmet"), ContainsSelectableWeapon(GearManager, ItemIdHelmet));
        Res &= Test->TestEqual(TEXT("2h. Manual Remove Helmet (Index 0): Item at index 0 is now Knife"), GetSelectableWeapons(GearManager)[0], KnifeData);

        GearManager->RemoveSelectableWeapon(99); // Remove Invalid Index
        Res &= Test->TestEqual(TEXT("2i. Manual Remove Invalid Index: Count still 4"), GetSelectableWeapons(GearManager).Num(), currentCount);

        // Remove Spear (last element, index 3)
        GearManager->RemoveSelectableWeapon(currentCount - 1);
        currentCount--;
        Res &= Test->TestEqual(TEXT("2j. Manual Remove Spear (Index 3): Count = 3"), GetSelectableWeapons(GearManager).Num(), currentCount);
		Res &= Test->TestFalse(TEXT("2k. Manual Remove Spear (Index 3): No longer contains Spear"), ContainsSelectableWeapon(GearManager, ItemIdSpear));
        // Expected state: [Knife, Rock, Bow]
        Res &= Test->TestEqual(TEXT("2l. State after manual removal: Item 0 = Knife"), GetSelectableWeapons(GearManager)[0], KnifeData);
        Res &= Test->TestEqual(TEXT("2m. State after manual removal: Item 1 = Rock"), GetSelectableWeapons(GearManager)[1], RockData);
        Res &= Test->TestEqual(TEXT("2n. State after manual removal: Item 2 = Bow"), GetSelectableWeapons(GearManager)[2], ShortbowData);


		// --- 3. Automatic Removal via Inventory ---
        Test->AddInfo(TEXT("Testing Automatic Removal via Inventory..."));
        // Current state: Selectable = [Knife, Rock, Bow]. Inventory has Rock (MH), Bow (OH). Knife is generic.
        Res &= Test->TestTrue(TEXT("3a. Setup Check: Inventory has Knife"), Inventory->Contains(ItemIdBrittleCopperKnife, 1));
        Res &= Test->TestTrue(TEXT("3b. Setup Check: Selectable has Knife"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));

        // Remove the generic Knife - it's the last one. Should trigger removal from selectable.
        Inventory->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 1, EItemChangeReason::Moved);
        Context.TickTime(0); // Allow inventory update
        // Simulate the handler call (Inventory component should do this)
        Res &= Test->TestFalse(TEXT("3c. Inventory check after removal"), Inventory->Contains(ItemIdBrittleCopperKnife, 1));
        GearManager->HandleItemRemovedFromGenericSlot(KnifeData, 1, EItemChangeReason::Moved);

        Res &= Test->TestFalse(TEXT("3d. Remove Last Knife: Selectable no longer contains Knife"), ContainsSelectableWeapon(GearManager, ItemIdBrittleCopperKnife));
        Res &= Test->TestEqual(TEXT("3e. Remove Last Knife: Count = 2"), GetSelectableWeapons(GearManager).Num(), 2);
        Res &= Test->TestEqual(TEXT("3f. Remove Last Knife: Item 0 = Rock"), GetSelectableWeapons(GearManager)[0], RockData); // Rock shifted to 0
        Res &= Test->TestEqual(TEXT("3g. Remove Last Knife: Item 1 = Bow"), GetSelectableWeapons(GearManager)[1], ShortbowData); // Bow shifted to 1


        // Add another Rock to generic inventory. Now we have equipped Rock + generic Rock.
        // Current state: Selectable=[Rock, Bow]. Inventory has Rock (MH), Bow (OH).
        Inventory->AddItemToAnySlot(Subsystem, ItemIdRock, 1); // Add generic Rock
        Res &= Test->TestEqual(TEXT("3h. Inventory has 2 Rocks"), Inventory->GetItemQuantityTotal(ItemIdRock), 2);
        Res &= Test->TestTrue(TEXT("3i. Selectable still contains Rock"), ContainsSelectableWeapon(GearManager, ItemIdRock));

        // Remove the *equipped* Rock (MH). Another Rock remains in inventory. Selectable should NOT change.
        Inventory->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, EItemChangeReason::Moved); // Use Moved/Removed
        Context.TickTime(0); Context.TickTime(GearManager->UnequipDelay);
        // Simulate handler call (even though inventory still Contains > 0)
        Res &= Test->TestTrue(TEXT("3j. Inventory still has 1 Rock (Generic)"), Inventory->Contains(ItemIdRock, 1));
        GearManager->HandleItemRemovedFromGenericSlot(RockData, 1, EItemChangeReason::Moved);

        Res &= Test->TestTrue(TEXT("3k. Remove Equipped Rock (Not Last): Selectable still contains Rock"), ContainsSelectableWeapon(GearManager, ItemIdRock));
        Res &= Test->TestEqual(TEXT("3l. Remove Equipped Rock (Not Last): Count still 2"), GetSelectableWeapons(GearManager).Num(), 2);
        // State: Selectable=[Rock, Bow]. Inventory has Rock (Generic), Bow (OH).


		// --- 4. End State Check ---
        // No need for a separate Unarmed test section, its behavior was tested in section 1.
        // Just verify the final state from section 3.
        Test->AddInfo(TEXT("Verifying Final State..."));
        Res &= Test->TestEqual(TEXT("4a. Final Count = 2"), GetSelectableWeapons(GearManager).Num(), 2);
        Res &= Test->TestEqual(TEXT("4b. Final Item 0 = Rock"), GetSelectableWeapons(GearManager)[0], RockData);
        Res &= Test->TestEqual(TEXT("4c. Final Item 1 = Bow"), GetSelectableWeapons(GearManager)[1], ShortbowData);
        Res &= Test->TestTrue(TEXT("4d. Final state contains Rock"), ContainsSelectableWeapon(GearManager, ItemIdRock));
        Res &= Test->TestTrue(TEXT("4e. Final state contains Bow"), ContainsSelectableWeapon(GearManager, ItemIdShortbow));


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
	Res &= TestScenarios.TestSelectableWeaponsLifecycle();

	return Res;
}
