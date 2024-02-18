/*
 * To learn about tests I used these resources:
 * https://dev.to/goals/unit-testing-private-functionality-in-unreal-engine-c-classes-2fig
 * https://github.com/ibbles/LearningUnrealEngine/blob/master/Unit%20tests%20source%20notes.md
 * https://www.jetbrains.com/help/rider/Unreal_Engine__Tests.html
 * https://www.youtube.com/watch?v=erNW3r9uRH0
 */


#include "SlotMapperTest.h" // header not needed

#include "NativeGameplayTags.h"
#include "Components/RancInventoryComponent.h"
#include "Engine/AssetManager.h"
#include "Management/RancInventoryFunctions.h"
#include "Misc/AutomationTest.h"
#include "Management/RancInventorySlotMapper.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlotMapperTest, "GameTests.SlotMapper.BasicTests", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

FSlotMapperTest* SMTest;


#define SETUP_SLOTMAPPER(MaxItems, CarryCapacity) \
    URancInventoryComponent* InventoryComponent = NewObject<URancInventoryComponent>(); \
    InventoryComponent->UniversalTaggedSlots.Add(LeftHandSlot.GetTag()); \
    InventoryComponent->UniversalTaggedSlots.Add(RightHandSlot.GetTag()); \
    InventoryComponent->SpecializedTaggedSlots.Add(HelmetSlot.GetTag()); \
    InventoryComponent->SpecializedTaggedSlots.Add(ChestSlot.GetTag()); \
    InventoryComponent->MaxNumItems = MaxItems; \
    InventoryComponent->MaxWeight = CarryCapacity; \
    URancInventorySlotMapper* SlotMapper = NewObject<URancInventorySlotMapper>(); \
    SlotMapper->Initialize(InventoryComponent, 9, false, false);

/*

bool TestAddingUnstackableItemX()
{
    SETUP_SLOTMAPPER(9, 100);
   // InventoryComponent->AddItems_IfServer(FRancItemInstance::EmptyItemInstance);
 //   SlotMapper->AddItems(FRancItemInstance(UnstackableItem1.GetTag(), 1));
    SlotMapper->DropItem(FGameplayTag(), 0, 1);
   // return Test->TestTrue("Unstackable Item should be in the first slot", SlotMapper->GetItem(0).ItemId.IsValid());
    return true;
}
bool TestAddingStackableItems()
{
    SETUP_SLOTMAPPER(9, 100);
    auto Item1Data = URancInventoryFunctions::GetItemDataById(StackableItem1.GetTag());

    // Add items below the max stack size and verify it's added correctly.
    int32 belowMaxStack = Item1Data->MaxStackSize - 1;
    int32 addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), belowMaxStack));
    bool test1 = Test->TestEqual("Adding below max stack should succeed with no remainder", addResult, 0);
    FRancItemInstance itemInSlot = SlotMapper->GetItem(0);
    bool test2 = Test->TestEqual("Slot 0 should have correct quantity below max stack", itemInSlot.Quantity, belowMaxStack);

    // Add items to reach max stack size.
    addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), 1));
    itemInSlot = SlotMapper->GetItem(0);
    bool test3 = Test->TestEqual("Adding to max stack should succeed with no remainder", addResult, 0);
    bool test4 = Test->TestEqual("Slot 0 should now be at max stack size", itemInSlot.Quantity, Item1Data->MaxStackSize);

    // Attempt to add more items than max stack size in a single operation.
   //int32 aboveMaxStackAdd = Item1Data->MaxStackSize + 5;
   //addResult = SlotMapper->AddItems(FRancItemInstance(StackableItem1.GetTag(), aboveMaxStackAdd));
   //bool test5 = Test->TestEqual("Adding above max stack should return correct remainder", addResult, 5);

   //// Ensure no overflow to other slots when adding above max stack in initial add.
   //bool overflowTest = true;
   //for (int32 i = 2; i < 9; ++i) {
   //    overflowTest &= SlotMapper->IsSlotEmpty(i);
   //}
   //bool test6 = Test->TestTrue("No overflow to other slots for initial above max stack add", overflowTest);

    return test1 && test2 && test3 && test4;// && test5 && test6;
}

bool TestItemSwapBetweenSlots()
{
    SETUP_SLOTMAPPER(9, 100);
    SlotMapper->AddItemToSlot(FRancItemInstance(StackableItem1.GetTag(), 1), 1);
    SlotMapper->AddItemToSlot(FRancItemInstance(StackableItem2.GetTag(), 1), 5);
    SlotMapper->MoveItem(FGameplayTag(), 1, FGameplayTag(), 5);
    bool test1 = Test->TestTrue("After moving, slot 1 should have StackableItem2.", SlotMapper->GetItem(1).ItemId.MatchesTag(StackableItem2));
    bool test2 = Test->TestTrue("After moving, slot 5 should have StackableItem1.", SlotMapper->GetItem(5).ItemId.MatchesTag(StackableItem1));
    return test1 && test2;
}

bool TestTaggedSlotOperations()
{
    SETUP_SLOTMAPPER(9, 100);
    SlotMapper->AddItemToTaggedSlot(FRancItemInstance(StackableItem1.GetTag(), 1), LeftHandSlot);
    bool test1 = Test->TestFalse("Left hand slot should not be empty after adding an item.", SlotMapper->IsTaggedSlotEmpty(LeftHandSlot));
    SlotMapper->RemoveItemFromTaggedSlot(LeftHandSlot, MAX_int32);
    bool test2 = Test->TestTrue("Left hand slot should be empty after removing the item.", SlotMapper->IsTaggedSlotEmpty(LeftHandSlot));
    SlotMapper->AddItemToTaggedSlot(FRancItemInstance(StackableItem1.GetTag(), 1), LeftHandSlot);
    SlotMapper->MoveItem(LeftHandSlot, -1, RightHandSlot, -1);
    bool test3 = Test->TestTrue("Right hand slot should not be empty after moving the item.", SlotMapper->IsTaggedSlotEmpty(RightHandSlot) == false);
    bool test4 = Test->TestTrue("Left hand slot should be empty after moving the item.", SlotMapper->IsTaggedSlotEmpty(LeftHandSlot));
    return test1 && test2 && test3 && test4;
}

bool TestDroppingPartialStack()
{
    SETUP_SLOTMAPPER(9, 100);
    SlotMapper->AddItemToTaggedSlot(FRancItemInstance(StackableItem1.GetTag(), 5), LeftHandSlot);
    const int32 droppedQuantity = 3;
    SlotMapper->DropItem(LeftHandSlot, -1, droppedQuantity);
    FRancTaggedItemInstance remainingItem = SlotMapper->GetItemForTaggedSlot(LeftHandSlot);
    return Test->TestEqual("Remaining quantity should match expected value after dropping part of the stack.", remainingItem.ItemInstance.Quantity, 2);
}

bool TestAddingItemsBeyondCapacity()
{
    SETUP_SLOTMAPPER(9, 100);
    FRancItemInstance extraItem(StackableItem2.GetTag(), 100);
    const int32 addResult = SlotMapper->AddItems(extraItem);
    return Test->TestTrue("Adding items beyond inventory capacity should fail.", addResult > 0);
}*/

bool FSlotMapperTest::RunTest(const FString& Parameters)
{
    if (UAssetManager* const AssetManager = UAssetManager::GetIfInitialized())
    {
        if (const TSharedPtr<FStreamableHandle> StreamableHandle = AssetManager->LoadPrimaryAssetsWithType(TEXT("RancInventory_ItemData")))
        {
            StreamableHandle->WaitUntilComplete(5.f);
        }
    }

    URancInventoryFunctions::AllItemsLoadedCallback();
    
    SMTest = this;
   //bool test1 = TestAddingUnstackableItem();
   //bool test2 = TestAddingStackableItems();
   //bool test3 = TestItemSwapBetweenSlots();
   //bool test4 = TestTaggedSlotOperations();
   //bool test5 = TestDroppingPartialStack();
   //bool test6 = TestAddingItemsBeyondCapacity();
    
    return true;//test1 && test2 && test3 && test4 && test5 && test6;
}