// NewEraBallEditor - Editor-only tooling module build rules.
// Houses the commandlet that authors the stadium materials and populates NewEraBall_Main.umap.
// Engine-bundled Editor modules only (UnrealEd, AssetRegistry). No plugins, no third-party code.

using UnrealBuildTool;

public class NewEraBallEditor : ModuleRules
{
	public NewEraBallEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"NewEraBall"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"AssetRegistry"
		});
	}
}
