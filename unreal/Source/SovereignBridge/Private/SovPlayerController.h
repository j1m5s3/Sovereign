// Seat 0's hands: camera, selection and orders. Every order leaves here as a
// sov::Command through USovGameSubsystem::Submit; nothing else changes the game.
// Keys are polled each tick, so no input assets are needed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "sovereign/commands.h"

#include "SovBattleSim.h"
#include "SovDiplomacy.h"

#include "SovPlayerController.generated.h"

class ASovCameraPawn;
class ASovHUD;
class ASovStreetScene;
class ASovBattleScene;
class ASovWalker;
class ASovMapActor;
class USovGameSubsystem;
class SSovDiplomacyPanel;

UCLASS()
class ASovPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASovPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	void SetMap(ASovMapActor* InMap) { Map = InMap; }

	// ---- street scenes (step 4): the leader walks its City Center
	bool InStreet() const { return Street != nullptr; }
	const ASovStreetScene* GetStreet() const { return Street; }
	const ASovWalker* GetWalker() const { return Walker; }
	// "" when nobody is in reach; otherwise what F would do.
	FString StreetPrompt() const;

	// ---- live battles (step 5)
	bool InBattle() const { return Battle != nullptr; }
	const FSovBattleSim& GetBattleSim() const { return Sim; }
	int32 GetBattleSquad() const { return BattleSquad; }
	bool BattleSettled() const { return bBattleSent; }
	const FSovBattleResult& BattleOutcome() const { return Outcome; }
	// Centres the camera on the viewer's capital, else their first unit.
	void CenterOnHome();

	// ---- diplomacy (leader doc §10): talking with an AI leader
	bool InDiplomacy() const { return Talk.IsValid(); }

protected:
	virtual void BeginPlay() override;

	enum class EChooser : uint8
	{
		None,
		Production,
		Research,
		Civic,
		Improvement,
		Gear,
		Throne,
		Assassins,
		Promotion,
		GreatPeople,
		Pantheon,
		ReligionFounder,
		ReligionFollower,
		Evangelize,
		TradeRoute,
		CityStates,
		Diplomacy
	};

	struct FChoice
	{
		FString Label;
		sov::Command Command;
	};

	USovGameSubsystem* Subsystem() const;
	ASovCameraPawn* CameraPawn() const;
	bool MyTurn() const;
	sov::PlayerId Me() const;

	void UpdateCamera(float DeltaTime);
	void HandleOrders();
	bool HexUnderCursor(int32& OutX, int32& OutY) const;
	void ClickSelect(int32 X, int32 Y);
	void ClickOrder(int32 X, int32 Y);
	void SelectUnit(int32 Id, bool bCenter);
	void SelectCity(int32 Id, bool bCenter);
	void SelectNextUnit();
	void EndTurn();
	// Sends the command; true when the core accepted it.
	bool Send(const sov::Command& Command);
	void AfterUnitOrder();

	void OpenChooser(EChooser Kind);
	void EnterStreet();
	void StartBattle();
	void UpdateBattle(float DeltaTime);
	void ExitBattle();
	void LookAround(float DeltaTime);
	void ExitStreet();
	void UpdateStreet(float DeltaTime);
	void Pick(int32 Index);
	void UpdatePanel();
	// Online and hot seat: the lobby, the hand-over screen, and the chat line (M).
	// True when they took this frame's input.
	bool HandleSessionScreens();
	void OpenChat();
	void CloseChat();
	void OpenDiplomacy(sov::PlayerId Leader);
	void UpdateDiplomacy();
	void CloseDiplomacy();
	// The leader's offer to us that waits for an answer, if any.
	const sov::Deal* OfferFrom(sov::PlayerId Leader) const;

	UPROPERTY()
	TObjectPtr<ASovMapActor> Map;

	int32 SelectedUnit = -1;
	int32 SelectedCity = -1;
	EChooser Chooser = EChooser::None;
	TArray<FChoice> Choices;
	int32 ChooserPage = 0;
	FString ChooserTitle;
	bool bWasMyTurn = false;

	UPROPERTY()
	TObjectPtr<ASovStreetScene> Street;

	UPROPERTY()
	TObjectPtr<ASovWalker> Walker;

	UPROPERTY()
	TObjectPtr<APawn> MapPawn;

	UPROPERTY()
	TObjectPtr<ASovBattleScene> Battle;

	FSovBattleSim Sim;
	int32 BattleSquad = -1;  // the squad the human is ordering (-1: all three)
	sov::UnitId ReligionUnit = sov::kNoUnit;  // the Prophet or Apostle a religion chooser is for
	sov::TypeIndex PendingFounder = sov::kNone;  // the Founder belief picked before the Follower
	FSovBattleResult Outcome;
	bool bBattleSent = false;
	float BattleExitTimer = 0.f;

	float PanSpeed = 1.4f;  // fraction of camera height per second

	TUniquePtr<FSovDiplomacyTalk> Talk;
	TSharedPtr<SSovDiplomacyPanel> DiplomacyPanel;
	bool bLeavingTalk = false;  // the summary is being written; the screen closes when it is in
	TSharedPtr<class SWidget> ChatBox;
	bool bCenteredOnGame = false;  // online games arrive after BeginPlay: centre on them once
};
