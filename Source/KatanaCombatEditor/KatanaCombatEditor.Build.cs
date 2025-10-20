// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KatanaCombatEditor : ModuleRules
{
    public KatanaCombatEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "KatanaCombat"  // Our runtime module
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",         // Editor framework
            "Slate",            // UI framework
            "SlateCore",        // UI core
            "PropertyEditor",   // Details panel customization
            "AssetRegistry",    // Finding assets
            "ContentBrowser",   // Asset browser integration
            "InputCore",        // Input handling
            "AnimGraph"         // Optional: For anim notify access
        });
    }
}