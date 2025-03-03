// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "ItemDurabilityTestInstanceData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "NativeGameplayTags.h"
#include "WeaponDefinition.h"
#include "Core/RISFunctions.h"
#include "Core/RISSubsystem.h"
#include "Data/ItemBundle.h"
#include "Data/ItemStaticData.h"

// Define Gameplay Tags
UE_DEFINE_GAMEPLAY_TAG(LeftHandSlot, "Test.Gameplay.Hands.LeftHand");
UE_DEFINE_GAMEPLAY_TAG(RightHandSlot, "Test.Gameplay.Hands.RightHand");
UE_DEFINE_GAMEPLAY_TAG(HelmetSlot, "Test.Gameplay.Slots.Helmet");
UE_DEFINE_GAMEPLAY_TAG(ChestSlot, "Test.Gameplay.Slots.Chest");

UE_DEFINE_GAMEPLAY_TAG(ItemTypeResource, "Test.Gameplay.Items.Types.Resource");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeArmor, "Test.Gameplay.Items.Types.Armor");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeTwoHanded, "Test.Gameplay.Items.Types.TwoHanded");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeTwoHandedOffhand, "Test.Gameplay.Items.Types.TwoHandedOffhand");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeOffHandOnly, "Test.Gameplay.Items.Types.OffHandOnly");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeMakeshiftWeapon, "Test.Gameplay.Items.Types.MakeshiftWeapon");
UE_DEFINE_GAMEPLAY_TAG(ItemTypeRangedWeapon, "Test.Gameplay.Items.Types.RangedWeapon");

UE_DEFINE_GAMEPLAY_TAG(ItemIdRock, "Test.Items.IDs.Rock");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSticks, "Test.Items.IDs.Sticks");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSpear, "Test.Items.IDs.StoneSpear");
UE_DEFINE_GAMEPLAY_TAG(ItemIdHelmet, "Test.Items.IDs.Helmet");
UE_DEFINE_GAMEPLAY_TAG(ItemIdSpecialHelmet, "Test.Items.IDs.SpecialHelmet");
UE_DEFINE_GAMEPLAY_TAG(ItemIdChestArmor, "Test.Items.IDs.ChestArmor");
UE_DEFINE_GAMEPLAY_TAG(ItemIdGiantBoulder, "Test.Items.IDs.GiantBoulder");
UE_DEFINE_GAMEPLAY_TAG(ItemIdBlockingOneHandedItem, "Test.Items.IDs.ItemIdBlockingOneHandedItem");
UE_DEFINE_GAMEPLAY_TAG(ItemIdBrittleCopperKnife, "Test.Items.IDs.BrittleCopperKnife");
UE_DEFINE_GAMEPLAY_TAG(ItemIdShortbow, "Test.Items.IDs.Shortbow");
UE_DEFINE_GAMEPLAY_TAG(ItemIdLongbow, "Test.Items.IDs.Longbow");

// Macros for commonly used item bundles
#define ONE_SPEAR ItemIdSpear, 1
#define ONE_ROCK ItemIdRock, 1
#define TWO_ROCKS ItemIdRock, 2
#define THREE_ROCKS ItemIdRock, 3
#define FOUR_ROCKS ItemIdRock, 4
#define FIVE_ROCKS ItemIdRock, 5
#define THREE_STICKS ItemIdSticks, 3
#define ONE_HELMET ItemIdHelmet, 1
#define ONE_SPECIAL_HELMET ItemIdSpecialHelmet, 1
#define ONE_CHEST_ARMOR ItemIdChestArmor, 1
#define GIANT_BOULDER ItemIdGiantBoulder, 1

#define OneSpear ItemIdSpear, 1
#define OneRock ItemIdRock, 1
#define TwoRocks ItemIdRock, 2
#define ThreeRocks ItemIdRock, 3
#define FourRocks ItemIdRock, 4
#define FiveRocks ItemIdRock, 5
#define OneStick ItemIdSticks, 1
#define ThreeSticks ItemIdSticks, 3
#define FiveSticks ItemIdSticks, 5
#define OneHelmet ItemIdHelmet, 1
#define OneSpecialHelmet ItemIdSpecialHelmet, 1
#define OneChestArmor ItemIdChestArmor, 1
#define GiantBoulder ItemIdGiantBoulder, 1
FGameplayTag NoTag = FGameplayTag::EmptyTag;

// Test fixture that sets up the persistent test world and subsystem
class FTestFixture
{
public:
	FTestFixture(FName TestName)
	{
		World = CreateTestWorld(TestName);
		Subsystem = EnsureSubsystem(World, TestName);
	}

	~FTestFixture()
	{
		// Clean up if needed; note that in many tests the engine will tear down the world.
	}

	UWorld* GetWorld() const { return World; }
	URISSubsystem* GetSubsystem() const { return Subsystem; }

	static bool AreGameplayTagsCorrupt()
	{
		// For some reason gameplaytags sometimes breaks and HasTag incorrectly returns false for tags that are present
		auto* ChestItemData = URISSubsystem::GetItemDataById(ItemIdChestArmor);
		if (ChestItemData->ItemCategories.HasTag(ChestSlot))
			return false;

		return true;
	}

	// Helper to initialize common test items
	void InitializeTestItems()
	{
		if (Subsystem->AreAllItemsLoaded()) return;
		
		UItemStaticData* RockData = NewObject<UItemStaticData>();
		RockData->ItemId = ItemIdRock;
		RockData->ItemName = FName("Rock");
		RockData->ItemDescription = FText::FromString("A sturdy rock, useful for crafting and building.");
		RockData->ItemPrimaryType = ItemTypeResource;
		RockData->MaxStackSize = 5;
		RockData->ItemWeight = 1;
		RockData->ItemCategories.AddTag(ItemTypeResource);
		Subsystem->HardcodeItem(ItemIdRock, RockData);

		UWeaponDefinition* MakeshiftWeaponDefinition = NewObject<UWeaponDefinition>();
		MakeshiftWeaponDefinition->WeaponType = ItemTypeMakeshiftWeapon;
		MakeshiftWeaponDefinition->Damage = 5;
		MakeshiftWeaponDefinition->Range = 1;
		MakeshiftWeaponDefinition->Cooldown = 1.0f;
		MakeshiftWeaponDefinition->HandCompatability = EHandCompatibility::BothHands;
		MakeshiftWeaponDefinition->IsLowPriority = true;
		RockData->ItemDefinitions.Add(MakeshiftWeaponDefinition);

		UItemStaticData* SticksData = NewObject<UItemStaticData>();
		SticksData->ItemId = ItemIdSticks;
		SticksData->ItemName = FName("Sticks");
		SticksData->ItemDescription = FText::FromString("Some sticks");
		SticksData->ItemPrimaryType = ItemTypeResource;
		SticksData->MaxStackSize = 5;
		SticksData->ItemWeight = 1;
		SticksData->ItemCategories.AddTag(ItemTypeResource);
		Subsystem->HardcodeItem(ItemIdSticks, SticksData);

		UItemStaticData* HelmetData = NewObject<UItemStaticData>();
		HelmetData->ItemId = ItemIdHelmet;
		HelmetData->ItemName = FName("Helmet");
		HelmetData->ItemDescription = FText::FromString("Protective gear for the head.");
		HelmetData->ItemPrimaryType = ItemTypeArmor;
		HelmetData->MaxStackSize = 1;
		HelmetData->ItemWeight = 2;
		HelmetData->ItemCategories.AddTag(HelmetSlot);
		Subsystem->HardcodeItem(ItemIdHelmet, HelmetData);

		UItemStaticData* SpecialHelmetData = NewObject<UItemStaticData>();
		SpecialHelmetData->ItemId = ItemIdSpecialHelmet;
		SpecialHelmetData->ItemName = FName("SpecialHelmet");
		SpecialHelmetData->ItemDescription = FText::FromString("Protective gear for the head.");
		SpecialHelmetData->ItemPrimaryType = ItemTypeArmor;
		SpecialHelmetData->MaxStackSize = 1;
		SpecialHelmetData->ItemWeight = 2;
		SpecialHelmetData->ItemCategories.AddTag(HelmetSlot);
		Subsystem->HardcodeItem(ItemIdSpecialHelmet, SpecialHelmetData);

		UItemStaticData* ChestData = NewObject<UItemStaticData>();
		ChestData->ItemId = ItemIdChestArmor;
		ChestData->ItemName = FName("Chest Armor");
		ChestData->ItemDescription = FText::FromString("Armor protecting the torso.");
		ChestData->ItemPrimaryType = ItemTypeArmor;
		ChestData->MaxStackSize = 1;
		ChestData->ItemWeight = 5;
		ChestData->ItemCategories.AddTag(ChestSlot);
		Subsystem->HardcodeItem(ItemIdChestArmor, ChestData);

		UItemStaticData* SpearData = NewObject<UItemStaticData>();
		SpearData->ItemId = ItemIdSpear;
		SpearData->ItemName = FName("Spear");
		SpearData->ItemDescription = FText::FromString("Sharp!");
		SpearData->ItemPrimaryType = ItemTypeTwoHanded;
		SpearData->MaxStackSize = 1;
		SpearData->ItemWeight = 3;
		SpearData->ItemCategories.AddTag(ItemTypeTwoHanded);
		SpearData->ItemCategories.AddTag(RightHandSlot); // Associate preference for right hand

		// Create and add a weapondefinition to the spear
		UWeaponDefinition* SpearWeaponDefinition = NewObject<UWeaponDefinition>();
		SpearWeaponDefinition->WeaponType = ItemTypeTwoHanded;
		SpearWeaponDefinition->Damage = 10;
		SpearWeaponDefinition->Range = 2;
		SpearWeaponDefinition->Cooldown = 1.0f;
		SpearWeaponDefinition->HandCompatability = EHandCompatibility::TwoHanded;
		SpearData->ItemDefinitions.Add(SpearWeaponDefinition);
		Subsystem->HardcodeItem(ItemIdSpear, SpearData);

		UItemStaticData* GiantBoulderData = NewObject<UItemStaticData>();
		GiantBoulderData->ItemId = ItemIdGiantBoulder;
		GiantBoulderData->ItemName = FName("Giant Boulder");
		GiantBoulderData->ItemDescription = FText::FromString("HEAVY!");
		GiantBoulderData->ItemPrimaryType = ItemTypeResource;
		GiantBoulderData->MaxStackSize = 1;
		GiantBoulderData->ItemWeight = 10;
		GiantBoulderData->ItemCategories.AddTag(ItemTypeResource);
		Subsystem->HardcodeItem(ItemIdGiantBoulder, GiantBoulderData);

		UItemStaticData* BrittleCopperKnife = NewObject<UItemStaticData>();
		BrittleCopperKnife->ItemId = ItemIdBrittleCopperKnife;
		BrittleCopperKnife->ItemName = FName("Brittle Copper Knife");
		BrittleCopperKnife->ItemDescription = FText::FromString("Careful!");
		BrittleCopperKnife->ItemPrimaryType = ItemTypeMakeshiftWeapon;
		BrittleCopperKnife->MaxStackSize = 1;
		BrittleCopperKnife->ItemWeight = 3;
		BrittleCopperKnife->ItemCategories.AddTag(ItemTypeMakeshiftWeapon);
		BrittleCopperKnife->ItemDefinitions.Add(MakeshiftWeaponDefinition);
		BrittleCopperKnife->ItemCategories.AddTag(LeftHandSlot);
		BrittleCopperKnife->ItemCategories.AddTag(RightHandSlot);
		BrittleCopperKnife->ItemInstanceDataClass = UItemDurabilityTestInstanceData::StaticClass();

		UItemStaticData* ShortbowData = NewObject<UItemStaticData>();
		ShortbowData->ItemId = ItemIdShortbow;
		ShortbowData->ItemName = FName("Shortbow");
		ShortbowData->ItemDescription = FText::FromString("A Shortbow that must be held in offhand.");
		ShortbowData->ItemPrimaryType = ItemTypeRangedWeapon;
		ShortbowData->MaxStackSize = 1;
		ShortbowData->ItemWeight = 3;
		ShortbowData->ItemCategories.AddTag(ItemTypeOffHandOnly);
		ShortbowData->ItemCategories.AddTag(LeftHandSlot);

		UWeaponDefinition* ShortBowWeaponDefinition = NewObject<UWeaponDefinition>();
		ShortBowWeaponDefinition->WeaponType = ItemTypeRangedWeapon;
		ShortBowWeaponDefinition->Damage = 10;
		ShortBowWeaponDefinition->Range = 20;
		ShortBowWeaponDefinition->Cooldown = 1.0f;
		ShortBowWeaponDefinition->HandCompatability = EHandCompatibility::OnlyOffhand;
		ShortbowData->ItemDefinitions.Add(ShortBowWeaponDefinition);
		Subsystem->HardcodeItem(ItemIdShortbow, ShortbowData);

		
		UItemStaticData* LongbowData = NewObject<UItemStaticData>();
		LongbowData->ItemId = ItemIdLongbow;
		LongbowData->ItemName = FName("Longbow");
		LongbowData->ItemDescription = FText::FromString("A longbow that must be held in left hand and uses both hands");
		LongbowData->ItemPrimaryType = ItemTypeRangedWeapon;
		LongbowData->MaxStackSize = 1;
		LongbowData->ItemWeight = 3;
		LongbowData->ItemCategories.AddTag(ItemTypeOffHandOnly);
		LongbowData->ItemCategories.AddTag(ItemTypeTwoHandedOffhand);
		LongbowData->ItemCategories.AddTag(LeftHandSlot);

		UWeaponDefinition* LongbowWeaponDefinition = NewObject<UWeaponDefinition>();
		LongbowWeaponDefinition->WeaponType = ItemTypeRangedWeapon;
		LongbowWeaponDefinition->Damage = 10;
		LongbowWeaponDefinition->Range = 20;
		LongbowWeaponDefinition->Cooldown = 1.0f;
		LongbowWeaponDefinition->HandCompatability = EHandCompatibility::TwoHandedOffhand;
		LongbowData->ItemDefinitions.Add(LongbowWeaponDefinition);
		Subsystem->HardcodeItem(ItemIdLongbow, LongbowData);

		Subsystem->HardcodeItem(ItemIdBrittleCopperKnife, BrittleCopperKnife);
	}

private:
	static UWorld* CreateTestWorld(const FName& WorldName)
	{
		UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName);
		if (GEngine)
		{
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(TestWorld);
		}

		AWorldSettings* WorldSettings = TestWorld->GetWorldSettings();
		if (WorldSettings)
		{
			WorldSettings->SetActorTickEnabled(true);
		}

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

		TestWorld->InitializeActorsForPlay(FURL());

		return TestWorld;
	}

	static URISSubsystem* EnsureSubsystem(UWorld* World, const FName& TestName)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("World is null in EnsureSubsystem."));
			return nullptr;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			GameInstance = NewObject<UGameInstance>(World);
			World->SetGameInstance(GameInstance);
			GameInstance->Init();
		}

		return GameInstance->GetSubsystem<URISSubsystem>();
	}

	UWorld* World = nullptr;
	URISSubsystem* Subsystem = nullptr;
};
