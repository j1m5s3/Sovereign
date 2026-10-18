#include "SovBattleScene.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "SovArt.h"

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
	UStaticMeshComponent* Floor = Add(CubeMesh, FVector(0, 0, -50), FVector(90.0, 60.0, 1.0), Ground);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
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
		Bodies.Add(Body);
		Heads.Add(Head);
	}
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		const FSovSoldier& S = Men[i];
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
