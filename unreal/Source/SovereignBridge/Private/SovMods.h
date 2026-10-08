// Data mods (specs/sovereign/player-retention.md §6; mods/README.md): folders of rules data with a
// mod.json, laid over data/rules in the order they are turned on. Found in the repository's mods/
// and in Saved/Sovereign/Mods/; the ones turned on are kept in Saved/Sovereign/mods.txt.
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

struct FSovMod
{
	FString Id, Name, Version, Description;
	FString Dir;
};

namespace SovMods
{
// Every installed mod, the shipped ones first, by folder name.
TArray<FSovMod> Discover();
// The mods turned on for new games, in load order.
TArray<FString> Enabled();
void SetEnabled(const TArray<FString>& Ids);
// The rules directories for a game with these mods: data/rules, then each mod's folder. False, with
// the first missing mod's id, when one is not installed.
bool RulesDirs(const TArray<FString>& Ids, std::vector<std::string>& OutDirs, FString& OutMissing);
// As the core's GameSetup::mods and the online handshake carry them.
std::vector<std::string> ToStd(const TArray<FString>& Ids);
}  // namespace SovMods
