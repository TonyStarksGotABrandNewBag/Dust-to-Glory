using UnrealBuildTool;

public class DustToGloryTarget : TargetRules
{
	public DustToGloryTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DustToGlory");
	}
}
