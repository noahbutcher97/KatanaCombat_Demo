// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class KatanaCombatEditor : ModuleRules
{
    private const string AllowPluginDriftVariable = "KATANA_ALLOW_PLUGIN_DRIFT";

    public KatanaCombatEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        VerifyAnimationAnalysisPin(Target);

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Authoring translation units reuse private helper names. Keep their scopes
        // independent when new editor tooling changes Unreal's unity-file grouping.
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AnimationCore",  // Bone indices for montage analysis
            "GameplayTags",   // Defense manifest and presentation validation
            "AnimationCapture", // Independent engine capture API and compatibility headers
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
            "RenderCore",
            "RHI",
            "KismetCompiler",       // Headless Blueprint compilation for migration-generated assets
            "StateTreeModule",      // StateTree assets
            "StateTreeEditorModule",// StateTree builder/compiler APIs
            "GameplayStateTreeModule", // AI StateTree component schema
            "PropertyBindingUtils"  // StateTree property binding implementation
        });
    }

    /// <summary>
    /// Plugins/AnimationAnalysis is a generated, git-ignored copy of the revision pinned in
    /// Tools/AnimationAnalysis/dependency.json. A pull that moves the pin leaves the old copy in
    /// place, so compare the pin with the revision setup_dependency.py last installed.
    /// Both files are ExternalDependencies so UBT re-runs these rules when either changes.
    /// </summary>
    private void VerifyAnimationAnalysisPin(ReadOnlyTargetRules Target)
    {
        if (Target.ProjectFile == null)
        {
            return;
        }

        string ProjectRoot = Target.ProjectFile.Directory.FullName;
        string LockPath = Path.Combine(ProjectRoot, "Tools", "AnimationAnalysis", "dependency.json");
        string MarkerPath = Path.Combine(ProjectRoot, "Saved", "AnalysisDependencies", "plugin-install.json");
        ExternalDependencies.Add(LockPath);
        ExternalDependencies.Add(MarkerPath);

        if (!File.Exists(LockPath))
        {
            return; // No pin, so no suite dependency to check.
        }

        string PinnedRevision = ReadRevision(LockPath);
        if (PinnedRevision == null)
        {
            throw new BuildException("[AnimationAnalysis pin mismatch] " + LockPath
                + " has no readable \"revision\" field. Restore it from git before building.");
        }

        string InstalledRevision = ReadRevision(MarkerPath);
        if (string.Equals(PinnedRevision, InstalledRevision, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        string SetupScript = Path.Combine(ProjectRoot, "Tools", "AnimationAnalysis", "setup_dependency.py");
        string Message = string.Join(Environment.NewLine,
            "[AnimationAnalysis pin mismatch] Plugins/AnimationAnalysis does not match the pinned revision.",
            "  Pinned    (Tools/AnimationAnalysis/dependency.json): " + PinnedRevision,
            "  Installed (Saved/AnalysisDependencies/plugin-install.json): " + (InstalledRevision ?? "<none - setup has not run>"),
            "  Fix: python \"" + SetupScript + "\"",
            "    - If it fails with a file-in-use error, close the Unreal Editor (or stop UnrealEditor*.exe) and retry.",
            "    - If it refuses because the plugin has unowned or modified source, those are local plugin edits:",
            "      preserve them (they belong in the AnimationAnalysis repository) before re-running. Do not delete them.",
            "  Override: set " + AllowPluginDriftVariable + "=1 only when deliberately building local plugin edits;",
            "    it is not a fix for this error.");

        if (Environment.GetEnvironmentVariable(AllowPluginDriftVariable) == "1")
        {
            Logger.LogWarning("{Message}{NewLine}  Continuing because {Variable}=1.", Message, Environment.NewLine, AllowPluginDriftVariable);
            return;
        }

        throw new BuildException(Message);
    }

    private static string ReadRevision(string JsonPath)
    {
        // Missing or unparsable files read as "no revision"; callers report that explicitly.
        if (!File.Exists(JsonPath)
            || !JsonObject.TryRead(new FileReference(JsonPath), out JsonObject Document)
            || !Document.TryGetStringField("revision", out string Revision))
        {
            return null;
        }

        return Revision;
    }
}
