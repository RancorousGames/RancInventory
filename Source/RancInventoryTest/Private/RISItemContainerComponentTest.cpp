// Copyright Rancorous Games, 2024

#include "RISItemContainerComponentTest.h"

#include "EngineUtils.h"
#include "LimitedTestItemSource.h"
#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/ItemContainerComponent.h"
#include "Framework/DebugTestResult.h"
#include "Framework/TestDelegateForwardHelper.h"
#include "MockClasses/ItemHoldingCharacter.h"

#define TestName "GameTests.RIS.ItemContainerComponent"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancItemContainerComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

URecursiveContainerInstanceData* GetRecursiveInstanceData(const FItemBundleWithInstanceData& Bundle)
{
	if (Bundle.InstanceData.IsValidIndex(0))
	{
		return Cast<URecursiveContainerInstanceData>(Bundle.InstanceData[0]);
	}
	return nullptr;
}

// Helper to safely get the UItemContainerComponent managed by a Recursive Container Instance Data
UItemContainerComponent* GetRecursiveContainerComponent(const FItemBundleWithInstanceData& Bundle)
{
	if (URecursiveContainerInstanceData* InstanceData = GetRecursiveInstanceData(Bundle))
	{
		return InstanceData->RepresentedContainer;
	}
	return nullptr;
}

// Helper to find a component of a specific class on an actor
template <typename T>
T* FindComponent(AActor* Actor)
{
	if (!Actor) return nullptr;
	return Actor->FindComponentByClass<T>();
}

// Helper to find ALL components of a specific class on an actor
template <typename T>
TArray<T*> FindAllComponents(AActor* Actor)
{
	TArray<T*> Components;
	if (Actor)
	{
		Actor->GetComponents<T>(Components);
	}
	return Components;
}

class FItemContainerTestContext
{
public:
	FItemContainerTestContext(int32 MaxItems, float CarryCapacity)
		: TestFixture(FName(*FString(TestName)))
	{
		URISSubsystem* Subsystem = TestFixture.GetSubsystem();
		TempActor = TestFixture.GetWorld()->SpawnActor<AItemHoldingCharacter>();
		ItemContainerComponent = NewObject<UItemContainerComponent>(TempActor);

		ItemContainerComponent->MaxSlotCount = MaxItems;
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
		Context.ItemContainerComponent->MaxSlotCount = 2;
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

		Context.ItemContainerComponent->MaxSlotCount = 3; // add one more slot
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

		static bool TestInstanceDataTransferBetweenContainers(FRancItemContainerComponentTest* Test)
	{
		// --- Setup ---
		FItemContainerTestContext ContextA(10, 50);
		FItemContainerTestContext ContextB(10, 50); // Second container
		auto* Subsystem = ContextA.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;

		const float TestDurability = 55.f;

		// --- Test ---
		// 1. Add Brittle Knife (with instance data) to Container A from Subsystem (infinite source)
		int32 Added = ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 1, false);
		Res &= Test->TestEqual(TEXT("[Transfer] Should add 1 knife to Container A"), Added, 1);

		// 2. Verify Instance Data creation and registration in Container A
		TArray<UItemInstanceData*> ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Transfer] Container A should have 1 instance data entry for the knife"), ItemStateA.Num(), 1);
		if (ItemStateA.Num() == 1)
		{
			UItemDurabilityTestInstanceData* DurabilityDataA = Cast<UItemDurabilityTestInstanceData>(ItemStateA[0]);
			Res &= Test->TestNotNull(TEXT("[Transfer] Instance data in A should be castable to Durability type"), DurabilityDataA);
			if (DurabilityDataA)
			{
				// Modify the data
				DurabilityDataA->Durability = TestDurability;
				// Check registration
				Res &= Test->TestTrue(TEXT("[Transfer] Instance data should be registered subobject with Owner A"),
				                      ContextA.TempActor->IsReplicatedSubObjectRegistered(DurabilityDataA));
			}
		}

		// 3. Transfer the knife from Container A to Container B
		UItemInstanceData* InstancePtrBeforeTransfer = ItemStateA.Num() > 0 ? ItemStateA[0] : nullptr; // Keep pointer for later check
		int32 Transferred = ContextB.ItemContainerComponent->ExtractItemFromContainer_IfServer(
			ItemIdBrittleCopperKnife, 1, ContextA.ItemContainerComponent, false);
		Res &= Test->TestEqual(TEXT("[Transfer] Should transfer 1 knife from A to B"), Transferred, 1);

		// 4. Verify Item state in Container A after transfer
		Res &= Test->TestEqual(TEXT("[Transfer] Container A should have 0 knives after transfer"),
		                       ContextA.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 0);
		ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Transfer] Container A should have 0 instance data entries after transfer"), ItemStateA.Num(), 0);
		if (InstancePtrBeforeTransfer)
		{
			Res &= Test->TestFalse(TEXT("[Transfer] Instance data should NOT be registered subobject with Owner A after transfer"),
			                       ContextA.TempActor->IsReplicatedSubObjectRegistered(InstancePtrBeforeTransfer));
		}

		// 5. Verify Item state and Instance Data in Container B after transfer
		Res &= Test->TestEqual(TEXT("[Transfer] Container B should have 1 knife after transfer"),
		                       ContextB.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
		TArray<UItemInstanceData*> ItemStateB = ContextB.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Transfer] Container B should have 1 instance data entry after transfer"), ItemStateB.Num(), 1);
		if (ItemStateB.Num() == 1)
		{
			UItemDurabilityTestInstanceData* DurabilityDataB = Cast<UItemDurabilityTestInstanceData>(ItemStateB[0]);
			Res &= Test->TestNotNull(TEXT("[Transfer] Instance data in B should be castable to Durability type"), DurabilityDataB);
			if (DurabilityDataB)
			{
				Res &= Test->TestEqual(TEXT("[Transfer] Durability value should be preserved after transfer"), DurabilityDataB->Durability, TestDurability);
				Res &= Test->TestTrue(TEXT("[Transfer] Instance data should be registered subobject with Owner B after transfer"),
				                      ContextB.TempActor->IsReplicatedSubObjectRegistered(DurabilityDataB));

				// Also verify the pointer itself was transferred (not a copy)
				Res &= Test->TestTrue(TEXT("[Transfer] Instance data pointer should be the same object transferred"), DurabilityDataB == InstancePtrBeforeTransfer);
			}
		}

		// 6. Add a rock (no instance data) and transfer it - ensure no instance data appears
		Added = ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdRock, 1, false);
		Res &= Test->TestEqual(TEXT("[Transfer] Should add 1 rock to Container A"), Added, 1);
		Transferred = ContextB.ItemContainerComponent->ExtractItemFromContainer_IfServer(ItemIdRock, 1, ContextA.ItemContainerComponent, false);
		Res &= Test->TestEqual(TEXT("[Transfer] Should transfer 1 rock from A to B"), Transferred, 1);
		ItemStateB = ContextB.ItemContainerComponent->GetItemState(ItemIdRock);
		Res &= Test->TestEqual(TEXT("[Transfer] Container B should have 0 instance data entries for the rock"), ItemStateB.Num(), 0);

		return Res;
	}


	static bool TestInstanceDataDropPickupAndDestruction(FRancItemContainerComponentTest* Test)
	{
		// --- Setup ---
		FItemContainerTestContext ContextA(10, 50);
		FItemContainerTestContext ContextB(10, 50); // For picking up
		auto* Subsystem = ContextA.TestFixture.GetSubsystem();
		FDebugTestResult Res = true;
		const float TestDurabilityDrop = 77.f;
		const float TestDurabilityDestroy1 = 33.f;
		const float TestDurabilityDestroy2 = 44.f;

		// --- Part 1: Drop and Pickup ---

		// 1. Add knife to Container A, set durability
		int32 Added = ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 1, false);
		Res &= Test->TestEqual(TEXT("[DropPickup] Should add 1 knife to Container A"), Added, 1);
		UItemDurabilityTestInstanceData* DurabilityDataA = nullptr;
		TArray<UItemInstanceData*> ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		if (ItemStateA.Num() == 1) DurabilityDataA = Cast<UItemDurabilityTestInstanceData>(ItemStateA[0]);
		Res &= Test->TestNotNull(TEXT("[DropPickup] Instance data A created"), DurabilityDataA);
		if (DurabilityDataA) DurabilityDataA->Durability = TestDurabilityDrop;

		UItemInstanceData* InstancePtrBeforeDrop = DurabilityDataA;
		Res &= Test->TestTrue(TEXT("[DropPickup] Instance should be registered with Owner A before drop"),
		                      ContextA.TempActor->IsReplicatedSubObjectRegistered(InstancePtrBeforeDrop));

		// 2. Drop the knife
		int32 Dropped = ContextA.ItemContainerComponent->DropItems(ItemIdBrittleCopperKnife, 1);
		Res &= Test->TestEqual(TEXT("[DropPickup] DropItems should report 1 item dropped"), Dropped, 1); // Client guess, but check anyway
		Res &= Test->TestEqual(TEXT("[DropPickup] Container A should have 0 knives after drop"),
		                       ContextA.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 0);
		Res &= Test->TestFalse(TEXT("[DropPickup] Instance should NOT be registered with Owner A after drop"),
		                       ContextA.TempActor->IsReplicatedSubObjectRegistered(InstancePtrBeforeDrop));

		// 3. Find the spawned AWorldItem
		AWorldItem* DroppedWorldItem = nullptr;
		UWorld* World = ContextA.TestFixture.GetWorld();
		for (TActorIterator<AWorldItem> It(World); It; ++It)
		{
			if (It->RepresentedItem.ItemId == ItemIdBrittleCopperKnife)
			{
				DroppedWorldItem = *It;
				break;
			}
		}
		Res &= Test->TestNotNull(TEXT("[DropPickup] Should find spawned AWorldItem for the knife"), DroppedWorldItem);

		// 4. Verify WorldItem state and instance data
		UItemDurabilityTestInstanceData* DurabilityDataWorld = nullptr;
		if (DroppedWorldItem)
		{
			Res &= Test->TestEqual(TEXT("[DropPickup] WorldItem should represent 1 knife"), DroppedWorldItem->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
			TArray<UItemInstanceData*>& WorldItemState = DroppedWorldItem->RepresentedItem.InstanceData;
			Res &= Test->TestEqual(TEXT("[DropPickup] WorldItem should have 1 instance data entry"), WorldItemState.Num(), 1);
			if (WorldItemState.Num() == 1)
			{
				DurabilityDataWorld = Cast<UItemDurabilityTestInstanceData>(WorldItemState[0]);
				Res &= Test->TestNotNull(TEXT("[DropPickup] Instance data in WorldItem should be castable"), DurabilityDataWorld);
				if (DurabilityDataWorld)
				{
					Res &= Test->TestEqual(TEXT("[DropPickup] Durability should be preserved in WorldItem"), DurabilityDataWorld->Durability, TestDurabilityDrop);
					Res &= Test->TestTrue(TEXT("[DropPickup] Instance should be registered with WorldItem actor"),
					                      DroppedWorldItem->IsReplicatedSubObjectRegistered(DurabilityDataWorld));
					// Check pointer transfer
					Res &= Test->TestTrue(TEXT("[DropPickup] WorldItem instance data pointer should be the same object"), DurabilityDataWorld == InstancePtrBeforeDrop);
				}
			}
		}

		// 5. Pick up the item into Container B
		UItemInstanceData* InstancePtrBeforePickup = DurabilityDataWorld;
		Added = ContextB.ItemContainerComponent->AddItem_IfServer(DroppedWorldItem, ItemIdBrittleCopperKnife, 1, false);
		Res &= Test->TestEqual(TEXT("[DropPickup] Should add 1 knife to Container B from WorldItem"), Added, 1);

		// 6. Verify WorldItem state after pickup
		if (DroppedWorldItem)
		{
			// WorldItem's ExtractItem should have removed the instance data and quantity
			Res &= Test->TestEqual(TEXT("[DropPickup] WorldItem should have 0 knives after pickup"), DroppedWorldItem->GetContainedQuantity(ItemIdBrittleCopperKnife), 0);
			Res &= Test->TestEqual(TEXT("[DropPickup] WorldItem should have 0 instance data entries after pickup"), DroppedWorldItem->RepresentedItem.InstanceData.Num(), 0);
			if (InstancePtrBeforePickup)
			{
				Res &= Test->TestFalse(TEXT("[DropPickup] Instance should NOT be registered with WorldItem after pickup"),
				                       DroppedWorldItem->IsReplicatedSubObjectRegistered(InstancePtrBeforePickup));
			}
			// In a real game, the WorldItem might destroy itself here. We'll just leave it empty for the test.
			DroppedWorldItem->Destroy(); // Clean up actor
		}

		// 7. Verify Container B state after pickup
		Res &= Test->TestEqual(TEXT("[DropPickup] Container B should have 1 knife after pickup"),
		                       ContextB.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
		TArray<UItemInstanceData*> ItemStateB = ContextB.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[DropPickup] Container B should have 1 instance data entry after pickup"), ItemStateB.Num(), 1);
		if (ItemStateB.Num() == 1)
		{
			UItemDurabilityTestInstanceData* DurabilityDataB = Cast<UItemDurabilityTestInstanceData>(ItemStateB[0]);
			Res &= Test->TestNotNull(TEXT("[DropPickup] Instance data in B should be castable"), DurabilityDataB);
			if (DurabilityDataB)
			{
				Res &= Test->TestEqual(TEXT("[DropPickup] Durability should be preserved in Container B after pickup"), DurabilityDataB->Durability, TestDurabilityDrop);
				Res &= Test->TestTrue(TEXT("[DropPickup] Instance should be registered with Owner B after pickup"),
				                      ContextB.TempActor->IsReplicatedSubObjectRegistered(DurabilityDataB));
				// Check pointer transfer
				Res &= Test->TestTrue(TEXT("[DropPickup] Container B instance data pointer should be the same object"), DurabilityDataB == InstancePtrBeforePickup);
			}
		}

		// --- Part 2: Creation and Destruction ---
		ContextA.ItemContainerComponent->Clear_IfServer(); // Start fresh for destruction tests
		TArray<UItemInstanceData*> PointersToDestroy;

		// 8. Add multiple knives
		Added = ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 3, true);
		Res &= Test->TestEqual(TEXT("[Destroy] Should add 3 knives"), Added, 3);
		ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Destroy] Should have 3 instance data entries"), ItemStateA.Num(), 3);
		if (ItemStateA.Num() == 3)
		{
			Cast<UItemDurabilityTestInstanceData>(ItemStateA[0])->Durability = TestDurabilityDestroy1;
			Cast<UItemDurabilityTestInstanceData>(ItemStateA[1])->Durability = TestDurabilityDestroy2;
			Cast<UItemDurabilityTestInstanceData>(ItemStateA[2])->Durability = 99.f; // The one we expect to keep initially
			PointersToDestroy.Add(ItemStateA[1]); // Track pointers for registration checks later
			PointersToDestroy.Add(ItemStateA[2]);
			Res &= Test->TestTrue(TEXT("[Destroy] Instance 0 should be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(ItemStateA[0]));
			Res &= Test->TestTrue(TEXT("[Destroy] Instance 1 should be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(ItemStateA[1]));
			Res &= Test->TestTrue(TEXT("[Destroy] Instance 2 should be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(ItemStateA[2]));
		}

		// 9. Destroy two knives
		int32 Destroyed = ContextA.ItemContainerComponent->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 2, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(TEXT("[Destroy] Should destroy 2 knives"), Destroyed, 2);
		Res &= Test->TestEqual(TEXT("[Destroy] Should have 1 knife remaining"), ContextA.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
		ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Destroy] Should have 1 instance data entry remaining"), ItemStateA.Num(), 1);

		// 10. Verify the last two instance data entries were destroyed and unregistered
		Res &= Test->TestFalse(TEXT("[Destroy] Destroyed Instance 0 should NOT be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(PointersToDestroy[0]));
		Res &= Test->TestFalse(TEXT("[Destroy] Destroyed Instance 1 should NOT be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(PointersToDestroy[1]));
		if (ItemStateA.Num() == 1)
		{
			UItemDurabilityTestInstanceData* RemainingData = Cast<UItemDurabilityTestInstanceData>(ItemStateA[0]);
			Res &= Test->TestNotNull(TEXT("[Destroy] Remaining instance data should be valid"), RemainingData);
			if (RemainingData)
			{
				// DestroyQuantity pops from the end, so the first one added should remain.
				Res &= Test->TestEqual(TEXT("[Destroy] Remaining instance data should have correct durability"), RemainingData->Durability, TestDurabilityDestroy1);
				Res &= Test->TestTrue(TEXT("[Destroy] Remaining instance data should still be registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(RemainingData));
			}
		}

		// 11. Destroy the last knife
		UItemInstanceData* LastInstancePtr = ItemStateA.Num() > 0 ? ItemStateA[0] : nullptr;
		Destroyed = ContextA.ItemContainerComponent->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 1, EItemChangeReason::Removed, true);
		Res &= Test->TestEqual(TEXT("[Destroy] Should destroy the last knife"), Destroyed, 1);
		Res &= Test->TestEqual(TEXT("[Destroy] Should have 0 knives remaining"), ContextA.ItemContainerComponent->GetContainedQuantity(ItemIdBrittleCopperKnife), 0);
		ItemStateA = ContextA.ItemContainerComponent->GetItemState(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("[Destroy] Should have 0 instance data entries remaining"), ItemStateA.Num(), 0);
		if (LastInstancePtr)
		{
			Res &= Test->TestFalse(TEXT("[Destroy] Last instance should NOT be registered after destruction"), ContextA.TempActor->IsReplicatedSubObjectRegistered(LastInstancePtr));
		}

		return Res;
	}

	    static bool TestRecursiveContainerLifecycle(FRancItemContainerComponentTest* Test)
    {
        // --- Setup ---
        // Use contexts with unique names to avoid potential world/subsystem conflicts if run in parallel later
        FItemContainerTestContext ContextA(10, 50); ContextA.TempActor->Rename(TEXT("ActorA"));
        FItemContainerTestContext ContextB(10, 50); ContextB.TempActor->Rename(TEXT("ActorB"));
        FItemContainerTestContext ContextC(10, 50); ContextC.TempActor->Rename(TEXT("ActorC"));
        auto* Subsystem = ContextA.TestFixture.GetSubsystem();
        FDebugTestResult Res = true;

        const int32 BackpackDefaultSlots = 5;
        const float BackpackDefaultWeight = 10.0f;
        const int32 PurseDefaultSlots = 2;
        const float PurseDefaultWeight = 1.0f;
        const float KnifeDurability = 88.f;

        // --- Test Steps ---

        // 1. Add Backpack to Container A
        int32 Added = ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBackpack, 1, false);
        Res &= Test->TestEqual(TEXT("[Recursive] 1. Add Backpack: Added Quantity"), Added, 1);
        FItemBundleWithInstanceData BackpackBundleA = ContextA.ItemContainerComponent->FindItemById(ItemIdBackpack);
        Res &= Test->TestTrue(TEXT("[Recursive] 1. Add Backpack: Bundle Valid"), BackpackBundleA.IsValid());

        // 2. Verify Instance Data and Sub-Component Creation (A)
        URecursiveContainerInstanceData* BackpackInstanceA = GetRecursiveInstanceData(BackpackBundleA);
        Res &= Test->TestNotNull(TEXT("[Recursive] 2. Verify Creation (A): Instance Data Exists"), BackpackInstanceA);
        if (!BackpackInstanceA) return false; // Stop test if instance data failed

        // Manually set capacity (since Initialize doesn't read static data yet)
        BackpackInstanceA->MaxSlotCount = BackpackDefaultSlots;
        BackpackInstanceA->MaxWeight = BackpackDefaultWeight;
        // Call initialize manually - This should ideally happen automatically when the instance is created/added
        // For now, we simulate it post-addition. You might need to adjust UItemContainerComponent::AddItem_ServerImpl
        // or the FItemBundleWithInstanceData constructor to call Initialize if appropriate.
        BackpackInstanceA->Initialize(true, nullptr, ContextA.ItemContainerComponent);

        UItemContainerComponent* SubContainerA = BackpackInstanceA->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Recursive] 2. Verify Creation (A): Sub-Component Exists"), SubContainerA);
        if (!SubContainerA) return false; // Stop test if sub-component failed

        Res &= Test->TestEqual(TEXT("[Recursive] 2. Verify Creation (A): Sub-Component Owner"), SubContainerA->GetOwner(), ContextA.TempActor);
        Res &= Test->TestTrue(TEXT("[Recursive] 2. Verify Creation (A): Sub-Component Registered"), SubContainerA->IsRegistered());
        // Initial capacity might not be set by Initialize yet, check based on manual set above
        Res &= Test->TestEqual(TEXT("[Recursive] 2. Verify Creation (A): Sub-Component Max Slots"), SubContainerA->MaxSlotCount, BackpackDefaultSlots);
        Res &= Test->TestEqual(TEXT("[Recursive] 2. Verify Creation (A): Sub-Component Max Weight"), SubContainerA->MaxWeight, BackpackDefaultWeight);
        Res &= Test->TestTrue(TEXT("[Recursive] 2. Verify Creation (A): Instance points to Sub-Component"), BackpackInstanceA->RepresentedContainer == SubContainerA);
        Res &= Test->TestTrue(TEXT("[Recursive] 2. Verify Creation (A): Backpack Instance Registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(BackpackInstanceA));


        // 3. Add Items INSIDE Backpack (Sub-Container A)
        Added = SubContainerA->AddItem_IfServer(Subsystem, ItemIdRock, 3, false); // Add 3 Rocks (Weight 3)
        Res &= Test->TestEqual(TEXT("[Recursive] 3. Add Inside (A): Rocks Added Quantity"), Added, 3);
        Added = SubContainerA->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 1, false); // Add 1 Knife (Weight 3)
        Res &= Test->TestEqual(TEXT("[Recursive] 3. Add Inside (A): Knife Added Quantity"), Added, 1);

        // 4. Verify Contents of Sub-Container A
        Res &= Test->TestEqual(TEXT("[Recursive] 4. Verify Contents (A): Rock Quantity"), SubContainerA->GetContainedQuantity(ItemIdRock), 3);
        Res &= Test->TestEqual(TEXT("[Recursive] 4. Verify Contents (A): Knife Quantity"), SubContainerA->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
        Res &= Test->TestEqual(TEXT("[Recursive] 4. Verify Contents (A): Sub-Container Weight"), SubContainerA->GetCurrentWeight(), 6.0f); // 3*1 + 3*1
        Res &= Test->TestFalse(TEXT("[Recursive] 4. Verify Contents (A): Primary Container has Rocks"), ContextA.ItemContainerComponent->Contains(ItemIdRock));
        Res &= Test->TestFalse(TEXT("[Recursive] 4. Verify Contents (A): Primary Container has Knife"), ContextA.ItemContainerComponent->Contains(ItemIdBrittleCopperKnife));

        // Set and Verify Knife Instance Data (Inside Backpack A)
        FItemBundleWithInstanceData KnifeBundleA = SubContainerA->FindItemById(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeInstanceA = KnifeBundleA.InstanceData.Num() > 0 ? Cast<UItemDurabilityTestInstanceData>(KnifeBundleA.InstanceData[0]) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Recursive] 4. Verify Contents (A): Knife Instance Exists"), KnifeInstanceA);
        if (KnifeInstanceA)
        {
            KnifeInstanceA->Durability = KnifeDurability;
            Res &= Test->TestTrue(TEXT("[Recursive] 4. Verify Contents (A): Knife Instance Registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(KnifeInstanceA));
        }

        // 5. Transfer Backpack from Container A to Container B
        UItemContainerComponent* PtrSubContainerA_BeforeTransfer = SubContainerA; // Keep pointer
        URecursiveContainerInstanceData* PtrInstanceA_BeforeTransfer = BackpackInstanceA;
        UItemDurabilityTestInstanceData* PtrKnifeInstanceA_BeforeTransfer = KnifeInstanceA;

        int32 Transferred = ContextB.ItemContainerComponent->ExtractItemFromContainer_IfServer(ItemIdBackpack, 1, ContextA.ItemContainerComponent, false);
        Res &= Test->TestEqual(TEXT("[Recursive] 5. Transfer A->B: Transferred Quantity"), Transferred, 1);

        // 6. Verify State After Transfer (A->B)
        // Container A Checks
        Res &= Test->TestFalse(TEXT("[Recursive] 6. Verify Post-Transfer (A): Backpack Exists"), ContextA.ItemContainerComponent->Contains(ItemIdBackpack));
        // Check if component was destroyed - might take a frame, check registration and owner
        TArray<UItemContainerComponent*> ComponentsOnA = FindAllComponents<UItemContainerComponent>(ContextA.TempActor);
        bool OldSubStillOnA = ComponentsOnA.Contains(PtrSubContainerA_BeforeTransfer);
        Res &= Test->TestFalse(TEXT("[Recursive] 6. Verify Post-Transfer (A): Old Sub-Component Still on Actor A"), OldSubStillOnA);
        Res &= Test->TestFalse(TEXT("[Recursive] 6. Verify Post-Transfer (A): Old Backpack Instance Registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(PtrInstanceA_BeforeTransfer));
        if (PtrKnifeInstanceA_BeforeTransfer)
            Res &= Test->TestFalse(TEXT("[Recursive] 6. Verify Post-Transfer (A): Old Knife Instance Registered"), ContextA.TempActor->IsReplicatedSubObjectRegistered(PtrKnifeInstanceA_BeforeTransfer));


        // Container B Checks
        FItemBundleWithInstanceData BackpackBundleB = ContextB.ItemContainerComponent->FindItemById(ItemIdBackpack);
        Res &= Test->TestTrue(TEXT("[Recursive] 6. Verify Post-Transfer (B): Backpack Exists"), BackpackBundleB.IsValid());
        URecursiveContainerInstanceData* BackpackInstanceB = GetRecursiveInstanceData(BackpackBundleB);
        Res &= Test->TestNotNull(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Backpack Instance Exists"), BackpackInstanceB);
        Res &= Test->TestTrue(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Backpack Instance Registered"), ContextB.TempActor->IsReplicatedSubObjectRegistered(BackpackInstanceB));
        if (!BackpackInstanceB) return false;

        // Manually call Initialize again - this reflects the need for the transfer logic to call it.
        BackpackInstanceB->Initialize(true, nullptr, ContextB.ItemContainerComponent);

        UItemContainerComponent* SubContainerB = BackpackInstanceB->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Component Exists"), SubContainerB);
        if (!SubContainerB) return false;

        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Component Owner"), SubContainerB->GetOwner(), ContextB.TempActor);
        Res &= Test->TestTrue(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Component Registered"), SubContainerB->IsRegistered());
        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Component Max Slots"), SubContainerB->MaxSlotCount, BackpackDefaultSlots);
        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Component Max Weight"), SubContainerB->MaxWeight, BackpackDefaultWeight);
        Res &= Test->TestTrue(TEXT("[Recursive] 6. Verify Post-Transfer (B): Instance points to New Sub-Component"), BackpackInstanceB->RepresentedContainer == SubContainerB);

        // Verify Contents Transferred to SubContainerB
        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): Rock Quantity in New Sub"), SubContainerB->GetContainedQuantity(ItemIdRock), 3);
        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): Knife Quantity in New Sub"), SubContainerB->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
        Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): New Sub-Container Weight"), SubContainerB->GetCurrentWeight(), 6.0f);

        // Verify Knife Instance Data Transferred
        FItemBundleWithInstanceData KnifeBundleB = SubContainerB->FindItemById(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeInstanceB = KnifeBundleB.InstanceData.Num() > 0 ? Cast<UItemDurabilityTestInstanceData>(KnifeBundleB.InstanceData[0]) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Recursive] 6. Verify Post-Transfer (B): Knife Instance Exists in New Sub"), KnifeInstanceB);
        if (KnifeInstanceB)
        {
            Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): Knife Durability Preserved"), KnifeInstanceB->Durability, KnifeDurability);
            Res &= Test->TestTrue(TEXT("[Recursive] 6. Verify Post-Transfer (B): Knife Instance Registered on Actor B"), ContextB.TempActor->IsReplicatedSubObjectRegistered(KnifeInstanceB));
            Res &= Test->TestEqual(TEXT("[Recursive] 6. Verify Post-Transfer (B): Knife Instance Pointer Same"), KnifeInstanceB, PtrKnifeInstanceA_BeforeTransfer); // Pointer check
        }

        // 7. Drop Backpack from Container B
        UItemContainerComponent* PtrSubContainerB_BeforeDrop = SubContainerB;
        URecursiveContainerInstanceData* PtrInstanceB_BeforeDrop = BackpackInstanceB;
        UItemDurabilityTestInstanceData* PtrKnifeInstanceB_BeforeDrop = KnifeInstanceB;

        int32 Dropped = ContextB.ItemContainerComponent->DropItems(ItemIdBackpack, 1);
        Res &= Test->TestEqual(TEXT("[Recursive] 7. Drop B->World: Dropped Quantity"), Dropped, 1);

        // 8. Verify State After Drop (B->World)
        // Container B Checks
        Res &= Test->TestFalse(TEXT("[Recursive] 8. Verify Post-Drop (B): Backpack Exists"), ContextB.ItemContainerComponent->Contains(ItemIdBackpack));
        TArray<UItemContainerComponent*> ComponentsOnB = FindAllComponents<UItemContainerComponent>(ContextB.TempActor);
        bool OldSubStillOnB = ComponentsOnB.Contains(PtrSubContainerB_BeforeDrop);
        Res &= Test->TestFalse(TEXT("[Recursive] 8. Verify Post-Drop (B): Old Sub-Component Still on Actor B"), OldSubStillOnB);
        Res &= Test->TestFalse(TEXT("[Recursive] 8. Verify Post-Drop (B): Old Backpack Instance Registered"), ContextB.TempActor->IsReplicatedSubObjectRegistered(PtrInstanceB_BeforeDrop));
        if (PtrKnifeInstanceB_BeforeDrop)
            Res &= Test->TestFalse(TEXT("[Recursive] 8. Verify Post-Drop (B): Old Knife Instance Registered"), ContextB.TempActor->IsReplicatedSubObjectRegistered(PtrKnifeInstanceB_BeforeDrop));

        // WorldItem Checks
        AWorldItem* DroppedWorldItem = nullptr;
        UWorld* World = ContextB.TestFixture.GetWorld(); // Use any context's world
        for (TActorIterator<AWorldItem> It(World); It; ++It)
        {
            if (It->RepresentedItem.ItemId == ItemIdBackpack)
            {
                DroppedWorldItem = *It;
                break;
            }
        }
        Res &= Test->TestNotNull(TEXT("[Recursive] 8. Verify Post-Drop (World): WorldItem Found"), DroppedWorldItem);
        if (!DroppedWorldItem) return false;

        FItemBundleWithInstanceData BackpackBundleWorld = DroppedWorldItem->RepresentedItem;
        Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Backpack Bundle Valid"), BackpackBundleWorld.IsValid());
        URecursiveContainerInstanceData* BackpackInstanceWorld = GetRecursiveInstanceData(BackpackBundleWorld);
        Res &= Test->TestNotNull(TEXT("[Recursive] 8. Verify Post-Drop (World): Backpack Instance Exists"), BackpackInstanceWorld);
        Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Backpack Instance Registered on WorldItem"), DroppedWorldItem->IsReplicatedSubObjectRegistered(BackpackInstanceWorld));
        if (!BackpackInstanceWorld) return false;

        // Manually call Initialize for WorldItem
        BackpackInstanceWorld->Initialize(false, DroppedWorldItem, nullptr);

        UItemContainerComponent* SubContainerWorld = BackpackInstanceWorld->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Recursive] 8. Verify Post-Drop (World): Sub-Component Exists"), SubContainerWorld);
        if (!SubContainerWorld) return false;

        Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Sub-Component Owner is WorldItem"), SubContainerWorld->GetOwner() == DroppedWorldItem);
        Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Sub-Component Registered"), SubContainerWorld->IsRegistered());
        Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Sub-Component Max Slots"), SubContainerWorld->MaxSlotCount, BackpackDefaultSlots);
        Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Sub-Component Max Weight"), SubContainerWorld->MaxWeight, BackpackDefaultWeight);
        Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Instance points to Sub-Component"), BackpackInstanceWorld->RepresentedContainer == SubContainerWorld);

        // Verify Contents in WorldItem's SubContainer
        Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Rock Quantity in Sub"), SubContainerWorld->GetContainedQuantity(ItemIdRock), 3);
        Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Knife Quantity in Sub"), SubContainerWorld->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
        FItemBundleWithInstanceData KnifeBundleWorld = SubContainerWorld->FindItemById(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeInstanceWorld = KnifeBundleWorld.InstanceData.Num() > 0 ? Cast<UItemDurabilityTestInstanceData>(KnifeBundleWorld.InstanceData[0]) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Recursive] 8. Verify Post-Drop (World): Knife Instance Exists in Sub"), KnifeInstanceWorld);
        if (KnifeInstanceWorld)
        {
            Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Knife Durability Preserved"), KnifeInstanceWorld->Durability, KnifeDurability);
            Res &= Test->TestTrue(TEXT("[Recursive] 8. Verify Post-Drop (World): Knife Instance Registered on WorldItem"), DroppedWorldItem->IsReplicatedSubObjectRegistered(KnifeInstanceWorld));
            Res &= Test->TestEqual(TEXT("[Recursive] 8. Verify Post-Drop (World): Knife Instance Pointer Same"), KnifeInstanceWorld, PtrKnifeInstanceB_BeforeDrop); // Pointer check
        }

        // 9. Pick Up Backpack from World into Container C
        UItemContainerComponent* PtrSubContainerWorld_BeforePickup = SubContainerWorld;
        URecursiveContainerInstanceData* PtrInstanceWorld_BeforePickup = BackpackInstanceWorld;
        UItemDurabilityTestInstanceData* PtrKnifeInstanceWorld_BeforePickup = KnifeInstanceWorld;

        Added = ContextC.ItemContainerComponent->AddItem_IfServer(DroppedWorldItem, ItemIdBackpack, 1, false);
        Res &= Test->TestEqual(TEXT("[Recursive] 9. Pickup World->C: Added Quantity"), Added, 1);

        // 10. Verify State After Pickup (World->C)
        // WorldItem Checks
        if (DroppedWorldItem) // Should still be valid but potentially empty/pending kill
        {
             Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (World): Item Quantity"), DroppedWorldItem->GetContainedQuantity(ItemIdBackpack), 0);
             TArray<UItemContainerComponent*> ComponentsOnWorld = FindAllComponents<UItemContainerComponent>(DroppedWorldItem);
             bool OldSubStillOnWorld = ComponentsOnWorld.Contains(PtrSubContainerWorld_BeforePickup);
             Res &= Test->TestFalse(TEXT("[Recursive] 10. Verify Post-Pickup (World): Old Sub-Component Still on WorldItem"), OldSubStillOnWorld);
             Res &= Test->TestFalse(TEXT("[Recursive] 10. Verify Post-Pickup (World): Old Backpack Instance Registered"), DroppedWorldItem->IsReplicatedSubObjectRegistered(PtrInstanceWorld_BeforePickup));
             if (PtrKnifeInstanceWorld_BeforePickup)
                  Res &= Test->TestFalse(TEXT("[Recursive] 10. Verify Post-Pickup (World): Old Knife Instance Registered"), DroppedWorldItem->IsReplicatedSubObjectRegistered(PtrKnifeInstanceWorld_BeforePickup));
             DroppedWorldItem->Destroy(); // Clean up the actor
        }

        // Container C Checks
        FItemBundleWithInstanceData BackpackBundleC = ContextC.ItemContainerComponent->FindItemById(ItemIdBackpack);
        Res &= Test->TestTrue(TEXT("[Recursive] 10. Verify Post-Pickup (C): Backpack Exists"), BackpackBundleC.IsValid());
        URecursiveContainerInstanceData* BackpackInstanceC = GetRecursiveInstanceData(BackpackBundleC);
        Res &= Test->TestNotNull(TEXT("[Recursive] 10. Verify Post-Pickup (C): New Backpack Instance Exists"), BackpackInstanceC);
        Res &= Test->TestTrue(TEXT("[Recursive] 10. Verify Post-Pickup (C): New Backpack Instance Registered"), ContextC.TempActor->IsReplicatedSubObjectRegistered(BackpackInstanceC));
        if(!BackpackInstanceC) return false;

        // Manual Initialize
        BackpackInstanceC->Initialize(true, nullptr, ContextC.ItemContainerComponent);

        UItemContainerComponent* SubContainerC = BackpackInstanceC->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Recursive] 10. Verify Post-Pickup (C): New Sub-Component Exists"), SubContainerC);
         if(!SubContainerC) return false;

        Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (C): New Sub-Component Owner"), SubContainerC->GetOwner(), ContextC.TempActor);
        Res &= Test->TestTrue(TEXT("[Recursive] 10. Verify Post-Pickup (C): New Sub-Component Registered"), SubContainerC->IsRegistered());
        Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (C): Rock Quantity in New Sub"), SubContainerC->GetContainedQuantity(ItemIdRock), 3);
        Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (C): Knife Quantity in New Sub"), SubContainerC->GetContainedQuantity(ItemIdBrittleCopperKnife), 1);
        FItemBundleWithInstanceData KnifeBundleC = SubContainerC->FindItemById(ItemIdBrittleCopperKnife);
        UItemDurabilityTestInstanceData* KnifeInstanceC = KnifeBundleC.InstanceData.Num() > 0 ? Cast<UItemDurabilityTestInstanceData>(KnifeBundleC.InstanceData[0]) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Recursive] 10. Verify Post-Pickup (C): Knife Instance Exists in New Sub"), KnifeInstanceC);
        if (KnifeInstanceC)
        {
            Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (C): Knife Durability Preserved"), KnifeInstanceC->Durability, KnifeDurability);
            Res &= Test->TestTrue(TEXT("[Recursive] 10. Verify Post-Pickup (C): Knife Instance Registered on Actor C"), ContextC.TempActor->IsReplicatedSubObjectRegistered(KnifeInstanceC));
            Res &= Test->TestEqual(TEXT("[Recursive] 10. Verify Post-Pickup (C): Knife Instance Pointer Same"), KnifeInstanceC, PtrKnifeInstanceWorld_BeforePickup); // Pointer check
        }

        // 11. Nested Test: Backpack in A -> Add Purse to Backpack -> Add Rocks to Purse -> Transfer Backpack A->B
        ContextA.ItemContainerComponent->Clear_IfServer(); // Reset A & B
        ContextB.ItemContainerComponent->Clear_IfServer();

        // Add backpack to A, initialize
        ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBackpack, 1, false);
        BackpackBundleA = ContextA.ItemContainerComponent->FindItemById(ItemIdBackpack);
        BackpackInstanceA = GetRecursiveInstanceData(BackpackBundleA);
        BackpackInstanceA->MaxSlotCount = BackpackDefaultSlots; BackpackInstanceA->MaxWeight = BackpackDefaultWeight;
        BackpackInstanceA->Initialize(true, nullptr, ContextA.ItemContainerComponent);
        SubContainerA = BackpackInstanceA->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Setup: SubContainerA valid"), SubContainerA);
        if (!SubContainerA) return false;

        // Add Coin Purse INSIDE Backpack A
        Added = SubContainerA->AddItem_IfServer(Subsystem, ItemIdCoinPurse, 1, false);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Setup: Added Purse to Backpack"), Added, 1);
        FItemBundleWithInstanceData PurseBundleA = SubContainerA->FindItemById(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Nested] 11. Setup: Purse Bundle valid"), PurseBundleA.IsValid());
        URecursiveContainerInstanceData* PurseInstanceA = GetRecursiveInstanceData(PurseBundleA);
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Setup: Purse Instance valid"), PurseInstanceA);
        if (!PurseInstanceA) return false;

        // Initialize Purse Instance Data
        PurseInstanceA->MaxSlotCount = PurseDefaultSlots; PurseInstanceA->MaxWeight = PurseDefaultWeight;
        // Initialize needs owner info. The 'OwningContainer' for the purse instance is SubContainerA
        PurseInstanceA->Initialize(true, nullptr, SubContainerA);
        UItemContainerComponent* SubContainerPurseA = PurseInstanceA->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Setup: SubContainerPurseA valid"), SubContainerPurseA);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Setup: SubContainerPurseA owner"), SubContainerPurseA->GetOwner(), ContextA.TempActor); // Still owned by Actor A
        if (!SubContainerPurseA) return false;

        // Add Rocks INSIDE Coin Purse A
        Added = SubContainerPurseA->AddItem_IfServer(Subsystem, ItemIdRock, 1, false); // 1 Rock (Weight 1)
        Res &= Test->TestEqual(TEXT("[Nested] 11. Setup: Added Rocks to Purse"), Added, 1);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Setup: Purse Content Quantity"), SubContainerPurseA->GetContainedQuantity(ItemIdRock), 1);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Setup: Purse Weight"), SubContainerPurseA->GetCurrentWeight(), 1.0f);
        Res &= Test->TestFalse(TEXT("[Nested] 11. Setup: Backpack contains Rocks"), SubContainerA->Contains(ItemIdRock));

        // Transfer Backpack (containing purse containing rocks) from A to B
        ContextB.ItemContainerComponent->ExtractItemFromContainer_IfServer(ItemIdBackpack, 1, ContextA.ItemContainerComponent, false);

        // Verify Nested Structure on B
        BackpackBundleB = ContextB.ItemContainerComponent->FindItemById(ItemIdBackpack);
        BackpackInstanceB = GetRecursiveInstanceData(BackpackBundleB);
         if (!BackpackInstanceB) return false;
        BackpackInstanceB->Initialize(true, nullptr, ContextB.ItemContainerComponent); // Manually initialize
        SubContainerB = BackpackInstanceB->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Verify Transfer: SubContainerB valid"), SubContainerB);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Verify Transfer: SubContainerB owner"), SubContainerB->GetOwner(), ContextB.TempActor);
        if (!SubContainerB) return false;

        // Verify Purse exists inside Backpack B
        FItemBundleWithInstanceData PurseBundleB = SubContainerB->FindItemById(ItemIdCoinPurse);
        Res &= Test->TestTrue(TEXT("[Nested] 11. Verify Transfer: Purse Bundle valid in B"), PurseBundleB.IsValid());
        URecursiveContainerInstanceData* PurseInstanceB = GetRecursiveInstanceData(PurseBundleB);
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Verify Transfer: Purse Instance valid in B"), PurseInstanceB);
        if (!PurseInstanceB) return false;

        // Manually initialize Purse on B
        PurseInstanceB->Initialize(true, nullptr, SubContainerB);
        UItemContainerComponent* SubContainerPurseB = PurseInstanceB->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Nested] 11. Verify Transfer: SubContainerPurseB valid"), SubContainerPurseB);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Verify Transfer: SubContainerPurseB owner"), SubContainerPurseB->GetOwner(), ContextB.TempActor); // Now owned by Actor B
        if (!SubContainerPurseB) return false;

        // Verify Rocks exist inside Purse B
        Res &= Test->TestEqual(TEXT("[Nested] 11. Verify Transfer: Rock Quantity in Purse B"), SubContainerPurseB->GetContainedQuantity(ItemIdRock), 1);
        Res &= Test->TestEqual(TEXT("[Nested] 11. Verify Transfer: Purse B Weight"), SubContainerPurseB->GetCurrentWeight(), 1.0f);
        Res &= Test->TestFalse(TEXT("[Nested] 11. Verify Transfer: Backpack B contains Rocks"), SubContainerB->Contains(ItemIdRock));

        // 12. Destruction Test
        ContextA.ItemContainerComponent->Clear_IfServer(); // Reset A
        // Add backpack to A, initialize
        ContextA.ItemContainerComponent->AddItem_IfServer(Subsystem, ItemIdBackpack, 1, false);
        BackpackBundleA = ContextA.ItemContainerComponent->FindItemById(ItemIdBackpack);
        BackpackInstanceA = GetRecursiveInstanceData(BackpackBundleA);
        BackpackInstanceA->MaxSlotCount = BackpackDefaultSlots; BackpackInstanceA->MaxWeight = BackpackDefaultWeight;
        BackpackInstanceA->Initialize(true, nullptr, ContextA.ItemContainerComponent);
        SubContainerA = BackpackInstanceA->RepresentedContainer;
        Res &= Test->TestNotNull(TEXT("[Destroy] 12. Setup: SubContainerA valid"), SubContainerA);
        if (!SubContainerA) return false;
        UItemContainerComponent* PtrSubContainerA_BeforeDestroy = SubContainerA;

        // Destroy the backpack item
        int32 Destroyed = ContextA.ItemContainerComponent->DestroyItem_IfServer(ItemIdBackpack, 1, EItemChangeReason::Removed, true);
        Res &= Test->TestEqual(TEXT("[Destroy] 12. Verify: Destroyed Quantity"), Destroyed, 1);
        Res &= Test->TestFalse(TEXT("[Destroy] 12. Verify: Backpack Exists"), ContextA.ItemContainerComponent->Contains(ItemIdBackpack));

        // Verify the sub-component associated with the backpack is gone
        TArray<UItemContainerComponent*> ComponentsOnA_AfterDestroy = FindAllComponents<UItemContainerComponent>(ContextA.TempActor);
        bool SubStillOnA_AfterDestroy = ComponentsOnA_AfterDestroy.Contains(PtrSubContainerA_BeforeDestroy);
        // Note: Destruction might be deferred, check if it's unregistered or !IsValid
        Res &= Test->TestFalse(TEXT("[Destroy] 12. Verify: Old Sub-Component Still on Actor A after destroy"), SubStillOnA_AfterDestroy);
         // Ideally check PtrSubContainerA_BeforeDestroy->IsPendingKill() or similar if needed


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
	
	Res &= FItemContainerTestScenarios::TestInstanceDataTransferBetweenContainers(this);
	Res &= FItemContainerTestScenarios::TestInstanceDataDropPickupAndDestruction(this);
    Res &= FItemContainerTestScenarios::TestRecursiveContainerLifecycle(this);
	return Res;
}
