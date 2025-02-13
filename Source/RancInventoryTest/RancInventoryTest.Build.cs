// Copyright Rancorous Games, 2024

using UnrealBuildTool;

public class RancInventoryTest : ModuleRules
{
    public RancInventoryTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "RancInventory",
            "RancInventoryWeapons",
            "CoreUObject",
            "UnrealEd",
            "AssetTools",
            "Engine",
            "GameplayTags", "GameplayTagsEditor", "GameplayTagsEditor"
        });
    }
}