#include "SovGameSubsystem.h"

#include "sovereign/commands.h"
#include "sovereign/game.h"
#include "sovereign/serialize.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
	LastMessage = FString::Printf(TEXT("New game: seed %llu, %d players, %s%s"), Setup.Seed, Setup.Players, *Setup.MapSize,
		Setup.bHumanSeat0 ? TEXT("") : TEXT(" (spectating)"));
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
	const std::vector<uint8_t> Bytes = sov::saveGame(Session.GetGame());
	TArray<uint8> Data;
	Data.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
	const FString Path = SavePath(Name);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveArrayToFile(Data, *Path);
}

TStatId USovGameSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovGameSubsystem, STATGROUP_Tickables);
}
