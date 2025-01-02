// Copyright Rancorous Games, 2024

#pragma once

#include "CoreMinimal.h"
#include "WeaponAttackRecorderDataTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimMontage.h"
#include "WeaponAttackRecorderSettings.h"

#include "WeaponAttackRecorderComponent.generated.h"

class USkeletalMeshComponent;
class AWeaponActor;


USTRUCT()
struct FRecordingSession
{
    GENERATED_BODY()

    FMontageData MontageData;
    TArray<FName> RelevantSockets;
    float AnimationDuration;
    float IntervalBetweenRecordings;
    float CurrentTime;
    int32 CurrentIndex;
    UWeaponAttackData* AttackData;
    FTimerHandle RecordingTimerHandle;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RANCINVENTORYWEAPONS_API UWeaponAttackRecorderComponent : public UActorComponent
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
    bool bIsRecording;
    bool RecordingInitialized;
    double RecordStartTime;
    int ReplayCurrentIndex;
    bool bReplaySlowmotion;
    FRecordingSession ReplayedSession;
    double ReplayStopTime;
    FTimerHandle ReplayTimerHandle;
    bool bReplayInitialOwnerPositionSaved;
    FTransform ReplayInitialOwnerPosition;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FRecordingSession CurrentSession;
    AWeaponActor* OwningWeapon;
    UGearManagerComponent* OwningGearManager;
    class UAnimInstance* OwningCharacterAnimInstance;
    
    void Initialize();
    void OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);
    void OnAnimNotifyBegin(FName AnimName);
    void OnAnimNotifyEnd(FName AnimName);
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);
    void OnAnimNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);
    UFUNCTION()
    void OnAttackPerformed(FMontageData MontageData);
    bool InitializeRecordingSession(FMontageData MontageData);
    void RecordAttackData(float DeltaTime);
    void StartRecording();
    void StopRecording();
    bool SaveAttackSequence(const FString& AssetName, UWeaponAttackData* AttackData);
    TArray<FWeaponAttackTimestamp> ReduceToKeyframesForSocket(const TArray<FWeaponAttackTimestamp>& OriginalSequence, int32 SocketIndex) const;
    void PostProcessRecordedData();

    TArray<FName> FindRelevantSockets(class UMeshComponent* WeaponMesh) const;
    bool ValidateAssetSavePath() const;
    FString GenerateUniqueAssetName(const FString& BaseName) const;
    void UpdateMontageDataWithRecordedSequence(FMontageData& MontageData, UWeaponAttackData* AttackData);
    void StartReplayVisualization();
    void StopReplayVisualization();
    void ReplayRecording();
};