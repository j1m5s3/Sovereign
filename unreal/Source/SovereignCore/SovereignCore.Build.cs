using System.IO;
using UnrealBuildTool;

// The rules core (../../../core) compiled as an Unreal module. Private/Core holds one
// generated wrapper per core source (tools/check_unreal_core_module.py), so core/ never
// contains an Unreal file. The core is plain C++17 with its own conventions, so it builds
// without PCHs or unity files (its files share anonymous-namespace names).
public class SovereignCore : ModuleRules
{
	public SovereignCore(ReadOnlyTargetRules Target) : base(Target)
	{
		string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));

		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = true;

		PublicIncludePaths.Add(Path.Combine(RepoRoot, "core", "include"));
		PrivateIncludePaths.Add(RepoRoot);

		PublicDependencyModuleNames.Add("Core");
	}
}
