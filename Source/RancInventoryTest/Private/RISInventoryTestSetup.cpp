// Copyright Rancorous Games, 2024

#pragma once

#include "NativeGameplayTags.h"
#include "CoreMinimal.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Data/ItemBundle.h"
#include "Data/ItemStaticData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"

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

/*
FItemBundle OneSpear(ItemIdSpear, 1);
FItemBundle OneRock(ItemIdRock, 1);
FItemBundle TwoRocks(ItemIdRock, 2);
FItemBundle ThreeRocks(ItemIdRock, 3);
FItemBundle FourRocks(ItemIdRock, 4);
FItemBundle FiveRocks(ItemIdRock, 5);
FItemBundle ThreeSticks(ItemIdSticks, 3);
FItemBundle OneHelmet(ItemIdHelmet, 1);
FItemBundle OneSpecialHelmet(ItemIdSpecialHelmet, 1);
FItemBundle OneChestArmor(ItemIdChestArmor, 1);
FItemBundle GiantBoulder(ItemIdGiantBoulder, 1);*/
#define OneSpear ItemIdSpear, 1
#define OneRock ItemIdRock, 1
#define TwoRocks ItemIdRock, 2
#define ThreeRocks ItemIdRock, 3
#define FourRocks ItemIdRock, 4
#define FiveRocks ItemIdRock, 5
#define ThreeSticks ItemIdSticks, 3
#define OneHelmet ItemIdHelmet, 1
#define OneSpecialHelmet ItemIdSpecialHelmet, 1
#define OneChestArmor ItemIdChestArmor, 1
#define GiantBoulder ItemIdGiantBoulder, 1
FGameplayTag NoTag = FGameplayTag::EmptyTag;


static UWorld* CreateTestWorld()
{
	// Create a new world with default parameters
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
        
	// Initialize the world
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
        
	// Setup basic world settings
	AWorldSettings* WorldSettings = TestWorld->GetWorldSettings();
	if (WorldSettings)
	{
		WorldSettings->SetActorTickEnabled(true);
	}

	// Initialize the world's scene
	if (!TestWorld->bIsWorldInitialized)
	{
		TestWorld->InitializeNewWorld(UWorld::InitializationValues()
			.ShouldSimulatePhysics(false)
			.AllowAudioPlayback(false)
			.RequiresHitProxies(false)
			.CreatePhysicsScene(true)
			.CreateNavigation(false)
			.CreateAISystem(false));
	}

	return TestWorld;
}

UWorld* GetOrCreateWorld()
{
	static UWorld* PersistentWorld = nullptr;
    
	if (!PersistentWorld)
	{
		PersistentWorld = CreateTestWorld();
	}
    
	return PersistentWorld;
}

UWorld* FindWorld(const UObject* ContextObject)
{
	return GetOrCreateWorld();
}

URISSubsystem* SetupSubsystem()
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UWorld* World = FindWorld(nullptr);
	if (World)
	{
		World->SetGameInstance(GameInstance);
	}
	GameInstance->Init();
    
	return GameInstance->GetSubsystem<URISSubsystem>();
}

void InitializeTestItems()
{
	URISSubsystem* SubSystem = SetupSubsystem(); 
	
	UItemStaticData* RockItemData = NewObject<UItemStaticData>();
	RockItemData->ItemId = ItemIdRock;
	RockItemData->ItemName = FName("Rock");
	RockItemData->ItemDescription = FText::FromString("A sturdy rock, useful for crafting and building.");
	RockItemData->ItemPrimaryType = ItemTypeResource;
	RockItemData->MaxStackSize = 5;
	RockItemData->ItemWeight = 1;
	RockItemData->ItemCategories.AddTag(ItemTypeResource);
	SubSystem->HardcodeItem(ItemIdRock, RockItemData);

	UItemStaticData* SticksItemData = NewObject<UItemStaticData>();
	SticksItemData->ItemId = ItemIdSticks;
	SticksItemData->ItemName = FName("Sticks");
	SticksItemData->ItemDescription = FText::FromString("Some sticks");
	SticksItemData->ItemPrimaryType = ItemTypeResource;
	SticksItemData->MaxStackSize = 5;
	SticksItemData->ItemWeight = 1;
	SticksItemData->ItemCategories.AddTag(ItemTypeResource);
	SubSystem->HardcodeItem(ItemIdSticks, SticksItemData);

	UItemStaticData* HelmetItemData = NewObject<UItemStaticData>();
	HelmetItemData->ItemId = ItemIdHelmet;
	HelmetItemData->ItemName = FName("Helmet");
	HelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	HelmetItemData->ItemPrimaryType = ItemTypeArmor;
	HelmetItemData->MaxStackSize = 1;
	HelmetItemData->ItemWeight = 2;
	HelmetItemData->ItemCategories.AddTag(HelmetSlot);
	SubSystem->HardcodeItem(ItemIdHelmet, HelmetItemData);
	
	UItemStaticData* SpecialHelmetItemData = NewObject<UItemStaticData>();
	SpecialHelmetItemData->ItemId = ItemIdSpecialHelmet;
	SpecialHelmetItemData->ItemName = FName("SpecialHelmet");
	SpecialHelmetItemData->ItemDescription = FText::FromString("Protective gear for the head.");
	SpecialHelmetItemData->ItemPrimaryType = ItemTypeArmor;
	SpecialHelmetItemData->MaxStackSize = 1;
	SpecialHelmetItemData->ItemWeight = 2;
	SpecialHelmetItemData->ItemCategories.AddTag(HelmetSlot);
	SubSystem->HardcodeItem(ItemIdSpecialHelmet, SpecialHelmetItemData);

	UItemStaticData* ChestItemData = NewObject<UItemStaticData>();
	ChestItemData->ItemId = ItemIdChestArmor;
	ChestItemData->ItemName = FName("Chest Armor");
	ChestItemData->ItemDescription = FText::FromString("Armor protecting the torso.");
	ChestItemData->ItemPrimaryType = ItemTypeArmor;
	ChestItemData->MaxStackSize = 1;
	ChestItemData->ItemWeight = 5; // Adjusted weight for chest armor
	ChestItemData->ItemCategories.AddTag(ChestSlot); // Adjust for chest slot
	SubSystem->HardcodeItem(ItemIdChestArmor, ChestItemData);

	UItemStaticData* SpearItemData = NewObject<UItemStaticData>();
	SpearItemData->ItemId = ItemIdSpear;
	SpearItemData->ItemName = FName("Spear");
	SpearItemData->ItemDescription = FText::FromString("Sharp!");
	SpearItemData->ItemPrimaryType = ItemTypeWeapon;
	SpearItemData->MaxStackSize = 1;
	SpearItemData->ItemWeight = 3;
	SpearItemData->ItemCategories.AddTag(ItemTypeWeapon);
	SubSystem->HardcodeItem(ItemIdSpear, SpearItemData);


	UItemStaticData* GiantBoulderItemData = NewObject<UItemStaticData>();
	GiantBoulderItemData->ItemId = ItemIdGiantBoulder;
	GiantBoulderItemData->ItemName = FName("Giant Boulder");
	GiantBoulderItemData->ItemDescription = FText::FromString("HEAVY!");
	GiantBoulderItemData->ItemPrimaryType = ItemTypeResource;
	GiantBoulderItemData->MaxStackSize = 1;
	GiantBoulderItemData->ItemWeight = 10;
	GiantBoulderItemData->ItemCategories.AddTag(ItemTypeResource);
	SubSystem->HardcodeItem(ItemIdGiantBoulder, GiantBoulderItemData);
}
