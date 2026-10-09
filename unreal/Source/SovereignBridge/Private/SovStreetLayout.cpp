#include "SovStreetLayout.h"

#include "Math/RandomStream.h"

#include "SovMirror.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S)
{
	return UTF8_TO_TCHAR(S.c_str());
}

// Landmark recipe per building (world doc, "How the rules pick pieces"); the rest share one hall.
FString LandmarkRecipe(const std::string& Id)
{
	if (Id == "BUILDING_PALACE") return TEXT("Palace");
	if (Id == "BUILDING_MONUMENT") return TEXT("Monument");
	if (Id == "BUILDING_GRANARY") return TEXT("Granary");
	if (Id == "BUILDING_SHRINE" || Id == "BUILDING_TEMPLE") return TEXT("Temple");
	return TEXT("Landmark");
}

bool InsideHex(const FVector2D& P, double Radius)
{
	// Pointy-top hex: inside when |y| <= r and the slanted edges hold.
	const double AX = FMath::Abs(P.X), AY = FMath::Abs(P.Y);
	const double Inner = Radius * 0.8660254037844386;
	return AX <= Inner && AY <= Radius - AX * 0.5773502691896258;
}

// Distance from a point to the segment a-b.
double SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
	const FVector2D AB = B - A;
	const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
	return FVector2D::Distance(P, A + AB * T);
}
}  // namespace

int32 FSovStreetLayout::Count(ESovStreetPiece Kind) const
{
	return Pieces.FilterByPredicate([Kind](const FSovStreetPiece& P) { return P.Kind == Kind; }).Num();
}

FSovStreetLayout BuildStreetLayout(const sov::Game& Game, int32 CityId)
{
	FSovStreetLayout L;
	const sov::City* City = Game.state().city(CityId);
	if (!City)
	{
		return L;
	}
	const sov::Rules& R = Game.rules();
	L.CityId = CityId;
	L.CityName = Str(City->name);
	L.Era = Game.playerEra(City->owner);
	// Same hex, same result: the seed comes from the map seed and the hex.
	const uint64 Seed = Game.state().setup.seed * 1000003ull + static_cast<uint64>(City->pos.y) * 4099ull + static_cast<uint64>(City->pos.x);
	FRandomStream Rng(static_cast<int32>(Seed ^ (Seed >> 32)));

	const sov::CityReport Rep = Game.cityReport(CityId);
	const FString Mood = Str(R.happiness.empty() ? std::string() : R.happiness[static_cast<size_t>(Rep.happiness)].id);
	L.bFear = Game.fearActive(*City);
	L.Mood = L.bFear ? ESovStreetMood::Fear
		: (Mood == TEXT("HAPPINESS_HAPPY") || Mood == TEXT("HAPPINESS_ECSTATIC")) ? ESovStreetMood::Happy
		: (Mood == TEXT("HAPPINESS_UNHAPPY") || Mood == TEXT("HAPPINESS_UNREST") || Mood == TEXT("HAPPINESS_REVOLT")) ? ESovStreetMood::Unhappy
		: ESovStreetMood::Content;
	const FLinearColor Civ = SovPlayerColor(Game, City->owner);
	L.CivColor = Civ;
	const double Rad = L.Radius;

	// Plaza at the centre.
	const double PlazaRadius = 1800.0;
	L.Pieces.Add({ESovStreetPiece::Plaza, FVector(0, 0, 2), FVector(PlazaRadius * 2, PlazaRadius * 2, 4), 0.f, FLinearColor(0.55f, 0.5f, 0.42f)});

	// Main streets: from the plaza to the middle of each hex edge.
	TArray<TPair<FVector2D, FVector2D>> Streets;
	for (int32 i = 0; i < 6; ++i)
	{
		const double Angle = FMath::DegreesToRadians(60.0 * i);
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D A = Dir * PlazaRadius * 0.9, B = Dir * Rad * 0.86;
		Streets.Add({A, B});
		const FVector2D Mid = (A + B) * 0.5;
		L.Pieces.Add({ESovStreetPiece::Street, FVector(Mid.X, Mid.Y, 1), FVector((B - A).Size(), 500, 2), static_cast<float>(60.0 * i),
			FLinearColor(0.4f, 0.37f, 0.33f)});
	}

	// Landmarks: every building the city has, around the plaza (stage 4, "Landmarks").
	const int32 Buildings = static_cast<int32>(City->buildings.size());
	for (int32 i = 0; i < Buildings; ++i)
	{
		const sov::BuildingType& B = R.buildings[static_cast<size_t>(City->buildings[static_cast<size_t>(i)])];
		const double Angle = FMath::DegreesToRadians(30.0 + 360.0 * i / FMath::Max(Buildings, 1));
		const bool bPalace = B.granted;
		const FVector2D At = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (PlazaRadius + (bPalace ? 1200.0 : 900.0));
		const FVector Size = bPalace ? FVector(1400, 1000, 1100) : FVector(900, 700, 600 + 100 * (i % 3));
		// Kit landmarks face the plaza (their door is on -Y): yaw so -Y points at the centre.
		const float Facing = static_cast<float>(FMath::RadiansToDegrees(Angle)) + 90.f;
		FSovStreetPiece Piece{ESovStreetPiece::Landmark, FVector(At.X, At.Y, Size.Z * 0.5), Size, Facing,
			bPalace ? Civ : FLinearColor(0.78f, 0.74f, 0.66f), Str(B.name), LandmarkRecipe(B.id)};
		L.Pieces.Add(Piece);
		if (bPalace)
		{
			// The ruler's banners flank the Palace in the owner's colour.
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const FVector2D Off = At + FVector2D(FMath::Cos(Angle + Side * 0.5), FMath::Sin(Angle + Side * 0.5)) * 300.0 -
									  FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 900.0;
				L.Pieces.Add({ESovStreetPiece::Banner, FVector(Off.X, Off.Y, 350), FVector(30, 30, 700), Facing, Civ, FString(), TEXT("Banner")});
			}
		}
	}

	// Filler houses by population (stage 4, "Filler"), kept off the streets and the plaza.
	const int32 Houses = 10 + City->population * 8;
	int32 Placed = 0;
	for (int32 Tries = 0; Tries < Houses * 30 && Placed < Houses; ++Tries)
	{
		const FVector2D P(Rng.FRandRange(-Rad, Rad), Rng.FRandRange(-Rad, Rad));
		if (!InsideHex(P, Rad * 0.92) || P.Size() < PlazaRadius + 1500.0)
		{
			continue;
		}
		bool bClear = true;
		for (const TPair<FVector2D, FVector2D>& S : Streets)
		{
			bClear &= SegmentDistance(P, S.Key, S.Value) > 650.0;
		}
		for (const FSovStreetPiece& O : L.Pieces)
		{
			if (O.Kind == ESovStreetPiece::House && FVector2D::Distance(P, FVector2D(O.Location)) < 700.0)
			{
				bClear = false;
			}
		}
		if (!bClear)
		{
			continue;
		}
		const double H = Rng.FRandRange(300.0, 520.0);
		const bool bBoarded = L.Mood == ESovStreetMood::Unhappy && Rng.FRand() < 0.35f;
		const float Shade = Rng.FRandRange(0.75f, 0.95f);
		static const TCHAR* HouseKinds[] = {TEXT("House_A"), TEXT("House_B"), TEXT("House_C")};
		const FString Recipe = bBoarded ? FString(TEXT("House_Boarded")) : FString(HouseKinds[Rng.RandRange(0, 2)]);
		// Houses turn their fronts toward the plaza, with a little scatter.
		const float Facing = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(P.Y, P.X))) + 90.f + Rng.FRandRange(-15.f, 15.f);
		L.Pieces.Add({bBoarded ? ESovStreetPiece::Boarded : ESovStreetPiece::House, FVector(P.X, P.Y, H * 0.5),
			FVector(Rng.FRandRange(450.0, 600.0), Rng.FRandRange(400.0, 550.0), H), Facing,
			bBoarded ? FLinearColor(0.25f, 0.2f, 0.17f) : FLinearColor(Shade, Shade * 0.9f, Shade * 0.78f), FString(), Recipe});
		++Placed;
	}

	// Walls: a ring of wall sections along the hex edge when the city has walls.
	for (sov::TypeIndex B : City->buildings)
	{
		L.bWalls |= R.buildings[static_cast<size_t>(B)].outerDefenseHp > 0;
	}
	if (L.bWalls)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const double A0 = FMath::DegreesToRadians(60.0 * i + 30.0), A1 = FMath::DegreesToRadians(60.0 * i + 90.0);
			const FVector2D P0 = FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Rad * 0.95;
			const FVector2D P1 = FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Rad * 0.95;
			const FVector2D Mid = (P0 + P1) * 0.5;
			const FVector2D D = P1 - P0;
			L.Pieces.Add({ESovStreetPiece::Wall, FVector(Mid.X, Mid.Y, 400), FVector(D.Size(), 250, 800),
				static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))), FLinearColor(0.5f, 0.48f, 0.45f), FString(), TEXT("Wall")});
		}
	}

	// Mood props (stage 5): banners when happy, guards at the street mouths under Fear.
	for (int32 i = 0; i < 6; ++i)
	{
		const double Angle = FMath::DegreesToRadians(60.0 * i + 15.0);
		const FVector2D At = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * PlazaRadius;
		if (L.Mood == ESovStreetMood::Happy)
		{
			L.Pieces.Add({ESovStreetPiece::Banner, FVector(At.X, At.Y, 350), FVector(30, 30, 700), 0.f, Civ, FString(), TEXT("Banner")});
			// Market stalls on the plaza when the city is happy (stage 5).
			const FVector2D Stall = At * 0.7;
			L.Pieces.Add({ESovStreetPiece::Market, FVector(Stall.X, Stall.Y, 120), FVector(260, 140, 240),
				static_cast<float>(60.0 * i), FLinearColor(0.6f, 0.4f, 0.25f), FString(), TEXT("MarketStall")});
		}
		if (L.bFear)
		{
			L.Pieces.Add({ESovStreetPiece::Guard, FVector(At.X, At.Y, 90), FVector(60, 60, 180), 0.f, FLinearColor(0.3f, 0.05f, 0.05f)});
		}
	}

	// Trees and bushes in the open ground near the hex edge (nature kit).
	for (int32 Tries = 0, Trees = 0; Tries < 400 && Trees < 14; ++Tries)
	{
		const FVector2D P(Rng.FRandRange(-Rad, Rad), Rng.FRandRange(-Rad, Rad));
		if (!InsideHex(P, Rad * 0.9) || P.Size() < Rad * 0.62)
		{
			continue;
		}
		bool bClear = true;
		for (const TPair<FVector2D, FVector2D>& S : Streets)
		{
			bClear &= SegmentDistance(P, S.Key, S.Value) > 700.0;
		}
		for (const FSovStreetPiece& O : L.Pieces)
		{
			bClear &= O.Kind != ESovStreetPiece::House || FVector2D::Distance(P, FVector2D(O.Location)) > 650.0;
		}
		if (!bClear)
		{
			continue;
		}
		static const TCHAR* Greens[] = {TEXT("Tree_Broadleaf"), TEXT("Tree_Conifer"), TEXT("Bush")};
		L.Pieces.Add({ESovStreetPiece::Tree, FVector(P.X, P.Y, 250), FVector(300, 300, 500), static_cast<float>(Rng.FRandRange(0.f, 360.f)),
			FLinearColor(0.2f, 0.4f, 0.2f), FString(), Greens[Rng.RandRange(0, 2)]});
		++Trees;
	}

	L.Crowd = 6 + City->population * 4;
	L.Herald = FVector(PlazaRadius * 0.45, -PlazaRadius * 0.3, 0);
	L.Captain = FVector(-PlazaRadius * 0.45, PlazaRadius * 0.3, 0);
	L.Entry = FVector(-PlazaRadius * 1.6, 0, 0);
	return L;
}
