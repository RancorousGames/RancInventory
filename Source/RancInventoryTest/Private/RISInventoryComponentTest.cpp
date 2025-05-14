// Copyright Rancorous Games, 2024

#include "InventoryEventListener.h"
#include "NativeGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "RISInventoryTestSetup.cpp"
#include "Components/InventoryComponent.h"
#include "Data/RecipeData.h"
#include "Framework/DebugTestResult.h"
#include "MockClasses/ItemHoldingCharacter.h"

#define TestName "GameTests.RIS.2_InventoryComponent"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRancInventoryComponentTest, TestName,
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

template<typename T>
T* GetInstanceDataFromBundle(const FItemBundle& Bundle, int32 Index = 0)
{
	if (Bundle.InstanceData.IsValidIndex(Index))
	{
		return Cast<T>(Bundle.InstanceData[Index]);
	}
	return nullptr;
}

template<typename T>
T* GetInstanceDataFromTaggedBundle(const FTaggedItemBundle& Bundle, int32 Index = 0)
{
	if (Bundle.InstanceData.IsValidIndex(Index))
	{
		return Cast<T>(Bundle.InstanceData[Index]);
	}
	return nullptr;
}

UItemInstanceData* FindInstanceById(const TArray<UItemInstanceData*>& Instances, int32 UniqueId)
{
	for (UItemInstanceData* Instance : Instances)
	{
		if (Instance && Instance->UniqueInstanceId == UniqueId)
		{
			return Instance;
		}
	}
	return nullptr;
}
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
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, ItemTypeTwoHanded));
		InventoryComponent->UniversalTaggedSlots.Add(FUniversalTaggedSlot(LeftHandSlot, RightHandSlot,ItemTypeTwoHandedOffhand, ItemTypeOffHandOnly));
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
    AItemHoldingCharacter* TempActor;
    UInventoryComponent* InventoryComponent;
};

// --- Test Scenarios ---
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

        Res &= Test->TestTrue(
            TEXT("No item should be in the left hand slot before addition"),
            !InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, false);
        Res &= Test->TestTrue(
            TEXT("Unstackable Item should be in the left hand slot after addition"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

        //InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true);
        //Res &= Test->TestTrue(
        //    TEXT("Second unstackable item should not replace the first one"),
        //    InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 1);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneSpear, true);
        Res &= Test->TestTrue(
            TEXT("Non-helmet item should not be added to the helmet slot"),
            !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
        Res &= Test->TestTrue(
            TEXT("Helmet item should be added to the helmet slot"),
            InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, false);
        Res &= Test->TestTrue(
            TEXT(
                "Different stackable item (Sticks) should replace  already contained stackable item (Rock)"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks));

		// Rock should have moved into the container
		Res &= Test->TestTrue(
			TEXT("Rock should be in the container after replacing it with sticks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);
		
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, FGameplayTag::EmptyTag, OneRock, false);
        Res &= Test->TestTrue(
            TEXT("Rock should be in the container after kicking it out of tagged slot"),
            InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, FiveSticks, true);
        int32 AmountAdded = InventoryComponent->
            AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeSticks, true);
        Res &= Test->TestEqual(
            TEXT("Stackable Item (Sticks) amount added should be none as already full stack"), AmountAdded, 0);
        Res &= Test->TestEqual(
            TEXT("Right hand slot should have 5 Stick"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity, 5);

        int32 QuantityRemoved = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 4, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        Res &= Test->TestEqual(
            TEXT("Should have removed 4 sticks"), QuantityRemoved, 4);
		
        // Instance Data Tests
        InventoryComponent->Clear_IfServer();
        AmountAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        Res &= Test->TestEqual(TEXT("[Instance] Added 1 knife to RightHandSlot"), AmountAdded, 1);
        FTaggedItemBundle KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Tagged slot should have 1 instance data entry"), KnifeBundle.InstanceData.Num(), 1);
        const FItemBundle* ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        Res &= Test->TestNotNull(TEXT("[Instance] Knife should exist in main container"), ContainerKnifeBundle);
        if (KnifeBundle.InstanceData.Num() == 1 && ContainerKnifeBundle && ContainerKnifeBundle->InstanceData.Num() == 1)
        {
            Res &= Test->TestEqual(TEXT("[Instance] Tagged instance pointer should match container instance pointer"), KnifeBundle.InstanceData[0], ContainerKnifeBundle->InstanceData[0]);
            Res &= Test->TestTrue(TEXT("[Instance] Tagged instance should be registered"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeBundle.InstanceData[0]));
        }

        return Res;
    }

    bool TestRemovingTaggedSlotItems() const
    {
        InventoryComponentTestContext Context(100);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, false);

        int32 RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
            RightHandSlot, 2, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        Res &= Test->TestTrue(
            TEXT("Should successfully remove a portion of the stackable item (Rock)"), RemovedQuantity == 2);
        Res &= Test->TestTrue(
            TEXT("Right hand slot should have 1 Rock remaining after partial removal"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 1);

        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
            RightHandSlot, 2, FItemBundle::NoInstances, EItemChangeReason::Removed, false);
        Res &= Test->TestTrue(
            TEXT(
                "Should not remove any items if attempting to remove more than present without allowing partial removal"),
            RemovedQuantity == 0);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
            HelmetSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        Res &= Test->TestTrue(TEXT("Should successfully remove unstackable item (Helmet)"), RemovedQuantity == 1);
        Res &= Test->TestFalse(
            TEXT("Helmet slot should be empty after removing the item"),
            InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
            LeftHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        Res &= Test->TestTrue(TEXT("Should not remove any items from an empty slot"), RemovedQuantity == 0);

        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(
            FGameplayTag::EmptyTag, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        Res &= Test->TestTrue(TEXT("Should not remove any items from a non-existent slot"), RemovedQuantity == 0);

        // Instance Data Tests
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        FTaggedItemBundle KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstancePtr = KnifeBundle.InstanceData.Num() > 0 ? KnifeBundle.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Knife instance pointer valid before removal"), KnifeInstancePtr);

        // Remove with DestroyFromContainer = true
        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true, true); // Destroy=true
        Res &= Test->TestEqual(TEXT("[Instance] Removed 1 knife"), RemovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[Instance] Tagged slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[Instance] Container should be empty"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        if (KnifeInstancePtr)
        {
            Res &= Test->TestFalse(TEXT("[Instance] Instance should be unregistered after destroy"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
            // Ideally check IsPendingKill here, but that might require ticking the world
        }

        // Re-add and remove without destroying
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        KnifeInstancePtr = KnifeBundle.InstanceData.Num() > 0 ? KnifeBundle.InstanceData[0] : nullptr;

        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Moved, true, false); // Destroy=false
        Res &= Test->TestEqual(TEXT("[Instance] Removed 1 knife (no destroy)"), RemovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[Instance] Tagged slot should be empty (no destroy)"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("[Instance] Container should still have the knife"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        const FItemBundle* ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Container bundle should have 1 instance"), ContainerKnifeBundle ? ContainerKnifeBundle->InstanceData.Num() : 0, 1);
        if (KnifeInstancePtr)
        {
            Res &= Test->TestTrue(TEXT("[Instance] Instance should still be registered after unequip"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
        }

        // Add multiple instances and remove specific one
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 2, true); // Add 2 to container
        InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, NoTag, RightHandSlot); // Move one to slot
        InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, NoTag, LeftHandSlot);  // Move other to slot
        FTaggedItemBundle RightKnife = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        FTaggedItemBundle LeftKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Right hand has 1 instance"), RightKnife.InstanceData.Num(), 1);
        Res &= Test->TestEqual(TEXT("[Instance] Left hand has 1 instance"), LeftKnife.InstanceData.Num(), 1);
        UItemInstanceData* RightInstancePtr = RightKnife.InstanceData[0];
        UItemInstanceData* LeftInstancePtr = LeftKnife.InstanceData[0];

        TArray<UItemInstanceData*> InstancesToRemove = { RightInstancePtr };
        RemovedQuantity = InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, InstancesToRemove, EItemChangeReason::Removed, false, true); // Destroy=true, AllowPartial=false (required for specific instances)
        Res &= Test->TestEqual(TEXT("[Instance] Removed specific right hand knife instance"), RemovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[Instance] Right hand slot empty after specific removal"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("[Instance] Left hand slot still has knife"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[Instance] Removed specific instance should be unregistered"), Context.TempActor->IsReplicatedSubObjectRegistered(RightInstancePtr));
        Res &= Test->TestTrue(TEXT("[Instance] Other instance should still be registered"), Context.TempActor->IsReplicatedSubObjectRegistered(LeftInstancePtr));


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
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
        Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Right hand should have 3 rocks before clear"),
                              InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic inventory should be empty before clear"),
                               InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 0);

        int32 MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(RightHandSlot);
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Should return 3 as the moved quantity"), MovedQuantity, 3);
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Right hand slot should be empty after clear"),
                               InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic inventory should now have 3 rocks"),
                               InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 3);
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
        InventoryComponent->Clear_IfServer();
        Listener->Clear();
        InventoryComponent->MaxWeight = 100;
        InventoryComponent->MaxSlotCount = 9;
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear, true);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Left hand should be blocked (with item)"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

        MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(RightHandSlot);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Clearing a blocked slot (with item) should succeed"), MovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Left hand slot should be empty after clearing blocked slot"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem] Right hand slot should be empty after clearing blocked slot"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem] Generic should have the spear after clearing blocked slot"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear), 1);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Remove event should fire for clearing blocked"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem] Add event should fire for clearing blocked"), Listener->bItemAddedTriggered);

        // --- Test Case 5: Move Item with Instance Data ---
        InventoryComponent->Clear_IfServer();
        Listener->Clear();
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        FTaggedItemBundle KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstancePtr = KnifeBundle.InstanceData.Num() > 0 ? KnifeBundle.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[RemoveAnyItem][Instance] Knife instance created"), KnifeInstancePtr);

        MovedQuantity = InventoryComponent->RemoveAnyItemFromTaggedSlot_IfServer(RightHandSlot);
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Should move 1 knife"), MovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[RemoveAnyItem][Instance] Right hand slot should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Generic inventory should have 1 knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 1);
        const FItemBundle* ContainerKnife = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Container knife instance count"), ContainerKnife ? ContainerKnife->InstanceData.Num() : 0, 1);
        if (ContainerKnife && ContainerKnife->InstanceData.Num() == 1)
        {
            Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Container instance pointer should match original"), ContainerKnife->InstanceData[0], KnifeInstancePtr);
            Res &= Test->TestTrue(TEXT("[RemoveAnyItem][Instance] Instance should still be registered"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
        }
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem][Instance] Remove event should fire"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Removed event instance count"), Listener->RemovedFromTaggedInstances.Num(), 1);
        if (Listener->RemovedFromTaggedInstances.Num() == 1)
        {
            Res &= Test->TestTrue(TEXT("[RemoveAnyItem][Instance] Removed event instance pointer check"), Listener->RemovedFromTaggedInstances[0] == KnifeInstancePtr);
        }
		Res &= Test->TestTrue(TEXT("[RemoveAnyItem][Instance] Add event should fire"), Listener->bItemAddedTriggered);
        Res &= Test->TestEqual(TEXT("[RemoveAnyItem][Instance] Added event instance count"), Listener->AddedInstances.Num(), 1);
        if (Listener->AddedInstances.Num() == 1)
        {
             Res &= Test->TestTrue(TEXT("[RemoveAnyItem][Instance] Added event instance pointer check"), Listener->AddedInstances[0] == KnifeInstancePtr);
        }

        return Res;
    }

    bool TestMoveTaggedSlotItems()
    {
        InventoryComponentTestContext Context(100);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Helmet item should be added to the helmet slot"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));
		Res &= Test->TestTrue(
			TEXT("Container should be empty"), InventoryComponent->GetQuantityTotal_Implementation(HelmetSlot) == 0);

        int32 SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, HelmetSlot, FGameplayTag::EmptyTag, FGameplayTag(), 0);
        Res &= Test->TestEqual(
            TEXT("Should simulate moving the helmet item from the helmet slot to generic inventory"), SimulatedMoveQuantity, 1);
        int32 MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should move the helmet item from the tagged slot to generic inventory"), MovedQuantity, 1);
        Res &= Test->TestFalse( // Changed from TestTrue(!...)
            TEXT("Helmet slot should be empty"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
        Res &= Test->TestEqual( // Changed from GetQuantityTotal
            TEXT("Generic inventory should contain the helmet item"),
            InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet), 1);

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, FGameplayTag::EmptyTag, ChestSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate not moving the helmet item to chest slot"), SimulatedMoveQuantity, 0); // Corrected expected value
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, FGameplayTag::EmptyTag, ChestSlot);
        Res &= Test->TestEqual(TEXT("Should not move the helmet item to the chest slot"), MovedQuantity, 0);
        Res &= Test->TestFalse(
            TEXT("Chest slot should not contain the helmet item"),
            InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate moving the helmet item from generic inventory to right hand slot"), SimulatedMoveQuantity, 1);
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should move the helmet item from generic inventory to right hand slot"), MovedQuantity, 1);
        Res &= Test->TestTrue(
            TEXT("Right hand slot should now contain the helmet item"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate moving the helmet item from right hand slot to left hand slot"), SimulatedMoveQuantity, 1);
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should move the helmet item from right hand slot to left hand slot"), MovedQuantity, 1);
        Res &= Test->TestTrue(
            TEXT("Left hand slot should now contain the helmet item"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdHelmet));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate not moving an item that doesn't exist"), SimulatedMoveQuantity, 0); // Corrected expected value
        MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should not move an item that doesn't exist in the source tagged slot"), MovedQuantity, 0);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock, true);
        Res &= Test->TestTrue(
            TEXT("Rock item should be added to the right hand slot"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, RightHandSlot, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate not moving the rock item to helmet slot"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, RightHandSlot, HelmetSlot);
        Res &= Test->TestEqual(TEXT("Should not move the rock item to helmet slot"), MovedQuantity, 0);
        Res &= Test->TestFalse( // Changed from TestTrue(!...)
            TEXT("Helmet slot should still be empty"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, HelmetSlot, RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate not moving the rock item from empty helmet slot"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, HelmetSlot, RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should not move the rock item from empty helmet slot"), MovedQuantity, 0); // Corrected
        Res &= Test->TestTrue(
            TEXT("Right hand slot should still contain the rock item"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneSpear, FItemBundle::NoInstances, HelmetSlot, RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should simulate not moving spear from incompatible/empty slot"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, HelmetSlot, RightHandSlot); // Corrected ItemId
        Res &= Test->TestEqual(
            TEXT("Should not move the spear item from incompatible/empty slot"), MovedQuantity, 0); // Corrected
        Res &= Test->TestTrue(
            TEXT("Right hand slot should still contain the rock item"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, LeftHandSlot, RightHandSlot, ItemIdRock, 1);
        Res &= Test->TestEqual(TEXT("Should simulate moving the helmet item from left hand slot to right hand slot"), SimulatedMoveQuantity, 1);
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, LeftHandSlot, RightHandSlot, ItemIdRock, 1);
        Res &= Test->TestEqual(TEXT("Should Swap the two items"), MovedQuantity, 1);
        Res &= Test->TestTrue(
            TEXT("Right hand slot should now contain the helmet item"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet));
        Res &= Test->TestTrue(
            TEXT("Left hand slot should now contain the rock item"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, RightHandSlot, HelmetSlot);
        Res &= Test->TestEqual(TEXT("Should simulate moving the helmet item from right hand slot to helmet slot"), SimulatedMoveQuantity, 1);
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, RightHandSlot, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should move the helmet item from right hand slot to helmet slot"), MovedQuantity, 1);
        Res &= Test->TestTrue(
            TEXT("Helmet slot should now contain the helmet item"),
            InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, LeftHandSlot, HelmetSlot);
        Res &= Test->TestEqual(TEXT("Should simulate not moving the rock item from left hand slot to helmet slot"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, LeftHandSlot, HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Should not move the rock item from left hand slot to helmet slot"), MovedQuantity, 0);
        Res &= Test->TestTrue(
            TEXT("Left hand slot should still contain the rock item"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneHelmet, FItemBundle::NoInstances, HelmetSlot, LeftHandSlot); // Corrected ItemId
        Res &= Test->TestEqual(TEXT("Should simulate not moving the helmet item from helmet slot to rock-occupied left hand"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, HelmetSlot, LeftHandSlot);
        Res &= Test->TestEqual(
            TEXT("Should not move the helmet item from helmet slot to left hand slot"), MovedQuantity, 0);
        Res &= Test->TestTrue(
            TEXT("Left hand slot should still contain the rock item"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdRock));

        InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, LeftHandSlot);
        Res &= Test->TestEqual(TEXT("Should simulate moving from empty LeftHandSlot"), SimulatedMoveQuantity, 0); // Corrected
        MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, LeftHandSlot); // Assuming RightHandSlot is empty
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

        InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 8);
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ItemIdRock, 8, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(TEXT("Should simulate moving 5 rocks to right hand slot"), SimulatedMoveQuantity, 5); // Max stack size is 5

		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks to right hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 5 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);
		// Try to move remaining 5, expecting 2 to be moved
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ItemIdRock, 5, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 5 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate not moving 3 rocks to full right hand slot"), SimulatedMoveQuantity, 0); // Corrected
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move any rocks to right hand slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
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
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should be empty"),
			!InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// Now we test the same kind of rock moving but to and then from generic inventory
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, FItemBundle::NoInstances, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from left hand slot to generic inventory"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FItemBundle::NoInstances, LeftHandSlot);
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
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FItemBundle::NoInstances, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks from left hand slot to generic inventory"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FItemBundle::NoInstances, LeftHandSlot);
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
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 2 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 2);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 6 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 6);

		// Try moving just 1 more rock to Right hand
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 1 rock to right hand slot"), SimulatedMoveQuantity, 1);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 1 rock to right hand slot"), MovedQuantity, 1);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 5 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 5);

		// move 2 more to get full stack
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(TwoRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate moving 2 rocks to right hand slot"), SimulatedMoveQuantity, 2);
		MovedQuantity = InventoryComponent->MoveItem(TwoRocks, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move 2 rocks to right hand slot"), MovedQuantity, 2);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should contain 5 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain 3 rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 3);

		// remove two rocks from right hand, leaving three, then add a stick to left hand
		// Then we try to swap the hand contents but with only 1 rock, which is invalid as it would leave 2 rocks behind making the swap impossible
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 2, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneStick, true);
		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(OneRock, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should simulate not moving 1 rock (invalid partial swap)"), SimulatedMoveQuantity, 0);
		MovedQuantity = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestEqual(
			TEXT("Should not move any rocks from right hand slot to left hand slot"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain the stick"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).ItemId.MatchesTag(ItemIdSticks));

		SimulatedMoveQuantity = InventoryComponent->ValidateMoveItem(ThreeRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot, ItemIdSticks, 1);
		Res &= Test->TestEqual(TEXT("Should simulate moving 3 rocks from right hand slot to left hand slot"), SimulatedMoveQuantity, 3);
		MovedQuantity = InventoryComponent->MoveItem(ThreeRocks, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot, ItemIdSticks, 1);
		Res &= Test->TestEqual(TEXT("Should move 3 rocks from right hand slot to left hand slot"), MovedQuantity, 3);
		Res &= Test->TestTrue(
			TEXT("Left hand slot should contain 3 rocks"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 3);
        Res &= Test->TestTrue( // Check swap result
            TEXT("Right hand slot should contain 1 stick"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSticks) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 1);


        // Instance Data Tests
        InventoryComponent->Clear_IfServer();
        // Container -> Tagged
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
        const FItemBundle* ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        UItemInstanceData* OriginalInstancePtr = ContainerKnifeBundle ? (ContainerKnifeBundle->InstanceData.Num() > 0 ? ContainerKnifeBundle->InstanceData[0] : nullptr) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Original instance pointer valid"), OriginalInstancePtr);

        MovedQuantity = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Moved knife from container to tagged"), MovedQuantity, 1);
        FTaggedItemBundle TaggedKnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Tagged slot has 1 instance"), TaggedKnifeBundle.InstanceData.Num(), 1);
        if (TaggedKnifeBundle.InstanceData.Num() == 1)
        {
            Res &= Test->TestEqual(TEXT("[Instance] Tagged instance pointer matches original"), TaggedKnifeBundle.InstanceData[0], OriginalInstancePtr);
        }
        Res &= Test->TestTrue(TEXT("[Instance] Container still contains knife bundle (as expected)"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife); // Re-fetch
        Res &= Test->TestEqual(TEXT("[Instance] Container bundle still has 1 instance"), ContainerKnifeBundle ? ContainerKnifeBundle->InstanceData.Num() : 0, 1);

        // Tagged -> Container
        MovedQuantity = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, RightHandSlot, FGameplayTag::EmptyTag);
        Res &= Test->TestEqual(TEXT("[Instance] Moved knife from tagged to container"), MovedQuantity, 1);
        TaggedKnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestFalse(TEXT("[Instance] Tagged slot is now empty"), TaggedKnifeBundle.IsValid());
        Res &= Test->TestEqual(TEXT("[Instance] Tagged slot has 0 instances"), TaggedKnifeBundle.InstanceData.Num(), 0);
        Res &= Test->TestTrue(TEXT("[Instance] Container still contains knife bundle"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife); // Re-fetch
        Res &= Test->TestEqual(TEXT("[Instance] Container bundle still has 1 instance"), ContainerKnifeBundle ? ContainerKnifeBundle->InstanceData.Num() : 0, 1);
         if (ContainerKnifeBundle && ContainerKnifeBundle->InstanceData.Num() == 1)
        {
             Res &= Test->TestEqual(TEXT("[Instance] Container instance pointer still matches original"), ContainerKnifeBundle->InstanceData[0], OriginalInstancePtr);
        }

        // Tagged -> Tagged (Move)
        InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot); // Move back to tagged slot
        MovedQuantity = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Moved knife Right -> Left"), MovedQuantity, 1);
        Res &= Test->TestFalse(TEXT("[Instance] Right hand empty after move"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        FTaggedItemBundle LeftHandKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] Left hand has knife"), LeftHandKnife.IsValid());
        Res &= Test->TestEqual(TEXT("[Instance] Left hand has 1 instance"), LeftHandKnife.InstanceData.Num(), 1);
        if(LeftHandKnife.InstanceData.Num() == 1)
        {
            Res &= Test->TestEqual(TEXT("[Instance] Left hand instance pointer matches original"), LeftHandKnife.InstanceData[0], OriginalInstancePtr);
        }

        // Tagged -> Tagged (Swap)
        InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 1);
        InventoryComponent->MoveItem(ItemIdRock, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot); // Rock in Right
        MovedQuantity = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, LeftHandSlot, RightHandSlot, ItemIdRock, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Swapped knife (Left) and rock (Right)"), MovedQuantity, 1);
        FTaggedItemBundle RightHandAfterSwap = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        FTaggedItemBundle LeftHandAfterSwap = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] Right hand has knife after swap"), RightHandAfterSwap.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Right hand instance count after swap"), RightHandAfterSwap.InstanceData.Num(), 1);
        Res &= Test->TestTrue(TEXT("[Instance] Left hand has rock after swap"), LeftHandAfterSwap.ItemId == ItemIdRock);
        Res &= Test->TestEqual(TEXT("[Instance] Left hand instance count after swap"), LeftHandAfterSwap.InstanceData.Num(), 0);
        if (RightHandAfterSwap.InstanceData.Num() == 1)
        {
             Res &= Test->TestEqual(TEXT("[Instance] Right hand instance pointer matches original after swap"), RightHandAfterSwap.InstanceData[0], OriginalInstancePtr);
        }

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
		int32 MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as right hand is occupied, and left hand is blocking and cannot be cleared"), MovedQuantity, 0);
		Res &= Test->TestTrue(
			TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(
			TEXT("Inventory should still contain 10 rock stacks"),
			InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock) == 10*5);

		// TODO: Clarify
		// Now with swapback which should still fail
		// MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 5);
		// Res &= Test->TestEqual(TEXT("Should not move spear to right hand slot as left hand blocking is occupied and cannot be cleared"), MovedQuantity, 0);
        // Res &= Test->TestTrue(
        //     TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

		// Now with swapback in opposite direction, rock in right hand -> generic swapping spear
		//MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FItemBundle::NoInstances, RightHandSlot, FGameplayTag::EmptyTag, OneSpear);
		//Res &= Test->TestEqual(TEXT("Should not move rock to generic slot as left hand is occupied and cannot be cleared blocking spear from swapback"), MovedQuantity, 0);
		//Res &= Test->TestTrue(
		//	TEXT("Right hand slot should still have a rock"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		
		// Now remove rock from left hand and try again, rock and spear should swap
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 5, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 5);
        Res &= Test->TestEqual(TEXT("Should move spear to right hand slot"), MovedQuantity, 1);
        Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(TEXT("Left hand should be empty"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Inventory should now contain 9 rocks total"),
			InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock) == 9*5);

		// Try to move spear into generic inventory thats full without swapback
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, FGameplayTag::EmptyTag);
		Res &= Test->TestEqual(TEXT("Should not move spear to generic inventory as it is full"), MovedQuantity, 0);
		Res &= Test->TestTrue(TEXT("Generic inventory should still contain 9 stacks of rocks"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 9*5);
		
		// Move spear back to generic inventory swapping with a rock explicitly
		MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, RightHandSlot, FGameplayTag::EmptyTag, ItemIdRock, 5);
		Res &= Test->TestEqual(TEXT("Should move spear to generic inventory"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Generic inventory should contain the spear"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Res &= Test->TestTrue(TEXT("Right hand should contain a rock"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);

        MovedQuantity = InventoryComponent->MoveItem(OneSpear, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 5);
        Res &= Test->TestEqual(TEXT("Should move spear to right hand slot"), MovedQuantity, 1);
        Res &= Test->TestTrue(TEXT("Right hand slot should contain the spear"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

        MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 5, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot, OneSpear);
        Res &= Test->TestEqual(TEXT("Should move rock to right hand slot"), MovedQuantity, 5);
        Res &= Test->TestTrue(TEXT("Right hand slot should contain the rock"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 5);

        // Instance Data Test
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory); // Knife in generic
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 1, true); // Rock in RightHand
        const FItemBundle* ContainerKnifeBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        UItemInstanceData* KnifeInstancePtr = ContainerKnifeBundle ? (ContainerKnifeBundle->InstanceData.Num() > 0 ? ContainerKnifeBundle->InstanceData[0] : nullptr) : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance][Swapback] Knife instance valid"), KnifeInstancePtr);

        // Swap Knife (generic) <-> Rock (RightHand) using explicit swapback params
        MovedQuantity = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot, ItemIdRock, 1);
        Res &= Test->TestEqual(TEXT("[Instance][Swapback] Moved knife to RightHand"), MovedQuantity, 1);
        FTaggedItemBundle RightHandItem = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance][Swapback] Right hand has knife"), RightHandItem.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance][Swapback] Right hand instance count correct"), RightHandItem.InstanceData.Num(), 1);
        if(RightHandItem.InstanceData.Num() == 1)
        {
            Res &= Test->TestEqual(TEXT("[Instance][Swapback] Right hand instance pointer correct"), RightHandItem.InstanceData[0], KnifeInstancePtr);
        }
        Res &= Test->TestEqual(TEXT("[Instance][Swapback] Generic has rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 1);
        Res &= Test->TestEqual(TEXT("[Instance][Swapback] Generic has no knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 0);


        return Res;
    }

     bool TestDroppingFromTaggedSlot()
    {
        InventoryComponentTestContext Context(100);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeRocks, true);
        Res &= Test->TestTrue(
            TEXT("Rocks should be added to the right hand slot"),
            InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdRock) && InventoryComponent
            ->GetItemForTaggedSlot(RightHandSlot).
            Quantity == 3);

        int32 DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 2, FItemBundle::NoInstances, FVector());
        Res &= Test->TestEqual(TEXT("Should set to drop a portion of the stackable item (2 Rocks)"), DroppedQuantity, 2);
		// Note: Server-side implementation would actually perform the drop & potentially find the world item. Client test just verifies the intent/return value.

        DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 5, FItemBundle::NoInstances, FVector());
        Res &= Test->TestEqual(
            TEXT("Should set to drop the remaining quantity of the item (1 Rock)"), DroppedQuantity, 1);

        DroppedQuantity = InventoryComponent->DropFromTaggedSlot(LeftHandSlot, 1, FItemBundle::NoInstances, FVector());
        Res &= Test->TestEqual(TEXT("Should not drop any items from an empty tagged slot"), DroppedQuantity, 0);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
        DroppedQuantity = InventoryComponent->DropFromTaggedSlot(HelmetSlot, 1, FItemBundle::NoInstances, FVector());
        Res &= Test->TestEqual(TEXT("Should set to drop the non-stackable item (Helmet)"), DroppedQuantity, 1);

         // Instance Data Test
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        FTaggedItemBundle KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstancePtr = KnifeBundle.InstanceData.Num() > 0 ? KnifeBundle.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Knife instance ptr valid before drop"), KnifeInstancePtr);
        Res &= Test->TestTrue(TEXT("[Instance] Instance registered before drop"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));

        DroppedQuantity = InventoryComponent->DropFromTaggedSlot(RightHandSlot, 1, FItemBundle::NoInstances, FVector());
        Res &= Test->TestEqual(TEXT("[Instance] Dropped 1 knife"), DroppedQuantity, 1);
        // On server, the item would be removed from the slot. On client, it might still appear until OnRep.
        // We will assume server logic removes it.
        Res &= Test->TestFalse(TEXT("[Instance] Slot should be empty after drop (server assumption)"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[Instance] Container should be empty after drop (server assumption)"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        if (KnifeInstancePtr)
        {
            // The instance should be *unregistered* from the original owner, but still exist and be transferred to the AWorldItem (which we can't easily check here)
             Res &= Test->TestFalse(TEXT("[Instance] Instance should be unregistered from original owner after drop"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
             // Ideally, we'd spawn the world item and check registration there, like in the ItemContainer test.
        }

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
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 3, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
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
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		// Clear Rocks
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
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
		InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		// Simulate removing 1 Rock from generic inventory
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		// Actually remove the moved item
		Res &= Test->TestFalse(
			TEXT(
				"CanCraftRecipe should return false when components in generic inventory are present but in insufficient quantities"),
			InventoryComponent->CanCraftRecipe(TestRecipe));

		return Res;
	}

    bool TestCraftRecipe()
    {
        InventoryComponentTestContext Context(100);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;

        UObjectRecipeData* TestRecipe = NewObject<UObjectRecipeData>();
        TestRecipe->ResultingObject = UObject::StaticClass();
        TestRecipe->QuantityCreated = 1;
        TestRecipe->Components.Add(FItemBundle(TwoRocks));
        TestRecipe->Components.Add(FItemBundle(ThreeSticks));

        InventoryComponent->AddItem_IfServer(Subsystem, FiveRocks);
        InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
        Res &= Test->TestTrue(
            TEXT("CraftRecipe_IfServer should return true when all components are present"),
            InventoryComponent->CraftRecipe_IfServer(TestRecipe));

        Res &= Test->TestEqual(
            TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"),
            InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock), 3);
        Res &= Test->TestEqual(
            TEXT("CraftRecipe_IfServer should remove the correct quantity of the component items"),
            InventoryComponent->GetQuantityTotal_Implementation(ItemIdSticks), 0);

        InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);

        Res &= Test->TestFalse(
            TEXT("CraftRecipe_IfServer should return false when a component is missing"),
            InventoryComponent->CraftRecipe_IfServer(TestRecipe));

        Res &= Test->TestFalse(
            TEXT("CraftRecipe_IfServer should return false when the recipe is null"),
            InventoryComponent->CraftRecipe_IfServer(nullptr));

        InventoryComponent->AddItem_IfServer(Subsystem, OneRock);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ThreeSticks, true);
        Res &= Test->TestTrue(
            TEXT("CraftRecipe_IfServer should return true when components are spread between generic and tagged slots"),
            InventoryComponent->CraftRecipe_IfServer(TestRecipe));

        InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(LeftHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
        InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdSticks, 2, true);
        Res &= Test->TestFalse(
            TEXT(
                "CraftRecipe_IfServer should return false when not all components are present in sufficient quantities"),
            InventoryComponent->CraftRecipe_IfServer(TestRecipe));

        // Instance Data Test
        InventoryComponent->Clear_IfServer();
        // Create recipe requiring 1 knife
        UObjectRecipeData* KnifeRecipe = NewObject<UObjectRecipeData>();
        KnifeRecipe->ResultingObject = UObject::StaticClass();
        KnifeRecipe->QuantityCreated = 1;
        KnifeRecipe->Components.Add(FItemBundle(ItemIdBrittleCopperKnife, 1));

        // Add knife to tagged slot
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        FTaggedItemBundle KnifeBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstancePtr = KnifeBundle.InstanceData.Num() > 0 ? KnifeBundle.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance][Craft] Knife instance ptr valid"), KnifeInstancePtr);

        // Craft using the knife
        bool bCrafted = InventoryComponent->CraftRecipe_IfServer(KnifeRecipe);
        Res &= Test->TestTrue(TEXT("[Instance][Craft] Crafting with tagged knife should succeed"), bCrafted);
        Res &= Test->TestFalse(TEXT("[Instance][Craft] Tagged slot should be empty after craft"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("[Instance][Craft] Container should be empty after craft"), InventoryComponent->Contains(ItemIdBrittleCopperKnife));
        if(KnifeInstancePtr)
        {
            Res &= Test->TestFalse(TEXT("[Instance][Craft] Knife instance should be unregistered after craft"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
        }

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
			TEXT("Should successfully add rocks within capacity"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock),
			3);
		InventoryComponent->AddItem_IfServer(Subsystem, ThreeSticks);
		// Trying to add more rocks, total weight would be 6 but capacity is 5
		Res &= Test->TestTrue(
			TEXT("Should fail to add all 3 sticks due to weight capacity"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdSticks) < 3);

		// Remove any sticks we might have added partially
		InventoryComponent->DestroyItem_IfServer(ItemIdSticks, 99, FItemBundle::NoInstances, EItemChangeReason::Removed, true);
		// Verify removal of sticks only
		Res &= Test->TestEqual(
			TEXT("Should remove all sticks"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdSticks), 0);
		Res &= Test->TestEqual(
			TEXT("Should not remove any rocks"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdRock), 3);

		// Step 2: Adding Unstackable Items to Tagged Slots
		int32 QuantityAdded = InventoryComponent->
			AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneHelmet, true); // Weight = 2
		Res &= Test->TestEqual(TEXT("Should successfully add a helmet within capacity"), QuantityAdded, 1);
		QuantityAdded = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneHelmet, true);
		// Trying to add another helmet, total weight would be 4
		Res &= Test->TestEqual(TEXT("Should fail to add a second helmet beyond capacity"), QuantityAdded, 0);

		// Step 3: Adding Stackable items
		InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdHelmet, 1, FItemBundle::NoInstances, EItemChangeReason::Removed);
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
			InventoryComponent->GetQuantityTotal_Implementation(ItemIdGiantBoulder), 0);

		return Res;
	}


    bool TestAddItemToAnySlot()
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
		InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdRock, 5, FItemBundle::NoInstances, EItemChangeReason::Removed);
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

		InventoryComponent->MoveItem(ItemIdRock, 5, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);

		// Adding items back to generic slots if there's still capacity after attempting tagged slots
		InventoryComponent->MaxWeight = 25; // Increase weight capacity for this test
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdGiantBoulder, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        Res &= Test->TestEqual(TEXT("Should add heavy items to generic slots after trying tagged slots"), Added, 1);

        // Instance Data Test
        InventoryComponent->Clear_IfServer();
        InventoryComponent->MaxSlotCount = 2; // Limit generic
        // Add knife, prefer tagged
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        Res &= Test->TestEqual(TEXT("[Instance] Added 1 knife, prefer tagged"), Added, 1);
        FTaggedItemBundle TaggedKnife = InventoryComponent->GetItemForTaggedSlot(RightHandSlot); // Assumes RightHand is preferred or first available universal
        Res &= Test->TestTrue(TEXT("[Instance] Knife in RightHandSlot"), TaggedKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Knife instance count in tagged slot"), TaggedKnife.InstanceData.Num(), 1);
        UItemInstanceData* Instance1 = TaggedKnife.InstanceData.Num() > 0 ? TaggedKnife.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Knife instance ptr 1 valid"), Instance1);
        const FItemBundle* ContainerKnife = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Container knife instance count"), ContainerKnife ? ContainerKnife->InstanceData.Num() : 0, 1);

        // Add another knife, prefer generic (should go to generic)
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= Test->TestEqual(TEXT("[Instance] Added 1 knife, prefer generic"), Added, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Generic container has 1 knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 1);
        FTaggedItemBundle LeftHandKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestFalse(TEXT("[Instance] Left hand does not have knife"), LeftHandKnife.IsValid());
        ContainerKnife = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife); // Re-fetch
        Res &= Test->TestEqual(TEXT("[Instance] Container now has 2 instances total"), ContainerKnife ? ContainerKnife->InstanceData.Num() : 0, 2);
        UItemInstanceData* Instance2 = nullptr;
        if(ContainerKnife && ContainerKnife->InstanceData.Num() == 2)
        {
            Instance2 = (ContainerKnife->InstanceData[0] != Instance1) ? ContainerKnife->InstanceData[0] : ContainerKnife->InstanceData[1];
        }
         Res &= Test->TestNotNull(TEXT("[Instance] Knife instance ptr 2 valid"), Instance2);

        // Add third knife, prefer generic (should go to generic slot 2)
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= Test->TestEqual(TEXT("[Instance] Added 1 knife, prefer generic (2nd)"), Added, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Generic container has 2 knives"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 2);

        // Add fourth knife, prefer generic (generic full, should spill to LeftHand)
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= Test->TestEqual(TEXT("[Instance] Added 1 knife, prefer generic (spill)"), Added, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Generic container still has 2 knives"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 2);
        LeftHandKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] Left hand now has knife"), LeftHandKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Left hand instance count"), LeftHandKnife.InstanceData.Num(), 1);
        UItemInstanceData* Instance4 = LeftHandKnife.InstanceData.Num() > 0 ? LeftHandKnife.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance] Knife instance ptr 4 valid"), Instance4);

        ContainerKnife = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife); // Re-fetch
        Res &= Test->TestEqual(TEXT("[Instance] Container now has 4 instances total"), ContainerKnife ? ContainerKnife->InstanceData.Num() : 0, 4);


        return Res;
    }

	bool TestAddItem()
    {
        InventoryComponentTestContext Context(100);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();
        FDebugTestResult Res = true;

        InventoryComponent->Clear_IfServer();

        int32 RequestedRocks = 60;

        int32 AddedRocks = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, RequestedRocks, true);
        Res &= Test->TestEqual(
            TEXT("Adding 60 Rocks should return 55 as the added quantity (5*9+5+5)"),
            AddedRocks, 55);

        int32 GenericRockCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock);
        Res &= Test->TestEqual(
            TEXT("Generic inventory should hold 45 Rocks"),
            GenericRockCount, 45);

        FTaggedItemBundle LeftHandBundle = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        FTaggedItemBundle RightHandBundle = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestEqual(
            TEXT("Left hand slot should hold 5 Rocks"),
            LeftHandBundle.Quantity, 5);
        Res &= Test->TestEqual(
            TEXT("Right hand slot should hold 5 Rocks"),
            RightHandBundle.Quantity, 5);

        Res &= Test->TestFalse(
            TEXT("Helmet slot should not contain Rocks"),
            InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
        Res &= Test->TestFalse(
            TEXT("Chest slot should not contain Rocks"),
            InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());

        InventoryComponent->Clear_IfServer();

        int32 RequestedHelmets = 15;
		// AddItem_IfServer calls AddItemToAnySlot with default policy (PreferSpecialized)
        int32 AddedHelmets = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdHelmet, RequestedHelmets, true);
        Res &= Test->TestEqual(
            TEXT("Adding 15 Helmets should only add 12 due to slot limits (9 generic + 1 helmet + 1 left + 1 right)"),
            AddedHelmets, 12);

        int32 GenericHelmetCount = InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet);
        Res &= Test->TestEqual(
            TEXT("Generic inventory should hold 9 Helmets"),
            GenericHelmetCount, 9);

        FTaggedItemBundle HelmetSlotBundle = InventoryComponent->GetItemForTaggedSlot(HelmetSlot);
        Res &= Test->TestEqual(
            TEXT("Helmet slot should hold 1 Helmet"),
            HelmetSlotBundle.Quantity, 1);

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
            TEXT("Should have 9 helmets to generic and 1 to helmet slot"), QuantityOnlyContainer, 9);
        Res &= Test->TestEqual(
            TEXT("Should still have 1 in helmet slot"),
            InventoryComponent->GetItemForTaggedSlot(HelmetSlot).Quantity, 1);
		Res &= Test->TestEqual(
            TEXT("Left hand should be empty as spear is blocking it"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity, 0);
        Res &= Test->TestTrue(
            TEXT("Should have spear in right hand"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));


        // Instance Data Test
        InventoryComponent->Clear_IfServer();
		InventoryComponent->MaxSlotCount = 1; // Limit generic
        // Add 3 knives. Expect: 1 generic, 1 left, 1 right
        AddedRocks = InventoryComponent->AddItem_IfServer(Subsystem, ItemIdBrittleCopperKnife, 3, true);
        Res &= Test->TestEqual(TEXT("[Instance] Added 3 knives"), AddedRocks, 3);

        Res &= Test->TestEqual(TEXT("[Instance] Generic has 1 knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 1);
        FTaggedItemBundle LeftKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        FTaggedItemBundle RightKnife = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] Left hand has knife"), LeftKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Left hand instance count"), LeftKnife.InstanceData.Num(), 1);
        Res &= Test->TestTrue(TEXT("[Instance] Right hand has knife"), RightKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Right hand instance count"), RightKnife.InstanceData.Num(), 1);

        const FItemBundle* ContainerBundle = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] Container has 3 total instances"), ContainerBundle ? ContainerBundle->InstanceData.Num() : 0, 3);

        UItemInstanceData* LeftPtr = LeftKnife.InstanceData.Num() > 0 ? LeftKnife.InstanceData[0] : nullptr;
        UItemInstanceData* RightPtr = RightKnife.InstanceData.Num() > 0 ? RightKnife.InstanceData[0] : nullptr;
        UItemInstanceData* GenericPtr = nullptr;
        if (ContainerBundle && ContainerBundle->InstanceData.Num() == 3)
        {
            for(auto* Ptr : ContainerBundle->InstanceData)
            {
                if (Ptr != LeftPtr && Ptr != RightPtr)
                {
                    GenericPtr = Ptr;
                    break;
                }
            }
        }
        Res &= Test->TestNotNull(TEXT("[Instance] Generic instance pointer found"), GenericPtr);
        Res &= Test->TestTrue(TEXT("[Instance] All 3 instance pointers unique"), LeftPtr && RightPtr && GenericPtr && LeftPtr != RightPtr && LeftPtr != GenericPtr && RightPtr != GenericPtr);


        return Res;
    }

	bool TestExclusiveUniversalSlots()
    {
        InventoryComponentTestContext Context(20);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();

        FDebugTestResult Res = true;

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

        // Instance Data Test (Using Knife as example, assuming it's NOT exclusive)
        InventoryComponent->Clear_IfServer();
        // Add knife to LeftHand (should succeed if not exclusive)
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdBrittleCopperKnife, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Should add knife to LeftHand"), Added, 1);
        FTaggedItemBundle LeftKnife = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        Res &= Test->TestTrue(TEXT("[Instance] LeftHand has knife"), LeftKnife.ItemId == ItemIdBrittleCopperKnife);
        Res &= Test->TestEqual(TEXT("[Instance] LeftHand instance count"), LeftKnife.InstanceData.Num(), 1);

        // Test with an item exclusive to LeftHand (e.g., Shortbow)
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdShortbow, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Should NOT add Shortbow to RightHand"), Added, 0);
		// Remove the knife from left hand
		InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, EItemChangeReason::Removed);
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdShortbow, 1);
        Res &= Test->TestEqual(TEXT("[Instance] Should add Shortbow to LeftHand"), Added, 1); // It replaces the knife

        return Res;
    }

	bool TestBlockingSlots()
    {
        InventoryComponentTestContext Context(20);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();
        InventoryComponent->MaxSlotCount = 2;

        FDebugTestResult Res = true;

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneSpear, true);
        Res &= Test->TestTrue(
            TEXT("Right hand slot should contain a spear"),
			InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));

		// Try adding a rock to left hand, should fail
		auto* RockItemData = Subsystem->GetItemDataById(ItemIdRock);
		Res &= Test->TestTrue(TEXT("Can't add rock to left hand"), InventoryComponent->GetReceivableQuantityForTaggedSlot(RockItemData, LeftHandSlot) == 0);
		int32 Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true, false);
        Res &= Test->TestEqual(TEXT("Should not add a rock to left hand slot"), Added, 0);
        Res &= Test->TestFalse(
            TEXT("Left hand slot should not contain a rock"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add a rock to generic inventory"), Added, 1);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain a rock"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 1);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a rock"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

        InventoryComponent->AddItem_IfServer(Subsystem, OneRock);
        Added = InventoryComponent->MoveItem(OneRock, FItemBundle::NoInstances, FGameplayTag::EmptyTag, LeftHandSlot);
        Res &= Test->TestEqual(TEXT("Should not move a rock to left hand slot"), Added, 0);
        Res &= Test->TestFalse(
            TEXT("Left hand slot should not contain a rock"),
            InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

		// Now add a helmet to helmetslot and verify we can't move it to left hand
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
		Res &= Test->TestTrue(
			TEXT("Helmet slot should contain a helmet"),
			InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet));
		int32 Moved = InventoryComponent->MoveItem(OneHelmet, FItemBundle::NoInstances, FGameplayTag::EmptyTag, LeftHandSlot);
		Res &= Test->TestEqual(TEXT("Should not move a helmet to left hand slot"), Moved, 0);
		Res &= Test->TestFalse(
			TEXT("Left hand slot should not contain a helmet"),
			InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

        InventoryComponent->Clear_IfServer();
		Res &= Test->TestFalse(TEXT("Left hand should not be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));
		
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
		
		auto* SpearItemData = Subsystem->GetItemDataById(ItemIdSpear);
		Res &= Test->TestTrue(TEXT("Can't add spear to right hand"), InventoryComponent->GetReceivableQuantityForTaggedSlot(SpearItemData, RightHandSlot) == 0);
		Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 1, EPreferredSlotPolicy::PreferAnyTaggedSlot);
		Res &= Test->TestEqual(TEXT("Should add a spear to generic inventory"), Added, 1);
		Res &= Test->TestTrue(
			TEXT("Generic inventory should contain a spear"),
			InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Res &= Test->TestFalse(TEXT("Right hand slot should not contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

        Added = InventoryComponent->MoveItem(ItemIdSpear, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(TEXT("Should not move a spear to right hand slot"), Added, 0);
        Res &= Test->TestFalse(TEXT("Right hand slot should not contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("Left hand slot should still contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        Res &= Test->TestTrue(TEXT("Right hand should contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
        Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true, false);
        Res &= Test->TestEqual(TEXT("Should not add a rock to left hand slot"), Added, 0);
        Res &= Test->TestFalse(TEXT("Left hand slot should not contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());

        InventoryComponent->Clear_IfServer();
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= Test->TestEqual(TEXT("Should add a spear to generic inventory"), Added, 1);
		Res &= Test->TestTrue(TEXT("Generic inventory should contain a spear"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 1);
		Added = InventoryComponent->MoveItem(ItemIdSpear, 1, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("Should move a spear to right hand slot"), Added, 1);
		Res &= Test->TestTrue(TEXT("Right hand slot should contain a spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));


        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSpear, 3, EPreferredSlotPolicy::PreferGenericInventory);
		Res &= Test->TestTrue(TEXT("Right hand should have spear"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestFalse(TEXT("Left hand should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Generic inventory should contain two spears"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdSpear) == 2);
		
		// Remove spear from right hand and Add a rock to right hand
		InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot, 1, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
		InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneRock, true, false);
		// Move the rock to generic inventory
		int32 MovedQuantity = InventoryComponent->MoveItem(ItemIdRock, 1, FItemBundle::NoInstances, RightHandSlot, FGameplayTag::EmptyTag, ItemIdSpear, 1);
		// Verify rock is in generic inventory and spear is in right hand
		Res &= Test->TestEqual(TEXT("Should have moved 1 item"), MovedQuantity, 1);
		Res &= Test->TestTrue(TEXT("Rock should be in generic inventory"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock) == 1);
		Res &= Test->TestTrue(TEXT("Spear should be in right hand"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdSpear));
		Res &= Test->TestFalse(TEXT("Left hand should be empty"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("Left hand should be blocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));


        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        InventoryComponent->AddItemToAnySlot(Subsystem, OneRock, EPreferredSlotPolicy::PreferGenericInventory);
        InventoryComponent->RemoveQuantityFromTaggedSlot_IfServer(RightHandSlot,1, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestFalse(TEXT("Right hand should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("Left hand should be unblocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));
        InventoryComponent->AddItemToAnySlot(Subsystem, OneSpear, EPreferredSlotPolicy::PreferAnyTaggedSlot);
        InventoryComponent->MoveItem(ItemIdSpear, 1, FItemBundle::NoInstances, RightHandSlot, FGameplayTag::EmptyTag);
        Res &= Test->TestFalse(TEXT("Right hand should be empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("Left hand should be unblocked"), InventoryComponent->IsTaggedSlotBlocked(LeftHandSlot));

        // Instance Data Test (Using Longbow - blocks RightHand via ItemTypeTwoHandedOffhand category)
        InventoryComponent->Clear_IfServer();
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdLongbow, 1);
        Res &= Test->TestEqual(TEXT("[Instance][Block] Added Longbow to LeftHand"), Added, 1);
        Res &= Test->TestTrue(TEXT("[Instance][Block] RightHand should be blocked by Longbow"), InventoryComponent->IsTaggedSlotBlocked(RightHandSlot));

        // Try adding knife to blocked RightHand
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true, false);
        Res &= Test->TestEqual(TEXT("[Instance][Block] Should fail to add knife to blocked RightHand"), Added, 0);
        Res &= Test->TestFalse(TEXT("[Instance][Block] RightHand remains empty"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());

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

		auto* SpearItemData = Subsystem->GetItemDataById(ItemIdSpear);
		int32 ReceivableQuantity = InventoryComponent->GetReceivableQuantity(SpearItemData);
		Res &= Test->TestEqual(TEXT("Should be able to receive 3 spears, one righthand, two in generic"), ReceivableQuantity, 3);

		// Remove old right hand and add a new version that does not make two handed items exclusive to right hand
		int32 IndexToReplace = InventoryComponent->UniversalTaggedSlots.IndexOfByPredicate([&](const FUniversalTaggedSlot& Slot) { return Slot.Slot == RightHandSlot; });
		InventoryComponent->UniversalTaggedSlots[IndexToReplace] = FUniversalTaggedSlot(RightHandSlot, LeftHandSlot, ItemTypeTwoHanded, FGameplayTag());
		
		// Now verify we can still only receive 3 spears as adding a spear to each hand would violate blocking
		ReceivableQuantity = InventoryComponent->GetReceivableQuantity(SpearItemData);
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
        Listener->Clear(); // Reset listener flags

        InventoryComponent->RemoveItemFromAnyTaggedSlots_IfServer(ItemIdRock, 5, FItemBundle::NoInstances, EItemChangeReason::Removed);
        Res &= Test->TestFalse(TEXT("Right hand slot should be empty after removal"),
                               InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("OnItemRemoved event should trigger for rock removal"), Listener->bItemRemovedFromTaggedTriggered);
        Listener->Clear();

        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= Test->TestEqual(TEXT("Should add 5 rocks to generic slots"), Added, 5);
        Res &= Test->TestFalse(TEXT("Right hand slot should remain empty"),
                               InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestFalse(TEXT("Left hand slot should remain empty"),
                               InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("OnItemAdded event should trigger for rock addition"), Listener->bItemAddedTriggered);
		Listener->Clear();

        InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 2, EPreferredSlotPolicy::PreferGenericInventory);
        Res &= Test->TestEqual(TEXT("Should add 2 sticks after generic slots are full"), Added, 2);
        Res &= Test->TestEqual(TEXT("First universal tagged slot (left hand) should contain 2 sticks"),
                               InventoryComponent->GetItemForTaggedSlot(InventoryComponent->UniversalTaggedSlots[0].Slot).Quantity, 2);
        Res &= Test->TestTrue(TEXT("OnItemAddedToTaggedSlot event should trigger for spilled sticks"), Listener->bItemAddedToTaggedTriggered);
        Res &= Test->TestTrue(TEXT("Previous item for spilled sticks should be empty"),
            (!Listener->AddedToTaggedPreviousItem.IsValid() ||
             (Listener->AddedToTaggedPreviousItem.ItemId == FGameplayTag::EmptyTag && Listener->AddedToTaggedPreviousItem.Quantity == 0)));
		Listener->Clear();
		
        InventoryComponent->Clear_IfServer();
        Listener->Clear();

        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdRock, 5, EPreferredSlotPolicy::PreferGenericInventory);
		Listener->Clear();
        Added = InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdSticks, 3, EPreferredSlotPolicy::PreferGenericInventory);
		Listener->Clear();
        InventoryComponent->MoveItem(ItemIdRock, 3, FItemBundle::NoInstances, FGameplayTag(), RightHandSlot);
        Res &= Test->TestTrue(TEXT("Right hand slot should contain rocks"),
                               InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
		Res &= Test->TestTrue(TEXT("Move Gen->Tagged Add event triggered"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestTrue(TEXT("Move Gen->Tagged Remove event triggered"), Listener->bItemRemovedTriggered); // From container
        Listener->Clear();

        InventoryComponent->MoveItem(ItemIdSticks, 2, FItemBundle::NoInstances, FGameplayTag(), LeftHandSlot);
        Res &= Test->TestTrue(TEXT("Event should fire for adding to tagged slot"), Listener->bItemAddedToTaggedTriggered);
        Res &= Test->TestTrue(TEXT("Left hand slot should have received sticks"), Listener->AddedSlotTag == LeftHandSlot);
        Res &= Test->TestTrue(TEXT("Added item should be sticks"), Listener->AddedToTaggedItemStaticData->ItemId == ItemIdSticks);
        Res &= Test->TestEqual(TEXT("Correct quantity moved"), Listener->AddedToTaggedQuantity, 2);
        Res &= Test->TestTrue(TEXT("Previous item should be empty"),
            !Listener->AddedToTaggedPreviousItem.IsValid());
		Res &= Test->TestEqual(TEXT("Event instance count"), Listener->AddedToTaggedInstances.Num(), 0); // Sticks have no instance data
		Listener->Clear();


        InventoryComponent->MoveItem(ItemIdRock, 3, FItemBundle::NoInstances, RightHandSlot, FGameplayTag());
        Res &= Test->TestTrue(TEXT("Event should fire for removing from tagged slot"), Listener->bItemRemovedFromTaggedTriggered);
        Res &= Test->TestTrue(TEXT("Removed slot should be RightHandSlot"), Listener->RemovedSlotTag == RightHandSlot);
        Res &= Test->TestTrue(TEXT("Removed item should be rocks"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdRock);
        Res &= Test->TestEqual(TEXT("Correct quantity removed"), Listener->RemovedFromTaggedQuantity, 3);
        Res &= Test->TestTrue(TEXT("Event should fire for adding to generic slots"), Listener->bItemAddedTriggered);
        Res &= Test->TestTrue(TEXT("Added item should be rocks"), Listener->AddedItemStaticData->ItemId == ItemIdRock);
        Res &= Test->TestEqual(TEXT("Correct quantity added to generic slots"), Listener->AddedQuantity, 3);
		Listener->Clear();

        InventoryComponent->MoveItem(ItemIdRock, 2, FItemBundle::NoInstances, FGameplayTag(), LeftHandSlot, ItemIdSticks, 2);
		// This is a swap: Rock (Gen) <-> Sticks (LeftHand)
        // 1. Remove Sticks from LeftHand
        Res &= Test->TestTrue(TEXT("Swap Remove Sticks from LeftHand"), Listener->bItemRemovedFromTaggedTriggered);
        Res &= Test->TestTrue(TEXT("Swap Remove Sticks Tag"), Listener->RemovedSlotTag == LeftHandSlot);
        Res &= Test->TestTrue(TEXT("Swap Remove Sticks Item"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdSticks);
        Res &= Test->TestEqual(TEXT("Swap Remove Sticks Qty"), Listener->RemovedFromTaggedQuantity, 2);
		// 2. Add Sticks to Generic
		Res &= Test->TestTrue(TEXT("Swap Add Sticks to Generic"), Listener->bItemAddedTriggered);
        Res &= Test->TestTrue(TEXT("Swap Add Sticks Item (Gen)"), Listener->AddedItemStaticData->ItemId == ItemIdSticks);
        Res &= Test->TestEqual(TEXT("Swap Add Sticks Qty (Gen)"), Listener->AddedQuantity, 2);
		// 3. Remove Rock from Generic (This happens implicitly during the Add-to-Tagged part of the move)
		Res &= Test->TestTrue(TEXT("Swap Remove Rock from Generic"), Listener->bItemRemovedTriggered);
		Res &= Test->TestTrue(TEXT("Swap Remove Rock Item (Gen)"), Listener->RemovedItemStaticData->ItemId == ItemIdRock);
		Res &= Test->TestEqual(TEXT("Swap Remove Rock Qty (Gen)"), Listener->RemovedQuantity, 2);
		// 4. Add Rock to LeftHand
        Res &= Test->TestTrue(TEXT("Swap Add Rock to LeftHand"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestTrue(TEXT("Swap Add Rock Tag"), Listener->AddedSlotTag == LeftHandSlot);
		Res &= Test->TestTrue(TEXT("Swap Add Rock Item"), Listener->AddedToTaggedItemStaticData->ItemId == ItemIdRock);
		Res &= Test->TestEqual(TEXT("Swap Add Rock Qty"), Listener->AddedToTaggedQuantity, 2);
        Res &= Test->TestTrue(TEXT("Swap Previous item should be sticks"),
            (Listener->AddedToTaggedPreviousItem.ItemId == ItemIdSticks && Listener->AddedToTaggedPreviousItem.Quantity == 2));
		Listener->Clear();

        InventoryComponent->MoveItem(ItemIdRock, 2, FItemBundle::NoInstances, LeftHandSlot, FGameplayTag());
        Res &= Test->TestTrue(TEXT("Event should fire for removing item from tagged slot"), Listener->bItemRemovedFromTaggedTriggered);
        Res &= Test->TestTrue(TEXT("Left hand slot should be affected"), Listener->RemovedSlotTag == LeftHandSlot);
        Res &= Test->TestTrue(TEXT("Removed item should be rocks"), Listener->RemovedFromTaggedItemStaticData->ItemId == ItemIdRock);
        Res &= Test->TestEqual(TEXT("Correct quantity removed"), Listener->RemovedFromTaggedQuantity, 2);
        Res &= Test->TestFalse(TEXT("Left hand slot should now be empty"),
                               InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
		Listener->Clear();

        InventoryComponent->Clear_IfServer();
        Listener->Clear();
        int32 Moved = InventoryComponent->MoveItem(ItemIdHelmet, 1, FItemBundle::NoInstances, FGameplayTag(), RightHandSlot);
        Res &= Test->TestEqual(TEXT("Should not move (no item)"), Moved, 0);
        Res &= Test->TestFalse(TEXT("No events should fire (no item)"), Listener->bItemAddedTriggered || Listener->bItemRemovedTriggered || Listener->bItemAddedToTaggedTriggered || Listener->bItemRemovedFromTaggedTriggered);
		Listener->Clear();

        InventoryComponent->AddItem_IfServer(Subsystem, ItemIdRock, 5);
		Listener->Clear();
        int PartialMove = InventoryComponent->MoveItem(ItemIdRock, 99, FItemBundle::NoInstances, FGameplayTag::EmptyTag, RightHandSlot);
        Res &= Test->TestEqual(TEXT("Should move only available quantity"), PartialMove, 5);
        Res &= Test->TestTrue(TEXT("Event should fire for adding to tagged slot (partial)"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestTrue(TEXT("Event should fire for removing from container (partial)"), Listener->bItemRemovedTriggered);
        Res &= Test->TestTrue(TEXT("Previous item for partial move should be empty"), !Listener->AddedToTaggedPreviousItem.IsValid());
		Listener->Clear();

        // Instance Data Event Test
        InventoryComponent->Clear_IfServer();
        Listener->Clear();
        Added = InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Add tagged event fired"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Add tagged instance count"), Listener->AddedToTaggedInstances.Num(), 1);
        UItemInstanceData* InstancePtr = Listener->AddedToTaggedInstances.Num() > 0 ? Listener->AddedToTaggedInstances[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Instance][Event] Add tagged instance ptr valid"), InstancePtr);
        Listener->Clear();

        InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, RightHandSlot, LeftHandSlot);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Move tagged remove event fired"), Listener->bItemRemovedFromTaggedTriggered);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Move tagged remove instance count"), Listener->RemovedFromTaggedInstances.Num(), 1);
		if(Listener->RemovedFromTaggedInstances.Num() == 1) Res &= Test->TestTrue(TEXT("[Instance][Event] Move tagged remove instance ptr"), Listener->RemovedFromTaggedInstances[0] == InstancePtr);

        Res &= Test->TestTrue(TEXT("[Instance][Event] Move tagged add event fired"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Move tagged add instance count"), Listener->AddedToTaggedInstances.Num(), 1);
        if(Listener->AddedToTaggedInstances.Num() == 1) Res &= Test->TestTrue(TEXT("[Instance][Event] Move tagged add instance ptr"), Listener->AddedToTaggedInstances[0] == InstancePtr);
		Listener->Clear();


		// Test specific instance move event
		InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, LeftHandSlot, NoTag); // Move back to container
		InventoryComponent->AddItemToAnySlot(Subsystem, ItemIdBrittleCopperKnife, 1, EPreferredSlotPolicy::PreferGenericInventory); // Add a second knife to container
		const FItemBundle* ContainerKnives = InventoryComponent->FindItemInstance(ItemIdBrittleCopperKnife);
		Res &= Test->TestEqual(TEXT("Container should have 2 knives"), ContainerKnives ? ContainerKnives->Quantity : 0, 2);
		Res &= Test->TestEqual(TEXT("Container should have 2 instances"), ContainerKnives ? ContainerKnives->InstanceData.Num() : 0, 2);
		UItemInstanceData* InstanceA = ContainerKnives->InstanceData[0];
		UItemInstanceData* InstanceB = ContainerKnives->InstanceData[1];
		TArray SpecificInstanceToMove = { InstanceA };
		Listener->Clear();

		int32 QuantityMoved = InventoryComponent->MoveItem(ItemIdBrittleCopperKnife, 1, SpecificInstanceToMove, NoTag, RightHandSlot);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Move specific instance quantity moved"), QuantityMoved, 1);
		Res &= Test->TestTrue(TEXT("[Instance][Event] Move specific instance add tagged event fired"), Listener->bItemAddedToTaggedTriggered);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Move specific instance add tagged instance count"), Listener->AddedToTaggedInstances.Num(), 1);
		if(Listener->AddedToTaggedInstances.Num() == 1) Res &= Test->TestTrue(TEXT("[Instance][Event] Move specific instance add tagged instance ptr check"), Listener->AddedToTaggedInstances[0] == InstanceA);

		Res &= Test->TestTrue(TEXT("[Instance][Event] Move specific instance remove container event fired"), Listener->bItemRemovedTriggered);
		Res &= Test->TestEqual(TEXT("[Instance][Event] Move specific instance remove container instance count"), Listener->RemovedInstances.Num(), 1);
		if(Listener->RemovedInstances.Num() == 1) Res &= Test->TestTrue(TEXT("[Instance][Event] Move specific instance remove container instance ptr check"), Listener->RemovedInstances[0] == InstanceA);
		Listener->Clear();


        return Res;
    }
	
	bool TestIndirectOperations()
    {
        InventoryComponentTestContext Context(99);
        auto* InventoryComponent = Context.InventoryComponent;
        auto* Subsystem = Context.TestFixture.GetSubsystem();
        InventoryComponent->MaxSlotCount = 9;
        FDebugTestResult Res = true;

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, OneRock, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, ChestSlot, OneChestArmor, true);

        FTaggedItemBundle RightHandKnife = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        UItemInstanceData* KnifeInstancePtr = RightHandKnife.InstanceData.Num() > 0 ? RightHandKnife.InstanceData[0] : nullptr;
        Res &= Test->TestNotNull(TEXT("[Indirect][Instance] Knife instance ptr valid"), KnifeInstancePtr);
        Res &= Test->TestTrue(TEXT("[Indirect][Instance] Knife instance registered"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));


        InventoryComponent->DestroyItem_IfServer(OneRock, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestFalse(TEXT("Left hand slot should not contain a rock"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestEqual( // Corrected check
            TEXT("Generic inventory should not contain a rock"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdRock), 0);

        InventoryComponent->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 1, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestFalse(TEXT("Right hand slot should not contain a knife"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestEqual( // Corrected check
            TEXT("Generic inventory should not contain a knife"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdBrittleCopperKnife), 0);
        if(KnifeInstancePtr)
        {
             Res &= Test->TestFalse(TEXT("[Indirect][Instance] Knife instance unregistered after destroy"), Context.TempActor->IsReplicatedSubObjectRegistered(KnifeInstancePtr));
        }

        InventoryComponent->DestroyItem_IfServer(OneHelmet, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestFalse(TEXT("Helmet slot should be empty after helmet destroy"), InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
        Res &= Test->TestEqual( // Corrected check
            TEXT("Generic inventory should not contain a helmet"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdHelmet), 0);

        InventoryComponent->DestroyItem_IfServer(OneChestArmor, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestFalse(TEXT("Chest slot should be empty after armor destroy"), InventoryComponent->GetItemForTaggedSlot(ChestSlot).IsValid());
        Res &= Test->TestEqual( // Corrected check
            TEXT("Generic inventory should not contain chest armor"), InventoryComponent->GetContainerOnlyItemQuantity(ItemIdChestArmor), 0);

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdRock, 3);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdRock, 5);

        InventoryComponent->DestroyItem_IfServer(OneRock, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        int32 AmountContainedInBothHands = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity + InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity;
        Res &= Test->TestEqual(TEXT("The hands should now contain combined 7 rocks"), AmountContainedInBothHands, 7);
        // Determine which slot lost the rock (depends on implementation, likely removes from the end of TaggedSlotItems first if multiple match)
		// Assuming RightHandSlot might be processed first due to internal ordering or recent addition.
        Res &= Test->TestTrue(TEXT("Left hand should contain 3 rocks"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).Quantity == 3);
        Res &= Test->TestTrue(TEXT("Right hand should contain 4 rocks"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 4);


        InventoryComponent->Clear_IfServer();

        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, OneHelmet, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, HelmetSlot, OneHelmet, true);

        InventoryComponent->DestroyItem_IfServer(OneHelmet, FItemBundle::NoInstances, EItemChangeReason::ForceDestroyed);
        Res &= Test->TestTrue(TEXT("Right hand or helmet slot should be empty after helmet destroy"), !InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid() || !InventoryComponent->GetItemForTaggedSlot(HelmetSlot).IsValid());
		// Check that ONE of them still has the helmet
		bool bHelmetRemains = (InventoryComponent->GetItemForTaggedSlot(HelmetSlot).ItemId.MatchesTag(ItemIdHelmet) && InventoryComponent->GetItemForTaggedSlot(HelmetSlot).Quantity == 1) ||
							  (InventoryComponent->GetItemForTaggedSlot(RightHandSlot).ItemId.MatchesTag(ItemIdHelmet) && InventoryComponent->GetItemForTaggedSlot(RightHandSlot).Quantity == 1);
        Res &= Test->TestTrue(TEXT("One slot should still contain a helmet"), bHelmetRemains);
		Res &= Test->TestEqual(TEXT("Total container quantity should be 1"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdHelmet), 1);

        // Test specific instance destruction
        InventoryComponent->Clear_IfServer();
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, RightHandSlot, ItemIdBrittleCopperKnife, 1, true);
        InventoryComponent->AddItemToTaggedSlot_IfServer(Subsystem, LeftHandSlot, ItemIdBrittleCopperKnife, 1, true);
        FTaggedItemBundle RightKnifeIndirect = InventoryComponent->GetItemForTaggedSlot(RightHandSlot);
        FTaggedItemBundle LeftKnifeIndirect = InventoryComponent->GetItemForTaggedSlot(LeftHandSlot);
        UItemInstanceData* RightInstancePtrIndirect = RightKnifeIndirect.InstanceData.Num() > 0 ? RightKnifeIndirect.InstanceData[0] : nullptr;
        UItemInstanceData* LeftInstancePtrIndirect = LeftKnifeIndirect.InstanceData.Num() > 0 ? LeftKnifeIndirect.InstanceData[0] : nullptr;

        TArray<UItemInstanceData*> InstancesToDestroy = { RightInstancePtrIndirect };
        InventoryComponent->DestroyItem_IfServer(ItemIdBrittleCopperKnife, 1, InstancesToDestroy, EItemChangeReason::ForceDestroyed);

        Res &= Test->TestFalse(TEXT("[Indirect][Instance] Right hand slot should be empty after specific destroy"), InventoryComponent->GetItemForTaggedSlot(RightHandSlot).IsValid());
        Res &= Test->TestTrue(TEXT("[Indirect][Instance] Left hand slot should still have knife"), InventoryComponent->GetItemForTaggedSlot(LeftHandSlot).IsValid());
        Res &= Test->TestEqual(TEXT("[Indirect][Instance] Total knife quantity should be 1"), InventoryComponent->GetQuantityTotal_Implementation(ItemIdBrittleCopperKnife), 1);
        if(RightInstancePtrIndirect) Res &= Test->TestFalse(TEXT("[Indirect][Instance] Destroyed instance unregistered"), Context.TempActor->IsReplicatedSubObjectRegistered(RightInstancePtrIndirect));
        if(LeftInstancePtrIndirect) Res &= Test->TestTrue(TEXT("[Indirect][Instance] Remaining instance registered"), Context.TempActor->IsReplicatedSubObjectRegistered(LeftInstancePtrIndirect));

        return Res;
    }
};

bool FRancInventoryComponentTest::RunTest(const FString& Parameters)
{
	FDebugTestResult Res = true;
	FInventoryComponentTestScenarios TestScenarios(this);
	Res &= TestScenarios.TestAddingTaggedSlotItems();
	Res &= TestScenarios.TestAddItem();
	Res &= TestScenarios.TestAddItemToAnySlot();
	Res &= TestScenarios.TestRemovingTaggedSlotItems();
	Res &= TestScenarios.TestRemoveAnyItemFromTaggedSlot();
	Res &= TestScenarios.TestMoveTaggedSlotItems();
	Res &= TestScenarios.TestMoveOperationsWithSwapback();
	Res &= TestScenarios.TestDroppingFromTaggedSlot();
	Res &= TestScenarios.TestExclusiveUniversalSlots();
	Res &= TestScenarios.TestBlockingSlots();
	Res &= TestScenarios.TestEventBroadcasting();
	Res &= TestScenarios.TestIndirectOperations();
	Res &= TestScenarios.TestCanCraftRecipe();
	Res &= TestScenarios.TestInventoryMaxCapacity();
	Res &= TestScenarios.TestReceivableQuantity();

	// Tests to add:
	//  * AddItemToTaggedSlot_IfServer with pushoutexistingitem = true
	//  * Move item with swapback that violates indirect blocking
	//  * RequestMoveItemToOtherContainer - include case where generic slots are full but there is space in tagged slots
	
	return Res;
}
