// Owns one rules-core game for the bridge. The only places the bridge changes the
// game are Submit (a player's sov::Command) and StepAI (sov::ai::playTurn for an AI
// seat); everything else reads (engine doc, Layers: the bridge owns no game rule).
#pragma once

#include "CoreMinimal.h"

#include <cstdint>
#include <memory>

namespace sov
{
class Game;
class Rules;
struct Command;
struct GameSetup;
enum class CommandError : uint8_t;
}

struct FSovSetup
{
	uint64 Seed = 7;
	int32 Players = 4;
	FString MapSize = TEXT("MAPSIZE_TINY");
	// Seat 0 is played by the human; false lets the AI play every seat (spectating seat 0).
	bool bHumanSeat0 = true;
	// Developer start: seat 0's warrior on its leader's plot, an enemy warrior next to it, at war.
	bool bBattleDemo = false;
	// Rules data directory; empty means <repo>/data/rules beside the Unreal project.
	FString RulesDir;

	// Defaults overridden by -SovSeed=, -SovPlayers=, -SovSize=, -SovSpectate and -SovBattleDemo.
	static FSovSetup FromCommandLine();
	static FString DefaultRulesDir();
};

class FSovSession
{
public:
	FSovSession();
	~FSovSession();

	bool Start(const FSovSetup& Setup, FString& OutError);
	bool IsRunning() const { return Game != nullptr; }
	const sov::Game& GetGame() const { return *Game; }
	const sov::Rules& GetRules() const { return *Rules; }
	const sov::GameSetup& GetCoreSetup() const { return *CoreSetup; }

	// The seat whose knowledge the mirror shows.
	int32 ViewPlayer() const { return 0; }
	bool IsHumanTurn() const;
	bool IsGameOver() const;

	// Validates and applies a player's command through Game::submit.
	sov::CommandError Submit(const sov::Command& Command);
	// Plays the current seat with the AI when it is not a human seat. Returns false
	// when it is a human's turn, the game is over, or the seat failed to end its turn
	// (then the session stops stepping and Stalled() is true).
	bool StepAI();
	bool Stalled() const { return bStalled; }

	// Bumped on every change to the game; observers resync when it moves.
	uint64 Revision() const { return Rev; }

private:
	std::unique_ptr<sov::Rules> Rules;
	std::unique_ptr<sov::GameSetup> CoreSetup;
	std::unique_ptr<sov::Game> Game;  // declared after Rules: it holds a pointer to them
	uint64 Rev = 0;
	bool bStalled = false;
};
