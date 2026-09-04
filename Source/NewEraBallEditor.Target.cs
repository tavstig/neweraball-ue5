// NewEraBall - Editor target rules

using UnrealBuildTool;
using System.Collections.Generic;

public class NewEraBallEditorTarget : TargetRules
{
	public NewEraBallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("NewEraBall");
	}
}
