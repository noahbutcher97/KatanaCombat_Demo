// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class KatanaCombatEditorTarget : TargetRules
{
	public KatanaCombatEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		ExtraModuleNames.AddRange( new string[] { "KatanaCombatTest" } );
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("KatanaCombat");
		RegisterModulesCreatedByRider();
	}

	private void RegisterModulesCreatedByRider()
	{
		ExtraModuleNames.AddRange(new string[] { "KatanaCombatEditor" });
	}
}
