#include "SovMods.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "SovSession.h"
#include "sovereign/json.h"

namespace
{
FString ModsListPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("mods.txt"));
}

// Folders to search: the repository's mods/ (beside data/), then the player's own.
TArray<FString> ModRoots()
{
	return {FPaths::ConvertRelativePathToFull(FPaths::Combine(FSovSetup::DefaultRulesDir(), TEXT(".."), TEXT(".."), TEXT("mods"))),
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("Mods"))};
}
}  // namespace

TArray<FSovMod> SovMods::Discover()
{
	TArray<FSovMod> Out;
	for (const FString& Root : ModRoots())
	{
		TArray<FString> Folders;
		IFileManager::Get().FindFiles(Folders, *FPaths::Combine(Root, TEXT("*")), false, true);
		Folders.Sort();
		for (const FString& Folder : Folders)
		{
			const FString Dir = FPaths::Combine(Root, Folder);
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("mod.json"))))
			{
				continue;
			}
			std::string Error;
			const sov::Json J = sov::Json::parse(TCHAR_TO_UTF8(*Text), &Error);
			if (!Error.empty() || !J.isObject() || J["id"].str().empty())
			{
				continue;
			}
			FSovMod Mod;
			Mod.Id = UTF8_TO_TCHAR(J["id"].str().c_str());
			Mod.Name = UTF8_TO_TCHAR(J["name"].str(J["id"].str()).c_str());
			Mod.Version = UTF8_TO_TCHAR(J["version"].str().c_str());
			Mod.Description = UTF8_TO_TCHAR(J["description"].str().c_str());
			Mod.Dir = Dir;
			if (!Out.ContainsByPredicate([&](const FSovMod& M) { return M.Id == Mod.Id; }))
			{
				Out.Add(Mod);
			}
		}
	}
	return Out;
}

TArray<FString> SovMods::Enabled()
{
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *ModsListPath());
	TArray<FString> Ids;
	for (const FString& L : Lines)
	{
		const FString Id = L.TrimStartAndEnd();
		if (!Id.IsEmpty()) Ids.AddUnique(Id);
	}
	return Ids;
}

void SovMods::SetEnabled(const TArray<FString>& Ids)
{
	FFileHelper::SaveStringToFile(FString::Join(Ids, LINE_TERMINATOR), *ModsListPath());
}

bool SovMods::RulesDirs(const TArray<FString>& Ids, std::vector<std::string>& OutDirs, FString& OutMissing)
{
	OutDirs = {std::string(TCHAR_TO_UTF8(*FSovSetup::DefaultRulesDir()))};
	const TArray<FSovMod> Installed = Discover();
	for (const FString& Id : Ids)
	{
		const FSovMod* Mod = Installed.FindByPredicate([&](const FSovMod& M) { return M.Id == Id; });
		if (!Mod)
		{
			OutMissing = Id;
			return false;
		}
		OutDirs.push_back(TCHAR_TO_UTF8(*Mod->Dir));
	}
	return true;
}

std::vector<std::string> SovMods::ToStd(const TArray<FString>& Ids)
{
	std::vector<std::string> Out;
	for (const FString& Id : Ids) Out.push_back(TCHAR_TO_UTF8(*Id));
	return Out;
}
