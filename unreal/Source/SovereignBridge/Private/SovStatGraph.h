// The end-game graphs (10: save and replay, "Replay data"): each major civ's figures over the game, one line per civ,
// from GameState::turnStats (Game::recordTurnStats).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

namespace sov
{
class Game;
}

struct FSovGraphSeries
{
	FString Name;
	FLinearColor Color = FLinearColor::White;
	bool bYou = false;
	TArray<FVector2D> Points;  // (turn, value), turns ascending
};

struct FSovGraphData
{
	TArray<FString> Stats;                    // "Score", "Science"...
	TArray<TArray<FSovGraphSeries>> Series;  // per stat, one per major civ that has figures
};

// Every stat the core keeps, one series per major civ in seat order; `Viewer`'s line is marked.
FSovGraphData SovEndGraphs(const sov::Game& Game, int32 Viewer);

class SSovStatGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSovStatGraph) {}
	SLATE_ATTRIBUTE(TSharedPtr<const FSovGraphData>, Data)
	SLATE_ATTRIBUTE(int32, Stat)  // index into FSovGraphData::Stats
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520.f, 380.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer,
		const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TAttribute<TSharedPtr<const FSovGraphData>> Data;
	TAttribute<int32> Stat;
};
