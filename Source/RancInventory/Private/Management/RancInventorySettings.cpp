// Author: Lucas Vilas-Boas
// Year: 2023
// Repo: https://github.com/lucoiso/UEAzSpeech

#include "Management/RancInventorySettings.h"
#include "LogRancInventory.h"

#ifdef UE_INLINE_GENERATED_CPP_BY_NAME
#include UE_INLINE_GENERATED_CPP_BY_NAME(RancInventorySettings)
#endif

URancInventorySettings::URancInventorySettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), bEnableInternalLogs(false)
{
    CategoryName = TEXT("Plugins");
}

const URancInventorySettings* URancInventorySettings::Get()
{
    static const URancInventorySettings* const Instance = GetDefault<URancInventorySettings>();
    return Instance;
}

#if WITH_EDITOR
void URancInventorySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URancInventorySettings, bEnableInternalLogs))
    {
        ToggleInternalLogs();
    }
}
#endif

void URancInventorySettings::PostInitProperties()
{
    Super::PostInitProperties();

    ToggleInternalLogs();
}

void URancInventorySettings::ToggleInternalLogs()
{
#if !UE_BUILD_SHIPPING
    LogRancInventory_Internal.SetVerbosity(bEnableInternalLogs ? ELogVerbosity::Display : ELogVerbosity::NoLogging);
#endif
}