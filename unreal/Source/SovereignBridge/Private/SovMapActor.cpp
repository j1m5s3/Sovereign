#include "SovMapActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "SovHexLayout.h"
#include "SovArt.h"

namespace
{
constexpr double BaseZ = -30.0;       // bottom of every tile's skirt
constexpr double TileScale = 0.95;    // leaves a thin dark seam between hexes
constexpr float FogFactor = 0.4f;     // revealed but not currently visible
constexpr double CityHeight = 30.0;

struct FSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;

	// Adds the triangle with both windings, so it shows whichever way it faces.
	void Tri(int32 A, int32 B, int32 C)
	{
		Triangles.Append({A, B, C, A, C, B});
	}
	int32 Vert(const FVector& P, const FVector& N)
	{
		Normals.Add(N);
		return Vertices.Add(P);
	}
};

uint32 ColorKey(const FLinearColor& C)
{
	return C.ToFColor(false).ToPackedRGBA();
}
}  // namespace

ASovMapActor::ASovMapActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->bUseAsyncCooking = true;
	Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Terrain;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BaseMaterial = Material.Object;
	ConeMesh = Cone.Object;
	SphereMesh = Sphere.Object;
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;

	Highlight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Highlight"));
	Highlight->SetupAttachment(RootComponent);
	Highlight->SetStaticMesh(CylinderMesh);
	Highlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Highlight->SetRelativeScale3D(FVector(1.55, 1.55, 0.04));
	Highlight->SetVisibility(false);
}

double ASovMapActor::ReliefHeight(ESovRelief Relief)
{
	switch (Relief)
	{
		case ESovRelief::Water: return -8.0;
		case ESovRelief::Hills: return 18.0;
		case ESovRelief::Mountain: return 55.0;
		default: return 0.0;
	}
}

double ASovMapActor::SurfaceZ(int32 X, int32 Y) const
{
	const int32 Index = Y * Width + X;
	return Heights.IsValidIndex(Index) ? Heights[Index] : 0.0;
}

UMaterialInstanceDynamic* ASovMapActor::MaterialFor(const FLinearColor& Color)
{
	const uint32 Key = ColorKey(Color);
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Materials.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Materials.Add(Key, Mid);
	return Mid;
}

UStaticMeshComponent* ASovMapActor::Marker(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32 Index, UStaticMesh* Mesh)
{
	while (Pool.Num() <= Index)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		Pool.Add(C);
	}
	UStaticMeshComponent* C = Pool[Index];
	if (C->GetStaticMesh() != Mesh)
	{
		C->SetStaticMesh(Mesh);
	}
	C->SetVisibility(true);
	return C;
}

void ASovMapActor::BuildTerrain(const FSovMirror& Mirror)
{
	Width = Mirror.Width;
	Heights.Init(0.0, Mirror.Width * Mirror.Height);

	TArray<FSection> Sections;
	TArray<FLinearColor> SectionColors;
	TMap<uint32, int32> SectionOf;

	auto SectionFor = [&](const FLinearColor& Color) -> FSection& {
		const uint32 Key = ColorKey(Color);
		if (const int32* Found = SectionOf.Find(Key))
		{
			return Sections[*Found];
		}
		SectionOf.Add(Key, Sections.Num());
		SectionColors.Add(Color);
		return Sections.AddDefaulted_GetRef();
	};

	// A dark floor under the whole map shows through the seams between hexes.
	{
		const FVector2D Far = SovHex::MapPos(Mirror.Width, Mirror.Height);
		FSection& Floor = SectionFor(FLinearColor(0.01f, 0.01f, 0.012f));
		const FVector Up(0, 0, 1);
		const double Pad = SovHex::Size * 2;
		const int32 A = Floor.Vert(SovHex::ToWorld(FVector2D(-Pad, -Pad), BaseZ + 5), Up);
		const int32 B = Floor.Vert(SovHex::ToWorld(FVector2D(Far.X + Pad, -Pad), BaseZ + 5), Up);
		const int32 C = Floor.Vert(SovHex::ToWorld(FVector2D(Far.X + Pad, Far.Y + Pad), BaseZ + 5), Up);
		const int32 D = Floor.Vert(SovHex::ToWorld(FVector2D(-Pad, Far.Y + Pad), BaseZ + 5), Up);
		Floor.Tri(A, B, C);
		Floor.Tri(A, C, D);
	}

	for (const FSovTile& Tile : Mirror.Tiles)
	{
		const double Top = ReliefHeight(Tile.Relief);
		Heights[Tile.Y * Mirror.Width + Tile.X] = Top;
		const FLinearColor Color = Tile.bVisible ? Tile.Color : Tile.Color * FogFactor;
		FSection& S = SectionFor(FLinearColor(Color.R, Color.G, Color.B, 1.f));

		const FVector2D Center = SovHex::MapPos(Tile.X, Tile.Y);
		FVector2D Corners[6];
		for (int32 i = 0; i < 6; ++i)
		{
			const double Angle = FMath::DegreesToRadians(60.0 * i + 30.0);
			Corners[i] = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * SovHex::Size * TileScale;
		}
		const FVector Up(0, 0, 1);
		const int32 Mid = S.Vert(SovHex::ToWorld(Center, Top), Up);
		int32 Ring[6];
		for (int32 i = 0; i < 6; ++i)
		{
			Ring[i] = S.Vert(SovHex::ToWorld(Corners[i], Top), Up);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			S.Tri(Mid, Ring[i], Ring[(i + 1) % 6]);
		}
		// Skirt down to the floor so raised plots read as solid.
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector2D& P0 = Corners[i];
			const FVector2D& P1 = Corners[(i + 1) % 6];
			const FVector2D Out = ((P0 + P1) * 0.5 - Center).GetSafeNormal();
			const FVector N = SovHex::ToWorld(Out, 0.0);
			const int32 A = S.Vert(SovHex::ToWorld(P0, Top), N);
			const int32 B = S.Vert(SovHex::ToWorld(P1, Top), N);
			const int32 C = S.Vert(SovHex::ToWorld(P1, BaseZ), N);
			const int32 D = S.Vert(SovHex::ToWorld(P0, BaseZ), N);
			S.Tri(A, B, C);
			S.Tri(A, C, D);
		}
	}

	Terrain->ClearAllMeshSections();
	const TArray<FVector2D> NoUVs;
	const TArray<FColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;
	for (int32 i = 0; i < Sections.Num(); ++i)
	{
		Terrain->CreateMeshSection(i, Sections[i].Vertices, Sections[i].Triangles, Sections[i].Normals, NoUVs, NoColors, NoTangents, false);
		Terrain->SetMaterial(i, MaterialFor(SectionColors[i]));
	}
}

int32 ASovMapActor::EdgeStrips(const TArray<FSovEdge>& Edges, TArray<TObjectPtr<UStaticMeshComponent>>& Pool, double Inset, double StripWidth, double Lift)
{
	int32 Count = 0;
	for (const FSovEdge& E : Edges)
	{
		const FVector A = SovHex::Center(E.A.X, E.A.Y, SurfaceZ(E.A.X, E.A.Y));
		const FVector B = SovHex::Center(E.B.X, E.B.Y, SurfaceZ(E.B.X, E.B.Y));
		const FVector Dir = B - A;
		if (Dir.Size2D() > SovHex::Size * 2.5)
		{
			continue;  // a pair across the east-west wrap
		}
		const FVector Across = FVector(Dir.X, Dir.Y, 0.0).GetSafeNormal();
		FVector Mid = (A + B) * 0.5 - Across * Inset;
		Mid.Z = FMath::Max(A.Z, B.Z) + Lift;
		UStaticMeshComponent* C = Marker(Pool, Count++, CubeMesh.Get());
		C->SetRelativeLocation(Mid);
		// The edge runs at right angles to the line between the two centres; a hex side is as long as the hex's radius.
		C->SetRelativeRotation(FRotator(0.f, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X))) + 90.f, 0.f));
		C->SetRelativeScale3D(FVector(SovHex::Size * TileScale / 100.0, StripWidth / 100.0, 0.02));
		C->SetMaterial(0, MaterialFor(E.Color));
	}
	for (int32 i = Count; i < Pool.Num(); ++i)
	{
		Pool[i]->SetVisibility(false);
	}
	return Count;
}

void ASovMapActor::Sync(const FSovMirror& Mirror)
{
	BuildTerrain(Mirror);

	// Rivers along plot edges; borders just inside the owner's side.
	EdgeStrips(Mirror.Rivers, RiverPieces, 0.0, 9.0, 1.0);
	EdgeStrips(Mirror.Borders, BorderPieces, 7.0, 4.0, 2.0);

	// Resources the viewer sees (a small ball: green bonus, violet luxury, red strategic) and improvements (a flat tile,
	// dark red when pillaged).
	int32 ResourceCount = 0, ImprovementCount = 0;
	for (const FSovTile& Tile : Mirror.Tiles)
	{
		const FVector At = SovHex::Center(Tile.X, Tile.Y, SurfaceZ(Tile.X, Tile.Y));
		if (Tile.ResourceClass > 0)
		{
			static const FLinearColor Colors[] = {FLinearColor::White, FLinearColor(0.35f, 0.8f, 0.3f), FLinearColor(0.7f, 0.35f, 0.9f), FLinearColor(0.9f, 0.25f, 0.2f)};
			UStaticMeshComponent* C = Marker(ResourcePieces, ResourceCount++, SphereMesh.Get());
			C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(-30.0, 26.0), 7.0));
			C->SetRelativeScale3D(FVector(0.14));
			C->SetMaterial(0, MaterialFor(Colors[Tile.ResourceClass & 3]));
		}
		if (Tile.bImproved)
		{
			UStaticMeshComponent* C = Marker(ImprovementPieces, ImprovementCount++, CubeMesh.Get());
			C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(30.0, -26.0), 2.0));
			C->SetRelativeScale3D(FVector(0.26, 0.26, 0.03));
			C->SetMaterial(0, MaterialFor(Tile.bPillaged ? FLinearColor(0.45f, 0.08f, 0.06f) : FLinearColor(0.78f, 0.66f, 0.35f)));
		}
	}
	for (int32 i = ResourceCount; i < ResourcePieces.Num(); ++i) ResourcePieces[i]->SetVisibility(false);
	for (int32 i = ImprovementCount; i < ImprovementPieces.Num(); ++i) ImprovementPieces[i]->SetVisibility(false);

	// Woods: a few kit trees per wooded tile (map scale: 1 km hexes drawn 1 m wide).
	int32 TreeCount = 0;
	if (SovArt::Mesh(TEXT("Nature"), TEXT("Tree_Broadleaf")))
	{
		for (const FSovTile& Tile : Mirror.Tiles)
		{
			if (!Tile.bWoods)
			{
				continue;
			}
			const uint32 H = static_cast<uint32>(Tile.X * 73856093) ^ static_cast<uint32>(Tile.Y * 19349663);
			for (int32 k = 0; k < 3; ++k)
			{
				const double A = (k * 2.1 + (H % 7)) * 1.0, R = 32.0 + (H >> (k * 3) & 7) * 3.0;
				UStaticMeshComponent* T = Marker(Trees, TreeCount++, nullptr);
				SovArt::SetKitMesh(T, TEXT("Nature"), (H >> k) & 1 ? TEXT("Tree_Conifer") : TEXT("Tree_Broadleaf"), FLinearColor::White);
				T->SetRelativeLocation(SovHex::Center(Tile.X, Tile.Y, SurfaceZ(Tile.X, Tile.Y)) + SovHex::ToWorld(FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, 0.0));
				T->SetRelativeScale3D(FVector(0.075));
				T->SetVisibility(true);
			}
		}
	}
	for (int32 i = TreeCount; i < Trees.Num(); ++i)
	{
		Trees[i]->SetVisibility(false);
	}

	// Roads: a thin packed-earth strip from centre to centre (wrapping pairs are skipped).
	int32 RoadCount = 0;
	for (const TPair<FIntPoint, FIntPoint>& R : Mirror.Roads)
	{
		const FVector A = SovHex::Center(R.Key.X, R.Key.Y, SurfaceZ(R.Key.X, R.Key.Y));
		const FVector B = SovHex::Center(R.Value.X, R.Value.Y, SurfaceZ(R.Value.X, R.Value.Y));
		const FVector Dir = B - A;
		if (Dir.Size2D() > SovHex::Size * 2.5)
		{
			continue;
		}
		UStaticMeshComponent* C = Marker(RoadPieces, RoadCount++, CubeMesh.Get());
		C->SetRelativeLocation((A + B) * 0.5 + FVector(0, 0, 1.5));
		C->SetRelativeRotation(FRotator(0.f, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X))), 0.f));
		C->SetRelativeScale3D(FVector(Dir.Size2D() / 100.0, 0.07, 0.02));
		C->SetMaterial(0, MaterialFor(FLinearColor(0.42f, 0.33f, 0.22f)));
	}
	for (int32 i = RoadCount; i < RoadPieces.Num(); ++i)
	{
		RoadPieces[i]->SetVisibility(false);
	}

	int32 WonderCount = 0;
	for (const FSovWonderMarker& W : Mirror.Wonders)
	{
		UStaticMeshComponent* C = Marker(WonderPieces, WonderCount++, CubeMesh.Get());
		const FVector At = SovHex::Center(W.X, W.Y, SurfaceZ(W.X, W.Y));
		if (SovArt::SetKitMesh(C, TEXT("Classical"), W.bComplete ? TEXT("Temple") : TEXT("Monument"), FLinearColor::White))
		{
			C->SetRelativeLocation(At);
			C->SetRelativeScale3D(FVector(W.bComplete ? 0.07 : 0.06));
			C->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
			continue;
		}
		C->SetRelativeLocation(At + FVector(0, 0, 20));
		C->SetRelativeScale3D(FVector(0.4, 0.4, W.bComplete ? 0.4 : 0.15));
		C->SetMaterial(0, MaterialFor(FLinearColor(0.85f, 0.8f, 0.6f)));
	}
	for (int32 i = WonderCount; i < WonderPieces.Num(); ++i)
	{
		WonderPieces[i]->SetVisibility(false);
	}
	// Tribal villages (01): a small hut.
	int32 VillageCount = 0;
	for (const FIntPoint& V : Mirror.Villages)
	{
		UStaticMeshComponent* C = Marker(VillagePieces, VillageCount++, CubeMesh.Get());
		C->SetRelativeLocation(SovHex::Center(V.X, V.Y, SurfaceZ(V.X, V.Y)) + FVector(0, 0, 10));
		C->SetRelativeRotation(FRotator(0.f, 30.f, 0.f));
		C->SetRelativeScale3D(FVector(0.22, 0.22, 0.18));
		C->SetMaterial(0, MaterialFor(FLinearColor(0.55f, 0.4f, 0.22f)));
	}
	for (int32 i = VillageCount; i < VillagePieces.Num(); ++i)
	{
		VillagePieces[i]->SetVisibility(false);
	}
	// Antiquity sites (07): a pale stone.
	int32 SiteCount = 0;
	for (const FIntPoint& A : Mirror.Antiquity)
	{
		UStaticMeshComponent* C = Marker(AntiquityPieces, SiteCount++, CubeMesh.Get());
		C->SetRelativeLocation(SovHex::Center(A.X, A.Y, SurfaceZ(A.X, A.Y)) + FVector(18, 0, 4));
		C->SetRelativeRotation(FRotator(0.f, 15.f, 0.f));
		C->SetRelativeScale3D(FVector(0.16, 0.1, 0.08));
		C->SetMaterial(0, MaterialFor(FLinearColor(0.8f, 0.78f, 0.7f)));
	}
	for (int32 i = SiteCount; i < AntiquityPieces.Num(); ++i)
	{
		AntiquityPieces[i]->SetVisibility(false);
	}

	for (int32 i = 0; i < Mirror.Cities.Num(); ++i)
	{
		const FSovCityMarker& City = Mirror.Cities[i];
		UStaticMeshComponent* C = Marker(CityMarkers, i, CubeMesh);
		const double Z = SurfaceZ(City.X, City.Y);
		// A small Palace for capitals and a hall for other cities, when the art exists.
		if (SovArt::SetKitMesh(C, TEXT("Classical"), City.bCapital ? TEXT("Palace") : TEXT("Landmark"), FLinearColor::White))
		{
			C->SetRelativeLocation(SovHex::Center(City.X, City.Y, Z));
			C->SetRelativeScale3D(FVector(City.bCapital ? 0.055 : 0.075));
			continue;
		}
		C->SetRelativeLocation(SovHex::Center(City.X, City.Y, Z + CityHeight * 0.5));
		C->SetRelativeScale3D(City.bCapital ? FVector(1.0, 1.0, CityHeight / 100.0) : FVector(0.8, 0.8, CityHeight / 100.0));
		C->SetMaterial(0, MaterialFor(City.Color));
	}
	for (int32 i = Mirror.Cities.Num(); i < CityMarkers.Num(); ++i)
	{
		CityMarkers[i]->SetVisibility(false);
	}

	int32 CrownCount = 0, BoatCount = 0;
	for (int32 i = 0; i < Mirror.Units.Num(); ++i)
	{
		const FSovUnitMarker& Unit = Mirror.Units[i];
		UStaticMesh* Mesh = Unit.bLeader ? CylinderMesh.Get() : Unit.bCivilian ? SphereMesh.Get() : ConeMesh.Get();
		UStaticMeshComponent* C = Marker(UnitMarkers, i, Mesh);
		const double Z = SurfaceZ(Unit.X, Unit.Y) + (Unit.bInCity ? CityHeight : 0.0);
		FVector Pos = SovHex::Center(Unit.X, Unit.Y);
		// Kit figures in the owner's colour when the art exists; a figure stands about 45 cm tall on the map.
		const TCHAR* FigureName = Unit.bNaval ? TEXT("Ship") : Unit.bLeader ? TEXT("Leader") : Unit.bCivilian ? TEXT("Citizen") : TEXT("Soldier");
		if (SovArt::SetKitMesh(C, TEXT("Figures"), FigureName, Unit.Color))
		{
			if (Unit.bNaval)
			{
				C->SetRelativeLocation(FVector(Pos.X, Pos.Y, SurfaceZ(Unit.X, Unit.Y)));
				C->SetRelativeScale3D(FVector(0.14));
				C->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));  // side-on to the camera
				continue;
			}
			const FVector2D Offset = Unit.bLeader ? FVector2D(-38.0, -30.0) : Unit.bCivilian ? FVector2D(38.0, 30.0) : FVector2D(0.0, 0.0);
			Pos += SovHex::ToWorld(Offset, 0.0);
			C->SetRelativeLocation(FVector(Pos.X, Pos.Y, SurfaceZ(Unit.X, Unit.Y)));
			C->SetRelativeScale3D(FVector(Unit.bLeader ? 0.3 : 0.26));
			C->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));  // face the camera (south)
			if (Unit.bEmbarked)
			{
				// An embarked unit rides in a boat in its owner's colour.
				UStaticMeshComponent* B = Marker(Boats, BoatCount++, CubeMesh.Get());
				if (SovArt::SetKitMesh(B, TEXT("Figures"), TEXT("Boat"), Unit.Color))
				{
					B->SetRelativeLocation(FVector(Pos.X, Pos.Y, SurfaceZ(Unit.X, Unit.Y)));
					B->SetRelativeScale3D(FVector(0.26));
					B->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
					C->SetRelativeLocation(FVector(Pos.X, Pos.Y, SurfaceZ(Unit.X, Unit.Y) + 0.26 * 30.0));
				}
			}
			continue;
		}
		if (Unit.bLeader)
		{
			// The leader stands to the north-west of its escort: an owner-coloured pillar with a gold crown.
			Pos += SovHex::ToWorld(FVector2D(-38.0, -30.0), 0.0);
			C->SetRelativeScale3D(FVector(0.3, 0.3, 0.55));
			C->SetRelativeLocation(FVector(Pos.X, Pos.Y, Z + 27.5));
			UStaticMeshComponent* Crown = Marker(Crowns, CrownCount++, SphereMesh.Get());
			Crown->SetRelativeScale3D(FVector(0.22));
			Crown->SetRelativeLocation(FVector(Pos.X, Pos.Y, Z + 62.0));
			Crown->SetMaterial(0, MaterialFor(FLinearColor(1.f, 0.75f, 0.1f)));
		}
		else if (Unit.bCivilian)
		{
			// Civilians stand to the south-east so an escort on the same plot stays visible.
			Pos += SovHex::ToWorld(FVector2D(38.0, 30.0), 0.0);
			C->SetRelativeScale3D(FVector(0.35));
			C->SetRelativeLocation(FVector(Pos.X, Pos.Y, Z + 17.5));
		}
		else
		{
			C->SetRelativeScale3D(FVector(0.45, 0.45, 0.7));
			C->SetRelativeLocation(FVector(Pos.X, Pos.Y, Z + 35.0));
		}
		C->SetMaterial(0, MaterialFor(Unit.Color));
	}
	for (int32 i = Mirror.Units.Num(); i < UnitMarkers.Num(); ++i)
	{
		UnitMarkers[i]->SetVisibility(false);
	}
	for (int32 i = CrownCount; i < Crowns.Num(); ++i)
	{
		Crowns[i]->SetVisibility(false);
	}
	for (int32 i = BoatCount; i < Boats.Num(); ++i)
	{
		Boats[i]->SetVisibility(false);
	}
}

void ASovMapActor::SetHighlight(int32 X, int32 Y)
{
	if (X < 0)
	{
		Highlight->SetVisibility(false);
		return;
	}
	Highlight->SetMaterial(0, MaterialFor(FLinearColor(1.f, 0.85f, 0.1f)));
	Highlight->SetRelativeLocation(SovHex::Center(X, Y, SurfaceZ(X, Y) + 2.0));
	Highlight->SetVisibility(true);
}
