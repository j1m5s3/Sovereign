#include "SovBattleScene.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "SovArt.h"

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
		// exists; the still figure stays hidden then. Riders have no rig (the skeleton has no horse), and stay still.
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
		Striding.Add(false);
		Bodies.Add(Body);
		Heads.Add(Head);
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double Dt = FMath::Max(Now - LastSyncTime, 1e-3);
	LastSyncTime = Now;
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		const FSovSoldier& S = Men[i];
		if (USkeletalMeshComponent* Rig = Rigs[i])
		{
			// Facing the way it goes while it walks, else the enemy's side; down where it fell.
			const FVector2D Step = S.Pos - LastPos[i];
			LastPos[i] = S.Pos;
			const float Speed = static_cast<float>(Step.Size() / Dt);
			const bool bWalk = S.bAlive && Speed > 25.f;
			if (bWalk != Striding[i] && S.bAlive)
			{
				Striding[i] = bWalk;
				if (UAnimSequence* A = SovArt::RiggedAnim(RigNames[i], bWalk ? TEXT("Walk") : TEXT("Idle"))) Rig->PlayAnimation(A, true);
			}
			const float Face = (S.Side == 0 ? -90.f : 90.f);
			const float Yaw = bWalk ? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Step.Y, Step.X))) - 90.f : Face;
			if (S.bAlive)
			{
				Rig->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 0), FRotator(0.f, Yaw, 0.f));
				if (bWalk) Rig->SetPlayRate(FMath::Clamp(Speed / 108.f, 0.5f, 3.f));  // rig_figures.py WALK_SPEED
			}
			else if (Rig->IsPlaying())
			{
				Rig->Stop();
				Rig->SetRelativeLocationAndRotation(FVector(S.Pos.X, S.Pos.Y, 15), FRotator(0.f, Face, 90.f));
			}
			continue;
		}
		if (S.bLeader && i == Sim.LeaderIndex() && S.bAlive)
		{
			// The human's leader is drawn by its walker.
			Bodies[i]->SetVisibility(false);
			Heads[i]->SetVisibility(false);
			continue;
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
