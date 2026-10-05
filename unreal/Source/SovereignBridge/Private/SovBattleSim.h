// A real-time melee between two Civ units, one of them with the human's leader fighting
// in person (leader doc §9). Soldiers follow their unit's HP; their blows follow the
// Civ strength difference (the same e^(0.04 x diff) shape as the core's damage), so the
// numbers decide most fights and skill shifts them. The core clamps whatever comes out
// to its band, so this simulation only has to be fair, not exact. Plain C++ so tests can
// run it without a world; the scene drives it each frame.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

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
	float Cooldown = 0.f;
	int32 Side = 0;      // 0 attacker, 1 defender
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
	static constexpr float Reach = 140.f;
	static constexpr float Speed = 260.f;
	static constexpr float SwingSeconds = 1.6f;

	void Start(const FSovBattleSpec& Spec);
	// Advances the fight. The human's leader moves along LeaderMove (unit vector, scaled by
	// speed) and strikes when bStrike; it is AI-driven when LeaderMove is nullopt.
	void Step(float Dt, TOptional<FVector2D> LeaderMove = {}, bool bStrike = false);
	bool Finished() const { return bFinished; }
	FSovBattleResult Result() const;

	const TArray<FSovSoldier>& Soldiers() const { return Men; }
	int32 Alive(int32 Side) const;
	int32 Started(int32 Side) const { return Initial[Side]; }
	float TimeLeft() const { return FMath::Max(0.f, Spec.TimeLimit - Elapsed); }
	int32 LeaderIndex() const { return Leader; }
	void SetCharge(int32 Side, bool bCharge) { Charge[Side] = bCharge; }
	bool Charging(int32 Side) const { return Charge[Side]; }
	const FSovBattleSpec& GetSpec() const { return Spec; }

private:
	int32 Nearest(int32 From) const;
	void Swing(int32 From, int32 To);
	int32 StrengthOf(const FSovSoldier& S) const;

	FSovBattleSpec Spec;
	TArray<FSovSoldier> Men;
	FRandomStream Rng;
	int32 Initial[2] = {0, 0};
	bool Charge[2] = {true, true};
	int32 Leader = -1;                 // the human's leader (controllable)
	int32 UnitLeader[2] = {-1, -1};    // a side whose unit is a lone leader
	float Elapsed = 0.f;
	bool bFinished = false;
	bool bTimedOut = false;
};
