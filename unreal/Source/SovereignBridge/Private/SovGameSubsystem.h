// The game instance's one rules-core game. Actors read it; input reaches it only as
// sov::Command through Submit; AI seats are stepped here, one seat per tick.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "SovDiplomacy.h"
#include "SovLens.h"
#include "SovMinimap.h"
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
	// The chronicle of the viewer's reign, written to Saved/Sovereign/Chronicles off the game thread by the
	// local model or the script (player-retention §2); the note lands in LastMessage. Also done at the game's end.
	void WriteChronicle();
	bool WritingChronicle() const { return Chronicle.IsBusy(); }
	// The Hall of Sovereigns: one line per finished reign, newest first (Saved/Sovereign/Hall.txt).
	static TArray<FString> HallEntries();
	// The local board of weekly challenge results (Saved/Sovereign/Challenges.txt) for one week.
	static TArray<FString> ChallengeResults(int32 Week);
	// Achievements earned on this machine (Rules::achievements ids, Saved/Sovereign/Achievements.txt) and the
	// cosmetic chosen for the ruler's figure (a Rules::cosmetics id; empty: none), kept in Saved/Sovereign/cosmetic.txt.
	// Cosmetic only: never part of the game (player-retention §7).
	static TArray<FString> Achievements();
	static FString Cosmetic();
	static void SetCosmetic(const FString& Id);
	// Saved games on this machine, newest first: their names and when they were written.
	static TArray<TPair<FString, FDateTime>> ListSaves();

	// Fires after every change to the game (player command or an AI seat's turn).
	FSovStateChanged OnStateChanged;

	// Map lenses and the minimap (plan D, step 6): this machine's view, not part of the game. The game mode
	// fills the legend and the minimap whenever it redraws the map.
	ESovLens Lens = ESovLens::None;
	TArray<FSovLensKey> LensLegend;
	TSharedPtr<const FSovMinimapData> Minimap;
	// The mirror the map was last drawn from (the HUD's labels read it rather than building one each frame).
	TSharedPtr<const struct FSovMirror> Mirror;

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
	bool bWroteEnd = false;  // the profile, rivals, chronicle and Hall entry were written when the game ended for this player
	FSovChronicleWriter Chronicle;
	static FString HallPath();
	static FString ChallengesPath();
};
