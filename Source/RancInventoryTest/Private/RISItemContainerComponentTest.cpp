// Copyright Rancorous Games, 2024

#include "RISItemContainerComponentTest.h"

#include "LimitedTestItemSource.h"
#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/ItemContainerComponent.h"
#include "Framework/DebugTestResult.h"
#include "Framework/TestDelegateForwardHelper.h"

#define TestName "GameTests.RIS.RancItemContainer"

#define TestName "GameTests.RIS.RancItemContainer"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancItemContainerComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FItemContainerTestContext
{
public:
	FItemContainerTestContext(int32 MaxItems, float CarryCapacity)
		: TestFixture(FName(*FString(TestName)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		TempActor = TestFixture.GetWorld()->SpawnActor<AActor>();
		ItemContainerComponent = NewObject<UItemContainerComponent>(TempActor);

		ItemContainerComponent->MaxContainerSlotCount = MaxItems;
		ItemContainerComponent->MaxWeight = CarryCapacity;

		ItemContainerComponent->RegisterComponent();
		TestFixture.InitializeTestItems();
	}

	~FItemContainerTestContext()
	{
		if (TempActor)
		{
			TempActor->Destroy();
		}
	}

	FTestFixture TestFixture;
	AActor* TempActor;
	UItemContainerComponent* ItemContainerComponent;
};

class FItemContainerTestScenarios
{
public:
	static bool TestAddItems(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 10);
		auto* Subsystem = Context.TestFixture.GetSubsystem();

		FDebugTestResult Res = true;

		// Test adding an item within weight and item count limits
		int32 AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(
			Subsystem, FiveRocks, false);
		Res &= Test->TestEqual(TEXT("Should add 5 rocks"), AddedQuantity, 5);
		Res &= Test->TestEqual(
			TEXT("Total weight should be 5 after adding rocks"), Context.ItemContainerComponent->GetCurrentWeight(),
			5.0f);

		// Test adding another item that exceeds the weight limit
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(
			Subsystem, GiantBoulder, false);
		Res &= Test->TestEqual(TEXT("Should not add Giant Boulder due to weight limit"), AddedQuantity, 0);
		Res &= Test->TestEqual(
			TEXT("Total weight should remain 5 after attempting to add Giant Boulder"),
			Context.ItemContainerComponent->GetCurrentWeight(), 5.0f);

		// Test adding item partially when exceeding the max weight
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(
			Subsystem, ItemIdSticks, 6, true);
		Res &= Test->TestEqual(TEXT("Should add only 5 sticks due to weight limit"), AddedQuantity, 5);
		Res &= Test->TestEqual(
			TEXT("Total weight should be 10 after partially adding sticks"),
			Context.ItemContainerComponent->GetCurrentWeight(), 10.0f);

		// Test adding an item when not enough slots are available but under weight limit
		Context.ItemContainerComponent->Clear_IfServer(); // Clear the inventory
		Context.ItemContainerComponent->MaxContainerSlotCount = 2;
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdRock, 10, false);
		// Fill two slots with 5 rocks each
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneRock, false);
		// Try adding one more
		Res &= Test->TestEqual(TEXT("Should not add another rock due to slot limit"), AddedQuantity, 0);

		// Clear the inventory and reset item count and weight capacity for unstackable items test
		Context.ItemContainerComponent->Clear_IfServer();
		Context.ItemContainerComponent->MaxWeight = 10;

		// Test adding an unstackable item. Spear is unstackable and has a weight of 3
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneSpear, false);
		Res &= Test->TestEqual(TEXT("Should add 1 spear"), AddedQuantity, 1);
		Res &= Test->TestEqual(
			TEXT("Total weight should be 3 after adding spear"), Context.ItemContainerComponent->GetCurrentWeight(),
			3.0f);
		Res &= Test->TestEqual(
			TEXT("Total used slot count should be 1"), Context.ItemContainerComponent->UsedContainerSlotCount, 1);

		// Test adding another unstackable item (Helmet), which also should not stack
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);
		Res &= Test->TestEqual(TEXT("Should add 1 helmet"), AddedQuantity, 1);
		Res &= Test->TestEqual(
			TEXT("Total weight should be 5 after adding helmet"), Context.ItemContainerComponent->GetCurrentWeight(),
			5.0f);
		Res &= Test->TestEqual(
			TEXT("Total used slot count should be 2"), Context.ItemContainerComponent->UsedContainerSlotCount, 2);

		// Test attempting to add another unstackable item when it would exceed the slot limit
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneSpear, false);
		Res &= Test->TestEqual(TEXT("Should not add another spear due to item count limit"), AddedQuantity, 0);
		Res &= Test->TestEqual(
			TEXT("Total item count should remain 2 after attempting to add another spear"),
			Context.ItemContainerComponent->GetAllContainerItems().Num(), 2);
		Res &= Test->TestEqual(
			TEXT("Total used slot count should be 2"), Context.ItemContainerComponent->UsedContainerSlotCount, 2);

		Context.ItemContainerComponent->MaxContainerSlotCount = 3; // add one more slot
		// Test attempting to add an unstackable item that exceeds the weight limit
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(
			Subsystem, GiantBoulder, false);
		Res &= Test->TestEqual(TEXT("Should not add heavy item due to weight limit"), AddedQuantity, 0);
		Res &= Test->TestEqual(
			TEXT("Total weight should remain 5 after attempting to add heavy helmet"),
			Context.ItemContainerComponent->GetCurrentWeight(), 5.0f);
		Res &= Test->TestEqual(
			TEXT("Total used slot count should be 2"), Context.ItemContainerComponent->UsedContainerSlotCount, 2);

		// but a small rock we have weight capacity for
		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneRock, false);
		Res &= Test->TestEqual(TEXT("Should add another rock"), AddedQuantity, 1);
		Res &= Test->TestEqual(
			TEXT("Total weight should be 6 after adding another rock"),
			Context.ItemContainerComponent->GetCurrentWeight(), 6.0f);
		Res &= Test->TestEqual(
			TEXT("Total used slot count should be 3 after adding another rock"),
			Context.ItemContainerComponent->UsedContainerSlotCount, 3);

		return Res;
	}

	static bool TestDestroyItems(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 20);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		// Add initial items for removal tests
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, FiveRocks, false);
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneSpear, false);
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);

		// Test removing a stackable item partially
		int32 RemovedQuantity = Context.ItemContainerComponent->DestroyItem_IfServer(
			TwoRocks, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(TEXT("Should remove 2 rocks"), RemovedQuantity, 2);
		Res &= Test->TestEqual(
			TEXT("Total rocks should be 3 after removal"),
			Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock), 3);

		// Test removing a stackable item completely
		RemovedQuantity = Context.ItemContainerComponent->DestroyItem_IfServer(
			ThreeRocks, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(TEXT("Should remove 3 rocks"), RemovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Rocks should be completely removed"), !Context.ItemContainerComponent->Contains(ItemIdRock, 1));

		// Test removing an unstackable item (Spear)
		RemovedQuantity = Context.ItemContainerComponent->DestroyItem_IfServer(
			OneSpear, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(TEXT("Should remove 1 spear"), RemovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Spear should be completely removed"), !Context.ItemContainerComponent->Contains(ItemIdSpear, 1));

		// Test attempting to remove more items than are available (Helmet)
		RemovedQuantity = Context.ItemContainerComponent->DestroyItem_IfServer(
			ItemIdHelmet, 2, EItemChangeReason::Removed, false);
		Res &= Test->TestEqual(TEXT("Should not remove any helmets as quantity exceeds available"), RemovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Helmet should remain after failed removal attempt"),
			Context.ItemContainerComponent->Contains(ItemIdHelmet, 1));

		// Test partial removal when not allowed (Helmet)
		RemovedQuantity = Context.ItemContainerComponent->DestroyItem_IfServer(
			OneHelmet, EItemChangeReason::Removed, false);
		Res &= Test->TestEqual(TEXT("Should remove helmet"), RemovedQuantity, 1);
		Res &= Test->TestFalse(
			TEXT("Helmet should be removed after successful removal"),
			Context.ItemContainerComponent->Contains(ItemIdHelmet, 1));

		return Res;
	}

	static bool TestCanReceiveItems(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(7, 15);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		Res &= Test->TestTrue(
			TEXT("Container should initially be able to receive rocks"),
			Context.ItemContainerComponent->CanContainerReceiveItems(FiveRocks));

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ThreeRocks, false);

		Res &= Test->TestTrue(
			TEXT("Container should still be able to receive more rocks"),
			Context.ItemContainerComponent->CanContainerReceiveItems(TwoRocks));
		Res &= Test->TestFalse(
			TEXT("Container should not be able to receive more rocks than its weight limit"),
			Context.ItemContainerComponent->CanContainerReceiveItems(ItemIdRock, 13));

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, 5, false);

		Res &= Test->TestTrue(
			TEXT("Container should be able to receive a helmet"),
			Context.ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
		Res &= Test->TestFalse(
			TEXT("Container should not be able to receive a spear"),
			Context.ItemContainerComponent->CanContainerReceiveItems(OneSpear));

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, TwoRocks, false);

		Res &= Test->TestFalse(
			TEXT("Container should not be able to receive any more items due to weight limit"),
			Context.ItemContainerComponent->CanContainerReceiveItems(ItemIdRock, 1));
		Res &= Test->TestFalse(
			TEXT("Container should not be able to receive any more unstackable items due to item count limit"),
			Context.ItemContainerComponent->CanContainerReceiveItems(OneHelmet));

		Context.ItemContainerComponent->MaxWeight = 20;
		Res &= Test->TestTrue(
			TEXT("Container should now be able to receive 1 more item"),
			Context.ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);
		Res &= Test->TestFalse(
			TEXT("Container should not be able to receive any more unstackable items due to slot count limit"),
			Context.ItemContainerComponent->CanContainerReceiveItems(OneHelmet));
		return Res;
	}

	static bool TestItemCountsAndPresence(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 20);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, FiveRocks, false);
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);

		Res &= Test->TestEqual(
			TEXT("Inventory should report 5 rocks"), Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock),
			5);
		Res &= Test->TestEqual(
			TEXT("Inventory should report 1 helmet"),
			Context.ItemContainerComponent->GetContainedQuantity(ItemIdHelmet), 1);

		Res &= Test->TestTrue(
			TEXT("Inventory should contain at least 5 rocks"), Context.ItemContainerComponent->Contains(ItemIdRock, 5));
		Res &= Test->TestFalse(
			TEXT("Inventory should not falsely report more rocks than it contains"),
			Context.ItemContainerComponent->Contains(ItemIdRock, 6));
		Res &= Test->TestTrue(
			TEXT("Inventory should confirm the presence of the helmet"),
			Context.ItemContainerComponent->Contains(ItemIdHelmet, 1));
		Res &= Test->TestFalse(
			TEXT("Inventory should not report more helmets than it contains"),
			Context.ItemContainerComponent->Contains(ItemIdHelmet, 2));

		TArray<FItemBundleWithInstanceData> AllItems = Context.ItemContainerComponent->GetAllContainerItems();
		Res &= Test->TestTrue(
			TEXT("GetAllItems should include rocks"), AllItems.ContainsByPredicate(
				[](const FItemBundleWithInstanceData& Item)
				{
					return Item.ItemId == ItemIdRock && Item.Quantity == 5;
				}));
		Res &= Test->TestTrue(
			TEXT("GetAllItems should include the helmet"), AllItems.ContainsByPredicate(
				[](const FItemBundleWithInstanceData& Item)
				{
					return Item.ItemId == ItemIdHelmet && Item.Quantity == 1;
				}));

		Context.ItemContainerComponent->DestroyItem_IfServer(ThreeRocks, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(
			TEXT("After removal, inventory should report 2 rocks"),
			Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock), 2);

		Res &= Test->TestFalse(TEXT("Inventory should not be empty"), Context.ItemContainerComponent->IsEmpty());

		Context.ItemContainerComponent->Clear_IfServer();
		Res &= Test->TestTrue(
			TEXT("After clearing, inventory should be empty"), Context.ItemContainerComponent->IsEmpty());

		return Res;
	}

	static bool TestMiscFunctions(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 50);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		FItemBundleWithInstanceData RockItemInstance = Context.ItemContainerComponent->FindItemById(ItemIdRock);
		Res &= Test->TestTrue(
			TEXT("FindItemById should not find an item before it's added"), RockItemInstance.ItemId != ItemIdRock);

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdRock, 1);

		RockItemInstance = Context.ItemContainerComponent->FindItemById(ItemIdRock);
		Res &= Test->TestTrue(
			TEXT("FindItemById should find the item after it's added"), RockItemInstance.ItemId == ItemIdRock);

		Res &= Test->TestTrue(
			TEXT("ContainsItems should return true for items present in the container"),
			Context.ItemContainerComponent->Contains(ItemIdRock, 1));

		Res &= Test->TestFalse(
			TEXT("IsEmpty should return false when items are present"), Context.ItemContainerComponent->IsEmpty());

		Context.ItemContainerComponent->Clear_IfServer();

		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdRock, 1);
		Context.ItemContainerComponent->DropItems(ItemIdRock, 1);

		return Res;
	}

	static bool TestSetAddItemValidationCallback(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 50);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		const auto DelegateHelper = NewObject<UTestDelegateForwardHelper>();
		UItemContainerComponent::FAddItemValidationDelegate MyDelegateInstance;
		MyDelegateInstance.BindUFunction(DelegateHelper, FName("DispatchItemToInt"));
		Context.ItemContainerComponent->SetAddItemValidationCallback_IfServer(MyDelegateInstance);

		DelegateHelper->CallFuncItemToInt = [](const FGameplayTag& ItemId, int32 RequestedQuantity)
		{
			return ItemId.MatchesTag(ItemIdRock) ? RequestedQuantity : 0;
		};

		int32 AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(
			Subsystem, FiveRocks, false);
		Res &= Test->TestEqual(TEXT("Should add 5 rocks since rocks are allowed"), AddedQuantity, 5);

		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);
		Res &= Test->TestEqual(TEXT("Should not add the helmet since only rocks are allowed"), AddedQuantity, 0);

		DelegateHelper->CallFuncItemToInt = [&](const FGameplayTag& ItemId, int32 Quantity) { return Quantity; };

		AddedQuantity = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, OneHelmet, false);
		Res &= Test->TestEqual(TEXT("Should add the helmet now that all items are allowed"), AddedQuantity, 1);

		return Res;
	}
	
	static bool TestExtractItems(FRancItemContainerComponentTest* Test)
	{
		FItemContainerTestContext Context(10, 50);
		auto* Subsystem = Context.TestFixture.GetSubsystem();
		
		FDebugTestResult Res = true;

		// Add 20 rocks and then extract 5, verify that we got 5 and 15 remain
		int32 Added = Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdRock, 20, false);
		Res &= Test->TestEqual(TEXT("Should add 20 rocks"), Added, 20);
		TArray<UItemInstanceData*> ExtractedDynamicItems = TArray<UItemInstanceData*>();
		int ExtractedCount = Context.ItemContainerComponent->ExtractItem_IfServer(ItemIdRock, 5, EItemChangeReason::Removed, ExtractedDynamicItems);
		Res &= Test->TestEqual(TEXT("Should extract 5 rocks"), ExtractedCount, 5);
		Res &= Test->TestEqual(TEXT("Should have 15 rocks remaining"), Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock), 15);
		Res &= Test->TestEqual(TEXT("Should have no dynamic data as rocks dont have instance data"), ExtractedDynamicItems.Num(), 0);

		// Now try to add an item with instance data and extract it
		Context.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 1, false);
		// Get item state then cast it to ItemDurabilityTestInstanceData
		TArray<UItemInstanceData*> ItemState = Context.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		// verify we got 1 item state
		Res &= Test->TestEqual(TEXT("Should have 1 item state"), ItemState.Num(), 1);
		UItemDurabilityTestInstanceData* DurabilityData = Cast<UItemDurabilityTestInstanceData>(ItemState[0]);
		// verify that the item state is of the correct type
		Res &= Test->TestNotNull(TEXT("Item state should be of the correct type"), DurabilityData);
		DurabilityData->Durability = 50;

		// Now extract it and verify that the item state is returned
		ExtractedDynamicItems = TArray<UItemInstanceData*>();
		ExtractedCount = Context.ItemContainerComponent->ExtractItem_IfServer(ItemIdBrittleCopperKnife, 1, EItemChangeReason::Removed, ExtractedDynamicItems);
		Res &= Test->TestEqual(TEXT("Should extract 1 knife"), ExtractedCount, 1);
		Res &= Test->TestEqual(TEXT("Should have 0 knives remaining"), Context.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 0);
		Res &= Test->TestEqual(TEXT("Should have 1 extracted knife instance data"), ExtractedDynamicItems.Num(), 1);
		if (ExtractedDynamicItems.Num() > 0)
		{
			UItemDurabilityTestInstanceData* ExtractedDurabilityData = Cast<UItemDurabilityTestInstanceData>(ExtractedDynamicItems[0]);
			Res &= Test->TestNotNull(TEXT("Extracted item state should be of the correct type"), ExtractedDurabilityData);
			Res &= Test->TestEqual(TEXT("Extracted item state should have the correct durability"), ExtractedDurabilityData->Durability, 50.f);
		}

		Context.ItemContainerComponent->Clear_IfServer();
		// Now we test with a ULimitedTestItemSource to verify how much we extract
		ULimitedTestItemSource* LimitedSource = NewObject<ULimitedTestItemSource>();
		LimitedSource->SourceRemainder = 5;

		Added = Context.ItemContainerComponent->AddItem_IfServer(LimitedSource, ItemIdRock, 10, false);
		Res &= Test->TestEqual(TEXT("Should add 5 rocks"), Added, 5);
		Res &= Test->TestEqual(TEXT("Should have 5 rocks"), Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock), 5);
		Res &= Test->TestEqual(TEXT("Should have exhausted the source"), LimitedSource->SourceRemainder, 0);
		// Try to add one more which should fail

		Added = Context.ItemContainerComponent->AddItem_IfServer(LimitedSource, ItemIdRock, 1, false);
		Res &= Test->TestEqual(TEXT("Should not add a rock"), Added, 0);
		Res &= Test->TestEqual(TEXT("Should still have 5 rocks"), Context.ItemContainerComponent->GetContainedQuantity(ItemIdRock), 5);
		Res &= Test->TestEqual(TEXT("Should still have exhausted the source"), LimitedSource->SourceRemainder, 0);
		
		return Res;
	}
};

bool FRancItemContainerComponentTest::RunTest(const FString& Parameters)
{
	FDebugTestResult Res = true;

	Res &= FItemContainerTestScenarios::TestDestroyItems(this);
	Res &= FItemContainerTestScenarios::TestCanReceiveItems(this);
	Res &= FItemContainerTestScenarios::TestItemCountsAndPresence(this);
	Res &= FItemContainerTestScenarios::TestMiscFunctions(this);
	Res &= FItemContainerTestScenarios::TestSetAddItemValidationCallback(this);
	Res &= FItemContainerTestScenarios::TestExtractItems(this);

	return Res;
}
