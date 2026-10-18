using UnrealBuildTool;

public class DustToGloryEditorTarget : TargetRules
{
	public DustToGloryEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DustToGlory");
	}
}
