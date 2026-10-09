#include "SovHUD.h"

#include <algorithm>

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

#include "SovCameraPawn.h"
#include "SovGameSubsystem.h"
#include "SovHexLayout.h"
#include "SovMirror.h"
#include "SovPlayerController.h"
#include "SovStreetScene.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S)
{
	return UTF8_TO_TCHAR(S.c_str());
}

const TCHAR* VictoryName(sov::Victory V)
{
	switch (V)
	{
		case sov::Victory::Domination: return TEXT("Domination");
		case sov::Victory::Score: return TEXT("Score");
		case sov::Victory::LastStanding: return TEXT("Last civ standing");
		case sov::Victory::Religious: return TEXT("Religious");
		case sov::Victory::Culture: return TEXT("Culture");
		case sov::Victory::Diplomatic: return TEXT("Diplomatic");
		case sov::Victory::Science: return TEXT("Science");
		default: return TEXT("");
	}
}

// On a wrapping map, the copy of a map point nearest the camera (the one on screen).
FVector NearCamera(const APlayerController* PC, const FVector& P)
{
	const ASovCameraPawn* Cam = PC ? Cast<ASovCameraPawn>(PC->GetPawn()) : nullptr;
	return Cam ? SovHex::NearestCopy(P, Cam->FocusPoint().Y, Cam->WrapWidth) : P;
}
}  // namespace

void ASovHUD::Line(const FString& Text, float X, float& Y, const FLinearColor& Color)
{
	UFont* Font = GEngine->GetSmallFont();
	DrawText(Text, FLinearColor(0, 0, 0, 0.8f), X + 1, Y + 1, Font, 1.2f);
	DrawText(Text, Color, X, Y, Font, 1.2f);
	Y += 18.f;
}

void ASovHUD::DrawStatus(const USovGameSubsystem& Sub, float& Y)
{
	const sov::Game& G = Sub.GetGame();
	const sov::GameState& S = G.state();
	const sov::Rules& R = G.rules();
	const sov::PlayerId Me = static_cast<sov::PlayerId>(Sub.GetSession().ViewPlayer());
	const sov::Player& P = S.players[static_cast<size_t>(Me)];
	const FString Civ = P.civ == sov::kNone ? TEXT("?") : Str(R.civs[static_cast<size_t>(P.civ)].name);
	const sov::Player& Current = S.players[static_cast<size_t>(S.currentPlayer)];
	const FString CurrentName = Current.cityState != sov::kNone ? Str(R.cityStates[static_cast<size_t>(Current.cityState)].name)
		: Current.civ == sov::kNone ? FString(TEXT("Barbarians")) : Str(R.civs[static_cast<size_t>(Current.civ)].name);

	if (TopInset <= 0.f)  // the top bar shows the turn
	Line(FString::Printf(TEXT("Turn %d / %d   %s   %s%s   (F1 how to play)"), S.turn, G.turnLimit(), *Civ, *Str(G.difficulty().name),
			 Sub.GetSession().IsHumanTurn() ? TEXT("") : TEXT("   (spectating)")),
		16, Y);
	// Our civ's identity (leaders-and-art-style): its ability, its leader's, and its uniques.
	if (P.civ != sov::kNone)
	{
		const sov::CivType& C = R.civs[static_cast<size_t>(P.civ)];
		FString Uniques;
		for (const sov::UnitType& U : R.units) if (U.uniqueTo == P.civ) Uniques += TEXT(", ") + Str(U.name);
		for (const sov::BuildingType& B : R.buildings) if (B.uniqueTo == P.civ) Uniques += TEXT(", ") + Str(B.name);
		for (const sov::ImprovementType& I : R.improvements) if (I.uniqueTo == P.civ) Uniques += TEXT(", ") + Str(I.name);
		// The ruler, and the personal trait an heir of the dynasty brings (leaders-and-art-style: Dynasties).
		FString Ruler = Str(P.leaderName);
		if (const sov::Dynasty* D = R.dynastyOf(P.civ); D && P.rulingHeir > 0 && static_cast<size_t>(P.rulingHeir) < D->traits.size())
			Ruler += FString::Printf(TEXT(" (%s)"), *Str(D->traits[static_cast<size_t>(P.rulingHeir)].name));
		Line(FString::Printf(TEXT("%s: %s   Leader: %s   Ruler: %s   Uniques: %s"), *Str(C.name), *Str(C.ability.name), *Str(C.leaderAbility.name), *Ruler,
				 Uniques.IsEmpty() ? TEXT("-") : *Uniques.RightChop(2)),
			16, Y, FLinearColor(0.85f, 0.8f, 0.6f));
	}
	if (TopInset <= 0.f)  // the top bar shows these
	Line(FString::Printf(TEXT("Gold %s (%+s)   Science %s   Culture %s   Score %d"), *Str(P.gold.toString()),
			 *Str(G.goldPerTurn(Me).toString()), *Str(G.sciencePerTurn(Me).toString()), *Str(G.culturePerTurn(Me).toString()), G.score(Me)),
		16, Y);
	const FString Research = P.techs.current == sov::kNone ? TEXT("none") : Str(R.techs[static_cast<size_t>(P.techs.current)].name);
	const FString Civic = P.civics.current == sov::kNone ? TEXT("none") : Str(R.civics[static_cast<size_t>(P.civics.current)].name);
	const FString Gov = P.government == sov::kNone ? TEXT("none") : Str(R.governments[static_cast<size_t>(P.government)].name);
	if (TopInset <= 0.f) Line(FString::Printf(TEXT("Research: %s   Civic: %s   Government: %s (F2)"), *Research, *Civic, *Gov), 16, Y);
	// Faith and religion (06).
	{
		FString Faith = FString::Printf(TEXT("Faith %s"), *Str(P.faith.toString()));
		if (P.pantheon != sov::kNone)
		{
			Faith += FString::Printf(TEXT("   Pantheon: %s"), *Str(R.beliefs[static_cast<size_t>(P.pantheon)].name));
		}
		else if (P.faith >= sov::Fixed::fromInt(R.globalInt("RELIGION_PANTHEON_MIN_FAITH")))
		{
			Faith += TEXT("   I: choose a pantheon");
		}
		if (P.religion >= 0)
		{
			const sov::FoundedReligion& Rel = S.religions[static_cast<size_t>(P.religion)];
			int32 Cities = 0;
			for (const sov::City& C : S.cities)
			{
				Cities += G.cityMajorityReligion(C) == P.religion;
			}
			Faith += FString::Printf(TEXT("   Religion: %s, followed in %d cities"), *Str(R.religions[static_cast<size_t>(Rel.type)].name), Cities);
		}
		Line(Faith, 16, Y);
	}
	// The world era, this civ's age and era score (09), and tourism (07).
	{
		static const TCHAR* Ages[] = {TEXT("Normal Age"), TEXT("Golden Age"), TEXT("Dark Age"), TEXT("Heroic Age")};
		const auto [Dark, Golden] = G.ageThresholds(Me);
		const int32 EraIndex = FMath::Clamp(S.gameEra, 0, static_cast<int32>(R.eras.size()) - 1);
		Line(FString::Printf(TEXT("%s Era, %s   Era score %d (Dark below %d, Golden at %d)   Tourism %d: %d visitors, %d at home"),
				 *Str(R.eras[static_cast<size_t>(EraIndex)].name), Ages[static_cast<size_t>(P.age) % 4], P.eraScore, Dark, Golden,
				 G.tourismPerTurn(Me), G.visitingTourists(Me), G.domesticTourists(Me)),
			16, Y, FLinearColor(0.85f, 0.85f, 1.f));
	}
	// Diplomatic Favor, victory points and the World Congress (08 [GS]).
	{
		FString Text = FString::Printf(TEXT("Diplomatic Favor %d (%+d/turn)   Diplomatic Victory %d/%d"), P.favor, G.favorPerTurn(Me), P.diplomaticVictoryPoints,
			R.globalInt("DIPLOMATIC_VICTORY_POINTS_REQUIRED"));
		if (G.congressInSession()) Text += TEXT("   World Congress in session: , to vote");
		else if (S.nextCongressTurn > 0) Text += FString::Printf(TEXT("   Congress meets on turn %d"), S.nextCongressTurn);
		for (const sov::PassedResolution& Pr : S.passedResolutions)
		{
			Text += FString::Printf(TEXT("   [%s %s]"), *Str(R.resolutions[static_cast<size_t>(Pr.resolution)].name), Pr.option == 0 ? TEXT("A") : TEXT("B"));
		}
		Line(Text, 16, Y, FLinearColor(0.75f, 0.95f, 1.f));
	}
	// The space race (09: Science victory): every civ whose exoplanet expedition is under way.
	{
		FString Race;
		for (const sov::Player& O : S.players)
		{
			const int32 Speed = O.alive && !O.barbarian ? G.expeditionSpeed(O.id) : 0;
			if (Speed <= 0) continue;
			const FString Who = O.id == Me ? FString(TEXT("we")) : (O.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(O.civ)].name));
			Race += FString::Printf(TEXT("   %s %d/%d ly (+%d)"), *Who, O.lightYears, R.globalInt("SCIENCE_VICTORY_POINTS_REQUIRED"), Speed);
		}
		if (!Race.IsEmpty()) Line(TEXT("Exoplanet expeditions:") + Race, 16, Y, FLinearColor(0.7f, 0.9f, 1.f));
	}
	// Alliances (08 [R&F]): type, level and turns left with each ally.
	{
		static const TCHAR* const Types[] = {TEXT("Research"), TEXT("Military"), TEXT("Economic"), TEXT("Cultural"), TEXT("Religious")};
		FString Allies;
		for (const sov::Player& O : S.players)
		{
			const sov::AllianceType T = G.alliance(Me, O.id);
			if (T == sov::AllianceType::None) continue;
			const FString Who = O.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(O.civ)].name);
			Allies += FString::Printf(TEXT("   %s (%s, level %d, %d turns)"), *Who, Types[static_cast<int32>(T)], G.allianceLevel(Me, O.id),
				P.relations[static_cast<size_t>(O.id)].allianceUntil - S.turn);
		}
		if (!Allies.IsEmpty()) Line(TEXT("Alliances:") + Allies, 16, Y, FLinearColor(0.6f, 1.f, 0.7f));
	}
	// City-state quests (08): what each city-state we have met asks of us.
	{
		FString Text;
		for (const sov::Quest& Q : S.quests)
		{
			if (Q.major != Me) continue;
			const sov::Player& CS = S.players[static_cast<size_t>(Q.cityState)];
			const FString Name = CS.cityState == sov::kNone ? FString(TEXT("?")) : Str(R.cityStates[static_cast<size_t>(CS.cityState)].name);
			Text += FString::Printf(TEXT("   %s: %s"), *Name, *Str(G.questText(Q)));
		}
		if (!Text.IsEmpty()) Line(TEXT("Quests:") + Text, 16, Y, FLinearColor(0.8f, 0.95f, 0.8f));
	}
	// Scored competitions (08 [GS]): the one running, with our standing and the leader's.
	for (const sov::Competition& C : S.competitions)
	{
		if (C.settled) continue;
		static const TCHAR* const Kinds[] = {TEXT("World's Fair"), TEXT("World Games"), TEXT("Nobel Prize in Literature"), TEXT("Nobel Peace Prize"),
			TEXT("Nobel Prize in Physics"), TEXT("Climate Accords"), TEXT("International Space Station"), TEXT("Aid Request"), TEXT("Military Aid Request")};
		int32 Best = INT32_MIN;
		sov::PlayerId Leader = sov::kNoPlayer;
		for (const sov::Player& O : S.players)
		{
			if (!G.isMajorCiv(O.id) || !O.alive) continue;
			const int32 Score = G.competitionStanding(C, O.id);
			if (Score > Best) { Best = Score; Leader = O.id; }
		}
		const sov::Player* LP = Leader == sov::kNoPlayer ? nullptr : &S.players[static_cast<size_t>(Leader)];
		const FString Who = Leader == Me ? FString(TEXT("us")) : (!LP || LP->civ == sov::kNone ? FString(TEXT("-")) : Str(R.civs[static_cast<size_t>(LP->civ)].name));
		Line(FString::Printf(TEXT("%s: %d turns left; our score %d, leading %s (%d)"), Kinds[static_cast<int32>(C.kind)], C.endTurn - S.turn,
				 G.competitionStanding(C, Me), *Who, Best),
			16, Y, FLinearColor(0.85f, 0.8f, 1.f));
	}
	// Emergencies (08): the running ones, and whether we are in them (join from the , chooser).
	{
		static const TCHAR* const Kinds[] = {TEXT("Military"), TEXT("City-State"), TEXT("Religious"), TEXT("Nuclear"), TEXT("Betrayal")};
		FString Text;
		for (size_t k = 0; k < S.emergencies.size(); ++k)
		{
			const sov::Emergency& E = S.emergencies[k];
			if (E.outcome != 0) continue;
			const sov::Player& T = S.players[static_cast<size_t>(E.target)];
			const FString Who = E.target == Me ? FString(TEXT("us")) : (T.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(T.civ)].name));
			const bool bIn = static_cast<size_t>(Me) < E.members.size() && E.members[static_cast<size_t>(Me)];
			Text += FString::Printf(TEXT("   %s vs %s (%d turns%s)"), Kinds[static_cast<int32>(E.kind)], *Who, E.endTurn - S.turn, bIn ? TEXT(", joined") : TEXT(""));
		}
		if (!Text.IsEmpty()) Line(TEXT("Emergencies:") + Text, 16, Y, FLinearColor(1.f, 0.55f, 0.55f));
	}
	// War weariness (08): points and the amenities every city loses to them.
	if (G.warWeariness(Me) > 0)
	{
		Line(FString::Printf(TEXT("War weariness %d (-%d amenities in every city)"), G.warWeariness(Me), G.warWearinessAmenities(Me)), 16, Y, FLinearColor(1.f, 0.7f, 0.5f));
	}
	// Nuclear weapons (05): devices held by anyone (Ctrl+right-click with a bomber or Nuclear Submarine delivers ours).
	{
		FString Arsenal;
		for (const sov::Player& O : S.players)
		{
			if (!O.alive || O.barbarian || G.wmdsHeld(O.id) == 0) continue;
			const FString Who = O.id == Me ? FString(TEXT("we")) : (O.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(O.civ)].name));
			Arsenal += TEXT("   ") + Who;
			for (size_t W = 0; W < O.wmds.size() && W < R.wmds.size(); ++W)
			{
				if (O.wmds[W] > 0) Arsenal += FString::Printf(TEXT(" %dx %s"), O.wmds[W], *Str(R.wmds[W].name));
			}
		}
		if (!Arsenal.IsEmpty()) Line(TEXT("Nuclear arsenals:") + Arsenal, 16, Y, FLinearColor(1.f, 0.6f, 0.5f));
	}
	// Climate (09: Climate and Disasters [GS]): the world's warming, its phase, our share of the CO2.
	if (S.co2 > 0 || S.climatePhase > 0)
	{
		static const TCHAR* const Roman[] = {TEXT("none"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII")};
		const int32 Tenths = G.temperatureTenths();
		Line(FString::Printf(TEXT("Climate: +%d.%d degrees, phase %s   World CO2 %lld (ours %lld%%)"), Tenths / 10, Tenths % 10,
				 Roman[FMath::Clamp(S.climatePhase, 0, 7)], static_cast<long long>(S.co2), S.co2 > 0 ? static_cast<long long>(P.co2 * 100 / S.co2) : 0LL),
			16, Y, FLinearColor(1.f, 0.85f, 0.6f));
	}
	// Governors (08): where each serves and whether it has established, and titles to spend.
	if (!P.governors.empty() || G.governorTitlesLeft(Me) > 0)
	{
		FString Text = FString::Printf(TEXT("Governor titles %d"), G.governorTitlesLeft(Me));
		for (const sov::Governor& Gv : P.governors)
		{
			const sov::City* At = S.city(Gv.city);
			Text += FString::Printf(TEXT("   %s: %s"), *Str(R.governors[static_cast<size_t>(Gv.type)].name),
				!At ? TEXT("unassigned") : Gv.establishTurns > 0 ? *FString::Printf(TEXT("%s in %d"), *Str(At->name), Gv.establishTurns) : *Str(At->name));
		}
		Line(Text + TEXT("   Z: governors"), 16, Y, FLinearColor(0.85f, 0.8f, 1.f));
	}
	// Offers from other leaders wait for an answer on the diplomacy screen (08; leader doc §10).
	for (const sov::Deal& D : S.deals)
	{
		if (D.to != Me)
		{
			continue;
		}
		const sov::Player& From = S.players[static_cast<size_t>(D.from)];
		Line(FString::Printf(TEXT("%s offers: %s   N: diplomacy"), *Str(From.leaderName), *Str(sov::describeDeal(R, S, D))), 16, Y,
			FLinearColor(0.6f, 1.f, 0.7f));
	}
	if (P.envoyTokens > 0)
	{
		Line(FString::Printf(TEXT("Envoys to send: %d   O: city-states"), P.envoyTokens), 16, Y, FLinearColor(0.6f, 0.9f, 1.f));
	}
	if (G.tradeRouteCapacity(Me) > 0)
	{
		Line(FString::Printf(TEXT("Trade routes: %d of %d"), G.tradeRoutesOf(Me), G.tradeRouteCapacity(Me)), 16, Y);
	}
	// The throne (leader doc §5).
	if (const sov::Unit* L = G.leaderOf(Me))
	{
		FString Gear;
		for (sov::TypeIndex GearId : L->gear)
		{
			if (GearId != sov::kNone)
			{
				Gear += (Gear.IsEmpty() ? TEXT("") : TEXT(", ")) + Str(R.gear[static_cast<size_t>(GearId)].name);
			}
		}
		Line(FString::Printf(TEXT("Leader: %s  HP %d   %s"), *Str(P.leaderName), L->hp, *Gear), 16, Y, FLinearColor(1.f, 0.85f, 0.3f));
	}
	else if (P.captor != sov::kNoPlayer)
	{
		const sov::Player& C = S.players[static_cast<size_t>(P.captor)];
		Line(FString::Printf(TEXT("%s is held captive by %s. H: abandon and crown a successor"), *Str(P.leaderName),
				 C.civ == sov::kNone ? TEXT("?") : *Str(R.civs[static_cast<size_t>(C.civ)].name)),
			16, Y, FLinearColor(1.f, 0.4f, 0.3f));
	}
	else if (P.successionPending)
	{
		Line(TEXT("The throne is empty. H: choose a successor"), 16, Y, FLinearColor(1.f, 0.4f, 0.3f));
	}
	// Assassination news involving us from the last two turns (leader doc §6).
	int32 GossipShown = 0;
	for (const sov::GameEvent& E : S.events)
	{
		const bool bWorldNews = E.kind == sov::EventKind::CongressSession || E.kind == sov::EventKind::ResolutionPassed || E.kind == sov::EventKind::ClimatePhase;
		// Others' doings reach us as gossip, as far as our access to them goes (08: Access level).
		if (E.turn < S.turn - 1 || (!bWorldNews && !G.hearsOf(Me, E)))
		{
			continue;
		}
		if (E.actor != Me && E.target != Me && !bWorldNews && ++GossipShown > 6)
		{
			continue;
		}
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
		Line(FString::Printf(TEXT("Turn %d: %s"), E.turn, *Text), 16, Y, FLinearColor(1.f, 0.5f, 0.8f));
	}
	// Dedications (09): this era's, and a reminder while one is still to choose.
	{
		FString Text;
		for (const sov::TypeIndex D : P.dedications) Text += (Text.IsEmpty() ? TEXT("") : TEXT(", ")) + Str(R.dedications[static_cast<size_t>(D)].name);
		if (!G.availableDedications(Me).empty()) Text += FString::Printf(TEXT("%sF2: choose %d dedication(s)"), Text.IsEmpty() ? TEXT("") : TEXT("   "), P.dedicationsPending);
		if (!Text.IsEmpty()) Line(TEXT("Dedications: ") + Text, 16, Y, FLinearColor(1.f, 0.85f, 0.5f));
	}
	// Reputation (leader doc §8.1).
	{
		const TCHAR* Standing = G.beloved(Me) ? TEXT("Beloved") : G.feared(Me) ? TEXT("Feared") : TEXT("Neither loved nor feared");
		Line(FString::Printf(TEXT("Reputation %+d: %s"), P.reputation, Standing), 16, Y, FLinearColor(0.8f, 0.85f, 1.f));
	}
	if (P.interregnumTurns > 0)
	{
		Line(FString::Printf(TEXT("Interregnum: policy slots empty for %d more turn(s)"), P.interregnumTurns), 16, Y, FLinearColor(1.f, 0.6f, 0.4f));
	}
	if (S.currentPlayer != Me)
	{
		Line(FString::Printf(TEXT("%s is playing..."), *CurrentName), 16, Y, FLinearColor(1.f, 0.8f, 0.3f));
	}
	if (!P.alive)
	{
		Line(TEXT("You have been eliminated. Esc opens the menu."), 16, Y, FLinearColor(1.f, 0.3f, 0.3f));
	}
	if (G.gameOver())
	{
		const sov::Player& W = S.players[static_cast<size_t>(S.winner)];
		const FString Winner = W.civ == sov::kNone ? TEXT("?") : Str(R.civs[static_cast<size_t>(W.civ)].name);
		Line(FString::Printf(TEXT("%s wins: %s victory. Esc opens the menu."), *Winner, VictoryName(S.victory)), 16, Y, FLinearColor(1.f, 0.9f, 0.2f));
	}
	if (!Sub.LastMessage.IsEmpty())
	{
		Line(Sub.LastMessage, 16, Y, FLinearColor(1.f, 0.6f, 0.4f));
	}
}

void ASovHUD::DrawYields(const USovGameSubsystem& Sub)
{
	const sov::Game& G = Sub.GetGame();
	const sov::GameState& S = G.state();
	const sov::PlayerId View = static_cast<sov::PlayerId>(Sub.GetSession().ViewPlayer());
	UFont* Font = GEngine->GetSmallFont();
	static const TCHAR* Letters[] = {TEXT("F"), TEXT("P"), TEXT("G"), TEXT("S"), TEXT("C"), TEXT("R")};
	static const FLinearColor Colors[] = {FLinearColor(0.5f, 1.f, 0.4f), FLinearColor(1.f, 0.6f, 0.3f), FLinearColor(1.f, 0.9f, 0.3f),
		FLinearColor(0.4f, 0.75f, 1.f), FLinearColor(0.9f, 0.5f, 1.f), FLinearColor(0.9f, 0.9f, 0.9f)};
	for (int32 i = 0; i < S.grid.size(); ++i)
	{
		const sov::Plot& P = S.plots[static_cast<size_t>(i)];
		if (P.owner != View || P.city == sov::kNoCity)
		{
			continue;
		}
		const sov::City* C = S.city(P.city);
		const sov::Hex H = S.grid.at(i);
		if (!C || G.visibility(View, H) == sov::Visibility::Unrevealed)
		{
			continue;
		}
		const FVector Screen = Project(NearCamera(PlayerOwner, SovHex::Center(H.x, H.y, 5.0)));
		if (Screen.Z <= 0 || Screen.X < 0 || Screen.Y < 0 || Screen.X > Canvas->ClipX || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		const sov::Yields Y = G.plotYields(H, *C);
		const bool bWorked = std::find(C->worked.begin(), C->worked.end(), i) != C->worked.end();
		float X = Screen.X - 30.f;
		for (size_t k = 0; k < sov::kNumYields && k < UE_ARRAY_COUNT(Letters); ++k)
		{
			const int32 V = static_cast<int32>(Y[k].toInt());
			if (V <= 0) continue;
			const FString T = FString::Printf(TEXT("%d%s"), V, Letters[k]);
			DrawText(T, FLinearColor(0, 0, 0, 0.9f), X + 1, Screen.Y + 1, Font, 1.0f);
			DrawText(T, Colors[k], X, Screen.Y, Font, 1.0f);
			X += 18.f;
		}
		if (bWorked) DrawText(TEXT("*"), FLinearColor::White, Screen.X - 8.f, Screen.Y - 14.f, Font, 1.2f);
	}
}

void ASovHUD::DrawHelp()
{
	static const TCHAR* const Lines[] = {
		TEXT("How to play (F1 closes)"),
		TEXT("Goal: win by science, culture, religion, diplomacy or conquest, or hold the best score at the turn limit."),
		TEXT("Left-click a unit or city to select it; right-click a plot to move or attack there. '.' next unit needing orders."),
		TEXT("Settler: F founds a city. Builder: B builds an improvement. U promotes a unit with enough XP."),
		TEXT("P production, T research, C civics, F2 government and policies, Y great people, Z governors."),
		TEXT("N diplomacy (talk to leaders, trade, demand), O city-states, J agents, ',' World Congress, I pantheon."),
		TEXT("Your Sovereign (the crowned leader): E gear, L link an escort, Q walk a city's streets; it can fight battles live."),
		TEXT("F3 yields on your plots (* worked). F4 the chronicle of your reign, F6 has it written up. Rest the cursor on a plot for its details."),
		TEXT("F5 quicksave, F9 quickload. Space or Enter ends the turn; if something needs your choice first, it opens."),
		TEXT("WASD / arrows pan, the wheel zooms, Home returns to your capital. Esc closes a chooser, then the menu (save, load, new game, quit)."),
	};
	const float W = 860.f, H = 16.f + 20.f * UE_ARRAY_COUNT(Lines);
	const float Left = (Canvas->ClipX - W) * 0.5f, Top = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f), Left, Top, W, H);
	float Y = Top + 8.f;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Lines); ++i) Line(Lines[i], Left + 14.f, Y, i == 0 ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor::White);
}

void ASovHUD::DrawChronicle(const USovGameSubsystem& Sub)
{
	const sov::Game& G = Sub.GetGame();
	const std::vector<std::string> All = G.chronicleLines(static_cast<sov::PlayerId>(Sub.GetSession().ViewPlayer()));
	constexpr int32 Shown = 22;
	const int32 From = FMath::Max(0, static_cast<int32>(All.size()) - Shown);
	const int32 Count = static_cast<int32>(All.size()) - From;
	const float W = 860.f, H = 56.f + 20.f * FMath::Max(1, Count);
	const float Left = (Canvas->ClipX - W) * 0.5f, Top = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.03f, 0.025f, 0.02f, 0.93f), Left, Top, W, H);
	float Y = Top + 8.f;
	Line(FString::Printf(TEXT("The chronicle of your reign (%d events%s; F4 closes)"), static_cast<int32>(All.size()), From > 0 ? TEXT(", the latest shown") : TEXT("")), Left + 14.f, Y,
		FLinearColor(1.f, 0.85f, 0.45f));
	if (All.empty()) Line(TEXT("Nothing of note has happened yet."), Left + 14.f, Y);
	for (int32 i = From; i < static_cast<int32>(All.size()); ++i) Line(UTF8_TO_TCHAR(All[static_cast<size_t>(i)].c_str()), Left + 14.f, Y);
	Line(Sub.WritingChronicle() ? TEXT("The court historian is writing...") : TEXT("F6: have the court historian write it up (a file in Saved/Sovereign/Chronicles)"), Left + 14.f, Y,
		FLinearColor(0.8f, 0.85f, 1.f));
}
void ASovHUD::DrawLabels(const USovGameSubsystem& Sub)
{
	const sov::Game& G = Sub.GetGame();
	const FSovMirror M = BuildMirror(G, Sub.GetSession().ViewPlayer());
	UFont* Font = GEngine->GetSmallFont();
	for (const FSovCityMarker& C : M.Cities)
	{
		const FVector Screen = Project(NearCamera(PlayerOwner, SovHex::Center(C.X, C.Y, 60.0)));
		if (Screen.Z <= 0)
		{
			continue;
		}
		// Loyalty shows once it slips below Loyal [R&F].
		const FString Loyalty = C.Loyalty <= 75 ? FString::Printf(TEXT("  L%d"), C.Loyalty) : FString();
		const FString Label = FString::Printf(TEXT("%s%s  %d%s"), C.bCapital ? TEXT("* ") : TEXT(""), *C.Name, C.Population, *Loyalty);
		float W = 0, H = 0;
		GetTextSize(Label, W, H, Font, 1.2f);
		DrawRect(FLinearColor(0, 0, 0, 0.7f), Screen.X - W / 2 - 8, Screen.Y - H - 2, W + 12, H + 4);
		DrawRect(C.Color, Screen.X - W / 2 - 8, Screen.Y - H - 2, 4, H + 4);
		DrawText(Label, FLinearColor::White, Screen.X - W / 2, Screen.Y - H, Font, 1.2f);
		if (C.MaxHp > 0 && C.Hp < C.MaxHp)
		{
			const float Frac = static_cast<float>(C.Hp) / C.MaxHp;
			DrawRect(FLinearColor(0.2f, 0, 0, 0.8f), Screen.X - 30, Screen.Y + 4, 60, 5);
			DrawRect(FLinearColor(0.2f, 0.9f, 0.2f, 0.9f), Screen.X - 30, Screen.Y + 4, 60 * Frac, 5);
		}
	}
	for (const FSovUnitMarker& U : M.Units)
	{
		if (U.bLeader)
		{
			// Leaders carry their ruler's name.
			const FVector At = Project(NearCamera(PlayerOwner, SovHex::Center(U.X, U.Y, 110.0) + SovHex::ToWorld(FVector2D(-38.0, -30.0), 0.0)));
			if (At.Z > 0)
			{
				float W = 0, H = 0;
				GetTextSize(U.Name, W, H, Font, 1.0f);
				DrawRect(FLinearColor(0, 0, 0, 0.6f), At.X - W / 2 - 3, At.Y - H - 1, W + 6, H + 2);
				DrawText(U.Name, FLinearColor(1.f, 0.85f, 0.3f), At.X - W / 2, At.Y - H, Font, 1.0f);
			}
		}
		if (U.Hp >= 100)
		{
			continue;
		}
		const FVector Screen = Project(NearCamera(PlayerOwner, SovHex::Center(U.X, U.Y, 80.0)));
		if (Screen.Z <= 0)
		{
			continue;
		}
		const float Frac = FMath::Clamp(U.Hp / 100.f, 0.f, 1.f);
		DrawRect(FLinearColor(0.2f, 0, 0, 0.8f), Screen.X - 16, Screen.Y, 32, 4);
		DrawRect(FLinearColor(0.2f, 0.9f, 0.2f, 0.9f), Screen.X - 16, Screen.Y, 32 * Frac, 4);
	}
}

void ASovHUD::DrawStreet(const USovGameSubsystem& Sub, const ASovPlayerController& PC)
{
	const ASovStreetScene* Scene = PC.GetStreet();
	const FSovStreetLayout& L = Scene->GetLayout();
	UFont* Font = GEngine->GetSmallFont();
	float Y = 12.f;
	const TCHAR* Moods[] = {TEXT("content"), TEXT("happy: banners in the square"), TEXT("unhappy: shutters closed"), TEXT("under Fear: guards at every corner")};
	Line(FString::Printf(TEXT("%s, City Center  (%s)"), *L.CityName, Moods[static_cast<int32>(L.Mood)]), 16, Y);
	const sov::City* City = Sub.GetGame().state().city(L.CityId);
	if (City)
	{
		const sov::CityReport Rep = Sub.GetGame().cityReport(L.CityId);
		Line(FString::Printf(TEXT("Population %d   Amenities %d/%d   Loyalty %d"), City->population, Rep.amenities, Rep.amenitiesNeeded, City->loyalty), 16, Y);
	}
	if (!Sub.LastMessage.IsEmpty())
	{
		Line(Sub.LastMessage, 16, Y, FLinearColor(1.f, 0.8f, 0.4f));
	}
	// Names over the landmarks and the two people who listen.
	for (const FSovStreetPiece& P : L.Pieces)
	{
		if (P.Kind != ESovStreetPiece::Landmark)
		{
			continue;
		}
		const FVector S = Project(Scene->ToWorld(P.Location + FVector(0, 0, P.Size.Z * 0.5 + 120)));
		if (S.Z > 0)
		{
			float W = 0, H = 0;
			GetTextSize(P.Label, W, H, Font, 1.1f);
			DrawRect(FLinearColor(0, 0, 0, 0.55f), S.X - W / 2 - 3, S.Y - H - 1, W + 6, H + 2);
			DrawText(P.Label, FLinearColor::White, S.X - W / 2, S.Y - H, Font, 1.1f);
		}
	}
	for (const TPair<FVector, FString>& Who : {TPair<FVector, FString>(L.Herald, TEXT("Herald")), TPair<FVector, FString>(L.Captain, TEXT("Captain of the guard"))})
	{
		const FVector S = Project(Scene->ToWorld(Who.Key + FVector(0, 0, 260)));
		if (S.Z > 0)
		{
			DrawText(Who.Value, FLinearColor(1.f, 0.85f, 0.3f), S.X - 30, S.Y, Font, 1.1f);
		}
	}
	const FString Prompt = PC.StreetPrompt();
	if (!Prompt.IsEmpty())
	{
		float W = 0, H = 0;
		GetTextSize(Prompt, W, H, Font, 1.4f);
		DrawRect(FLinearColor(0, 0, 0, 0.7f), Canvas->ClipX / 2 - W / 2 - 8, Canvas->ClipY * 0.7f - 4, W + 16, H + 8);
		DrawText(Prompt, FLinearColor::White, Canvas->ClipX / 2 - W / 2, Canvas->ClipY * 0.7f, Font, 1.4f);
	}
	float PY = Canvas->ClipY - 26.f;
	Line(TEXT("WASD walk   hold right mouse / Q E look   F talk   Esc back to the map"), 16, PY);
}

void ASovHUD::DrawBattle(const USovGameSubsystem& Sub, const ASovPlayerController& PC)
{
	const FSovBattleSim& Sim = PC.GetBattleSim();
	const FSovBattleSpec& Spec = Sim.GetSpec();
	float Y = 12.f;
	if (PC.InReplay())
	{
		Line(FString::Printf(TEXT("REPLAY  %s"), *PC.ReplayTitle()), 16, Y, FLinearColor(1.f, 0.85f, 0.3f));
		Line(FString::Printf(TEXT("Attackers %d/%d   Defenders %d/%d   Time %d s"), Sim.Alive(0), Sim.Started(0), Sim.Alive(1), Sim.Started(1),
				 static_cast<int32>(Sim.TimeLeft())), 16, Y);
		float RY = Canvas->ClipY - 26.f;
		Line(TEXT("Hold right mouse or Q E to look   Esc leaves the replay"), 16, RY);
		return;
	}
	Line(FString::Printf(TEXT("BATTLE: %s (strength %d) attacks %s (strength %d)"), *Spec.Attacker.Name, Spec.Attacker.Strength,
			 *Spec.Defender.Name, Spec.Defender.Strength), 16, Y, FLinearColor(1.f, 0.85f, 0.3f));
	Line(FString::Printf(TEXT("Attackers %d/%d   Defenders %d/%d   Time %d s"), Sim.Alive(0), Sim.Started(0), Sim.Alive(1), Sim.Started(1),
			 static_cast<int32>(Sim.TimeLeft())), 16, Y);
	if (Sim.LeaderIndex() != INDEX_NONE)
	{
		const FSovSoldier& L = Sim.Soldiers()[Sim.LeaderIndex()];
		Line(FString::Printf(TEXT("Your leader: %s"), L.bAlive ? *FString::Printf(TEXT("%d%%"), FMath::RoundToInt(100.f * L.Hp / L.MaxHp)) : TEXT("down!")), 16, Y,
			L.bAlive ? FLinearColor::White : FLinearColor(1.f, 0.3f, 0.3f));
	}
	auto OrderOf = [&](int32 Squad) { return FString(UTF8_TO_TCHAR(sov::battle::orderName(Sim.GetOrder(Spec.HumanSide, Squad)))); };
	const TCHAR* Picked[] = {TEXT("left"), TEXT("centre"), TEXT("right")};
	const int32 Squad = PC.GetBattleSquad();
	Line(FString::Printf(TEXT("Your squads: left %s, centre %s, right %s   (ordering: %s)"), *OrderOf(0), *OrderOf(1), *OrderOf(2),
			 Squad >= 0 ? Picked[Squad] : TEXT("all")), 16, Y);
	Line(TEXT("1 advance  2 hold  3 flank left  4 flank right  5 fall back  6 hunt their leader   7 8 9 pick a squad, 0 all"), 16, Y,
		FLinearColor(0.75f, 0.75f, 0.75f));
	const TCHAR* Foe = Sim.RemoteView() ? TEXT("Your rival fights this battle as their leader; you command your squads.")
		: Sim.RemoteEnemy()             ? TEXT("Your rival commands the enemy's squads.")
		: Sim.EnemyTrained()            ? TEXT("The enemy is led by the trained battle AI.")
										: TEXT("The enemy charges (no trained battle AI found).");
	Line(Foe, 16, Y, FLinearColor(0.75f, 0.75f, 0.75f));
	if (!Sub.LastMessage.IsEmpty())
	{
		Line(Sub.LastMessage, 16, Y, FLinearColor(1.f, 0.8f, 0.4f));
	}
	if (PC.BattleSettled())
	{
		const FSovBattleResult& R = PC.BattleOutcome();
		UFont* Font = GEngine->GetSmallFont();
		const FString Text = FString::Printf(TEXT("%s   Field result: attacker lost %d HP, defender %d HP%s. The rules hold it within 25%% of the expected Civ result."),
			R.Winner == Spec.HumanSide ? TEXT("VICTORY") : R.Winner < 0 ? TEXT("STALEMATE") : TEXT("DEFEAT"), R.ToAttacker, R.ToDefender,
			R.LeaderWound > 0 ? *FString::Printf(TEXT(", your leader wounded %d"), R.LeaderWound) : TEXT(""));
		float W = 0, H = 0;
		GetTextSize(Text, W, H, Font, 1.4f);
		DrawRect(FLinearColor(0, 0, 0, 0.7f), Canvas->ClipX / 2 - W / 2 - 8, Canvas->ClipY * 0.4f - 4, W + 16, H + 8);
		DrawText(Text, FLinearColor::White, Canvas->ClipX / 2 - W / 2, Canvas->ClipY * 0.4f, Font, 1.4f);
	}
	float PY = Canvas->ClipY - 26.f;
	Line(TEXT("WASD move   left click / F strike   Tab charge or hold   1-6 squad orders   hold right mouse / Q E look   Esc settle now"), 16, PY);
}

void ASovHUD::DrawHUD()
{
	Super::DrawHUD();
	const UGameInstance* GI = GetGameInstance();
	const USovGameSubsystem* Sub = GI ? GI->GetSubsystem<USovGameSubsystem>() : nullptr;
	float Y = 12.f + TopInset;
	// Online: the lobby until the host starts the game.
	if (Sub && Sub->GetSession().InLobby())
	{
		const bool bHost = Sub->GetSession().NetMode() == ESovNet::Host;
		const bool bSteam = Sub->GetSession().UsesSteam();
		Line(bHost ? (bSteam ? TEXT("Hosting a game for Steam friends. F: invite friends. Enter starts it; open seats are played by the AI. M: chat")
							 : TEXT("Hosting a game. Enter starts it; open seats are played by the AI. M: chat"))
				   : TEXT("In the host's lobby. Waiting for the game to start. M: chat"),
			16, Y, FLinearColor(1.f, 0.85f, 0.45f));
		for (const FString& L : Sub->GetSession().LobbyLines()) Line(L, 32, Y);
		Y += 10.f;
		for (const FString& L : Sub->NetLines) Line(L, 16, Y, FLinearColor(0.7f, 0.85f, 1.f));
		if (!Sub->LastMessage.IsEmpty()) Line(Sub->LastMessage, 16, Y, FLinearColor(1.f, 0.4f, 0.4f));
		return;
	}
	// A battle replay plays with or without a game (player-retention §2).
	if (const ASovPlayerController* RPC = Cast<ASovPlayerController>(PlayerOwner); Sub && RPC && RPC->InBattle() && RPC->InReplay())
	{
		DrawBattle(*Sub, *RPC);
		return;
	}
	if (!Sub || !Sub->IsRunning())
	{
		// The main menu draws itself; only a failure needs a line here.
		if (Sub && !Sub->LastMessage.IsEmpty()) Line(Sub->LastMessage, 16, Y, FLinearColor(1.f, 0.4f, 0.4f));
		return;
	}
	// Hot seat: the screen stays dark until the next human takes over (their fog of war).
	if (Sub->GetSession().HandoverPending())
	{
		DrawRect(FLinearColor(0.01f, 0.01f, 0.015f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		float CY = Canvas->ClipY * 0.45f;
		Line(FString::Printf(TEXT("%s's turn. Hand over, then press Enter."), *Sub->GetSession().HandoverName()), Canvas->ClipX * 0.38f, CY,
			FLinearColor(1.f, 0.85f, 0.45f));
		return;
	}
	const ASovPlayerController* PC = Cast<ASovPlayerController>(PlayerOwner);
	if (PC && PC->InBattle())
	{
		DrawBattle(*Sub, *PC);
		return;
	}
	if (PC && PC->InStreet())
	{
		DrawStreet(*Sub, *PC);
		return;
	}
	if (bShowYields) DrawYields(*Sub);
	DrawLabels(*Sub);
	DrawStatus(*Sub, Y);
	// The plot under the cursor: terrain, resource, improvement, owner and yields.
	int32 TX = 0, TY = 0;
	float MX = 0.f, MY = 0.f;
	if (PC && PC->CursorHex(TX, TY) && PC->GetMousePosition(MX, MY))
	{
		const TArray<FString> Tip = SovPlotTooltip(Sub->GetGame(), Sub->GetSession().ViewPlayer(), TX, TY);
		if (Tip.Num() > 0)
		{
			const float W = 300.f, H = 8.f + 18.f * Tip.Num();
			const float Left = FMath::Min(MX + 18.f, Canvas->ClipX - W - 4.f), Top = FMath::Min(MY + 18.f, Canvas->ClipY - H - 4.f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), Left, Top, W, H);
			float TipY = Top + 4.f;
			for (const FString& L : Tip) Line(L, Left + 6.f, TipY);
		}
	}
	// Online: the latest notices and chat (whose turn it is shows in the status lines).
	if (Sub->GetSession().NetMode() != ESovNet::Local)
	{
		float NY = Canvas->ClipY - 210.f;
		for (const FString& L : Sub->NetLines) Line(L, Canvas->ClipX - 620.f, NY, FLinearColor(0.7f, 0.85f, 1.f));
	}
	float PY = Canvas->ClipY - 20.f - BottomInset - 18.f * PanelLines.Num();
	for (const FString& L : PanelLines)
	{
		Line(L, 16, PY);
	}
	if (bShowHelp) DrawHelp();
	if (bShowChronicle && Sub && Sub->IsRunning()) DrawChronicle(*Sub);
}
