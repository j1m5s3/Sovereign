#include "SovPlayerController.h"

#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

#include "SovCameraPawn.h"
#include "SovGameSubsystem.h"
#include "SovHUD.h"
#include "SovHexLayout.h"
#include "SovMapActor.h"
#include "SovStreetScene.h"
#include "SovBattleScene.h"
#include "SovDiplomacyPanel.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/PlatformProcess.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Engine/World.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S)
{
	return UTF8_TO_TCHAR(S.c_str());
}

// Turns left before the leader can take another stance in this city.
int32 rules_cooldown(const sov::Game& G, const sov::City& C)
{
	return FMath::Max(0, C.stanceTurn + G.rules().globalInt("STANCE_COOLDOWN_TURNS") - G.state().turn);
}

FString ItemName(const sov::Rules& R, sov::ProductionItem Item)
{
	const size_t T = static_cast<size_t>(Item.type);
	switch (Item.kind)
	{
		case sov::ProductionKind::Unit:
		{
			// Trained whole as a Corps/Fleet or an Army/Armada (05).
			const bool bSea = R.units[T].domain == sov::Domain::Sea;
			if (Item.formation == 1) return Str(R.units[T].name) + (bSea ? TEXT(" Fleet") : TEXT(" Corps"));
			if (Item.formation == 2) return Str(R.units[T].name) + (bSea ? TEXT(" Armada") : TEXT(" Army"));
			return Str(R.units[T].name);
		}
		case sov::ProductionKind::Building: return Str(R.buildings[T].name);
		case sov::ProductionKind::District: return Str(R.districts[T].name);
		case sov::ProductionKind::Project: return Str(R.projects[T].name);
	}
	return TEXT("?");
}

int32 TurnsFor(int32 Remaining, int32 PerTurn)
{
	return PerTurn > 0 ? FMath::Max(1, (Remaining + PerTurn - 1) / PerTurn) : 999;
}

const FKey DigitKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
constexpr int32 PageSize = 9;
}  // namespace

ASovPlayerController::ASovPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
}

void ASovPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

USovGameSubsystem* ASovPlayerController::Subsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USovGameSubsystem>() : nullptr;
}

ASovCameraPawn* ASovPlayerController::CameraPawn() const
{
	return Cast<ASovCameraPawn>(GetPawn());
}

sov::PlayerId ASovPlayerController::Me() const
{
	const USovGameSubsystem* Sub = Subsystem();
	return static_cast<sov::PlayerId>(Sub ? Sub->GetSession().ViewPlayer() : 0);
}

bool ASovPlayerController::MyTurn() const
{
	const USovGameSubsystem* Sub = Subsystem();
	return Sub && Sub->IsRunning() && Sub->GetSession().IsHumanTurn() && !Sub->GetSession().IsGameOver() &&
		Sub->GetGame().state().currentPlayer == Me();
}

void ASovPlayerController::CenterOnHome()
{
	USovGameSubsystem* Sub = Subsystem();
	ASovCameraPawn* Cam = CameraPawn();
	if (!Sub || !Sub->IsRunning() || !Cam)
	{
		return;
	}
	const sov::GameState& S = Sub->GetGame().state();
	for (const sov::City& C : S.cities)
	{
		if (C.owner == Me() && C.capital)
		{
			Cam->LookAt(SovHex::Center(C.pos.x, C.pos.y));
			return;
		}
	}
	for (const sov::Unit& U : S.units)
	{
		if (U.owner == Me())
		{
			Cam->LookAt(SovHex::Center(U.pos.x, U.pos.y));
			return;
		}
	}
}

void ASovPlayerController::UpdateCamera(float DeltaTime)
{
	ASovCameraPawn* Cam = CameraPawn();
	if (!Cam)
	{
		return;
	}
	FVector2D Dir(0, 0);
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Dir.Y += 1;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Dir.Y -= 1;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Dir.X += 1;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Dir.X -= 1;
	if (!Dir.IsZero())
	{
		Cam->Pan(Dir.GetSafeNormal() * Cam->Height * PanSpeed * DeltaTime);
	}
	if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) Cam->Zoom(1.f);
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) Cam->Zoom(-1.f);
	if (WasInputKeyJustPressed(EKeys::Home)) CenterOnHome();
}

bool ASovPlayerController::HexUnderCursor(int32& OutX, int32& OutY) const
{
	FVector Origin, Dir;
	if (!DeprojectMousePositionToWorld(Origin, Dir) || Dir.Z >= -KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Ground = Origin + Dir * (-Origin.Z / Dir.Z);
	const FIntPoint P = SovHex::FromWorld(Ground);
	const USovGameSubsystem* Sub = Subsystem();
	const std::optional<sov::Hex> H = Sub->GetGame().state().grid.normalize(sov::Hex{P.X, P.Y});
	if (!H)
	{
		return false;
	}
	OutX = H->x;
	OutY = H->y;
	return true;
}

bool ASovPlayerController::Send(const sov::Command& Command)
{
	return Subsystem()->Submit(Command) == sov::CommandError::Ok;
}

void ASovPlayerController::SelectUnit(int32 Id, bool bCenter)
{
	SelectedUnit = Id;
	SelectedCity = -1;
	Chooser = EChooser::None;
	const sov::Unit* U = Subsystem()->GetGame().state().unit(Id);
	if (U && bCenter && CameraPawn())
	{
		CameraPawn()->LookAt(SovHex::Center(U->pos.x, U->pos.y));
	}
}

void ASovPlayerController::SelectCity(int32 Id, bool bCenter)
{
	SelectedCity = Id;
	SelectedUnit = -1;
	Chooser = EChooser::None;
	const sov::City* C = Subsystem()->GetGame().state().city(Id);
	if (C && bCenter && CameraPawn())
	{
		CameraPawn()->LookAt(SovHex::Center(C->pos.x, C->pos.y));
	}
}

void ASovPlayerController::SelectNextUnit()
{
	const sov::Game& G = Subsystem()->GetGame();
	const std::vector<sov::UnitId> Waiting = G.unitsNeedingOrders(Me());
	if (Waiting.empty())
	{
		SelectedUnit = -1;
		return;
	}
	// The first waiting unit after the current one, wrapping around.
	for (sov::UnitId Id : Waiting)
	{
		if (Id > SelectedUnit)
		{
			SelectUnit(Id, true);
			return;
		}
	}
	SelectUnit(Waiting.front(), true);
}

void ASovPlayerController::AfterUnitOrder()
{
	const sov::Game& G = Subsystem()->GetGame();
	const sov::Unit* U = G.state().unit(SelectedUnit);
	if (!U || U->moveTarget || U->movesLeft <= sov::Fixed() || U->activity != sov::Activity::Awake)
	{
		SelectNextUnit();
	}
}

void ASovPlayerController::ClickSelect(int32 X, int32 Y)
{
	const sov::Game& G = Subsystem()->GetGame();
	const sov::GameState& S = G.state();
	TArray<int32> Mine;
	for (const sov::Unit& U : S.units)
	{
		if (U.owner == Me() && U.pos.x == X && U.pos.y == Y)
		{
			Mine.Add(U.id);
		}
	}
	const int32 At = Mine.IndexOfByKey(SelectedUnit);
	const sov::City* City = S.cityAt(sov::Hex{X, Y});
	const bool bMyCity = City && City->owner == Me();
	if (Mine.Num() > 0 && (At == INDEX_NONE || At + 1 < Mine.Num()))
	{
		// Clicking a plot again cycles through its units, then its city.
		SelectUnit(Mine[At == INDEX_NONE ? 0 : At + 1], false);
	}
	else if (bMyCity && SelectedCity != City->id)
	{
		SelectCity(City->id, false);
	}
	else if (Mine.Num() > 0)
	{
		SelectUnit(Mine[0], false);
	}
	else
	{
		SelectedUnit = SelectedCity = -1;
		Chooser = EChooser::None;
	}
}

void ASovPlayerController::ClickOrder(int32 X, int32 Y)
{
	const sov::Game& G = Subsystem()->GetGame();
	const sov::GameState& S = G.state();
	const sov::Hex Target{X, Y};
	const bool bEnemyCity = S.cityAt(Target) && S.cityAt(Target)->owner != Me();
	const bool bEnemyUnit = S.foreignUnitAt(Target, Me()) && G.visibility(Me(), Target) == sov::Visibility::Visible;
	if (SelectedCity >= 0)
	{
		// Ctrl+right-click: the city's Encampment fires (03: Defense).
		const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
		Send(bCtrl ? sov::Command::encampmentStrike(Me(), SelectedCity, Target) : sov::Command::cityStrike(Me(), SelectedCity, Target));
		return;
	}
	const sov::Unit* U = S.unit(SelectedUnit);
	if (!U)
	{
		return;
	}
	// Ctrl+right-click with a bomber or Nuclear Submarine: deliver the strongest device held (05: Nuclear weapons).
	if ((IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl)) && G.rules().units[static_cast<size_t>(U->type)].deliversWmd)
	{
		const sov::Player& P = S.players[static_cast<size_t>(Me())];
		for (int32 W = static_cast<int32>(P.wmds.size()) - 1; W >= 0; --W)
		{
			if (P.wmds[static_cast<size_t>(W)] > 0)
			{
				Send(sov::Command::launchWmd(Me(), U->id, static_cast<sov::TypeIndex>(W), Target));
				AfterUnitOrder();
				return;
			}
		}
		Subsystem()->LastMessage = TEXT("No nuclear devices to deliver.");
		return;
	}
	if (bEnemyCity || bEnemyUnit)
	{
		const sov::UnitType& T = G.rules().units[static_cast<size_t>(U->type)];
		const bool bRanged = T.ranged > 0 || T.bombard > 0;
		if (bRanged && G.previewAttack(U->id, Target, true).valid)
		{
			Send(sov::Command::rangedAttack(Me(), U->id, Target));
		}
		else if (G.previewAttack(U->id, Target, false).valid)
		{
			Send(sov::Command::attack(Me(), U->id, Target));
		}
		else if (S.grid.distance(U->pos, Target) > 1 || !bRanged)
		{
			// Not in reach: walk towards it (the core stops next to blockers).
			Send(sov::Command::move(Me(), U->id, Target));
		}
		else
		{
			Send(sov::Command::attack(Me(), U->id, Target));  // shows why it was refused
		}
	}
	else if (G.isAircraft(*U))
	{
		// Aircraft rebase to one of our air bases with room (05: air units).
		Send(sov::Command::rebaseUnit(Me(), U->id, Target));
	}
	else
	{
		Send(sov::Command::move(Me(), U->id, Target));
	}
	AfterUnitOrder();
}

void ASovPlayerController::EndTurn()
{
	USovGameSubsystem* Sub = Subsystem();
	const sov::CommandError Result = Sub->Submit(sov::Command::endTurn(Me()));
	const sov::Game& G = Sub->GetGame();
	switch (Result)
	{
		case sov::CommandError::Ok:
			SelectedUnit = SelectedCity = -1;
			Chooser = EChooser::None;
			break;
		case sov::CommandError::UnitsNeedOrders:
			SelectNextUnit();
			break;
		case sov::CommandError::ProductionNeeded:
		{
			const std::vector<sov::CityId> Cities = G.citiesNeedingProduction(Me());
			if (!Cities.empty())
			{
				SelectCity(Cities.front(), true);
				OpenChooser(EChooser::Production);
			}
			break;
		}
		case sov::CommandError::ResearchNeeded: OpenChooser(EChooser::Research); break;
		case sov::CommandError::CivicNeeded: OpenChooser(EChooser::Civic); break;
		case sov::CommandError::LeaderNeeded: OpenChooser(EChooser::Throne); break;
		default: break;
	}
}

void ASovPlayerController::OpenChooser(EChooser Kind)
{
	const sov::Game& G = Subsystem()->GetGame();
	const sov::Rules& R = G.rules();
	const sov::Player& P = G.state().players[static_cast<size_t>(Me())];
	Choices.Reset();
	ChooserPage = 0;
	Chooser = Kind;
	switch (Kind)
	{
		case EChooser::Production:
		{
			if (SelectedCity < 0)
			{
				const std::vector<sov::CityId> Waiting = G.citiesNeedingProduction(Me());
				for (const sov::City& C : G.state().cities)
				{
					if (C.owner == Me())
					{
						SelectCity(Waiting.empty() ? C.id : Waiting.front(), true);
						Chooser = Kind;
						break;
					}
				}
			}
			const sov::City* City = G.state().city(SelectedCity);
			if (!City)
			{
				break;
			}
			ChooserTitle = FString::Printf(TEXT("Production in %s"), *Str(City->name));
			const int32 PerTurn = static_cast<int32>(G.cityReport(City->id).yields[static_cast<size_t>(sov::YieldType::Production)].toInt());
			for (const sov::ProductionItem& Item : G.buildableItems(City->id))
			{
				sov::Hex Plot{};
				FString Where;
				if (Item.kind == sov::ProductionKind::District && !City->district(Item.type, false))
				{
					// A new district goes on the plot with the best adjacency.
					const std::vector<sov::Hex> Plots = G.districtPlots(City->id, Item.type);
					if (Plots.empty())
					{
						continue;
					}
					sov::Fixed Best = sov::Fixed::fromInt(-1);
					for (const sov::Hex& H : Plots)
					{
						sov::Fixed Sum;
						for (const sov::Fixed& Y : G.districtAdjacency(Me(), Item.type, H))
						{
							Sum += Y;
						}
						if (Best < Sum)
						{
							Best = Sum;
							Plot = H;
						}
					}
					Where = FString::Printf(TEXT(" at (%d,%d), +%s adjacency"), Plot.x, Plot.y, *Str(Best.toString()));
				}
				if (Item.kind == sov::ProductionKind::Building && R.buildings[static_cast<size_t>(Item.type)].wonder &&
					std::none_of(City->wonders.begin(), City->wonders.end(), [&](const sov::CityWonder& W) { return W.building == Item.type; }))
				{
					// A world wonder goes on the first plot that suits it (03: Wonders).
					const std::vector<sov::Hex> Plots = G.wonderPlots(City->id, Item.type);
					if (Plots.empty())
					{
						continue;
					}
					Plot = Plots.front();
					Where = FString::Printf(TEXT(" (wonder) at (%d,%d)"), Plot.x, Plot.y);
				}
				const int32 Cost = Item.kind == sov::ProductionKind::District ? G.districtCost(Me(), Item.type) : G.productionCost(Me(), Item);
				Choices.Add({FString::Printf(TEXT("%s (%d turns)%s"), *ItemName(R, Item), TurnsFor(Cost, PerTurn), *Where),
					sov::Command::setProduction(Me(), City->id, Item, Plot)});
			}
			// Religious units and worship buildings are bought with Faith (06).
			for (size_t u = 0; u < R.units.size(); ++u)
			{
				const sov::ProductionItem Item{sov::ProductionKind::Unit, static_cast<sov::TypeIndex>(u)};
				const int32 Faith = G.faithPurchaseCost(Me(), *City, Item);
				if (Faith > 0)
				{
					Choices.Add({FString::Printf(TEXT("Buy %s for %d faith"), *Str(R.units[u].name), Faith), sov::Command::purchaseWithFaith(Me(), City->id, Item)});
				}
			}
			for (size_t b = 0; b < R.buildings.size(); ++b)
			{
				const sov::ProductionItem Item{sov::ProductionKind::Building, static_cast<sov::TypeIndex>(b)};
				const int32 Faith = G.faithPurchaseCost(Me(), *City, Item);
				if (Faith > 0)
				{
					Choices.Add({FString::Printf(TEXT("Buy %s for %d faith"), *Str(R.buildings[b].name), Faith), sov::Command::purchaseWithFaith(Me(), City->id, Item)});
				}
			}
			// Gold buys units and buildings outright (02: Purchasing), and plots next to the city's border.
			for (const sov::ProductionItem& Item : G.buildableItems(City->id))
			{
				if (Item.kind != sov::ProductionKind::Unit && Item.kind != sov::ProductionKind::Building) continue;
				const sov::Command Buy = sov::Command::purchase(Me(), City->id, Item);
				if (G.validate(Buy) != sov::CommandError::Ok) continue;
				Choices.Add({FString::Printf(TEXT("Buy %s for %d gold"), *ItemName(R, Item), G.purchaseCost(Me(), Item)), Buy});
			}
			// A placed district, with Reyna's Contractor (Gold) or Moksha's Divine Architect (Faith) here (08: Governors).
			for (const sov::CityDistrict& D : City->districts)
			{
				if (D.complete) continue;
				const sov::ProductionItem Item{sov::ProductionKind::District, D.type};
				for (const bool bFaith : {false, true})
				{
					const sov::Command Buy = bFaith ? sov::Command::purchaseWithFaith(Me(), City->id, Item) : sov::Command::purchase(Me(), City->id, Item);
					if (G.validate(Buy) == sov::CommandError::Ok)
						Choices.Add({FString::Printf(TEXT("Buy the %s for %d %s"), *ItemName(R, Item), G.districtPurchaseCost(*City, D.type, bFaith), bFaith ? TEXT("faith") : TEXT("gold")), Buy});
				}
			}
			for (const sov::Hex& H : G.state().grid.within(City->pos, 3))
			{
				const sov::Command Buy = sov::Command::buyPlot(Me(), City->id, H);
				if (G.validate(Buy) == sov::CommandError::Ok)
					Choices.Add({FString::Printf(TEXT("Buy the tile at %d,%d for %d gold"), H.x, H.y, G.plotPurchaseCost(City->id, H)), Buy});
			}
			if (G.canRazeCity(Me(), City->id)) Choices.Add({TEXT("Raze this city"), sov::Command::razeCity(Me(), City->id)});
			break;
		}
		case EChooser::Research:
		{
			ChooserTitle = TEXT("Research");
			const int32 PerTurn = static_cast<int32>(G.sciencePerTurn(Me()).toInt());
			for (sov::TypeIndex T : G.availableTechs(Me()))
			{
				const int32 Left = G.techCost(T) - static_cast<int32>(P.techs.progress[static_cast<size_t>(T)].toInt());
				Choices.Add({FString::Printf(TEXT("%s (%d turns)"), *Str(R.techs[static_cast<size_t>(T)].name), TurnsFor(Left, PerTurn)),
					sov::Command::chooseResearch(Me(), T)});
			}
			break;
		}
		case EChooser::Civic:
		{
			ChooserTitle = TEXT("Civics");
			const int32 PerTurn = static_cast<int32>(G.culturePerTurn(Me()).toInt());
			for (sov::TypeIndex T : G.availableCivics(Me()))
			{
				const int32 Left = G.civicCost(T) - static_cast<int32>(P.civics.progress[static_cast<size_t>(T)].toInt());
				Choices.Add({FString::Printf(TEXT("%s (%d turns)"), *Str(R.civics[static_cast<size_t>(T)].name), TurnsFor(Left, PerTurn)),
					sov::Command::chooseCivic(Me(), T)});
			}
			break;
		}
		case EChooser::Gear:
		{
			const sov::Unit* U = G.state().unit(SelectedUnit);
			if (!U || !G.isLeader(*U))
			{
				break;
			}
			ChooserTitle = TEXT("Gear (in your city; takes the leader's turn)");
			for (size_t i = 0; i < R.gear.size(); ++i)
			{
				const sov::GearType& Gear = R.gear[i];
				const sov::TypeIndex Id = static_cast<sov::TypeIndex>(i);
				if (!G.gearUnlocked(Me(), Id) || U->gear[static_cast<size_t>(Gear.slot)] == Id)
				{
					continue;
				}
				FString Stats = Gear.slot == sov::GearSlot::Armor ? FString::Printf(TEXT("+%d defence"), Gear.defense)
					: Gear.slot == sov::GearSlot::Mount ? FString::Printf(TEXT("+%d moves, upkeep x2"), Gear.moves)
					: Gear.ranged > 0 ? FString::Printf(TEXT("%d melee, %d ranged (range %d)"), Gear.combat, Gear.ranged, Gear.range)
					: FString::Printf(TEXT("%d melee"), Gear.combat);
				if (Gear.strategicResource != sov::kNone)
				{
					Stats += FString::Printf(TEXT(", %d %s"), Gear.strategicCost, *Str(R.resources[static_cast<size_t>(Gear.strategicResource)].name));
				}
				Choices.Add({FString::Printf(TEXT("%s: %s, %d gold"), *Str(Gear.name), *Stats, G.gearCost(Id)), sov::Command::equipGear(Me(), U->id, Id)});
			}
			if (U->gear[static_cast<size_t>(sov::GearSlot::Mount)] != sov::kNone)
			{
				Choices.Add({TEXT("Dismount"), sov::Command::removeGear(Me(), U->id, sov::GearSlot::Mount)});
			}
			break;
		}
		case EChooser::Throne:
		{
			ChooserTitle = TEXT("The throne");
			if (P.captor != sov::kNoPlayer)
			{
				Choices.Add({FString::Printf(TEXT("Abandon %s and crown a successor"), *Str(P.leaderName)), sov::Command::abandonLeader(Me())});
				break;
			}
			if (G.hasHeir(Me()))
			{
				const sov::Dynasty* D = R.dynastyOf(P.civ);
				const FString Heir = Str(D->names[static_cast<size_t>(P.dynastyNext)]);
				Choices.Add({FString::Printf(TEXT("The heir, %s"), *Heir), sov::Command::chooseSuccessor(Me(), sov::Succession::Heir)});
				// The heir may keep one of the fallen leader's promotions (leader doc §5).
				for (sov::TypeIndex Kept : P.savedPromotions)
				{
					Choices.Add({FString::Printf(TEXT("The heir, %s, keeping %s"), *Heir, *Str(R.promotions[static_cast<size_t>(Kept)].name)),
						sov::Command::chooseSuccessor(Me(), sov::Succession::Heir, sov::kNoUnit, Kept)});
				}
			}
			for (sov::UnitId Id : G.successorUnits(Me()))
			{
				const sov::Unit* U = G.state().unit(Id);
				Choices.Add({FString::Printf(TEXT("%s, level %d (the unit is lost)"), *Str(R.units[static_cast<size_t>(U->type)].name), U->level()),
					sov::Command::chooseSuccessor(Me(), sov::Succession::Unit, Id)});
			}
			if (G.canSucceed(Me(), sov::Succession::Regent, sov::kNoUnit))
			{
				Choices.Add({TEXT("A regent"), sov::Command::chooseSuccessor(Me(), sov::Succession::Regent)});
			}
			break;
		}
		case EChooser::Pantheon:
		case EChooser::ReligionFounder:
		case EChooser::ReligionFollower:
		case EChooser::Evangelize:
		{
			// 06: a pantheon belief; a religion's Founder then Follower belief; an Apostle's new belief.
			auto Note = [&](sov::TypeIndex B) {
				const sov::BeliefType& Bt = R.beliefs[static_cast<size_t>(B)];
				return FString::Printf(TEXT("%s: %s%s"), *Str(Bt.name), *Str(Bt.text).Left(110), G.beliefModelled(B) ? TEXT("") : TEXT(" (not in the game yet)"));
			};
			if (Kind == EChooser::Pantheon)
			{
				ChooserTitle = TEXT("Choose a pantheon");
				for (sov::TypeIndex B : G.availableBeliefs(sov::BeliefClass::Pantheon))
				{
					Choices.Add({Note(B), sov::Command::foundPantheon(Me(), B)});
				}
			}
			else if (Kind == EChooser::Evangelize)
			{
				ChooserTitle = TEXT("The Apostle adds a belief to your religion (or spreads it here)");
				if (G.canSpreadReligion(ReligionUnit))
				{
					Choices.Add({TEXT("Spread the religion here"), sov::Command::spreadReligion(Me(), ReligionUnit)});
				}
				for (int32 Cls = static_cast<int32>(sov::BeliefClass::Follower); Cls < sov::kNumBeliefClasses; ++Cls)
				{
					for (sov::TypeIndex B : G.availableBeliefs(static_cast<sov::BeliefClass>(Cls)))
					{
						if (G.canEvangelize(ReligionUnit, B))
						{
							Choices.Add({Note(B), sov::Command::evangelizeBelief(Me(), ReligionUnit, B)});
						}
					}
				}
			}
			else
			{
				// The first religion no one has founded, with the chosen beliefs.
				sov::TypeIndex Religion = sov::kNone;
				for (size_t r = 0; r < R.religions.size() && Religion == sov::kNone; ++r)
				{
					bool bTaken = false;
					for (const sov::FoundedReligion& F : G.state().religions)
					{
						bTaken |= F.type == static_cast<sov::TypeIndex>(r);
					}
					if (!bTaken)
					{
						Religion = static_cast<sov::TypeIndex>(r);
					}
				}
				const FString Name = Religion == sov::kNone ? TEXT("?") : Str(R.religions[static_cast<size_t>(Religion)].name);
				if (Kind == EChooser::ReligionFounder)
				{
					ChooserTitle = FString::Printf(TEXT("Found %s: choose a Founder belief"), *Name);
					for (sov::TypeIndex B : G.availableBeliefs(sov::BeliefClass::Founder))
					{
						Choices.Add({Note(B), sov::Command::foundPantheon(Me(), B)});  // placeholder: picking moves on to the Follower
					}
				}
				else
				{
					ChooserTitle = FString::Printf(TEXT("Found %s: choose a Follower belief"), *Name);
					for (sov::TypeIndex B : G.availableBeliefs(sov::BeliefClass::Follower))
					{
						Choices.Add({Note(B), sov::Command::foundReligion(Me(), ReligionUnit, Religion, PendingFounder, B)});
					}
				}
			}
			break;
		}
		case EChooser::CityStates:
		{
			// 08: each city-state we have met, its kind, our envoys and its suzerain; picking sends an envoy.
			ChooserTitle = FString::Printf(TEXT("City-states (%d envoys to send). Pick one to send an envoy"), P.envoyTokens);
			static const TCHAR* Kinds[] = {TEXT("Scientific"), TEXT("Cultural"), TEXT("Religious"), TEXT("Trade"), TEXT("Industrial"), TEXT("Militaristic")};
			for (const sov::Player& Cs : G.state().players)
			{
				if (Cs.cityState == sov::kNone || !Cs.alive)
				{
					continue;
				}
				bool bMet = false;
				for (const sov::City& C : G.state().cities)
				{
					bMet |= C.owner == Cs.id && G.visibility(Me(), C.pos) != sov::Visibility::Unrevealed;
				}
				if (!bMet)
				{
					continue;
				}
				const sov::CityStateType& T = R.cityStates[static_cast<size_t>(Cs.cityState)];
				const sov::PlayerId Suz = G.suzerainOf(Cs.id);
				const FString SuzName = Suz == sov::kNoPlayer ? FString(TEXT("none"))
					: Suz == Me() ? FString(TEXT("you"))
					: Str(G.state().players[static_cast<size_t>(Suz)].leaderName);
				Choices.Add({FString::Printf(TEXT("%s (%s): your envoys %d, suzerain %s"), *Str(T.name), Kinds[static_cast<size_t>(T.kind) % 6],
								 G.envoysAt(Me(), Cs.id), *SuzName),
					sov::Command::sendEnvoy(Me(), Cs.id)});
			}
			break;
		}
		case EChooser::Governors:
		{
			// 08: appoint with a title, promote along the tree, or send one to the selected city.
			const int32 Titles = G.governorTitlesLeft(Me());
			const sov::City* Sel = SelectedCity >= 0 ? G.state().city(SelectedCity) : nullptr;
			ChooserTitle = FString::Printf(TEXT("Governors: %d title(s) to spend%s"), Titles,
				Sel ? *FString::Printf(TEXT(", %s selected"), *Str(Sel->name)) : TEXT(" (select a city to assign one)"));
			for (size_t i = 0; i < R.governors.size(); ++i)
			{
				const sov::TypeIndex T = static_cast<sov::TypeIndex>(i);
				const sov::GovernorType& Gt = R.governors[i];
				const sov::Governor* Gv = G.governor(Me(), T);
				if (!Gv)
				{
					if (G.canAppointGovernor(Me(), T))
					{
						Choices.Add({FString::Printf(TEXT("Appoint %s the %s: %s"), *Str(Gt.name), *Str(Gt.title),
										 *Str(R.governorPromotions[static_cast<size_t>(Gt.promotions.front())].effects)),
							sov::Command::appointGovernor(Me(), T)});
					}
					continue;
				}
				if (Sel && G.canAssignGovernor(Me(), T, Sel->id))
				{
					Choices.Add({FString::Printf(TEXT("Send %s to %s (%d turns to establish)"), *Str(Gt.name), *Str(Sel->name), G.governorEstablishTurns(T)),
						sov::Command::assignGovernor(Me(), T, Sel->id)});
				}
				for (sov::TypeIndex Promo : Gt.promotions)
				{
					if (G.canPromoteGovernor(Me(), T, Promo))
					{
						const sov::GovernorPromotionType& Pt = R.governorPromotions[static_cast<size_t>(Promo)];
						Choices.Add({FString::Printf(TEXT("Promote %s: %s (%s)"), *Str(Gt.name), *Str(Pt.name), *Str(Pt.effects)),
							sov::Command::promoteGovernor(Me(), T, Promo)});
					}
				}
			}
			break;
		}
		case EChooser::Diplomacy:
		{
			// Every major civ we have met: its leader, how it feels about us, and any offer it waits on.
			ChooserTitle = TEXT("Diplomacy: pick a leader to speak with");
			for (const sov::Player& O : G.state().players)
			{
				if (O.id == Me() || !G.isMajorCiv(O.id) || !G.hasMet(Me(), O.id))
				{
					continue;
				}
				const FString Offer = OfferFrom(O.id) ? FString(TEXT("  [an offer waits]")) : FString();
				Choices.Add({FString::Printf(TEXT("%s of %s: %s (%+d)%s"), *Str(O.leaderName), *Str(R.civs[static_cast<size_t>(O.civ)].name),
								 UTF8_TO_TCHAR(sov::relationshipName(G.relationship(O.id, Me()))), G.opinionOf(O.id, Me()), *Offer),
					sov::Command::denounce(Me(), O.id)});  // only arg is used: Pick opens the screen
			}
			// War and peace (08): a declaration (with any casus belli held), or peace once the war allows.
			static const TCHAR* const Reasons[] = {TEXT(""), TEXT("Holy War"), TEXT("War of Liberation"), TEXT("Reconquest War"), TEXT("Protectorate War"),
				TEXT("Colonial War"), TEXT("War of Territorial Expansion"), TEXT("Ideological War"), TEXT("War of Retribution")};
			for (const sov::Player& O : G.state().players)
			{
				if (O.id == Me() || !G.isMajorCiv(O.id) || !G.hasMet(Me(), O.id)) continue;
				const FString Who = Str(R.civs[static_cast<size_t>(O.civ)].name);
				if (G.canDeclareWar(Me(), O.id))
				{
					const bool bFormal = G.denouncing(Me(), O.id);
					Choices.Add({FString::Printf(TEXT("Declare %s war on %s"), bFormal ? TEXT("a formal") : TEXT("a surprise"), *Who), sov::Command::declareWar(Me(), O.id)});
					for (int32 W = 1; W < sov::kNumCasusBelli; ++W)
					{
						if (G.hasCasusBelli(Me(), O.id, static_cast<sov::CasusBelli>(W)))
						{
							Choices.Add({FString::Printf(TEXT("Declare a %s on %s (%d%% grievances)"), Reasons[W], *Who, G.casusBelliGrievancePercent(static_cast<sov::CasusBelli>(W))),
								sov::Command::declareWarFor(Me(), O.id, static_cast<sov::CasusBelli>(W))});
						}
					}
				}
				if (G.canMakePeace(Me(), O.id)) Choices.Add({FString::Printf(TEXT("Offer peace to %s"), *Who), sov::Command::makePeace(Me(), O.id)});
				// Delegations and embassies (08): access levels.
				for (const bool bEmbassy : {false, true})
				{
					const sov::Command Send = sov::Command::sendDelegation(Me(), O.id, bEmbassy);
					if (G.validate(Send) == sov::CommandError::Ok)
						Choices.Add({FString::Printf(TEXT("Send %s to %s (%d gold; access now %s)"), bEmbassy ? TEXT("a resident embassy") : TEXT("a delegation"), *Who,
										 bEmbassy ? 50 : 25, UTF8_TO_TCHAR(sov::Game::accessName(G.accessLevel(Me(), O.id)))),
							Send});
				}
				// Promises [GS] (30 favor each).
				static const TCHAR* const Promises[] = {TEXT("not to settle near us"), TEXT("not to convert our cities"), TEXT("not to spy on us"), TEXT("not to dig in our lands")};
				for (int32 K = 0; K < sov::kNumPromiseKinds; ++K)
				{
					const sov::Command Ask = sov::Command::askPromise(Me(), O.id, static_cast<sov::PromiseKind>(K));
					if (G.validate(Ask) == sov::CommandError::Ok) Choices.Add({FString::Printf(TEXT("Ask %s to promise %s (30 favor)"), *Who, Promises[K]), Ask});
				}
			}
			break;
		}
		case EChooser::TradeRoute:
		{
			// 07: the Trader's destinations in range, with what each pays its city per turn.
			ChooserTitle = FString::Printf(TEXT("Trade route (%d of %d in use, %d turns)"), G.tradeRoutesOf(Me()), G.tradeRouteCapacity(Me()), G.tradeRouteLength());
			const sov::City* From = G.tradeOrigin(ReligionUnit);
			for (sov::CityId Dest : G.tradeDestinations(ReligionUnit))
			{
				const sov::City& D = *G.state().city(Dest);
				const sov::Yields Y = G.tradeRouteYields(*From, D);
				FString Pays;
				static const TCHAR* Names[] = {TEXT("Food"), TEXT("Production"), TEXT("Gold"), TEXT("Science"), TEXT("Culture"), TEXT("Faith")};
				for (size_t k = 0; k < sov::kNumYields; ++k)
				{
					if (Y[k] > sov::Fixed())
					{
						Pays += FString::Printf(TEXT(" +%s %s"), *Str(Y[k].toString()), Names[k]);
					}
				}
				Choices.Add({FString::Printf(TEXT("%s%s:%s"), *Str(D.name), D.owner == Me() ? TEXT("") : TEXT(" (abroad)"), *Pays),
					sov::Command::startTradeRoute(Me(), ReligionUnit, Dest)});
			}
			break;
		}
		case EChooser::GreatPeople:
		{
			// 07: each class offers one person to everyone; points earn them, gold or faith buys them now.
			ChooserTitle = TEXT("Great people (points / cost, +per turn). Pick one to buy it now");
			for (size_t c = 0; c < R.greatPersonClasses.size(); ++c)
			{
				const sov::TypeIndex Cls = static_cast<sov::TypeIndex>(c);
				const sov::TypeIndex Who = G.currentGreatPerson(Cls);
				const FString ClassName = Str(R.greatPersonClasses[c].name);
				if (Who == sov::kNone)
				{
					continue;
				}
				const sov::GreatPersonType& Gp = R.greatPeople[static_cast<size_t>(Who)];
				const int32 Have = c < P.greatPersonPoints.size() ? P.greatPersonPoints[c] : 0;
				const FString Head = FString::Printf(TEXT("%s: %s (%s) %d/%d, +%d"), *ClassName, *Str(Gp.name), *Str(R.eras[static_cast<size_t>(Gp.era)].name), Have,
					G.greatPersonCost(Who), G.greatPersonPointsPerTurn(Me(), Cls));
				const int32 Gold = G.patronageCost(Me(), Cls, false);
				const int32 Faith = G.patronageCost(Me(), Cls, true);
				if (Gold > 0)
				{
					Choices.Add({FString::Printf(TEXT("%s   buy %d gold"), *Head, Gold), sov::Command::patronizeGreatPerson(Me(), Cls, false)});
				}
				if (Faith > 0 && P.faith >= sov::Fixed::fromInt(Faith))
				{
					Choices.Add({FString::Printf(TEXT("%s   buy %d faith"), *Head, Faith), sov::Command::patronizeGreatPerson(Me(), Cls, true)});
				}
				const sov::Command Pass = sov::Command::passGreatPerson(Me(), Cls);
				if (G.validate(Pass) == sov::CommandError::Ok) Choices.Add({FString::Printf(TEXT("%s   pass on %s"), *ClassName, *Str(Gp.name)), Pass});
			}
			break;
		}
		case EChooser::Government:
		{
			// 04: Governments and policies. A new government, then a card for each slot of the current one.
			const sov::TypeIndex Current = P.government;
			ChooserTitle = FString::Printf(TEXT("Government: %s%s"), Current == sov::kNone ? TEXT("none") : *Str(R.governments[static_cast<size_t>(Current)].name),
				P.anarchyTurns > 0 ? *FString::Printf(TEXT(" (anarchy, %d turns)"), P.anarchyTurns) : TEXT(""));
			for (size_t g = 0; g < R.governments.size(); ++g)
			{
				if (static_cast<sov::TypeIndex>(g) != Current && G.canAdoptGovernment(Me(), static_cast<sov::TypeIndex>(g)))
					Choices.Add({FString::Printf(TEXT("Adopt %s"), *Str(R.governments[g].name)), sov::Command::changeGovernment(Me(), static_cast<sov::TypeIndex>(g))});
			}
			static const TCHAR* const SlotNames[] = {TEXT("Military"), TEXT("Economic"), TEXT("Diplomatic"), TEXT("Wildcard"), TEXT("Great Person")};
			if (Current != sov::kNone)
			{
				const sov::GovernmentType& Gov = R.governments[static_cast<size_t>(Current)];
				for (int32 Slot = 0; Slot < Gov.totalSlots(); ++Slot)
				{
					const sov::TypeIndex In = static_cast<size_t>(Slot) < P.policies.size() ? P.policies[static_cast<size_t>(Slot)] : sov::kNone;
					const FString Holds = In == sov::kNone ? FString(TEXT("empty")) : Str(R.policies[static_cast<size_t>(In)].name);
					for (size_t pol = 0; pol < R.policies.size(); ++pol)
					{
						if (static_cast<sov::TypeIndex>(pol) == In || !G.canSetPolicy(Me(), Slot, static_cast<sov::TypeIndex>(pol))) continue;
						Choices.Add({FString::Printf(TEXT("%s slot %d (%s): %s"), SlotNames[static_cast<int32>(sov::Game::slotType(Gov, Slot))], Slot + 1, *Holds,
										 *Str(R.policies[pol].name)),
							sov::Command::setPolicy(Me(), Slot, static_cast<sov::TypeIndex>(pol))});
					}
				}
			}
			break;
		}
		case EChooser::Assassins:
		{
			ChooserTitle = FString::Printf(TEXT("Agents: assassins %d of %d (one per Encampment), spies %d of %d"), G.agentsOf(Me()), G.agentCapacity(Me()),
				G.spiesOf(Me()), G.spyCapacity(Me()));
			static const TCHAR* const Missions[] = {TEXT("idle"), TEXT("Counterspy"), TEXT("Listening Post"), TEXT("Gain Sources"), TEXT("Siphon Funds"),
				TEXT("Steal Tech Boost"), TEXT("Sabotage Production"), TEXT("Neutralize Governor"), TEXT("Foment Unrest"),
				TEXT("Great Work Heist"), TEXT("Recruit Partisans"), TEXT("Breach Dam"), TEXT("Disrupt Rocketry"), TEXT("Fabricate Scandal")};
			for (const sov::Agent& A : G.state().agents)
			{
				if (A.owner != Me())
				{
					continue;
				}
				if (A.spy)
				{
					// 08: pick the spy, then the city and operation.
					const sov::City* At = G.state().city(A.city);
					const FString Where = At ? FString::Printf(TEXT("%s in %s%s"), Missions[static_cast<int32>(A.mission) % sov::kNumSpyMissions], *Str(At->name),
												   A.travel > 0 ? *FString::Printf(TEXT(", arriving in %d"), A.travel) : TEXT(""))
											 : FString(TEXT("at home"));
					sov::Command Open = sov::Command::spyMission(Me(), A.id, sov::SpyMission::None, sov::kNoCity);
					Open.arg = -1;  // opens the spy's missions instead of being sent
					Choices.Add({FString::Printf(TEXT("Spy %d (level %d): %s"), A.id, A.level, *Where), Open});
					continue;
				}
				if (A.target != sov::kNoPlayer)
				{
					const sov::Player& T = G.state().players[static_cast<size_t>(A.target)];
					Choices.Add({FString::Printf(TEXT("Recall assassin %d (level %d, hunting %s%s)"), A.id, A.level,
									 *Str(R.civs[static_cast<size_t>(T.civ)].name), A.travel > 0 ? TEXT(", travelling") : TEXT("")),
						sov::Command::sendAssassin(Me(), A.id, sov::kNoPlayer)});
					continue;
				}
				for (const sov::Player& T : G.state().players)
				{
					if (T.id == Me() || !T.alive || T.barbarian || T.cityState != sov::kNone)
					{
						continue;
					}
					const sov::Unit* L = G.leaderOf(T.id);
					const FString Odds = L && G.visibility(Me(), L->pos) == sov::Visibility::Visible
											 ? FString::Printf(TEXT(", %d%% now"), G.assassinSuccessPercent(A, *L))
											 : FString();
					Choices.Add({FString::Printf(TEXT("Send assassin %d (level %d) after %s of %s%s"), A.id, A.level, *Str(T.leaderName),
									 *Str(R.civs[static_cast<size_t>(T.civ)].name), *Odds),
						sov::Command::sendAssassin(Me(), A.id, T.id)});
				}
			}
			break;
		}
		case EChooser::Congress:
		{
			// 08 [GS]: one free vote on each resolution in session, more bought with favor.
			const int32 Favor = G.state().players[static_cast<size_t>(Me())].favor;
			ChooserTitle = FString::Printf(TEXT("World Congress: %d favor; casting %d vote(s) (+%d bought for %d favor)"), Favor, 1 + CongressExtraVotes,
				CongressExtraVotes, sov::Game::extraVoteCost(CongressExtraVotes));
			// Emergencies (08): join any we may.
			static const TCHAR* const Kinds[] = {TEXT("Military"), TEXT("City-State"), TEXT("Religious"), TEXT("Nuclear"), TEXT("Betrayal")};
			for (size_t k = 0; k < G.state().emergencies.size(); ++k)
			{
				const sov::Emergency& E = G.state().emergencies[k];
				if (!G.canJoinEmergency(Me(), static_cast<int32>(k))) continue;
				const sov::Player& T = G.state().players[static_cast<size_t>(E.target)];
				const FString Who = T.civ == sov::kNone ? FString(TEXT("?")) : Str(R.civs[static_cast<size_t>(T.civ)].name);
				Choices.Add({FString::Printf(TEXT("Join the %s Emergency against %s (until turn %d)"), Kinds[static_cast<int32>(E.kind)], *Who, E.endTurn),
					sov::Command::joinEmergency(Me(), static_cast<int32>(k))});
			}
			if (!G.congressInSession())
			{
				break;
			}
			sov::Command More = sov::Command::congressVote(Me(), -1, 0, 0);  // id -1: buy one more vote for the next cast
			Choices.Add({FString::Printf(TEXT("Buy another vote (total %d favor)"), sov::Game::extraVoteCost(CongressExtraVotes + 1)), More});
			for (size_t k = 0; k < G.state().congress.size(); ++k)
			{
				if (G.hasVoted(Me(), static_cast<int32>(k)))
				{
					continue;
				}
				const sov::CongressItem& Item = G.state().congress[k];
				const sov::ResolutionType& Res = R.resolutions[static_cast<size_t>(Item.resolution)];
				for (int32 Opt = 0; Opt < 2; ++Opt)
				{
					for (size_t c = 0; c < Item.candidates.size(); ++c)
					{
						Choices.Add({FString::Printf(TEXT("%s, %s: %s (%s)"), *Str(Res.name), Opt == 0 ? TEXT("A") : TEXT("B"),
										 *Str(G.candidateName(Item, static_cast<int32>(c))), *Str(Opt == 0 ? Res.optionA : Res.optionB)),
							sov::Command::congressVote(Me(), static_cast<int32>(k), Opt, static_cast<int32>(c), CongressExtraVotes)});
					}
				}
			}
			break;
		}
		case EChooser::SpyMissions:
		{
			static const TCHAR* const Missions[] = {TEXT("Home"), TEXT("Counterspy"), TEXT("Listening Post"), TEXT("Gain Sources"), TEXT("Siphon Funds"),
				TEXT("Steal Tech Boost"), TEXT("Sabotage Production"), TEXT("Neutralize Governor"), TEXT("Foment Unrest"),
				TEXT("Great Work Heist"), TEXT("Recruit Partisans"), TEXT("Breach Dam"), TEXT("Disrupt Rocketry"), TEXT("Fabricate Scandal")};
			ChooserTitle = FString::Printf(TEXT("Spy %d: choose an operation (cities you have seen)"), SpyAgent);
			Choices.Add({TEXT("Bring the spy home"), sov::Command::spyMission(Me(), SpyAgent, sov::SpyMission::None, sov::kNoCity)});
			// A promotion to choose first (08: Espionage levels).
			if (const sov::Agent* Spy = G.agent(SpyAgent); Spy && Spy->promotionsPending > 0)
			{
				for (size_t Pr = 0; Pr < R.spyPromotions.size(); ++Pr)
				{
					const sov::Command Promote = sov::Command::promoteSpy(Me(), SpyAgent, static_cast<sov::TypeIndex>(Pr));
					if (G.validate(Promote) == sov::CommandError::Ok) Choices.Add({FString::Printf(TEXT("Promote: %s"), *Str(R.spyPromotions[Pr].name)), Promote});
				}
			}
			for (const sov::City& Cty : G.state().cities)
			{
				if (G.visibility(Me(), Cty.pos) == sov::Visibility::Unrevealed)
				{
					continue;
				}
				for (int32 M = 1; M < sov::kNumSpyMissions; ++M)
				{
					const sov::SpyMission Mission = static_cast<sov::SpyMission>(M);
					if (!G.canSpyMission(Me(), SpyAgent, Mission, Cty.id))
					{
						continue;
					}
					const int32 Odds = G.spySuccessPercent(SpyAgent, Mission, Cty.id);
					Choices.Add({FString::Printf(TEXT("%s in %s%s"), Missions[M], *Str(Cty.name), Odds < 100 ? *FString::Printf(TEXT(" (%d%%)"), Odds) : TEXT("")),
						sov::Command::spyMission(Me(), SpyAgent, Mission, Cty.id)});
				}
			}
			break;
		}
		case EChooser::Promotion:
		{
			const sov::Unit* U = G.state().unit(SelectedUnit);
			if (!U)
			{
				break;
			}
			ChooserTitle = TEXT("Promotion or upgrade (either ends the unit's turn)");
			for (sov::TypeIndex Pr : G.availablePromotions(U->id))
			{
				const sov::PromotionType& T = R.promotions[static_cast<size_t>(Pr)];
				const FString Branch = T.branch.empty() ? FString() : FString::Printf(TEXT(" [%s]"), *Str(T.branch));
				Choices.Add({FString::Printf(TEXT("%s%s"), *Str(T.name), *Branch), sov::Command::promote(Me(), U->id, Pr)});
			}
			// Upgrade to the next unit in the line (05: Upgrades), in our territory for gold.
			const sov::TypeIndex To = R.units[static_cast<size_t>(U->type)].upgradesTo;
			if (To != sov::kNone && G.upgradeProblem(U->id) == sov::CommandError::Ok)
			{
				Choices.Add({FString::Printf(TEXT("Upgrade to %s (%d gold)"), *Str(R.units[static_cast<size_t>(To)].name), G.upgradeCost(*U)),
					sov::Command::upgradeUnit(Me(), U->id)});
			}
			break;
		}
		case EChooser::Improvement:
		{
			const sov::Unit* U = G.state().unit(SelectedUnit);
			if (!U)
			{
				break;
			}
			ChooserTitle = TEXT("Build improvement");
			for (sov::TypeIndex T : G.improvementsAt(Me(), U->pos))
			{
				// Only what this unit builds (Military Engineers: Fort, Airstrip, Missile Silo).
				const sov::Command Build = sov::Command::buildImprovement(Me(), U->id, T);
				if (G.validate(Build) == sov::CommandError::Ok)
				{
					Choices.Add({Str(R.improvements[static_cast<size_t>(T)].name), Build});
				}
			}
			if (G.canHarvestAt(Me(), U->pos))
			{
				Choices.Add({TEXT("Harvest"), sov::Command::harvest(Me(), U->id)});
			}
			// Archaeology (07): an Archaeologist digs the site it stands on.
			if (G.excavateProblem(Me(), U->id) == sov::CommandError::Ok)
			{
				Choices.Add({TEXT("Excavate an Artifact"), sov::Command::excavate(Me(), U->id)});
			}
			// Formations (05): merge with a neighbouring twin.
			for (const sov::Unit& W : G.state().units)
			{
				if (G.formationProblem(Me(), U->id, W.id) == sov::CommandError::Ok)
				{
					Choices.Add({FString::Printf(TEXT("Form %s with unit %d"), U->formation == 0 ? TEXT("a Corps") : TEXT("an Army"), W.id), sov::Command::formUnit(Me(), U->id, W.id)});
				}
			}
			// Pillage (military units in enemy land) and repair (Builders on a pillaged improvement of ours).
			if (G.pillageProblem(Me(), U->id) == sov::CommandError::Ok)
			{
				Choices.Add({TEXT("Pillage"), sov::Command::pillage(Me(), U->id)});
			}
			for (const sov::Hex& Shore : G.state().grid.within(U->pos, 1))
			{
				if (G.coastalRaidProblem(Me(), U->id, Shore) == sov::CommandError::Ok)
				{
					Choices.Add({FString::Printf(TEXT("Coastal raid at %d,%d"), Shore.x, Shore.y), sov::Command::coastalRaid(Me(), U->id, Shore)});
				}
			}
			if (G.repairProblem(Me(), U->id) == sov::CommandError::Ok)
			{
				Choices.Add({TEXT("Repair the improvement"), sov::Command::repairImprovement(Me(), U->id)});
			}
			// Military Engineers [GS]: a railroad here, a tunnel into a neighbouring mountain.
			if (G.railroadProblem(Me(), U->id) == sov::CommandError::Ok)
			{
				Choices.Add({TEXT("Railroad (1 Iron, 1 Coal)"), sov::Command::buildRailroad(Me(), U->id)});
			}
			const sov::TypeIndex Tunnel = R.improvement("IMPROVEMENT_MOUNTAIN_TUNNEL");
			for (const sov::Hex& Site : G.tunnelSites(Me(), U->id))
			{
				Choices.Add({FString::Printf(TEXT("Mountain Tunnel at %d,%d"), Site.x, Site.y), sov::Command::buildTunnel(Me(), U->id, Tunnel, Site)});
			}
			break;
		}
		default: break;
	}
	if (Choices.Num() == 0)
	{
		Subsystem()->LastMessage = TEXT("Nothing to choose here.");
		Chooser = EChooser::None;
	}
}

void ASovPlayerController::Pick(int32 Index)
{
	const int32 I = ChooserPage * PageSize + Index;
	if (!Choices.IsValidIndex(I))
	{
		return;
	}
	const EChooser Was = Chooser;
	const sov::Command Command = Choices[I].Command;
	Chooser = EChooser::None;
	if (Was == EChooser::Assassins && Command.type == sov::CommandType::SpyMission && Command.arg == -1)
	{
		SpyAgent = Command.id;  // a spy: choose its operation
		OpenChooser(EChooser::SpyMissions);
		return;
	}
	if (Was == EChooser::Congress)
	{
		if (Command.id < 0)
		{
			++CongressExtraVotes;  // then choose again
			OpenChooser(EChooser::Congress);
			return;
		}
		if (Send(Command))
		{
			CongressExtraVotes = 0;
		}
		return;
	}
	if (Was == EChooser::Diplomacy && Command.type == sov::CommandType::Denounce)
	{
		OpenDiplomacy(static_cast<sov::PlayerId>(Command.arg));  // the civ to talk to, not a command to send
		return;
	}
	if (Was == EChooser::ReligionFounder)
	{
		PendingFounder = static_cast<sov::TypeIndex>(Command.arg);  // the Founder belief; now the Follower
		OpenChooser(EChooser::ReligionFollower);
		return;
	}
	if (Send(Command) && Was == EChooser::Improvement)
	{
		AfterUnitOrder();
	}
}

// ------------------------------------------------------------------ street scenes

void ASovPlayerController::LookAround(float DeltaTime)
{
	FRotator Look = GetControlRotation();
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		float DX = 0.f, DY = 0.f;
		GetInputMouseDelta(DX, DY);
		Look.Yaw += DX * 2.5f;
		Look.Pitch = FMath::ClampAngle(Look.Pitch + DY * 2.5f, -60.f, 20.f);
	}
	if (IsInputKeyDown(EKeys::Q)) Look.Yaw -= 90.f * DeltaTime;
	if (IsInputKeyDown(EKeys::E)) Look.Yaw += 90.f * DeltaTime;
	SetControlRotation(Look);
}

sov::PlayerId ASovPlayerController::BattleOpponent() const
{
	const USovGameSubsystem* Sub = Subsystem();
	if (!Sub || !Sub->IsRunning() || Sub->GetSession().NetMode() == ESovNet::Local || !Sub->GetGame().battlePending())
	{
		return sov::kNoPlayer;
	}
	const sov::GameState& S = Sub->GetGame().state();
	const sov::PendingBattle& B = S.pendingBattle;
	const sov::Unit* A = S.unit(B.attacker);
	const sov::Unit* D = S.unit(B.defender);
	const sov::City* C = S.city(B.city);
	if (!A || (!D && !C))
	{
		return sov::kNoPlayer;
	}
	const sov::PlayerId Other = A->owner == B.liveFor ? (D ? D->owner : C->owner) : A->owner;
	return Other != B.liveFor && S.players[static_cast<size_t>(Other)].human ? Other : sov::kNoPlayer;
}

void ASovPlayerController::HandleBattleRelays()
{
	USovGameSubsystem* Sub = Subsystem();
	if (!Sub || Sub->GetSession().NetMode() == ESovNet::Local)
	{
		return;
	}
	FSovSession& Session = Sub->GetSessionMut();
	for (const TPair<int32, std::vector<uint8_t>>& R : Session.TakeRelays())
	{
		const std::vector<uint8_t>& M = R.Value;
		if (M.empty()) continue;
		const sov::PlayerId From = static_cast<sov::PlayerId>(R.Key);
		switch (M[0])
		{
			case 1:  // a snapshot of the field, for the side watching
				if (InBattle() && Sim.RemoteView() && From == BattlePeer)
				{
					FSovBattleSnapshot Snap;
					if (Snap.Decode(std::vector<uint8_t>(M.begin() + 1, M.end()))) Sim.ApplySnapshot(Snap);
				}
				break;
			case 2:  // the other side's player takes command of their men
				if (InBattle() && !Sim.RemoteView())
				{
					BattlePeer = From;
					Sim.SetRemoteEnemy(true);
					Sub->LastMessage = FString::Printf(TEXT("%s takes command of their men."),
						UTF8_TO_TCHAR(Sub->GetGame().state().players[static_cast<size_t>(From)].leaderName.c_str()));
				}
				else if (!InBattle())
				{
					PendingJoin = From;
				}
				break;
			case 3:  // an order for the remote side's squads
				if (InBattle() && !Sim.RemoteView() && From == BattlePeer && M.size() >= 3)
				{
					const int32 Squad = static_cast<int8>(M[1]);
					const int32 Order = FMath::Clamp<int32>(M[2], 0, sov::battle::kOrders - 1);
					Sim.SetOrder(1 - Sim.GetSpec().HumanSide, static_cast<sov::battle::Order>(Order), Squad < 0 ? -1 : FMath::Min(Squad, 2));
				}
				break;
			case 4:  // they leave: the trained AI leads their men again
				if (InBattle() && !Sim.RemoteView() && From == BattlePeer)
				{
					Sim.SetRemoteEnemy(false);
					BattlePeer = sov::kNoPlayer;
				}
				break;
			default: break;
		}
	}
}

void ASovPlayerController::StartBattle(bool bRemoteView)
{
	USovGameSubsystem* Sub = Subsystem();
	const sov::Game& G = Sub->GetGame();
	const sov::PendingBattle& B = G.state().pendingBattle;
	const sov::Unit* A = G.state().unit(B.attacker);
	const sov::Unit* D = G.state().unit(B.defender);
	const sov::City* C = G.state().city(B.city);  // a city assault: the city's garrison defends
	if (!A || (!D && !C))
	{
		Send(sov::Command::autoResolveBattle(Me()));
		return;
	}
	// The game autosaves when a live battle starts (engine doc).
	if (!bRemoteView)
	{
		Sub->SaveGame(TEXT("autosave"));
	}
	const sov::Unit* L = G.state().unit(B.leader);
	const sov::Rules& R = G.rules();
	FSovBattleSpec Spec;
	const sov::PlayerId DefOwner = D ? D->owner : C->owner;
	const sov::Hex Where = D ? D->pos : C->pos;
	if (D)
	{
		Spec.Attacker = {Str(R.units[static_cast<size_t>(A->type)].name), A->owner, G.combatStrength(*A, *D, true, false), A->hp, G.isLeader(*A)};
		Spec.Defender = {Str(R.units[static_cast<size_t>(D->type)].name), D->owner, G.combatStrength(*D, *A, false, false), D->hp, G.isLeader(*D)};
	}
	else
	{
		Spec.Attacker = {Str(R.units[static_cast<size_t>(A->type)].name), A->owner, G.combatStrengthVsCity(*A, *C, true, false), A->hp, G.isLeader(*A)};
		Spec.Defender = {Str(C->name), C->owner, G.cityStrength(*C), FMath::Clamp(C->hp, 1, 100), false};
	}
	Spec.HumanSide = A->owner == Me() ? 0 : 1;
	Spec.bLeaderPresent = L != nullptr;
	if (L)
	{
		if (D)
		{
			Spec.LeaderStrength = G.combatStrength(*L, Spec.HumanSide == 0 ? *D : *A, Spec.HumanSide == 0, false);
		}
		else
		{
			// Storming: the leader against the city; holding it: the city's own strength.
			Spec.LeaderStrength = Spec.HumanSide == 0 ? G.combatStrengthVsCity(*L, *C, true, false) : G.cityStrength(*C);
		}
		Spec.LeaderHp = L->hp;
	}
	Spec.Seed = G.state().turn * 7919 + B.attacker;
	Spec.TimeLimit = 180.f;
	// The enemy adapts to how this human has fought before (leader doc §10, player modelling; King and up).
	if (const sov::PlayerProfile* Prof = G.profile(Me()); Prof && Prof->battles > 0 && G.difficulty().aiSkill >= 4)
	{
		Spec.Counter.holdFlanks = Prof->battleFlank >= 300;
		Spec.Counter.huntLeader = Prof->battleLeaderFront >= 400;
		Spec.Counter.pursue = Prof->battleFallBack >= 250;
	}
	BattlePeer = sov::kNoPlayer;
	SnapshotTimer = 0.f;
	if (bRemoteView)
	{
		// Another machine runs this battle: we see its snapshots and command our own squads.
		Sim.StartRemoteView(Spec);
		BattlePeer = B.liveFor;
		Sub->GetSessionMut().SendRelay(BattlePeer, {2});
	}
	else
	{
		Sim.Start(Spec, FSovBattleSim::TrainedPolicy());
		if (PendingJoin != sov::kNoPlayer && PendingJoin == BattleOpponent())
		{
			BattlePeer = PendingJoin;
			Sim.SetRemoteEnemy(true);
		}
	}
	PendingJoin = sov::kNoPlayer;
	BattleSquad = -1;
	Outcome = FSovBattleResult();
	bBattleSent = false;

	bool bWoods = false;
	const FLinearColor Ground = SovPlotColor(G, Where.x, Where.y, &bWoods);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Battle = GetWorld()->SpawnActor<ASovBattleScene>(ASovBattleScene::Origin(), FRotator::ZeroRotator, Params);
	Battle->Build(Spec, Ground, bWoods && !C, SovPlayerColor(G, A->owner), SovPlayerColor(G, DefOwner), C != nullptr, C && C->wallHp > 0);
	Battle->Sync(Sim);
	const FVector2D Start = Sim.LeaderIndex() != INDEX_NONE ? Sim.Soldiers()[Sim.LeaderIndex()].Pos : FVector2D(Spec.HumanSide == 0 ? -2300.f : 2300.f, 0.f);
	Walker = GetWorld()->SpawnActor<ASovWalker>(Battle->ToWorld(Start, 90.0), FRotator::ZeroRotator, Params);
	Walker->SetColor(SovPlayerColor(G, Me()));
	Walker->GetCharacterMovement()->DisableMovement();  // the simulation moves the leader
	MapPawn = GetPawn();
	Possess(Walker);
	SetControlRotation(FRotator(-20.f, Spec.HumanSide == 0 ? 0.f : 180.f, 0.f));
	Chooser = EChooser::None;
	Sub->LastMessage = bRemoteView
		? TEXT("You command your men in a battle your rival fights live: Tab charge/hold, 1-6 squad orders (7 8 9 pick a squad), Esc leave them to your generals.")
		: TEXT("To battle! WASD move, left click or F strike, Tab charge/hold, 1-6 squad orders (7 8 9 pick a squad), Esc settle now.");
}

void ASovPlayerController::UpdateBattle(float DeltaTime)
{
	LookAround(DeltaTime);
	HandleBattleRelays();
	if (Sim.RemoteView())
	{
		USovGameSubsystem* Sub = Subsystem();
		FSovSession& Session = Sub->GetSessionMut();
		const int32 Side = Sim.GetSpec().HumanSide;
		auto SendOrder = [&](int32 Squad, sov::battle::Order Order) {
			Session.SendRelay(BattlePeer, {3, static_cast<uint8_t>(static_cast<int8>(Squad)), static_cast<uint8_t>(Order)});
		};
		if (WasInputKeyJustPressed(EKeys::Tab)) SendOrder(-1, Sim.Charging(Side) ? sov::battle::Order::Hold : sov::battle::Order::Advance);
		const FKey PickKeys[] = {EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
		for (int32 k = 0; k < 4; ++k)
		{
			if (WasInputKeyJustPressed(PickKeys[k])) BattleSquad = k < 3 ? k : -1;
		}
		const FKey OrderKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
		for (int32 k = 0; k < sov::battle::kOrders; ++k)
		{
			if (WasInputKeyJustPressed(OrderKeys[k])) SendOrder(BattleSquad, static_cast<sov::battle::Order>(k));
		}
		Battle->Sync(Sim);
		// The battle ends when its result arrives in the game; Esc hands our men back to the AI.
		if (!Sub->GetGame().battlePending() || WasInputKeyJustPressed(EKeys::Escape))
		{
			if (Sub->GetGame().battlePending()) Session.SendRelay(BattlePeer, {4});
			Sub->LastMessage = Sub->GetGame().battlePending() ? TEXT("Your generals lead your men.") : TEXT("The battle is over.");
			ExitBattle();
		}
		return;
	}
	if (bBattleSent)
	{
		BattleExitTimer -= DeltaTime;
		if (BattleExitTimer <= 0.f)
		{
			ExitBattle();
		}
		return;
	}
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	const FVector F3 = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), R3 = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	FVector2D Move(0, 0);
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Move += FVector2D(F3);
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Move -= FVector2D(F3);
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Move += FVector2D(R3);
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Move -= FVector2D(R3);
	const bool bStrike = WasInputKeyJustPressed(EKeys::LeftMouseButton) || WasInputKeyJustPressed(EKeys::F);
	const int32 Side = Sim.GetSpec().HumanSide;
	if (WasInputKeyJustPressed(EKeys::Tab)) Sim.SetCharge(Side, !Sim.Charging(Side));
	// Squad orders: 7 8 9 pick the left, centre or right squad (0: all), 1 to 6 give the order.
	const FKey PickKeys[] = {EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
	for (int32 k = 0; k < 4; ++k)
	{
		if (WasInputKeyJustPressed(PickKeys[k])) BattleSquad = k < 3 ? k : -1;
	}
	const FKey OrderKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
	for (int32 k = 0; k < sov::battle::kOrders; ++k)
	{
		if (WasInputKeyJustPressed(OrderKeys[k])) Sim.SetOrder(Side, static_cast<sov::battle::Order>(k), BattleSquad);
	}
	const bool bSettleNow = WasInputKeyJustPressed(EKeys::Escape);
	Sim.Step(DeltaTime, Move.GetSafeNormal(), bStrike);
	Battle->Sync(Sim);
	// Online: the other side's player sees the field ten times a second.
	SnapshotTimer += DeltaTime;
	if (BattlePeer != sov::kNoPlayer && (SnapshotTimer >= 0.1f || Sim.Finished()))
	{
		SnapshotTimer = 0.f;
		std::vector<uint8_t> Msg = {1};
		const std::vector<uint8_t> Body = Sim.Snapshot().Encode();
		Msg.insert(Msg.end(), Body.begin(), Body.end());
		Subsystem()->GetSessionMut().SendRelay(BattlePeer, Msg);
	}
	if (Sim.LeaderIndex() != INDEX_NONE)
	{
		const FSovSoldier& Me3 = Sim.Soldiers()[Sim.LeaderIndex()];
		Walker->SetActorLocation(Battle->ToWorld(Me3.Pos, 90.0));
		if (!Move.IsNearlyZero()) Walker->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Move.Y, Move.X)), 0.f));
		Walker->Body->SetVisibility(Me3.bAlive);
		Walker->Crown->SetVisibility(Me3.bAlive && !Walker->bFigure);
	}
	if (Sim.Finished() || bSettleNow)
	{
		// One result command into the game; the core clamps it to the band (§9).
		Outcome = Sim.Result();
		Send(sov::Command::battleResult(Me(), Outcome.ToDefender, Outcome.ToAttacker, Outcome.LeaderWound, Outcome.Habits));
		bBattleSent = true;
		BattleExitTimer = 3.f;
	}
}

void ASovPlayerController::ExitBattle()
{
	if (MapPawn)
	{
		Possess(MapPawn);
	}
	if (Walker)
	{
		Walker->Destroy();
	}
	if (Battle)
	{
		Battle->Destroy();
	}
	Walker = nullptr;
	Battle = nullptr;
}

void ASovPlayerController::EnterStreet()
{
	USovGameSubsystem* Sub = Subsystem();
	const sov::Game& G = Sub->GetGame();
	const sov::Unit* Leader = G.leaderOf(Me());
	const sov::City* City = Leader ? G.state().cityAt(Leader->pos) : nullptr;
	if (!City || City->owner != Me())
	{
		Sub->LastMessage = TEXT("The leader must stand in one of your cities to walk its streets.");
		return;
	}
	// The game autosaves when a live scene starts; there is no saving inside one (engine doc).
	Sub->SaveGame(TEXT("autosave"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Street = GetWorld()->SpawnActor<ASovStreetScene>(ASovStreetScene::Origin(), FRotator::ZeroRotator, Params);
	Street->Build(BuildStreetLayout(G, City->id));
	Walker = GetWorld()->SpawnActor<ASovWalker>(Street->ToWorld(Street->GetLayout().Entry + FVector(0, 0, 120)), FRotator::ZeroRotator, Params);
	Walker->SetColor(SovPlayerColor(G, Me()));
	MapPawn = GetPawn();
	Possess(Walker);
	SetControlRotation(FRotator(-15.f, 0.f, 0.f));
	Chooser = EChooser::None;
	Sub->LastMessage = FString::Printf(TEXT("You walk into %s. WASD to walk, hold right mouse to look, F to talk, Esc to return."),
		*Street->GetLayout().CityName);
}

void ASovPlayerController::ExitStreet()
{
	if (MapPawn)
	{
		Possess(MapPawn);
	}
	if (Walker)
	{
		Walker->Destroy();
	}
	if (Street)
	{
		Street->Destroy();
	}
	Walker = nullptr;
	Street = nullptr;
	Subsystem()->LastMessage.Reset();
}

FString ASovPlayerController::StreetPrompt() const
{
	if (!Street || !Walker)
	{
		return FString();
	}
	const FVector At = Walker->GetActorLocation();
	const FSovStreetLayout& L = Street->GetLayout();
	if (FVector::Dist2D(At, Street->ToWorld(L.Herald)) < 450.0)
	{
		return TEXT("F: hear petitions and give alms (Benevolence)");
	}
	if (FVector::Dist2D(At, Street->ToWorld(L.Captain)) < 450.0)
	{
		return TEXT("F: order a show of force (Fear)");
	}
	return FString();
}

void ASovPlayerController::UpdateStreet(float DeltaTime)
{
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		ExitStreet();
		return;
	}
	// Look around: the control rotation is set directly (input-axis rotation is consumed before this runs).
	FRotator Look = GetControlRotation();
	if (IsInputKeyDown(EKeys::RightMouseButton))
	{
		float DX = 0.f, DY = 0.f;
		GetInputMouseDelta(DX, DY);
		Look.Yaw += DX * 2.5f;
		Look.Pitch = FMath::ClampAngle(Look.Pitch + DY * 2.5f, -60.f, 20.f);
	}
	if (IsInputKeyDown(EKeys::Q)) Look.Yaw -= 90.f * DeltaTime;
	if (IsInputKeyDown(EKeys::E)) Look.Yaw += 90.f * DeltaTime;
	SetControlRotation(Look);
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Walker->AddMovementInput(Forward);
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Walker->AddMovementInput(-Forward);
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Walker->AddMovementInput(Right);
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Walker->AddMovementInput(-Right);
	if (WasInputKeyJustPressed(EKeys::F))
	{
		const FSovStreetLayout& L = Street->GetLayout();
		const FVector At = Walker->GetActorLocation();
		const bool bHerald = FVector::Dist2D(At, Street->ToWorld(L.Herald)) < 450.0;
		const bool bCaptain = FVector::Dist2D(At, Street->ToWorld(L.Captain)) < 450.0;
		if (bHerald || bCaptain)
		{
			const sov::Stance St = bHerald ? sov::Stance::Benevolence : sov::Stance::Fear;
			if (Send(sov::Command::cityStance(Me(), L.CityId, St)))
			{
				// Rebuild the street so the new mood shows (banners or guards).
				const FVector Keep = Walker->GetActorLocation() - Street->GetActorLocation();
				Street->Destroy();
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Street = GetWorld()->SpawnActor<ASovStreetScene>(ASovStreetScene::Origin(), FRotator::ZeroRotator, Params);
				Street->Build(BuildStreetLayout(Subsystem()->GetGame(), L.CityId));
				Walker->SetActorLocation(Street->GetActorLocation() + Keep);
				Subsystem()->LastMessage = St == sov::Stance::Benevolence ? TEXT("The petitioners bless your name. (+amenities)")
																		  : TEXT("The guard parades through the square. Order holds. (+loyalty)");
			}
		}
	}
}

void ASovPlayerController::HandleOrders()
{
	// A live battle waits for us, whoever's turn it is (leader doc §9).
	const sov::Game& Gm = Subsystem()->GetGame();
	if (Gm.battlePending())
	{
		if (Gm.state().pendingBattle.liveFor == Me())
		{
			if (WasInputKeyJustPressed(EKeys::R)) Send(sov::Command::autoResolveBattle(Me()));
			else if (WasInputKeyJustPressed(EKeys::B)) StartBattle();
		}
		else if (BattleOpponent() == Me())
		{
			// Online: the other side's player may command their men in the live battle, or settle it.
			if (WasInputKeyJustPressed(EKeys::B)) StartBattle(true);
			else if (WasInputKeyJustPressed(EKeys::R)) Send(sov::Command::autoResolveBattle(Me()));
		}
		return;
	}
	int32 X = 0, Y = 0;
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && HexUnderCursor(X, Y))
	{
		ClickSelect(X, Y);
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (Chooser != EChooser::None) Chooser = EChooser::None;
		else SelectedUnit = SelectedCity = -1;
	}
	if (Chooser != EChooser::None)
	{
		for (int32 i = 0; i < PageSize; ++i)
		{
			if (WasInputKeyJustPressed(DigitKeys[i]))
			{
				Pick(i);
				return;
			}
		}
		if (WasInputKeyJustPressed(EKeys::Zero))
		{
			const int32 Pages = (Choices.Num() + PageSize - 1) / PageSize;
			ChooserPage = Pages > 0 ? (ChooserPage + 1) % Pages : 0;
		}
	}
	if (!MyTurn())
	{
		return;
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && HexUnderCursor(X, Y))
	{
		ClickOrder(X, Y);
	}
	if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter))
	{
		EndTurn();
	}
	if (WasInputKeyJustPressed(EKeys::Period)) SelectNextUnit();
	if (WasInputKeyJustPressed(EKeys::P)) OpenChooser(EChooser::Production);
	if (WasInputKeyJustPressed(EKeys::T)) OpenChooser(EChooser::Research);
	if (WasInputKeyJustPressed(EKeys::C)) OpenChooser(EChooser::Civic);
	if (WasInputKeyJustPressed(EKeys::H)) OpenChooser(EChooser::Throne);
	if (WasInputKeyJustPressed(EKeys::J)) OpenChooser(EChooser::Assassins);
	if (WasInputKeyJustPressed(EKeys::Y)) OpenChooser(EChooser::GreatPeople);
	if (WasInputKeyJustPressed(EKeys::O)) OpenChooser(EChooser::CityStates);
	if (WasInputKeyJustPressed(EKeys::N)) OpenChooser(EChooser::Diplomacy);
	if (WasInputKeyJustPressed(EKeys::F2)) OpenChooser(EChooser::Government);
	if (WasInputKeyJustPressed(EKeys::Z)) OpenChooser(EChooser::Governors);
	if (WasInputKeyJustPressed(EKeys::Comma)) OpenChooser(EChooser::Congress);
	if (WasInputKeyJustPressed(EKeys::I) && Subsystem()->GetGame().state().players[static_cast<size_t>(Me())].pantheon == sov::kNone)
		OpenChooser(EChooser::Pantheon);
	// Citizen stances in the selected city where the leader stands (classic control's panel, leader doc §4).
	if (SelectedCity >= 0 && (WasInputKeyJustPressed(EKeys::V) || WasInputKeyJustPressed(EKeys::X)))
	{
		const sov::Stance St = WasInputKeyJustPressed(EKeys::V) ? sov::Stance::Benevolence : sov::Stance::Fear;
		if (Send(sov::Command::cityStance(Me(), SelectedCity, St)))
		{
			Subsystem()->LastMessage = St == sov::Stance::Benevolence ? TEXT("Petitions heard, alms given: the city warms to you.")
																	  : TEXT("Order is imposed. The city obeys, for now.");
		}
	}

	const sov::Game& G = Subsystem()->GetGame();
	const sov::Unit* U = G.state().unit(SelectedUnit);
	if (!U || U->owner != Me())
	{
		return;
	}
	const sov::UnitType& T = G.rules().units[static_cast<size_t>(U->type)];
	const sov::Hex Pos = U->pos;
	if (WasInputKeyJustPressed(EKeys::F) && T.id == "UNIT_TRADER")
	{
		ReligionUnit = U->id;
		OpenChooser(EChooser::TradeRoute);
		return;
	}
	if (WasInputKeyJustPressed(EKeys::F) && T.foundReligion)
	{
		// A Great Prophet founds a religion on a Holy Site (06).
		ReligionUnit = U->id;
		OpenChooser(EChooser::ReligionFounder);
		return;
	}
	if (WasInputKeyJustPressed(EKeys::F) && U->religion >= 0)
	{
		// Religious units: Apostles may add a belief; everyone spreads where they stand.
		ReligionUnit = U->id;
		bool bCanEvangelize = false;
		for (int32 Cls = static_cast<int32>(sov::BeliefClass::Follower); Cls < sov::kNumBeliefClasses && !bCanEvangelize; ++Cls)
		{
			for (sov::TypeIndex B : G.availableBeliefs(static_cast<sov::BeliefClass>(Cls)))
			{
				bCanEvangelize |= G.canEvangelize(U->id, B);
			}
		}
		if (bCanEvangelize)
		{
			OpenChooser(EChooser::Evangelize);
		}
		else if (Send(sov::Command::spreadReligion(Me(), U->id)))
		{
			Subsystem()->LastMessage = TEXT("The faith is preached in the city.");
		}
		return;
	}
	if (U->greatPerson != sov::kNone && WasInputKeyJustPressed(EKeys::F))
	{
		// A great person is used where it stands (07): an effect, or a Great Work in a free slot.
		const FString Who = Str(G.rules().greatPeople[static_cast<size_t>(U->greatPerson)].name);
		if (Send(sov::Command::activateGreatPerson(Me(), U->id)))
		{
			Subsystem()->LastMessage = FString::Printf(TEXT("%s has left a mark on your civilization."), *Who);
		}
		return;
	}
	if (WasInputKeyJustPressed(EKeys::F) && Send(sov::Command::foundCity(Me(), U->id)))
	{
		// The new city needs something to build first.
		if (const sov::City* City = G.state().cityAt(Pos))
		{
			SelectCity(City->id, false);
			OpenChooser(EChooser::Production);
		}
		return;
	}
	if (WasInputKeyJustPressed(EKeys::K) && Send(sov::Command::setActivity(Me(), U->id, sov::Activity::Skip)))
	{
		AfterUnitOrder();
		return;
	}
	if (WasInputKeyJustPressed(EKeys::G))
	{
		const sov::Activity A = T.layer == sov::UnitLayer::Military ? sov::Activity::Fortify : sov::Activity::Sleep;
		if (Send(sov::Command::setActivity(Me(), U->id, A)))
		{
			AfterUnitOrder();
		}
		return;
	}
	if (WasInputKeyJustPressed(EKeys::B)) OpenChooser(EChooser::Improvement);
	if (WasInputKeyJustPressed(EKeys::E) && G.isLeader(*U)) OpenChooser(EChooser::Gear);
	if (WasInputKeyJustPressed(EKeys::U)) OpenChooser(EChooser::Promotion);
	if (WasInputKeyJustPressed(EKeys::Q) && G.isLeader(*U)) EnterStreet();
	if (WasInputKeyJustPressed(EKeys::L))
	{
		// Link the leader and the military unit on its plot, or end the link.
		const sov::Unit* Leader = G.isLeader(*U) ? U : G.state().unitAt(U->pos, sov::UnitLayer::Leader, G.rules());
		const sov::Unit* Guard = G.isLeader(*U) ? G.state().unitAt(U->pos, sov::UnitLayer::Military, G.rules()) : U;
		if (Leader && Guard && Leader->owner == Me() && Guard->owner == Me())
		{
			const bool bLinked = G.escortOf(*Leader) && G.escortOf(*Leader)->id == Guard->id;
			Send(sov::Command::linkEscort(Me(), Guard->id, bLinked ? -1 : Leader->id));
			Subsystem()->LastMessage = bLinked ? TEXT("Escort released.") : TEXT("Escort linked: it moves with the leader.");
		}
		else
		{
			Subsystem()->LastMessage = TEXT("Link needs your leader and a military unit on the same plot.");
		}
	}
}

void ASovPlayerController::UpdatePanel()
{
	ASovHUD* Hud = Cast<ASovHUD>(GetHUD());
	if (!Hud)
	{
		return;
	}
	TArray<FString>& L = Hud->PanelLines;
	L.Reset();
	const sov::Game& G = Subsystem()->GetGame();
	const sov::Rules& R = G.rules();
	const sov::GameState& S = G.state();
	if (const sov::Unit* U = S.unit(SelectedUnit))
	{
		const sov::UnitType& T = R.units[static_cast<size_t>(U->type)];
		const FString Name = G.isLeader(*U)                     ? Str(S.players[static_cast<size_t>(U->owner)].leaderName)
							 : U->greatPerson != sov::kNone ? FString::Printf(TEXT("%s, %s"), *Str(R.greatPeople[static_cast<size_t>(U->greatPerson)].name), *Str(T.name))
															: Str(T.name);
		FString Line = FString::Printf(TEXT("%s   HP %d   Moves %s/%d"), *Name, U->hp, *Str(U->movesLeft.toString()), G.maxMoves(*U));
		if (G.meleeStrength(*U) > 0) Line += FString::Printf(TEXT("   Strength %d"), G.meleeStrength(*U));
		if (G.rangedStrength(*U) > 0) Line += FString::Printf(TEXT("   Ranged %d (range %d)"), G.rangedStrength(*U), G.unitRange(*U));
		if (const sov::Unit* E = G.isLeader(*U) ? G.escortOf(*U) : nullptr) Line += FString::Printf(TEXT("   Escort: %s"), *Str(R.units[static_cast<size_t>(E->type)].name));
		if (T.buildCharges > 0) Line += FString::Printf(TEXT("   Charges %d"), U->charges);
		if (!T.promotionClass.empty()) Line += FString::Printf(TEXT("   Level %d (XP %d/%d)"), U->level(), U->xp, G.xpForNextLevel(*U));
		L.Add(Line);
		FString Keys = TEXT("Right-click: move/attack   K skip   G fortify/sleep");
		if (!G.availablePromotions(U->id).empty()) Keys += TEXT("   U promote");
		if (T.foundCity) Keys += TEXT("   F found city");
		if (T.foundReligion) Keys += TEXT("   F found a religion (on a Holy Site)");
		if (T.id == "UNIT_TRADER") Keys += FString::Printf(TEXT("   F start a trade route (%d of %d in use)"), G.tradeRoutesOf(Me()), G.tradeRouteCapacity(Me()));
		if (U->religion >= 0)
		{
			Keys += FString::Printf(TEXT("   %s, %d spread%s   F spread%s   right-click a foe's religious unit: theological combat"),
				*Str(R.religions[static_cast<size_t>(S.religions[static_cast<size_t>(U->religion)].type)].name), U->charges, U->charges == 1 ? TEXT("") : TEXT("s"),
				R.units[static_cast<size_t>(U->type)].id == "UNIT_APOSTLE" ? TEXT(" or add a belief") : TEXT(""));
		}
		if (U->greatPerson != sov::kNone)
		{
			const sov::GreatPersonType& Gp = R.greatPeople[static_cast<size_t>(U->greatPerson)];
			FString Use = Gp.greatWorkCount > 0 ? FString::Printf(TEXT("creates a Great Work in a city with a free slot (%d left)"), U->charges)
				: Gp.effects.empty()             ? FString(TEXT("its gift needs systems still to come"))
				: Gp.district != sov::kNone      ? FString::Printf(TEXT("use on a %s"), *Str(R.districts[static_cast<size_t>(Gp.district)].name))
												 : FString(TEXT("use where it stands"));
			Keys += FString::Printf(TEXT("   F use (%s)%s"), *Use, G.canActivateGreatPerson(U->id) ? TEXT(", ready") : TEXT(""));
		}
		if (G.isLeader(*U)) Keys += TEXT("   E gear   L link escort   Q walk the streets");
		else if (T.layer == sov::UnitLayer::Military && G.state().unitAt(U->pos, sov::UnitLayer::Leader, R)) Keys += TEXT("   L escort the leader");
		if (T.buildCharges > 0) Keys += TEXT("   B build");
		L.Add(Keys);
	}
	else if (const sov::City* C = S.city(SelectedCity))
	{
		const sov::CityReport Rep = G.cityReport(C->id);
		auto Y = [&](sov::YieldType T) { return Str(Rep.yields[static_cast<size_t>(T)].toString()); };
		L.Add(FString::Printf(TEXT("%s   Pop %d   HP %d/%d   Food %s  Prod %s  Gold %s  Sci %s  Cul %s"), *Str(C->name), C->population, C->hp,
			G.cityMaxHp(), *Y(sov::YieldType::Food), *Y(sov::YieldType::Production), *Y(sov::YieldType::Gold), *Y(sov::YieldType::Science),
			*Y(sov::YieldType::Culture)));
		const sov::LoyaltyLevel* Level = G.loyaltyLevel(*C);
		L.Add(FString::Printf(TEXT("Loyalty %d (%+d per turn)%s%s"), C->loyalty, static_cast<int32>(G.loyaltyPerTurn(C->id).round()),
			Level ? *FString::Printf(TEXT("   %s"), *Str(Level->id)) : TEXT(""),
			// Power [GS] (09: Power): what the city's buildings need and what reaches it.
			C->powerDemand > 0 || C->powerSupply > 0
				? *FString::Printf(TEXT("   Power %d/%d%s"), C->powerSupply, C->powerDemand, C->powerSupply < C->powerDemand ? TEXT(" (short)") : TEXT(""))
				: TEXT("")));
		// Specialists (02): citizens working district slots.
		{
			FString Spec;
			for (const sov::CityDistrict& D : C->districts)
			{
				if (D.specialists > 0) Spec += FString::Printf(TEXT("   %s %d/%d"), *Str(R.districts[static_cast<size_t>(D.type)].name), D.specialists, G.specialistSlots(*C, D));
			}
			if (!Spec.IsEmpty()) L.Add(TEXT("Specialists:") + Spec);
		}
		// Religion here (06): the majority, and every faith with followers.
		{
			const int32 Maj = G.cityMajorityReligion(*C);
			FString Rel = FString::Printf(TEXT("Religion: %s"), Maj < 0 ? TEXT("none") : *Str(R.religions[static_cast<size_t>(S.religions[static_cast<size_t>(Maj)].type)].name));
			for (size_t r = 0; r < S.religions.size(); ++r)
			{
				const int32 F = G.cityFollowers(*C, static_cast<int32>(r));
				if (F > 0)
				{
					Rel += FString::Printf(TEXT("   %s %d"), *Str(R.religions[static_cast<size_t>(S.religions[r].type)].name), F);
				}
			}
			L.Add(Rel);
		}
		FString Queue = TEXT("Building: ");
		Queue += C->queue.empty() ? FString(TEXT("nothing")) : ItemName(R, C->queue.front());
		L.Add(Queue + TEXT("   P choose production   right-click: city strike"));
		const sov::Unit* Here = G.leaderOf(Me());
		if (Here && Here->pos == C->pos)
		{
			const int32 Wait = rules_cooldown(G, *C);
			L.Add(Wait > 0 ? FString::Printf(TEXT("Stances: ready in %d turn(s)"), Wait)
						   : FString::Printf(TEXT("V Benevolence (%d gold, +amenities)   X Fear (needs a garrison: +loyalty, resentment later)"),
								 G.benevolenceCost(*C)));
		}
		if (G.state().turn < C->benevolenceUntil) L.Add(FString::Printf(TEXT("Benevolence: %d more turn(s)"), C->benevolenceUntil - G.state().turn));
		if (G.fearActive(*C)) L.Add(FString::Printf(TEXT("Fear: order for %d more turn(s)"), C->fearUntil - G.state().turn));
		else if (G.state().turn < C->fearAfterUntil) L.Add(FString::Printf(TEXT("Resentment after Fear: %d more turn(s)"), C->fearAfterUntil - G.state().turn));
	}
	if (Chooser != EChooser::None)
	{
		const int32 Pages = (Choices.Num() + PageSize - 1) / PageSize;
		L.Add(FString::Printf(TEXT("%s%s  (1-9 pick, Esc close)"), *ChooserTitle,
			Pages > 1 ? *FString::Printf(TEXT(" page %d/%d, 0 next"), ChooserPage + 1, Pages) : TEXT("")));
		for (int32 i = 0; i < PageSize; ++i)
		{
			const int32 I = ChooserPage * PageSize + i;
			if (Choices.IsValidIndex(I))
			{
				L.Add(FString::Printf(TEXT("  %d. %s"), i + 1, *Choices[I].Label));
			}
		}
	}
	if (G.battlePending() && G.state().pendingBattle.liveFor != Me() && BattleOpponent() == Me())
	{
		L.Add(FString::Printf(TEXT("BATTLE! %s fights your army live. B: take command of your men   R: settle it by the numbers   (otherwise your generals lead them)"),
			UTF8_TO_TCHAR(S.players[static_cast<size_t>(G.state().pendingBattle.liveFor)].leaderName.c_str())));
	}
	if (G.battlePending() && G.state().pendingBattle.liveFor == Me())
	{
		const sov::PendingBattle& Bt = G.state().pendingBattle;
		const sov::Unit* A = S.unit(Bt.attacker);
		const sov::Unit* D = S.unit(Bt.defender);
		const sov::City* C = S.city(Bt.city);
		L.Add(C ? TEXT("BATTLE! A city is stormed, and your leader is in the fight.") : TEXT("BATTLE! Your leader's stack is in a melee."));
		if (A && C)
		{
			L.Add(FString::Printf(TEXT("%s storms %s. Expected: %d damage to the city, %d to the attacker (the field can shift it 25%%)."),
				*Str(R.units[static_cast<size_t>(A->type)].name), *Str(C->name), Bt.expectedToDefender, Bt.expectedToAttacker));
		}
		if (A && D)
		{
			L.Add(FString::Printf(TEXT("%s attacks %s. Expected: %d damage to the defender, %d to the attacker (the field can shift it 25%%)."),
				*Str(R.units[static_cast<size_t>(A->type)].name), *Str(R.units[static_cast<size_t>(D->type)].name), Bt.expectedToDefender,
				Bt.expectedToAttacker));
		}
		L.Add(TEXT("B fight it live   R auto-resolve"));
		return;
	}
	if (MyTurn())
	{
		const size_t Waiting = G.unitsNeedingOrders(Me()).size();
		L.Add(FString::Printf(TEXT("Your turn. %d unit(s) need orders.   Space end turn   . next unit   T research   C civics   Y great people   O city-states   N diplomacy   F2 government   Z governors   I pantheon   J assassins   WASD/wheel camera"),
			static_cast<int32>(Waiting)));
	}
}

void ASovPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (InBattle())
	{
		UpdateBattle(DeltaTime);
		if (ASovHUD* Hud = Cast<ASovHUD>(GetHUD()))
		{
			Hud->PanelLines.Reset();
		}
		return;
	}
	if (InStreet())
	{
		UpdateStreet(DeltaTime);
		if (ASovHUD* Hud = Cast<ASovHUD>(GetHUD()))
		{
			Hud->PanelLines.Reset();
		}
		return;
	}
	if (!InBattle())
	{
		HandleBattleRelays();
	}
	if (InDiplomacy())
	{
		UpdateDiplomacy();
		if (ASovHUD* Hud = Cast<ASovHUD>(GetHUD()))
		{
			Hud->PanelLines.Reset();
		}
		return;
	}
	UpdateCamera(DeltaTime);
	if (HandleSessionScreens())
	{
		if (ASovHUD* Hud = Cast<ASovHUD>(GetHUD()))
		{
			Hud->PanelLines.Reset();
		}
		return;
	}
	USovGameSubsystem* Sub = Subsystem();
	if (!Sub || !Sub->IsRunning())
	{
		return;
	}
	if (!bCenteredOnGame)
	{
		bCenteredOnGame = true;
		CenterOnHome();
	}
	const sov::GameState& S = Sub->GetGame().state();
	if (SelectedUnit >= 0 && (!S.unit(SelectedUnit) || S.unit(SelectedUnit)->owner != Me())) SelectedUnit = -1;
	if (SelectedCity >= 0 && (!S.city(SelectedCity) || S.city(SelectedCity)->owner != Me())) SelectedCity = -1;
	// A new turn starts with the first unit that needs orders selected.
	const bool bMyTurn = MyTurn();
	if (bMyTurn && !bWasMyTurn && Chooser == EChooser::None)
	{
		SelectNextUnit();
	}
	bWasMyTurn = bMyTurn;
	HandleOrders();
	if (Map)
	{
		const sov::GameState& Now = Sub->GetGame().state();
		if (const sov::Unit* U = Now.unit(SelectedUnit)) Map->SetHighlight(U->pos.x, U->pos.y);
		else if (const sov::City* C = Now.city(SelectedCity)) Map->SetHighlight(C->pos.x, C->pos.y);
		else Map->SetHighlight(-1, -1);
	}
	UpdatePanel();
}

// ---------------------------------------------------------------- diplomacy

const sov::Deal* ASovPlayerController::OfferFrom(sov::PlayerId Leader) const
{
	for (const sov::Deal& D : Subsystem()->GetGame().state().deals)
	{
		if (D.from == Leader && D.to == Me())
		{
			return &D;
		}
	}
	return nullptr;
}

void ASovPlayerController::OpenDiplomacy(sov::PlayerId Leader)
{
	if (InDiplomacy() || !MyTurn())
	{
		return;
	}
	int32 Port = 8080;
	FParse::Value(FCommandLine::Get(), TEXT("SovLlmPort="), Port);
	const sov::Game& G = Subsystem()->GetGame();
	Talk = MakeUnique<FSovDiplomacyTalk>(G, Leader, Me(), Port);
	bLeavingTalk = false;
	if (const sov::Deal* D = OfferFrom(Leader))
	{
		Talk->AddNote(FString::Printf(TEXT("They have put an offer to you: %s."), *Str(sov::describeDeal(G.rules(), G.state(), *D))));
	}
	FSovDiplomacyTalk* T = Talk.Get();
	DiplomacyPanel = SNew(SSovDiplomacyPanel)
		.Talk(T)
		.Header([this, T]() {
			const sov::diplomacy::Persona& P = T->GetPersona();
			// Their agenda shows from Open access (08: Access level).
			const int32 Access = Subsystem()->GetGame().accessLevel(Me(), T->Leader());
			const FString Agenda = Access >= 2 ? FString::Printf(TEXT("Agenda, %s: %s"), *Str(P.agendaName), *Str(P.agendaText))
											   : FString(TEXT("Agenda: unknown (needs Open access: Printing, a delegation, a trade route, an alliance or a spy)"));
			return FText::FromString(FString::Printf(TEXT("%s of %s, a %s. %s toward you (opinion %+d). Access: %s.\n%s"), *Str(P.leaderName),
				*Str(P.civName), *Str(P.leaning), UTF8_TO_TCHAR(sov::relationshipName(P.relationship)), P.opinion,
				UTF8_TO_TCHAR(sov::Game::accessName(Access)), *Agenda));
		})
		.Reasons([T]() {
			const sov::diplomacy::Persona& P = T->GetPersona();
			FString Text = TEXT("Why they feel this way:\n");
			for (const std::string& R : P.reasons) Text += TEXT("  ") + Str(R) + TEXT("\n");
			if (P.reasons.empty()) Text += TEXT("  Nothing in particular yet.\n");
			Text += TEXT("\nThey could offer:\n");
			for (const std::string& O : P.leaderOffers) Text += TEXT("  ") + Str(O) + TEXT("\n");
			Text += TEXT("\nYou could offer:\n");
			for (const std::string& O : P.playerOffers) Text += TEXT("  ") + Str(O) + TEXT("\n");
			if (!P.pastTalks.empty())
			{
				Text += TEXT("\nPast talks:\n");
				for (const std::string& O : P.pastTalks) Text += TEXT("  ") + Str(O) + TEXT("\n");
			}
			return FText::FromString(Text);
		})
		.Proposal([this, T]() {
			const sov::diplomacy::Exchange* E = T->LastExchange();
			if (!E || E->verdict == sov::diplomacy::Verdict::None) return FText::GetEmpty();
			const sov::Game& Gm = Subsystem()->GetGame();
			if (E->verdict == sov::diplomacy::Verdict::Invalid) return FText::FromString(TEXT("What you asked for cannot be traded now."));
			if (E->proposal.items.empty()) return FText::GetEmpty();
			const TCHAR* Verdict = E->verdict == sov::diplomacy::Verdict::Accept ? TEXT("they would accept") : TEXT("they would refuse");
			return FText::FromString(FString::Printf(TEXT("Your proposal: %s (%s)"), *Str(sov::describeDeal(Gm.rules(), Gm.state(), E->proposal)), Verdict));
		})
		.TheirOffer([this, Leader]() {
			const sov::Deal* D = OfferFrom(Leader);
			if (!D) return FText::GetEmpty();
			const sov::Game& Gm = Subsystem()->GetGame();
			return FText::FromString(FString::Printf(TEXT("Their offer: %s"), *Str(sov::describeDeal(Gm.rules(), Gm.state(), *D))));
		})
		.CanPropose([T]() {
			const sov::diplomacy::Exchange* E = T->LastExchange();
			return !T->IsBusy() && E && !E->proposal.items.empty();
		})
		.CanDenounce([this, T]() { return !T->IsBusy() && Subsystem()->GetGame().canDenounce(Me(), T->Leader()); })
		.OnSay([T](const FString& Words) { T->Say(Words); })
		.OnPropose([this, T]() {
			const sov::diplomacy::Exchange* E = T->LastExchange();
			if (!E || E->proposal.items.empty()) return;
			if (Send(sov::diplomacy::Conversation::proposalCommand(*E)))
			{
				// An AI answers within the command: the newest deal event says how.
				const auto& Events = Subsystem()->GetGame().state().events;
				const bool bMade = !Events.empty() && Events.back().kind == sov::EventKind::DealAccepted;
				T->AddNote(bMade ? TEXT("[The deal is made.]") : TEXT("[They turned the deal down.]"));
			}
			T->ClearProposal();
			T->Refresh();
		})
		.OnAnswerOffer([this, T, Leader](bool bAccept) {
			if (const sov::Deal* D = OfferFrom(Leader))
			{
				if (Send(sov::Command::answerDeal(Me(), D->id, bAccept)))
				{
					T->AddNote(bAccept ? TEXT("[You accepted their offer.]") : TEXT("[You turned their offer down.]"));
				}
			}
			T->Refresh();
		})
		.OnDenounce([this, T]() {
			if (Send(sov::Command::denounce(Me(), T->Leader())))
			{
				T->AddNote(TEXT("[You denounced them before the world.]"));
			}
			T->Refresh();
		})
		.OnLeave([this]() { CloseDiplomacy(); });
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(DiplomacyPanel.ToSharedRef(), 50);
	}
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(DiplomacyPanel->GetInput());
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void ASovPlayerController::UpdateDiplomacy()
{
	Talk->Poll();
	sov::Command Summary;
	if (bLeavingTalk && Talk->SummaryReady(Summary))
	{
		// The leader's memory of this talk, recorded like any other command (leader doc §10, Sync).
		if (!Summary.text.empty())
		{
			Send(Summary);
		}
		if (GEngine && GEngine->GameViewport && DiplomacyPanel.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(DiplomacyPanel.ToSharedRef());
		}
		DiplomacyPanel.Reset();
		Talk.Reset();
		bLeavingTalk = false;
		FInputModeGameAndUI Mode;  // as at BeginPlay
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
		return;
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		CloseDiplomacy();
	}
}

void ASovPlayerController::CloseDiplomacy()
{
	if (!Talk || bLeavingTalk || Talk->IsBusy())
	{
		return;
	}
	bLeavingTalk = true;
	Talk->AddNote(TEXT("[The talk ends.]"));
	Talk->Finish();
}

// ---------------------------------------------------------------- online and hot seat

bool ASovPlayerController::HandleSessionScreens()
{
	USovGameSubsystem* Sub = Subsystem();
	if (!Sub)
	{
		return false;
	}
	FSovSession& Session = Sub->GetSessionMut();
	if (Menu.IsValid())
	{
		return true;
	}
	if (ChatBox.IsValid())
	{
		if (WasInputKeyJustPressed(EKeys::Escape)) CloseChat();
		return true;  // typing: the map takes no keys
	}
	if (Session.NetMode() != ESovNet::Local && WasInputKeyJustPressed(EKeys::M))
	{
		OpenChat();
		return true;
	}
	if (Session.InLobby())
	{
		if (Session.UsesSteam() && Session.NetMode() == ESovNet::Host && WasInputKeyJustPressed(EKeys::F))
		{
			Session.InviteFriends();
		}
		if (Session.NetMode() == ESovNet::Host && (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)))
		{
			FString Error;
			if (!Session.StartHostedGame(Error))
			{
				Sub->LastMessage = FString::Printf(TEXT("Could not start: %s"), *Error);
			}
			Sub->OnStateChanged.Broadcast();
		}
		return true;
	}
	if (Session.HandoverPending())
	{
		// Hot seat: the next human presses Enter when the screen is theirs.
		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar))
		{
			Session.TakeOver();
			SelectedUnit = SelectedCity = -1;
			Chooser = EChooser::None;
			Sub->OnStateChanged.Broadcast();
			CenterOnHome();
		}
		return true;
	}
	return false;
}

void ASovPlayerController::OpenChat()
{
	if (ChatBox.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TSharedPtr<SEditableTextBox> Box;
	ChatBox = SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(16.f, 0.f, 0.f, 260.f))
		[
			SNew(SBox).WidthOverride(560.f)
			[
				SAssignNew(Box, SEditableTextBox)
				.HintText(FText::FromString(TEXT("Say to everyone, then Enter (Esc closes)")))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How) {
					if (How == ETextCommit::OnEnter && !Text.IsEmpty())
					{
						if (USovGameSubsystem* S = Subsystem()) S->GetSessionMut().Chat(Text.ToString());
					}
					CloseChat();
				})
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(ChatBox.ToSharedRef(), 40);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Box);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void ASovPlayerController::CloseChat()
{
	if (!ChatBox.IsValid())
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ChatBox.ToSharedRef());
	}
	ChatBox.Reset();
	FInputModeGameAndUI Mode;  // as at BeginPlay
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

// ---------------------------------------------------------------- the main menu

void ASovPlayerController::OpenMenu()
{
	if (Menu.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TSharedPtr<SEditableTextBox> Address;
	auto Item = [this](const FString& Label, TFunction<void()> Click) {
		return SNew(SBox).Padding(FMargin(0.f, 4.f)).WidthOverride(420.f)[
			SNew(SButton).HAlign(HAlign_Center).Text(FText::FromString(Label)).OnClicked_Lambda([Click]() {
				Click();
				return FReply::Handled();
			})];
	};
	const FString Name = FPlatformProcess::UserName();
	auto Base = [this, Name]() {
		FSovSetup S;
		S.PlayerName = Name.IsEmpty() ? FString(TEXT("Player")) : Name;
		S.Difficulty = MenuDifficulty;
		return S;
	};
	static const TCHAR* const Levels[] = {TEXT("Settler"), TEXT("Chieftain"), TEXT("Warlord"), TEXT("Prince"), TEXT("King"), TEXT("Emperor"), TEXT("Immortal"), TEXT("Deity")};
	Menu = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.95f)).Padding(24.f)[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f)[
				SNew(STextBlock).Text(FText::FromString(TEXT("Sovereign"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("<"))).OnClicked_Lambda([this]() {
					MenuDifficulty = FMath::Max(0, MenuDifficulty - 1);
					return FReply::Handled();
				})]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)[
					SNew(STextBlock).Text_Lambda([this]() {
						return FText::FromString(FString::Printf(TEXT("Difficulty: %s"), Levels[FMath::Clamp(MenuDifficulty, 0, 7)]));
					})]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT(">"))).OnClicked_Lambda([this]() {
					MenuDifficulty = FMath::Min(7, MenuDifficulty + 1);
					return FReply::Handled();
				})]]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Single player"), [this, Base]() { StartFromMenu(Base()); })]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Hot seat (two players, one screen)"), [this, Base]() {
				FSovSetup S = Base();
				S.HumanSeats = 2;
				StartFromMenu(S);
			})]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Host a game on your network"), [this, Base]() {
				FSovSetup S = Base();
				S.Net = ESovNet::Host;
				S.HumanSeats = 2;
				StartFromMenu(S);
			})]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[SAssignNew(Address, SEditableTextBox).Text(FText::FromString(TEXT("127.0.0.1")))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 0.f, 0.f)[
					SNew(SButton).Text(FText::FromString(TEXT("Join by address"))).OnClicked_Lambda([this, Base, Address]() {
						FSovSetup S = Base();
						S.Net = ESovNet::Join;
						S.JoinAddress = Address->GetText().ToString().TrimStartAndEnd();
						StartFromMenu(S);
						return FReply::Handled();
					})]]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Host a game for Steam friends"), [this, Base]() {
				FSovSetup S = Base();
				S.Net = ESovNet::Host;
				S.HumanSeats = 2;
				S.bSteam = true;
				StartFromMenu(S);
			})]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Join a Steam friend (accept their invite)"), [this, Base]() {
				FSovSetup S = Base();
				S.Net = ESovNet::Join;
				S.bSteam = true;
				StartFromMenu(S);
			})]
			+ SVerticalBox::Slot().AutoHeight()[Item(TEXT("Quit"), [this]() { ConsoleCommand(TEXT("quit")); })]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 0.f)[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(1.f, 0.5f, 0.5f))
				.Text_Lambda([this]() {
					const USovGameSubsystem* S = Subsystem();
					return FText::FromString(S ? S->LastMessage : FString());
				})]
		]];
	GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 60);
}

void ASovPlayerController::CloseMenu()
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
}

void ASovPlayerController::StartFromMenu(const FSovSetup& Setup)
{
	USovGameSubsystem* Sub = Subsystem();
	if (!Sub)
	{
		return;
	}
	if (Sub->StartGame(Setup))
	{
		CloseMenu();
		bCenteredOnGame = false;
	}
}
