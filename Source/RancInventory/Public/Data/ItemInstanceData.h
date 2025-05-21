// Copyright Rancorous Games, 2024
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ItemInstanceData.generated.h"

class AWorldItem;
class UItemContainerComponent;

/**
 * Abstract base class for replicated item instance data.
 */
UCLASS(Blueprintable, Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class RANCINVENTORY_API UItemInstanceData : public UObject
{
	GENERATED_BODY()

public:
	// Constructor
	UItemInstanceData(){}
	virtual ~UItemInstanceData() override {}

	// This function is called when the item instance data is created with a reference to its initial owner
	// AND it is called whenever ownership is changed
	UFUNCTION(BlueprintNativeEvent, Category = "RIS")
	void Initialize(bool OwnedByComponent, AWorldItem* OwningWorldItem, UItemContainerComponent* OwningContainer);
	
	UFUNCTION(BlueprintNativeEvent, Category = "RIS")
	void OnDestroy();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* Implement this function to determine which item should be removed first from a stack of items
	 * Example: A stack of apples, when the user eats an apple, this function will return the one closest to turning foul
	 * It might first check if any of the apples are already foul, and return that one assuming that the caller is a system responsible for calling remove on foul apples
	 * By Default it will pick using the First In Last Out method
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ranc Inventory")
	UItemInstanceData* PickInstanceToRemove(const TArray<UItemInstanceData*>& StateInstances);

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RIS")
	int32 UniqueInstanceId = 0;
};
