#include "SovMirror.h"

#include "sovereign/game.h"

namespace
{
FLinearColor Srgb(uint8 R, uint8 G, uint8 B)
{
	return FLinearColor::FromSRGBColor(FColor(R, G, B));
}

// Plain-map palette keyed by the terrain's climate base (presentation only).
FLinearColor BaseColor(const std::string& Base)
{
	if (Base == "GRASSLAND") return Srgb(86, 140, 58);
	if (Base == "PLAINS") return Srgb(166, 160, 84);
	if (Base == "DESERT") return Srgb(222, 200, 132);
	if (Base == "TUNDRA") return Srgb(132, 128, 108);
	if (Base == "SNOW") return Srgb(236, 240, 244);
	if (Base == "COAST") return Srgb(64, 132, 186);
	if (Base == "OCEAN") return Srgb(26, 62, 128);
	return Srgb(255, 0, 255);
}

// Features tint the plot towards their own colour.
bool FeatureTint(const std::string& Id, FLinearColor& Tint, float& Amount)
{
	struct FEntry
	{
		const char* Prefix;
		uint8 R, G, B;
		float Amount;
	};
	static const FEntry Entries[] = {
		{"FEATURE_FOREST", 34, 84, 34, 0.6f},
		{"FEATURE_JUNGLE", 20, 104, 52, 0.65f},
		{"FEATURE_MARSH", 80, 120, 100, 0.5f},
		{"FEATURE_FLOODPLAINS", 150, 160, 80, 0.3f},
		{"FEATURE_OASIS", 60, 160, 140, 0.5f},
		{"FEATURE_ICE", 230, 240, 250, 0.85f},
		{"FEATURE_REEF", 80, 170, 180, 0.5f},
		{"FEATURE_VOLCAN", 60, 40, 36, 0.6f},
		{"FEATURE_GEOTHERMAL", 150, 110, 90, 0.4f},
		{"FEATURE_BURN", 70, 50, 40, 0.5f},
	};
	for (const FEntry& E : Entries)
	{
		if (Id.rfind(E.Prefix, 0) == 0)
		{
			Tint = Srgb(E.R, E.G, E.B);
			Amount = E.Amount;
			return true;
		}
	}
	return false;
}

FLinearColor PlotColor(const sov::Rules& Rules, const sov::Plot& Plot, ESovRelief& OutRelief)
{
	const sov::TerrainType& T = Rules.terrains[static_cast<size_t>(Plot.terrain)];
	FLinearColor C = BaseColor(T.base);
	if (T.water)
	{
		OutRelief = ESovRelief::Water;
	}
	else if (T.relief == sov::Relief::Mountain)
	{
		OutRelief = ESovRelief::Mountain;
		C = FLinearColor::LerpUsingHSV(C, Srgb(110, 104, 100), 0.65f);
	}
	else if (T.relief == sov::Relief::Hills)
	{
		OutRelief = ESovRelief::Hills;
		C *= 0.85f;
	}
	else
	{
		OutRelief = ESovRelief::Flat;
	}
	if (Plot.feature != sov::kNone)
	{
		FLinearColor Tint;
		float Amount = 0.f;
		if (FeatureTint(Rules.features[static_cast<size_t>(Plot.feature)].id, Tint, Amount))
		{
			C = FMath::Lerp(C, Tint, Amount);
		}
		else if (Rules.features[static_cast<size_t>(Plot.feature)].naturalWonder)
		{
			C = FMath::Lerp(C, Srgb(214, 175, 72), 0.6f);  // natural wonders (01): a golden landmark
		}
	}
	C.A = 1.f;
	return C;
}
}  // namespace

FLinearColor SovPlotColor(const sov::Game& Game, int32 X, int32 Y, bool* bWoods)
{
	const sov::Plot& Plot = Game.state().plot(sov::Hex{X, Y});
	ESovRelief Relief = ESovRelief::Flat;
	if (bWoods)
	{
		const std::string Id = Plot.feature == sov::kNone ? std::string() : Game.rules().features[static_cast<size_t>(Plot.feature)].id;
		*bWoods = Id.rfind("FEATURE_FOREST", 0) == 0 || Id.rfind("FEATURE_JUNGLE", 0) == 0;
	}
	return PlotColor(Game.rules(), Plot, Relief);
}

FLinearColor SovPlayerColor(const sov::Game& Game, int32 Player)
{
	const sov::GameState& S = Game.state();
	if (Player >= 0 && Player < static_cast<int32>(S.players.size()) && S.players[static_cast<size_t>(Player)].freeCity)
	{
		return Srgb(170, 170, 170);  // the Free Cities
	}
	if (Player >= 0 && Player < static_cast<int32>(S.players.size()) && S.players[static_cast<size_t>(Player)].barbarian)
	{
		return Srgb(40, 40, 40);
	}
	if (Player >= 0 && Player < static_cast<int32>(S.players.size()) && S.players[static_cast<size_t>(Player)].cityState != sov::kNone)
	{
		// City-states by kind (08): pale versions of Civ's type colours.
		static const FColor Kinds[] = {FColor(120, 160, 230), FColor(190, 130, 220), FColor(235, 235, 235), FColor(230, 210, 110),
			FColor(230, 160, 90), FColor(210, 100, 100)};
		const sov::TypeIndex Cs = S.players[static_cast<size_t>(Player)].cityState;
		const FColor K = Kinds[static_cast<size_t>(Game.rules().cityStates[static_cast<size_t>(Cs)].kind) % 6];
		return Srgb(K.R, K.G, K.B);
	}
	static const FColor Palette[] = {
		FColor(220, 40, 40), FColor(40, 90, 220), FColor(240, 200, 30), FColor(150, 50, 200),
		FColor(240, 130, 20), FColor(30, 190, 190), FColor(240, 110, 180), FColor(255, 255, 255),
		FColor(110, 200, 60), FColor(120, 70, 30), FColor(20, 60, 60), FColor(180, 180, 255),
	};
	return FLinearColor::FromSRGBColor(Palette[static_cast<uint32>(Player) % UE_ARRAY_COUNT(Palette)]);
}

FSovMirror BuildMirror(const sov::Game& Game, int32 Viewer)
{
	const sov::GameState& S = Game.state();
	const sov::Rules& Rules = Game.rules();
	const sov::PlayerId View = static_cast<sov::PlayerId>(Viewer);
	FSovMirror M;
	M.Width = S.grid.width();
	M.Height = S.grid.height();
	M.Viewer = Viewer;

	M.Tiles.Reserve(S.grid.size());
	for (int32 i = 0; i < S.grid.size(); ++i)
	{
		const sov::Hex H = S.grid.at(i);
		const sov::Visibility V = Game.visibility(View, H);
		if (V == sov::Visibility::Unrevealed)
		{
			continue;
		}
		FSovTile& T = M.Tiles.AddDefaulted_GetRef();
		T.X = H.x;
		T.Y = H.y;
		T.bVisible = V == sov::Visibility::Visible;
		T.Color = PlotColor(Rules, S.plots[static_cast<size_t>(i)], T.Relief);
		const sov::Plot& Pl = S.plots[static_cast<size_t>(i)];
		if (Pl.feature != sov::kNone)
		{
			const std::string& Id = Rules.features[static_cast<size_t>(Pl.feature)].id;
			T.bWoods = Id.rfind("FEATURE_FOREST", 0) == 0 || Id.rfind("FEATURE_JUNGLE", 0) == 0;
		}
		// Roads (01: Routes) to the east, south-east and south-west neighbours, so each joint is listed once.
		if (Pl.route >= 0)
		{
			for (int32 D = 0; D < sov::kNumDirs; ++D)
			{
				const std::optional<sov::Hex> N = S.grid.neighbor(H, static_cast<sov::Dir>(D));
				if (!N || S.grid.index(*N) < i || S.plot(*N).route < 0 || Game.visibility(View, *N) == sov::Visibility::Unrevealed)
				{
					continue;
				}
				M.Roads.Add({FIntPoint(H.x, H.y), FIntPoint(N->x, N->y)});
			}
		}
	}

	for (const sov::City& C : S.cities)
	{
		if (Game.visibility(View, C.pos) == sov::Visibility::Unrevealed)
		{
			continue;
		}
		FSovCityMarker& Marker = M.Cities.AddDefaulted_GetRef();
		Marker.Id = C.id;
		Marker.X = C.pos.x;
		Marker.Y = C.pos.y;
		Marker.Owner = C.owner;
		Marker.Color = SovPlayerColor(Game, C.owner);
		Marker.Name = UTF8_TO_TCHAR(C.name.c_str());
		Marker.Population = C.population;
		Marker.Hp = C.hp;
		Marker.MaxHp = Game.cityMaxHp();
		Marker.bCapital = C.capital;
		Marker.Loyalty = C.loyalty;
		for (const sov::CityWonder& W : C.wonders)
		{
			if (Game.visibility(View, W.pos) == sov::Visibility::Unrevealed)
			{
				continue;
			}
			FSovWonderMarker& WM = M.Wonders.AddDefaulted_GetRef();
			WM.X = W.pos.x;
			WM.Y = W.pos.y;
			WM.bComplete = C.has(W.building);
			WM.Name = UTF8_TO_TCHAR(Rules.buildings[static_cast<size_t>(W.building)].name.c_str());
		}
	}

	for (int32 I = 0; I < S.grid.size(); ++I)
	{
		const sov::Hex H = S.grid.at(I);
		if (S.plot(H).village && Game.visibility(View, H) != sov::Visibility::Unrevealed) M.Villages.Add(FIntPoint(H.x, H.y));
	}

	for (const sov::Unit& U : S.units)
	{
		if (U.owner != View && Game.visibility(View, U.pos) != sov::Visibility::Visible)
		{
			continue;
		}
		const sov::UnitType& Type = Rules.units[static_cast<size_t>(U.type)];
		FSovUnitMarker& Marker = M.Units.AddDefaulted_GetRef();
		Marker.Id = U.id;
		Marker.X = U.pos.x;
		Marker.Y = U.pos.y;
		Marker.Owner = U.owner;
		Marker.Color = SovPlayerColor(Game, U.owner);
		Marker.bLeader = Type.layer == sov::UnitLayer::Leader;
		Marker.bCivilian = Type.layer == sov::UnitLayer::Civilian || Type.layer == sov::UnitLayer::Support;
		Marker.bInCity = S.cityAt(U.pos) != nullptr;
		Marker.bNaval = Type.domain == sov::Domain::Sea;
		Marker.bEmbarked = Game.isEmbarked(U);
		Marker.Hp = U.hp;
		Marker.Name = UTF8_TO_TCHAR(Type.name.c_str());
		if (Marker.bLeader)
		{
			Marker.Name = UTF8_TO_TCHAR(S.players[static_cast<size_t>(U.owner)].leaderName.c_str());
		}
	}
	return M;
}
