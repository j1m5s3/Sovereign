#include "SovAttack.h"

#include "sovereign/game.h"

#include <optional>

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

// The casus belli by sov::CasusBelli, as the diplomacy list names them (08: War types).
const TCHAR* const kWarReasons[] = {TEXT(""), TEXT("Holy War"), TEXT("War of Liberation"), TEXT("Reconquest War"), TEXT("Protectorate War"),
	TEXT("Colonial War"), TEXT("War of Territorial Expansion"), TEXT("Ideological War"), TEXT("War of Retribution"), TEXT("Golden Age War"), TEXT("Joint War")};
static_assert(sizeof(kWarReasons) / sizeof(kWarReasons[0]) == static_cast<size_t>(sov::kNumCasusBelli), "a name for each casus belli");

// Why the unit cannot attack the target from anywhere it can reach, in validateCombat's order.
FString AttackProblem(const sov::Game& G, const sov::Unit& U, sov::Hex Target)
{
	const sov::UnitType& T = G.rules().units[static_cast<size_t>(U.type)];
	if (T.religiousStrength > 0) return TEXT("Religious units fight only religious units of another religion, from beside them.");
	if (T.layer != sov::UnitLayer::Military && T.layer != sov::UnitLayer::Leader && T.layer != sov::UnitLayer::Air)
		return FString::Printf(TEXT("The %s cannot attack."), *Str(T.name));
	if (G.visibility(U.owner, Target) != sov::Visibility::Visible) return TEXT("It cannot attack what it cannot see.");
	if (U.movesLeft <= sov::Fixed()) return TEXT("No moves left this turn: it can attack next turn.");
	if (U.attacks >= G.maxAttacks(U)) return TEXT("It has already attacked this turn.");
	if (G.isEmbarked(U)) return TEXT("Embarked units cannot attack.");
	return TEXT("It cannot attack that from anywhere it can reach.");
}
}  // namespace

bool SovFoeAt(const sov::Game& G, sov::PlayerId Me, sov::Hex Target)
{
	const sov::GameState& S = G.state();
	if (const sov::City* C = S.cityAt(Target)) return C->owner != Me;
	// An Encampment is a target only at war: at peace its plot is walked on like any other.
	if (const sov::City* Camp = G.encampmentTargetAt(Target); Camp && Camp->owner != Me && G.atWar(Me, Camp->owner)) return true;
	return S.foreignUnitAt(Target, Me) && G.visibility(Me, Target) == sov::Visibility::Visible;
}

sov::Command SovAttackCommand(sov::PlayerId Me, sov::UnitId Unit, sov::Hex Target, bool bRanged)
{
	return bRanged ? sov::Command::rangedAttack(Me, Unit, Target) : sov::Command::attack(Me, Unit, Target);
}

FString SovPlayerName(const sov::Game& G, sov::PlayerId Player)
{
	const sov::GameState& S = G.state();
	if (Player < 0 || static_cast<size_t>(Player) >= S.players.size()) return TEXT("nobody");
	const sov::Player& P = S.players[static_cast<size_t>(Player)];
	const sov::Rules& R = G.rules();
	if (P.cityState != sov::kNone) return Str(R.cityStates[static_cast<size_t>(P.cityState)].name);
	if (Player == G.freeCityPlayer()) return TEXT("the Free Cities");
	if (P.barbarian) return TEXT("the barbarians");
	if (P.civ != sov::kNone) return Str(R.civs[static_cast<size_t>(P.civ)].name);
	return Str(P.leaderName);
}

FString SovWarBlockedText(const sov::Game& G, sov::PlayerId Me, sov::PlayerId On)
{
	const sov::GameState& S = G.state();
	const FString Who = SovPlayerName(G, On);
	if (On < 0 || static_cast<size_t>(On) >= S.players.size() || S.players[static_cast<size_t>(On)].barbarian)
		return FString::Printf(TEXT("No war can be declared on %s."), *Who);
	// canDeclareWar's locks (08): a peace treaty holds DIPLOMACY_PEACE_MIN_TURNS, and no war before the earliest turn.
	const sov::Player& P = S.players[static_cast<size_t>(Me)];
	const int32 PeaceTurns = G.rules().globalInt("DIPLOMACY_PEACE_MIN_TURNS");
	if (static_cast<size_t>(On) < P.relations.size())
	{
		const sov::Relation& Rel = P.relations[static_cast<size_t>(On)];
		if (!Rel.war && Rel.since > 0 && S.turn - Rel.since < PeaceTurns)
			return FString::Printf(TEXT("At peace with %s: the peace treaty holds %d more turn(s)."), *Who, PeaceTurns - (S.turn - Rel.since));
	}
	const int32 Earliest = G.rules().globalInt("DIPLOMACY_EARLIEST_MAJOR_DOW_TURN");
	if (S.turn < Earliest) return FString::Printf(TEXT("At peace with %s: no war may be declared before turn %d."), *Who, Earliest);
	return FString::Printf(TEXT("At peace with %s, and no war can be declared on them now."), *Who);
}

int32 SovCityStateWarGrievances(const sov::Game& G, sov::PlayerId Me, sov::PlayerId CityState, FString* OutWho)
{
	// As Game::onWarDeclared adds them: its suzerain (if not us) the most, every other major with envoys there less.
	const sov::Rules& R = G.rules();
	const sov::PlayerId Suzerain = G.suzerainOf(CityState);
	int32 Sum = 0;
	for (const sov::Player& O : G.state().players)
	{
		if (O.id == Me || !G.isMajorCiv(O.id)) continue;
		int32 Add = 0;
		if (O.id == Suzerain) Add = R.globalInt("GRIEVANCES_SUZERAIN_CITY_STATE_DOW");
		else if (G.envoysAt(O.id, CityState) > 0) Add = R.globalInt("GRIEVANCES_HAVE_ENVOYS_CITY_STATE_DOW");
		if (Add <= 0) continue;
		Sum += Add;
		if (OutWho)
		{
			*OutWho += FString::Printf(TEXT("%s%s %d%s"), OutWho->IsEmpty() ? TEXT("") : TEXT(", "), *SovPlayerName(G, O.id), Add,
				O.id == Suzerain ? TEXT(" (its suzerain)") : TEXT(""));
		}
	}
	return Sum;
}

TArray<FSovWarChoice> SovWarChoices(const sov::Game& G, sov::PlayerId Me, sov::PlayerId On, bool bAttack)
{
	TArray<FSovWarChoice> Out;
	if (!G.canDeclareWar(Me, On)) return Out;
	const FString Who = SovPlayerName(G, On);
	const TCHAR* const Then = bAttack ? TEXT(" and attack") : TEXT("");
	// What a war on a city-state costs besides (08): the grievances of the majors with envoys there, its envoy
	// bonuses while the war lasts, and a war that lasts, as city-states do not make peace.
	FString Right, Tip;
	if (G.isCityState(On))
	{
		FString Angry;
		const int32 Grievances = SovCityStateWarGrievances(G, Me, On, &Angry);
		Right = Grievances > 0 ? FString::Printf(TEXT("%d grievances"), Grievances) : FString();
		if (!Angry.IsEmpty()) Tip += FString::Printf(TEXT("Grievances against you: %s.\n"), *Angry);
		if (G.envoysAt(Me, On) > 0) Tip += TEXT("Your envoys there give you nothing while the war lasts.\n");
		Tip += TEXT("City-states do not make peace: the war lasts.");
	}
	// The kind of war, as the diplomacy list words it: formal while we denounce them, else a surprise.
	const bool bFormal = G.denouncing(Me, On);
	Out.Add({FString::Printf(TEXT("Declare %s war on %s%s"), bFormal ? TEXT("a formal") : TEXT("a surprise"), *Who, Then), sov::Command::declareWar(Me, On),
		Right, Tip});
	for (int32 W = 1; W < sov::kNumCasusBelli; ++W)
	{
		const sov::CasusBelli Why = static_cast<sov::CasusBelli>(W);
		if (!G.hasCasusBelli(Me, On, Why)) continue;
		Out.Add({FString::Printf(TEXT("Declare a %s on %s (%d%% grievances)%s"), kWarReasons[W], *Who, G.casusBelliGrievancePercent(Why), Then),
			sov::Command::declareWarFor(Me, On, Why), Right, Tip});
	}
	return Out;
}

FSovAttackPlan SovPlanAttack(const sov::Game& G, sov::UnitId Unit, sov::Hex Target)
{
	FSovAttackPlan Plan;
	const sov::Unit* U = G.state().unit(Unit);
	if (!U)
	{
		Plan.Text = TEXT("No unit selected.");
		return Plan;
	}
	const sov::PlayerId Me = U->owner;
	const sov::UnitType& T = G.rules().units[static_cast<size_t>(U->type)];
	Plan.bRanged = T.ranged > 0 || T.bombard > 0;
	Plan.Plot = U->pos;
	// Open now: a ranged unit shoots; anything else strikes, theological combat too (it needs no war, 06).
	if (Plan.bRanged && G.previewAttack(Unit, Target, true).valid)
	{
		Plan.Kind = ESovAttackPlan::Attack;
		Plan.bAttackOnArrival = true;
		return Plan;
	}
	if (G.previewAttack(Unit, Target, false).valid)
	{
		Plan.Kind = ESovAttackPlan::Attack;
		Plan.bRanged = false;
		Plan.bAttackOnArrival = true;
		return Plan;
	}
	// Only a war stops it where it stands: ask first, whoever the owner, a major civ or a city-state (barbarians are
	// at war with everyone already).
	auto AskFor = [&](sov::PlayerId On) {
		Plan.War = On;
		if (G.canDeclareWar(Me, On))
		{
			Plan.Kind = ESovAttackPlan::AskWar;
			Plan.Text = FString::Printf(TEXT("At peace with %s: right-click to declare war and attack"), *SovPlayerName(G, On));
		}
		else
		{
			Plan.Kind = ESovAttackPlan::WarBlocked;
			Plan.Text = SovWarBlockedText(G, Me, On);
		}
	};
	if (const sov::PlayerId On = G.warNeededToAttack(Unit, Target, Plan.bRanged); On != sov::kNoPlayer)
	{
		Plan.bAttackOnArrival = true;
		AskFor(On);
		return Plan;
	}
	// Out of reach: where it would attack from (never the foe's own plot).
	const std::optional<sov::AttackApproach> A = G.attackApproach(Unit, Target, Plan.bRanged);
	if (!A)
	{
		Plan.Text = AttackProblem(G, *U, Target);
		return Plan;
	}
	Plan.Plot = A->plot;
	Plan.Turns = A->turns;
	Plan.bAttackOnArrival = A->attackOnArrival;
	if (A->plot == U->pos)
	{
		// In place, and still it cannot attack: no moves or attacks left (and maybe at peace as well).
		Plan.Text = AttackProblem(G, *U, Target);
		if (A->war != sov::kNoPlayer) Plan.Text += FString::Printf(TEXT(" You are also at peace with %s."), *SovPlayerName(G, A->war));
		return Plan;
	}
	if (A->war != sov::kNoPlayer)
	{
		AskFor(A->war);
		return Plan;
	}
	Plan.Kind = ESovAttackPlan::Approach;
	const TCHAR* const Where = Plan.bRanged ? TEXT("into range") : TEXT("beside it");
	Plan.Text = A->attackOnArrival ? FString::Printf(TEXT("Moves %s, then attacks"), Where)
		: A->turns == 0 ? FString::Printf(TEXT("Moves %s; attacks next turn"), Where)
						: FString::Printf(TEXT("%d turn(s) to get %s, then right-click it to attack"), A->turns, Where);
	return Plan;
}
