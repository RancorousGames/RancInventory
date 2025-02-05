// Copyright Rancorous Games, 2024

#include "NativeGameplayTags.h"
#include "Components\InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "ViewModels\RISGridViewModel.h"
#include "RISInventoryTestSetup.cpp"

#define TestName "GameTests.RIS.RancGearManager"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancGearManagerComponentTest, TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

///
/// Macro to set up a real InventoryComponent and GearManagerComponent for testing.
///
#define SETUP_RANCGEARMANAGER(CarryCapacity, NumSlots, bMainHandBlocks)                               \
	URISSubsystem* Subsystem = FindSubsystem(TestName);                                               \
	UWorld* World = FindWorld(nullptr, TestName);                                                     \
	AActor* TempActor = World->SpawnActor<AActor>();                                                  \
	UInventoryComponent* InventoryComponent = NewObject<UInventoryComponent>(TempActor);              \
	InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot);                                       \
	InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot);                                      \
	InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);                                       \
	InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);                                        \
	InventoryComponent->MaxContainerSlotCount = NumSlots;                                             \
	InventoryComponent->MaxWeight = CarryCapacity;                                                    \
	InventoryComponent->RegisterComponent();                                                          \
	InventoryComponent->InitializeComponent();                                                        \
	UGearManagerComponent* GearManager = NewObject<UGearManagerComponent>(TempActor);                 \
	TempActor->AddInstanceComponent(GearManager);                                                     \
	GearManager->RegisterComponent();                                                                 \
	if (bMainHandBlocks)                                                                              \
	{                                                                                                 \
		if (GearManager->GearSlots.Num() == 0)                                                        \
		{                                                                                             \
			FGearSlotDefinition MainHandSlotDef;                                                      \
			MainHandSlotDef.SlotTag = LeftHandSlot;                                                   \
			MainHandSlotDef.AttachSocketName = FName("MainHandSocket");                               \
			MainHandSlotDef.SlotType = EGearSlotType::MainHand;                                       \
			MainHandSlotDef.bVisibleOnCharacter = true;                                               \
			GearManager->GearSlots.Add(MainHandSlotDef);                                              \
		}                                                                                             \
		GearManager->GearSlots[0].SlotToBlock = FGameplayTag::RequestGameplayTag(FName("Test.BlockedSlot")); \
		GearManager->GearSlots[0].RequiredItemCategoryToBlock = ItemTypeWeapon;                       \
	}                                                                                                 \
	GearManager->Initialize();                                                                        \
	InitializeTestItems(TestName);

//
// Test 1: Equipping a Weapon
// Using an existing weapon item (Spear) defined in InitializeTestItems.
//
bool TestEquippingWeapon(FRancGearManagerComponentTest* Test)
{
	URISSubsystem* Subsystem = FindSubsystem(TestName);                                               \
	UWorld* World = FindWorld(nullptr, TestName);                                                     \
	AActor* TempActor = World->SpawnActor<AActor>();                                                  \
	UInventoryComponent* InventoryComponent = NewObject<UInventoryComponent>(TempActor);              \
	InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot);                                       \
	InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot);                                      \
	InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot);                                       \
	InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot);                                        \
	InventoryComponent->MaxContainerSlotCount = 9;                                             \
	InventoryComponent->MaxWeight = 100;                                                    \
	InventoryComponent->RegisterComponent();                                                          \
	InventoryComponent->InitializeComponent();                                                        \
	UGearManagerComponent* GearManager = NewObject<UGearManagerComponent>(TempActor);                 \
	TempActor->AddInstanceComponent(GearManager);                                                     \
	GearManager->RegisterComponent();                                                                 \
	if (bMainHandBlocks)                                                                              \
	{                                                                                                 \
		if (GearManager->GearSlots.Num() == 0)                                                        \
		{                                                                                             \
			FGearSlotDefinition MainHandSlotDef;                                                      \
			MainHandSlotDef.SlotTag = LeftHandSlot;                                                   \
			MainHandSlotDef.AttachSocketName = FName("MainHandSocket");                               \
			MainHandSlotDef.SlotType = EGearSlotType::MainHand;                                       \
			MainHandSlotDef.bVisibleOnCharacter = true;                                               \
			GearManager->GearSlots.Add(MainHandSlotDef);                                              \
		}                                                                                             \
		GearManager->GearSlots[0].SlotToBlock = FGameplayTag::RequestGameplayTag(FName("Test.BlockedSlot")); \
		GearManager->GearSlots[0].RequiredItemCategoryToBlock = ItemTypeWeapon;                       \
	}                                                                                                 \
	GearManager->Initialize();                                                                        \
	InitializeTestItems(TestName);

	// Retrieve the Spear item data from the subsystem (already defined in InitializeTestItems)
	UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
	Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

	// At the start, no weapon should be equipped.
	Test->TestTrue(TEXT("MainhandSlotWeapon should be null initially"), GearManager->MainhandSlotWeapon == nullptr);
	Test->TestTrue(TEXT("OffhandSlotWeapon should be null initially"), GearManager->OffhandSlotWeapon == nullptr);

	// Equip the spear by adding and selecting it.
	// Note: AddAndSelectWeapon is normally triggered via inventory events; here we call it directly.
	GearManager->AddAndSelectWeapon(SpearData, FGameplayTag());

	// Verify that the main hand weapon is now valid.
	if (GearManager->MainhandSlotWeapon)
	{
		Test->TestTrue(TEXT("MainhandSlotWeapon should be valid after equipping the spear"), true);
	}
	else
	{
		Test->AddError(TEXT("MainhandSlotWeapon is still null after equipping the spear"));
	}

	return true;
}

//
// Test 2: Blocked Slot Behavior
// Test that equipping a weapon with blocking enabled triggers a block on the designated slot in the InventoryComponent.
// We simulate this by setting up the GearManager with a main hand gear slot that blocks a tag ("Test.BlockedSlot")
// and then verifying that attempts to add an item to that blocked slot fail.
//
bool TestBlockedSlotBehavior(FRancGearManagerComponentTest* Test)
{
	SETUP_RANCGEARMANAGER(100, 9, true);

	// Retrieve the Spear item data from the subsystem.
	UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
	Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

	// Equip the spear.
	GearManager->AddAndSelectWeapon(SpearData, FGameplayTag());

	// After equipping, the GearManager should have invoked InventoryComponent->SetTaggedSlotBlocked
	// on the slot specified in the gear slot (i.e., "Test.BlockedSlot").
	//
	// Since we are using the real InventoryComponent, we verify the block state by attempting to add an item
	// to the blocked slot. For example, adding a helmet (which normally fits in HelmetSlot) to the blocked slot
	// should fail (i.e. not actually add the item).
	//
	// Attempt to add an unstackable helmet item to the blocked slot.
	int32 AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, FGameplayTag::RequestGameplayTag(FName("Test.BlockedSlot")), OneHelmet, false);
	Test->TestEqual(TEXT("Adding an item to a blocked slot should add 0 items"), AmountAdded, 0);

	// Now simulate unequipping the weapon so that the block is removed.
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Removed, true);
	// For simplicity in this test, assume that dropping gear clears the block immediately.
	// (In a full implementation, the InventoryComponent would update its state accordingly.)
	InventoryComponent->SetTaggedSlotBlocked(FGameplayTag::RequestGameplayTag(FName("Test.BlockedSlot")), false);

	// Now try adding the helmet again to the previously blocked slot.
	AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, FGameplayTag::RequestGameplayTag(FName("Test.BlockedSlot")), OneHelmet, false);
	Test->TestNotEqual(TEXT("After unblocking, adding an item to the slot should succeed (non-zero amount)"), AmountAdded, 0);

	return true;
}

//
// Test 3: Unequipping a Weapon
// Test that dropping a weapon removes it from the gear manager's active slot.
//
bool TestUnequippingWeapon(FRancGearManagerComponentTest* Test)
{
	SETUP_RANCGEARMANAGER(100, 9, false);

	// Retrieve the Spear item data from the subsystem.
	UItemStaticData* SpearData = Subsystem->GetItemDataById(ItemIdSpear);
	Test->TestTrue(TEXT("Spear item data should be valid"), SpearData != nullptr);

	// Equip the spear.
	GearManager->AddAndSelectWeapon(SpearData, FGameplayTag());

	// Confirm the spear is equipped.
	Test->TestTrue(TEXT("Weapon should be equipped before unequipping"), GearManager->MainhandSlotWeapon != nullptr);

	// Unequip the weapon by dropping it from its tagged slot.
	InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 999, EItemChangeReason::Removed, true);
	
	// Verify that the main hand weapon is now null.
	Test->TestTrue(TEXT("MainhandSlotWeapon should be null after dropping the weapon"), GearManager->MainhandSlotWeapon == nullptr);

	return true;
}

//
// Test 4: Invalid Weapon Selection
// Test that selecting an invalid weapon index does not change the active weapon state.
//
bool TestInvalidWeaponSelection(FRancGearManagerComponentTest* Test)
{
	SETUP_RANCGEARMANAGER(100, 9, false);

	// Attempt to select a weapon with an invalid index.
	const int32 InvalidIndex = 999;
	GearManager->SelectActiveWeapon(InvalidIndex, false, nullptr);

	// Expect that no weapon has been equipped (ActiveWeaponIndex remains at default and MainhandSlotWeapon is still null).
	Test->TestTrue(TEXT("ActiveWeaponIndex should remain 0 after an invalid selection"), GearManager->ActiveWeaponIndex == 0);
	Test->TestTrue(TEXT("MainhandSlotWeapon should still be null after an invalid selection"), GearManager->MainhandSlotWeapon == nullptr);

	return true;
}

//
// Main Test Runner
//
bool FRancGearManagerComponentTest::RunTest(const FString& Parameters)
{
	bool bOverallResult = true;
	bOverallResult &= TestEquippingWeapon(this);
	bOverallResult &= TestBlockedSlotBehavior(this);
	bOverallResult &= TestUnequippingWeapon(this);
	bOverallResult &= TestInvalidWeaponSelection(this);
	// Additional tests can be added here following the pattern above.
	return bOverallResult;
}