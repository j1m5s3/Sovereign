#include "SovGameSubsystem.h"

#include "sovereign/challenge.h"
#include "sovereign/commands.h"
#include "sovereign/game.h"
#include "sovereign/serialize.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Algo/Reverse.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovereign, Log, All);

bool USovGameSubsystem::StartGame(const FSovSetup& Setup)
{
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		LastMessage = FString::Printf(TEXT("Could not start the game: %s"), *Error);
		UE_LOG(LogSovereign, Error, TEXT("%s"), *LastMessage);
		return false;
	}
	bWroteEnd = false;
	LastMessage = Setup.Net == ESovNet::Local
		? FString::Printf(TEXT("New game: seed %llu, %d players, %s%s"), Setup.Seed, Setup.Players, *Setup.MapSize,
			  Setup.bHumanSeat0 ? (Setup.HumanSeats > 1 ? TEXT(" (hot seat)") : TEXT("")) : TEXT(" (spectating)"))
		: FString();
	UE_LOG(LogSovereign, Log, TEXT("%s"), *LastMessage);
	OnStateChanged.Broadcast();
	return true;
}

sov::CommandError USovGameSubsystem::Submit(const sov::Command& Command)
{
	const sov::CommandError Result = Session.Submit(Command);
	if (Result == sov::CommandError::Ok)
	{
		LastMessage.Reset();
		OnStateChanged.Broadcast();
	}
	else
	{
		LastMessage = FString::Printf(TEXT("%s refused: %s"), UTF8_TO_TCHAR(sov::describe(Command).c_str()),
			UTF8_TO_TCHAR(sov::commandErrorName(Result)));
	}
	return Result;
}

void USovGameSubsystem::Tick(float DeltaTime)
{
	if (Session.Poll())
	{
		OnStateChanged.Broadcast();
	}
	for (const FString& N : Session.TakeNotices())
	{
		NetLines.Add(N);
		UE_LOG(LogSovereign, Log, TEXT("%s"), *N);
	}
	FString ChronicleNote;
	if (Chronicle.Poll(ChronicleNote))
	{
		LastMessage = ChronicleNote;
	}
	while (NetLines.Num() > 8)
	{
		NetLines.RemoveAt(0);
	}
	// The game is over for this player (a victory, or eliminated): the play profile and the rivals'
	// memories travel to the next game whether or not it was saved (player-retention §1).
	if (Session.IsRunning() && !bWroteEnd)
	{
		const sov::GameState& S = Session.GetGame().state();
		const int32 Seat = Session.ViewPlayer();
		if (Session.IsGameOver() || (Seat >= 0 && Seat < static_cast<int32>(S.players.size()) && !S.players[static_cast<size_t>(Seat)].alive))
		{
			Session.SaveProfile();
			// The reign enters the Hall of Sovereigns, and its chronicle is written (player-retention §2).
			const std::string Entry = Session.GetGame().hallEntry(static_cast<sov::PlayerId>(Seat));
			if (!Entry.empty() && Session.GetGame().state().players[static_cast<size_t>(Seat)].human)
			{
				FFileHelper::SaveStringToFile(FDateTime::Now().ToString(TEXT("%Y-%m-%d  ")) + UTF8_TO_TCHAR(Entry.c_str()) + LINE_TERMINATOR, *HallPath(),
					FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
				WriteChronicle();
			}
			// The weekly challenge: the result on the local board, and the save to submit (player-retention §3).
			if (const int32 Week = Session.GetChallengeWeek(); Week >= 0)
			{
				const sov::Challenge Ch = sov::weeklyChallenge(Session.GetRules(), Week);
				const sov::ChallengeResult R = sov::judgeChallenge(Session.GetGame(), Ch);
				const FString Line = FString::Printf(TEXT("week %d  %s  turn %d  score %d"), Week, R.goalMet ? TEXT("goal met") : TEXT("goal missed"), R.turn, R.score);
				FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *ChallengesPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
				SaveGame(FString::Printf(TEXT("challenge week %d"), Week));
				LastMessage = FString::Printf(TEXT("Weekly challenge: %s. Saved as \"challenge week %d\" to submit."), *Line, Week);
			}
			bWroteEnd = true;
		}
	}
	// Online, the host plays the AI seats inside Poll; there is nothing to step here.
	if (!Session.IsRunning() || Session.NetMode() != ESovNet::Local || Session.HandoverPending())
	{
		return;
	}
	if (Session.IsHumanTurn() || Session.IsGameOver() || Session.Stalled() || Session.GetGame().battlePending())
	{
		SinceLastSeat = 0.f;
		return;
	}
	SinceLastSeat += DeltaTime;
	if (SinceLastSeat < AISeatDelay)
	{
		return;
	}
	SinceLastSeat = 0.f;
	Session.StepAI();
	if (Session.Stalled())
	{
		LastMessage = FString::Printf(TEXT("AI seat %d did not end its turn; AI stopped"), Session.GetGame().state().currentPlayer);
		UE_LOG(LogSovereign, Error, TEXT("%s"), *LastMessage);
	}
	OnStateChanged.Broadcast();
}

FString USovGameSubsystem::SavePath(const FString& Name)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), Name + TEXT(".sov"));
}

bool USovGameSubsystem::SaveGame(const FString& Name)
{
	if (!Session.IsRunning())
	{
		return false;
	}
	Session.SaveProfile();  // the player model travels to the next game (leader doc §10)
	const std::vector<uint8_t> Bytes = sov::saveGame(Session.GetGame());
	TArray<uint8> Data;
	Data.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
	const FString Path = SavePath(Name);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveArrayToFile(Data, *Path);
}

bool USovGameSubsystem::LoadGame(const FString& Name)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *SavePath(Name)))
	{
		LastMessage = FString::Printf(TEXT("No saved game named %s"), *Name);
		return false;
	}
	const std::vector<uint8_t> Bytes(Data.GetData(), Data.GetData() + Data.Num());
	FString Error;
	if (!Session.LoadLocal(Bytes, Error))
	{
		LastMessage = FString::Printf(TEXT("Could not load %s: %s"), *Name, *Error);
		UE_LOG(LogSovereign, Error, TEXT("%s"), *LastMessage);
		return false;
	}
	bWroteEnd = false;
	LastMessage = FString::Printf(TEXT("Loaded %s (turn %d)"), *Name, Session.GetGame().state().turn);
	UE_LOG(LogSovereign, Log, TEXT("%s"), *LastMessage);
	OnStateChanged.Broadcast();
	return true;
}

TArray<TPair<FString, FDateTime>> USovGameSubsystem::ListSaves()
{
	TArray<FString> Files;
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"));
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.sov")), true, false);
	TArray<TPair<FString, FDateTime>> Out;
	for (const FString& F : Files)
	{
		Out.Add({FPaths::GetBaseFilename(F), IFileManager::Get().GetTimeStamp(*FPaths::Combine(Dir, F))});
	}
	Out.Sort([](const TPair<FString, FDateTime>& A, const TPair<FString, FDateTime>& B) { return A.Value > B.Value; });
	return Out;
}

TStatId USovGameSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovGameSubsystem, STATGROUP_Tickables);
}

FString USovGameSubsystem::ChallengesPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("Challenges.txt"));
}

TArray<FString> USovGameSubsystem::ChallengeResults(int32 Week)
{
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *ChallengesPath());
	const FString Prefix = FString::Printf(TEXT("week %d "), Week);
	Lines.RemoveAll([&](const FString& L) { return !L.StartsWith(Prefix); });
	return Lines;
}

FString USovGameSubsystem::HallPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("Hall.txt"));
}

TArray<FString> USovGameSubsystem::HallEntries()
{
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *HallPath());
	Lines.RemoveAll([](const FString& L) { return L.TrimStartAndEnd().IsEmpty(); });
	Algo::Reverse(Lines);
	return Lines;
}

void USovGameSubsystem::WriteChronicle()
{
	if (!Session.IsRunning() || Chronicle.IsBusy())
	{
		return;
	}
	const sov::Game& G = Session.GetGame();
	const sov::PlayerId Seat = static_cast<sov::PlayerId>(Session.ViewPlayer());
	const sov::Player& P = G.state().players[static_cast<size_t>(Seat)];
	const FString Civ = P.civ == sov::kNone ? FString(TEXT("Sovereign")) : FString(UTF8_TO_TCHAR(G.rules().civs[static_cast<size_t>(P.civ)].name.c_str()));
	const FString Ruler = P.leaderName.empty() ? Civ : FString(UTF8_TO_TCHAR(P.leaderName.c_str()));
	int32 Port = 8080;
	FParse::Value(FCommandLine::Get(), TEXT("SovLlmPort="), Port);
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sovereign"), TEXT("Chronicles"),
		FString::Printf(TEXT("%s seed %llu turn %d.txt"), *Civ, G.state().setup.seed, G.state().turn));
	Chronicle.Start(FString::Printf(TEXT("%s of %s"), *Ruler, *Civ), G.chronicleLines(Seat), Port, Path);
	LastMessage = TEXT("The court historian is writing the chronicle...");
}