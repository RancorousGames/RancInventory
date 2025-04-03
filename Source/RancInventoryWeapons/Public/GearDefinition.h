#pragma once

#include "CoreMinimal.h"
#include "RISWeaponsDataTypes.h"
#include "Data/ItemDefinitionBase.h"

#include "GearDefinition.generated.h"

/**
 * UGearDefinition - Data definition class for equipment-specific data.
 */
UCLASS(Blueprintable, DefaultToInstanced, EditInlineNew, Category = "RIS | Classes | Data")
class RANCINVENTORYWEAPONS_API UGearDefinition : public UItemDefinitionBase
{
    GENERATED_BODY()

public:
    explicit UGearDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
    {
    }
    /** Animation montage data for equipping the weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gear", meta = (AssetBundles = "Data"))
    FMontageData EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gear", meta = (AssetBundles = "Data"))
	FMontageData HolsterMontage;
};