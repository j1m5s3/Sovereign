// The live battle in Unreal: an adapter over the shared battle simulation
// (battle/include/sovereign_battle, leader doc §9), which the trainer and the headless tests
// run too. The human fights as the leader and orders their own squads; the trained battle
// AI (leader doc §10, layer 3) commands the other side. Plain C++ so tests can run it
// without a world; the scene draws it each frame.
#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "sovereign_battle/commander.h"
#include "sovereign_battle/sim.h"
THIRD_PARTY_INCLUDES_END

struct FSovBattleUnitSpec
{
	FString Name;
	int32 Owner = 0;
	int32 Strength = 20;  // Civ combat strength in this fight (attacking or defending)
	int32 Hp = 100;
	bool bLeaderIsUnit = false;  // an unescorted leader: the unit is the leader alone
};

struct FSovBattleSpec
{
	FSovBattleUnitSpec Attacker, Defender;
	int32 HumanSide = 0;          // 0: attacker's side, 1: defender's side
	bool bLeaderPresent = true;   // the human's leader fights on its side
	int32 LeaderStrength = 20;
	int32 LeaderHp = 100;
	int32 Seed = 1;
	float TimeLimit = 180.f;      // LIVE_BATTLE_SECONDS: settles from the current state when reached
};

struct FSovSoldier
{
	FVector2D Pos = FVector2D::ZeroVector;
	float Hp = 10.f;
	float MaxHp = 10.f;
	int32 Side = 0;      // 0 attacker, 1 defender
	int32 Squad = -1;    // 0 left, 1 centre, 2 right; -1 an escorted leader
	bool bLeader = false;
	bool bAlive = true;
};

struct FSovBattleResult
{
	int32 ToAttacker = 0;   // HP damage to the attacking unit
	int32 ToDefender = 0;   // HP damage to the defending unit
	int32 LeaderWound = 0;  // HP the escorted leader lost fighting in person
	bool bTimedOut = false;
	int32 Winner = -1;      // side that held the field (-1: neither)
};

class FSovBattleSim
{
public:
	// The trained commander shipped in data/battle_ai (loaded once), or null when the file is
	// missing or does not match this build (the enemy then advances, the scripted baseline).
	static std::shared_ptr<const sov::battle::Policy> TrainedPolicy();

	// Policy: who commands the side the human does not control (null: the scripted baseline).
	void Start(const FSovBattleSpec& Spec, std::shared_ptr<const sov::battle::Policy> Policy = nullptr);
	// Advances the fight. The human's leader moves along LeaderMove (unit vector) and strikes
	// when bStrike; with LeaderMove unset nobody is at the controls and the leader keeps to
	// its men.
	void Step(float Dt, TOptional<FVector2D> LeaderMove = {}, bool bStrike = false);
	bool Finished() const { return Sim.finished(); }
	FSovBattleResult Result() const;

	const TArray<FSovSoldier>& Soldiers() const { return Men; }
	int32 Alive(int32 Side) const { return Sim.alive(Side); }
	int32 Started(int32 Side) const { return Sim.started(Side); }
	float TimeLeft() const { return Sim.timeLeft(); }
	int32 LeaderIndex() const { return Sim.humanLeader() >= 0 ? Sim.humanLeader() : INDEX_NONE; }
	const FSovBattleSpec& GetSpec() const { return Spec; }

	// The human's orders to their own squads (Squad -1: all three).
	void SetOrder(int32 Side, sov::battle::Order Order, int32 Squad = -1);
	sov::battle::Order GetOrder(int32 Side, int32 Squad = 1) const { return Sim.order(Side, Squad); }
	// Tab: charge (advance) or hold, for all of a side's squads.
	void SetCharge(int32 Side, bool bCharge) { SetOrder(Side, bCharge ? sov::battle::Order::Advance : sov::battle::Order::Hold); }
	bool Charging(int32 Side) const { return Sim.order(Side, 1) != sov::battle::Order::Hold; }
	bool EnemyTrained() const { return bEnemyTrained; }

private:
	void Mirror();

	FSovBattleSpec Spec;
	sov::battle::Sim Sim;
	sov::battle::Commander Enemy = sov::battle::Commander::fixed(sov::battle::Order::Advance);
	sov::battle::Commander Idle = sov::battle::Commander::fixed(sov::battle::Order::Advance);  // the human's squads when nobody plays
	bool bEnemyTrained = false;
	TArray<FSovSoldier> Men;
};
