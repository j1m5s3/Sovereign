// What a right-click on a foe does, as in Civ VI: attack at once, walk to where it can attack from and attack, or ask
// to declare war first. Plain functions over the rules core (Game::warNeededToAttack, Game::attackApproach), shared by
// the controller's click, hover and reach rings and by the automation tests.
#pragma once

#include "CoreMinimal.h"

#include "sovereign/commands.h"

namespace sov
{
class Game;
}

enum class ESovAttackPlan : uint8
{
	Refuse,      // nothing to do: Text says why
	Attack,      // attack from where it stands
	Approach,    // move to Plot, then attack if it may this turn
	AskWar,      // only a war with War stops it: ask first (then attack from Plot)
	WarBlocked   // only a war with War stops it, and that war cannot be declared now (Text says why)
};

struct FSovAttackPlan
{
	ESovAttackPlan Kind = ESovAttackPlan::Refuse;
	bool bRanged = false;                 // a ranged attack (else melee, or theological combat)
	sov::PlayerId War = sov::kNoPlayer;   // AskWar and WarBlocked: the player a war must be declared on
	sov::Hex Plot;                        // where it attacks from: its own plot, or the one to move to
	int32 Turns = 0;                      // to reach Plot (0: this turn)
	bool bAttackOnArrival = false;        // there this turn with moves and an attack left
	FString Text;                         // a plain sentence for the message line and the hover
};

// The foe a right-click on `Target` is aimed at: a city of another player, a standing Encampment of a player we are at
// war with, or another player's unit we see there.
bool SovFoeAt(const sov::Game& G, sov::PlayerId Me, sov::Hex Target);
// The plan for our unit against the foe at `Target`.
FSovAttackPlan SovPlanAttack(const sov::Game& G, sov::UnitId Unit, sov::Hex Target);
// The attack itself: a ranged attack or a melee one.
sov::Command SovAttackCommand(sov::PlayerId Me, sov::UnitId Unit, sov::Hex Target, bool bRanged);
// The name a player goes by: its civ, a city-state's name, the Free Cities or the barbarians.
FString SovPlayerName(const sov::Game& G, sov::PlayerId Player);
// Why no war can be declared on `On` now (a peace treaty's turns left...), in a plain sentence.
FString SovWarBlockedText(const sov::Game& G, sov::PlayerId Me, sov::PlayerId On);
// War on a city-state (08): the grievances the other majors with envoys there, and its suzerain, would hold against
// us (their sum; OutWho names them), and a note on what else it costs.
int32 SovCityStateWarGrievances(const sov::Game& G, sov::PlayerId Me, sov::PlayerId CityState, FString* OutWho = nullptr);

// One way to declare war in the declare-war prompt: the kind of war (and any casus belli), what it costs.
struct FSovWarChoice
{
	FString Label;
	sov::Command Command;
	FString Right;
	FString Tip;
};
// Every war that may be declared on `On` now, worded as the diplomacy list words them ("Declare a surprise war on
// Rome", "Declare a Holy War on Rome (50% grievances)"), with " and attack" after each when bAttack. Empty when no war
// may be declared.
TArray<FSovWarChoice> SovWarChoices(const sov::Game& G, sov::PlayerId Me, sov::PlayerId On, bool bAttack);
