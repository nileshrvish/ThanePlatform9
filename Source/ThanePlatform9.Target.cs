using UnrealBuildTool;

public class ThanePlatform9Target : TargetRules
{
	public ThanePlatform9Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ThanePlatform9");
	}
}
