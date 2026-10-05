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
	FLinearColor Color;     // terrain and feature, before fog
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
};

struct FSovMirror
{
	int32 Width = 0, Height = 0;
	int32 Viewer = 0;
	TArray<FSovTile> Tiles;  // revealed plots only
	TArray<FSovUnitMarker> Units;  // the viewer's own, and others' on visible plots
	TArray<FSovCityMarker> Cities;  // on revealed plots
	TArray<TPair<FIntPoint, FIntPoint>> Roads;  // neighbouring revealed plots joined by a road
};

FSovMirror BuildMirror(const sov::Game& Game, int32 Viewer);

// A plot's plain-map colour (terrain and feature) and whether it is wooded.
FLinearColor SovPlotColor(const sov::Game& Game, int32 X, int32 Y, bool* bWoods = nullptr);

// Owner colour for markers and labels (barbarians are always the last seat).
FLinearColor SovPlayerColor(const sov::Game& Game, int32 Player);
