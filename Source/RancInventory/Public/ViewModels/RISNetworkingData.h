// Copyright Rancorous Games, 2024

#pragma once
#include "GameplayTagContainer.h"
#include <CoreMinimal.h>
#include "Management/RISInventoryData.h"
#include "RISNetworkingData.generated.h"


USTRUCT(BlueprintType)
struct FVersionedItemInstanceArray
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	TArray<FItemBundle> Items;

	FVersionedItemInstanceArray() = default;
	FVersionedItemInstanceArray(int32 InVersion, const TArray<FItemBundle>& InItems)
		: Version(InVersion), Items(InItems) { }
};

UENUM()
enum RISSlotOperation
{
	Add,
	AddTagged,
	Remove,
	RemoveTagged,
};

USTRUCT()
struct FRISExpectedOperation
{
	GENERATED_BODY()

	RISSlotOperation Operation = RISSlotOperation::Add;
	FGameplayTag TaggedSlot = FGameplayTag();
	FGameplayTag ItemId = FGameplayTag();
	int32 Quantity = 0;

	// Constructor for tagged operations
	FRISExpectedOperation(RISSlotOperation InOperation, FGameplayTag InTaggedSlot, FGameplayTag InItemID, int32 InQuantity)
		: Operation(InOperation), TaggedSlot(InTaggedSlot), ItemId(InItemID), Quantity(InQuantity) { }

	// Constructor for non-tagged operations
	FRISExpectedOperation(RISSlotOperation InOperation, FGameplayTag InItemID, int32 InQuantity)
		: Operation(InOperation), ItemId(InItemID), Quantity(InQuantity) { }

	// Default constructor
	FRISExpectedOperation() = default;
};

