#include "SovStatus.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}  // namespace

TArray<FSovStatusLine> SovStatusLines(const sov::Game& G, sov::PlayerId Me)
{
	const sov::GameState& S = G.state();
	const sov::Rules& R = G.rules();
	const sov::Player& P = S.players[static_cast<size_t>(Me)];
	TArray<FSovStatusLine> Out;
	auto Add = [&Out](const FString& Text, const FLinearColor& Color = FLinearColor::White) { Out.Add({Text, Color}); };
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
		Add(FString::Printf(TEXT("%s: %s   Leader: %s   Ruler: %s   Uniques: %s"), *Str(C.name), *Str(C.ability.name), *Str(C.leaderAbility.name), *Ruler,
				 Uniques.IsEmpty() ? TEXT("-") : *Uniques.RightChop(2)), FLinearColor(0.85f, 0.8f, 0.6f));
	}
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
		Add(Faith);
	}
	// The world era, this civ's age and era score (09), and tourism (07).
	{
		static const TCHAR* Ages[] = {TEXT("Normal Age"), TEXT("Golden Age"), TEXT("Dark Age"), TEXT("Heroic Age")};
		const auto [Dark, Golden] = G.ageThresholds(Me);
		const int32 EraIndex = FMath::Clamp(S.gameEra, 0, static_cast<int32>(R.eras.size()) - 1);
		Add(FString::Printf(TEXT("%s Era, %s   Era score %d (Dark below %d, Golden at %d)   Tourism %d: %d visitors, %d at home"),
				 *Str(R.eras[static_cast<size_t>(EraIndex)].name), Ages[static_cast<size_t>(P.age) % 4], P.eraScore, Dark, Golden,
				 G.tourismPerTurn(Me), G.visitingTourists(Me), G.domesticTourists(Me)), FLinearColor(0.85f, 0.85f, 1.f));
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
		Add(Text, FLinearColor(0.75f, 0.95f, 1.f));
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
		if (!Race.IsEmpty()) Add(TEXT("Exoplanet expeditions:") + Race, FLinearColor(0.7f, 0.9f, 1.f));
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
		if (!Allies.IsEmpty()) Add(TEXT("Alliances:") + Allies, FLinearColor(0.6f, 1.f, 0.7f));
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
		if (!Text.IsEmpty()) Add(TEXT("Quests:") + Text, FLinearColor(0.8f, 0.95f, 0.8f));
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
		Add(FString::Printf(TEXT("%s: %d turns left; our score %d, leading %s (%d)"), Kinds[static_cast<int32>(C.kind)], C.endTurn - S.turn,
				 G.competitionStanding(C, Me), *Who, Best), FLinearColor(0.85f, 0.8f, 1.f));
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
		if (!Text.IsEmpty()) Add(TEXT("Emergencies:") + Text, FLinearColor(1.f, 0.55f, 0.55f));
	}
	// War weariness (08): points and the amenities every city loses to them.
	if (G.warWeariness(Me) > 0)
	{
		Add(FString::Printf(TEXT("War weariness %d (-%d amenities in every city)"), G.warWeariness(Me), G.warWearinessAmenities(Me)), FLinearColor(1.f, 0.7f, 0.5f));
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
		if (!Arsenal.IsEmpty()) Add(TEXT("Nuclear arsenals:") + Arsenal, FLinearColor(1.f, 0.6f, 0.5f));
	}
	// Climate (09: Climate and Disasters [GS]): the world's warming, its phase, our share of the CO2.
	if (S.co2 > 0 || S.climatePhase > 0)
	{
		static const TCHAR* const Roman[] = {TEXT("none"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII")};
		const int32 Tenths = G.temperatureTenths();
		Add(FString::Printf(TEXT("Climate: +%d.%d degrees, phase %s   World CO2 %lld (ours %lld%%)"), Tenths / 10, Tenths % 10,
				 Roman[FMath::Clamp(S.climatePhase, 0, 7)], static_cast<long long>(S.co2), S.co2 > 0 ? static_cast<long long>(P.co2 * 100 / S.co2) : 0LL), FLinearColor(1.f, 0.85f, 0.6f));
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
		Add(Text + TEXT("   Z: governors"), FLinearColor(0.85f, 0.8f, 1.f));
	}
	// Offers from other leaders wait for an answer on the diplomacy screen (08; leader doc §10).
	for (const sov::Deal& D : S.deals)
	{
		if (D.to != Me)
		{
			continue;
		}
		const sov::Player& From = S.players[static_cast<size_t>(D.from)];
		Add(FString::Printf(TEXT("%s offers: %s   N: diplomacy"), *Str(From.leaderName), *Str(sov::describeDeal(R, S, D))),
			FLinearColor(0.6f, 1.f, 0.7f));
	}
	if (P.envoyTokens > 0)
	{
		Add(FString::Printf(TEXT("Envoys to send: %d   O: city-states"), P.envoyTokens), FLinearColor(0.6f, 0.9f, 1.f));
	}
	if (G.tradeRouteCapacity(Me) > 0)
	{
		Add(FString::Printf(TEXT("Trade routes: %d of %d"), G.tradeRoutesOf(Me), G.tradeRouteCapacity(Me)));
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
		Add(FString::Printf(TEXT("Leader: %s  HP %d   %s"), *Str(P.leaderName), L->hp, *Gear), FLinearColor(1.f, 0.85f, 0.3f));
	}
	else if (P.captor != sov::kNoPlayer)
	{
		const sov::Player& C = S.players[static_cast<size_t>(P.captor)];
		Add(FString::Printf(TEXT("%s is held captive by %s. H: abandon and crown a successor"), *Str(P.leaderName),
				 C.civ == sov::kNone ? TEXT("?") : *Str(R.civs[static_cast<size_t>(C.civ)].name)), FLinearColor(1.f, 0.4f, 0.3f));
	}
	else if (P.successionPending)
	{
		Add(TEXT("The throne is empty. H: choose a successor"), FLinearColor(1.f, 0.4f, 0.3f));
	}
	// Dedications (09): this era's, and a reminder while one is still to choose.
	{
		FString Text;
		for (const sov::TypeIndex D : P.dedications) Text += (Text.IsEmpty() ? TEXT("") : TEXT(", ")) + Str(R.dedications[static_cast<size_t>(D)].name);
		if (!G.availableDedications(Me).empty()) Text += FString::Printf(TEXT("%sF2: choose %d dedication(s)"), Text.IsEmpty() ? TEXT("") : TEXT("   "), P.dedicationsPending);
		if (!Text.IsEmpty()) Add(TEXT("Dedications: ") + Text, FLinearColor(1.f, 0.85f, 0.5f));
	}
	// Reputation (leader doc §8.1).
	{
		const TCHAR* Standing = G.beloved(Me) ? TEXT("Beloved") : G.feared(Me) ? TEXT("Feared") : TEXT("Neither loved nor feared");
		Add(FString::Printf(TEXT("Reputation %+d: %s"), P.reputation, Standing), FLinearColor(0.8f, 0.85f, 1.f));
	}
	if (P.interregnumTurns > 0)
	{
		Add(FString::Printf(TEXT("Interregnum: policy slots empty for %d more turn(s)"), P.interregnumTurns), FLinearColor(1.f, 0.6f, 0.4f));
	}
	return Out;
}

TArray<FString> SovHelpLines()
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
	TArray<FString> Out;
	for (const TCHAR* L : Lines) Out.Add(L);
	return Out;
}
