// Copyright Rancorous Games, 2024

using UnrealBuildTool;

public class RancInventoryEditor : ModuleRules
{
    public RancInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
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
            "InputCore",
            "CoreUObject",
            "UnrealEd",
            "AssetTools",
            "ToolMenus",
            "Engine",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "WorkspaceMenuStructure",
            "PropertyEditor",
            "GameplayTags", 
            "GameplayTagsEditor", 
            "GameplayTagsEditor"
        });
    }
}