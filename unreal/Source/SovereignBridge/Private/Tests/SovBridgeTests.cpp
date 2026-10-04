// Headless bridge tests (no world, no rendering):
//   UnrealEditor-Cmd.exe Sovereign.uproject -ExecCmds="Automation RunTests Sovereign; Quit" -nullrhi -unattended
#include "Misc/AutomationTest.h"

#include "SovHexLayout.h"
#include "SovMirror.h"
#include "SovSession.h"

#include "sovereign/game.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr EAutomationTestFlags kSovTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// Lets the AI play every seat until `Turns` turns have passed or the game ends.
bool PlayAITurns(FSovSession& Session, int32 Turns)
{
	const int32 StopAt = Session.GetGame().state().turn + Turns;
	int32 Guard = Turns * 64;
	while (Session.GetGame().state().turn < StopAt && !Session.IsGameOver() && Guard-- > 0)
	{
		if (!Session.StepAI())
		{
			return Session.IsGameOver();
		}
	}
	return true;
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHexLayoutTest, "Sovereign.Bridge.HexLayoutRoundTrip", kSovTestFlags)
bool FSovHexLayoutTest::RunTest(const FString& Parameters)
{
	for (int32 Row = 0; Row < 40; ++Row)
	{
		for (int32 Col = 0; Col < 60; ++Col)
		{
			const FVector C = SovHex::Center(Col, Row);
			// The centre and points well inside the hex map back to it.
			for (const FVector& Offset : {FVector::ZeroVector, FVector(60, 0, 0), FVector(-60, 0, 0), FVector(0, 60, 0), FVector(0, -60, 0)})
			{
				const FIntPoint Back = SovHex::FromWorld(C + Offset);
				if (Back != FIntPoint(Col, Row))
				{
					AddError(FString::Printf(TEXT("(%d,%d) + (%.0f,%.0f) maps to (%d,%d)"), Col, Row, Offset.X, Offset.Y, Back.X, Back.Y));
					return false;
				}
			}
		}
	}
	// Neighbours in the core are neighbours in the world: centres 2 * Size * sqrt(3)/2 apart.
	sov::HexGrid Grid(20, 20, false);
	for (int32 Dir = 0; Dir < sov::kNumDirs; ++Dir)
	{
		const sov::Hex H{7, 7};
		const std::optional<sov::Hex> N = Grid.neighbor(H, static_cast<sov::Dir>(Dir));
		TestTrue(TEXT("neighbour exists"), N.has_value());
		const double D = FVector::Dist(SovHex::Center(H.x, H.y), SovHex::Center(N->x, N->y));
		TestTrue(FString::Printf(TEXT("direction %d distance %.2f"), Dir, D), FMath::IsNearlyEqual(D, SovHex::Size * SovHex::Sqrt3, 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMirrorTest, "Sovereign.Bridge.MirrorFollowsCore", kSovTestFlags)
bool FSovMirrorTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;
	Setup.bHumanSeat0 = false;
	FString Error;
	if (!TestTrue(TEXT("game starts: ") + Error, Session.Start(Setup, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("AI plays 20 turns"), PlayAITurns(Session, 20));
	TestFalse(TEXT("no AI seat stalled"), Session.Stalled());

	const sov::Game& G = Session.GetGame();
	const sov::GameState& S = G.state();
	const sov::PlayerId Me = 0;
	const FSovMirror M = BuildMirror(G, Me);

	int32 Revealed = 0, Visible = 0;
	for (int32 i = 0; i < S.grid.size(); ++i)
	{
		const sov::Visibility V = G.visibility(Me, S.grid.at(i));
		Revealed += V != sov::Visibility::Unrevealed;
		Visible += V == sov::Visibility::Visible;
	}
	int32 Units = 0, Cities = 0;
	for (const sov::Unit& U : S.units)
	{
		Units += U.owner == Me || G.visibility(Me, U.pos) == sov::Visibility::Visible;
	}
	for (const sov::City& C : S.cities)
	{
		Cities += G.visibility(Me, C.pos) != sov::Visibility::Unrevealed;
	}
	int32 MirrorVisible = 0;
	for (const FSovTile& T : M.Tiles)
	{
		MirrorVisible += T.bVisible;
	}
	TestTrue(TEXT("seat 0 has explored"), Revealed > 0 && Revealed < S.grid.size());
	TestEqual(TEXT("tiles = revealed plots"), M.Tiles.Num(), Revealed);
	TestEqual(TEXT("visible tiles"), MirrorVisible, Visible);
	TestEqual(TEXT("unit markers"), M.Units.Num(), Units);
	TestEqual(TEXT("city markers"), M.Cities.Num(), Cities);
	TestTrue(TEXT("seat 0 founded a city"), M.Cities.ContainsByPredicate([](const FSovCityMarker& C) { return C.Owner == 0; }));
	for (const FSovUnitMarker& U : M.Units)
	{
		TestTrue(TEXT("no marker on unrevealed plots"), G.visibility(Me, sov::Hex{U.X, U.Y}) != sov::Visibility::Unrevealed);
	}
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
