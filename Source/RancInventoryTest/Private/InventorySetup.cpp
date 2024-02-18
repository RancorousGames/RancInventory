#pragma once

#include "NativeGameplayTags.h"
#include "CoreMinimal.h"
#include "Management/RancInventoryFunctions.h"
#include "Management/RancInventoryData.h"

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

void InitializeTestItems()
{
	URancItemData* RockItemData = NewObject<URancItemData>();
	RockItemData->ItemId = ItemIdRock;
	RockItemData->ItemName = FName("Rock");
	RockItemData->ItemDescription = FText::FromString("A sturdy rock, useful for crafting and building.");
	RockItemData->ItemPrimaryType = ItemTypeResource;
	RockItemData->bIsStackable = true; 
	RockItemData->MaxStackSize = 5; 
	RockItemData->ItemCategories.AddTag(ItemTypeResource); 
	URancInventoryFunctions::HardcodeItem(ItemIdRock, RockItemData);
	
	URancItemData* SticksItemData = NewObject<URancItemData>();
	SticksItemData->ItemId = ItemIdRock;
	SticksItemData->ItemName = FName("Sticks");
	SticksItemData->ItemDescription = FText::FromString("Some sticks");
	SticksItemData->ItemPrimaryType = ItemTypeResource;
	SticksItemData->bIsStackable = true; 
	SticksItemData->MaxStackSize = 5; 
	SticksItemData->ItemCategories.AddTag(ItemTypeResource); 
	URancInventoryFunctions::HardcodeItem(ItemIdSticks, SticksItemData);

	URancItemData* HelmetItemData = NewObject<URancItemData>();
	HelmetItemData->ItemId = ItemIdHelmet;
	HelmetItemData->ItemName = FName("Helmet");
	HelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	HelmetItemData->ItemPrimaryType = ItemTypeArmor; 
	HelmetItemData->bIsStackable = false;
	HelmetItemData->MaxStackSize = 1;
	HelmetItemData->ItemCategories.AddTag(HelmetSlot); 
	URancInventoryFunctions::HardcodeItem(ItemIdHelmet, HelmetItemData);

	URancItemData* SpearItemData = NewObject<URancItemData>();
	SpearItemData->ItemId = ItemIdSpear;
	SpearItemData->ItemName = FName("Spear");
	SpearItemData->ItemDescription = FText::FromString("Sharp!");
	SpearItemData->ItemPrimaryType = ItemTypeWeapon; 
	SpearItemData->bIsStackable = false;
	SpearItemData->MaxStackSize = 5; // should not matter
 	SpearItemData->ItemCategories.AddTag(ItemTypeWeapon); 
	URancInventoryFunctions::HardcodeItem(ItemIdSpear, SpearItemData);
}
