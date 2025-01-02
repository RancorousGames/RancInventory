// Copyright Rancorous Games, 2024

#include "RecordingSystem/WeaponAttackRecorderComponent.h"
#include "WeaponActor.h"
#include "Engine/SkeletalMeshSocket.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SavePackage.h"

UWeaponAttackRecorderComponent::UWeaponAttackRecorderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UWeaponAttackRecorderComponent::BeginPlay()
{
    Super::BeginPlay();

    Initialize();
}

void UWeaponAttackRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe from the OnAttackPerformed event
    if (OwningGearManager)
    {
        OwningGearManager->OnAttackPerformed.RemoveDynamic(this, &UWeaponAttackRecorderComponent::OnAttackPerformed);
    }
    
    Super::EndPlay(EndPlayReason);
}

void UWeaponAttackRecorderComponent::Initialize()
{
    OwningWeapon = Cast<AWeaponActor>(GetOwner());
    if (!OwningWeapon || !OwningWeapon->GetOwner()->GetOwner())
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponAttackRecorderComponent is not attached to an AWeaponActor!"));
        return;
    }
    ACharacter* OwningCharacter = Cast<ACharacter>(OwningWeapon->GetOwner());
    // Gear UGearManagerComponent
    OwningGearManager = OwningCharacter->FindComponentByClass<UGearManagerComponent>();
    if (OwningGearManager)
    {
        OwningGearManager->OnAttackPerformed.AddDynamic(this, &UWeaponAttackRecorderComponent::OnAttackPerformed);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponAttackRecorderComponent is not attached to an AWeaponActor!"));
        return;
    }

    
    if (Settings == nullptr)
    {
        // Find the first WeaponAttackRecorderSettings asset in the project
        TArray<FAssetData> AssetData;
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FTopLevelAssetPath ClassPath = UWeaponAttackRecorderSettings::StaticClass()->GetClassPathName();
        AssetRegistryModule.Get().GetAssetsByClass(ClassPath, AssetData);

        if (AssetData.Num() > 0)
        {
            Settings = Cast<UWeaponAttackRecorderSettings>(AssetData[0].GetAsset());
        }

        // If still null, create a new instance with default values
        if (Settings == nullptr)
        {
            Settings = NewObject<UWeaponAttackRecorderSettings>(this, UWeaponAttackRecorderSettings::StaticClass());
            UE_LOG(LogTemp, Warning, TEXT("No WeaponAttackRecorderSettings found. Created a new instance with default values."));
        }
    }

    // Get the owning character's animation instance
    if (OwningCharacter)
    {
        OwningCharacterAnimInstance = OwningCharacter->GetMesh()->GetAnimInstance();
        if (OwningCharacterAnimInstance)
        {
            OwningCharacterAnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UWeaponAttackRecorderComponent::OnAnimNotifyBegin);
            OwningCharacterAnimInstance->OnPlayMontageNotifyEnd.AddDynamic(this, &UWeaponAttackRecorderComponent::OnAnimNotifyEnd);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponAttackRecorderComponent could not find the owning character!"));
    }
}


void UWeaponAttackRecorderComponent::OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
    if (NotifyName == Settings->StartRecordingNotify)
    {
        StartRecording();
    }
}

void UWeaponAttackRecorderComponent::OnAnimNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
    if (NotifyName == Settings->StopRecordingNotify)
    {
        StopRecording();
    }
}

bool UWeaponAttackRecorderComponent::InitializeRecordingSession(FMontageData MontageData)
{
    UE_LOG(LogTemp, Warning, TEXT("Initializing recording session"));
    CurrentSession = FRecordingSession();
    CurrentSession.MontageData = MontageData;

    if (!CurrentSession.MontageData.Montage.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid MontageData"));
        return false;
    }
  
    // Get the weapon mesh
    UMeshComponent* WeaponMesh = OwningWeapon->GetStaticMeshComponent();
    if (!WeaponMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get weapon mesh for recording"));
        return false;
    }

    // Find relevant sockets
    CurrentSession.RelevantSockets = FindRelevantSockets(WeaponMesh);
    if (CurrentSession.RelevantSockets.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No relevant sockets found for recording"));
        return false;
    }

    // Calculate animation duration and interval between recordings
    CurrentSession.CurrentTime = 0;
    CurrentSession.CurrentIndex = 0;

    CurrentSession.AttackData = NewObject<UWeaponAttackData>(this, UWeaponAttackData::StaticClass());
    // CurrentSession.AttackData->AttackSequence.SetNum(Settings->Resolution);
    RecordingInitialized = true;
    return true;
}

void UWeaponAttackRecorderComponent::StartRecording()
{
    if (bIsRecording)
    {
        UE_LOG(LogTemp, Warning, TEXT("Recording already in progress. Stopping all recording to ensure clean state."));
        StopRecording();
    }
    UE_LOG(LogTemp, Warning, TEXT("Starting recording"));
    // Start a high-frequency recording (every frame)
    CurrentSession.CurrentIndex = 0;
    bIsRecording = true;
    RecordStartTime = GetWorld()->GetTimeSeconds();
    RecordAttackData(0.0f);
}

void UWeaponAttackRecorderComponent::StopRecording()
{
    UE_LOG(LogTemp, Warning, TEXT("Stopping recording"));
    if (!bIsRecording || !CurrentSession.AttackData)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid recording session when stopping"));
        bIsRecording = false;
        return;
    }

    double TimeSinceLastRecord =GetWorld()->GetTimeSeconds() - (RecordStartTime + CurrentSession.CurrentTime);
    if (TimeSinceLastRecord > 0.05f || CurrentSession.AttackData->AttackSequence.Num() < 2)
    {
        RecordAttackData(TimeSinceLastRecord);
    }

    // Generate a unique asset name
    FString AssetName = GenerateUniqueAssetName(CurrentSession.MontageData.Montage.Get()->GetName() + "_AttackData");

    // Reduce data to fewer keyframes
    PostProcessRecordedData(); 
    // Save the attack sequence
    // Log sequence
    UE_LOG(LogTemp, Warning, TEXT("Attack sequence recorded with %d keyframes."), CurrentSession.AttackData->AttackSequence.Num());
    if (SaveAttackSequence(AssetName, CurrentSession.AttackData))
    {
        UE_LOG(LogTemp, Warning, TEXT("Attack sequence saved as %s."), *AssetName);
        // Update the MontageData with the new RecordedTraceSequence
        UpdateMontageDataWithRecordedSequence(CurrentSession.MontageData, CurrentSession.AttackData);
    }

    RecordingInitialized = false;
    bIsRecording = false;
    CurrentSession = FRecordingSession();
}

void UWeaponAttackRecorderComponent::OnAnimNotifyBegin(FName AnimName)
{
    UE_LOG(LogTemp, Warning, TEXT("OnAnimNotifyBegin: %s"), *AnimName.ToString());

    UAnimMontage* Montage = CurrentSession.MontageData.Montage.Get();
    // Load CurrentSession.MontageData.Montage softpointer if its not loaded
    if (!CurrentSession.MontageData.Montage.IsValid())
    {
        // Load the softpointer
        Montage = CurrentSession.MontageData.Montage.LoadSynchronous();
    }
    
    if (!RecordingInitialized || !Montage)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid recording session or montage"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("Recording initialized"));
    // Check if the current animation is the one we're recording

    
    TArray<UAnimationAsset*> AllAnimSequences;
  
    if (Montage && Montage->GetFName() == AnimName && !bIsRecording)
    {
        StartRecording();
    }
}

void UWeaponAttackRecorderComponent::OnAnimNotifyEnd(FName AnimName)
{
    UE_LOG(LogTemp, Warning, TEXT("OnAnimNotifyEnd: %s"), *AnimName.ToString())
    
    UAnimMontage* Montage = CurrentSession.MontageData.Montage.Get();
    // Load CurrentSession.MontageData.Montage softpointer if its not loaded
    if (!CurrentSession.MontageData.Montage.IsValid())
    {
        // Load the softpointer
        Montage = CurrentSession.MontageData.Montage.LoadSynchronous();
    }
    
    if (!RecordingInitialized || !Montage)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid recording session or montage"));
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Recording initialized"));
    TArray<UAnimationAsset*> AllAnimSequences;
    if (Montage && Montage->GetFName() == AnimName && bIsRecording)
    {
        StopRecording();
    }
}

void UWeaponAttackRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsRecording && !Settings->bRecordKeyframesOnly)
    {
        RecordAttackData(DeltaTime);
    }
}

void UWeaponAttackRecorderComponent::RecordAttackData(float DeltaTime)
{
    if (!RecordingInitialized || !bIsRecording || !OwningWeapon)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid recording session"));
        return;
    }

    // increase time
    CurrentSession.CurrentTime += DeltaTime;

    // Create a new timestamp entry
    FWeaponAttackTimestamp Timestamp;
    Timestamp.Timestamp = CurrentSession.CurrentTime;
    Timestamp.OriginalIndex = CurrentSession.CurrentIndex;
    
    // Record traces for each relevant socket
    for (const FName& SocketName : CurrentSession.RelevantSockets)
    {
        FVector SocketGlobalLocation = OwningWeapon->GetStaticMeshComponent()->GetSocketLocation(SocketName);
        // Get relative location of the socket
        FVector SocketLocation = OwningWeapon->GetOwner()->GetActorTransform().InverseTransformPosition(SocketGlobalLocation);
        
        Timestamp.SocketPositions.Add(SocketLocation);
    }

    CurrentSession.AttackData->AttackSequence.Add(Timestamp);

    // Increment the current index
    CurrentSession.CurrentIndex++;
}


void UWeaponAttackRecorderComponent::OnAttackPerformed(FMontageData MontageData)
{
    // Check if we should record this attack
    if (bIsRecording || !MontageData.IsValid() || (MontageData.RecordedTraceSequence.IsValid() && !Settings->bOverwriteExisting))
    {
        if (bIsRecording)
        {
            UE_LOG(LogTemp, Warning, TEXT("New attack started while recording was already in progress. Stopping current recording."));
            StopRecording();
        }
        return;
    }
    
    // Initialize the recording session
    InitializeRecordingSession(MontageData);
}

TArray<FName> UWeaponAttackRecorderComponent::FindRelevantSockets(UMeshComponent* WeaponMesh) const
{
    TArray<FName> RelevantSockets;
    
    if (!WeaponMesh)
    {
        return RelevantSockets;
    }

    TArray<FName> AllSockets = WeaponMesh->GetAllSocketNames();
    for (const FName& SocketName : AllSockets)
    {
        if (SocketName.ToString().StartsWith(Settings->SocketPrefix))
        {
            RelevantSockets.Add(SocketName);
        }
    }

    return RelevantSockets;
}



TArray<FWeaponAttackTimestamp> UWeaponAttackRecorderComponent::ReduceToKeyframesForSocket(const TArray<FWeaponAttackTimestamp>& OriginalSequence, int32 SocketIndex) const
{
    TArray<FWeaponAttackTimestamp> Keyframes;

    // Check if the original sequence is empty
    if (OriginalSequence.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ReduceToKeyframesForSocket: Original sequence is empty."));
        return Keyframes;
    }

    // Check if the socket index is valid
    if (!OriginalSequence[0].SocketPositions.IsValidIndex(SocketIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("ReduceToKeyframesForSocket: Invalid socket index %d."), SocketIndex);
        return Keyframes;
    }

    // If there's only one element in the original sequence, print an error and return empty
    if (OriginalSequence.Num() == 1)
    {
        UE_LOG(LogTemp, Error, TEXT("ReduceToKeyframesForSocket: Original sequence has only one element, which is insufficient for reduction."));
        return Keyframes;
    }

    // If there are exactly two elements in the original sequence, return them early
    if (OriginalSequence.Num() == 2)
    {
        return OriginalSequence;
    }

    // Always include the first keyframe
    Keyframes.Add(OriginalSequence[0]);
    int32 LastKeyframeIndex = 0;

    const float MinDistance = Settings->MinDistance; // Minimum distance in centimeters to consider a keyframe
    const float AngleThreshold = FMath::DegreesToRadians(Settings->AngleThreshold); // Angle threshold in radians

    for (int32 i = 1; i < OriginalSequence.Num() - 1; ++i)
    {
        const FWeaponAttackTimestamp& Prev = OriginalSequence[LastKeyframeIndex];
        const FWeaponAttackTimestamp& Current = OriginalSequence[i];
        const FWeaponAttackTimestamp& Next = OriginalSequence[i + 1];

        // Ensure all timestamps have valid traces for the specified socket
        if (!Prev.SocketPositions.IsValidIndex(SocketIndex) || 
            !Current.SocketPositions.IsValidIndex(SocketIndex) || 
            !Next.SocketPositions.IsValidIndex(SocketIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("ReduceToKeyframesForSocket: Invalid trace data at index %d."), i);
            continue;
        }

        // Calculate vectors between points for the specified socket
        FVector Vector1 = Current.SocketPositions[SocketIndex] - Prev.SocketPositions[SocketIndex];
        FVector Vector2 = Next.SocketPositions[SocketIndex] - Current.SocketPositions[SocketIndex];

        // Calculate angle between vectors
        float Angle = 0.0f;
        if (!Vector1.IsNearlyZero() && !Vector2.IsNearlyZero())
        {
            Angle = FMath::Acos(FVector::DotProduct(Vector1.GetSafeNormal(), Vector2.GetSafeNormal()));
        }

        // Calculate distance from the last keyframe
        float Distance = FVector::Dist(Prev.SocketPositions[SocketIndex], Current.SocketPositions[SocketIndex]);

        // Check if both the angle change and distance are significant
        if (Angle > AngleThreshold && Distance > MinDistance)
        {
            Keyframes.Add(Current);
            LastKeyframeIndex = i;
        }
    }
 
    // Always include the last keyframe
    Keyframes.Add(OriginalSequence.Last());

    return Keyframes;
}


void UWeaponAttackRecorderComponent::PostProcessRecordedData()
{
    TArray<FWeaponAttackTimestamp> ReducedSequence;

    if (CurrentSession.AttackData->AttackSequence.Num() < 2)
    {
        // Not enough data to reduce
        return;
    }

    // Invert the dimensions of the keyframes so its an array of sockets each with a number of timestamps
    // And reduce the keyframes to only the necessary amount
    TArray<TArray<FWeaponAttackTimestamp>> SocketKeyframes;
    for (int32 SocketIndex = 0; SocketIndex < CurrentSession.RelevantSockets.Num(); ++SocketIndex)
    {
        TArray<FWeaponAttackTimestamp> ReducedKeys = ReduceToKeyframesForSocket(CurrentSession.AttackData->AttackSequence, SocketIndex);
        SocketKeyframes.Add(ReducedKeys);
    }

    // Find the socket with the most keyframes
    int32 MaxKeyframesIndex = 0;
    int32 MaxKeyframesCount = SocketKeyframes[0].Num();
    for (int32 i = 1; i < SocketKeyframes.Num(); ++i)
    {
        if (SocketKeyframes[i].Num() > MaxKeyframesCount)
        {
            MaxKeyframesCount = SocketKeyframes[i].Num();
            MaxKeyframesIndex = i;
        }
    }

    const TArray<FWeaponAttackTimestamp>& MasterKeyframes = SocketKeyframes[MaxKeyframesIndex];

    // Collect positions from all sockets at the master keyframe timestamps and revert the dimensions back to original format
    for (int32 i = 0; i < MasterKeyframes.Num(); ++i)
    {
        const FWeaponAttackTimestamp& MasterKeyframe = MasterKeyframes[i];
        FWeaponAttackTimestamp NewTimestamp;
        NewTimestamp.Timestamp = MasterKeyframe.Timestamp;

        for (int32 SocketIndex = 0; SocketIndex < CurrentSession.RelevantSockets.Num(); ++SocketIndex)
        {
            // Directly access the matching timestamp in the original sequence for each socket
            const TArray<FVector>& OriginalKeysAtKeyframe = CurrentSession.AttackData->AttackSequence[MasterKeyframe.OriginalIndex].SocketPositions;
            FVector Trace = OriginalKeysAtKeyframe[SocketIndex];
            NewTimestamp.SocketPositions.Add(Trace);
        }

        ReducedSequence.Add(NewTimestamp);
    }

    // Replace the original sequence with the reduced one
    CurrentSession.AttackData->AttackSequence = ReducedSequence;

    // Start replay visualization after processing
    StartReplayVisualization();
}



bool UWeaponAttackRecorderComponent::SaveAttackSequence(const FString& AssetName, UWeaponAttackData* AttackData)
{
    if (!ValidateAssetSavePath() || !IsValid(AttackData))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid asset save path or AttackData is null"));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Saving attack sequence as %s"), *AssetName);

    FString PackagePath = FString::Printf(TEXT("/Game/%s"), *Settings->AssetSavePath.Path);
    FString FullPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    // Create or get the package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create or find package: %s"), *FullPath);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Package created or found: %s"), *FullPath);
    // Fully load the package
    Package->FullyLoad();

    // Check if an object with the same name already exists
    UWeaponAttackData* ExistingAsset = FindObject<UWeaponAttackData>(Package, *AssetName);
    
    UWeaponAttackData* NewAsset;
    if (ExistingAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("Asset already exists. Updating existing asset."));
        // If the asset already exists, we'll update it
        NewAsset = ExistingAsset;
        NewAsset->AttackSequence.Empty(); // Clear existing data
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating new asset."));
        // Create a new object in the package
        NewAsset = NewObject<UWeaponAttackData>(Package, *AssetName, RF_Public | RF_Standalone);
    }

    if (!NewAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create or find asset: %s"), *AssetName);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Asset created or found: %s"), *AssetName);
    // Copy properties from the input AttackData to the new asset
    NewAsset->AttackSequence = AttackData->AttackSequence;

    // Mark the package as dirty
    Package->MarkPackageDirty();

    // Notify the asset registry
    if (!ExistingAsset)
    {
    UE_LOG(LogTemp, Warning, TEXT("Asset created. Notifying asset registry."));
        FAssetRegistryModule::AssetCreated(NewAsset);
    }

    // Save the package
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GError;
    SaveArgs.bForceByteSwapping = true;
    SaveArgs.bWarnOfLongFilename = true;
    SaveArgs.SaveFlags = SAVE_NoError;

    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    bool bSaved = UPackage::SavePackage(Package, NewAsset, *PackageFileName, SaveArgs);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save asset: %s"), *PackageFileName);
    }

    UE_LOG(LogTemp, Warning, TEXT("Asset saved: %s"), *PackageFileName);
    return bSaved;
}

bool UWeaponAttackRecorderComponent::ValidateAssetSavePath() const
{
    return !Settings->AssetSavePath.Path.IsEmpty() && FPaths::DirectoryExists(FPaths::ProjectContentDir() + Settings->AssetSavePath.Path);
}

FString UWeaponAttackRecorderComponent::GenerateUniqueAssetName(const FString& BaseName) const
{
    FString UniqueName = BaseName;
    int32 Suffix = 1;

    while (true)
    {
        FString FullPath = Settings->AssetSavePath.Path / UniqueName;
        if (!FPackageName::DoesPackageExist(FullPath))
        {
            break;
        }
        UniqueName = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix);
        Suffix++;
    }

    return UniqueName;
}

void UWeaponAttackRecorderComponent::UpdateMontageDataWithRecordedSequence(FMontageData& MontageData, UWeaponAttackData* AttackData)
{
    if (AttackData)
    {
        MontageData.RecordedTraceSequence = AttackData;
    }
}

void UWeaponAttackRecorderComponent::StartReplayVisualization()
{
    UE_LOG(LogTemp, Warning, TEXT("Starting replay visualization"));
    bReplayInitialOwnerPositionSaved = false;
    ReplayCurrentIndex = 0;
    bReplaySlowmotion = false;
    ReplayedSession = CurrentSession;
    // Set stopping time 60 seconds from noww
    ReplayStopTime = GetWorld()->GetTimeSeconds() + 60.0f;
    // Replay the recording in a loop for 1 minute or until the next recording starts
    ReplayRecording();
}

void UWeaponAttackRecorderComponent::StopReplayVisualization()
{
    GetWorld()->GetTimerManager().ClearTimer(ReplayTimerHandle);
    // Clear the current session
    ReplayedSession = FRecordingSession();
}

void UWeaponAttackRecorderComponent::ReplayRecording()
{
    if (!IsValid(ReplayedSession.AttackData) || ReplayedSession.AttackData->AttackSequence.Num() == 0 || GetWorld()->GetTimeSeconds() > ReplayStopTime)
    {
        StopReplayVisualization();
        return;
    }

    // Save the initial owner's position at the start of replaying if not already saved
    if (!bReplayInitialOwnerPositionSaved)
    {
        ReplayInitialOwnerPosition = OwningWeapon->GetOwner()->GetActorTransform();
        bReplayInitialOwnerPositionSaved = true;
    }

    // Increment the current frame index
    if (ReplayCurrentIndex >= ReplayedSession.AttackData->AttackSequence.Num() - 1)
    {
        // Take a break before restarting the replay
        GetWorld()->GetTimerManager().ClearTimer(ReplayTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(ReplayTimerHandle, this, &UWeaponAttackRecorderComponent::ReplayRecording, 1.f, false);
        ReplayCurrentIndex = 0;

        // Toggle the replay speed every other loop
        bReplaySlowmotion = !bReplaySlowmotion;
        return;
    }

    const FWeaponAttackTimestamp& CurrentTimestamp = ReplayedSession.AttackData->AttackSequence[ReplayCurrentIndex];
    const FWeaponAttackTimestamp& NextTimestamp = ReplayedSession.AttackData->AttackSequence[ReplayCurrentIndex + 1];
    
    // Adjust the timer to control replay speed
    float TimeDelta = NextTimestamp.Timestamp - CurrentTimestamp.Timestamp;
    float ReplayInterval = bReplaySlowmotion ? TimeDelta * 4.0f : TimeDelta;
    // Draw debug lines for each socket's movement from current to next position
    for (int32 SocketIndex = 0; SocketIndex < CurrentTimestamp.SocketPositions.Num(); ++SocketIndex)
    {
        FVector StartLocation = ReplayInitialOwnerPosition.TransformPosition(CurrentTimestamp.SocketPositions[SocketIndex]);
        FVector EndLocation = ReplayInitialOwnerPosition.TransformPosition(NextTimestamp.SocketPositions[SocketIndex]);

        FColor LineColor = bReplaySlowmotion ? FColor::Cyan : FColor::Green;
        DrawDebugLine(GetWorld(), StartLocation, EndLocation, LineColor, false, ReplayInterval, 0, 2.0f);
    }

    ReplayCurrentIndex++;
    GetWorld()->GetTimerManager().SetTimer(ReplayTimerHandle, this, &UWeaponAttackRecorderComponent::ReplayRecording, ReplayInterval, false);
}