using UnrealBuildTool;

// The bridge: mirrors the rules core's state into actors and turns player input into
// sov::Command (engine doc, Layers). It owns no game rule.
public class SovereignBridge : ModuleRules
{
	public SovereignBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Each file builds on its own: several keep same-named helpers in anonymous namespaces.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "SovereignCore", "ProceduralMeshComponent", "Slate", "SlateCore", "HTTP", "Sockets", "Networking" });
		// Steam lobbies, invites and networking (SovSteam), called directly on Windows.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
			PrivateDefinitions.Add("SOV_WITH_STEAM=1");
		}
		else
		{
			PrivateDefinitions.Add("SOV_WITH_STEAM=0");
		}
	}
}
