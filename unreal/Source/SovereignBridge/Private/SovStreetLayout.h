// The street-level City Center, generated from the city's game state with a per-hex
// seed (specs/sovereign/world-scale-and-generation.md, The generator: Stages 4-5).
// One style for now (a temperate kit of primitives); the same inputs always give the
// same layout. Plain data so tests can check it without a world.
#pragma once

#include "CoreMinimal.h"

namespace sov
{
class Game;
}

enum class ESovStreetPiece : uint8
{
	Plaza,
	Street,
	Landmark,
	House,
	Wall,
	Banner,
	Boarded,
	Guard,
	Tree,
	Market
};

struct FSovStreetPiece
{
	ESovStreetPiece Kind = ESovStreetPiece::House;
	FVector Location = FVector::ZeroVector;  // local to the hex centre, ground at Z = 0
	FVector Size = FVector(100.0);           // full extents in centimetres
	float Yaw = 0.f;
	FLinearColor Color = FLinearColor::White;
	FString Label;  // landmarks: the building's name
	FString Recipe; // the kit piece that draws it (Classical/Nature mesh name), empty: primitives only
};

enum class ESovStreetMood : uint8
{
	Content,
	Happy,    // banners and markets
	Unhappy,  // boarded-up houses
	Fear      // guards and patrols
};

struct FSovStreetLayout
{
	int32 CityId = -1;
	FString CityName;
	double Radius = 9000.0;  // hex centre to corner
	ESovStreetMood Mood = ESovStreetMood::Content;
	bool bFear = false;
	bool bWalls = false;
	int32 Crowd = 0;  // wandering citizens
	TArray<FSovStreetPiece> Pieces;
	FVector Herald = FVector::ZeroVector;   // Benevolence: hear petitions
	FVector Captain = FVector::ZeroVector;  // Fear: the captain of the guard
	FVector Entry = FVector::ZeroVector;    // where the leader appears

	int32 Count(ESovStreetPiece Kind) const;
};

// Builds the City Center of `City` as `Viewer` knows it.
FSovStreetLayout BuildStreetLayout(const sov::Game& Game, int32 City);
