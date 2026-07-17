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
            "GameplayTags",   // Defense manifest and presentation validation
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
            "EnhancedInput",        // Proof input action/context wiring
            "Niagara",              // Defense impact dependency inventory
            "MotionWarping",        // Role-specific proof montage warp validation
            "AnimGraph",            // Optional: For anim notify access
            "BlueprintGraph",       // Guard AnimBP state and bIsBlocking graph validation
            "Blutility",            // Editor Utility Objects for testing
            "LevelEditor",          // Menu extension for Montage Analyzer window
            "AdvancedPreviewScene", // Preview scene for montage analysis dashboards
            "ApplicationCore",      // Clipboard functionality for copy/export
            "Json",                 // Headless asset migration reports
            "KismetCompiler",       // Headless Blueprint compilation for migration-generated assets
            "StateTreeModule",      // StateTree assets
            "StateTreeEditorModule",// StateTree builder/compiler APIs
            "GameplayStateTreeModule", // AI StateTree component schema
            "PropertyBindingUtils"  // StateTree property binding implementation
        });
    }
}
