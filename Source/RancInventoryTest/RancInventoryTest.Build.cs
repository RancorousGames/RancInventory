// Copyright Rancorous Games, 2024

using UnrealBuildTool;

public class RancInventoryTest : ModuleRules
{
    public RancInventoryTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "RancInventory",
            "CoreUObject",
            "UnrealEd",
            "AssetTools",
            "Engine",
            "GameplayTags", "GameplayTagsEditor", "GameplayTagsEditor"
        });
    }
}