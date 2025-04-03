#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "RISWeaponsDataTypes.generated.h"

class UWeaponAttackData;

UENUM(BlueprintType)
enum class EHandCompatibility : uint8
{
    /** Compatible with both hands */
    BothHands,
    OnlyMainHand,
    TwoHanded,
    TwoHandedOffhand,
    OnlyOffhand,
    /** Not compatible with any hand */
    None
};

USTRUCT(BlueprintType)
struct FAttackMontageData2
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    UAnimMontage* Montage = nullptr;

};

USTRUCT(BlueprintType)
struct FAttackMontageData3
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    TObjectPtr<UAnimMontage> Montage = nullptr;

};

USTRUCT(BlueprintType)
struct FAttackMontageData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    UAnimMontage* Montage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    float PlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    TSoftObjectPtr<UWeaponAttackData> RecordedTraceSequence;

    bool IsValid() const
    {
        return Montage != nullptr;
    }
};

USTRUCT(BlueprintType)
struct FMontageData
{
    GENERATED_BODY()

    // Note: Always loaded in memory, unlike attack montages
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    float PlayRate = 1.0f;
    
    bool IsValid() const
    {
        return Montage != nullptr;
    }
};