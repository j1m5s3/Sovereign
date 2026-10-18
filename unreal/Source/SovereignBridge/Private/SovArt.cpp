#include "SovArt.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace
{
TMap<FString, TWeakObjectPtr<UStaticMesh>>& Cache()
{
	static TMap<FString, TWeakObjectPtr<UStaticMesh>> Meshes;
	return Meshes;
}

FString MeshPath(const TCHAR* Kit, const FString& Name)
{
	return FString::Printf(TEXT("/Game/Art/%s/SM_%s.SM_%s"), Kit, *Name, *Name);
}
}  // namespace

namespace SovArt
{
UStaticMesh* Mesh(const TCHAR* Kit, const FString& Name)
{
	const FString Path = MeshPath(Kit, Name);
	if (TWeakObjectPtr<UStaticMesh>* Found = Cache().Find(Path))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	Cache().Add(Path, Loaded);
	return Loaded;
}

UMaterialInterface* KitMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> Material;
	if (!Material.IsValid())
	{
		Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/M_SovKit.M_SovKit"), nullptr, LOAD_Quiet | LOAD_NoWarn);
	}
	return Material.Get();
}

UMaterialInstanceDynamic* Tinted(UObject* Outer, const FLinearColor& Tint)
{
	UMaterialInterface* Base = KitMaterial();
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
	Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
	return Mid;
}

bool SetKitMesh(UStaticMeshComponent* Component, const TCHAR* Kit, const FString& Name, const FLinearColor& Tint)
{
	UStaticMesh* M = Mesh(Kit, Name);
	if (!M)
	{
		return false;
	}
	if (Component->GetStaticMesh() != M)
	{
		Component->SetStaticMesh(M);
		Component->EmptyOverrideMaterials();
	}
	if (M->GetStaticMaterials().Num() > 1)
	{
		// One tinted instance per component and colour is plenty for the kit's simple material.
		UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(1));
		if (!Mid || Mid->GetOuter() != Component)
		{
			Mid = Tinted(Component, Tint);
			Component->SetMaterial(1, Mid);
		}
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Tint"), Tint);
		}
	}
	return true;
}

const TCHAR* CivStyle(const FString& CivId)
{
	// Two launch civs per architectural style (leaders-and-art-style.md); city-states and the rest are Mediterranean.
	static const TPair<const TCHAR*, const TCHAR*> Styles[] = {
		{TEXT("ENGLAND"), TEXT("European")}, {TEXT("FRANCE"), TEXT("European")}, {TEXT("ROME"), TEXT("Mediterranean")},
		{TEXT("GREECE"), TEXT("Mediterranean")}, {TEXT("PERSIA"), TEXT("MiddleEast")}, {TEXT("ARABIA"), TEXT("MiddleEast")},
		{TEXT("CHINA"), TEXT("Asian")}, {TEXT("JAPAN"), TEXT("Asian")}, {TEXT("EGYPT"), TEXT("African")}, {TEXT("MALI"), TEXT("African")},
		{TEXT("AZTEC"), TEXT("American")}, {TEXT("INCA"), TEXT("American")}};
	for (const auto& S : Styles)
	{
		if (CivId.EndsWith(S.Key)) return S.Value;
	}
	return TEXT("Mediterranean");
}

TPair<const TCHAR*, FString> EraPiece(int32 Era, const FString& ClassicalName, const FString& CivId)
{
	const bool bCity = ClassicalName == TEXT("Palace") || ClassicalName == TEXT("Landmark") || ClassicalName.StartsWith(TEXT("House_"));
	if (!bCity || ClassicalName == TEXT("House_Boarded"))
	{
		return {TEXT("Classical"), ClassicalName};
	}
	const FString Part = ClassicalName == TEXT("Landmark") ? FString(TEXT("Hall")) : ClassicalName;
	const FString Culture = CivStyle(CivId);
	// Until the Industrial era a city is built in its culture's style; from then on in the world's.
	if (Era < 4 && Culture != TEXT("Mediterranean") && Culture != TEXT("European"))
	{
		const FString Name = Culture + TEXT("_") + Part;
		if (Mesh(TEXT("Styles"), Name)) return {TEXT("Styles"), Name};
	}
	const TCHAR* Style = Era >= 6 ? TEXT("Modern_") : Era >= 4 ? TEXT("Industrial_") : Era >= 2 || Culture == TEXT("European") ? TEXT("Medieval_") : nullptr;
	if (Style)
	{
		const FString Name = FString(Style) + Part;
		if (Mesh(TEXT("Towns"), Name)) return {TEXT("Towns"), Name};
	}
	return {TEXT("Classical"), ClassicalName};
}

FString LandmarkName(const FString& WonderId)
{
	TArray<FString> Words;
	WonderId.RightChop(WonderId.StartsWith(TEXT("BUILDING_")) ? 9 : 0).ParseIntoArray(Words, TEXT("_"));
	FString Out;
	for (const FString& W : Words) Out += W.Left(1).ToUpper() + W.RightChop(1).ToLower();
	return Out;
}

TArray<FString> RequiredAssets()
{
	TArray<FString> Out = {TEXT("/Game/Art/M_SovKit.M_SovKit")};
	for (const TCHAR* Name : {TEXT("Tree_Broadleaf"), TEXT("Tree_Conifer"), TEXT("Bush"), TEXT("Rocks"), TEXT("Reeds"), TEXT("Palms"),
			 TEXT("Coral"), TEXT("IceFloe"), TEXT("Volcano"), TEXT("Fumarole"), TEXT("BurntTree"), TEXT("Peak"), TEXT("Mesa"), TEXT("Spires"),
			 TEXT("Pool"), TEXT("Cliffs"), TEXT("Mounds")})
	{
		Out.Add(MeshPath(TEXT("Nature"), Name));
	}
	for (const TCHAR* Name : {TEXT("House_A"), TEXT("House_B"), TEXT("House_C"), TEXT("House_Boarded"), TEXT("Palace"), TEXT("Monument"),
			 TEXT("Granary"), TEXT("Temple"), TEXT("Landmark"), TEXT("Wall"), TEXT("MarketStall"), TEXT("Banner")})
	{
		Out.Add(MeshPath(TEXT("Classical"), Name));
	}
	for (const TCHAR* Style : {TEXT("MiddleEast_"), TEXT("Asian_"), TEXT("African_"), TEXT("American_")})
	{
		for (const TCHAR* Part : {TEXT("House_A"), TEXT("House_B"), TEXT("House_C"), TEXT("Hall"), TEXT("Palace")})
		{
			Out.Add(MeshPath(TEXT("Styles"), FString(Style) + Part));
		}
	}
	for (const TCHAR* Style : {TEXT("Medieval_"), TEXT("Industrial_"), TEXT("Modern_")})
	{
		for (const TCHAR* Part : {TEXT("House_A"), TEXT("House_B"), TEXT("House_C"), TEXT("Hall"), TEXT("Palace")})
		{
			Out.Add(MeshPath(TEXT("Towns"), FString(Style) + Part));
		}
	}
	for (const TCHAR* Name : {TEXT("Sheaf"), TEXT("Beast"), TEXT("Fish"), TEXT("Shell"), TEXT("Ore"), TEXT("Blocks"), TEXT("Shrub"),
			 TEXT("OilPool"), TEXT("Hive"), TEXT("Goods")})
	{
		Out.Add(MeshPath(TEXT("Resources"), Name));
	}
	// The wonders modelled one by one (kit_landmarks.py's PIECES): all but Stonehenge and the Hanging Gardens, which are their kind.
	for (const TCHAR* Id : {TEXT("PYRAMIDS"), TEXT("JEBEL_BARKAL"), TEXT("GREAT_BATH"), TEXT("TEMPLE_OF_ARTEMIS"), TEXT("ORACLE"),
			 TEXT("GREAT_LIGHTHOUSE"), TEXT("COLOSSUS"), TEXT("PETRA"), TEXT("COLOSSEUM"), TEXT("GREAT_LIBRARY"), TEXT("MAHABODHI_TEMPLE"),
			 TEXT("TERRACOTTA_ARMY"), TEXT("MACHU_PICCHU"), TEXT("STATUE_OF_ZEUS"), TEXT("APADANA"), TEXT("MAUSOLEUM_AT_HALICARNASSUS"),
			 TEXT("ETEMENANKI"), TEXT("HAGIA_SOPHIA"), TEXT("ALHAMBRA"), TEXT("CHICHEN_ITZA"), TEXT("MONT_ST_MICHEL"), TEXT("MEENAKSHI_TEMPLE"),
			 TEXT("UNIVERSITY_OF_SANKORE"), TEXT("HUEY_TEOCALLI"), TEXT("KOTOKU_IN"), TEXT("ANGKOR_WAT"), TEXT("VENETIAN_ARSENAL"), TEXT("GREAT_ZIMBABWE"),
			 TEXT("FORBIDDEN_CITY"), TEXT("KILWA_KISIWANI"), TEXT("ORSZ_GH_Z"), TEXT("CASA_DE_CONTRATACI_N"), TEXT("PANAMA_CANAL"),
			 TEXT("ST_BASIL_S_CATHEDRAL"), TEXT("TAJ_MAHAL"), TEXT("TORRE_DE_BEL_M"), TEXT("POTALA_PALACE"), TEXT("RUHR_VALLEY"), TEXT("BOLSHOI_THEATRE"),
			 TEXT("OXFORD_UNIVERSITY"), TEXT("STATUE_OF_LIBERTY"), TEXT("BIG_BEN"), TEXT("HERMITAGE"), TEXT("EIFFEL_TOWER"), TEXT("BROADWAY"),
			 TEXT("CRISTO_REDENTOR"), TEXT("GOLDEN_GATE_BRIDGE"), TEXT("AMUNDSEN_SCOTT_RESEARCH_STATION"), TEXT("EST_DIO_DO_MARACAN"), TEXT("BIOSPH_RE"),
			 TEXT("SYDNEY_OPERA_HOUSE")})
	{
		Out.Add(MeshPath(TEXT("Landmarks"), LandmarkName(FString(TEXT("BUILDING_")) + Id)));
	}
	for (const TCHAR* Name : {TEXT("Pyramid"), TEXT("StepPyramid"), TEXT("StoneCircle"), TEXT("Gardens"), TEXT("Statue"), TEXT("Tower"),
			 TEXT("LatticeTower"), TEXT("DomedHall"), TEXT("Arena"), TEXT("Citadel"), TEXT("Arsenal")})
	{
		Out.Add(MeshPath(TEXT("Wonders"), Name));
	}
	for (const TCHAR* Name : {TEXT("Campus"), TEXT("HolySite"), TEXT("CommercialHub"), TEXT("Harbor"), TEXT("TheaterSquare"),
			 TEXT("Encampment"), TEXT("IndustrialZone"), TEXT("EntertainmentComplex"), TEXT("Aqueduct"), TEXT("Neighborhood"),
			 TEXT("Spaceport"), TEXT("GovernmentPlaza"), TEXT("Dam"), TEXT("Canal"), TEXT("Aerodrome"), TEXT("WaterPark"),
			 TEXT("DiplomaticQuarter"), TEXT("Preserve")})
	{
		Out.Add(MeshPath(TEXT("Districts"), Name));
	}
	// The districts built in the other architectural styles (kit_districts.py STYLED).
	for (const TCHAR* Style : {TEXT("MiddleEast"), TEXT("Asian"), TEXT("African"), TEXT("American")})
	{
		for (const TCHAR* Name : {TEXT("Campus"), TEXT("HolySite"), TEXT("CommercialHub"), TEXT("Harbor"), TEXT("TheaterSquare"), TEXT("Neighborhood"),
				 TEXT("GovernmentPlaza"), TEXT("DiplomaticQuarter")})
		{
			Out.Add(MeshPath(TEXT("Districts"), FString(Name) + TEXT("_") + Style));
		}
	}
	for (const TCHAR* Name : {TEXT("Farm"), TEXT("Mine"), TEXT("Quarry"), TEXT("Pasture"), TEXT("Plantation"), TEXT("Camp"),
			 TEXT("FishingBoats"), TEXT("LumberMill"), TEXT("OilWell"), TEXT("Fort"), TEXT("WindFarm"), TEXT("SolarFarm"), TEXT("Works")})
	{
		Out.Add(MeshPath(TEXT("Fields"), Name));
	}
	for (const TCHAR* Name : {TEXT("Citizen"), TEXT("Herald"), TEXT("Captain"), TEXT("Soldier"), TEXT("Leader"), TEXT("Ship"), TEXT("Boat"),
			 TEXT("Archer"), TEXT("Rider"), TEXT("Siege"), TEXT("Musketeer"), TEXT("Rifleman"), TEXT("Tank"), TEXT("Plane"), TEXT("Steamship"), TEXT("Soldier_MiddleEast"), TEXT("Soldier_Asian"), TEXT("Soldier_African"), TEXT("Soldier_American"),
			 TEXT("Archer_MiddleEast"), TEXT("Archer_Asian"), TEXT("Archer_African"), TEXT("Archer_American"), TEXT("Rider_MiddleEast"),
			 TEXT("Rider_Asian"), TEXT("Rider_African"), TEXT("Rider_American")})
	{
		Out.Add(MeshPath(TEXT("Figures"), Name));
	}
	return Out;
}
}  // namespace SovArt
