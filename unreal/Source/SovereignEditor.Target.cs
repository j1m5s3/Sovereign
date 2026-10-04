using UnrealBuildTool;

public class SovereignEditorTarget : TargetRules
{
	public SovereignEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "SovereignBridge", "SovereignCore" });
	}
}
