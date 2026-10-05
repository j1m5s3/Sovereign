#include "SovHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

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
		default: return TEXT("");
	}
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

	Line(FString::Printf(TEXT("Turn %d / %d   %s%s"), S.turn, G.turnLimit(), *Civ, Sub.GetSession().IsHumanTurn() ? TEXT("") : TEXT("   (spectating)")), 16, Y);
	Line(FString::Printf(TEXT("Gold %s (%+s)   Science %s   Culture %s   Score %d"), *Str(P.gold.toString()),
			 *Str(G.goldPerTurn(Me).toString()), *Str(G.sciencePerTurn(Me).toString()), *Str(G.culturePerTurn(Me).toString()), G.score(Me)),
		16, Y);
	const FString Research = P.techs.current == sov::kNone ? TEXT("none") : Str(R.techs[static_cast<size_t>(P.techs.current)].name);
	const FString Civic = P.civics.current == sov::kNone ? TEXT("none") : Str(R.civics[static_cast<size_t>(P.civics.current)].name);
	Line(FString::Printf(TEXT("Research: %s   Civic: %s"), *Research, *Civic), 16, Y);
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
	for (const sov::GameEvent& E : S.events)
	{
		if (E.turn < S.turn - 1 || (E.actor != Me && E.target != Me))
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
			case sov::EventKind::HistoricMoment:
				Text = FString::Printf(TEXT("Historic moment: %s (+%d era score)."), *Str(R.moments[static_cast<size_t>(E.value)].name),
					R.moments[static_cast<size_t>(E.value)].eraScore);
				break;
			case sov::EventKind::NewAge:
			{
				static const TCHAR* Ages[] = {TEXT("a Normal Age"), TEXT("a Golden Age"), TEXT("a Dark Age"), TEXT("a Heroic Age")};
				Text = FString::Printf(TEXT("A new era dawns: %s begins."), Ages[static_cast<size_t>(E.value) % 4]);
				break;
			}
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
				Text = FString::Printf(TEXT("%s joins you as a %s (Y: great people)."), *Str(R.greatPeople[static_cast<size_t>(E.value)].name),
					*Str(R.greatPersonClasses[static_cast<size_t>(R.greatPeople[static_cast<size_t>(E.value)].cls)].name));
				break;
		}
		Line(FString::Printf(TEXT("Turn %d: %s"), E.turn, *Text), 16, Y, FLinearColor(1.f, 0.5f, 0.8f));
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
		Line(TEXT("You have been eliminated."), 16, Y, FLinearColor(1.f, 0.3f, 0.3f));
	}
	if (G.gameOver())
	{
		const sov::Player& W = S.players[static_cast<size_t>(S.winner)];
		const FString Winner = W.civ == sov::kNone ? TEXT("?") : Str(R.civs[static_cast<size_t>(W.civ)].name);
		Line(FString::Printf(TEXT("%s wins: %s victory"), *Winner, VictoryName(S.victory)), 16, Y, FLinearColor(1.f, 0.9f, 0.2f));
	}
	if (!Sub.LastMessage.IsEmpty())
	{
		Line(Sub.LastMessage, 16, Y, FLinearColor(1.f, 0.6f, 0.4f));
	}
}

void ASovHUD::DrawLabels(const USovGameSubsystem& Sub)
{
	const sov::Game& G = Sub.GetGame();
	const FSovMirror M = BuildMirror(G, Sub.GetSession().ViewPlayer());
	UFont* Font = GEngine->GetSmallFont();
	for (const FSovCityMarker& C : M.Cities)
	{
		const FVector Screen = Project(SovHex::Center(C.X, C.Y, 60.0));
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
			const FVector At = Project(SovHex::Center(U.X, U.Y, 110.0) + SovHex::ToWorld(FVector2D(-38.0, -30.0), 0.0));
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
		const FVector Screen = Project(SovHex::Center(U.X, U.Y, 80.0));
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
	Line(Sim.EnemyTrained() ? TEXT("The enemy is led by the trained battle AI.") : TEXT("The enemy charges (no trained battle AI found)."), 16, Y,
		FLinearColor(0.75f, 0.75f, 0.75f));
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
	float Y = 12.f;
	if (!Sub || !Sub->IsRunning())
	{
		Line(Sub ? Sub->LastMessage : FString(TEXT("No game")), 16, Y, FLinearColor(1.f, 0.4f, 0.4f));
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
	DrawLabels(*Sub);
	DrawStatus(*Sub, Y);
	float PY = Canvas->ClipY - 20.f - 18.f * PanelLines.Num();
	for (const FString& L : PanelLines)
	{
		Line(L, 16, PY);
	}
}
