// Where a core hex sits in the Unreal world. Matches sov::HexGrid: pointy-top,
// odd rows shifted half a hex east (odd-r), row 0 on the north edge. Map east is
// world +Y and map north is world +X, so a camera with yaw 0 sees north at the top.
#pragma once

#include "CoreMinimal.h"

namespace SovHex
{
// Centre-to-corner distance of a hex, in centimetres.
constexpr double Size = 100.0;
constexpr double Sqrt3 = 1.7320508075688772;

// Map-plane position (east, south) of an offset hex centre.
inline FVector2D MapPos(int32 Col, int32 Row)
{
	return FVector2D(Size * Sqrt3 * (Col + 0.5 * (Row & 1)), Size * 1.5 * Row);
}

inline FVector ToWorld(FVector2D Map, double Z = 0.0)
{
	return FVector(-Map.Y, Map.X, Z);
}

inline FVector Center(int32 Col, int32 Row, double Z = 0.0)
{
	return ToWorld(MapPos(Col, Row), Z);
}

// Offset hex under a world point (not wrapped or range-checked).
inline FIntPoint FromWorld(const FVector& World)
{
	const double MX = World.Y, MY = -World.X;
	const double Q = (Sqrt3 / 3.0 * MX - MY / 3.0) / Size;
	const double R = (2.0 / 3.0 * MY) / Size;
	const double S = -Q - R;
	double RQ = FMath::RoundToDouble(Q), RR = FMath::RoundToDouble(R), RS = FMath::RoundToDouble(S);
	const double DQ = FMath::Abs(RQ - Q), DR = FMath::Abs(RR - R), DS = FMath::Abs(RS - S);
	if (DQ > DR && DQ > DS)
	{
		RQ = -RR - RS;
	}
	else if (DR > DS)
	{
		RR = -RQ - RS;
	}
	const int32 AxQ = static_cast<int32>(RQ), AxR = static_cast<int32>(RR);
	// odd-r: col = q + floor(r / 2)
	return FIntPoint(AxQ + (AxR >= 0 ? AxR / 2 : -((-AxR + 1) / 2)), AxR);
}
}  // namespace SovHex
