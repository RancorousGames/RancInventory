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

    FAttackMontageData MontageData;
    UPROPERTY()
    TArray<FName> RelevantSockets;
    float AnimationDuration;
    float IntervalBetweenRecordings;
    float CurrentTime;
    FTransform PivotTransform;
    FTransform PivotOffsetTransform;
    int32 CurrentIndex;
    UPROPERTY()
    TObjectPtr<UWeaponAttackData> AttackData;
    UPROPERTY()
    FTimerHandle RecordingTimerHandle;
};

		
#if WITH_EDITOR
// A component for recording trace sequences to be replayed later. Added to weaponactors when settings are defined.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RANCINVENTORYWEAPONS_API UWeaponAttackRecorderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponAttackRecorderComponent();

#endif

#if WITH_EDITORONLY_DATA 
    // Settings for the recording process
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording")
    UWeaponAttackRecorderSettings* Settings;

    // Folder path to save new assets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording", meta = (RelativeToGameContentDir))
    FDirectoryPath AssetSavePath;

    UPROPERTY()
    FRecordingSession ReplayedSession;
    
    UPROPERTY()
    ACharacter* OwningCharacter;

private:
    FRecordingSession CurrentSession;
    UPROPERTY()
    AWeaponActor* OwningWeapon;
    UPROPERTY()
    UMeshComponent* OwningWeaponMesh; // can be static or skeletal mesh
    UPROPERTY()
    UGearManagerComponent* OwningGearManager;
    UPROPERTY()
    UAnimInstance* OwningCharacterAnimInstance;
public:
    
#endif
#if WITH_EDITOR
    
    bool bIsRecording;
    bool RecordingInitialized;
    double RecordStartTime;
    double RecordInitTime;
    int ReplayCurrentIndex;
    bool bReplaySlowmotion;
    double ReplayStopTime;
    FTimerHandle ReplayTimerHandle;

    void OnAnimNotifyBegin(FName AnimName);
    void OnAnimNotifyEnd(FName AnimName);
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


    
    void Initialize();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    UFUNCTION()
    void OnAttackPerformed(FAttackMontageData MontageData);
    bool InitializeRecordingSession(FAttackMontageData MontageData);
    void RecordAttackData(float DeltaTime);
    void StartRecording();
    void StopRecording();
    bool SaveAttackSequence(const FString& AssetName, UWeaponAttackData* AttackData);
    TArray<FWeaponAttackTimestamp> ReduceToKeyframesForSocket(const TArray<FWeaponAttackTimestamp>& OriginalSequence, int32 SocketIndex) const;
    void PostProcessRecordedData();

    TArray<FName> FindRelevantSockets(class UMeshComponent* WeaponMesh) const;
    bool ValidateAssetSavePath() const;
    void UpdateMontageDataWithRecordedSequence(FAttackMontageData& MontageData, UWeaponAttackData* AttackData);
    void StartReplayVisualization();
    void StopReplayVisualization();
    void ReplayRecording();
};

#endif // WITH_EDITOR