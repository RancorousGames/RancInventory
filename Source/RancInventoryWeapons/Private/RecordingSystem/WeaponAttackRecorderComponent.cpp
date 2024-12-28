// Copyright Rancorous Games, 2024

#include "WeaponAttackRecorderComponent.h"
#include "AWeaponActor.h" // Make sure to include the correct header for your weapon actor
#include "Engine/SkeletalMeshSocket.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"

UWeaponAttackRecorderComponent::UWeaponAttackRecorderComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // We don't need to tick this component
}

void UWeaponAttackRecorderComponent::BeginPlay()
{
    Super::BeginPlay();

    InitializeSettings();

    // Cast the owning actor to AWeaponActor and subscribe to the OnAttackPerformed event
    OwningWeapon = Cast<AWeaponActor>(GetOwner());
    if (OwningWeapon)
    {
        OwningWeapon->OnAttackPerformed.AddDynamic(this, &UWeaponAttackRecorderComponent::OnAttackPerformed);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("WeaponAttackRecorderComponent is not attached to an AWeaponActor!"));
    }
}

void UWeaponAttackRecorderComponent::InitializeSettings()
{
    if (Settings == nullptr)
    {
        // Find the first WeaponAttackRecorderSettings asset in the project
        TArray<FAssetData> AssetData;
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistryModule.Get().GetAssetsByClass(UWeaponAttackRecorderSettings::StaticClass()->GetFName(), AssetData);

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
}

void UWeaponAttackRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe from the OnAttackPerformed event
    if (OwningWeapon)
    {
        OwningWeapon->OnAttackPerformed.RemoveDynamic(this, &UWeaponAttackRecorderComponent::OnAttackPerformed);
    }

    Super::EndPlay(EndPlayReason);
}

void UWeaponAttackRecorderComponent::OnAttackPerformed(FMontageData& MontageData)
{
    // Check if we should record this attack
 if (!MontageData.IsValid() || (MontageData.RecordedTraceSequence.IsValid() && !Settings->bOverwriteExisting))
    {
        return;
    }

    // Initialize the recording session
    if (InitializeRecordingSession(MontageData))
    {
        // Start the recording process
        RecordAttackData();

        // Set up a timer to stop recording after the montage duration
        GetWorld()->GetTimerManager().SetTimer(
            CurrentSession.RecordingTimerHandle,
            this,
            &UWeaponAttackRecorderComponent::StopRecording,
            CurrentSession.AnimationDuration,
            false
        );
    }
}

bool UWeaponAttackRecorderComponent::InitializeRecordingSession(FMontageData& MontageData)
{
    CurrentSession = FRecordingSession();
    CurrentSession.MontageData = &MontageData;

    // Get the weapon mesh
    USkeletalMeshComponent* WeaponMesh = OwningWeapon->GetMesh();
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
 CurrentSession.AnimationDuration = MontageData.Montage.Get()->GetPlayLength() / MontageData.PlayRate;
    CurrentSession.IntervalBetweenRecordings = CurrentSession.AnimationDuration / (Settings->Resolution - 1);
    CurrentSession.CurrentIndex = 0;

    CurrentSession.AttackData = NewObject<UWeaponAttackData>(this, UWeaponAttackData::StaticClass());
    CurrentSession.AttackData->AttackSequence.SetNum(Settings->Resolution);

    return true;
}

TArray<FName> UWeaponAttackRecorderComponent::FindRelevantSockets(USkeletalMeshComponent* WeaponMesh) const
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

void UWeaponAttackRecorderComponent::RecordAttackData()
{
    if (!CurrentSession.MontageData || !OwningWeapon)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid recording session"));
        return;
    }

    // Get the current timestamp
    float CurrentTime = CurrentSession.CurrentIndex * CurrentSession.IntervalBetweenRecordings;

    // Create a new timestamp entry
    FWeaponAttackTimestamp& Timestamp = CurrentSession.AttackData->AttackSequence[CurrentSession.CurrentIndex];
    Timestamp.Timestamp = CurrentTime;

    // Record traces for each relevant socket
    for (const FName& SocketName : CurrentSession.RelevantSockets)
    {
        FVector SocketLocation = OwningWeapon->GetMesh()->GetSocketLocation(SocketName);

        FWeaponAttackTrace Trace;
        Trace.StartOffset = OwningWeapon->GetActorTransform().InverseTransformPosition(SocketLocation);

        // You might want to customize how you calculate the EndOffset based on your needs
        Trace.EndOffset = Trace.StartOffset + FVector(50, 0, 0); // Example: extend 50 units in X direction

        Timestamp.TracesAtTime.Add(Trace);
    }

    // Increment the current index
    CurrentSession.CurrentIndex++;

    // If we haven't recorded all timestamps, set up the next recording
    if (CurrentSession.CurrentIndex < RecordingSettings.Resolution)
    {
        FTimerHandle NextRecordingTimer;
        GetWorld()->GetTimerManager().SetTimer(
            NextRecordingTimer,
            this,
   &UWeaponAttackRecorderComponent::RecordAttackData,
            CurrentSession.IntervalBetweenRecordings,
            false
        );
    }
}

void UWeaponAttackRecorderComponent::StopRecording()
{
    if (!CurrentSession.MontageData || !CurrentSession.AttackData)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid recording session when stopping"));
        return;
    }

    // Generate a unique asset name
    FString AssetName = GenerateUniqueAssetName(CurrentSession.MontageData->Montage.Get()->GetName() + "_AttackData");

    // Save the attack sequence
    if (SaveAttackSequence(AssetName, CurrentSession.AttackData))
    {
        // Update the MontageData with the new RecordedTraceSequence
        UpdateMontageDataWithRecordedSequence(*CurrentSession.MontageData, CurrentSession.AttackData);
    }

    // Clear the current session
    CurrentSession = FRecordingSession();
}

bool UWeaponAttackRecorderComponent::SaveAttackSequence(const FString& AssetName, UWeaponAttackData* AttackData)
{
    if (!ValidateAssetSavePath() || !AttackData)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid asset save path or AttackData is null"));
        return false;
    }

    FString PackagePath = FString::Printf(TEXT("/Game/%s"), *Settings->AssetSavePath.Path);
    FString FullPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    // Create or get the package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create or find package: %s"), *FullPath);
        return false;
    }

    // Fully load the package
    Package->FullyLoad();

    // Check if an object with the same name already exists
    UWeaponAttackData* ExistingAsset = FindObject<UWeaponAttackData>(Package, *AssetName);

    UWeaponAttackData* NewAsset;
    if (ExistingAsset)
    {
        // If the asset already exists, we'll update it
        NewAsset = ExistingAsset;
        NewAsset->AttackSequence.Empty(); // Clear existing data
    }
    else
    {
        // Create a new object in the package
        NewAsset = NewObject<UWeaponAttackData>(Package, *AssetName, RF_Public | RF_Standalone);
    }

    if (!NewAsset)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create or find asset: %s"), *AssetName);
        return false;
    }

    // Copy properties from the input AttackData to the new asset
    NewAsset->AttackSequence = AttackData->AttackSequence;

    // Mark the package as dirty
    Package->MarkPackageDirty();

    // Notify the asset registry
    if (!ExistingAsset)
    {
        FAssetRegistryModule::AssetCreated(NewAsset);
    }

    // Save the package
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    bool bSaved = UPackage::SavePackage(Package, NewAsset, RF_Public | RF_Standalone, *PackageFileName, GError, nullptr, true, true, SAVE_NoError);

    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save asset: %s"), *PackageFileName);
    }

    return bSaved;
}

void UWeaponAttackRecorderComponent::UpdateMontageDataWithRecordedSequence(FMontageData& MontageData, UWeaponAttackData* AttackData)
{
    if (AttackData)
    {
        MontageData.RecordedTraceSequence = AttackData;
    }
}

void UWeaponAttackRecorderComponent::CleanupUnusedAttackData()
{
    // This is a placeholder implementation. You might want to implement this based on your specific needs.
    // For example, you could:
    // 1. Scan the AssetSavePath for UWeaponAttackData assets
    // 2. Check if each asset is referenced by any FMontageData
    // 3. Delete unreferenced assets

    UE_LOG(LogTemp, Warning, TEXT("CleanupUnusedAttackData is not implemented yet"));
}