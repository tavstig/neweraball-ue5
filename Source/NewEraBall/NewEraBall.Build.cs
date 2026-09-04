// NewEraBall - Primary game module build rules
// Engine modules only. No third-party dependencies.

using UnrealBuildTool;

public class NewEraBall : ModuleRules
{
	public NewEraBall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"NewEraBall"
		});
	}
}
