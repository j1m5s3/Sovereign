// Owns one rules-core game for the bridge. The only places the bridge changes the
// game are Submit (a player's sov::Command) and StepAI (sov::ai::playTurn for an AI
// seat); everything else reads (engine doc, Layers: the bridge owns no game rule).
// Online (net/), the game lives in a sov::net::Host or Client instead: Submit sends the
// command to be ordered, Poll takes in what the session has applied, and the host plays the
// AI seats. Hot seat: several human seats on one machine, the view handed over between them.
#pragma once

#include "CoreMinimal.h"

#include <cstdint>
#include <memory>

namespace sov
{
namespace net
{
class Host;
class Client;
class Listener;
}
class Game;
class Rules;
struct Command;
struct GameSetup;
enum class CommandError : uint8_t;
}

enum class ESovNet : uint8
{
	Local,  // this machine plays every seat (hot seat when several are human)
	Host,   // hosts an online game (TCP, LAN or direct IP)
	Join    // joins one
};

struct FSovSetup
{
	uint64 Seed = 7;
	int32 Players = 4;
	FString MapSize = TEXT("MAPSIZE_TINY");
	// Seat 0 is played by the human; false lets the AI play every seat (spectating seat 0).
	bool bHumanSeat0 = true;
	// Developer start: seat 0's warrior on its leader's plot, an enemy warrior next to it, at war.
	bool bBattleDemo = false;
	// Developer start: seat 0 knows Shipbuilding, with a galley and an embarked warrior on the nearest coast.
	bool bNavalDemo = false;
	// Developer start: every civ has met every other, each has 200 gold, and seat 1 waits on seat 0
	// with a small gift of gold (to try the diplomacy screen at once).
	bool bDiploDemo = false;
	// Rules data directory; empty means <repo>/data/rules beside the Unreal project.
	FString RulesDir;
	// Human seats: the first HumanSeats seats (hot seat locally; seats others may claim when hosting).
	int32 HumanSeats = 1;
	ESovNet Net = ESovNet::Local;
	int32 Port = 7777;
	FString JoinAddress = TEXT("127.0.0.1");
	FString PlayerName = TEXT("Player");
	// Hosting: start by itself once this many players have joined (0: wait for Enter).
	int32 AutoStartPlayers = 0;
	// Online through Steam (a friends-only lobby and Steam's networking) instead of TCP.
	bool bSteam = false;
	uint64 SteamLobby = 0;  // joining: this lobby (0: wait for an invite)

	// Defaults overridden by -SovSeed=, -SovPlayers=, -SovSize=, -SovSpectate, -SovBattleDemo, -SovNavalDemo,
	// -SovDiploDemo, -SovHotSeat=N (N human seats), -SovHost (with -SovHumans=N), -SovJoin=address, -SovPort=, -SovName=, -SovAutoStart=N,
	// -SovSteam (with -SovHost, or alone to join through an invite) and -SovSteamLobby=id.
	static FSovSetup FromCommandLine();
	// Whether the command line asks for a game at once (any -Sov start option); otherwise the menu opens.
	static bool HasStartOptions();
	static FString DefaultRulesDir();
};

class FSovSession
{
public:
	FSovSession();
	~FSovSession();

	bool Start(const FSovSetup& Setup, FString& OutError);
	// A game exists (online: once the host has started it and it has arrived here).
	bool IsRunning() const { return CurrentGame() != nullptr; }
	// Running, or waiting in an online lobby.
	bool IsActive() const;
	const sov::Game& GetGame() const { return *CurrentGame(); }
	const sov::Rules& GetRules() const { return *Rules; }
	const sov::GameSetup& GetCoreSetup() const { return *CoreSetup; }

	// The seat whose knowledge the mirror shows (online: this machine's seat; hot seat: the
	// human whose turn it is, once they take over).
	int32 ViewPlayer() const;
	bool IsHumanTurn() const;
	bool IsGameOver() const;

	// Validates and applies a player's command through Game::submit.
	sov::CommandError Submit(const sov::Command& Command);
	// Plays the current seat with the AI when it is not a human seat. Returns false
	// when it is a human's turn, the game is over, or the seat failed to end its turn
	// (then the session stops stepping and Stalled() is true).
	bool StepAI();
	bool Stalled() const { return bStalled; }
	// Online: exchanges messages and takes in applied commands; true when the game changed.
	// Hot seat: notices when the turn has passed to another human. Call every frame.
	bool Poll();

	// ---- online
	ESovNet NetMode() const { return Mode; }
	bool InLobby() const;
	// Seat table and status for the lobby screen.
	TArray<FString> LobbyLines() const;
	bool UsesSteam() const { return bSteam; }
	// Steam: opens the overlay to invite friends to the lobby.
	void InviteFriends();
	bool StartHostedGame(FString& OutError);
	void Chat(const FString& Text);
	// Joins, leaves, chat and resyncs since the last call.
	TArray<FString> TakeNotices();

	// ---- hot seat
	// Another human's turn has come: the screen is hidden until they take over.
	bool HandoverPending() const { return bHandover; }
	FString HandoverName() const;
	void TakeOver();

	// Bumped on every change to the game; observers resync when it moves.
	uint64 Revision() const { return Rev; }

private:
	const sov::Game* CurrentGame() const;

	std::unique_ptr<sov::Rules> Rules;
	std::unique_ptr<sov::GameSetup> CoreSetup;
	std::unique_ptr<sov::Game> Game;  // local play; declared after Rules: it holds a pointer to them
	std::unique_ptr<sov::net::Listener> Listener;
	std::unique_ptr<sov::net::Host> NetHost;      // after Rules and Listener: it refers to both
	std::unique_ptr<sov::net::Client> NetClient;
	ESovNet Mode = ESovNet::Local;
	uint64 Rev = 0;
	bool bStalled = false;
	int32 ViewSeat = 0;
	int32 AutoStartPlayers = 0;
	bool bSteam = false;
	FString LocalName;
	bool bHandover = false;
	const sov::Game* SeenGame = nullptr;  // online: which game object, and how long its log, at the last Poll
	size_t SeenLog = 0;
	TArray<FString> Notices;
};
