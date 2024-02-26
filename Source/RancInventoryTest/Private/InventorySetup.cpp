#pragma once

#include "NativeGameplayTags.h"
#include "CoreMinimal.h"
#include "Management/RancInventoryFunctions.h"
#include "Management/RancInventoryData.h"
#include "Components/RancInventoryComponent.h"

UE_DEFINE_GAMEPLAY_TAG(LeftHandSlot, "Hands.LeftHand");
UE_DEFINE_GAMEPLAY_TAG(RightHandSlot, "Hands.RightHand");
UE_DEFINE_GAMEPLAY_TAG(HelmetSlot, "Slots.Helmet");
UE_DEFINE_GAMEPLAY_TAG(ChestSlot, "Slots.Chest");

UE_DEFINE_GAMEPLAY_TAG(ItemTypeResource, "Items.Types.Resource");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeArmor, "Items.Types.Armor");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeWeapon, "Items.Types.Weapon");

UE_DEFINE_GAMEPLAY_TAG(ItemIdRock, "Items.IDs.Rock");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSticks, "Items.IDs.Sticks");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSpear, "Items.IDs.StoneSpear");
UE_DEFINE_GAMEPLAY_TAG(ItemIdHelmet, "Items.IDs.Helmet");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSpecialHelmet, "Items.IDs.SpecialHelmet");
UE_DEFINE_GAMEPLAY_TAG(ItemIdChestArmor, "Items.IDs.ChestArmor");
UE_DEFINE_GAMEPLAY_TAG(ItemIdGiantBoulder, "Items.IDs.GiantBoulder");


FRancItemInstance OneSpear(ItemIdSpear, 1);
FRancItemInstance OneRock(ItemIdRock, 1);
FRancItemInstance TwoRocks(ItemIdRock, 2);
FRancItemInstance ThreeRocks(ItemIdRock, 3);
FRancItemInstance FourRocks(ItemIdRock, 4);
FRancItemInstance FiveRocks(ItemIdRock, 5);
FRancItemInstance ThreeSticks(ItemIdSticks, 3);
FRancItemInstance OneHelmet(ItemIdHelmet, 1);
FRancItemInstance OneSpecialHelmet(ItemIdSpecialHelmet, 1);
FRancItemInstance OneChestArmor(ItemIdChestArmor, 1);
FRancItemInstance GiantBoulder(ItemIdGiantBoulder, 1);
FGameplayTag NoTag = FGameplayTag::EmptyTag;

void InitializeTestItems()
{
	URancItemData* RockItemData = NewObject<URancItemData>();
	RockItemData->ItemId = ItemIdRock;
	RockItemData->ItemName = FName("Rock");
	RockItemData->ItemDescription = FText::FromString("A sturdy rock, useful for crafting and building.");
	RockItemData->ItemPrimaryType = ItemTypeResource;
	RockItemData->bIsStackable = true;
	RockItemData->MaxStackSize = 5;
	RockItemData->ItemWeight = 1;
	RockItemData->ItemCategories.AddTag(ItemTypeResource);
	URancInventoryFunctions::HardcodeItem(ItemIdRock, RockItemData);

	URancItemData* SticksItemData = NewObject<URancItemData>();
	SticksItemData->ItemId = ItemIdRock;
	SticksItemData->ItemName = FName("Sticks");
	SticksItemData->ItemDescription = FText::FromString("Some sticks");
	SticksItemData->ItemPrimaryType = ItemTypeResource;
	SticksItemData->bIsStackable = true;
	SticksItemData->MaxStackSize = 5;
	SticksItemData->ItemWeight = 1;
	SticksItemData->ItemCategories.AddTag(ItemTypeResource);
	URancInventoryFunctions::HardcodeItem(ItemIdSticks, SticksItemData);

	URancItemData* HelmetItemData = NewObject<URancItemData>();
	HelmetItemData->ItemId = ItemIdHelmet;
	HelmetItemData->ItemName = FName("Helmet");
	HelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	HelmetItemData->ItemPrimaryType = ItemTypeArmor;
	HelmetItemData->bIsStackable = false;
	HelmetItemData->MaxStackSize = 1;
	HelmetItemData->ItemWeight = 2;
	HelmetItemData->ItemCategories.AddTag(HelmetSlot);
	URancInventoryFunctions::HardcodeItem(ItemIdHelmet, HelmetItemData);
	
	URancItemData* SpecialHelmetItemData = NewObject<URancItemData>();
	SpecialHelmetItemData->ItemId = ItemIdSpecialHelmet;
	SpecialHelmetItemData->ItemName = FName("SpecialHelmet");
	SpecialHelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	SpecialHelmetItemData->ItemPrimaryType = ItemTypeArmor;
	SpecialHelmetItemData->bIsStackable = false;
	SpecialHelmetItemData->MaxStackSize = 1;
	SpecialHelmetItemData->ItemWeight = 2;
	SpecialHelmetItemData->ItemCategories.AddTag(HelmetSlot);
	URancInventoryFunctions::HardcodeItem(ItemIdSpecialHelmet, SpecialHelmetItemData);

	URancItemData* ChestItemData = NewObject<URancItemData>();
	ChestItemData->ItemId = ItemIdChestArmor;
	ChestItemData->ItemName = FName("Chest Armor");
	ChestItemData->ItemDescription = FText::FromString("Armor protecting the torso.");
	ChestItemData->ItemPrimaryType = ItemTypeArmor;
	ChestItemData->bIsStackable = false;
	ChestItemData->MaxStackSize = 1;
	ChestItemData->ItemWeight = 5; // Adjusted weight for chest armor
	ChestItemData->ItemCategories.AddTag(ChestSlot); // Adjust for chest slot
	URancInventoryFunctions::HardcodeItem(ItemIdChestArmor, ChestItemData);

	URancItemData* SpearItemData = NewObject<URancItemData>();
	SpearItemData->ItemId = ItemIdSpear;
	SpearItemData->ItemName = FName("Spear");
	SpearItemData->ItemDescription = FText::FromString("Sharp!");
	SpearItemData->ItemPrimaryType = ItemTypeWeapon;
	SpearItemData->bIsStackable = false;
	SpearItemData->MaxStackSize = 5; // should not matter
	SpearItemData->ItemWeight = 3;
	SpearItemData->ItemCategories.AddTag(ItemTypeWeapon);
	URancInventoryFunctions::HardcodeItem(ItemIdSpear, SpearItemData);


	URancItemData* GiantBoulderItemData = NewObject<URancItemData>();
	GiantBoulderItemData->ItemId = ItemIdGiantBoulder;
	GiantBoulderItemData->ItemName = FName("Giant Boulder");
	GiantBoulderItemData->ItemDescription = FText::FromString("HEAVY!");
	GiantBoulderItemData->ItemPrimaryType = ItemTypeResource;
	GiantBoulderItemData->bIsStackable = false;
	GiantBoulderItemData->MaxStackSize = 1;
	GiantBoulderItemData->ItemWeight = 10;
	GiantBoulderItemData->ItemCategories.AddTag(ItemTypeResource);
	URancInventoryFunctions::HardcodeItem(ItemIdGiantBoulder, GiantBoulderItemData);
}
