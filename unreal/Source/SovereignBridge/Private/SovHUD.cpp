#include "SovHUD.h"

#include <algorithm>

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

#include "SovCameraPawn.h"
#include "SovEvents.h"
#include "SovGameSubsystem.h"
#include "SovHexLayout.h"
#include "SovMirror.h"
#include "SovStatus.h"
#include "SovPlayerController.h"
#include "SovStreetScene.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S)
{
	return UTF8_TO_TCHAR(S.c_str());
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
	if (TopInset <= 0.f)  // the top bar shows these
	Line(FString::Printf(TEXT("Gold %s (%+s)   Science %s   Culture %s   Score %d"), *Str(P.gold.toString()),
			 *Str(G.goldPerTurn(Me).toString()), *Str(G.sciencePerTurn(Me).toString()), *Str(G.culturePerTurn(Me).toString()), G.score(Me)),
		16, Y);
	const FString Research = P.techs.current == sov::kNone ? TEXT("none") : Str(R.techs[static_cast<size_t>(P.techs.current)].name);
	const FString Civic = P.civics.current == sov::kNone ? TEXT("none") : Str(R.civics[static_cast<size_t>(P.civics.current)].name);
	const FString Gov = P.government == sov::kNone ? TEXT("none") : Str(R.governments[static_cast<size_t>(P.government)].name);
	if (TopInset <= 0.f) Line(FString::Printf(TEXT("Research: %s   Civic: %s   Government: %s (F2)"), *Research, *Civic, *Gov), 16, Y);
	// The empire's standing (SovStatus; the widgets show it in the Empire panel).
	if (TopInset <= 0.f)
	{
		for (const FSovStatusLine& L : SovStatusLines(G, Me)) Line(L.Text, 16, Y, L.Color);
	}
	// Assassination news involving us from the last two turns (leader doc §6).
	int32 GossipShown = 0;
	for (const sov::GameEvent& E : S.events)
	{
		if (TopInset > 0.f) break;  // the widgets show these as notifications
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
		const FString Text = SovEventText(G, Me, E);
		Line(FString::Printf(TEXT("Turn %d: %s"), E.turn, *Text), 16, Y, FLinearColor(1.f, 0.5f, 0.8f));
	}
	if (S.currentPlayer != Me && TopInset <= 0.f)  // the end-turn button says so with the widgets
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
		Line(FString::Printf(TEXT("%s wins: %s victory. Esc opens the menu."), *Winner, SovVictoryName(S.victory)), 16, Y, FLinearColor(1.f, 0.9f, 0.2f));
	}
	if (!Sub.LastMessage.IsEmpty() && TopInset <= 0.f)  // the widgets show it as a toast
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
	const TArray<FString> Lines = SovHelpLines();
	const float W = 860.f, H = 16.f + 20.f * Lines.Num();
	const float Left = (Canvas->ClipX - W) * 0.5f, Top = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f), Left, Top, W, H);
	float Y = Top + 8.f;
	for (int32 i = 0; i < Lines.Num(); ++i) Line(Lines[i], Left + 14.f, Y, i == 0 ? FLinearColor(1.f, 0.85f, 0.45f) : FLinearColor::White);
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
	if (!Sub.Mirror.IsValid()) return;
	const FSovMirror& M = *Sub.Mirror;
	const sov::PlayerId View = static_cast<sov::PlayerId>(Sub.GetSession().ViewPlayer());
	UFont* Font = GEngine->GetSmallFont();
	const FLinearColor Ink(0.03f, 0.027f, 0.024f, 0.88f), Edge(0.42f, 0.28f, 0.11f, 1.f);
	for (const FSovCityMarker& C : M.Cities)
	{
		const FVector Screen = Project(NearCamera(PlayerOwner, SovHex::Center(C.X, C.Y, 60.0)));
		if (Screen.Z <= 0)
		{
			continue;
		}
		// A banner: the population in the owner's colour, the name (a star on a capital), loyalty once it slips
		// below Loyal [R&F]; under it, our own city's production and, when hurt, its health.
		const FString Name = FString::Printf(TEXT("%s%s"), C.bCapital ? TEXT("* ") : TEXT(""), *C.Name);
		const FString Pop = FString::FromInt(C.Population);
		const FString Loyalty = C.Loyalty <= 75 ? FString::Printf(TEXT("L%d"), C.Loyalty) : FString();
		float NW = 0, NH = 0, PW = 0, PH = 0, LW = 0, LH = 0;
		GetTextSize(Name, NW, NH, Font, 1.2f);
		GetTextSize(Pop, PW, PH, Font, 1.2f);
		if (!Loyalty.IsEmpty()) GetTextSize(Loyalty, LW, LH, Font, 1.0f);
		const float BoxW = FMath::Max(PW + 10.f, NH + 4.f), H = NH + 6.f;
		const float W = BoxW + NW + 14.f + (LW > 0 ? LW + 8.f : 0.f);
		const float L = Screen.X - W / 2, T = Screen.Y - H - 2.f;
		DrawRect(Edge, L - 1, T - 1, W + 2, H + 2);
		DrawRect(Ink, L, T, W, H);
		DrawRect(C.Color, L, T, BoxW, H);
		DrawText(Pop, FLinearColor::White, L + (BoxW - PW) / 2, T + 3.f, Font, 1.2f);
		DrawText(Name, FLinearColor(0.93f, 0.89f, 0.8f), L + BoxW + 7.f, T + 3.f, Font, 1.2f);
		if (LW > 0) DrawText(Loyalty, FLinearColor(0.95f, 0.5f, 0.4f), L + BoxW + NW + 14.f, T + 5.f, Font, 1.0f);
		float BarY = T + H + 1.f;
		if (C.Owner == View)
		{
			if (const sov::City* City = G.state().city(C.Id); City && !City->queue.empty())
			{
				const sov::ProductionItem& Item = City->queue.front();
				sov::Fixed Done;
				for (const sov::ProductionProgress& Pr : City->progress)
					if (Pr.item == Item) Done = Pr.amount;
				const int32 Cost = Item.kind == sov::ProductionKind::District ? G.districtCost(View, Item.type) : G.productionCost(View, Item, City);
				const float Frac = Cost > 0 ? FMath::Clamp(static_cast<float>(Done.toInt()) / Cost, 0.f, 1.f) : 0.f;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), L, BarY, W, 3.f);
				DrawRect(FLinearColor(0.95f, 0.6f, 0.25f, 0.95f), L, BarY, W * Frac, 3.f);
				BarY += 4.f;
			}
		}
		if (C.MaxHp > 0 && C.Hp < C.MaxHp)
		{
			const float Frac = static_cast<float>(C.Hp) / C.MaxHp;
			DrawRect(FLinearColor(0.2f, 0, 0, 0.8f), L, BarY, W, 4.f);
			DrawRect(FLinearColor(0.2f, 0.9f, 0.2f, 0.9f), L, BarY, W * Frac, 4.f);
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
		if (!RPC->UsesWidgets()) DrawBattle(*Sub, *RPC);  // else the battle screen shows it
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
		if (!PC->UsesWidgets()) DrawBattle(*Sub, *PC);
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
	if (TopInset <= 0.f && PC && PC->CursorHex(TX, TY) && PC->GetMousePosition(MX, MY))  // the widgets show it themselves
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
	// With the widgets up, the reader panel shows these.
	if (bShowHelp && TopInset <= 0.f) DrawHelp();
	if (bShowChronicle && TopInset <= 0.f && Sub && Sub->IsRunning()) DrawChronicle(*Sub);
}
