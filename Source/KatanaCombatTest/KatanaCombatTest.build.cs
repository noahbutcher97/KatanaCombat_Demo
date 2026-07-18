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
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"KatanaCombat"  // Main module we're testing
		});

		// Test framework dependencies (editor-only)
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",          // For editor utilities in tests
				"AssetRegistry",     // Temporary proof assets and FiB cache setup
				"ImageCore",         // Rendered proof-frame decode and pixel validation
				"Kismet",           // Find-in-Blueprint cache reload regression coverage
				"KatanaCombatEditor", // For editor tool regression tests
				"Json",             // For asset migration report tests
				"MotionWarping",    // For alignment warp-target ownership tests
				"Niagara",          // For defense presentation effect fixtures
				"StateTreeModule",  // Enemy AI proof asset validation
				"GameplayStateTreeModule"
			});
		}

		PublicIncludePaths.AddRange(new string[] {"KatanaCombatTest/Public"});
		PrivateIncludePaths.AddRange(new string[] {"KatanaCombatTest/Private"});
	}
}
