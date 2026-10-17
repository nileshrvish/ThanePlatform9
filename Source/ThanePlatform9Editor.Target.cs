using UnrealBuildTool;

public class ThanePlatform9EditorTarget : TargetRules
{
	public ThanePlatform9EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ThanePlatform9");
	}
}
