// The game instance's one rules-core game. Actors read it; input reaches it only as
// sov::Command through Submit; AI seats are stepped here, one seat per tick.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "SovSession.h"

#include "SovGameSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FSovStateChanged);

UCLASS()
class USovGameSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Starts a new game with the setup (logs and keeps the error on failure).
	bool StartGame(const FSovSetup& Setup);

	bool IsRunning() const { return Session.IsRunning(); }
	bool IsActive() const { return Session.IsActive(); }  // running, or in an online lobby
	const FSovSession& GetSession() const { return Session; }
	FSovSession& GetSessionMut() { return Session; }
	const sov::Game& GetGame() const { return Session.GetGame(); }

	// Sends a player's command to the core; the result is also kept as the last message.
	sov::CommandError Submit(const sov::Command& Command);

	// Writes the game in the core's save format (live scenes autosave on entry, engine doc).
	bool SaveGame(const FString& Name);
	static FString SavePath(const FString& Name);
	// Resumes a game written by SaveGame (local play only).
	bool LoadGame(const FString& Name);
	// Saved games on this machine, newest first: their names and when they were written.
	static TArray<TPair<FString, FDateTime>> ListSaves();

	// Fires after every change to the game (player command or an AI seat's turn).
	FSovStateChanged OnStateChanged;

	FString LastMessage;
	// Online notices and chat, newest last (a few kept for the HUD).
	TArray<FString> NetLines;
	// Seconds between AI seats, so their turns can be watched.
	float AISeatDelay = 0.15f;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Session.IsActive(); }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
	FSovSession Session;
	float SinceLastSeat = 0.f;
	bool bWroteEnd = false;  // the profile and rivals were written when the game ended for this player
};
