#include "SovBattleScene.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#include "SovArt.h"
#include "SovAudio.h"
#include "SovLocomotion.h"

namespace
{
// The fighting field's half extents (the simulation keeps the armies inside them).
constexpr double FieldX = 4500.0, FieldY = 3000.0;

// Smooth value noise in about 0..1 from a few sines (as the street scene's).
double Noise(double X, double Y, double Scale)
{
	const double U = X / Scale, V = Y / Scale;
	return 0.5 + 0.25 * FMath::Sin(U * 1.7 + FMath::Cos(V * 0.9)) * FMath::Cos(V * 1.3 - 0.7) + 0.15 * FMath::Sin(U * 3.1 + V * 2.3)
		+ 0.1 * FMath::Cos(U * 5.3 - V * 4.1);
}

// A linear colour as the bytes M_SovKit squares back, and its detail cell in alpha.
FColor KitPaint(const FLinearColor& Linear, int32 Cell)
{
	auto Byte = [](float V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Sqrt(FMath::Max(V, 0.f)) * 255.f), 0, 255)); };
	return FColor(Byte(Linear.R), Byte(Linear.G), Byte(Linear.B), static_cast<uint8>(FMath::RoundToInt(Cell * 255.f / (SovArt::KitCells - 1))));
}

// The ground's height: flat across the field, rising beyond it to hills.
double FieldZ(double X, double Y)
{
	const double D = FMath::Max(FMath::Abs(X) / FieldX, FMath::Abs(Y) / FieldY);
	const double T = FMath::Clamp((D - 1.05) / 1.6, 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T) * (1300.0 + 2000.0 * Noise(X, Y, 3800.0));
}

// A rigged man walks above this speed (cm/s); below kFightingPace one who has a foe in reach faces him (standing, or
// edging about in the press), above it the way he goes.
constexpr float kWalking = 25.f;
constexpr float kFightingPace = 100.f;
// A man going faster than kFightingPace and striking more than this far (degrees) off the way he walks is giving ground
// (falling back): his blow is not shown, as facing it he would walk backward (the blend has no backward gait). Slower,
// he faces his foe whichever way he edges, so it is shown.
constexpr float kStrikeAway = 100.f;
// Each sound of a fight plays at most once in this many seconds.
constexpr double kSoundGap = 0.2;

// The yaw (degrees) that turns a rigged figure at From toward To: a man is modelled facing +Y, a horseman along X
// (Turn 90), as the walking heading in Sync.
float YawToward(const FVector2D& From, const FVector2D& To, float Turn)
{
	const FVector2D D = To - From;
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))) - 90.f + Turn;
}

// The sound of a figure's blow: a musket or rifle fires and a bow looses (at the strike, as its blow would land);
// anything else clashes as the blow lands.
ESovBattleSound BlowSound(const FString& Figure)
{
	if (Figure.StartsWith(TEXT("Musketeer")) || Figure.StartsWith(TEXT("Rifleman"))) return ESovBattleSound::Gunshot;
	if (Figure.StartsWith(TEXT("Archer")) || Figure == TEXT("Rider_Asian")) return ESovBattleSound::Bow;
	return ESovBattleSound::Clash;
}

// A man lying on his Death clip reaches about his height from where he stood, past the standing mesh's bounds, which
// do not follow the pose (the rigs have no physics asset): his bounds grow so he is not culled while he is on screen.
// The old roll turned the component, and its bounds with it.
void CoverFall(USkeletalMeshComponent* Rig)
{
	const USkeletalMesh* Mesh = Rig ? Rig->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh) return;
	const FVector Extent = Mesh->GetBounds().BoxExtent;
	const double Across = FMath::Max(FMath::Min(Extent.X, Extent.Y), 1.0);
	const float Scale = static_cast<float>(FMath::Clamp(2.0 * Extent.Z / Across, 1.0, 10.0));
	if (Rig->BoundsScale < Scale) Rig->SetBoundsScale(Scale);
}
}  // namespace

ASovBattleScene::ASovBattleScene()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	BaseMaterial = Material.Object;
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	ConeMesh = Cone.Object;
}

UMaterialInstanceDynamic* ASovBattleScene::MaterialFor(const FLinearColor& Color)
{
	const uint32 Key = Color.ToFColor(false).ToPackedRGBA();
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Materials.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Materials.Add(Key, Mid);
	return Mid;
}

UStaticMeshComponent* ASovBattleScene::Add(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Location);
	C->SetRelativeScale3D(Scale);
	C->SetMaterial(0, MaterialFor(Color));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	return C;
}

void ASovBattleScene::Build(const FSovBattleSpec& Spec, const FLinearColor& Ground, bool bWoods, const FLinearColor& AttackerColor,
	const FLinearColor& DefenderColor, bool bCity, bool bWalls)
{
	SideColor[0] = AttackerColor;
	SideColor[1] = DefenderColor;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FName F = Side == 0 ? Spec.Attacker.Figure : Spec.Defender.Figure;
		// A figure in its culture's dress (SovCultureFigure) is Soldier_, Archer_ or Rider_ and its style; without that
		// mesh it fights in the plain figure.
		FString Base, Style;
		const bool bDressed = F.ToString().Split(TEXT("_"), &Base, &Style);
		const FName Plain = bDressed ? FName(*Base) : F;
		const bool bOnFoot = Plain == FName("Archer") || Plain == FName("Musketeer") || Plain == FName("Rifleman") || Plain == FName("Rider")
			|| Plain == FName("Soldier");
		SideFigure[Side] = !bOnFoot ? FString(TEXT("Soldier"))
			: SovArt::Mesh(TEXT("Figures"), F.ToString()) ? F.ToString()
			: SovArt::Mesh(TEXT("Figures"), Plain.ToString()) ? Plain.ToString() : FString(TEXT("Soldier"));
		SideYaw[Side] = SideFigure[Side].StartsWith(TEXT("Rider")) ? 90.f : 0.f;
		// Siege engines and tanks fight among their crews: catapult hands as spearmen, tank riders as riflemen.
		const bool bMachine = (F == FName("Siege") || F == FName("Tank")) && SovArt::Mesh(TEXT("Figures"), F.ToString());
		SideVehicle[Side] = bMachine ? F.ToString() : FString();
		if (bMachine && F == FName("Tank") && SovArt::Mesh(TEXT("Figures"), TEXT("Rifleman")))
		{
			SideFigure[Side] = TEXT("Rifleman");
		}
	}
	// The field and its hills when the art exists; else a slab.
	if (!BuildField(Ground, Spec.Seed))
	{
		UStaticMeshComponent* Floor = Add(CubeMesh, FVector(0, 0, -50), FVector(90.0, 60.0, 1.0), Ground);
		Floor->SetCollisionProfileName(TEXT("BlockAll"));
	}
	FRandomStream Rng(Spec.Seed + 101);
	if (bWoods)
	{
		for (int32 i = 0; i < 60; ++i)
		{
			const FVector At(Rng.FRandRange(-4400.f, 4400.f), Rng.FRandRange(-2900.f, 2900.f), 0.f);
			if (FMath::Abs(At.Y) < 900.f)
			{
				continue;  // the fighting lane stays open
			}
			UStaticMeshComponent* Trunk = Add(CylinderMesh, At + FVector(0, 0, 120), FVector(0.25, 0.25, 2.4), FLinearColor(0.3f, 0.2f, 0.12f));
			if (SovArt::SetKitMesh(Trunk, TEXT("Nature"), Rng.FRand() < 0.5f ? TEXT("Tree_Broadleaf") : TEXT("Tree_Conifer"), FLinearColor::White))
			{
				Trunk->SetRelativeLocationAndRotation(At, FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
				Trunk->SetRelativeScale3D(FVector(Rng.FRandRange(0.85f, 1.2f)));
				continue;
			}
			Add(ConeMesh, At + FVector(0, 0, 420), FVector(1.6, 1.6, 3.2), FLinearColor(0.12f, 0.32f, 0.12f));
		}
	}
	if (bCity)
	{
		// The city behind the defenders: wall sections (10 m kit pieces) either side of an open gate,
		// or the outer houses of an unwalled town.
		const TCHAR* Houses[] = {TEXT("House_A"), TEXT("House_B"), TEXT("House_C")};
		int32 h = 0;
		for (const float Y : {-2600.f, -1600.f, 1600.f, 2600.f})
		{
			const FVector At(3400.f, Y, 0.f);
			const TCHAR* Name = bWalls ? TEXT("Wall") : Houses[h++ % 3];
			UStaticMeshComponent* Piece = Add(CubeMesh, At + FVector(0, 0, bWalls ? 250 : 200), bWalls ? FVector(1.6, 10.0, 5.0) : FVector(5.0, 5.0, 4.0),
				bWalls ? FLinearColor(0.5f, 0.48f, 0.45f) : FLinearColor(0.7f, 0.62f, 0.5f));
			if (SovArt::SetKitMesh(Piece, TEXT("Classical"), Name, DefenderColor))
			{
				Piece->SetRelativeLocationAndRotation(At, FRotator(0.f, 90.f, 0.f));
				Piece->SetRelativeScale3D(FVector(1.0));
			}
		}
	}
	// Banners mark each side's start.
	Add(CylinderMesh, FVector(-2600, 900, 300), FVector(0.1, 0.1, 6.0), AttackerColor);
	Add(CylinderMesh, FVector(2600, 900, 300), FVector(0.1, 0.1, 6.0), DefenderColor);
}

void ASovBattleScene::Sync(const FSovBattleSim& Sim)
{
	const TArray<FSovSoldier>& Men = Sim.Soldiers();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	while (Bodies.Num() < Men.Num())
	{
		const int32 i = Bodies.Num();
		const FSovSoldier& S = Men[i];
		const FLinearColor C = SideColor[S.Side];
		UStaticMeshComponent* Body = Add(CylinderMesh.Get(), FVector::ZeroVector, S.bLeader ? FVector(0.6, 0.6, 1.7) : FVector(0.45, 0.45, 1.5), C);
		UStaticMeshComponent* Head = Add(S.bLeader ? SphereMesh.Get() : ConeMesh.Get(), FVector::ZeroVector, S.bLeader ? FVector(0.35) : FVector(0.3, 0.3, 0.45),
			S.bLeader ? FLinearColor(1.f, 0.75f, 0.1f) : FLinearColor(0.6f, 0.6f, 0.62f));
		// A machine for every third man of a squad on a siege or armoured side (the first of each three).
		const bool bMachine = !S.bLeader && !SideVehicle[S.Side].IsEmpty() && S.Squad >= 0 && S.Squad < 3
			&& SquadMen[S.Side][S.Squad]++ % 3 == 0;
		// Kit figures when the art exists: soldiers (and leaders) in their side's colour.
		const FString Name = S.bLeader ? FString(TEXT("Leader")) : bMachine ? SideVehicle[S.Side] : SideFigure[S.Side];
		if (SovArt::SetKitMesh(Body, TEXT("Figures"), Name, C))
		{
			Body->SetRelativeScale3D(FVector(1.0));
			Head->SetVisibility(false);
			Figure.Add(true);
		}
		else
		{
			Figure.Add(false);
		}
		Vehicle.Add(bMachine && Figure.Last());
		// A man of a plain arm (spearman, archer, musketeer, rifleman) walks on its rigged figure's legs when that art
		// exists; the still figure stays hidden then. Horsemen ride on their own skeleton (rig_riders.py).
		USkeletalMeshComponent* Rig = nullptr;
		if (!S.bLeader && !bMachine && SovArt::RiggedMesh(Name))
		{
			Rig = NewObject<USkeletalMeshComponent>(this);
			Rig->SetupAttachment(RootComponent);
			Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Rig->RegisterComponent();
			if (SovArt::SetRiggedFigure(Rig, Name, C, TEXT("Idle")))
			{
				Rig->SetPosition(FMath::FRand() * 3.f);  // not all breathing in step
				USovLocomotion::Attach(Rig, Name, 1.f, /*bActions*/ true);  // idle, walk and run blended by speed; strikes, hits, falls
				Body->SetVisibility(false);
			}
			else
			{
				Rig->DestroyComponent();
				Rig = nullptr;
			}
		}
		Rigs.Add(Rig);
		RigNames.Add(Name);
		LastPos.Add(S.Pos);
		MovedAt.Add(Now);
		MoveSpeed.Add(0.f);
		Striding.Add(false);
		Yaws.Add((S.Side == 0 ? -90.f : 90.f) + (Name.StartsWith(TEXT("Rider")) ? 90.f : 0.f));
		MoveYaw.Add(Yaws.Last());
		StrikeAt.Add(INDEX_NONE);
		StrikeEnds.Add(0.0);
		Bodies.Add(Body);
		Heads.Add(Head);
	}
	const double Dt = FMath::Max(Now - LastSyncTime, 1e-3);
	LastSyncTime = Now;
	// Who strikes, who is hit and who falls (sov::battle::Show: each swing where the battle runs here, inferred from a
	// replay's or the online view's snapshots), played on the men's rigs and never fed back to the battle. Once the
	// battle is over and its last frame has been read (a replay holds it for 2 s, the online view until the result
	// comes), men still face to face strike no more.
	if (!bShownOver)
	{
		SovSeen(Men, SeenMen);
		Cues.clear();
		Shows.update(SeenMen, Now, Cues);
		for (const sov::battle::Cue& Cue : Cues) PlayCue(Sim, Cue, Now);
		bShownOver = Sim.Finished();
	}
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		const FSovSoldier& S = Men[i];
		if (USkeletalMeshComponent* Rig = Rigs[i])
		{
			// A horseman is modelled along X (rig_riders.py), a man facing +Y (rig_figures.py); each walks at its own pace.
			const bool bHorse = RigNames[i].StartsWith(TEXT("Rider"));
			const float Turn = bHorse ? 90.f : 0.f;
			const float Pace = bHorse ? kSovHorseWalkSpeed : kSovProceduralWalkSpeed;  // the walk's own speed (SovGait.h), cm/s
			const float Face = (S.Side == 0 ? -90.f : 90.f) + Turn;
			// Speed and heading from the man's own moves, not from the frames: a replay or the online view (Sim.RemoteView)
			// moves the men ten times a second while this runs every frame; the battle fought here moves them every frame,
			// so there the move is the frame's step, as before. A snapshot's move spans at least the 0.1 s between
			// snapshots, even when one held up on the way lands a frame before the next (no burst of speed), and a man not
			// moved since keeps his speed and heading a quarter second, a little over two snapshots, then stands; here he
			// stands at once. Setting off from a stand, his move is the last snapshot's (or frame's), not his whole wait.
			const bool bSnapshots = Sim.RemoteView();
			const double Floor = bSnapshots ? FMath::Max(Dt, 0.1) : Dt;
			const FVector2D Step = S.Pos - LastPos[i];
			if (!Step.IsNearlyZero(0.01))
			{
				const double Since = MoveSpeed[i] > 0.f ? FMath::Clamp(Now - MovedAt[i], Floor, FMath::Max(Floor, 0.25)) : Floor;
				MoveSpeed[i] = static_cast<float>(Step.Size() / Since);
				MoveYaw[i] = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Step.Y, Step.X))) - 90.f + Turn;
				LastPos[i] = S.Pos;
				MovedAt[i] = Now;
			}
			else if (Now - MovedAt[i] > (bSnapshots ? 0.25 : 0.0))
			{
				MoveSpeed[i] = 0.f;
			}
			const float Speed = MoveSpeed[i];
			const bool bWalk = S.bAlive && Speed > kWalking;
			USovLocomotion* Loco = Cast<USovLocomotion>(Rig->GetAnimInstance());
			if (S.bAlive && (Rig->bPauseAnims || (Loco && Loco->Asked(ESovAction::Death))))
			{
				// Up again: nothing rewinds a battle today, but a scene shown an earlier snapshot must not leave him frozen
				// or lying.
				Rig->bPauseAnims = false;
				Rig->bNoSkeletonUpdate = false;
				Rig->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
				Striding[i] = !bWalk;  // without the blend, his clip starts again below
				if (Loco) Loco->Clear();
				if (Rig->BoundsScale != 1.f) Rig->SetBoundsScale(1.f);
			}
			if (Loco)
			{
				Loco->Speed = bWalk ? Speed : 0.f;  // the blend eases between standing, walking and running
			}
			else if (bWalk != Striding[i] && S.bAlive)
			{
				Striding[i] = bWalk;
				if (UAnimSequence* A = SovArt::RiggedAnim(RigNames[i], bWalk ? TEXT("Walk") : TEXT("Idle"))) Rig->PlayAnimation(A, true);
			}
			// Facing, as the sim faces its men: toward whom his strike is at while it plays; else toward whom he fights
			// while he stands or edges about in the press; else the way he walks; else the enemy's side. Down where he fell.
			const int32 Striking = StrikingAt(i);
			const int32 Foe = Striking != INDEX_NONE ? Striking : Speed < kFightingPace ? Target(i) : INDEX_NONE;
			const float Yaw = Foe != INDEX_NONE ? YawToward(S.Pos, Men[Foe].Pos, Turn) : bWalk ? MoveYaw[i] : Face;
			if (S.bAlive)
			{
				// Turning over a moment (a man about 360 degrees a second, a horse half that), not at a snap.
				Yaws[i] = FMath::FixedTurn(Yaws[i], Yaw, static_cast<float>(Dt) * (bHorse ? 180.f : 360.f));
				Rig->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 0), FRotator(0.f, Yaws[i], 0.f));
				if (bWalk && !Loco) Rig->SetPlayRate(FMath::Clamp(Speed / Pace, 0.5f, 3.f));
			}
			else if (Loco && Loco->Has(ESovAction::Death))
			{
				// He falls on his own clip where he stood, facing as he did (a fall backward lies with his feet toward his
				// killer), when the blow that killed him lands; once his bones hold him lying (Fallen) he costs nothing: no
				// pose ticked or refreshed. While he falls his bones are refreshed on or off camera, so a man killed behind
				// the camera lies down too and then stops ticking, rather than tick until seen and stand for a frame.
				if (!Rig->bPauseAnims)
				{
					if (!Loco->Asked(ESovAction::Death)) Loco->Play(ESovAction::Death, 0.f, 1.f, (i & 1) != 0);
					Rig->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					CoverFall(Rig);
					Rig->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 0), FRotator(0.f, Yaws[i], 0.f));
					if (Loco->Fallen())
					{
						Rig->bPauseAnims = true;
						Rig->bNoSkeletonUpdate = true;
					}
				}
			}
			else if (!Rig->bPauseAnims)
			{
				// Without a Death clip (a rig imported before the one-shots), the old fall: the fallen lie still where they
				// fell, rolled over (a horse goes down on its side), and then cost nothing: no pose ticked or refreshed.
				// Stop is for a single clip; the blend has none to stop.
				Rig->bPauseAnims = true;
				if (!Loco) Rig->Stop();
				Rig->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, bHorse ? 30 : 15), bHorse ? FRotator(0.f, Face, 80.f) : FRotator(0.f, Face, 90.f));
				Rig->bNoSkeletonUpdate = true;
			}
			continue;
		}
		if (S.bLeader && i == Sim.LeaderIndex())
		{
			// The human's leader is drawn by its walker, which also falls and lies where he fell when it has a Death clip
			// (else, dead, he is hidden, as before: the walker hides its figure, and his still figure here stays hidden).
			USovLocomotion* Loco = LocomotionOf(Sim, i);
			if (S.bAlive || (Loco && Loco->Has(ESovAction::Death)))
			{
				Bodies[i]->SetVisibility(false);
				Heads[i]->SetVisibility(false);
				if (!S.bAlive && LeaderRig.IsValid() && !LeaderRig->bPauseAnims)
				{
					if (!Loco->Asked(ESovAction::Death)) Loco->Play(ESovAction::Death, 0.f, 1.f, false);
					CoverFall(LeaderRig.Get());
					if (Loco->Fallen())
					{
						LeaderRig->bPauseAnims = true;
						LeaderRig->bNoSkeletonUpdate = true;
					}
				}
				continue;
			}
		}
		// Figures face the enemy's side of the field (kit figures face +Y in Unreal; riders and machines lie along X).
		const float Facing = (S.Side == 0 ? -90.f : 90.f) + (S.bLeader ? 0.f : Vehicle[i] ? 90.f : SideYaw[S.Side]);
		if (S.bAlive)
		{
			if (Figure[i])
			{
				Bodies[i]->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 0), FRotator(0.f, Facing, 0.f));
				continue;
			}
			Bodies[i]->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 75), FRotator::ZeroRotator);
			Heads[i]->SetRelativeLocation(FVector(S.Pos.X, S.Pos.Y, 175));
		}
		else if (Vehicle[i])
		{
			// A wrecked machine stays where it stopped, burnt dark and slewed a little.
			if (!WreckMaterial)
			{
				WreckMaterial = SovArt::Tinted(this, FLinearColor(0.18f, 0.17f, 0.16f));
			}
			if (WreckMaterial && Bodies[i]->GetMaterial(0) != WreckMaterial)
			{
				for (int32 Slot = 0; Slot < Bodies[i]->GetNumMaterials(); ++Slot) Bodies[i]->SetMaterial(Slot, WreckMaterial);
			}
			Bodies[i]->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 0), FRotator(0.f, Facing + 20.f, 6.f));
		}
		else if (Figure[i])
		{
			// The fallen lie where they fell.
			Bodies[i]->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 15), FRotator(0.f, Facing, 90.f));
		}
		else
		{
			Bodies[i]->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 22), FRotator(90.f, 0.f, 0.f));
			Bodies[i]->SetMaterial(0, MaterialFor(SideColor[S.Side] * 0.35f));
			Heads[i]->SetVisibility(false);
		}
	}
}

void ASovBattleScene::SetLeaderRig(USkeletalMeshComponent* Rig)
{
	LeaderRig = Rig;
}

int32 ASovBattleScene::Target(int32 Man) const
{
	const int32 Foe = Shows.target(Man);
	return Foe >= 0 ? Foe : INDEX_NONE;
}

int32 ASovBattleScene::StrikingAt(int32 Man) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return StrikeAt.IsValidIndex(Man) && Now < StrikeEnds[Man] ? StrikeAt[Man] : INDEX_NONE;
}

USovLocomotion* ASovBattleScene::LocomotionOf(const FSovBattleSim& Sim, int32 Man) const
{
	// The ruler fighting here is drawn by his walker; a man drawn still (a leader, a machine, missing art) has none.
	USkeletalMeshComponent* Rig = Man == Sim.LeaderIndex() ? LeaderRig.Get() : Rigs.IsValidIndex(Man) ? Rigs[Man].Get() : nullptr;
	return Rig ? Cast<USovLocomotion>(Rig->GetAnimInstance()) : nullptr;
}

void ASovBattleScene::PlayCue(const FSovBattleSim& Sim, const sov::battle::Cue& Cue, double Now)
{
	const TArray<FSovSoldier>& Men = Sim.Soldiers();
	const int32 i = Cue.man;
	if (!Men.IsValidIndex(i)) return;
	USovLocomotion* Loco = LocomotionOf(Sim, i);
	switch (Cue.kind)
	{
		case sov::battle::CueKind::Strike:
		{
			if (!Loco || !Loco->Has(ESovAction::Strike)) return;
			// A man giving ground strikes back as he walks away, facing the way he goes: his blow is not shown, though its
			// victim's hit still waits for it.
			if (Men.IsValidIndex(Cue.other) && Rigs[i] && MoveSpeed[i] >= kFightingPace)
			{
				const float Turn = RigNames[i].StartsWith(TEXT("Rider")) ? 90.f : 0.f;
				const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(MoveYaw[i], YawToward(Men[i].Pos, Men[Cue.other].Pos, Turn)));
				if (Off > kStrikeAway) return;
			}
			Loco->Play(ESovAction::Strike);
			StrikeAt[i] = Men.IsValidIndex(Cue.other) ? Cue.other : INDEX_NONE;
			StrikeEnds[i] = Now + kSovStrikeLength;
			// A musket or rifle fires, and a bow looses, as the blow would land.
			const ESovBattleSound Shot = BlowSound(RigNames[i]);
			if (Shot != ESovBattleSound::Clash) PlaySound(Shot, kSovStrikeImpact);
			return;
		}
		case sov::battle::CueKind::Hit:
			if (Loco) Loco->Play(ESovAction::Hit, Cue.delay, Cue.weight);
			break;
		case sov::battle::CueKind::Fall:
			if (Loco) Loco->Play(ESovAction::Death, Cue.delay, 1.f, Cue.backward);
			break;
		case sov::battle::CueKind::Lie:
			if (Loco) Loco->Play(ESovAction::Death, 0.f, 1.f, Cue.backward, /*bAtEnd*/ true);
			return;
	}
	// A blow that lands clashes as it lands; a miss is silent. A shot or an arrow whose strike is shown was heard as it
	// was fired or loosed (above), so it does not clash too.
	if (Cue.other < 0) return;
	const bool bShot = RigNames.IsValidIndex(Cue.other) && BlowSound(RigNames[Cue.other]) != ESovBattleSound::Clash
		&& StrikingAt(Cue.other) != INDEX_NONE;
	if (!bShot) PlaySound(ESovBattleSound::Clash, Cue.delay);
}

void ASovBattleScene::PlaySound(ESovBattleSound Kind, float Delay)
{
	if (Delay <= 0.f)
	{
		SoundNow(Kind);  // a timer of 0 would be cleared, not fired
		return;
	}
	// A weak timer: it still fires in the moments after the result, when nothing calls Sync, and not after the scene goes.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Kind]() { SoundNow(Kind); }), Delay, false);
}

void ASovBattleScene::SoundNow(ESovBattleSound Kind)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	double& Last = SoundAt[static_cast<int32>(Kind)];
	if (Now - Last < kSoundGap) return;
	Last = Now;
	static const TCHAR* const Names[] = {TEXT("SFX_Clash"), TEXT("SFX_Bow"), TEXT("SFX_Gunshot")};
	FSovAudio::Play(this, Names[static_cast<int32>(Kind)]);
}

bool ASovBattleScene::BuildField(const FLinearColor& Ground, int32 Seed)
{
	UMaterialInterface* Kit = SovArt::KitMaterial();
	if (!Kit) return false;
	// Sand on a desert (the brightest ground), else grass in the plot's own colour, deeper than the map draws it (light,
	// to read from above) and drawn toward a meadow's green so plains do not glare yellow at ground level.
	const bool bDesert = Ground.GetLuminance() > 0.38f;
	const int32 Cell = bDesert ? SovArt::Cell::Sand : SovArt::Cell::Grass;
	const FLinearColor Base = bDesert ? Ground * 0.6f : FMath::Lerp(Ground * 0.55f, FLinearColor(0.1f, 0.18f, 0.05f), 0.4f);
	const FLinearColor Sere(0.26f, 0.24f, 0.12f);
	// A grid finer on the field than in the hills: x = Extent * sign(u) * |u|^1.6.
	const int32 N = 170;
	const double Extent = FieldX * 3.4;
	TArray<double> Axis;
	for (int32 i = 0; i <= N; ++i)
	{
		const double U = -1.0 + 2.0 * i / N;
		Axis.Add(Extent * FMath::Sign(U) * FMath::Pow(FMath::Abs(U), 1.6));
	}
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<int32> Triangles;
	for (int32 j = 0; j <= N; ++j)
	{
		for (int32 i = 0; i <= N; ++i)
		{
			const double X = Axis[i], Y = Axis[j];
			const double Z = FieldZ(X, Y);
			Vertices.Add(FVector(X, Y, Z));
			const double E = 60.0;
			Normals.Add(FVector(FieldZ(X - E, Y) - FieldZ(X + E, Y), FieldZ(X, Y - E) - FieldZ(X, Y + E), 2.0 * E).GetSafeNormal());
			UVs.Add(FVector2D(X, Y) / 160.0);
			const float Var = static_cast<float>(0.85 + 0.3 * Noise(X, Y, 900.0));
			const float Dry = static_cast<float>(FMath::Clamp(Z / 2400.0, 0.0, 1.0) * 0.7 + 0.25 * Noise(Y, X, 2200.0));
			Colors.Add(KitPaint(FMath::Lerp(Base, Sere, Dry) * Var, Cell));
		}
	}
	for (int32 j = 0; j < N; ++j)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const int32 A = j * (N + 1) + i, B = A + 1, Cn = A + N + 1, D = Cn + 1;
			Triangles.Append({A, Cn, B, B, Cn, D});
		}
	}
	Field = NewObject<UProceduralMeshComponent>(this);
	Field->SetupAttachment(RootComponent);
	Field->RegisterComponent();
	Field->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
	Field->SetMaterial(0, Kit);
	Field->SetCollisionProfileName(TEXT("BlockAll"));
	// Woods on the hills all round, whatever the plot (the woods on the field itself are the plot's own).
	FRandomStream Rng(Seed + 211);
	for (int32 k = 0; k < 110; ++k)
	{
		const double X = Rng.FRandRange(-Extent * 0.8, Extent * 0.8), Y = Rng.FRandRange(-Extent * 0.8, Extent * 0.8);
		if (FMath::Max(FMath::Abs(X) / FieldX, FMath::Abs(Y) / FieldY) < 1.25) continue;
		const TCHAR* Kind = Rng.FRand() < 0.55f ? TEXT("Tree_Broadleaf") : TEXT("Tree_Conifer");
		if (SovArt::Mesh(TEXT("Nature"), Kind))
		{
			UStaticMeshComponent* T = Add(CubeMesh, FVector::ZeroVector, FVector(1.0), FLinearColor::White);
			SovArt::SetKitMesh(T, TEXT("Nature"), Kind, FLinearColor::White);
			T->SetRelativeLocationAndRotation(FVector(X, Y, FieldZ(X, Y) - 20.0), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
			T->SetRelativeScale3D(FVector(Rng.FRandRange(1.2f, 2.1f)));
			T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	// A haze that softens the hills, and a warm grade over the scene (the map keeps its own).
	Haze = NewObject<UExponentialHeightFogComponent>(this);
	Haze->SetupAttachment(RootComponent);
	Haze->SetFogDensity(0.007f);
	Haze->SetFogHeightFalloff(0.08f);
	Haze->SetStartDistance(3000.f);
	Haze->SetFogInscatteringColor(FLinearColor(0.62f, 0.66f, 0.72f));
	Haze->RegisterComponent();
	Grade = NewObject<UPostProcessComponent>(this);
	Grade->SetupAttachment(RootComponent);
	Grade->bUnbound = true;
	Grade->Priority = 1.f;
	FPostProcessSettings& S = Grade->Settings;
	S.bOverride_ColorContrast = S.bOverride_ColorSaturation = S.bOverride_WhiteTemp = S.bOverride_VignetteIntensity = true;
	S.ColorContrast = FVector4(1.1, 1.1, 1.1, 1.0);
	S.ColorSaturation = FVector4(1.12, 1.12, 1.12, 1.0);
	S.WhiteTemp = 7200.f;  // above 6500 warms the image
	S.VignetteIntensity = 0.35f;
	Grade->RegisterComponent();
	return true;
}
