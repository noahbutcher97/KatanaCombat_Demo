// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using UnrealBuildTool;

public class KatanaCombat : ModuleRules
{
	public KatanaCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"MotionWarping"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "MotionWarping" });

		PublicIncludePaths.AddRange(new string[] {
			"KatanaCombat",
			"KatanaCombat/Public",
			"KatanaCombat/Variant_Platforming",
			"KatanaCombat/Variant_Platforming/Animation",
			"KatanaCombat/Variant_Combat",
			"KatanaCombat/Variant_Combat/AI",
			"KatanaCombat/Variant_Combat/Animation",
			"KatanaCombat/Variant_Combat/Gameplay",
			"KatanaCombat/Variant_Combat/Interfaces",
			"KatanaCombat/Variant_Combat/UI",
			"KatanaCombat/Variant_SideScrolling",
			"KatanaCombat/Variant_SideScrolling/AI",
			"KatanaCombat/Variant_SideScrolling/Gameplay",
			"KatanaCombat/Variant_SideScrolling/Interfaces",
			"KatanaCombat/Variant_SideScrolling/UI"
		});
		PrivateIncludePaths.AddRange(new string[] {
			
			"KatanaCombat/Private",
		});


		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
