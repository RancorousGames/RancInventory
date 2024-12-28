// WeaponAttackRecorderSettings.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponAttackRecorderSettings.generated.h"

UCLASS(BlueprintType)
class RANCINVENTORY_API UWeaponAttackRecorderSettings : public UDataAsset
{
    GENERATED_BODY()

public:
    // Number of timestamps to record (minimum 2 for start and end)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording", meta = (ClampMin = "2"))
    int32 Resolution = 3;

    // Prefix for socket names to record
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording")
    FString SocketPrefix = "Weapon_";

    // Whether to overwrite existing records or create new versions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording")
    bool bOverwriteExisting = false;

    // Folder path to save new assets
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording", meta = (RelativeToGameContentDir))
    FDirectoryPath AssetSavePath;
};