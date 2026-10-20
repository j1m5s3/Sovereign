// The weekly challenge board from the game (player-retention §3): the board server is board/ (sovboard serve); the
// game submits a finished challenge's save and reads the week's ranking through board/'s Client, over an engine socket.
// Both run on a worker thread and answer on the game thread.
#pragma once

#include "CoreMinimal.h"

struct FSovBoardEntry
{
	FString Name;
	bool bGoalMet = false;
	int32 Turn = 0, Score = 0;
};

namespace SovBoard
{
// host:port of the board server: -SovBoard= on the command line, else the settings ([Sovereign] Board in
// GameUserSettings); empty when none is set.
FString Address();
void SetAddress(const FString& HostPort);
// Submits a save; Done gets a line for the player ("accepted: rank 3 of 12", or why not).
void SubmitAsync(int32 Week, const FString& Name, TArray<uint8> Save, TFunction<void(bool bAccepted, const FString& Line)> Done);
// The week's ranking, best first; bOk false when the board did not answer.
void FetchAsync(int32 Week, TFunction<void(bool bOk, const TArray<FSovBoardEntry>& Entries)> Done);
// The same, blocking (for the worker thread, and tests): Line as SubmitAsync's; Fetch false when the board did not answer.
bool Submit(const FString& Board, int32 Week, const FString& Name, const TArray<uint8>& Save, FString& Line);
bool Fetch(const FString& Board, int32 Week, TArray<FSovBoardEntry>& Entries);
// A board name from a player name: 1-32 letters, digits, '_', '-' or '.'.
FString CleanName(const FString& Name);
}  // namespace SovBoard
