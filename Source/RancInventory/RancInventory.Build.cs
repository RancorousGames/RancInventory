// Author: Lucas Vilas-Boas
// Year: 2023

using UnrealBuildTool;

public class RancInventory : ModuleRules
{
    public RancInventory(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Engine",
            "NetCore",
            "CoreUObject",
            "GameplayTags",
            "DeveloperSettings"
        });
    }
}