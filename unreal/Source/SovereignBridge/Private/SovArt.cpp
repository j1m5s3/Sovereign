#include "SovArt.h"

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
	return Out;
}
}  // namespace SovArt
