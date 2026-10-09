// The live battle's screen (plan E): both armies' strength and the clock, the leader's health, the squads'
// orders as buttons (each presses the battle's key), and the result when it is settled.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

struct FSovBattleModel
{
	bool bOpen = false;
	bool bReplay = false;
	FString Title;                 // "Elizabeth I's army attacks the Warrior"; a replay's own title
	FString Attacker, Defender;    // names
	int32 AttackerAlive = 0, AttackerStarted = 1, DefenderAlive = 0, DefenderStarted = 1;
	int32 Seconds = 0;
	float LeaderHealth = -1.f;     // 0..1; below 0: no leader of ours on the field
	FString Orders[3];             // each squad's order
	int32 Squad = -1;              // the squad being ordered (-1: all)
	bool bCharging = false;
	bool bRemoteView = false;      // another machine runs this battle: our squads only
	FString Foe;                   // who leads the enemy
	FString Result;                // empty until it is settled
	FString ResultTitle;           // "Victory", "Defeat", "Stalemate"
	bool bWon = false;
};

class SSovBattleHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovBattleHud) {}
	SLATE_EVENT(TDelegate<void(FKey)>, OnKey)  // a button pressed a battle key
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetModel(const FSovBattleModel& InModel) { Model = InModel; }

private:
	TSharedRef<SWidget> KeyButton(const FString& Label, const FKey& Key, TFunction<bool()> Lit, const FString& Tip);

	FSovBattleModel Model;
	TDelegate<void(FKey)> OnKey;
};
