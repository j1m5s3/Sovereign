#include "SovSession.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "sovereign/ai.h"
#include "sovereign/game.h"

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
	return Setup;
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
	bStalled = false;
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
	for (int32 i = 0; i < Setup.Players; ++i)
	{
		const sov::CivType& Civ = Rules->civs[static_cast<size_t>(i) % Rules->civs.size()];
		CoreSetup->players.push_back({Civ.id, i == 0 && Setup.bHumanSeat0});
	}
	Game = sov::Game::create(*Rules, *CoreSetup, &Error);
	if (!Game)
	{
		OutError = FString::Printf(TEXT("create: %s"), UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}
	++Rev;
	return true;
}

bool FSovSession::IsHumanTurn() const
{
	if (!Game)
	{
		return false;
	}
	const sov::GameState& S = Game->state();
	return S.players[static_cast<size_t>(S.currentPlayer)].human;
}

bool FSovSession::IsGameOver() const
{
	return Game && Game->gameOver();
}

sov::CommandError FSovSession::Submit(const sov::Command& Command)
{
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

bool FSovSession::StepAI()
{
	if (!Game || bStalled || Game->gameOver() || IsHumanTurn())
	{
		return false;
	}
	const sov::GameState& S = Game->state();
	const int Turn = S.turn;
	const sov::PlayerId Seat = S.currentPlayer;
	sov::ai::playTurn(*Game);
	++Rev;
	if (!Game->gameOver() && Game->state().turn == Turn && Game->state().currentPlayer == Seat)
	{
		bStalled = true;
		return false;
	}
	return true;
}
