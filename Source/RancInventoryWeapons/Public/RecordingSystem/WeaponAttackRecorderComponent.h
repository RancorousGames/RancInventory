// Copyright Rancorous Games, 2024

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "WeaponAttackRecorderSettings.h"
#include "WeaponAttackData.h"

#include "WeaponAttackRecorderComponent.generated.h"

class USkeletalMeshComponent;
class AWeaponActor;

USTRUCT(BlueprintType)
struct FMontageData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    TSoftObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    float PlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranc Inventory | Weapon")
    TSoftObjectPtr<UWeaponAttackData> RecordedTraceSequence;

    bool IsValid() const
    {
       return Montage.IsValid();
    }
};

USTRUCT()
struct FRecordingSession
{
    GENERATED_BODY()

    FMontageData* MontageData;
    TArray<FName> RelevantSockets;
    float AnimationDuration;
    float IntervalBetweenRecordings;
    int32 CurrentIndex;
    UWeaponAttackData* AttackData;
    FTimerHandle RecordingTimerHandle;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RANCINVENTORY_API UWeaponAttackRecorderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponAttackRecorderComponent();

    // Settings for the recording process
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording")
    UWeaponAttackRecorderSettings* Settings;

    // Folder path to save new assets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (RelativeToGameContentDir))
    FDirectoryPath AssetSavePath;

    // Clean up unused or outdated attack data assets
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void CleanupUnusedAttackData();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FRecordingSession CurrentSession;
    AWeaponActor* OwningWeapon;

    void Initialize();
    UFUNCTION()
    void OnAttackPerformed(FMontageData& MontageData);
    bool InitializeRecordingSession(FMontageData& MontageData);
    void RecordAttackData();
    void StopRecording();
    bool SaveAttackSequence(const FString& AssetName, UWeaponAttackData* AttackData);
    TArray<FName> FindRelevantSockets(UMeshComponent* WeaponMesh) const;
    bool ValidateAssetSavePath() const;
    FString GenerateUniqueAssetName(const FString& BaseName) const;
    void UpdateMontageDataWithRecordedSequence(FMontageData& MontageData, UWeaponAttackData* AttackData);
};