#include "SovDescribe.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

const TCHAR* YieldName(sov::YieldType Y)
{
	static const TCHAR* const Names[] = {TEXT("Food"), TEXT("Production"), TEXT("Gold"), TEXT("Science"), TEXT("Culture"), TEXT("Faith")};
	return Names[FMath::Clamp(static_cast<int32>(Y), 0, 5)];
}

// "+2", "-25" (whole numbers), "+0.5".
FString Amount(const sov::Fixed& V)
{
	FString S = Str(V.toString());
	return V >= sov::Fixed() ? TEXT("+") + S : S;
}

// "MELEE" -> "melee"; "ANTI_CAVALRY" -> "anti-cavalry".
FString ClassName(const std::string& Class)
{
	return Str(Class).ToLower().Replace(TEXT("_"), TEXT("-"));
}

FString Where(sov::ModCollection C)
{
	switch (C)
	{
		case sov::ModCollection::OwnerCity: return TEXT(" in the city");
		case sov::ModCollection::OwnerCityPlots: return TEXT(" on the city's plots");
		case sov::ModCollection::PlayerCities: return TEXT(" in every city");
		case sov::ModCollection::PlayerCapital: return TEXT(" in the capital");
		case sov::ModCollection::PlayerCityPlots: return TEXT(" on your cities' plots");
		default: return FString();
	}
}

// "a Granary", "an Encampment".
FString Article(const FString& Name)
{
	if (Name.EndsWith(TEXT("Walls"))) return Name;  // plural: "with Ancient Walls"
	return (Name.Len() > 0 && FString(TEXT("AEIOUaeiou")).Contains(Name.Left(1)) ? TEXT("an ") : TEXT("a ")) + Name;
}

// The conditions, briefly ("with a garrison", "next to a river"); unknown ones are left out.
FString When(const sov::Rules& R, const sov::RequirementSet& Set)
{
	TArray<FString> Parts;
	for (const sov::Requirement& Q : Set.reqs)
	{
		FString P;
		switch (Q.type)
		{
			case sov::ReqType::CityIsCapital: P = TEXT("the capital"); break;
			case sov::ReqType::CityHasGarrison: P = TEXT("a garrison"); break;
			case sov::ReqType::CityHasGovernor: P = TEXT("an established governor"); break;
			case sov::ReqType::CityHasBuilding:
				if (Q.ref >= 0 && static_cast<size_t>(Q.ref) < R.buildings.size()) P = Article(Str(R.buildings[static_cast<size_t>(Q.ref)].name));
				break;
			case sov::ReqType::CityHasDistrict:
				if (Q.ref >= 0 && static_cast<size_t>(Q.ref) < R.districts.size()) P = Article(Str(R.districts[static_cast<size_t>(Q.ref)].name));
				break;
			case sov::ReqType::CityMinPopulation: P = FString::Printf(TEXT("%d or more citizens"), Q.value); break;
			case sov::ReqType::CityMinSpecialtyDistricts: P = FString::Printf(TEXT("%d or more specialty districts"), Q.value); break;
			case sov::ReqType::PlayerAtPeace: P = TEXT("peace with every major civ"); break;
			case sov::ReqType::CityFullLoyalty: P = TEXT("full loyalty"); break;
			case sov::ReqType::CityIsCoastal: P = TEXT("a coast"); break;
			case sov::ReqType::PlotNextToRiver: P = TEXT("a river beside it"); break;
			case sov::ReqType::PlotHasImprovement:
				P = Q.ref >= 0 && static_cast<size_t>(Q.ref) < R.improvements.size() ? Article(Str(R.improvements[static_cast<size_t>(Q.ref)].name)) : FString(TEXT("an improvement"));
				break;
			case sov::ReqType::PlotHasResource:
				if (Q.ref >= 0 && static_cast<size_t>(Q.ref) < R.resources.size()) P = Str(R.resources[static_cast<size_t>(Q.ref)].name);
				break;
			default: break;
		}
		if (!P.IsEmpty()) Parts.Add((Q.negate ? TEXT("no ") : TEXT("")) + P);
	}
	return Parts.Num() == 0 ? FString() : TEXT(" with ") + FString::Join(Parts, Set.any ? TEXT(" or ") : TEXT(" and "));
}
}  // namespace

FString SovModifierText(const sov::Rules& R, const sov::Modifier& M)
{
	const FString A = Amount(M.amount);
	const FString In = Where(M.collection);
	const FString If = When(R, M.subjectReqs) + When(R, M.ownerReqs);
	auto DistrictName = [&]() { return M.district >= 0 && static_cast<size_t>(M.district) < R.districts.size() ? Str(R.districts[static_cast<size_t>(M.district)].name) : FString(TEXT("district")); };
	auto GpName = [&]() { return M.gpClass >= 0 && static_cast<size_t>(M.gpClass) < R.greatPersonClasses.size() ? Str(R.greatPersonClasses[static_cast<size_t>(M.gpClass)].name) : FString(TEXT("great person")); };
	const FString Units = M.unitClass.empty() ? FString(TEXT("units")) : ClassName(M.unitClass) + TEXT(" units");
	FString T;
	switch (M.effect)
	{
		case sov::ModEffect::CityYield: T = FString::Printf(TEXT("%s %s%s"), *A, YieldName(M.yield), *In); break;
		case sov::ModEffect::CityYieldPercent: T = FString::Printf(TEXT("%s%% %s%s"), *A, YieldName(M.yield), *In); break;
		case sov::ModEffect::PlotYield: T = FString::Printf(TEXT("%s %s on worked plots%s"), *A, YieldName(M.yield), *If); return T;
		case sov::ModEffect::CityHousing: T = FString::Printf(TEXT("%s Housing%s"), *A, *In); break;
		case sov::ModEffect::CityAmenities: T = FString::Printf(TEXT("%s Amenities%s"), *A, *In); break;
		case sov::ModEffect::CityGrowthPercent: T = FString::Printf(TEXT("%s%% growth%s"), *A, *In); break;
		case sov::ModEffect::CityDefense: T = FString::Printf(TEXT("%s city defense%s"), *A, *In); break;
		case sov::ModEffect::UnitProductionPercent:
			T = FString::Printf(TEXT("%s%% Production toward %s%s"), *A, M.military ? TEXT("military units") : *Units, *In);
			break;
		case sov::ModEffect::PlotPurchaseCostPercent: T = FString::Printf(TEXT("%s%% to the gold cost of tiles%s"), *A, *In); break;
		case sov::ModEffect::UnitMaintenanceDiscount: T = FString::Printf(TEXT("%s gold off each unit's upkeep"), *Amount(sov::Fixed() - M.amount)); break;
		case sov::ModEffect::GrantAbility:
			T = M.ability >= 0 && static_cast<size_t>(M.ability) < R.abilities.size() ? FString::Printf(TEXT("%s gain %s"), *Units, *Str(R.abilities[static_cast<size_t>(M.ability)].name))
																					   : Units + TEXT(" gain an ability");
			break;
		case sov::ModEffect::UnitXpPercent: T = FString::Printf(TEXT("%s%% combat experience for %s"), *A, *Units); break;
		case sov::ModEffect::UnitStrength: T = FString::Printf(TEXT("%s Combat Strength for %s%s"), *A, *Units, M.vsBarbarians ? TEXT(" against barbarians") : TEXT("")); break;
		case sov::ModEffect::DistrictAdjacencyPercent: T = FString::Printf(TEXT("%s%% adjacency for the %s"), *A, *DistrictName()); break;
		case sov::ModEffect::CityLoyalty: T = FString::Printf(TEXT("%s Loyalty a turn%s"), *A, *In); break;
		case sov::ModEffect::FounderYieldPerCity: T = FString::Printf(TEXT("%s %s for each city following your religion"), *A, YieldName(M.yield)); break;
		case sov::ModEffect::FounderYieldPerFollowers: T = FString::Printf(TEXT("%s %s per %d followers"), *A, YieldName(M.yield), M.per); break;
		case sov::ModEffect::FounderYieldPerDistrict: T = FString::Printf(TEXT("%s %s per %s in cities of your religion"), *A, YieldName(M.yield), *DistrictName()); break;
		case sov::ModEffect::ReligionPressureRange: T = FString::Printf(TEXT("%s tiles of religious pressure range"), *A); break;
		case sov::ModEffect::ReligionPressurePercent: T = FString::Printf(TEXT("%s%% religious pressure"), *A); break;
		case sov::ModEffect::ReligiousUnitDiscountPercent: T = FString::Printf(TEXT("%s%% off religious units"), *A); break;
		case sov::ModEffect::UnitStrengthNearFollowingCity: T = FString::Printf(TEXT("%s Combat Strength near %scities of your religion"), *A, M.foreign ? TEXT("foreign ") : TEXT("")); break;
		case sov::ModEffect::ReligiousUnitsIgnoreTerrain: T = TEXT("religious units ignore terrain"); break;
		case sov::ModEffect::NoCombatPressureLoss: T = TEXT("theological defeats cost no pressure"); break;
		case sov::ModEffect::ReligionColonizes: T = TEXT("new cities follow your religion"); break;
		case sov::ModEffect::CityYieldPerPop: T = FString::Printf(TEXT("%s %s per citizen%s"), *A, YieldName(M.yield), *In); break;
		case sov::ModEffect::CityYieldPerDistrict: T = FString::Printf(TEXT("%s %s per district%s"), *A, YieldName(M.yield), *In); break;
		case sov::ModEffect::CityGreatPersonPercent: T = FString::Printf(TEXT("%s%% great person points%s"), *A, *In); break;
		case sov::ModEffect::CityHarvestPercent: T = FString::Printf(TEXT("%s%% from harvests and chops%s"), *A, *In); break;
		case sov::ModEffect::CityBorderGrowthPercent: T = FString::Printf(TEXT("%s%% border growth%s"), *A, *In); break;
		case sov::ModEffect::CityDistrictProductionPercent: T = FString::Printf(TEXT("%s%% Production toward districts%s"), *A, *In); break;
		case sov::ModEffect::CityReligionPressurePercent: T = FString::Printf(TEXT("%s%% religious pressure%s"), *A, *In); break;
		case sov::ModEffect::SettlerNoPopCost: T = TEXT("Settlers cost no population"); break;
		case sov::ModEffect::BuilderExtraCharges: T = FString::Printf(TEXT("%s Builder charges"), *A); break;
		case sov::ModEffect::WarWearinessPercent: T = FString::Printf(TEXT("%s%% war weariness"), *A); break;
		case sov::ModEffect::TradeRouteYield:
		{
			const FString Scope = M.scope.empty() || M.scope == "ALL" ? FString(TEXT("")) : Str(M.scope).ToLower().Replace(TEXT("_"), TEXT("-")) + TEXT(" ");
			T = FString::Printf(TEXT("%s %s on %strade routes%s"), *A, YieldName(M.yield), *Scope, M.toDestination ? TEXT(" (to the destination)") : TEXT(""));
			break;
		}
		case sov::ModEffect::ItemProductionPercent:
		{
			FString What = Str(M.scope).ToLower().Replace(TEXT("_"), TEXT(" "));
			if (M.building >= 0 && static_cast<size_t>(M.building) < R.buildings.size()) What = Str(R.buildings[static_cast<size_t>(M.building)].name);
			else if (M.district >= 0) What = DistrictName();
			T = FString::Printf(TEXT("%s%% Production toward %s%s"), *A, *What, *In);
			break;
		}
		case sov::ModEffect::GreatPersonPoints:
		case sov::ModEffect::CityGreatPersonPoints: T = FString::Printf(TEXT("%s %s points a turn%s"), *A, *GpName(), *In); break;
		case sov::ModEffect::FavorPerTurn:
		case sov::ModEffect::CityFavorPerTurn: T = FString::Printf(TEXT("%s Diplomatic Favor a turn%s"), *A, *In); break;
		case sov::ModEffect::InfluencePerTurn: T = FString::Printf(TEXT("%s influence a turn toward envoys"), *A); break;
		case sov::ModEffect::RouteTourismPercent: T = FString::Printf(TEXT("%s%% tourism toward civs you trade with"), *A); break;
		case sov::ModEffect::DistrictTourism: T = FString::Printf(TEXT("%s Tourism from each %s"), *A, *DistrictName()); break;
		case sov::ModEffect::CityAppeal: T = FString::Printf(TEXT("%s Appeal%s"), *A, *In); break;
		case sov::ModEffect::CityTourism: T = FString::Printf(TEXT("%s Tourism%s"), *A, *In); break;
		case sov::ModEffect::EmbarkedMoves: T = FString::Printf(TEXT("%s movement when embarked"), *A); break;
		case sov::ModEffect::PurchaseDiscountPercent: T = FString::Printf(TEXT("%s%% off what %s buys"), *Amount(sov::Fixed() - M.amount), YieldName(M.yield)); break;
		default: T = Str(M.id); break;
	}
	return T + If;
}

FString SovSourceText(const sov::Rules& R, sov::ModSource Kind, sov::TypeIndex Index)
{
	// Phrases that differ only in the yield merge: "+1 Food, Production and Gold in every city with a Palace".
	static const TCHAR* const Yields[] = {TEXT("Food"), TEXT("Production"), TEXT("Gold"), TEXT("Science"), TEXT("Culture"), TEXT("Faith")};
	TArray<FString> Keys;               // in first-seen order: "amount|rest", or the whole phrase
	TMap<FString, TArray<FString>> Of;  // key -> yields
	for (const sov::Modifier& M : R.modifiers)
	{
		if (M.sourceKind != Kind || M.sourceIndex != Index) continue;
		const FString T = SovModifierText(R, M);
		if (T.IsEmpty()) continue;
		FString Key = T, Yield;
		int32 Space = INDEX_NONE;
		if (T.FindChar(TCHAR(' '), Space))
		{
			const FString Rest = T.RightChop(Space + 1);
			for (const TCHAR* Y : Yields)
			{
				const int32 N = FCString::Strlen(Y);
				if (Rest.StartsWith(Y) && (Rest.Len() == N || Rest[N] == TCHAR(' ')))
				{
					Yield = Y;
					Key = T.Left(Space) + TEXT("|") + Rest.RightChop(N);
					break;
				}
			}
		}
		if (!Of.Contains(Key)) Keys.Add(Key);
		TArray<FString>& Ys = Of.FindOrAdd(Key);
		if (!Yield.IsEmpty()) Ys.AddUnique(Yield);
	}
	TArray<FString> Parts;
	for (const FString& Key : Keys)
	{
		const TArray<FString>& Ys = Of[Key];
		FString AmountPart, Rest;
		if (Ys.Num() == 0 || !Key.Split(TEXT("|"), &AmountPart, &Rest))
		{
			Parts.Add(Key);
			continue;
		}
		FString List = Ys[0];
		for (int32 i = 1; i < Ys.Num(); ++i) List += (i + 1 == Ys.Num() ? TEXT(" and ") : TEXT(", ")) + Ys[i];
		Parts.Add(AmountPart + TEXT(" ") + List + Rest);
	}
	return FString::Join(Parts, TEXT("; "));
}
