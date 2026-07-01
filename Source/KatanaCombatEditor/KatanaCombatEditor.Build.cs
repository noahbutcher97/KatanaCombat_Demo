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
            "AnimationCore",  // Bone indices for montage analysis
            "KatanaCombat"    // Our runtime module
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "EditorSubsystem",      // UEditorSubsystem base class for PairedAnimationAnalysisSubsystem
            "UnrealEd",             // Editor framework
            "Slate",                // UI framework
            "SlateCore",            // UI core
            "PropertyEditor",       // Details panel customization
            "AssetRegistry",        // Finding assets
            "ContentBrowser",       // Asset browser integration
            "InputCore",            // Input handling
            "AnimGraph",            // Optional: For anim notify access
            "Blutility",            // Editor Utility Objects for testing
            "LevelEditor",          // Menu extension for Montage Analyzer window
            "AdvancedPreviewScene", // Preview scene for montage analysis dashboards
            "ApplicationCore",      // Clipboard functionality for copy/export
            "Json"                  // Headless asset migration reports
        });
    }
}
