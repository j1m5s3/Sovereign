using UnrealBuildTool;

public class SovereignTarget : TargetRules
{
	public SovereignTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "SovereignBridge", "SovereignCore" });
	}
}
