#include "SovSession.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

#include <algorithm>

#include "sovereign/ai.h"
#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"
#include "sovereign_net/session.h"

#include "SovNetLink.h"
#include "SovSteam.h"

FSovSetup FSovSetup::FromCommandLine()
{
	FSovSetup Setup;
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("SovSeed="), Setup.Seed);
	FParse::Value(Cmd, TEXT("SovPlayers="), Setup.Players);
	FParse::Value(Cmd, TEXT("SovSize="), Setup.MapSize);
	if (FParse::Param(Cmd, TEXT("SovSpectate")))
	{
		Setup.bHumanSeat0 = false;
	}
	Setup.bBattleDemo = FParse::Param(Cmd, TEXT("SovBattleDemo"));
	Setup.bNavalDemo = FParse::Param(Cmd, TEXT("SovNavalDemo"));
	Setup.bDiploDemo = FParse::Param(Cmd, TEXT("SovDiploDemo"));
	FParse::Value(Cmd, TEXT("SovHotSeat="), Setup.HumanSeats);
	FParse::Value(Cmd, TEXT("SovPort="), Setup.Port);
	FParse::Value(Cmd, TEXT("SovName="), Setup.PlayerName);
	FParse::Value(Cmd, TEXT("SovAutoStart="), Setup.AutoStartPlayers);
	if (FParse::Param(Cmd, TEXT("SovSteam")))
	{
		Setup.bSteam = true;
		if (Setup.Net == ESovNet::Local) Setup.Net = ESovNet::Join;  // alone: join through an invite
	}
	FParse::Value(Cmd, TEXT("SovSteamLobby="), Setup.SteamLobby);
	if (FParse::Param(Cmd, TEXT("SovHost")))
	{
		Setup.Net = ESovNet::Host;
		Setup.HumanSeats = 2;
		FParse::Value(Cmd, TEXT("SovHumans="), Setup.HumanSeats);
	}
	if (FParse::Value(Cmd, TEXT("SovJoin="), Setup.JoinAddress))
	{
		Setup.Net = ESovNet::Join;
	}
	Setup.HumanSeats = FMath::Clamp(Setup.HumanSeats, 1, FMath::Max(1, Setup.Players));
	return Setup;
}

bool FSovSetup::HasStartOptions()
{
	static const TCHAR* const Options[] = {TEXT("SovSeed="), TEXT("SovPlayers="), TEXT("SovSize="), TEXT("SovSpectate"), TEXT("SovBattleDemo"),
		TEXT("SovNavalDemo"), TEXT("SovDiploDemo"), TEXT("SovHotSeat="), TEXT("SovHost"), TEXT("SovJoin="), TEXT("SovSteam"), TEXT("SovQuickStart")};
	const FString Cmd = FCommandLine::Get();
	for (const TCHAR* O : Options)
	{
		if (Cmd.Contains(FString(TEXT("-")) + O)) return true;
	}
	return false;
}

FString FSovSetup::DefaultRulesDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/rules")));
}

FSovSession::FSovSession() = default;
FSovSession::~FSovSession() = default;

bool FSovSession::Start(const FSovSetup& Setup, FString& OutError)
{
	Game.reset();
	NetHost.reset();
	NetClient.reset();
	Listener.reset();
	Mode = Setup.Net;
	AutoStartPlayers = Setup.AutoStartPlayers;
	Demos = Setup;
	if (bSteam && FSovSteam::Get()) FSovSteam::Get()->LeaveLobby();
	bSteam = Setup.bSteam;
	LocalName = Setup.PlayerName;
	bStalled = false;
	bHandover = false;
	ViewSeat = 0;
	SeenGame = nullptr;
	SeenLog = 0;
	Rules = std::make_unique<sov::Rules>();
	std::string Error;
	const FString Dir = Setup.RulesDir.IsEmpty() ? FSovSetup::DefaultRulesDir() : Setup.RulesDir;
	if (!Rules->load({std::string(TCHAR_TO_UTF8(*Dir))}, &Error))
	{
		OutError = FString::Printf(TEXT("rules (%s): %s"), *Dir, UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}
	if (Rules->civs.empty() || Setup.Players < 1)
	{
		OutError = TEXT("no civilizations to seat");
		return false;
	}
	CoreSetup = std::make_unique<sov::GameSetup>();
	CoreSetup->seed = Setup.Seed;
	CoreSetup->mapSize = TCHAR_TO_UTF8(*Setup.MapSize);
	CoreSetup->liveBattles = Setup.bHumanSeat0;  // melee with the human's leader stack can be fought live
	// Natural disasters: -SovDisasters=0..4 (Minimal..Hyperreal), -1 for none; Moderate by default.
	int32 Disasters = CoreSetup->disasterIntensity;
	if (FParse::Value(FCommandLine::Get(), TEXT("SovDisasters="), Disasters)) CoreSetup->disasterIntensity = FMath::Clamp(Disasters, -1, 4);
	// Barbarian Clans mode: -SovClans (camps can be bribed, hired, incited and grow into city-states).
	CoreSetup->barbarianClans = FParse::Param(FCommandLine::Get(), TEXT("SovClans"));
	// Monopolies and Corporations mode: -SovMonopolies (Industries and Corporations on luxuries, Monopolies).
	CoreSetup->monopolies = FParse::Param(FCommandLine::Get(), TEXT("SovMonopolies"));
	// Difficulty: -SovDifficulty=0..7 (Settler .. Prince 3 .. Deity); Prince by default.
	CoreSetup->difficulty = FMath::Clamp(Setup.Difficulty, 0, 7);
	int32 Difficulty = CoreSetup->difficulty;
	if (FParse::Value(FCommandLine::Get(), TEXT("SovDifficulty="), Difficulty)) CoreSetup->difficulty = FMath::Clamp(Difficulty, 0, 7);
	for (int32 i = 0; i < Setup.Players; ++i)
	{
		const sov::CivType& Civ = Rules->civs[static_cast<size_t>(i) % Rules->civs.size()];
		CoreSetup->players.push_back({Civ.id, Setup.bHumanSeat0 && i < Setup.HumanSeats});
	}
	// The local human's profile from earlier games (leader doc §10, player modelling): it enters the
	// game through the setup, so replays and every machine online see the same profile.
	if (Setup.bHumanSeat0 && !CoreSetup->players.empty() && Mode != ESovNet::Join)
	{
		FString Text;
		sov::PlayerProfile Loaded;
		if (FFileHelper::LoadFileToString(Text, *ProfilePath(Setup.PlayerName)) && sov::profileFromText(TCHAR_TO_UTF8(*Text), Loaded))
		{
			CoreSetup->players[0].hasProfile = true;
			CoreSetup->players[0].profile = Loaded;
		}
	}
	if (bSteam)
	{
#if SOV_WITH_STEAM
		FString SteamError;
		if (Mode == ESovNet::Local || !FSovSteam::Start(SteamError))
		{
			OutError = Mode == ESovNet::Local ? FString(TEXT("Steam play needs hosting or joining")) : SteamError;
			return false;
		}
		LocalName = FSovSteam::Get()->PersonaName();
#else
		OutError = TEXT("Steam is not available on this platform");
		return false;
#endif
	}
	const std::string Name = TCHAR_TO_UTF8(*LocalName);
#if SOV_WITH_STEAM
	if (bSteam && Mode == ESovNet::Host)
	{
		FSovSteam::Get()->CreateLobby(Setup.Players);
		Listener = FSovSteam::Get()->MakeListener();
		NetHost = std::make_unique<sov::net::Host>(*Rules, *CoreSetup, *Listener, Name, 0);
		Notices.Add(TEXT("Opening a Steam lobby..."));
		++Rev;
		return true;
	}
	if (bSteam && Mode == ESovNet::Join)
	{
		if (Setup.SteamLobby != 0) FSovSteam::Get()->JoinLobby(Setup.SteamLobby);
		Notices.Add(Setup.SteamLobby != 0 ? FString(TEXT("Joining the Steam lobby..."))
										  : FString(TEXT("Waiting for a Steam invite: accept a friend's invite in the Steam overlay (Shift+Tab).")));
		++Rev;
		return true;
	}
#endif
	if (Mode == ESovNet::Host)
	{
		// The lobby: others claim the human seats; StartHostedGame creates the game.
		auto L = std::make_unique<FSovTcpListener>(Setup.Port);
		if (!L->IsListening())
		{
			OutError = FString::Printf(TEXT("cannot listen on port %d"), Setup.Port);
			return false;
		}
		Listener = std::move(L);
		NetHost = std::make_unique<sov::net::Host>(*Rules, *CoreSetup, *Listener, Name, 0);
		Notices.Add(FString::Printf(TEXT("Hosting on port %d. Waiting for players; Enter starts the game."), Setup.Port));
		++Rev;
		return true;
	}
	if (Mode == ESovNet::Join)
	{
		std::unique_ptr<sov::net::Link> Link = FSovTcpLink::Connect(Setup.JoinAddress, Setup.Port);
		if (!Link)
		{
			OutError = FString::Printf(TEXT("nobody answered at %s:%d"), *Setup.JoinAddress, Setup.Port);
			return false;
		}
		NetClient = std::make_unique<sov::net::Client>(*Rules, std::move(Link), Name);
		Notices.Add(FString::Printf(TEXT("Connected to %s:%d. Waiting for the host to start."), *Setup.JoinAddress, Setup.Port));
		++Rev;
		return true;
	}
	Game = sov::Game::create(*Rules, *CoreSetup, &Error);
	if (!Game)
	{
		OutError = FString::Printf(TEXT("create: %s"), UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}
	ApplyDemos(Setup);
	++Rev;
	return true;
}

// The developer starts (-SovBattleDemo, -SovNavalDemo, -SovDiploDemo) reshape the new game in Game.
void FSovSession::ApplyDemos(const FSovSetup& Setup)
{
	if (Setup.bBattleDemo && Setup.Players >= 2)
	{
		// A hand-made opening for trying live battles: an escorted leader meets an enemy warrior.
		sov::GameState S = Game->state();
		const sov::Unit* Leader = Game->leaderOf(0);
		sov::Unit* Guard = nullptr;
		sov::Unit* Foe = nullptr;
		for (sov::Unit& U : S.units)
		{
			const bool bMilitary = Rules->units[static_cast<size_t>(U.type)].layer == sov::UnitLayer::Military;
			if (bMilitary && U.owner == 0 && !Guard) Guard = &U;
			if (bMilitary && U.owner == 1 && !Foe) Foe = &U;
		}
		if (Leader && Guard && Foe)
		{
			Guard->pos = Leader->pos;
			for (int32 D : {1, 0, 2, 3, 4, 5})  // east first
			{
				const std::optional<sov::Hex> N = S.grid.neighbor(Leader->pos, static_cast<sov::Dir>(D));
				if (N && sov::isLandPassable(S, *Rules, *N) && !S.cityAt(*N))
				{
					Foe->pos = *N;
					break;
				}
			}
			S.players[0].relations[1].war = S.players[1].relations[0].war = true;
			S.players[0].relations[1].since = S.players[1].relations[0].since = 1;
			Game = sov::Game::fromScenario(*Rules, std::move(S));
		}
	}
	if (Setup.bNavalDemo && Game)
	{
		// A hand-made opening for seeing ships: Shipbuilding, a galley and an embarked warrior
		// on the coast nearest the leader.
		sov::GameState S = Game->state();
		const sov::Unit* Leader = Game->leaderOf(0);
		sov::Player& P = S.players[0];
		sov::Game::fitPlayerToRules(P, *Rules);
		for (const char* Tech : {"TECH_SAILING", "TECH_SHIPBUILDING"})
		{
			P.techs.done[static_cast<size_t>(Rules->tech(Tech))] = 1;
		}
		std::vector<sov::Hex> Coast;
		if (Leader)
		{
			for (int32 R = 1; R <= 8 && Coast.size() < 2; ++R)
			{
				for (const sov::Hex& H : S.grid.within(Leader->pos, R))
				{
					const sov::TerrainType& T = Rules->terrains[static_cast<size_t>(S.plot(H).terrain)];
					if (T.water && !T.impassable && T.id != "TERRAIN_OCEAN" && !S.unitAt(H, sov::UnitLayer::Military, *Rules) &&
						std::find(Coast.begin(), Coast.end(), H) == Coast.end())
					{
						Coast.push_back(H);
						if (Coast.size() == 2) break;
					}
				}
			}
		}
		if (Coast.size() == 2)
		{
			for (int32 k = 0; k < 2; ++k)
			{
				sov::Unit U;
				U.id = S.nextUnitId++;
				U.type = Rules->unit(k == 0 ? "UNIT_GALLEY" : "UNIT_WARRIOR");
				U.owner = 0;
				U.pos = Coast[static_cast<size_t>(k)];
				U.movesLeft = sov::Fixed::fromInt(Rules->units[static_cast<size_t>(U.type)].moves);
				S.units.push_back(U);
			}
			Game = sov::Game::fromScenario(*Rules, std::move(S));
		}
	}
	if (Setup.bDiploDemo && Game && Setup.Players >= 2)
	{
		sov::GameState S = Game->state();
		for (sov::Player& P : S.players)
		{
			P.met.assign(S.players.size(), 1);
			P.gold = sov::Fixed::fromInt(200);
		}
		sov::Deal Gift;
		Gift.id = S.nextDealId++;
		Gift.from = 1;
		Gift.to = 0;
		Gift.turn = S.turn;
		Gift.items.push_back({sov::DealItemKind::Gold, 1, 30, sov::kNone});
		S.deals.push_back(Gift);
		Game = sov::Game::fromScenario(*Rules, std::move(S));
	}
}

FString FSovSession::ProfilePath(const FString& PlayerName)
{
	FString Safe;
	for (const TCHAR C : PlayerName)
	{
		Safe.AppendChar(FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_') ? C : TEXT('_'));
	}
	if (Safe.IsEmpty()) Safe = TEXT("Player");
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("Profiles"), Safe + TEXT(".txt"));
}

void FSovSession::SaveProfile() const
{
	const sov::Game* G = CurrentGame();
	if (!G || ViewSeat < 0 || static_cast<size_t>(ViewSeat) >= G->state().players.size() || !G->state().players[static_cast<size_t>(ViewSeat)].human)
	{
		return;
	}
	const sov::PlayerProfile* P = G->profile(static_cast<sov::PlayerId>(ViewSeat));
	if (!P || P->turnsObserved == 0)
	{
		return;
	}
	const FString Path = ProfilePath(LocalName);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(sov::profileToText(*P).c_str())), *Path);
}

const sov::Game* FSovSession::CurrentGame() const
{
	if (NetHost) return NetHost->game();
	if (NetClient) return NetClient->game();
	return Game.get();
}

bool FSovSession::IsActive() const
{
	return IsRunning() || NetHost != nullptr || (NetClient && NetClient->connected()) || (bSteam && Mode == ESovNet::Join && !NetClient);
}

int32 FSovSession::ViewPlayer() const
{
	if (NetHost) return NetHost->seat();
	if (NetClient) return NetClient->seat() >= 0 ? NetClient->seat() : 0;
	return ViewSeat;
}

bool FSovSession::IsHumanTurn() const
{
	const sov::Game* G = CurrentGame();
	if (!G)
	{
		return false;
	}
	const sov::GameState& S = G->state();
	return S.players[static_cast<size_t>(S.currentPlayer)].human;
}

bool FSovSession::IsGameOver() const
{
	const sov::Game* G = CurrentGame();
	return G && G->gameOver();
}

sov::CommandError FSovSession::Submit(const sov::Command& Command)
{
	if (NetHost)
	{
		if (!NetHost->game()) return sov::CommandError::BadPlayer;
		const sov::CommandError Result = NetHost->submit(Command);
		if (Result == sov::CommandError::Ok) ++Rev;
		return Result;
	}
	if (NetClient)
	{
		// Checked here for an answer at once; it takes effect when the host's order comes back.
		const sov::Game* G = NetClient->game();
		if (!G) return sov::CommandError::BadPlayer;
		const sov::CommandError Check = G->validate(Command);
		if (Check != sov::CommandError::Ok) return Check;
		return NetClient->submit(Command) ? sov::CommandError::Ok : sov::CommandError::BadPlayer;
	}
	if (!Game)
	{
		return sov::CommandError::BadPlayer;
	}
	const sov::CommandError Result = Game->submit(Command);
	if (Result == sov::CommandError::Ok)
	{
		++Rev;
	}
	return Result;
}

bool FSovSession::Poll()
{
	bool bChanged = false;
#if SOV_WITH_STEAM
	if (bSteam)
	{
		FSovSteam::Tick();
		FSovSteam* Steam = FSovSteam::Get();
		if (Steam && Mode == ESovNet::Join && !NetClient)
		{
			// An invite accepted in the overlay, then the lobby's owner as our host.
			if (const uint64 Invite = Steam->TakeInvite())
			{
				Steam->JoinLobby(Invite);
				Notices.Add(TEXT("Joining the Steam lobby..."));
			}
			if (Steam->LobbyState() == FSovSteam::ELobby::In && !Steam->OwnsLobby())
			{
				NetClient = std::make_unique<sov::net::Client>(*Rules, Steam->MakeLink(Steam->LobbyOwner()), std::string(TCHAR_TO_UTF8(*LocalName)));
				Notices.Add(TEXT("In the lobby. Waiting for the host to start."));
				bChanged = true;
			}
		}
		static FSovSteam::ELobby Shown = FSovSteam::ELobby::None;
		if (Steam && Steam->LobbyState() != Shown)
		{
			Shown = Steam->LobbyState();
			if (Shown == FSovSteam::ELobby::In && Mode == ESovNet::Host) Notices.Add(TEXT("Steam lobby open. F: invite friends."));
			if (Shown == FSovSteam::ELobby::Failed) Notices.Add(Steam->LastError());
			bChanged = true;
		}
	}
#endif
	if (NetHost) NetHost->poll();
	if (NetHost && !NetHost->started() && AutoStartPlayers > 0)
	{
		int32 Joined = 0;
		for (size_t i = 0; i < NetHost->seats().size(); ++i)
		{
			Joined += static_cast<sov::PlayerId>(i) != NetHost->seat() && NetHost->seats()[i].connected ? 1 : 0;
		}
		FString Error;
		if (Joined >= AutoStartPlayers && !StartHostedGame(Error))
		{
			Notices.Add(TEXT("Could not start: ") + Error);
			AutoStartPlayers = 0;
		}
	}
	if (NetClient) NetClient->poll();
	if (NetHost || NetClient)
	{
		for (const std::string& N : NetHost ? NetHost->takeNotices() : NetClient->takeNotices())
		{
			Notices.Add(UTF8_TO_TCHAR(N.c_str()));
		}
		const sov::Game* G = CurrentGame();
		const size_t Log = G ? G->log().size() : 0;
		if (G != SeenGame || Log != SeenLog)
		{
			SeenGame = G;
			SeenLog = Log;
			bChanged = true;
		}
	}
	// Hot seat: the turn has passed to another human on this machine.
	if (Game && !bHandover && !Game->gameOver())
	{
		const sov::GameState& S = Game->state();
		const sov::Player& Cur = S.players[static_cast<size_t>(S.currentPlayer)];
		const sov::PlayerId Live = Game->battlePending() ? S.pendingBattle.liveFor : sov::kNoPlayer;
		const sov::PlayerId Next = Live != sov::kNoPlayer ? Live : (Cur.human ? S.currentPlayer : sov::kNoPlayer);
		if (Next != sov::kNoPlayer && Next != ViewSeat)
		{
			bHandover = true;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		++Rev;
	}
	return bChanged;
}

bool FSovSession::InLobby() const
{
	return (NetHost && !NetHost->started()) || (NetClient && !NetClient->inGame()) || (bSteam && Mode == ESovNet::Join && !NetClient);
}

void FSovSession::InviteFriends()
{
#if SOV_WITH_STEAM
	if (bSteam && FSovSteam::Get()) FSovSteam::Get()->InviteFriends();
#endif
}

TArray<FString> FSovSession::LobbyLines() const
{
	TArray<FString> Lines;
	const std::vector<sov::net::SeatInfo>* Seats = NetHost ? &NetHost->seats() : NetClient ? &NetClient->seats() : nullptr;
	if (!Seats)
	{
		return Lines;
	}
	for (size_t i = 0; i < Seats->size(); ++i)
	{
		const sov::net::SeatInfo& S = (*Seats)[i];
		const FString Who = S.connected ? FString(UTF8_TO_TCHAR(S.name.c_str())) : S.human ? FString(TEXT("(open)")) : FString(TEXT("AI"));
		const FString Civ = i < CoreSetup->players.size() ? FString(UTF8_TO_TCHAR(CoreSetup->players[i].civ.c_str())) : FString();
		const bool bMine = static_cast<int32>(i) == ViewPlayer();
		Lines.Add(FString::Printf(TEXT("Seat %d: %s%s"), static_cast<int32>(i) + 1, *Who, bMine ? TEXT("  (you)") : TEXT("")));
	}
	return Lines;
}

bool FSovSession::StartHostedGame(FString& OutError)
{
	if (!NetHost)
	{
		return false;
	}
	std::string Error;
	if (Demos.bBattleDemo || Demos.bNavalDemo || Demos.bDiploDemo)
	{
		// A developer start: made here with the lobby's seats, reshaped, then handed to the host.
		Game = sov::Game::create(*Rules, NetHost->lobbySetup(), &Error);
		if (!Game)
		{
			OutError = UTF8_TO_TCHAR(Error.c_str());
			return false;
		}
		ApplyDemos(Demos);
		NetHost->start(std::move(Game));
		Game.reset();
		++Rev;
		return true;
	}
	if (!NetHost->start(&Error))
	{
		OutError = UTF8_TO_TCHAR(Error.c_str());
		return false;
	}
	++Rev;
	return true;
}

void FSovSession::SendRelay(int32 ToSeat, const std::vector<uint8_t>& Blob)
{
	if (NetHost) NetHost->sendRelay(static_cast<sov::PlayerId>(ToSeat), Blob);
	if (NetClient) NetClient->sendRelay(static_cast<sov::PlayerId>(ToSeat), Blob);
}

TArray<TPair<int32, std::vector<uint8_t>>> FSovSession::TakeRelays()
{
	TArray<TPair<int32, std::vector<uint8_t>>> Out;
	if (NetHost || NetClient)
	{
		for (auto& R : NetHost ? NetHost->takeRelays() : NetClient->takeRelays())
		{
			Out.Add(TPair<int32, std::vector<uint8_t>>(R.first, std::move(R.second)));
		}
	}
	return Out;
}

void FSovSession::Chat(const FString& Text)
{
	const std::string T = TCHAR_TO_UTF8(*Text);
	if (NetHost) NetHost->chat(T);
	if (NetClient) NetClient->chat(T);
}

TArray<FString> FSovSession::TakeNotices()
{
	TArray<FString> Out = MoveTemp(Notices);
	Notices.Reset();
	return Out;
}

FString FSovSession::HandoverName() const
{
	if (!Game)
	{
		return FString();
	}
	const sov::GameState& S = Game->state();
	const sov::PlayerId Live = Game->battlePending() ? S.pendingBattle.liveFor : sov::kNoPlayer;
	const sov::Player& P = S.players[static_cast<size_t>(Live != sov::kNoPlayer ? Live : S.currentPlayer)];
	return UTF8_TO_TCHAR(P.leaderName.c_str());
}

void FSovSession::TakeOver()
{
	if (!Game || !bHandover)
	{
		return;
	}
	const sov::GameState& S = Game->state();
	const sov::PlayerId Live = Game->battlePending() ? S.pendingBattle.liveFor : sov::kNoPlayer;
	ViewSeat = Live != sov::kNoPlayer ? Live : S.currentPlayer;
	bHandover = false;
	++Rev;
}

bool FSovSession::StepAI()
{
	if (!Game || bStalled || Game->gameOver() || IsHumanTurn() || Game->battlePending())
	{
		return false;
	}
	const sov::GameState& S = Game->state();
	const int Turn = S.turn;
	const sov::PlayerId Seat = S.currentPlayer;
	sov::ai::playTurn(*Game);
	++Rev;
	// An attack on the human's leader stack pauses the seat until the battle is settled.
	if (Game->battlePending())
	{
		return true;
	}
	if (!Game->gameOver() && Game->state().turn == Turn && Game->state().currentPlayer == Seat)
	{
		bStalled = true;
		return false;
	}
	return true;
}
