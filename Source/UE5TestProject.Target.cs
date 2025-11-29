using UnrealBuildTool;
using System.Collections.Generic;

public class UE5TestProjectTarget : TargetRules
{
	public UE5TestProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("UE5TestProject");
	}
}
