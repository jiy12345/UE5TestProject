using UnrealBuildTool;
using System.Collections.Generic;

public class UE5TestProjectEditorTarget : TargetRules
{
	public UE5TestProjectEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("UE5TestProject");
	}
}
