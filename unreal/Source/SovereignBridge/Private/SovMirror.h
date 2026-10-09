// What the viewing player knows, flattened for actors and the HUD. Built from the
// core state on every change; the world shows the viewer's knowledge, not the true
// state (world doc).
#pragma once

#include "CoreMinimal.h"

namespace sov
{
class Game;
}

enum class ESovRelief : uint8
{
	Water,
	Flat,
	Hills,
	Mountain
};

struct FSovTile
{
	int32 X = 0, Y = 0;
	ESovRelief Relief = ESovRelief::Flat;
	bool bVisible = false;  // false: revealed earlier, drawn fogged
	bool bWoods = false;    // woods or rainforest: trees on the tile
	// The art kit's detail tile its ground wears (tools/art/blender/kitlib.py PATTERNS): water 12, grass 13, sand 14,
	// stone 7 (hills, mountains), foliage 10 (woods), plaster 8 (tundra, snow).
	uint8 Detail = 8;
	FLinearColor Color;     // terrain and feature, before fog
	int32 Owner = -1;       // the civ whose territory it is (-1: none)
	// A resource the viewer can see (0 none, 1 bonus, 2 luxury, 3 strategic) and the plot's improvement.
	uint8 ResourceClass = 0;
	bool bImproved = false;
	bool bPillaged = false;
	FString Improvement;     // its rules id (IMPROVEMENT_FARM), for the model the map draws
	FString Resource;        // the visible resource's rules id (RESOURCE_WHEAT), likewise
	FString Feature;         // its feature's rules id (FEATURE_MARSH), likewise
};

// One edge between two neighbouring plots: a river, or a territory border drawn on the owner's side.
struct FSovEdge
{
	FIntPoint A, B;  // A: the plot whose side it is drawn on
	FLinearColor Color;
};

struct FSovUnitMarker
{
	int32 Id = 0;
	int32 X = 0, Y = 0;
	int32 Owner = 0;
	FLinearColor Color;  // owner colour
	bool bCivilian = false;
	bool bLeader = false;  // the player's Sovereign (its own layer)
	bool bInCity = false;
	bool bNaval = false;     // a ship
	bool bEmbarked = false;  // a land unit afloat
	int32 Hp = 100;
	FString Name;  // unit type, or the ruler's name for a leader
	FName Icon;    // its flag's icon (unreal/Content/Slate/Icons): attack, ranged, found, build, religion...
};

struct FSovCityMarker
{
	int32 Id = 0;
	int32 X = 0, Y = 0;
	int32 Owner = 0;
	FLinearColor Color;  // owner colour
	FString Name;
	int32 Population = 1;
	int32 Hp = 0, MaxHp = 0;
	bool bCapital = false;
	int32 Loyalty = 100;
	int32 Era = 0;  // its owner's era (Game::playerEra), for the style the map draws it in
};

// A wonder's plot: built, or reserved while it is being built.
struct FSovWonderMarker
{
	int32 X = 0, Y = 0;
	bool bComplete = false;
	FString Name;
	FString Id;  // its rules id (BUILDING_PYRAMIDS), for the model the map draws
};

// A specialty district's plot (not the City Center, which the city marker draws).
struct FSovDistrictMarker
{
	int32 X = 0, Y = 0;
	FString Type;        // its rules id (DISTRICT_CAMPUS), for the model the map draws
	FLinearColor Color;  // owner colour
	bool bComplete = false;
	bool bPillaged = false;
};

struct FSovMirror
{
	int32 Width = 0, Height = 0;
	bool bWrap = false;  // the map wraps east-west (drawn with a copy on each side)
	int32 Viewer = 0;
	TArray<FSovTile> Tiles;  // revealed plots only
	TArray<FSovUnitMarker> Units;  // the viewer's own, and others' on visible plots
	TArray<FSovCityMarker> Cities;  // on revealed plots
	TArray<TPair<FIntPoint, FIntPoint>> Roads;  // neighbouring revealed plots joined by a road
	TArray<FSovWonderMarker> Wonders;           // on revealed plots
	TArray<FSovDistrictMarker> Districts;       // on revealed plots
	TArray<FIntPoint> Villages;                 // tribal villages on revealed plots (01)
	TArray<FIntPoint> Unexplored;               // plots the viewer has never seen (drawn as blank parchment)
	TArray<FIntPoint> Antiquity;                // antiquity sites and shipwrecks, once the viewer knows Natural History (07)
	TArray<FSovEdge> Rivers;                    // river edges between revealed plots (01)
	TArray<FSovEdge> Borders;                   // territory edges, in the owner's colour (02)
};

// What a plot shows when the cursor rests on it (terrain, resource, improvement, owner, yields), for the viewer.
TArray<FString> SovPlotTooltip(const sov::Game& Game, int32 Viewer, int32 X, int32 Y);

FSovMirror BuildMirror(const sov::Game& Game, int32 Viewer);

// A plot's plain-map colour (terrain and feature) and whether it is wooded.
FLinearColor SovPlotColor(const sov::Game& Game, int32 X, int32 Y, bool* bWoods = nullptr);

// Owner colour for markers and labels (barbarians are always the last seat).
FLinearColor SovPlayerColor(const sov::Game& Game, int32 Player);
