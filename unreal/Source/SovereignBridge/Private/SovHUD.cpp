#include "SovHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

#include "SovGameSubsystem.h"
#include "SovHexLayout.h"
#include "SovMirror.h"

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
	const FString CurrentName = Current.civ == sov::kNone ? TEXT("Barbarians") : Str(R.civs[static_cast<size_t>(Current.civ)].name);

	Line(FString::Printf(TEXT("Turn %d / %d   %s%s"), S.turn, G.turnLimit(), *Civ, Sub.GetSession().IsHumanTurn() ? TEXT("") : TEXT("   (spectating)")), 16, Y);
	Line(FString::Printf(TEXT("Gold %s (%+s)   Science %s   Culture %s   Score %d"), *Str(P.gold.toString()),
			 *Str(G.goldPerTurn(Me).toString()), *Str(G.sciencePerTurn(Me).toString()), *Str(G.culturePerTurn(Me).toString()), G.score(Me)),
		16, Y);
	const FString Research = P.techs.current == sov::kNone ? TEXT("none") : Str(R.techs[static_cast<size_t>(P.techs.current)].name);
	const FString Civic = P.civics.current == sov::kNone ? TEXT("none") : Str(R.civics[static_cast<size_t>(P.civics.current)].name);
	Line(FString::Printf(TEXT("Research: %s   Civic: %s"), *Research, *Civic), 16, Y);
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
			return X.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(X.civ)].name);
		};
		FString Text;
		switch (E.kind)
		{
			case sov::EventKind::AssassinKilledLeader: Text = FString::Printf(TEXT("An assassin from %s killed the ruler of %s."), *CivOf(E.actor), *CivOf(E.target)); break;
			case sov::EventKind::AssassinWoundedLeader: Text = FString::Printf(TEXT("An assassin from %s wounded the ruler of %s (%d damage)."), *CivOf(E.actor), *CivOf(E.target), E.value); break;
			case sov::EventKind::AssassinKilled: Text = FString::Printf(TEXT("An assassin sent against %s was killed."), *CivOf(E.target)); break;
			case sov::EventKind::AssassinCaptured: Text = FString::Printf(TEXT("%s caught an assassin sent by %s."), *CivOf(E.target), *CivOf(E.actor)); break;
		}
		Line(FString::Printf(TEXT("Turn %d: %s"), E.turn, *Text), 16, Y, FLinearColor(1.f, 0.5f, 0.8f));
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
	DrawLabels(*Sub);
	DrawStatus(*Sub, Y);
	float PY = Canvas->ClipY - 20.f - 18.f * PanelLines.Num();
	for (const FString& L : PanelLines)
	{
		Line(L, 16, PY);
	}
}
