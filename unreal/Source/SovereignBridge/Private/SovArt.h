// The scripted art kits (tools/art/): meshes under /Game/Art/<Kit>/ and the kit material.
// Lookups return null when an asset is missing, so scenes fall back to primitives.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

namespace SovArt
{
// /Game/Art/<Kit>/SM_<Name>, cached; null when the kit has not been built.
UStaticMesh* Mesh(const TCHAR* Kit, const FString& Name);
// The kit master material (vertex colour x Tint), or null.
UMaterialInterface* KitMaterial();
// A tinted instance of the kit material owned by Outer (owner colours on banners and figures).
UMaterialInstanceDynamic* Tinted(UObject* Outer, const FLinearColor& Tint);
// Puts a kit mesh on a component and paints its team slot (slot 1: clothing, shields, banner cloth)
// with Tint; returns false when the mesh is missing (callers keep their primitive).
bool SetKitMesh(UStaticMeshComponent* Component, const TCHAR* Kit, const FString& Name, const FLinearColor& Tint);

// A Classical kit city piece (Palace, Landmark, House_A/B/C) in a civ's era (Game::playerEra; kit_towns.py): the
// Classical kit through the Classical era, then the Towns kit's Medieval (Medieval, Renaissance), Industrial
// (Industrial, Modern) or Modern (Atomic on) piece, the Landmark becoming its Hall. Returns the kit and mesh name;
// the Classical piece for anything else, or when the Towns art is missing.
TPair<const TCHAR*, FString> EraPiece(int32 Era, const FString& ClassicalName);

// Every asset the game names, for the asset test.
TArray<FString> RequiredAssets();
}  // namespace SovArt
