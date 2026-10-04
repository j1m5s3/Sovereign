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
		const FString Label = FString::Printf(TEXT("%s%s  %d"), C.bCapital ? TEXT("* ") : TEXT(""), *C.Name, C.Population);
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
