// Headless bridge tests (no world, no rendering):
//   UnrealEditor-Cmd.exe Sovereign.uproject -ExecCmds="Automation RunTests Sovereign; Quit" -nullrhi -unattended
#include "Misc/AutomationTest.h"

#include "SovHexLayout.h"
#include "SovMirror.h"
#include "SovSession.h"
#include "SovStreetLayout.h"
#include "SovBattleSim.h"
#include "SovArt.h"
#include "UObject/UObjectGlobals.h"

#include "sovereign/commands.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHumanSeatTest, "Sovereign.Bridge.HumanSeatPlaysThroughCommands", kSovTestFlags)
bool FSovHumanSeatTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;  // seat 0 human, AI elsewhere
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	const sov::PlayerId Me = 0;
	TestTrue(TEXT("seat 0 starts"), Session.IsHumanTurn() && Session.GetGame().state().currentPlayer == Me);

	// Found the capital where the settler stands, as the F key does.
	sov::UnitId Settler = sov::kNoUnit;
	for (const sov::Unit& U : Session.GetGame().state().units)
	{
		if (U.owner == Me && Session.GetRules().units[static_cast<size_t>(U.type)].foundCity)
		{
			Settler = U.id;
			break;
		}
	}
	TestEqual(TEXT("found city"), Session.Submit(sov::Command::foundCity(Me, Settler)), sov::CommandError::Ok);

	// Ten turns: end the turn, answering each refusal the way the controller's choosers do
	// (first option), then let the AI seats play until it is seat 0's turn again.
	const int32 StartTurn = Session.GetGame().state().turn;
	int32 Guard = 2000;
	while (Session.GetGame().state().turn < StartTurn + 10 && !Session.IsGameOver() && Guard-- > 0)
	{
		if (Session.GetGame().battlePending())
		{
			// An AI attack on the leader stack waits for us: settle it the classic way.
			TestEqual(TEXT("auto-resolve"), Session.Submit(sov::Command::autoResolveBattle(Me)), sov::CommandError::Ok);
			continue;
		}
		if (!Session.IsHumanTurn())
		{
			if (!Session.StepAI() && !Session.IsGameOver())
			{
				AddError(TEXT("an AI seat stalled"));
				return false;
			}
			continue;
		}
		const sov::Game& G = Session.GetGame();
		const sov::CommandError Result = Session.Submit(sov::Command::endTurn(Me));
		switch (Result)
		{
			case sov::CommandError::Ok: break;
			case sov::CommandError::UnitsNeedOrders:
				for (sov::UnitId Id : G.unitsNeedingOrders(Me))
				{
					TestEqual(TEXT("skip"), Session.Submit(sov::Command::setActivity(Me, Id, sov::Activity::Skip)), sov::CommandError::Ok);
				}
				break;
			case sov::CommandError::ProductionNeeded:
				for (sov::CityId Id : G.citiesNeedingProduction(Me))
				{
					TestEqual(TEXT("production"), Session.Submit(sov::Command::setProduction(Me, Id, G.buildableItems(Id).front())), sov::CommandError::Ok);
				}
				break;
			case sov::CommandError::ResearchNeeded:
				TestEqual(TEXT("research"), Session.Submit(sov::Command::chooseResearch(Me, G.availableTechs(Me).front())), sov::CommandError::Ok);
				break;
			case sov::CommandError::CivicNeeded:
				TestEqual(TEXT("civic"), Session.Submit(sov::Command::chooseCivic(Me, G.availableCivics(Me).front())), sov::CommandError::Ok);
				break;
			default:
				AddError(FString::Printf(TEXT("end turn refused: %s"), UTF8_TO_TCHAR(sov::commandErrorName(Result))));
				return false;
		}
	}
	TestTrue(TEXT("ten turns played"), Session.GetGame().state().turn >= StartTurn + 10);
	const sov::Player& P = Session.GetGame().state().players[0];
	TestTrue(TEXT("research chosen"), P.techs.current != sov::kNone || P.techs.has(0));

	std::string ReplayError;
	const std::unique_ptr<sov::Game> Replayed = sov::Game::replay(Session.GetRules(), Session.GetCoreSetup(), Session.GetGame().log(), &ReplayError);
	TestTrue(FString::Printf(TEXT("log replays (%s)"), UTF8_TO_TCHAR(ReplayError.c_str())), Replayed != nullptr);
	if (Replayed)
	{
		TestEqual(TEXT("replayed state hash"), Replayed->stateHash(), Session.GetGame().stateHash());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLeaderMirrorTest, "Sovereign.Bridge.LeaderInMirrorAndCommands", kSovTestFlags)
bool FSovLeaderMirrorTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	const sov::Game& G = Session.GetGame();
	const sov::Unit* Leader = G.leaderOf(0);
	if (!TestNotNull(TEXT("seat 0 has a leader"), Leader))
	{
		return false;
	}
	const FSovMirror M = BuildMirror(G, 0);
	const FSovUnitMarker* Marker = M.Units.FindByPredicate([&](const FSovUnitMarker& U) { return U.Id == Leader->id; });
	if (!TestNotNull(TEXT("leader marker"), Marker))
	{
		return false;
	}
	TestTrue(TEXT("marked as the leader, not a civilian"), Marker->bLeader && !Marker->bCivilian);
	TestEqual(TEXT("labelled with the ruler"), Marker->Name, FString(UTF8_TO_TCHAR(G.state().players[0].leaderName.c_str())));
	TestEqual(TEXT("one leader marker of ours"), M.Units.FilterByPredicate([](const FSovUnitMarker& U) { return U.bLeader && U.Owner == 0; }).Num(), 1);

	// The escort link the L key sends: the starting Warrior, once it stands on the leader's plot.
	const sov::Unit* Warrior = nullptr;
	for (const sov::Unit& U : G.state().units)
	{
		if (U.owner == 0 && Session.GetRules().units[static_cast<size_t>(U.type)].layer == sov::UnitLayer::Military)
		{
			Warrior = &U;
		}
	}
	if (!TestNotNull(TEXT("starting warrior"), Warrior))
	{
		return false;
	}
	const sov::UnitId WarriorId = Warrior->id, LeaderId = Leader->id;
	if (Warrior->pos != Leader->pos)
	{
		TestEqual(TEXT("warrior joins the leader"), Session.Submit(sov::Command::move(0, WarriorId, Leader->pos)), sov::CommandError::Ok);
	}
	if (G.state().unit(WarriorId)->pos == G.state().unit(LeaderId)->pos)
	{
		TestEqual(TEXT("link escort"), Session.Submit(sov::Command::linkEscort(0, WarriorId, LeaderId)), sov::CommandError::Ok);
		TestTrue(TEXT("escort linked"), G.escortOf(*G.state().unit(LeaderId)) != nullptr);
	}
	// Assassins need an Encampment: nobody can send one at the start.
	TestEqual(TEXT("no assassins yet"), G.agentCapacity(0), 0);
	TestEqual(TEXT("cannot send a missing agent"), Session.Submit(sov::Command::sendAssassin(0, 1, 1)), sov::CommandError::CannotSendAgent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStreetLayoutTest, "Sovereign.Street.CityCenterFromGameState", kSovTestFlags)
bool FSovStreetLayoutTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	sov::UnitId Settler = sov::kNoUnit;
	for (const sov::Unit& U : Session.GetGame().state().units)
	{
		if (U.owner == 0 && Session.GetRules().units[static_cast<size_t>(U.type)].foundCity) Settler = U.id;
	}
	TestEqual(TEXT("found the capital"), Session.Submit(sov::Command::foundCity(0, Settler)), sov::CommandError::Ok);
	const sov::City& City = Session.GetGame().state().cities.back();
	const FSovStreetLayout A = BuildStreetLayout(Session.GetGame(), City.id);
	const FSovStreetLayout B = BuildStreetLayout(Session.GetGame(), City.id);
	TestEqual(TEXT("one landmark per building"), A.Count(ESovStreetPiece::Landmark), static_cast<int32>(City.buildings.size()));
	TestTrue(TEXT("the Palace is a landmark"), A.Pieces.ContainsByPredicate([](const FSovStreetPiece& P) { return P.Kind == ESovStreetPiece::Landmark && P.Label == TEXT("Palace"); }));
	TestEqual(TEXT("filler houses follow population"), A.Count(ESovStreetPiece::House) + A.Count(ESovStreetPiece::Boarded), 10 + City.population * 8);
	TestEqual(TEXT("six main streets"), A.Count(ESovStreetPiece::Street), 6);
	TestFalse(TEXT("no walls yet"), A.bWalls);
	TestEqual(TEXT("crowd by population"), A.Crowd, 6 + City.population * 4);
	TestEqual(TEXT("same hex, same result"), A.Pieces.Num(), B.Pieces.Num());
	bool bSame = A.Pieces.Num() == B.Pieces.Num();
	for (int32 i = 0; bSame && i < A.Pieces.Num(); ++i) bSame = A.Pieces[i].Location.Equals(B.Pieces[i].Location);
	TestTrue(TEXT("identical placement"), bSame);
	TestTrue(TEXT("herald and captain on the plaza"), A.Herald.Size2D() < 1800.0 && A.Captain.Size2D() < 1800.0);
	return true;
}

namespace
{
FSovBattleResult RunBattle(int32 AttackerStrength, int32 DefenderStrength, int32 Seed, bool bLeaderAlone = false)
{
	FSovBattleSpec Spec;
	Spec.Attacker = {TEXT("Swordsman"), 0, AttackerStrength, 100, false};
	Spec.Defender = {TEXT("Swordsman"), 1, DefenderStrength, 100, bLeaderAlone};
	Spec.HumanSide = 0;
	Spec.LeaderStrength = 30;
	Spec.Seed = Seed;
	FSovBattleSim Sim;
	Sim.Start(Spec);
	for (int32 i = 0; i < 20000 && !Sim.Finished(); ++i)
	{
		Sim.Step(1.f / 30.f);  // nobody at the controls: the battle AI fights for both sides
	}
	return Sim.Result();
}
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBattleSimTest, "Sovereign.Battle.NumbersDecideMostFights", kSovTestFlags)
bool FSovBattleSimTest::RunTest(const FString& Parameters)
{
	// Even fights hurt both sides; a much stronger side usually wins and loses less.
	const FSovBattleResult Even = RunBattle(35, 35, 1);
	TestTrue(TEXT("both sides bleed"), Even.ToAttacker > 0 && Even.ToDefender > 0);
	int32 StrongWins = 0, StrongLessHurt = 0;
	for (int32 Seed = 1; Seed <= 12; ++Seed)
	{
		const FSovBattleResult R = RunBattle(50, 30, Seed);
		StrongWins += R.Winner == 0;
		StrongLessHurt += R.ToAttacker < R.ToDefender;
	}
	TestTrue(FString::Printf(TEXT("strong side wins %d of 12"), StrongWins), StrongWins >= 10);
	TestTrue(FString::Printf(TEXT("strong side hurt less %d of 12"), StrongLessHurt), StrongLessHurt >= 10);
	// Same seed, same battle when nobody intervenes.
	const FSovBattleResult A = RunBattle(40, 38, 7), B = RunBattle(40, 38, 7);
	TestTrue(TEXT("deterministic without input"), A.ToAttacker == B.ToAttacker && A.ToDefender == B.ToDefender && A.LeaderWound == B.LeaderWound);
	// An unescorted leader fights alone; its damage is the unit's.
	const FSovBattleResult Lone = RunBattle(40, 16, 3, true);
	TestTrue(TEXT("lone leader takes the brunt"), Lone.ToDefender > 0);
	TestTrue(TEXT("results stay within HP"), Lone.ToDefender <= 100 && Lone.ToAttacker <= 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovArtAssetsTest, "Sovereign.Art.KitAssetsLoad", kSovTestFlags)
bool FSovArtAssetsTest::RunTest(const FString& Parameters)
{
	// Every kit asset the game names was built by tools/art and loads.
	for (const FString& Path : SovArt::RequiredAssets())
	{
		TestNotNull(*FString::Printf(TEXT("loads %s"), *Path), LoadObject<UObject>(nullptr, *Path));
	}
	TestNotNull(TEXT("kit material"), SovArt::KitMaterial());
	TestNotNull(TEXT("palace mesh"), SovArt::Mesh(TEXT("Classical"), TEXT("Palace")));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
