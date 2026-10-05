using UnrealBuildTool;

// The bridge: mirrors the rules core's state into actors and turns player input into
// sov::Command (engine doc, Layers). It owns no game rule.
public class SovereignBridge : ModuleRules
{
	public SovereignBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "SovereignCore", "ProceduralMeshComponent", "Slate", "SlateCore", "HTTP" });
	}
}
