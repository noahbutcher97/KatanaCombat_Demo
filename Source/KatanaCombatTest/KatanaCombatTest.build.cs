using UnrealBuildTool;

public class KatanaCombatTest : ModuleRules
{
	public KatanaCombatTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core dependencies
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"KatanaCombat"  // Main module we're testing
		});

		// Test framework dependencies (editor-only)
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd"  // For editor utilities in tests
			});
		}

		PublicIncludePaths.AddRange(new string[] {"KatanaCombatTest/Public"});
		PrivateIncludePaths.AddRange(new string[] {"KatanaCombatTest/Private"});
	}
}