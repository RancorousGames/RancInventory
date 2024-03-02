// Copyright Rancorous Games, 2024

#pragma once

#include "NativeGameplayTags.h"
#include "CoreMinimal.h"
#include "..\..\RancInventory\Public\Management\RISInventoryFunctions.h"
#include "..\..\RancInventory\Public\Management\RISInventoryData.h"
#include "..\..\RancInventory\Public\Components\RISInventoryComponent.h"

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


FRISItemInstance OneSpear(ItemIdSpear, 1);
FRISItemInstance OneRock(ItemIdRock, 1);
FRISItemInstance TwoRocks(ItemIdRock, 2);
FRISItemInstance ThreeRocks(ItemIdRock, 3);
FRISItemInstance FourRocks(ItemIdRock, 4);
FRISItemInstance FiveRocks(ItemIdRock, 5);
FRISItemInstance ThreeSticks(ItemIdSticks, 3);
FRISItemInstance OneHelmet(ItemIdHelmet, 1);
FRISItemInstance OneSpecialHelmet(ItemIdSpecialHelmet, 1);
FRISItemInstance OneChestArmor(ItemIdChestArmor, 1);
FRISItemInstance GiantBoulder(ItemIdGiantBoulder, 1);
FGameplayTag NoTag = FGameplayTag::EmptyTag;

void InitializeTestItems()
{
	URISItemData* RockItemData = NewObject<URISItemData>();
	RockItemData->ItemId = ItemIdRock;
	RockItemData->ItemName = FName("Rock");
	RockItemData->ItemDescription = FText::FromString("A sturdy rock, useful for crafting and building.");
	RockItemData->ItemPrimaryType = ItemTypeResource;
	RockItemData->bIsStackable = true;
	RockItemData->MaxStackSize = 5;
	RockItemData->ItemWeight = 1;
	RockItemData->ItemCategories.AddTag(ItemTypeResource);
	URISInventoryFunctions::HardcodeItem(ItemIdRock, RockItemData);

	URISItemData* SticksItemData = NewObject<URISItemData>();
	SticksItemData->ItemId = ItemIdRock;
	SticksItemData->ItemName = FName("Sticks");
	SticksItemData->ItemDescription = FText::FromString("Some sticks");
	SticksItemData->ItemPrimaryType = ItemTypeResource;
	SticksItemData->bIsStackable = true;
	SticksItemData->MaxStackSize = 5;
	SticksItemData->ItemWeight = 1;
	SticksItemData->ItemCategories.AddTag(ItemTypeResource);
	URISInventoryFunctions::HardcodeItem(ItemIdSticks, SticksItemData);

	URISItemData* HelmetItemData = NewObject<URISItemData>();
	HelmetItemData->ItemId = ItemIdHelmet;
	HelmetItemData->ItemName = FName("Helmet");
	HelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	HelmetItemData->ItemPrimaryType = ItemTypeArmor;
	HelmetItemData->bIsStackable = false;
	HelmetItemData->MaxStackSize = 1;
	HelmetItemData->ItemWeight = 2;
	HelmetItemData->ItemCategories.AddTag(HelmetSlot);
	URISInventoryFunctions::HardcodeItem(ItemIdHelmet, HelmetItemData);
	
	URISItemData* SpecialHelmetItemData = NewObject<URISItemData>();
	SpecialHelmetItemData->ItemId = ItemIdSpecialHelmet;
	SpecialHelmetItemData->ItemName = FName("SpecialHelmet");
	SpecialHelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	SpecialHelmetItemData->ItemPrimaryType = ItemTypeArmor;
	SpecialHelmetItemData->bIsStackable = false;
	SpecialHelmetItemData->MaxStackSize = 1;
	SpecialHelmetItemData->ItemWeight = 2;
	SpecialHelmetItemData->ItemCategories.AddTag(HelmetSlot);
	URISInventoryFunctions::HardcodeItem(ItemIdSpecialHelmet, SpecialHelmetItemData);

	URISItemData* ChestItemData = NewObject<URISItemData>();
	ChestItemData->ItemId = ItemIdChestArmor;
	ChestItemData->ItemName = FName("Chest Armor");
	ChestItemData->ItemDescription = FText::FromString("Armor protecting the torso.");
	ChestItemData->ItemPrimaryType = ItemTypeArmor;
	ChestItemData->bIsStackable = false;
	ChestItemData->MaxStackSize = 1;
	ChestItemData->ItemWeight = 5; // Adjusted weight for chest armor
	ChestItemData->ItemCategories.AddTag(ChestSlot); // Adjust for chest slot
	URISInventoryFunctions::HardcodeItem(ItemIdChestArmor, ChestItemData);

	URISItemData* SpearItemData = NewObject<URISItemData>();
	SpearItemData->ItemId = ItemIdSpear;
	SpearItemData->ItemName = FName("Spear");
	SpearItemData->ItemDescription = FText::FromString("Sharp!");
	SpearItemData->ItemPrimaryType = ItemTypeWeapon;
	SpearItemData->bIsStackable = false;
	SpearItemData->MaxStackSize = 5; // should not matter
	SpearItemData->ItemWeight = 3;
	SpearItemData->ItemCategories.AddTag(ItemTypeWeapon);
	URISInventoryFunctions::HardcodeItem(ItemIdSpear, SpearItemData);


	URISItemData* GiantBoulderItemData = NewObject<URISItemData>();
	GiantBoulderItemData->ItemId = ItemIdGiantBoulder;
	GiantBoulderItemData->ItemName = FName("Giant Boulder");
	GiantBoulderItemData->ItemDescription = FText::FromString("HEAVY!");
	GiantBoulderItemData->ItemPrimaryType = ItemTypeResource;
	GiantBoulderItemData->bIsStackable = false;
	GiantBoulderItemData->MaxStackSize = 1;
	GiantBoulderItemData->ItemWeight = 10;
	GiantBoulderItemData->ItemCategories.AddTag(ItemTypeResource);
	URISInventoryFunctions::HardcodeItem(ItemIdGiantBoulder, GiantBoulderItemData);
}
