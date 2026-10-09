#include "SovMirror.h"

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

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
	M.bWrap = S.grid.wrapX();
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
		{
			const std::string& Ground = Rules.terrains[static_cast<size_t>(Pl.terrain)].id;
			auto Has = [&](const char* Part) { return Ground.find(Part) != std::string::npos; };
			T.Detail = T.Relief == ESovRelief::Water ? 12 : T.Relief != ESovRelief::Flat ? 7 : T.bWoods ? 10 : Has("DESERT") ? 14
				: Has("GRASS") || Has("PLAINS") ? 13 : 8;
		}
		// Territory, the resource the viewer can see, and the improvement.
		T.Owner = Pl.owner;
		if (Pl.resource != sov::kNone && Game.resourceVisible(View, H))
		{
			const sov::ResourceClass Cls = Rules.resources[static_cast<size_t>(Pl.resource)].cls;
			T.ResourceClass = Cls == sov::ResourceClass::Luxury ? 2 : Cls == sov::ResourceClass::Strategic ? 3 : 1;
			T.Resource = UTF8_TO_TCHAR(Rules.resources[static_cast<size_t>(Pl.resource)].id.c_str());
		}
		T.bImproved = Pl.improvement != sov::kNone;
		T.bPillaged = T.bImproved && Pl.pillagedTurns > 0;
		if (T.bImproved) T.Improvement = UTF8_TO_TCHAR(Rules.improvements[static_cast<size_t>(Pl.improvement)].id.c_str());
		for (int32 D = 0; D < sov::kNumDirs; ++D)
		{
			const std::optional<sov::Hex> N = S.grid.neighbor(H, static_cast<sov::Dir>(D));
			if (!N)
			{
				continue;
			}
			const bool bKnown = Game.visibility(View, *N) != sov::Visibility::Unrevealed;
			// Rivers on the edges a plot owns (E, SE, SW), so each is listed once.
			if ((D == static_cast<int32>(sov::Dir::E) || D == static_cast<int32>(sov::Dir::SE) || D == static_cast<int32>(sov::Dir::SW)) && bKnown &&
				sov::hasRiver(S, H, static_cast<sov::Dir>(D)))
			{
				M.Rivers.Add({FIntPoint(H.x, H.y), FIntPoint(N->x, N->y), Srgb(70, 150, 230)});
			}
			// A border where the territory ends.
			if (Pl.owner != sov::kNoPlayer && (!bKnown || S.plot(*N).owner != Pl.owner))
			{
				M.Borders.Add({FIntPoint(H.x, H.y), FIntPoint(N->x, N->y), SovPlayerColor(Game, Pl.owner)});
			}
		}
		// Roads (01: Routes) to the east, south-east and south-west neighbours, so each joint is listed once.
		// A pillaged road is not drawn until it is repaired (05: Pillage).
		if (Pl.route >= 0 && !Pl.routePillaged)
		{
			for (int32 D = 0; D < sov::kNumDirs; ++D)
			{
				const std::optional<sov::Hex> N = S.grid.neighbor(H, static_cast<sov::Dir>(D));
				if (!N || S.grid.index(*N) < i || S.plot(*N).route < 0 || S.plot(*N).routePillaged || Game.visibility(View, *N) == sov::Visibility::Unrevealed)
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
			WM.Id = UTF8_TO_TCHAR(Rules.buildings[static_cast<size_t>(W.building)].id.c_str());
		}
		for (const sov::CityDistrict& D : C.districts)
		{
			const std::string& Type = Rules.districts[static_cast<size_t>(D.type)].id;
			if (D.pos == C.pos || Type == "DISTRICT_CITY_CENTER" || Game.visibility(View, D.pos) == sov::Visibility::Unrevealed)
			{
				continue;
			}
			FSovDistrictMarker& DM = M.Districts.AddDefaulted_GetRef();
			DM.X = D.pos.x;
			DM.Y = D.pos.y;
			DM.Type = UTF8_TO_TCHAR(Type.c_str());
			DM.Color = Marker.Color;
			DM.bComplete = D.complete;
			DM.bPillaged = D.pillagedTurns > 0;
		}
	}

	for (int32 I = 0; I < S.grid.size(); ++I)
	{
		const sov::Hex H = S.grid.at(I);
		if (S.plot(H).village && Game.visibility(View, H) != sov::Visibility::Unrevealed) M.Villages.Add(FIntPoint(H.x, H.y));
		if (S.plot(H).antiquity && Game.seesAntiquity(View) && Game.visibility(View, H) != sov::Visibility::Unrevealed) M.Antiquity.Add(FIntPoint(H.x, H.y));
	}

	for (const sov::Unit& U : S.units)
	{
		if (!Game.unitVisibleTo(View, U))  // in sight, and stealthy ships only when found (05)
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
		{
			// The flag's icon, by what the unit does.
			const std::string& Cls = Type.unitClass;
			Marker.Icon = Type.foundCity ? FName("found")
				: Type.id.find("GREAT_") != std::string::npos ? FName("greatperson")
				: Type.id == "UNIT_TRADER" ? FName("trade")
				: Type.religiousStrength > 0 || Cls.rfind("RELIGIOUS", 0) == 0 ? FName("religion")
				: Type.buildCharges > 0 ? FName("build")
				: Cls == "ROCK_BAND" ? FName("tourism")
				: Cls == "CIVILIAN" || Cls == "ESPIONAGE" ? FName("culture")
				: Cls == "RANGED" || Cls == "SIEGE" || Cls == "NAVAL_RANGED" || Cls.rfind("AIR_", 0) == 0 ? FName("ranged")
				: Cls == "RECON" ? FName("moves")
				: Cls == "SUPPORT" ? FName("fortify")
				: FName("attack");
		}
		if (Marker.bLeader)
		{
			Marker.Name = UTF8_TO_TCHAR(S.players[static_cast<size_t>(U.owner)].leaderName.c_str());
		}
	}
	return M;
}

TArray<FString> SovPlotTooltip(const sov::Game& Game, int32 Viewer, int32 X, int32 Y)
{
	TArray<FString> Out;
	const sov::GameState& S = Game.state();
	const sov::Rules& R = Game.rules();
	const sov::Hex H{X, Y};
	if (!S.grid.normalize(H) || Game.visibility(static_cast<sov::PlayerId>(Viewer), H) == sov::Visibility::Unrevealed)
	{
		return Out;
	}
	const sov::Plot& P = S.plot(H);
	auto Str = [](const std::string& V) { return FString(UTF8_TO_TCHAR(V.c_str())); };
	FString Land = Str(R.terrains[static_cast<size_t>(P.terrain)].name);
	if (P.feature != sov::kNone)
	{
		Land += TEXT(", ") + Str(R.features[static_cast<size_t>(P.feature)].name);
	}
	bool bRiver = false;
	for (int32 D = 0; D < sov::kNumDirs; ++D)
	{
		bRiver = bRiver || sov::hasRiver(S, H, static_cast<sov::Dir>(D));
	}
	Out.Add(FString::Printf(TEXT("(%d,%d) %s%s"), X, Y, *Land, bRiver ? TEXT(", river") : TEXT("")));
	if (P.resource != sov::kNone && Game.resourceVisible(static_cast<sov::PlayerId>(Viewer), H))
	{
		const sov::ResourceType& Res = R.resources[static_cast<size_t>(P.resource)];
		const TCHAR* Cls = Res.cls == sov::ResourceClass::Luxury ? TEXT("luxury") : Res.cls == sov::ResourceClass::Strategic ? TEXT("strategic") : TEXT("bonus");
		Out.Add(FString::Printf(TEXT("%s (%s)"), *Str(Res.name), Cls));
	}
	if (P.improvement != sov::kNone)
	{
		Out.Add(Str(R.improvements[static_cast<size_t>(P.improvement)].name) + (P.pillagedTurns > 0 ? TEXT(" (pillaged)") : TEXT("")));
	}
	if (const sov::CityDistrict* Dist = S.districtAt(H))
	{
		Out.Add(Str(R.districts[static_cast<size_t>(Dist->type)].name) + (Dist->complete ? TEXT("") : TEXT(" (being built)")));
	}
	if (P.owner != sov::kNoPlayer)
	{
		const sov::Player& O = S.players[static_cast<size_t>(P.owner)];
		const FString Who = O.cityState != sov::kNone ? Str(R.cityStates[static_cast<size_t>(O.cityState)].name)
			: O.civ != sov::kNone ? Str(R.civs[static_cast<size_t>(O.civ)].name) : FString(TEXT("Free Cities"));
		const sov::City* C = P.city != sov::kNoCity ? S.city(P.city) : nullptr;
		Out.Add(C ? FString::Printf(TEXT("%s, %s"), *Str(C->name), *Who) : Who);
		if (C)
		{
			// What working it would bring that city.
			const sov::Yields Y2 = Game.plotYields(H, *C);
			static const TCHAR* Names[] = {TEXT("Food"), TEXT("Production"), TEXT("Gold"), TEXT("Science"), TEXT("Culture"), TEXT("Faith")};
			FString Yl;
			for (size_t i = 0; i < sov::kNumYields && i < UE_ARRAY_COUNT(Names); ++i)
			{
				const int32 V = static_cast<int32>(Y2[i].toInt());
				if (V != 0) Yl += FString::Printf(TEXT("%s%d %s"), Yl.IsEmpty() ? TEXT("") : TEXT(", "), V, Names[i]);
			}
			if (!Yl.IsEmpty()) Out.Add(Yl);
		}
	}
	// The units on it, where the viewer sees them now (plan E, step 3).
	if (Game.visibility(static_cast<sov::PlayerId>(Viewer), H) == sov::Visibility::Visible)
	{
		for (const sov::Unit& U : S.units)
		{
			if (U.pos.x != H.x || U.pos.y != H.y || U.type == sov::kNone) continue;
			const sov::UnitType& T = R.units[static_cast<size_t>(U.type)];
			const sov::Player& O = S.players[static_cast<size_t>(U.owner)];
			const FString Who = O.barbarian ? FString(TEXT("Barbarian")) : O.cityState != sov::kNone ? Str(R.cityStates[static_cast<size_t>(O.cityState)].name)
				: O.civ != sov::kNone ? Str(R.civs[static_cast<size_t>(O.civ)].name) : FString(TEXT("?"));
			FString Line = FString::Printf(TEXT("%s (%s), %d HP"), *Str(T.name), *Who, U.hp);
			if (T.combat > 0) Line += FString::Printf(TEXT(", strength %d"), Game.meleeStrength(U));
			if (T.ranged > 0) Line += FString::Printf(TEXT(", ranged %d"), T.ranged);
			Out.Add(Line);
		}
	}
	return Out;
}
