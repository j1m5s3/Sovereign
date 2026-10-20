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
// The detail sheet's cells (tools/art/blender/textures.py, 8 x 8): a kit vertex carries its cell as alpha = cell / (KitCells - 1).
constexpr int32 KitCells = 64;
// Cells the map and the street paint their own ground with (kitlib.PATTERNS).
namespace Cell
{
constexpr int32 Stone = 7, Plaster = 8, Foliage = 10, Water = 12, Grass = 13, Sand = 14, Masonry = 15, Flagstones = 18, Cobbles = 19;
}
// /Game/Art/<Kit>/SM_<Name>, cached; null when the kit has not been built.
UStaticMesh* Mesh(const TCHAR* Kit, const FString& Name);
// The kit master material (vertex colour x Tint), or null.
UMaterialInterface* KitMaterial();
// A tinted instance of the kit material owned by Outer (owner colours on banners and figures).
UMaterialInstanceDynamic* Tinted(UObject* Outer, const FLinearColor& Tint);
// Puts a kit mesh on a component and paints its team slot (slot 1: clothing, shields, banner cloth)
// with Tint; returns false when the mesh is missing (callers keep their primitive).
bool SetKitMesh(UStaticMeshComponent* Component, const TCHAR* Kit, const FString& Name, const FLinearColor& Tint);
// The rigged people (tools/art/blender/rig_figures.py): SK_<Name> in /Game/Art/Rigged with its Walk and Idle animations.
// SetRiggedFigure puts one on a skeletal component, its team cloth tinted, playing Anim (looping); false without the art.
class USkeletalMesh* RiggedMesh(const FString& Name);
class UAnimSequence* RiggedAnim(const FString& Name, const TCHAR* Anim);
bool SetRiggedFigure(class USkeletalMeshComponent* Component, const FString& Name, const FLinearColor& Tint, const TCHAR* Anim);

// A civ's architectural style (leaders-and-art-style.md) by its rules id: European, Mediterranean, MiddleEast,
// Asian, African or American (Mediterranean for anything else).
const TCHAR* CivStyle(const FString& CivId);
// A civ's leader as themselves (tools/art/blender/kit_leaders.py): Leader_ENGLAND for CIVILIZATION_ENGLAND; the generic
// ruler (Leader) for city-states and civs without one. The mesh, rig and portrait all go by this name.
FString LeaderFigure(const FString& CivId);
// The kits' model for a rules id: an improvement's Fields piece (a walled yard for the rest), a district's Districts piece
// (DISTRICT_HOLY_SITE -> HolySite), a wonder's Wonders kind (null: the Classical temple).
const TCHAR* FieldsModel(const FString& Id);
FString DistrictModel(const FString& Id);
const TCHAR* WonderModel(const FString& Id);
// A Classical kit city piece (Palace, Landmark, House_A/B/C) in a civ's style and era (Game::playerEra), the Landmark
// becoming its Hall. Until the Industrial era: the Styles kit's piece for Middle Eastern, Asian, African and American
// civs (kit_styles.py), the Towns kit's Medieval piece for European ones, and for Mediterranean ones the Classical
// kit, then Medieval from the Medieval era. From the Industrial era every civ builds the Towns kit's Industrial
// (Industrial, Modern) and Modern (Atomic on) pieces. Returns the kit and mesh name; the Classical piece for anything
// else, or when that art is missing.
TPair<const TCHAR*, FString> EraPiece(int32 Era, const FString& ClassicalName, const FString& CivId = FString());

// The Landmarks kit's piece for a wonder (kit_landmarks.py): its rules id without BUILDING_ in CamelCase
// (BUILDING_TEMPLE_OF_ARTEMIS: TempleOfArtemis). The map tries it before the wonder's kind in the Wonders kit.
FString LandmarkName(const FString& WonderId);

// Every asset the game names, for the asset test.
TArray<FString> RequiredAssets();
}  // namespace SovArt
