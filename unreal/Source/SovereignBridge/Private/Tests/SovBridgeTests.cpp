// Headless bridge tests (no world, no rendering):
//   UnrealEditor-Cmd.exe Sovereign.uproject -ExecCmds="Automation RunTests Sovereign; Quit" -nullrhi -unattended
#include "Misc/AutomationTest.h"

#include "SovDescribe.h"
#include "SovHexLayout.h"
#include "SovKeys.h"
#include "SovLens.h"
#include "SovMirror.h"
#include "SovMods.h"
#include "SovSession.h"
#include "SovStreetLayout.h"
#include "SovBattleSim.h"
#include "SovArt.h"
#include "SovDiplomacy.h"
#include "sovereign_net/session.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

#include "sovereign/challenge.h"
#include "sovereign/commands.h"
#include "sovereign/ai.h"
#include "sovereign/game.h"
#include "sovereign/serialize.h"

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
	// A wrapping map: the copies drawn a map's width west and east pick back to the same plot, and
	// neighbours across the wrap sit side by side once the far one is taken from the nearest copy.
	sov::HexGrid Wrap(20, 12, true);
	const double W = SovHex::MapWorldWidth(Wrap.width());
	for (int32 Row = 0; Row < Wrap.height(); ++Row)
	{
		for (int32 Col = 0; Col < Wrap.width(); ++Col)
		{
			const FVector C = SovHex::Center(Col, Row);
			for (const double Shift : {-W, W})
			{
				const FIntPoint P = SovHex::FromWorld(C + FVector(0.0, Shift, 0.0));
				const std::optional<sov::Hex> Back = Wrap.normalize(sov::Hex{P.X, P.Y});
				TestTrue(TEXT("a copy picks back to its plot"), Back && Back->x == Col && Back->y == Row);
			}
			for (int32 Dir = 0; Dir < sov::kNumDirs; ++Dir)
			{
				const std::optional<sov::Hex> N = Wrap.neighbor(sov::Hex{Col, Row}, static_cast<sov::Dir>(Dir));
				if (!N) continue;
				const double D = FVector::Dist(C, SovHex::NearestCopy(SovHex::Center(N->x, N->y), C.Y, W));
				if (!FMath::IsNearlyEqual(D, SovHex::Size * SovHex::Sqrt3, 0.01))
				{
					AddError(FString::Printf(TEXT("(%d,%d) and its neighbour (%d,%d) are %.1f apart"), Col, Row, N->x, N->y, D));
					return false;
				}
			}
		}
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
	TestEqual(TEXT("unexplored = the rest"), M.Unexplored.Num(), S.grid.size() - Revealed);

	// Each arm and era gets its figure (kit_figures.py).
	const sov::Rules& R = G.rules();
	for (const auto& [Id, Want] : {std::pair<const char*, const TCHAR*>{"UNIT_WARRIOR", TEXT("Soldier")}, {"UNIT_ARCHER", TEXT("Archer")},
			 {"UNIT_MUSKETMAN", TEXT("Musketeer")}, {"UNIT_INFANTRY", TEXT("Rifleman")}, {"UNIT_HORSEMAN", TEXT("Rider")},
			 {"UNIT_TANK", TEXT("Tank")}, {"UNIT_CATAPULT", TEXT("Siege")}, {"UNIT_GALLEY", TEXT("Ship")}, {"UNIT_IRONCLAD", TEXT("Steamship")},
			 {"UNIT_FIGHTER", TEXT("Plane")}, {"UNIT_SETTLER", TEXT("Citizen")}})
	{
		const sov::TypeIndex T = R.unit(Id);
		if (!TestTrue(FString(TEXT("unit exists: ")) + Id, T != sov::kNone)) continue;
		const sov::UnitType& Type = R.units[static_cast<size_t>(T)];
		TestEqual(FString(TEXT("figure of ")) + Id, SovUnitFigure(Type, false, Type.layer == sov::UnitLayer::Civilian || Type.layer == sov::UnitLayer::Support).ToString(), FString(Want));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBattleAiTest, "Sovereign.Battle.TrainedCommanderLeads", kSovTestFlags)
bool FSovBattleAiTest::RunTest(const FString& Parameters)
{
	// The trained battle AI ships in data/battle_ai and commands the side the human does not.
	const std::shared_ptr<const sov::battle::Policy> Policy = FSovBattleSim::TrainedPolicy();
	TestTrue(TEXT("trained commander loads"), Policy != nullptr);
	FSovBattleSpec Spec;
	Spec.Attacker = {TEXT("Swordsman"), 0, 36, 100, false};
	Spec.Defender = {TEXT("Swordsman"), 1, 36, 100, false};
	Spec.HumanSide = 0;
	Spec.LeaderStrength = 36;
	Spec.Seed = 5;
	FSovBattleSim Sim;
	Sim.Start(Spec, Policy);
	TestTrue(TEXT("enemy led by it"), Sim.EnemyTrained());
	Sim.SetOrder(0, sov::battle::Order::Hold, 1);
	TestTrue(TEXT("the human orders a squad"), Sim.GetOrder(0, 1) == sov::battle::Order::Hold && Sim.GetOrder(0, 0) == sov::battle::Order::Advance);
	for (int32 i = 0; i < 20000 && !Sim.Finished(); ++i)
	{
		Sim.Step(1.f / 30.f, FVector2D::ZeroVector, false);  // the leader stands still; its squads follow orders
	}
	TestTrue(TEXT("the battle ends"), Sim.Finished());
	const FSovBattleResult R = Sim.Result();
	TestTrue(TEXT("results stay within HP"), R.ToAttacker <= 100 && R.ToDefender <= 100 && R.LeaderWound <= 100);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDiplomacyTalkTest, "Sovereign.Bridge.DiplomacyTalkFallsBackToScript", kSovTestFlags)
bool FSovDiplomacyTalkTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	// Port 1: no model server answers, so the scripted leader speaks (leader doc §10, Fallback).
	FSovDiplomacyTalk Talk(Session.GetGame(), 1, 0, 1);
	TestFalse(TEXT("the persona names the leader"), Talk.GetPersona().leaderName.empty());
	Talk.Say(TEXT("Greetings. What do you want?"));
	for (int32 i = 0; i < 200 && Talk.IsBusy(); ++i)
	{
		FPlatformProcess::Sleep(0.05f);
	}
	TestTrue(TEXT("the exchange finished"), Talk.Poll());
	TestTrue(TEXT("the server check ran"), Talk.Checked());
	TestFalse(TEXT("no model was used"), Talk.UsingModel());
	TestTrue(TEXT("the leader answered"), Talk.Lines().Num() >= 3 && Talk.Lines().Last().Kind == FSovTalkLine::EKind::Leader &&
		!Talk.Lines().Last().Text.IsEmpty());
	Talk.Finish();
	for (int32 i = 0; i < 200 && Talk.IsBusy(); ++i)
	{
		FPlatformProcess::Sleep(0.05f);
	}
	sov::Command Summary;
	TestTrue(TEXT("a summary is ready"), Talk.SummaryReady(Summary));
	TestEqual(TEXT("it is a RecordTalk command"), static_cast<int32>(Summary.type), static_cast<int32>(sov::CommandType::RecordTalk));
	TestFalse(TEXT("with text"), Summary.text.empty());
	return true;
}

// Plays the seat whose turn it is in this session (the AI standing in for its person), through Submit.
static void PlaySeat(FSovSession& S)
{
	const sov::Game& G = S.GetGame();
	if (G.battlePending())
	{
		S.Submit(sov::Command::autoResolveBattle(G.state().pendingBattle.liveFor));
		return;
	}
	for (const sov::Command& C : sov::net::aiCommands(S.GetRules(), G))
	{
		S.Submit(C);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovOnlineTest, "Sovereign.Bridge.HostAndJoinStayInLockstep", kSovTestFlags)
bool FSovOnlineTest::RunTest(const FString& Parameters)
{
	FSovSetup HostSetup;
	HostSetup.Net = ESovNet::Host;
	HostSetup.Port = 17791;
	HostSetup.HumanSeats = 2;
	HostSetup.PlayerName = TEXT("Host");
	FSovSession Host, Guest;
	FString Error;
	if (!Host.Start(HostSetup, Error))
	{
		AddError(Error);
		return false;
	}
	FSovSetup JoinSetup = HostSetup;
	JoinSetup.Net = ESovNet::Join;
	JoinSetup.PlayerName = TEXT("Guest");
	if (!Guest.Start(JoinSetup, Error))
	{
		AddError(Error);
		return false;
	}
	auto PumpBoth = [&](int32 Rounds) {
		for (int32 i = 0; i < Rounds; ++i)
		{
			Host.Poll();
			Guest.Poll();
			FPlatformProcess::Sleep(0.002f);
		}
	};
	for (int32 i = 0; i < 500 && Host.LobbyLines().Num() > 1 && !Host.LobbyLines()[1].Contains(TEXT("Guest")); ++i) PumpBoth(1);
	TestTrue(TEXT("the guest took seat 2"), Host.LobbyLines().Num() > 1 && Host.LobbyLines()[1].Contains(TEXT("Guest")));
	TestTrue(TEXT("the host starts the game"), Host.StartHostedGame(Error));
	for (int32 i = 0; i < 1000 && !Guest.IsRunning(); ++i) PumpBoth(1);
	TestTrue(TEXT("the guest has the game"), Guest.IsRunning());
	TestEqual(TEXT("the guest views its own seat"), Guest.ViewPlayer(), 1);
	// Each machine plays its own seat; the host plays the rest.
	size_t HostAt = SIZE_MAX, GuestAt = SIZE_MAX;
	for (int32 Step = 0; Step < 20000 && Host.GetGame().state().turn < 8 && !Host.IsGameOver(); ++Step)
	{
		const sov::Game& HG = Host.GetGame();
		const bool bHostTurn = HG.battlePending() ? HG.state().pendingBattle.liveFor == 0 : HG.state().currentPlayer == 0;
		if (bHostTurn && HG.log().size() != HostAt)
		{
			HostAt = HG.log().size();
			PlaySeat(Host);
		}
		const sov::Game& GG = Guest.GetGame();
		const bool bGuestTurn = GG.battlePending() ? GG.state().pendingBattle.liveFor == 1 : GG.state().currentPlayer == 1;
		if (bGuestTurn && GG.log().size() == HG.log().size() && GG.log().size() != GuestAt)
		{
			GuestAt = GG.log().size();
			PlaySeat(Guest);
		}
		PumpBoth(1);
	}
	for (int32 i = 0; i < 1000 && Guest.GetGame().log().size() < Host.GetGame().log().size(); ++i) PumpBoth(1);
	TestTrue(TEXT("eight turns were played"), Host.GetGame().state().turn >= 8);
	TestEqual(TEXT("both machines hold the same game"), Guest.GetGame().stateHash(), Host.GetGame().stateHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHotSeatTest, "Sovereign.Bridge.HotSeatHandsOver", kSovTestFlags)
bool FSovHotSeatTest::RunTest(const FString& Parameters)
{
	FSovSetup Setup;
	Setup.HumanSeats = 2;
	FSovSession Session;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	Session.Poll();
	TestFalse(TEXT("seat 1 starts without a hand-over"), Session.HandoverPending());
	PlaySeat(Session);  // seat 1 (index 0) ends its turn
	Session.Poll();
	TestTrue(TEXT("the screen waits for the second player"), Session.HandoverPending());
	TestEqual(TEXT("still showing the first player's view"), Session.ViewPlayer(), 0);
	Session.TakeOver();
	TestFalse(TEXT("handed over"), Session.HandoverPending());
	TestEqual(TEXT("now the second player's view"), Session.ViewPlayer(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRemoteBattleTest, "Sovereign.Battle.OnlineSnapshotsAndRemoteOrders", kSovTestFlags)
bool FSovRemoteBattleTest::RunTest(const FString& Parameters)
{
	FSovBattleSpec Spec;
	Spec.Attacker = {TEXT("Swordsman"), 0, 36, 100, false};
	Spec.Defender = {TEXT("Spearman"), 1, 30, 100, false};
	Spec.Seed = 11;
	FSovBattleSim Host;
	Host.Start(Spec, FSovBattleSim::TrainedPolicy());
	// The other side's player commands the defenders: their order holds, whatever the AI would do.
	Host.SetRemoteEnemy(true);
	Host.SetOrder(1, sov::battle::Order::Hold);
	for (int32 i = 0; i < 40; ++i) Host.Step(0.05f);
	TestEqual(TEXT("the remote side's order stands"), static_cast<int32>(Host.GetOrder(1, 1)), static_cast<int32>(sov::battle::Order::Hold));
	// The snapshot crosses the wire and draws the same field on the other machine.
	FSovBattleSnapshot Snap;
	TestTrue(TEXT("a snapshot decodes"), Snap.Decode(Host.Snapshot().Encode()));
	FSovBattleSim View;
	View.StartRemoteView(Spec);
	View.ApplySnapshot(Snap);
	TestEqual(TEXT("same soldiers"), View.Soldiers().Num(), Host.Soldiers().Num());
	TestEqual(TEXT("same survivors"), View.Alive(0), Host.Alive(0));
	TestEqual(TEXT("same orders"), static_cast<int32>(View.GetOrder(1, 1)), static_cast<int32>(sov::battle::Order::Hold));
	bool bClose = true;
	for (int32 i = 0; i < View.Soldiers().Num(); ++i)
	{
		bClose &= FVector2D::Distance(View.Soldiers()[i].Pos, Host.Soldiers()[i].Pos) < 0.02f;
	}
	TestTrue(TEXT("positions within a hundredth"), bClose);
	TestFalse(TEXT("a truncated snapshot is refused"), Snap.Decode(std::vector<uint8_t>(3, 0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovOnlineBattleTest, "Sovereign.Bridge.OnlineLiveBattleSettlesEverywhere", kSovTestFlags)
bool FSovOnlineBattleTest::RunTest(const FString& Parameters)
{
	FSovSetup HostSetup;
	HostSetup.Net = ESovNet::Host;
	HostSetup.Port = 17792;
	HostSetup.HumanSeats = 2;
	HostSetup.bBattleDemo = true;  // seat 1's leader and warrior beside seat 2's warrior, at war
	FSovSession Host, Guest;
	FString Error;
	if (!Host.Start(HostSetup, Error))
	{
		AddError(Error);
		return false;
	}
	FSovSetup JoinSetup = HostSetup;
	JoinSetup.Net = ESovNet::Join;
	JoinSetup.bBattleDemo = false;
	if (!Guest.Start(JoinSetup, Error))
	{
		AddError(Error);
		return false;
	}
	auto PumpBoth = [&](int32 Rounds) {
		for (int32 i = 0; i < Rounds; ++i)
		{
			Host.Poll();
			Guest.Poll();
			FPlatformProcess::Sleep(0.002f);
		}
	};
	for (int32 i = 0; i < 500 && !(Host.LobbyLines().Num() > 1 && Host.LobbyLines()[1].Contains(TEXT("Player"))); ++i) PumpBoth(1);
	TestTrue(TEXT("the host starts the prepared game"), Host.StartHostedGame(Error));
	for (int32 i = 0; i < 1000 && !Guest.IsRunning(); ++i) PumpBoth(1);
	if (!Guest.IsRunning())
	{
		AddError(TEXT("the guest never got the game"));
		return false;
	}
	// The host's warrior, escorting its leader, attacks the guest's warrior: a live battle for the host.
	const sov::GameState& S = Host.GetGame().state();
	const sov::Unit* Mine = nullptr;
	const sov::Unit* Theirs = nullptr;
	for (const sov::Unit& U : S.units)
	{
		const bool bMilitary = Host.GetRules().units[static_cast<size_t>(U.type)].layer == sov::UnitLayer::Military;
		if (bMilitary && U.owner == 0 && !Mine) Mine = &U;
		if (bMilitary && U.owner == 1 && !Theirs) Theirs = &U;
	}
	if (!Mine || !Theirs)
	{
		AddError(TEXT("the battle demo did not set up both warriors"));
		return false;
	}
	TestEqual(TEXT("the attack goes in"), Host.Submit(sov::Command::attack(0, Mine->id, Theirs->pos)), sov::CommandError::Ok);
	TestTrue(TEXT("the battle waits for the host's leader"), Host.GetGame().battlePending() && Host.GetGame().state().pendingBattle.liveFor == 0);
	for (int32 i = 0; i < 500 && !Guest.GetGame().battlePending(); ++i) PumpBoth(1);
	TestTrue(TEXT("the guest sees the battle waiting"), Guest.GetGame().battlePending());
	// The field streams to the guest; the guest's orders stream back (the controller's traffic).
	Host.SendRelay(1, {1, 2, 3});
	Guest.SendRelay(0, {3, 0xFF, 1});
	TArray<TPair<int32, std::vector<uint8_t>>> AtGuest, AtHost;
	for (int32 i = 0; i < 500 && (AtGuest.Num() == 0 || AtHost.Num() == 0); ++i)
	{
		PumpBoth(1);
		AtGuest.Append(Guest.TakeRelays());
		AtHost.Append(Host.TakeRelays());
	}
	TestTrue(TEXT("the guest got the field"), AtGuest.Num() == 1 && AtGuest[0].Key == 0);
	TestTrue(TEXT("the host got the order"), AtHost.Num() == 1 && AtHost[0].Key == 1 && AtHost[0].Value.size() == 3);
	// One result command settles it on every machine.
	const sov::PendingBattle B = Host.GetGame().state().pendingBattle;
	TestEqual(TEXT("the result goes in"), Host.Submit(sov::Command::battleResult(0, B.expectedToDefender, B.expectedToAttacker, 0)), sov::CommandError::Ok);
	for (int32 i = 0; i < 500 && Guest.GetGame().log().size() < Host.GetGame().log().size(); ++i) PumpBoth(1);
	TestFalse(TEXT("settled on the guest's machine too"), Guest.GetGame().battlePending());
	TestEqual(TEXT("the same game on both"), Guest.GetGame().stateHash(), Host.GetGame().stateHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovChronicleTest, "Sovereign.Bridge.ChronicleWrittenWithoutAModel", kSovTestFlags)
bool FSovChronicleTest::RunTest(const FString& Parameters)
{
	// With no model server (port 1) the scripted chronicle is written to the file, off the game thread.
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("chronicle-test.txt"));
	IFileManager::Get().Delete(*Path);
	FSovChronicleWriter Writer;
	Writer.Start(TEXT("Elizabeth I of England"), {"Turn 1: England declared war on France.", "Turn 9: France made peace with England."}, 1, Path);
	TestTrue(TEXT("busy at first"), Writer.IsBusy());
	FString Note;
	for (int32 i = 0; i < 400 && !Writer.Poll(Note); ++i) FPlatformProcess::Sleep(0.05f);
	TestTrue(TEXT("a note for the HUD"), Note.Contains(TEXT("chronicle is written")));
	FString Text;
	TestTrue(TEXT("the file is there"), FFileHelper::LoadFileToString(Text, *Path));
	TestTrue(TEXT("its title"), Text.Contains(TEXT("The Chronicle of Elizabeth I of England")));
	TestTrue(TEXT("its events"), Text.Contains(TEXT("In the 9th year of the reign, France made peace with England.")));
	IFileManager::Get().Delete(*Path);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovModsTest, "Sovereign.Bridge.ModsLayerOverTheRules", kSovTestFlags)
bool FSovModsTest::RunTest(const FString& Parameters)
{
	// The shipped example mod is found, a game with it has its rules, and its save loads it again.
	TestTrue(TEXT("the example mod is installed"), SovMods::Discover().ContainsByPredicate([](const FSovMod& M) { return M.Id == TEXT("swift-settlers"); }));
	FSovSession Session;
	FSovSetup Setup;
	Setup.bHumanSeat0 = false;
	Setup.Mods = {TEXT("swift-settlers")};
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	const sov::Rules& R = Session.GetRules();
	TestEqual(TEXT("Settlers cost 60"), R.units[static_cast<size_t>(R.unit("UNIT_SETTLER"))].cost, 60);
	TestTrue(TEXT("the setup names the mod"), Session.GetGame().state().setup.mods == std::vector<std::string>{"swift-settlers"});
	FSovSession Resumed;
	TestTrue(TEXT("its save loads with the mod"), Resumed.LoadLocal(sov::saveGame(Session.GetGame()), Error));
	TestEqual(TEXT("the same game"), Resumed.GetGame().stateHash(), Session.GetGame().stateHash());
	FSovSession Missing;
	Setup.Mods = {TEXT("no-such-mod")};
	TestFalse(TEXT("a missing mod is refused"), Missing.Start(Setup, Error));
	TestTrue(TEXT("and named"), Error.Contains(TEXT("no-such-mod")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovChallengeTest, "Sovereign.Bridge.WeeklyChallengeIsTheWeeksGame", kSovTestFlags)
bool FSovChallengeTest::RunTest(const FString& Parameters)
{
	// This week's challenge starts from the week's setup whatever the options and mods, and a save of it
	// is still the challenge; its log passes the board's check.
	const int32 Week = FSovSession::CurrentChallengeWeek();
	FSovSession Session;
	FSovSetup Setup;
	Setup.ChallengeWeek = Week;
	Setup.Mods = {TEXT("swift-settlers")};
	Setup.Seed = 99;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	const sov::Challenge Ch = sov::weeklyChallenge(Session.GetRules(), Week);
	TestEqual(TEXT("the week's seed"), Session.GetGame().state().setup.seed, Ch.setup.seed);
	TestTrue(TEXT("no mods"), Session.GetGame().state().setup.mods.empty());
	TestEqual(TEXT("the challenge"), Session.GetChallengeWeek(), Week);
	TestFalse(TEXT("its text"), FSovSession::ChallengeText(Week).IsEmpty());
	const std::vector<uint8_t> Bytes = sov::saveGame(Session.GetGame());
	FSovSession Resumed;
	TestTrue(TEXT("its save loads"), Resumed.LoadLocal(Bytes, Error));
	TestEqual(TEXT("and is still the challenge"), Resumed.GetChallengeWeek(), Week);
	TestTrue(TEXT("the board's check passes"), sov::checkChallenge(Session.GetRules(), Ch, Bytes).valid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBattleReplayTest, "Sovereign.Battle.RecordedBattleReplays", kSovTestFlags)
bool FSovBattleReplayTest::RunTest(const FString& Parameters)
{
	// A battle fought with no input is recorded ten times a second; the file round-trips, and playing
	// its frames through the remote view ends on the field the battle ended on.
	FSovBattleSpec Spec;
	Spec.Attacker = {TEXT("Swordsman"), 0, 36, 100, false};
	Spec.Defender = {TEXT("Archer"), 1, 25, 100, false};
	Spec.Seed = 5;
	FSovBattleSim Live;
	Live.Start(Spec);
	FSovBattleRecording Rec;
	Rec.Title = TEXT("Turn 3: Swordsman attacks Archer");
	Rec.Spec = Spec;
	Rec.bWoods = true;
	Rec.Frames.push_back(Live.Snapshot().Encode());
	for (int32 Step = 0; Step < 4000 && !Live.Finished(); ++Step)
	{
		Live.Step(0.05f);
		if (Step % 2 == 1 || Live.Finished()) Rec.Frames.push_back(Live.Snapshot().Encode());
	}
	TestTrue(TEXT("the battle ended"), Live.Finished());
	FSovBattleRecording Back;
	TestTrue(TEXT("the file decodes"), Back.Decode(Rec.Encode()));
	TestEqual(TEXT("its title"), Back.Title, Rec.Title);
	TestEqual(TEXT("its frames"), Back.Frames.size(), Rec.Frames.size());
	TestTrue(TEXT("its field"), Back.bWoods && Back.Spec.Attacker.Name == TEXT("Swordsman") && Back.Spec.Seed == 5);
	FSovBattleSim Replay;
	Replay.StartRemoteView(Back.Spec);
	for (const std::vector<uint8_t>& F : Back.Frames)
	{
		FSovBattleSnapshot S;
		TestTrue(TEXT("a frame decodes"), S.Decode(F));
		Replay.ApplySnapshot(S);
	}
	TestEqual(TEXT("attackers standing at the end"), Replay.Alive(0), Live.Alive(0));
	TestEqual(TEXT("defenders standing at the end"), Replay.Alive(1), Live.Alive(1));
	// Kept where a developer can watch it: -SovReplay=<Saved>/Automation/replay-test.sovbattle
	{
		const std::vector<uint8_t> Bytes = Rec.Encode();
		TArray<uint8> Data;
		Data.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
		FFileHelper::SaveArrayToFile(Data, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("replay-test.sovbattle")));
	}
	std::vector<uint8_t> Junk = Rec.Encode();
	Junk.resize(Junk.size() - 3);
	TestFalse(TEXT("a cut file is refused"), FSovBattleRecording().Decode(Junk));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveLoadTest, "Sovereign.Bridge.SaveAndLoadResume", kSovTestFlags)
bool FSovSaveLoadTest::RunTest(const FString& Parameters)
{
	// An all-AI game plays a few turns, is saved, and resumes in a fresh session as the same game.
	FSovSession Session;
	FSovSetup Setup;
	Setup.bHumanSeat0 = false;
	FString Error;
	if (!Session.Start(Setup, Error))
	{
		AddError(Error);
		return false;
	}
	int32 Guard = 400;
	while (Session.GetGame().state().turn < 6 && Guard-- > 0 && Session.StepAI())
	{
	}
	const std::vector<uint8_t> Bytes = sov::saveGame(Session.GetGame());
	FSovSession Resumed;
	if (!Resumed.LoadLocal(Bytes, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("the same game"), Resumed.GetGame().stateHash(), Session.GetGame().stateHash());
	TestEqual(TEXT("the same turn"), Resumed.GetGame().state().turn, Session.GetGame().state().turn);
	TestTrue(TEXT("it plays on"), Resumed.StepAI());
	std::vector<uint8_t> Junk = Bytes;
	Junk.resize(Junk.size() / 2);
	TestFalse(TEXT("a truncated save is refused"), Resumed.LoadLocal(Junk, Error));
	TestTrue(TEXT("and the game in hand stays"), Resumed.IsRunning());
	return true;
}

namespace
{
// A plain human at seat 0 for the long playthrough: it only gives orders the controller offers
// (the first fitting choice, the way a newcomer clicks), and answers every End Turn refusal through
// the chooser the controller would open. Returns false, with Why, on a dead end.
struct FNaivePlayer
{
	FSovSession* Session = nullptr;
	sov::PlayerId Me = 0;
	FString Why;
	int32 Founded = 0;

	bool Ok(const sov::Command& C) const { return Session->GetGame().validate(C) == sov::CommandError::Ok; }
	bool Try(const sov::Command& C) { return Ok(C) && Session->Submit(C) == sov::CommandError::Ok; }

	// What the production chooser lists first for this city (EChooser::Production skips a new
	// district or wonder with no plot). Settlers while the empire is small, else one by the turn.
	bool ChooseProduction(sov::CityId City)
	{
		const sov::Game& G = Session->GetGame();
		const sov::Rules& R = G.rules();
		const sov::City* C = G.state().city(City);
		TArray<sov::Command> Listed;
		int32 Settler = INDEX_NONE;
		for (const sov::ProductionItem& Item : G.buildableItems(City))
		{
			sov::Hex Plot{};
			if (Item.kind == sov::ProductionKind::District && !C->district(Item.type, false))
			{
				const std::vector<sov::Hex> Plots = G.districtPlots(City, Item.type);
				if (Plots.empty()) continue;
				Plot = Plots.front();
			}
			if (Item.kind == sov::ProductionKind::Building && R.buildings[static_cast<size_t>(Item.type)].wonder &&
				std::none_of(C->wonders.begin(), C->wonders.end(), [&](const sov::CityWonder& W) { return W.building == Item.type; }))
			{
				const std::vector<sov::Hex> Plots = G.wonderPlots(City, Item.type);
				if (Plots.empty()) continue;
				Plot = Plots.front();
			}
			if (Item.kind == sov::ProductionKind::Unit && Item.formation == 0 && R.units[static_cast<size_t>(Item.type)].foundCity) Settler = Listed.Num();
			Listed.Add(sov::Command::setProduction(Me, City, Item, Plot));
		}
		if (Listed.Num() == 0)
		{
			Why = FString::Printf(TEXT("turn %d: production needed in %s but the chooser lists nothing"), G.state().turn, UTF8_TO_TCHAR(C->name.c_str()));
			return false;
		}
		int32 Cities = 0;
		for (const sov::City& Mine : G.state().cities) Cities += Mine.owner == Me;
		const int32 Pick = Settler != INDEX_NONE && Cities < 5 ? Settler : G.state().turn % Listed.Num();
		if (Session->Submit(Listed[Pick]) != sov::CommandError::Ok && Session->Submit(Listed[0]) != sov::CommandError::Ok)
		{
			Why = FString::Printf(TEXT("turn %d: the production chooser's choices are refused in %s"), G.state().turn, UTF8_TO_TCHAR(C->name.c_str()));
			return false;
		}
		return true;
	}

	// The throne chooser's choices in its order: abandon a captured leader, the heir, a unit, a regent.
	bool ChooseSuccessor()
	{
		const sov::Game& G = Session->GetGame();
		TArray<sov::Command> Listed{sov::Command::abandonLeader(Me), sov::Command::chooseSuccessor(Me, sov::Succession::Heir)};
		for (sov::UnitId Id : G.successorUnits(Me)) Listed.Add(sov::Command::chooseSuccessor(Me, sov::Succession::Unit, Id));
		Listed.Add(sov::Command::chooseSuccessor(Me, sov::Succession::Regent));
		for (const sov::Command& C : Listed)
		{
			if (Try(C)) return true;
		}
		Why = FString::Printf(TEXT("turn %d: a successor is needed but the throne offers none"), G.state().turn);
		return false;
	}

	void OrderUnit(sov::UnitId Id)
	{
		const sov::Game& G = Session->GetGame();
		const sov::Rules& R = G.rules();
		const sov::GameState& S = G.state();
		const sov::Unit* U = S.unit(Id);
		if (!U) return;
		const sov::UnitType& T = R.units[static_cast<size_t>(U->type)];
		if (T.foundCity)
		{
			// F where it stands when it may; otherwise walk to the best site nearby.
			if (Try(sov::Command::foundCity(Me, Id)))
			{
				++Founded;
				return;
			}
			sov::Hex Best = U->pos;
			int32 BestScore = 0;
			for (const sov::Hex& H : S.grid.within(U->pos, 5))
			{
				if (!G.canFoundCityAt(Me, H)) continue;
				const int32 Score = sov::ai::settleScore(G, Me, H) - 10 * S.grid.distance(U->pos, H);
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = H;
				}
			}
			if (!(Best == U->pos) && Try(sov::Command::move(Me, Id, Best))) return;
		}
		if (T.buildCharges > 0)
		{
			for (size_t I = 0; I < R.improvements.size(); ++I)
			{
				if (!R.improvements[I].tunnel && Try(sov::Command::buildImprovement(Me, Id, static_cast<sov::TypeIndex>(I)))) return;
			}
			// Off to the nearest plot of ours with nothing on it.
			for (int32 Radius = 1; Radius <= 4; ++Radius)
			{
				for (const sov::Hex& H : S.grid.within(U->pos, Radius))
				{
					const sov::Plot& P = S.plot(H);
					if (P.owner == Me && P.improvement == sov::kNone && !S.cityAt(H) && !S.districtAt(H) && !(H == U->pos) &&
						Try(sov::Command::move(Me, Id, H)))
						return;
				}
			}
		}
		const std::vector<sov::CityId> Destinations = G.tradeDestinations(Id);
		if (!Destinations.empty() && Try(sov::Command::startTradeRoute(Me, Id, Destinations.front()))) return;
		const std::vector<sov::TypeIndex> Promotions = G.availablePromotions(Id);
		if (!Promotions.empty() && Try(sov::Command::promote(Me, Id, Promotions.front()))) return;
		if (Try(sov::Command::setActivity(Me, Id, sov::Activity::Fortify))) return;
		Try(sov::Command::setActivity(Me, Id, sov::Activity::Skip));
	}

	// The side choices a player makes now and then: a pantheon, a government and its cards, envoys, governors.
	void Housekeeping()
	{
		const sov::Game& G = Session->GetGame();
		const sov::Rules& R = G.rules();
		for (sov::TypeIndex B : G.availableBeliefs(sov::BeliefClass::Pantheon))
		{
			if (Try(sov::Command::foundPantheon(Me, B))) break;
		}
		for (int32 Gov = static_cast<int32>(R.governments.size()) - 1; Gov >= 0; --Gov)
		{
			const sov::Player& P = G.state().players[static_cast<size_t>(Me)];
			if (Gov > P.government && G.canAdoptGovernment(Me, static_cast<sov::TypeIndex>(Gov)) && Try(sov::Command::changeGovernment(Me, static_cast<sov::TypeIndex>(Gov)))) break;
		}
		const int32 Slots = static_cast<int32>(G.state().players[static_cast<size_t>(Me)].policies.size());
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (G.state().players[static_cast<size_t>(Me)].policies[static_cast<size_t>(Slot)] != sov::kNone) continue;
			for (size_t Pol = 0; Pol < R.policies.size(); ++Pol)
			{
				if (G.canSetPolicy(Me, Slot, static_cast<sov::TypeIndex>(Pol)) && Try(sov::Command::setPolicy(Me, Slot, static_cast<sov::TypeIndex>(Pol)))) break;
			}
		}
		for (const sov::Player& Cs : G.state().players)
		{
			if (Cs.cityState != sov::kNone && Cs.alive) Try(sov::Command::sendEnvoy(Me, Cs.id));
		}
		for (size_t Gv = 0; Gv < R.governors.size(); ++Gv)
		{
			if (G.canAppointGovernor(Me, static_cast<sov::TypeIndex>(Gv))) Try(sov::Command::appointGovernor(Me, static_cast<sov::TypeIndex>(Gv)));
			for (const sov::City& C : G.state().cities)
			{
				if (C.owner == Me && G.canAssignGovernor(Me, static_cast<sov::TypeIndex>(Gv), C.id))
				{
					Try(sov::Command::assignGovernor(Me, static_cast<sov::TypeIndex>(Gv), C.id));
					break;
				}
			}
		}
	}

	// Seat 0's turn: orders, then End Turn until it goes through. False on a dead end.
	bool PlayTurn()
	{
		Housekeeping();
		for (sov::UnitId Id : Session->GetGame().unitsNeedingOrders(Me)) OrderUnit(Id);
		for (int32 Attempt = 0; Attempt < 32; ++Attempt)
		{
			const sov::Game& G = Session->GetGame();
			if (G.battlePending())
			{
				if (Session->Submit(sov::Command::autoResolveBattle(Me)) != sov::CommandError::Ok)
				{
					Why = FString::Printf(TEXT("turn %d: a waiting battle cannot be auto-resolved"), G.state().turn);
					return false;
				}
				continue;
			}
			const sov::CommandError Result = Session->Submit(sov::Command::endTurn(Me));
			switch (Result)
			{
				case sov::CommandError::Ok: return true;
				case sov::CommandError::UnitsNeedOrders:
					// The controller selects each waiting unit; K skips it.
					for (sov::UnitId Id : G.unitsNeedingOrders(Me))
					{
						if (!Try(sov::Command::setActivity(Me, Id, sov::Activity::Skip)))
						{
							Why = FString::Printf(TEXT("turn %d: unit %d waits for orders and cannot be skipped"), G.state().turn, Id);
							return false;
						}
					}
					break;
				case sov::CommandError::ProductionNeeded:
					for (sov::CityId Id : G.citiesNeedingProduction(Me))
					{
						if (!ChooseProduction(Id)) return false;
					}
					break;
				case sov::CommandError::ResearchNeeded:
				{
					const std::vector<sov::TypeIndex> Techs = G.availableTechs(Me);
					Session->Submit(sov::Command::chooseResearch(Me, Techs[static_cast<size_t>(G.state().turn) % Techs.size()]));
					break;
				}
				case sov::CommandError::CivicNeeded:
				{
					const std::vector<sov::TypeIndex> Civics = G.availableCivics(Me);
					Session->Submit(sov::Command::chooseCivic(Me, Civics[static_cast<size_t>(G.state().turn) % Civics.size()]));
					break;
				}
				case sov::CommandError::LeaderNeeded:
					if (!ChooseSuccessor()) return false;
					break;
				default:
					Why = FString::Printf(TEXT("turn %d: End Turn refused (%s) and the controller opens nothing for it"), G.state().turn,
						UTF8_TO_TCHAR(sov::commandErrorName(Result)));
					return false;
			}
		}
		Why = FString::Printf(TEXT("turn %d: End Turn still refused after every chooser was answered"), Session->GetGame().state().turn);
		return false;
	}
};
}  // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHumanLongGameTest, "Sovereign.Bridge.HumanSeatPlaysLongGame", kSovTestFlags)
bool FSovHumanLongGameTest::RunTest(const FString& Parameters)
{
	// A newcomer at seat 0 plays a whole game (to turn 250 or its end) through what the controller
	// offers, quicksaving and resuming every 50 turns as F5 and the menu's Continue do.
	for (const uint64 Seed : {7ull, 11ull})
	{
		TUniquePtr<FSovSession> Session = MakeUnique<FSovSession>();
		FSovSetup Setup;
		Setup.Seed = Seed;
		FString Error;
		if (!Session->Start(Setup, Error))
		{
			AddError(Error);
			return false;
		}
		FNaivePlayer Human;
		Human.Session = Session.Get();
		int32 Guard = 100000;
		int32 NextSave = 50;
		while (Session->GetGame().state().turn < 250 && !Session->IsGameOver() && Guard-- > 0)
		{
			if (Session->GetGame().battlePending())
			{
				TestEqual(TEXT("auto-resolve"), Session->Submit(sov::Command::autoResolveBattle(0)), sov::CommandError::Ok);
				continue;
			}
			if (!Session->IsHumanTurn())
			{
				if (!Session->StepAI() && !Session->IsGameOver())
				{
					AddError(FString::Printf(TEXT("seed %llu turn %d: an AI seat stalled"), Seed, Session->GetGame().state().turn));
					return false;
				}
				continue;
			}
			if (!Session->GetGame().state().players[0].alive) break;
			if (Session->GetGame().state().turn >= NextSave)
			{
				NextSave += 50;
				const std::vector<uint8_t> Bytes = sov::saveGame(Session->GetGame());
				const uint64_t Hash = Session->GetGame().stateHash();
				TUniquePtr<FSovSession> Resumed = MakeUnique<FSovSession>();
				if (!Resumed->LoadLocal(Bytes, Error))
				{
					AddError(FString::Printf(TEXT("seed %llu: the quicksave does not load: %s"), Seed, *Error));
					return false;
				}
				TestEqual(TEXT("the resumed game is the same"), Resumed->GetGame().stateHash(), Hash);
				TestTrue(TEXT("seat 0 is still the human's"), Resumed->IsHumanTurn());
				Session = MoveTemp(Resumed);
				Human.Session = Session.Get();
			}
			if (!Human.PlayTurn())
			{
				AddError(FString::Printf(TEXT("seed %llu: %s"), Seed, *Human.Why));
				return false;
			}
		}
		const sov::GameState& S = Session->GetGame().state();
		int32 Cities = 0;
		for (const sov::City& C : S.cities) Cities += C.owner == 0;
		AddInfo(FString::Printf(TEXT("seed %llu: turn %d, game over %d, seat 0 alive %d with %d cities (%d founded)"), Seed, S.turn, Session->IsGameOver() ? 1 : 0,
			S.players[0].alive ? 1 : 0, Cities, Human.Founded));
		TestTrue(TEXT("the game reached turn 250 or its end"), S.turn >= 250 || Session->IsGameOver() || !S.players[0].alive);
		TestTrue(TEXT("seat 0 founded its capital"), Human.Founded >= 1);
	}
	return true;
}

// Key rebinding (plan E, step 2) swaps: no two actions ever share a key, and movement keys stay fixed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovKeyRebindTest, "Sovereign.Bridge.KeyRebindingSwaps", kSovTestFlags)
bool FSovKeyRebindTest::RunTest(const FString& Parameters)
{
	SovKeys::ResetAll();
	TestEqual(TEXT("unbound keys are themselves"), SovKeys::Physical(EKeys::F), EKeys::F);
	SovKeys::Bind(EKeys::F, EKeys::G);  // Found onto Fortify's key: Fortify takes F
	TestEqual(TEXT("found moves to G"), SovKeys::Physical(EKeys::F), EKeys::G);
	TestEqual(TEXT("fortify takes F"), SovKeys::Physical(EKeys::G), EKeys::F);
	SovKeys::Bind(EKeys::F, EKeys::Semicolon);  // onto a free key: Fortify keeps F
	TestEqual(TEXT("found on a free key"), SovKeys::Physical(EKeys::F), EKeys::Semicolon);
	TestEqual(TEXT("fortify keeps F"), SovKeys::Physical(EKeys::G), EKeys::F);
	SovKeys::Bind(EKeys::G, EKeys::G);  // back to its own key
	TestEqual(TEXT("fortify back on G"), SovKeys::Physical(EKeys::G), EKeys::G);
	TestFalse(TEXT("movement stays fixed"), SovKeys::CanBind(EKeys::W));
	TestFalse(TEXT("the mouse stays fixed"), SovKeys::CanBind(EKeys::LeftMouseButton));
	SovKeys::Bind(EKeys::P, EKeys::Escape);
	TestEqual(TEXT("a fixed key is refused"), SovKeys::Physical(EKeys::P), EKeys::P);
	// Every action still has its own key.
	TSet<FKey> Seen;
	for (const FSovKeyAction& A : SovKeys::Actions())
	{
		const FKey K = SovKeys::Physical(A.Logical);
		TestFalse(*FString::Printf(TEXT("%s shares a key"), *A.Logical.ToString()), Seen.Contains(K));
		Seen.Add(K);
	}
	SovKeys::ResetAll();
	return true;
}

// Every lens recolours a real game's mirror and gives a legend.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLensTest, "Sovereign.Bridge.LensesTintTheMirror", kSovTestFlags)
bool FSovLensTest::RunTest(const FString& Parameters)
{
	FSovSession Session;
	FSovSetup Setup;
	Setup.bHumanSeat0 = false;
	FString Error;
	if (!TestTrue(TEXT("game starts: ") + Error, Session.Start(Setup, Error))) return false;
	PlayAITurns(Session, 30);
	const FSovMirror Plain = BuildMirror(Session.GetGame(), 0);
	for (int32 L = 1; L < static_cast<int32>(ESovLens::Count); ++L)
	{
		FSovMirror M = Plain;
		TArray<FSovLensKey> Legend;
		SovApplyLens(M, Session.GetGame(), 0, static_cast<ESovLens>(L), &Legend);
		int32 Changed = 0;
		for (int32 i = 0; i < M.Tiles.Num(); ++i) Changed += !M.Tiles[i].Color.Equals(Plain.Tiles[i].Color);
		TestTrue(*FString::Printf(TEXT("%s lens recolours plots"), SovLensName(static_cast<ESovLens>(L))), Changed > 0);
		TestTrue(*FString::Printf(TEXT("%s lens has a legend"), SovLensName(static_cast<ESovLens>(L))), Legend.Num() > 0);
	}
	return true;
}

// Every policy card and government reads as words, not as a modifier id (plan E: the government screen).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDescribeTest, "Sovereign.Bridge.PoliciesAreDescribed", kSovTestFlags)
bool FSovDescribeTest::RunTest(const FString& Parameters)
{
	sov::Rules R;
	std::string Error;
	if (!TestTrue(TEXT("rules load"), R.load({std::string(TCHAR_TO_UTF8(*FSovSetup::DefaultRulesDir()))}, &Error))) return false;
	int32 Described = 0;
	for (size_t p = 0; p < R.policies.size(); ++p)
	{
		const FString T = SovSourceText(R, sov::ModSource::Policy, static_cast<sov::TypeIndex>(p));
		if (T.IsEmpty()) continue;
		++Described;
		TestFalse(*FString::Printf(TEXT("%s reads as words: %s"), UTF8_TO_TCHAR(R.policies[p].name.c_str()), *T), T.Contains(TEXT("POLICY_")) || T.Contains(TEXT("MODIFIER")));
		if (p % 10 == 0) AddInfo(FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(R.policies[p].name.c_str()), *T));
	}
	TestTrue(TEXT("most cards have a description"), Described * 2 > static_cast<int32>(R.policies.size()));
	// Buildings and units too: every building says something, and samples are logged for review.
	for (size_t b = 0; b < R.buildings.size(); ++b)
	{
		const FString T = SovBuildingText(R, static_cast<sov::TypeIndex>(b));
		TestFalse(*FString::Printf(TEXT("%s reads as words: %s"), UTF8_TO_TCHAR(R.buildings[b].name.c_str()), *T), T.Contains(TEXT("BUILDING_")));
		if (b % 15 == 0) AddInfo(FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(R.buildings[b].name.c_str()), *T));
	}
	for (size_t u = 0; u < R.units.size(); u += 20) AddInfo(FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(R.units[u].name.c_str()), *SovUnitText(R, static_cast<sov::TypeIndex>(u))));
	// Most promotions say what they do.
	int32 Promos = 0;
	for (size_t p = 0; p < R.promotions.size(); ++p)
	{
		const FString T = SovPromotionText(R, static_cast<sov::TypeIndex>(p));
		Promos += !T.IsEmpty();
		if (p % 25 == 0) AddInfo(FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(R.promotions[p].name.c_str()), *T));
	}
	TestTrue(TEXT("most promotions have a description"), Promos * 2 > static_cast<int32>(R.promotions.size()));
	for (size_t g = 0; g < R.governments.size(); ++g)
		AddInfo(FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(R.governments[g].name.c_str()), *SovSourceText(R, sov::ModSource::Government, static_cast<sov::TypeIndex>(g))));
	return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
