#include "SovEvents.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}  // namespace

bool SovEventIsWorldNews(const sov::GameEvent& E)
{
	return E.kind == sov::EventKind::CongressSession || E.kind == sov::EventKind::ResolutionPassed || E.kind == sov::EventKind::ClimatePhase;
}

bool SovEventHeard(const sov::Game& G, sov::PlayerId Me, const sov::GameEvent& E)
{
	// Others' doings reach us as gossip, as far as our access to them goes (08: Access level).
	return SovEventIsWorldNews(E) || G.hearsOf(Me, E);
}

FString SovEventText(const sov::Game& G, sov::PlayerId Me, const sov::GameEvent& E)
{
	const sov::Rules& R = G.rules();
	const sov::GameState& S = G.state();
	auto CivOf = [&](sov::PlayerId Id) {
		const sov::Player& X = S.players[static_cast<size_t>(Id)];
		if (X.cityState != sov::kNone) return Str(R.cityStates[static_cast<size_t>(X.cityState)].name);
		return X.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(X.civ)].name);
	};
	FString Text;
	switch (E.kind)
	{
		case sov::EventKind::AssassinKilledLeader: Text = FString::Printf(TEXT("An assassin from %s killed the ruler of %s."), *CivOf(E.actor), *CivOf(E.target)); break;
		case sov::EventKind::AssassinWoundedLeader: Text = FString::Printf(TEXT("An assassin from %s wounded the ruler of %s (%d damage)."), *CivOf(E.actor), *CivOf(E.target), E.value); break;
		case sov::EventKind::AssassinKilled: Text = FString::Printf(TEXT("An assassin sent against %s was killed."), *CivOf(E.target)); break;
		case sov::EventKind::AssassinCaptured: Text = FString::Printf(TEXT("%s caught an assassin sent by %s."), *CivOf(E.target), *CivOf(E.actor)); break;
		case sov::EventKind::Rebellion: Text = FString::Printf(TEXT("Rebels rise against the iron fist of %s."), *CivOf(E.target)); break;
		case sov::EventKind::LeaderLost:
			Text = FString::Printf(TEXT("%s %s the ruler of %s in battle."), *CivOf(E.actor), E.value ? TEXT("captured") : TEXT("killed"), *CivOf(E.target));
			break;
		case sov::EventKind::HistoricMoment:
			Text = FString::Printf(TEXT("%s: %s (+%d era score)."), E.actor == Me ? TEXT("Historic moment") : *FString::Printf(TEXT("Word from %s"), *CivOf(E.actor)), *Str(R.moments[static_cast<size_t>(E.value)].name),
				R.moments[static_cast<size_t>(E.value)].eraScore);
			break;
		case sov::EventKind::NewAge:
		{
			static const TCHAR* Ages[] = {TEXT("a Normal Age"), TEXT("a Golden Age"), TEXT("a Dark Age"), TEXT("a Heroic Age")};
			Text = E.actor == Me ? FString::Printf(TEXT("A new era dawns: %s begins."), Ages[static_cast<size_t>(E.value) % 4])
								 : FString::Printf(TEXT("%s enters %s."), *CivOf(E.actor), Ages[static_cast<size_t>(E.value) % 4]);
			break;
		}
		case sov::EventKind::CongressSession: Text = TEXT("The World Congress opens a session (, to vote)."); break;
		case sov::EventKind::Disaster: Text = FString::Printf(TEXT("Disaster: a %s strikes our lands."), *Str(R.disasters[static_cast<size_t>(E.value)].name)); break;
		case sov::EventKind::ClimatePhase:
			Text = FString::Printf(TEXT("The climate warms: phase %d. The seas are rising."), E.value);
			break;
		case sov::EventKind::GoodyHut:
			if (E.actor == Me && E.value >= 0 && static_cast<size_t>(E.value) < R.goodies.size())
				Text = FString::Printf(TEXT("A tribal village welcomes us: %s."), *Str(R.goodies[static_cast<size_t>(E.value)].id).Replace(TEXT("GOODY_"), TEXT("")).Replace(TEXT("_"), TEXT(" ")).ToLower());
			break;
		case sov::EventKind::ResolutionPassed:
			Text = FString::Printf(TEXT("The World Congress passes %s%s."), *Str(R.resolutions[static_cast<size_t>(E.value)].name),
				E.target != sov::kNoPlayer ? *FString::Printf(TEXT(" for %s"), *CivOf(E.target)) : TEXT(""));
			break;
		case sov::EventKind::SpyOperation:
		{
			static const TCHAR* const Missions[] = {TEXT("an operation"), TEXT("Counterspy"), TEXT("Listening Post"), TEXT("Gain Sources"), TEXT("Siphon Funds"),
				TEXT("Steal Tech Boost"), TEXT("Sabotage Production"), TEXT("Neutralize Governor"), TEXT("Foment Unrest"),
			TEXT("Great Work Heist"), TEXT("Recruit Partisans"), TEXT("Breach Dam"), TEXT("Disrupt Rocketry"), TEXT("Fabricate Scandal")};
			Text = E.actor == Me ? FString::Printf(TEXT("Your spy succeeds: %s against %s."), Missions[E.value % sov::kNumSpyMissions], *CivOf(E.target))
			   : E.target == Me ? FString::Printf(TEXT("Spies have struck in your lands: %s."), Missions[E.value % sov::kNumSpyMissions])
								 : FString::Printf(TEXT("Rumour: spies from %s struck %s (%s)."), *CivOf(E.actor), *CivOf(E.target), Missions[E.value % sov::kNumSpyMissions]);
			break;
		}
		case sov::EventKind::SpyCaught:
			Text = E.actor == Me ? FString::Printf(TEXT("Your spy was caught by %s%s."), *CivOf(E.target), E.value ? TEXT(" but escaped home") : TEXT(""))
								 : FString::Printf(TEXT("You caught a spy from %s%s."), *CivOf(E.actor), E.value ? TEXT("; it escaped") : TEXT(""));
			break;
		case sov::EventKind::DealProposed:
			if (const sov::Deal* D = G.deal(E.value))
			{
				Text = FString::Printf(TEXT("%s proposes a deal: %s."), *CivOf(E.actor), *Str(sov::describeDeal(R, S, *D)));
			}
			break;
		case sov::EventKind::DealAccepted: Text = FString::Printf(TEXT("%s accepted a deal from %s."), *CivOf(E.target), *CivOf(E.actor)); break;
		case sov::EventKind::DealRejected: Text = FString::Printf(TEXT("%s turned down a deal from %s."), *CivOf(E.target), *CivOf(E.actor)); break;
		case sov::EventKind::Denounced: Text = FString::Printf(TEXT("%s denounced %s."), *CivOf(E.actor), *CivOf(E.target)); break;
		case sov::EventKind::FriendshipDeclared: Text = FString::Printf(TEXT("%s and %s declared friendship."), *CivOf(E.actor), *CivOf(E.target)); break;
		case sov::EventKind::WarDeclared:
			Text = FString::Printf(TEXT("%s declared %s on %s."), *CivOf(E.actor), E.value ? TEXT("a surprise war") : TEXT("war"), *CivOf(E.target));
			break;
		case sov::EventKind::PeaceMade: Text = FString::Printf(TEXT("%s and %s made peace."), *CivOf(E.actor), *CivOf(E.target)); break;
		case sov::EventKind::DealBroken: Text = FString::Printf(TEXT("%s could not keep its deal with %s."), *CivOf(E.actor), *CivOf(E.target)); break;
		case sov::EventKind::GreatPersonRecruited:
		{
			const FString Who = Str(R.greatPeople[static_cast<size_t>(E.value)].name);
			const FString Cls = Str(R.greatPersonClasses[static_cast<size_t>(R.greatPeople[static_cast<size_t>(E.value)].cls)].name);
			Text = E.actor == Me ? FString::Printf(TEXT("%s joins you as a %s (Y: great people)."), *Who, *Cls)
								 : FString::Printf(TEXT("%s recruits %s, a %s."), *CivOf(E.actor), *Who, *Cls);
		}
			break;
	}
	return Text;
}

FName SovEventIcon(const sov::GameEvent& E)
{
	switch (E.kind)
	{
		case sov::EventKind::WarDeclared:
		case sov::EventKind::AssassinKilledLeader:
		case sov::EventKind::AssassinWoundedLeader:
		case sov::EventKind::AssassinKilled:
		case sov::EventKind::AssassinCaptured:
		case sov::EventKind::Rebellion:
		case sov::EventKind::LeaderLost: return "attack";
		case sov::EventKind::Succession: return "government";
		case sov::EventKind::GreatPersonRecruited: return "greatperson";
		case sov::EventKind::HistoricMoment:
		case sov::EventKind::NewAge: return "era";
		case sov::EventKind::Disaster:
		case sov::EventKind::ClimatePhase: return "health";
		case sov::EventKind::GoodyHut: return "gold";
		case sov::EventKind::SpyOperation:
		case sov::EventKind::SpyCaught: return "link";
		default: return "favor";  // diplomacy and the World Congress
	}
}
