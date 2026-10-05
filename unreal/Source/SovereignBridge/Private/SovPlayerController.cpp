#include "SovPlayerController.h"

#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"

#include "SovCameraPawn.h"
#include "SovGameSubsystem.h"
#include "SovHUD.h"
#include "SovHexLayout.h"
#include "SovMapActor.h"
#include "SovStreetScene.h"

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
		case sov::ProductionKind::Unit: return Str(R.units[T].name);
		case sov::ProductionKind::Building: return Str(R.buildings[T].name);
		case sov::ProductionKind::District: return Str(R.districts[T].name);
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
		Send(sov::Command::cityStrike(Me(), SelectedCity, Target));
		return;
	}
	const sov::Unit* U = S.unit(SelectedUnit);
	if (!U)
	{
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
				const int32 Cost = Item.kind == sov::ProductionKind::District ? G.districtCost(Me(), Item.type) : G.productionCost(Me(), Item);
				Choices.Add({FString::Printf(TEXT("%s (%d turns)%s"), *ItemName(R, Item), TurnsFor(Cost, PerTurn), *Where),
					sov::Command::setProduction(Me(), City->id, Item, Plot)});
			}
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
		case EChooser::Assassins:
		{
			ChooserTitle = FString::Printf(TEXT("Assassins (%d of %d; one per Encampment)"), G.agentsOf(Me()), G.agentCapacity(Me()));
			for (const sov::Agent& A : G.state().agents)
			{
				if (A.owner != Me())
				{
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
					if (T.id == Me() || !T.alive || T.barbarian)
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
		case EChooser::Promotion:
		{
			const sov::Unit* U = G.state().unit(SelectedUnit);
			if (!U)
			{
				break;
			}
			ChooserTitle = TEXT("Promotion (heals 50 and ends the unit's turn)");
			for (sov::TypeIndex Pr : G.availablePromotions(U->id))
			{
				const sov::PromotionType& T = R.promotions[static_cast<size_t>(Pr)];
				const FString Branch = T.branch.empty() ? FString() : FString::Printf(TEXT(" [%s]"), *Str(T.branch));
				Choices.Add({FString::Printf(TEXT("%s%s"), *Str(T.name), *Branch), sov::Command::promote(Me(), U->id, Pr)});
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
				Choices.Add({Str(R.improvements[static_cast<size_t>(T)].name), sov::Command::buildImprovement(Me(), U->id, T)});
			}
			if (G.canHarvestAt(Me(), U->pos))
			{
				Choices.Add({TEXT("Harvest"), sov::Command::harvest(Me(), U->id)});
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
	if (Send(Command) && Was == EChooser::Improvement)
	{
		AfterUnitOrder();
	}
}

// ------------------------------------------------------------------ street scenes

void ASovPlayerController::StartBattle()
{
	// The live battle scene arrives with battle milestone 2; until then the fight is auto-resolved.
	Subsystem()->LastMessage = TEXT("Live battle scenes are not built yet: auto-resolving.");
	Send(sov::Command::autoResolveBattle(Me()));
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
		const FString Name = G.isLeader(*U) ? Str(S.players[static_cast<size_t>(U->owner)].leaderName) : Str(T.name);
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
		L.Add(FString::Printf(TEXT("Loyalty %d (%+d per turn)%s"), C->loyalty, static_cast<int32>(G.loyaltyPerTurn(C->id).round()),
			Level ? *FString::Printf(TEXT("   %s"), *Str(Level->id)) : TEXT("")));
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
	if (G.battlePending() && G.state().pendingBattle.liveFor == Me())
	{
		const sov::PendingBattle& Bt = G.state().pendingBattle;
		const sov::Unit* A = S.unit(Bt.attacker);
		const sov::Unit* D = S.unit(Bt.defender);
		L.Add(TEXT("BATTLE! Your leader's stack is in a melee."));
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
		L.Add(FString::Printf(TEXT("Your turn. %d unit(s) need orders.   Space end turn   . next unit   T research   C civics   J assassins   WASD/wheel camera"),
			static_cast<int32>(Waiting)));
	}
}

void ASovPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (InStreet())
	{
		UpdateStreet(DeltaTime);
		if (ASovHUD* Hud = Cast<ASovHUD>(GetHUD()))
		{
			Hud->PanelLines.Reset();
		}
		return;
	}
	UpdateCamera(DeltaTime);
	USovGameSubsystem* Sub = Subsystem();
	if (!Sub || !Sub->IsRunning())
	{
		return;
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
