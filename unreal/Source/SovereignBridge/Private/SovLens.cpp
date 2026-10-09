#include "SovLens.h"

#include "SovMirror.h"

#include "sovereign/game.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

const FLinearColor kNone(0.16f, 0.16f, 0.17f);  // plots the lens says nothing about

// Distinct colours for religions, in founding order.
FLinearColor ReligionColor(int32 Index)
{
	static const FLinearColor Palette[] = {FLinearColor(0.95f, 0.85f, 0.3f), FLinearColor(0.35f, 0.55f, 0.95f), FLinearColor(0.85f, 0.3f, 0.3f),
		FLinearColor(0.4f, 0.8f, 0.45f), FLinearColor(0.75f, 0.45f, 0.9f), FLinearColor(0.95f, 0.6f, 0.25f), FLinearColor(0.3f, 0.8f, 0.8f),
		FLinearColor(0.9f, 0.5f, 0.7f)};
	return Palette[Index % UE_ARRAY_COUNT(Palette)];
}

FLinearColor Ramp(float T)  // 0 red, 0.5 yellow, 1 green
{
	T = FMath::Clamp(T, 0.f, 1.f);
	return T < 0.5f ? FMath::Lerp(FLinearColor(0.85f, 0.2f, 0.15f), FLinearColor(0.95f, 0.85f, 0.25f), T * 2.f)
					: FMath::Lerp(FLinearColor(0.95f, 0.85f, 0.25f), FLinearColor(0.3f, 0.8f, 0.3f), (T - 0.5f) * 2.f);
}
}  // namespace

const TCHAR* SovLensName(ESovLens Lens)
{
	switch (Lens)
	{
		case ESovLens::Religion: return TEXT("Religion");
		case ESovLens::Loyalty: return TEXT("Loyalty");
		case ESovLens::Appeal: return TEXT("Appeal");
		case ESovLens::Settler: return TEXT("Settler");
		case ESovLens::Trade: return TEXT("Trade");
		default: return TEXT("No lens");
	}
}

FName SovLensIcon(ESovLens Lens)
{
	switch (Lens)
	{
		case ESovLens::Religion: return "religion";
		case ESovLens::Loyalty: return "amenity";
		case ESovLens::Appeal: return "tourism";
		case ESovLens::Settler: return "found";
		case ESovLens::Trade: return "trade";
		default: return "menu";
	}
}

void SovApplyLens(FSovMirror& Mirror, const sov::Game& Game, int32 Viewer, ESovLens Lens, TArray<FSovLensKey>* Legend)
{
	if (Lens == ESovLens::None) return;
	const sov::GameState& S = Game.state();
	const sov::Rules& R = Game.rules();
	const sov::PlayerId Me = static_cast<sov::PlayerId>(Viewer);
	TSet<int32> OnRoute, RouteEnds;
	if (Lens == ESovLens::Trade)
	{
		for (const sov::TradeRoute& T : S.tradeRoutes)
		{
			if (T.owner != Me) continue;
			for (int32 P : T.path) OnRoute.Add(P);
			if (const sov::City* C = S.city(T.origin)) RouteEnds.Add(S.grid.index(C->pos));
			if (const sov::City* C = S.city(T.destination)) RouteEnds.Add(S.grid.index(C->pos));
		}
	}
	TMap<int32, int32> ReligionsSeen;  // religion index -> plots, for the legend
	for (FSovTile& Tile : Mirror.Tiles)
	{
		const sov::Hex H{Tile.X, Tile.Y};
		const int32 Index = S.grid.index(H);
		const sov::Plot& P = S.plots[static_cast<size_t>(Index)];
		const sov::City* City = P.city != sov::kNoCity ? S.city(P.city) : nullptr;
		FLinearColor C = kNone;
		switch (Lens)
		{
			case ESovLens::Religion:
				if (City)
				{
					const int32 Maj = Game.cityMajorityReligion(*City);
					if (Maj >= 0)
					{
						C = ReligionColor(Maj);
						ReligionsSeen.FindOrAdd(Maj)++;
					}
					else C = FLinearColor(0.45f, 0.45f, 0.45f);
				}
				break;
			case ESovLens::Loyalty:
				if (City) C = Ramp(City->loyalty / 100.f);
				break;
			case ESovLens::Appeal:
			{
				const int32 A = Game.plotAppeal(H);
				C = A >= 4 ? FLinearColor(0.2f, 0.75f, 0.3f) : A >= 2 ? FLinearColor(0.55f, 0.85f, 0.4f) : A >= 0 ? FLinearColor(0.55f, 0.55f, 0.5f)
					: A >= -2 ? FLinearColor(0.9f, 0.55f, 0.25f) : FLinearColor(0.8f, 0.2f, 0.15f);
				break;
			}
			case ESovLens::Settler:
				C = Game.canFoundCityAt(Me, H) ? FLinearColor(0.3f, 0.85f, 0.35f) : FLinearColor(0.28f, 0.11f, 0.09f);
				break;
			case ESovLens::Trade:
				C = RouteEnds.Contains(Index) ? FLinearColor(1.f, 0.85f, 0.3f) : OnRoute.Contains(Index) ? FLinearColor(0.85f, 0.6f, 0.2f) : kNone;
				break;
			default: break;
		}
		// Keep a little of the ground so the map still reads; fogged plots stay darker in the mesh.
		Tile.Color = FMath::Lerp(Tile.Color, C, 0.8f);
	}
	if (!Legend) return;
	Legend->Reset();
	switch (Lens)
	{
		case ESovLens::Religion:
			for (const TPair<int32, int32>& Seen : ReligionsSeen)
			{
				const sov::TypeIndex Type = S.religions[static_cast<size_t>(Seen.Key)].type;
				Legend->Add({ReligionColor(Seen.Key), Type == sov::kNone ? FString(TEXT("?")) : Str(R.religions[static_cast<size_t>(Type)].name)});
			}
			Legend->Add({FLinearColor(0.45f, 0.45f, 0.45f), TEXT("No majority")});
			break;
		case ESovLens::Loyalty:
			Legend->Add({Ramp(1.f), TEXT("Loyal (100)")});
			Legend->Add({Ramp(0.5f), TEXT("Wavering (50)")});
			Legend->Add({Ramp(0.f), TEXT("About to revolt (0)")});
			break;
		case ESovLens::Appeal:
			Legend->Add({FLinearColor(0.2f, 0.75f, 0.3f), TEXT("Breathtaking (4+)")});
			Legend->Add({FLinearColor(0.55f, 0.85f, 0.4f), TEXT("Charming (2-3)")});
			Legend->Add({FLinearColor(0.55f, 0.55f, 0.5f), TEXT("Average (0-1)")});
			Legend->Add({FLinearColor(0.9f, 0.55f, 0.25f), TEXT("Uninviting (-1 to -2)")});
			Legend->Add({FLinearColor(0.8f, 0.2f, 0.15f), TEXT("Disgusting (-3 or less)")});
			break;
		case ESovLens::Settler:
			Legend->Add({FLinearColor(0.3f, 0.85f, 0.35f), TEXT("A city can be founded")});
			Legend->Add({FLinearColor(0.28f, 0.11f, 0.09f), TEXT("Not here")});
			break;
		case ESovLens::Trade:
			Legend->Add({FLinearColor(1.f, 0.85f, 0.3f), TEXT("Your routes' cities")});
			Legend->Add({FLinearColor(0.85f, 0.6f, 0.2f), TEXT("On a route")});
			break;
		default: break;
	}
}
