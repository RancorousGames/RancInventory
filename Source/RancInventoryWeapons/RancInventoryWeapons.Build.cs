// Copyright Rancorous Games, 2024

using UnrealBuildTool;

public class RancInventoryWeapons : ModuleRules
{
    public RancInventoryWeapons(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "RancInventory",
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Engine",
            "NetCore",
            "CoreUObject",
            "GameplayTags",
            "RancInventory",
        });
    }
}