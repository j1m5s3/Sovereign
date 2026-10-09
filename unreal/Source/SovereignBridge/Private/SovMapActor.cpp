#include "SovMapActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
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
	// For the art kit's material (M_SovKit): the colour as sRGB bytes it squares back, the detail tile in alpha,
	// and UVs laid flat across the map so the tile repeats about once a plot.
	TArray<FColor> Colors;
	TArray<FVector2D> UVs;
	FColor Paint = FColor::White;

	// Adds the triangle with both windings, so it shows whichever way it faces.
	void Tri(int32 A, int32 B, int32 C)
	{
		Triangles.Append({A, B, C, A, C, B});
	}
	int32 Vert(const FVector& P, const FVector& N)
	{
		Normals.Add(N);
		Colors.Add(Paint);
		UVs.Add(FVector2D(P.Y + P.Z * 0.5, P.Z * 0.5 - P.X) / 170.0);
		return Vertices.Add(P);
	}
};

uint32 ColorKey(const FLinearColor& C)
{
	return C.ToFColor(false).ToPackedRGBA();
}

// A linear colour as the sRGB-ish bytes M_SovKit squares back to linear, with the tile index in alpha.
FColor KitPaint(const FLinearColor& Linear, int32 Tile)
{
	auto Byte = [](float V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Sqrt(FMath::Max(V, 0.f)) * 255.f), 0, 255)); };
	return FColor(Byte(Linear.R), Byte(Linear.G), Byte(Linear.B), static_cast<uint8>(FMath::RoundToInt(Tile / 15.f * 255.f)));
}
// The Fields kit's model for an improvement (tools/art/blender/kit_fields.py); the rest share a walled yard.
const TCHAR* FieldsModel(const FString& Id)
{
	static const TPair<const TCHAR*, const TCHAR*> Models[] = {
		{TEXT("IMPROVEMENT_FARM"), TEXT("Farm")}, {TEXT("IMPROVEMENT_MINE"), TEXT("Mine")}, {TEXT("IMPROVEMENT_QUARRY"), TEXT("Quarry")},
		{TEXT("IMPROVEMENT_PASTURE"), TEXT("Pasture")}, {TEXT("IMPROVEMENT_PLANTATION"), TEXT("Plantation")},
		{TEXT("IMPROVEMENT_CAMP"), TEXT("Camp")}, {TEXT("IMPROVEMENT_FISHING_BOATS"), TEXT("FishingBoats")},
		{TEXT("IMPROVEMENT_FISHERY"), TEXT("FishingBoats")}, {TEXT("IMPROVEMENT_LUMBER_MILL"), TEXT("LumberMill")},
		{TEXT("IMPROVEMENT_OIL_WELL"), TEXT("OilWell")}, {TEXT("IMPROVEMENT_OFFSHORE_OIL_RIG"), TEXT("OilWell")},
		{TEXT("IMPROVEMENT_FORT"), TEXT("Fort")}, {TEXT("IMPROVEMENT_WIND_FARM"), TEXT("WindFarm")},
		{TEXT("IMPROVEMENT_OFFSHORE_WIND_FARM"), TEXT("WindFarm")}, {TEXT("IMPROVEMENT_SOLAR_FARM"), TEXT("SolarFarm")}};
	for (const auto& M : Models)
		if (Id == M.Key) return M.Value;
	return TEXT("Works");
}
// The Districts kit's model for a district id: DISTRICT_HOLY_SITE -> HolySite (tools/art/blender/kit_districts.py).
FString DistrictModel(const FString& Id)
{
	FString Out;
	TArray<FString> Words;
	Id.RightChop(9).ParseIntoArray(Words, TEXT("_"));
	for (const FString& W : Words)
	{
		Out += W.Left(1) + W.RightChop(1).ToLower();
	}
	return Out;
}

// The Wonders kit's model for a wonder (tools/art/blender/kit_wonders.py), by kind; null for the classical temple.
const TCHAR* WonderModel(const FString& Id)
{
	// Each model and the wonders (their ids without BUILDING_) drawn with it.
	static const TPair<const TCHAR*, const TCHAR*> Kinds[] = {
		{TEXT("Pyramid"), TEXT("PYRAMIDS JEBEL_BARKAL")},
		{TEXT("StepPyramid"), TEXT("ETEMENANKI CHICHEN_ITZA HUEY_TEOCALLI MAHABODHI_TEMPLE MEENAKSHI_TEMPLE ANGKOR_WAT")},
		{TEXT("StoneCircle"), TEXT("STONEHENGE")},
		{TEXT("Gardens"), TEXT("HANGING_GARDENS GREAT_BATH BIOSPH_RE")},
		{TEXT("Statue"), TEXT("COLOSSUS STATUE_OF_ZEUS STATUE_OF_LIBERTY CRISTO_REDENTOR TERRACOTTA_ARMY KOTOKU_IN")},
		{TEXT("Tower"), TEXT("GREAT_LIGHTHOUSE TORRE_DE_BEL_M BIG_BEN KILWA_KISIWANI")},
		{TEXT("LatticeTower"), TEXT("EIFFEL_TOWER")},
		{TEXT("DomedHall"), TEXT("HAGIA_SOPHIA TAJ_MAHAL ST_BASIL_S_CATHEDRAL UNIVERSITY_OF_SANKORE OXFORD_UNIVERSITY HERMITAGE")},
		{TEXT("Arena"), TEXT("COLOSSEUM EST_DIO_DO_MARACAN BOLSHOI_THEATRE BROADWAY SYDNEY_OPERA_HOUSE")},
		{TEXT("Citadel"), TEXT("PETRA MACHU_PICCHU ALHAMBRA MONT_ST_MICHEL GREAT_ZIMBABWE FORBIDDEN_CITY POTALA_PALACE ORSZ_GH_Z")},
		{TEXT("Arsenal"), TEXT("VENETIAN_ARSENAL RUHR_VALLEY PANAMA_CANAL CASA_DE_CONTRATACI_N GOLDEN_GATE_BRIDGE AMUNDSEN_SCOTT_RESEARCH_STATION")},
	};
	const FString Key = Id.RightChop(9);
	for (const auto& K : Kinds)
	{
		TArray<FString> Ids;
		FString(K.Value).ParseIntoArray(Ids, TEXT(" "));
		if (Ids.Contains(Key)) return K.Key;
	}
	return nullptr;
}

// The Resources kit's model for a resource and the colour its accent is tinted (tools/art/blender/kit_resources.py).
struct FResourceLook
{
	const TCHAR* Model;
	FLinearColor Accent;
};
const FResourceLook* ResourceLook(const FString& Id)
{
	static const TMap<FString, FResourceLook> Looks = {
		{TEXT("BANANAS"), {TEXT("Shrub"), FLinearColor(0.85f, 0.75f, 0.2f)}},
		{TEXT("CATTLE"), {TEXT("Beast"), FLinearColor(0.45f, 0.3f, 0.2f)}},
		{TEXT("COPPER"), {TEXT("Ore"), FLinearColor(0.75f, 0.45f, 0.25f)}},
		{TEXT("CRABS"), {TEXT("Shell"), FLinearColor(0.85f, 0.35f, 0.25f)}},
		{TEXT("DEER"), {TEXT("Beast"), FLinearColor(0.6f, 0.45f, 0.3f)}},
		{TEXT("FISH"), {TEXT("Fish"), FLinearColor(0.6f, 0.65f, 0.7f)}},
		{TEXT("MAIZE"), {TEXT("Sheaf"), FLinearColor(0.9f, 0.75f, 0.25f)}},
		{TEXT("RICE"), {TEXT("Sheaf"), FLinearColor(0.85f, 0.85f, 0.7f)}},
		{TEXT("SHEEP"), {TEXT("Beast"), FLinearColor(0.92f, 0.9f, 0.85f)}},
		{TEXT("STONE"), {TEXT("Blocks"), FLinearColor(0.7f, 0.68f, 0.64f)}},
		{TEXT("WHEAT"), {TEXT("Sheaf"), FLinearColor(0.85f, 0.65f, 0.3f)}},
		{TEXT("AMBER"), {TEXT("Ore"), FLinearColor(0.85f, 0.55f, 0.15f)}},
		{TEXT("CINNAMON"), {TEXT("Shrub"), FLinearColor(0.6f, 0.35f, 0.2f)}},
		{TEXT("CITRUS"), {TEXT("Shrub"), FLinearColor(0.95f, 0.6f, 0.15f)}},
		{TEXT("CLOVES"), {TEXT("Shrub"), FLinearColor(0.45f, 0.25f, 0.2f)}},
		{TEXT("COCOA"), {TEXT("Shrub"), FLinearColor(0.5f, 0.3f, 0.15f)}},
		{TEXT("COFFEE"), {TEXT("Shrub"), FLinearColor(0.6f, 0.15f, 0.12f)}},
		{TEXT("COSMETICS"), {TEXT("Goods"), FLinearColor(0.9f, 0.6f, 0.7f)}},
		{TEXT("COTTON"), {TEXT("Shrub"), FLinearColor(0.95f, 0.95f, 0.92f)}},
		{TEXT("DIAMONDS"), {TEXT("Ore"), FLinearColor(0.85f, 0.92f, 0.98f)}},
		{TEXT("DYES"), {TEXT("Shrub"), FLinearColor(0.5f, 0.2f, 0.6f)}},
		{TEXT("FURS"), {TEXT("Beast"), FLinearColor(0.35f, 0.25f, 0.18f)}},
		{TEXT("GYPSUM"), {TEXT("Blocks"), FLinearColor(0.92f, 0.9f, 0.85f)}},
		{TEXT("HONEY"), {TEXT("Hive"), FLinearColor(0.85f, 0.6f, 0.15f)}},
		{TEXT("INCENSE"), {TEXT("Shrub"), FLinearColor(0.75f, 0.7f, 0.55f)}},
		{TEXT("IVORY"), {TEXT("Beast"), FLinearColor(0.6f, 0.6f, 0.62f)}},
		{TEXT("JADE"), {TEXT("Ore"), FLinearColor(0.3f, 0.7f, 0.45f)}},
		{TEXT("JEANS"), {TEXT("Goods"), FLinearColor(0.25f, 0.35f, 0.6f)}},
		{TEXT("MARBLE"), {TEXT("Blocks"), FLinearColor(0.95f, 0.93f, 0.9f)}},
		{TEXT("MERCURY"), {TEXT("Ore"), FLinearColor(0.7f, 0.2f, 0.2f)}},
		{TEXT("OLIVES"), {TEXT("Shrub"), FLinearColor(0.4f, 0.45f, 0.2f)}},
		{TEXT("PEARLS"), {TEXT("Shell"), FLinearColor(0.95f, 0.92f, 0.88f)}},
		{TEXT("PERFUME"), {TEXT("Goods"), FLinearColor(0.7f, 0.5f, 0.8f)}},
		{TEXT("SALT"), {TEXT("Ore"), FLinearColor(0.95f, 0.95f, 0.95f)}},
		{TEXT("SILK"), {TEXT("Shrub"), FLinearColor(0.92f, 0.88f, 0.75f)}},
		{TEXT("SILVER"), {TEXT("Ore"), FLinearColor(0.8f, 0.82f, 0.85f)}},
		{TEXT("SPICES"), {TEXT("Shrub"), FLinearColor(0.8f, 0.3f, 0.1f)}},
		{TEXT("SUGAR"), {TEXT("Sheaf"), FLinearColor(0.6f, 0.75f, 0.35f)}},
		{TEXT("TEA"), {TEXT("Shrub"), FLinearColor(0.35f, 0.55f, 0.25f)}},
		{TEXT("TOBACCO"), {TEXT("Shrub"), FLinearColor(0.6f, 0.55f, 0.3f)}},
		{TEXT("TOYS"), {TEXT("Goods"), FLinearColor(0.85f, 0.25f, 0.2f)}},
		{TEXT("TRUFFLES"), {TEXT("Ore"), FLinearColor(0.25f, 0.2f, 0.18f)}},
		{TEXT("TURTLES"), {TEXT("Shell"), FLinearColor(0.35f, 0.5f, 0.3f)}},
		{TEXT("WHALES"), {TEXT("Fish"), FLinearColor(0.3f, 0.35f, 0.45f)}},
		{TEXT("WINE"), {TEXT("Shrub"), FLinearColor(0.4f, 0.15f, 0.35f)}},
		{TEXT("ALUMINUM"), {TEXT("Ore"), FLinearColor(0.75f, 0.78f, 0.8f)}},
		{TEXT("COAL"), {TEXT("Ore"), FLinearColor(0.15f, 0.15f, 0.15f)}},
		{TEXT("HORSES"), {TEXT("Beast"), FLinearColor(0.55f, 0.32f, 0.18f)}},
		{TEXT("IRON"), {TEXT("Ore"), FLinearColor(0.5f, 0.3f, 0.25f)}},
		{TEXT("NITER"), {TEXT("Ore"), FLinearColor(0.9f, 0.88f, 0.8f)}},
		{TEXT("OIL"), {TEXT("OilPool"), FLinearColor(0.7f, 0.2f, 0.15f)}},
		{TEXT("URANIUM"), {TEXT("Ore"), FLinearColor(0.4f, 0.85f, 0.25f)}},
	};
	return Looks.Find(Id.RightChop(9));
}

// The Nature kit's piece for a terrain feature or natural wonder (tools/art/blender/kit_nature.py); null for none
// (woods have their trees, floodplains only their colour).
const TCHAR* FeatureModel(const FString& Id)
{
	// Each piece and the features (their ids without FEATURE_) drawn with it.
	static const TPair<const TCHAR*, const TCHAR*> Kinds[] = {
		{TEXT("Reeds"), TEXT("MARSH PANTANAL UBSUNUR_HOLLOW")},
		{TEXT("Palms"), TEXT("OASIS GALAPAGOS_ISLANDS PAITITI")},
		{TEXT("Coral"), TEXT("REEF GREAT_BARRIER_REEF")},
		{TEXT("IceFloe"), TEXT("ICE")},
		{TEXT("Volcano"), TEXT("VOLCANO MOUNT_VESUVIUS EYJAFJALLAJOKULL")},
		{TEXT("Fumarole"), TEXT("GEOTHERMAL_FISSURE")},
		{TEXT("BurntTree"), TEXT("BURNING_FOREST BURNT_FOREST BURNING_JUNGLE BURNT_JUNGLE")},
		{TEXT("Peak"), TEXT("MOUNT_EVEREST MOUNT_KILIMANJARO MATTERHORN")},
		{TEXT("Mesa"), TEXT("ULURU MOUNT_RORAIMA MATO_TIPILA GOBUSTAN SAHARA_EL_BEYDA DELICATE_ARCH")},
		{TEXT("Spires"), TEXT("TSINGY_DE_BEMARAHA TORRES_DEL_PAINE ZHANGYE_DANXIA GIANT_S_CAUSEWAY HA_LONG_BAY PIOPIOTAHI LYSEFJORD")},
		{TEXT("Pool"), TEXT("CRATER_LAKE DEAD_SEA IK_KIL PAMUKKALE LAKE_RETBA FOUNTAIN_OF_YOUTH EYE_OF_THE_SAHARA")},
		{TEXT("Cliffs"), TEXT("CLIFFS_OF_DOVER YOSEMITE")},
		{TEXT("Mounds"), TEXT("CHOCOLATE_HILLS")},
	};
	static TMap<FString, const TCHAR*> ById;
	if (ById.IsEmpty())
	{
		for (const auto& K : Kinds)
		{
			TArray<FString> Ids;
			FString(K.Value).ParseIntoArray(Ids, TEXT(" "));
			for (const FString& I : Ids) ById.Add(I, K.Key);
		}
	}
	const TCHAR* const* Found = ById.Find(Id.RightChop(8));
	return Found ? *Found : nullptr;
}

// Features drawn as one large piece in the middle of the plot (volcanoes and natural wonders): no rocks under them.
bool IsLandform(const TCHAR* Model)
{
	return Model && FCString::Strcmp(Model, TEXT("Reeds")) && FCString::Strcmp(Model, TEXT("Palms")) && FCString::Strcmp(Model, TEXT("Coral"))
		&& FCString::Strcmp(Model, TEXT("IceFloe")) && FCString::Strcmp(Model, TEXT("Fumarole")) && FCString::Strcmp(Model, TEXT("BurntTree"));
}

// Tints a kit piece's first material: white as painted, grey while being built, brown when pillaged.
void ShadeKit(UStaticMeshComponent* C, const FLinearColor& Tint)
{
	UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
	if (!Mid || Mid->GetOuter() != C) Mid = C->CreateAndSetMaterialInstanceDynamic(0);
	if (Mid) Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
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

	auto SectionFor = [&](const FLinearColor& Color, int32 Tile = 0) -> FSection& {
		const uint32 Key = ColorKey(Color) ^ (static_cast<uint32>(Tile) * 2654435761u);
		if (const int32* Found = SectionOf.Find(Key))
		{
			return Sections[*Found];
		}
		SectionOf.Add(Key, Sections.Num());
		SectionColors.Add(Color);
		FSection& S = Sections.AddDefaulted_GetRef();
		S.Paint = KitPaint(Color, Tile);
		return S;
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

	// Unexplored plots: blank parchment, a little below the sea, so the known world stands up out of the map. Drawn a
	// touch larger than a plot, so they join into one sheet with no seams.
	{
		FSection& Blank = SectionFor(FLinearColor(0.2f, 0.17f, 0.12f), 1);
		const FVector Up(0, 0, 1);
		const double Z = ReliefHeight(ESovRelief::Water) - 6.0;
		for (const FIntPoint& P : Mirror.Unexplored)
		{
			const FVector2D Center = SovHex::MapPos(P.X, P.Y);
			const int32 Mid = Blank.Vert(SovHex::ToWorld(Center, Z), Up);
			int32 Ring[6];
			for (int32 i = 0; i < 6; ++i)
			{
				const double Angle = FMath::DegreesToRadians(60.0 * i + 30.0);
				Ring[i] = Blank.Vert(SovHex::ToWorld(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * SovHex::Size * 1.02, Z), Up);
			}
			for (int32 i = 0; i < 6; ++i)
			{
				Blank.Tri(Mid, Ring[i], Ring[(i + 1) % 6]);
			}
		}
	}

	for (const FSovTile& Tile : Mirror.Tiles)
	{
		const double Top = ReliefHeight(Tile.Relief);
		Heights[Tile.Y * Mirror.Width + Tile.X] = Top;
		const FLinearColor Color = Tile.bVisible ? Tile.Color : Tile.Color * FogFactor;
		FSection& S = SectionFor(FLinearColor(Color.R, Color.G, Color.B, 1.f), Tile.Detail);

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
	const TArray<FProcMeshTangent> NoTangents;
	for (int32 i = 0; i < Sections.Num(); ++i)
	{
		Terrain->CreateMeshSection(i, Sections[i].Vertices, Sections[i].Triangles, Sections[i].Normals, Sections[i].UVs, Sections[i].Colors, NoTangents, false);
		// The art kit's material paints the terrain (vertex colour times a detail tile); without it the plain one does.
		// Loaded here, not in the constructor: a constructor load roots it, and the art import could not rebuild it.
		UMaterialInterface* Kit = SovArt::KitMaterial();
		Terrain->SetMaterial(i, Kit ? Kit : static_cast<UMaterialInterface*>(MaterialFor(SectionColors[i])));
	}
}

void ASovMapActor::SetReach(const TArray<FSovEdge>& Edges)
{
	EdgeStrips(Edges, ReachPieces, 6.0, 5.0, 3.0);
}

int32 ASovMapActor::EdgeStrips(const TArray<FSovEdge>& Edges, TArray<TObjectPtr<UStaticMeshComponent>>& Pool, double Inset, double StripWidth, double Lift)
{
	int32 Count = 0;
	for (const FSovEdge& E : Edges)
	{
		const FVector A = SovHex::Center(E.A.X, E.A.Y, SurfaceZ(E.A.X, E.A.Y));
		// A pair across the east-west wrap is drawn between A and B's copy beside it.
		const FVector B = SovHex::NearestCopy(SovHex::Center(E.B.X, E.B.Y, SurfaceZ(E.B.X, E.B.Y)), A.Y, WrapWidth);
		const FVector Dir = B - A;
		if (Dir.Size2D() > SovHex::Size * 2.5)
		{
			continue;  // a pair across the edge of a map that does not wrap
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

void ASovMapActor::SyncGhosts(const FSovMirror& Mirror)
{
	if (WrapWidth <= 0.0)
	{
		for (ASovMapActor* Ghost : Ghosts) Ghost->SetActorHiddenInGame(true);
		return;
	}
	while (Ghosts.Num() < 2)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASovMapActor* Ghost = GetWorld()->SpawnActor<ASovMapActor>(GetActorLocation(), FRotator::ZeroRotator, Params);
		Ghost->bGhost = true;
		Ghosts.Add(Ghost);
	}
	for (int32 i = 0; i < Ghosts.Num(); ++i)
	{
		Ghosts[i]->SetActorLocation(GetActorLocation() + FVector(0.0, (i == 0 ? -1.0 : 1.0) * WrapWidth, 0.0));
		Ghosts[i]->SetActorHiddenInGame(false);
		Ghosts[i]->Sync(Mirror);
	}
}

void ASovMapActor::Sync(const FSovMirror& Mirror)
{
	WrapWidth = Mirror.bWrap ? SovHex::MapWorldWidth(Mirror.Width) : 0.0;
	if (!bGhost)
	{
		SyncGhosts(Mirror);
	}
	BuildTerrain(Mirror);

	// Rivers along plot edges; borders just inside the owner's side.
	EdgeStrips(Mirror.Rivers, RiverPieces, 0.0, 9.0, 1.0);
	EdgeStrips(Mirror.Borders, BorderPieces, 7.0, 4.0, 2.0);

	// Resources the viewer sees (a small ball: green bonus, violet luxury, red strategic) and improvements (the Fields
	// kit's model, darkened when pillaged; without the art, a flat tile, dark red when pillaged).
	int32 ResourceCount = 0, ImprovementCount = 0;
	for (const FSovTile& Tile : Mirror.Tiles)
	{
		const FVector At = SovHex::Center(Tile.X, Tile.Y, SurfaceZ(Tile.X, Tile.Y));
		const FResourceLook* Look = Tile.ResourceClass > 0 ? ResourceLook(Tile.Resource) : nullptr;
		if (Look && SovArt::Mesh(TEXT("Resources"), Look->Model))
		{
			const uint32 H = static_cast<uint32>(Tile.X * 40503) ^ static_cast<uint32>(Tile.Y * 2654435761u);
			UStaticMeshComponent* C = Marker(ResourcePieces, ResourceCount++, nullptr);
			SovArt::SetKitMesh(C, TEXT("Resources"), Look->Model, Look->Accent);
			ShadeKit(C, FLinearColor::White);
			C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(-30.0, 26.0), 0.0));
			C->SetRelativeRotation(FRotator(0.f, static_cast<float>(H % 360), 0.f));
			C->SetRelativeScale3D(FVector(Tile.Resource == TEXT("RESOURCE_WHALES") ? 0.13 : 0.1));
		}
		else if (Tile.ResourceClass > 0)
		{
			static const FLinearColor Colors[] = {FLinearColor::White, FLinearColor(0.35f, 0.8f, 0.3f), FLinearColor(0.7f, 0.35f, 0.9f), FLinearColor(0.9f, 0.25f, 0.2f)};
			UStaticMeshComponent* C = Marker(ResourcePieces, ResourceCount++, SphereMesh.Get());
			C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(-30.0, 26.0), 7.0));
			C->SetRelativeScale3D(FVector(0.14));
			C->SetMaterial(0, MaterialFor(Colors[Tile.ResourceClass & 3]));
		}
		if (Tile.bImproved && SovArt::Mesh(TEXT("Fields"), FieldsModel(Tile.Improvement)))
		{
			const uint32 H = static_cast<uint32>(Tile.X * 19349663) ^ static_cast<uint32>(Tile.Y * 83492791);
			UStaticMeshComponent* C = Marker(ImprovementPieces, ImprovementCount++, nullptr);
			SovArt::SetKitMesh(C, TEXT("Fields"), FieldsModel(Tile.Improvement), FLinearColor::White);
			ShadeKit(C, Tile.bPillaged ? FLinearColor(0.45f, 0.3f, 0.25f) : FLinearColor::White);  // browner when pillaged
			C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(14.0, -12.0), 0.0));
			C->SetRelativeRotation(FRotator(0.f, static_cast<float>(H % 6 * 60), 0.f));
			C->SetRelativeScale3D(FVector(0.09));
		}
		else if (Tile.bImproved)
		{
			UStaticMeshComponent* C = Marker(ImprovementPieces, ImprovementCount++, CubeMesh.Get());
			C->SetRelativeRotation(FRotator::ZeroRotator);
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

	// Features: reeds in marshes, palms at oases, coral on reefs, floes on ice, fissures, burnt trees, and a landform
	// for volcanoes and natural wonders (larger, in the middle of the plot).
	int32 FeatureCount = 0;
	for (const FSovTile& Tile : Mirror.Tiles)
	{
		const TCHAR* Model = Tile.Feature.IsEmpty() ? nullptr : FeatureModel(Tile.Feature);
		if (!Model || !SovArt::Mesh(TEXT("Nature"), Model))
		{
			continue;
		}
		const uint32 H = static_cast<uint32>(Tile.X * 2246822519u) ^ static_cast<uint32>(Tile.Y * 3266489917u);
		const bool bLandform = IsLandform(Model);
		const bool bBurnt = !FCString::Strcmp(Model, TEXT("BurntTree"));
		const int32 Count = bBurnt ? 3 : 1;
		for (int32 k = 0; k < Count; ++k)
		{
			const double A = (k * 2.1 + (H % 7)) * 1.0;
			const double R = bLandform ? 0.0 : bBurnt ? 32.0 + ((H >> (k * 3)) & 7) * 3.0 : 22.0;
			UStaticMeshComponent* C = Marker(FeaturePieces, FeatureCount++, nullptr);
			SovArt::SetKitMesh(C, TEXT("Nature"), Model, FLinearColor::White);
			ShadeKit(C, FLinearColor::White);
			C->SetRelativeLocation(SovHex::Center(Tile.X, Tile.Y, SurfaceZ(Tile.X, Tile.Y)) + SovHex::ToWorld(FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, 0.0));
			C->SetRelativeRotation(FRotator(0.f, static_cast<float>((H >> 5) % 360), 0.f));
			C->SetRelativeScale3D(FVector(bLandform ? 0.13 : bBurnt ? 0.075 : !FCString::Strcmp(Model, TEXT("IceFloe")) ? 0.12 : 0.09));
		}
	}
	for (int32 i = FeatureCount; i < FeaturePieces.Num(); ++i)
	{
		FeaturePieces[i]->SetVisibility(false);
	}

	// Rocks: a small cluster on a hill, a large one with a smaller beside it crowning a mountain.
	int32 RockCount = 0;
	if (SovArt::Mesh(TEXT("Nature"), TEXT("Rocks")))
	{
		for (const FSovTile& Tile : Mirror.Tiles)
		{
			if (Tile.Relief != ESovRelief::Hills && Tile.Relief != ESovRelief::Mountain) continue;
			if (IsLandform(FeatureModel(Tile.Feature))) continue;
			const uint32 H = static_cast<uint32>(Tile.X * 83492791) ^ static_cast<uint32>(Tile.Y * 2971215073u);
			const bool bMountain = Tile.Relief == ESovRelief::Mountain;
			const int32 Count = bMountain ? 2 : 1;
			for (int32 k = 0; k < Count; ++k)
			{
				const double A = (H % 360) * PI / 180.0 + k * 2.4;
				const double R = k == 0 ? (bMountain ? 0.0 : 28.0) : 42.0;
				const double Scale = bMountain ? (k == 0 ? 0.34 : 0.2) : 0.12;
				UStaticMeshComponent* Rk = Marker(Rocks, RockCount++, nullptr);
				SovArt::SetKitMesh(Rk, TEXT("Nature"), TEXT("Rocks"), FLinearColor::White);
				Rk->SetRelativeLocation(SovHex::Center(Tile.X, Tile.Y, SurfaceZ(Tile.X, Tile.Y)) + SovHex::ToWorld(FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, 0.0));
				Rk->SetRelativeRotation(FRotator(0.f, static_cast<float>((H >> 4) % 360 + k * 97), 0.f));
				Rk->SetRelativeScale3D(FVector(Scale));
				Rk->SetVisibility(!Tile.bWoods || bMountain);
			}
		}
	}
	for (int32 i = RockCount; i < Rocks.Num(); ++i)
	{
		Rocks[i]->SetVisibility(false);
	}

	// Roads: a thin packed-earth strip from centre to centre (across the wrap, to the copy beside it).
	int32 RoadCount = 0;
	for (const TPair<FIntPoint, FIntPoint>& R : Mirror.Roads)
	{
		const FVector A = SovHex::Center(R.Key.X, R.Key.Y, SurfaceZ(R.Key.X, R.Key.Y));
		const FVector B = SovHex::NearestCopy(SovHex::Center(R.Value.X, R.Value.Y, SurfaceZ(R.Value.X, R.Value.Y)), A.Y, WrapWidth);
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
		// Its kind's model from the Wonders kit, grey and smaller while it is being built.
		const TCHAR* Kind = WonderModel(W.Id);
		if (Kind && SovArt::SetKitMesh(C, TEXT("Wonders"), Kind, FLinearColor::White))
		{
			ShadeKit(C, W.bComplete ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f));
			C->SetRelativeLocation(At);
			C->SetRelativeScale3D(FVector(W.bComplete ? 0.1 : 0.07));
			C->SetRelativeRotation(FRotator::ZeroRotator);
			continue;
		}
		if (SovArt::SetKitMesh(C, TEXT("Classical"), W.bComplete ? TEXT("Temple") : TEXT("Monument"), FLinearColor::White))
		{
			ShadeKit(C, W.bComplete ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f));
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
	// Tribal villages (01): the Fields kit's camp, or a small hut.
	int32 VillageCount = 0;
	for (const FIntPoint& V : Mirror.Villages)
	{
		UStaticMeshComponent* C = Marker(VillagePieces, VillageCount++, CubeMesh.Get());
		if (SovArt::SetKitMesh(C, TEXT("Fields"), TEXT("Camp"), FLinearColor::White))
		{
			ShadeKit(C, FLinearColor::White);
			C->SetRelativeLocation(SovHex::Center(V.X, V.Y, SurfaceZ(V.X, V.Y)));
			C->SetRelativeRotation(FRotator(0.f, 30.f, 0.f));
			C->SetRelativeScale3D(FVector(0.09));
			continue;
		}
		C->SetRelativeLocation(SovHex::Center(V.X, V.Y, SurfaceZ(V.X, V.Y)) + FVector(0, 0, 10));
		C->SetRelativeRotation(FRotator(0.f, 30.f, 0.f));
		C->SetRelativeScale3D(FVector(0.22, 0.22, 0.18));
		C->SetMaterial(0, MaterialFor(FLinearColor(0.55f, 0.4f, 0.22f)));
	}
	for (int32 i = VillageCount; i < VillagePieces.Num(); ++i)
	{
		VillagePieces[i]->SetVisibility(false);
	}
	// Antiquity sites (07): the Resources kit's blocks, pale like old ruins, or a pale stone.
	int32 SiteCount = 0;
	for (const FIntPoint& A : Mirror.Antiquity)
	{
		UStaticMeshComponent* C = Marker(AntiquityPieces, SiteCount++, CubeMesh.Get());
		if (SovArt::SetKitMesh(C, TEXT("Resources"), TEXT("Blocks"), FLinearColor(0.8f, 0.78f, 0.7f)))
		{
			ShadeKit(C, FLinearColor::White);
			C->SetRelativeLocation(SovHex::Center(A.X, A.Y, SurfaceZ(A.X, A.Y)) + FVector(18, 0, 0));
			C->SetRelativeRotation(FRotator(0.f, 15.f, 0.f));
			C->SetRelativeScale3D(FVector(0.09));
			continue;
		}
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
		const TPair<const TCHAR*, FString> Centre = SovArt::EraPiece(City.Era, City.bCapital ? TEXT("Palace") : TEXT("Landmark"), City.Civ);
		if (SovArt::SetKitMesh(C, Centre.Key, Centre.Value, City.Color))
		{
			C->SetRelativeLocation(SovHex::Center(City.X, City.Y, Z));
			C->SetRelativeScale3D(FVector(City.bCapital ? 0.045 : 0.06));
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

	// Houses round each city centre: one more for every two citizens, up to eight, placed by the city's hash.
	int32 HouseCount = 0;
	if (SovArt::Mesh(TEXT("Classical"), TEXT("House_A")))
	{
		for (const FSovCityMarker& City : Mirror.Cities)
		{
			const FVector At = SovHex::Center(City.X, City.Y, SurfaceZ(City.X, City.Y));
			const uint32 H = static_cast<uint32>(City.X * 2654435761u) ^ static_cast<uint32>(City.Y * 40503);
			const int32 Count = FMath::Clamp(1 + City.Population / 2, 1, 8);
			for (int32 k = 0; k < Count; ++k)
			{
				// Round the ring, filling it in a scattered order; the front (toward the camera, +Y) stays clear.
				const double A = PI * (0.78 + 1.44 * ((k * 3) % 8) / 7.0) + ((H >> k) % 7) * 0.03;
				const double R = 64.0 + ((H >> (k + 3)) % 4) * 3.0;
				UStaticMeshComponent* C = Marker(CityHouses, HouseCount++, nullptr);
				static const TCHAR* Houses[] = {TEXT("House_A"), TEXT("House_B"), TEXT("House_C")};
				const TPair<const TCHAR*, FString> House = SovArt::EraPiece(City.Era, Houses[(H + k) % 3], City.Civ);
				SovArt::SetKitMesh(C, House.Key, House.Value, FLinearColor::White);
				ShadeKit(C, FLinearColor::White);
				C->SetRelativeLocation(At + SovHex::ToWorld(FVector2D(FMath::Cos(A), FMath::Sin(A)) * R, 0.0));
				C->SetRelativeRotation(FRotator(0.f, static_cast<float>(FMath::RadiansToDegrees(A)) + 90.f, 0.f));
				C->SetRelativeScale3D(FVector(0.034));
			}
		}
	}
	for (int32 i = HouseCount; i < CityHouses.Num(); ++i)
	{
		CityHouses[i]->SetVisibility(false);
	}

	// Districts: the kit's model with the owner's pennant; smaller and greyer while being built, browner when pillaged.
	int32 DistrictCount = 0;
	for (const FSovDistrictMarker& D : Mirror.Districts)
	{
		const FString Model = DistrictModel(D.Type);
		if (!SovArt::Mesh(TEXT("Districts"), Model))
		{
			continue;
		}
		UStaticMeshComponent* C = Marker(DistrictPieces, DistrictCount++, nullptr);
		SovArt::SetKitMesh(C, TEXT("Districts"), Model, D.Color);
		ShadeKit(C, D.bPillaged ? FLinearColor(0.45f, 0.3f, 0.25f) : D.bComplete ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f));
		C->SetRelativeLocation(SovHex::Center(D.X, D.Y, SurfaceZ(D.X, D.Y)));
		C->SetRelativeRotation(FRotator::ZeroRotator);
		C->SetRelativeScale3D(FVector(D.bComplete ? 0.1 : 0.075));
	}
	for (int32 i = DistrictCount; i < DistrictPieces.Num(); ++i)
	{
		DistrictPieces[i]->SetVisibility(false);
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
		// Its arm's figure (FSovUnitMarker::Figure); the vehicles, several metres long, are drawn smaller.
		const FString FigureName = Unit.Figure.IsNone() ? FString(Unit.bNaval ? TEXT("Ship") : Unit.bLeader ? TEXT("Leader") : Unit.bCivilian ? TEXT("Citizen") : TEXT("Soldier"))
			: Unit.Figure.ToString();
		const bool bFigureOk = SovArt::SetKitMesh(C, TEXT("Figures"), FigureName, Unit.Color)
			|| SovArt::SetKitMesh(C, TEXT("Figures"), Unit.bNaval ? TEXT("Ship") : Unit.bCivilian ? TEXT("Citizen") : TEXT("Soldier"), Unit.Color);
		const double Vehicle = FigureName == TEXT("Tank") ? 0.13 : FigureName == TEXT("Plane") ? 0.12 : FigureName == TEXT("Siege") ? 0.2
			: FigureName == TEXT("Rider") ? 0.22 : FigureName == TEXT("Steamship") ? 0.13 : 0.0;
		if (bFigureOk)
		{
			if (Unit.bNaval || Vehicle > 0.0)
			{
				C->SetRelativeLocation(FVector(Pos.X, Pos.Y, SurfaceZ(Unit.X, Unit.Y) + (FigureName == TEXT("Plane") ? 25.0 : 0.0)));
				C->SetRelativeScale3D(FVector(Vehicle > 0.0 ? Vehicle : 0.14));
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
	for (ASovMapActor* Ghost : Ghosts) Ghost->SetHighlight(X, Y);
	if (X < 0)
	{
		Highlight->SetVisibility(false);
		return;
	}
	Highlight->SetMaterial(0, MaterialFor(FLinearColor(1.f, 0.85f, 0.1f)));
	Highlight->SetRelativeLocation(SovHex::Center(X, Y, SurfaceZ(X, Y) + 2.0));
	Highlight->SetVisibility(true);
}
