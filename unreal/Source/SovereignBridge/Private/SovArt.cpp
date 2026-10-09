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

TArray<FString> RequiredAssets()
{
	TArray<FString> Out = {TEXT("/Game/Art/M_SovKit.M_SovKit")};
	for (const TCHAR* Name : {TEXT("Tree_Broadleaf"), TEXT("Tree_Conifer"), TEXT("Bush"), TEXT("Rocks")})
	{
		Out.Add(MeshPath(TEXT("Nature"), Name));
	}
	for (const TCHAR* Name : {TEXT("House_A"), TEXT("House_B"), TEXT("House_C"), TEXT("House_Boarded"), TEXT("Palace"), TEXT("Monument"),
			 TEXT("Granary"), TEXT("Temple"), TEXT("Landmark"), TEXT("Wall"), TEXT("MarketStall"), TEXT("Banner")})
	{
		Out.Add(MeshPath(TEXT("Classical"), Name));
	}
	for (const TCHAR* Name : {TEXT("Sheaf"), TEXT("Beast"), TEXT("Fish"), TEXT("Shell"), TEXT("Ore"), TEXT("Blocks"), TEXT("Shrub"),
			 TEXT("OilPool"), TEXT("Hive"), TEXT("Goods")})
	{
		Out.Add(MeshPath(TEXT("Resources"), Name));
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
	for (const TCHAR* Name : {TEXT("Farm"), TEXT("Mine"), TEXT("Quarry"), TEXT("Pasture"), TEXT("Plantation"), TEXT("Camp"),
			 TEXT("FishingBoats"), TEXT("LumberMill"), TEXT("OilWell"), TEXT("Fort"), TEXT("WindFarm"), TEXT("SolarFarm"), TEXT("Works")})
	{
		Out.Add(MeshPath(TEXT("Fields"), Name));
	}
	for (const TCHAR* Name : {TEXT("Citizen"), TEXT("Herald"), TEXT("Captain"), TEXT("Soldier"), TEXT("Leader"), TEXT("Ship"), TEXT("Boat")})
	{
		Out.Add(MeshPath(TEXT("Figures"), Name));
	}
	return Out;
}
}  // namespace SovArt
