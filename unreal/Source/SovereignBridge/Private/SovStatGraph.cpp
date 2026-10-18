#include "SovStatGraph.h"

#include "Rendering/DrawElements.h"
#include "SovMirror.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"

#include "sovereign/game.h"

namespace
{
FString CivLabel(const sov::Game& Game, int32 Player)
{
	const sov::Player& P = Game.state().players[static_cast<size_t>(Player)];
	return P.civ == sov::kNone ? FString(TEXT("?")) : FString(UTF8_TO_TCHAR(Game.rules().civs[static_cast<size_t>(P.civ)].name.c_str()));
}
}  // namespace

FSovGraphData SovEndGraphs(const sov::Game& Game, int32 Viewer)
{
	using Field = int32_t sov::TurnStats::*;
	static const TPair<const TCHAR*, Field> Fields[] = {{TEXT("Score"), &sov::TurnStats::score}, {TEXT("Science"), &sov::TurnStats::science},
		{TEXT("Culture"), &sov::TurnStats::culture}, {TEXT("Faith"), &sov::TurnStats::faith}, {TEXT("Gold"), &sov::TurnStats::gold},
		{TEXT("Military"), &sov::TurnStats::military}, {TEXT("Cities"), &sov::TurnStats::cities}, {TEXT("Population"), &sov::TurnStats::population},
		{TEXT("Techs"), &sov::TurnStats::techs}, {TEXT("Civics"), &sov::TurnStats::civics}};
	const sov::GameState& S = Game.state();
	FSovGraphData D;
	// One series per seat with figures, in seat order; Index maps a seat to its series.
	TArray<int32> Index;
	Index.Init(INDEX_NONE, static_cast<int32>(S.players.size()));
	TArray<FSovGraphSeries> Seats;
	for (const sov::TurnStats& T : S.turnStats)
	{
		if (T.player >= 0 && T.player < Index.Num() && Index[T.player] == INDEX_NONE) Index[T.player] = 0;
	}
	for (int32 P = 0; P < Index.Num(); ++P)
	{
		if (Index[P] == INDEX_NONE) continue;
		Index[P] = Seats.Num();
		FSovGraphSeries& Se = Seats.AddDefaulted_GetRef();
		Se.Name = CivLabel(Game, P) + (P == Viewer ? TEXT(" (you)") : TEXT(""));
		Se.Color = SovPlayerColor(Game, P);
		Se.bYou = P == Viewer;
	}
	for (const auto& F : Fields)
	{
		D.Stats.Add(F.Key);
		TArray<FSovGraphSeries>& Out = D.Series.Add_GetRef(Seats);
		for (const sov::TurnStats& T : S.turnStats)
		{
			if (T.player < 0 || T.player >= Index.Num() || Index[T.player] == INDEX_NONE) continue;
			Out[Index[T.player]].Points.Add(FVector2D(T.turn, T.*(F.Value)));
		}
	}
	return D;
}

void SSovStatGraph::Construct(const FArguments& Args)
{
	Data = Args._Data;
	Stat = Args._Stat;
}

int32 SSovStatGraph::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer,
	const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2D Size = Geometry.GetLocalSize();
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0.012f, 0.011f, 0.01f, 0.9f));
	const TSharedPtr<const FSovGraphData> D = Data.Get();
	const int32 K = Stat.Get();
	const FSlateFontInfo Small = FSovStyle::Font(9);
	auto Label = [&](const FString& Text, FVector2D At, const FLinearColor& Color, int32 L) {
		FSlateDrawElement::MakeText(Out, L, Geometry.ToPaintGeometry(FVector2D(160, 16), FSlateLayoutTransform(At)), Text, Small, ESlateDrawEffect::None, Color);
	};
	if (!D.IsValid() || !D->Series.IsValidIndex(K))
	{
		Label(TEXT("No figures were kept for this game."), FVector2D(12, 12), FSovStyle::Dim, Layer + 1);
		return Layer + 2;
	}
	const TArray<FSovGraphSeries>& Series = D->Series[K];
	double T0 = TNumericLimits<double>::Max(), T1 = TNumericLimits<double>::Lowest(), VMax = 1.0;
	for (const FSovGraphSeries& Se : Series)
	{
		for (const FVector2D& P : Se.Points)
		{
			T0 = FMath::Min(T0, P.X);
			T1 = FMath::Max(T1, P.X);
			VMax = FMath::Max(VMax, P.Y);
		}
	}
	if (T0 > T1)
	{
		Label(TEXT("No figures were kept for this game."), FVector2D(12, 12), FSovStyle::Dim, Layer + 1);
		return Layer + 2;
	}
	if (T1 <= T0) T1 = T0 + 1.0;
	// The plot area, leaving room for the value labels on the left and the turns below.
	const FVector2D TL(48, 10), BR(Size.X - 12, Size.Y - 24);
	const FVector2D Area = BR - TL;
	auto At = [&](const FVector2D& P) { return FVector2D(TL.X + (P.X - T0) / (T1 - T0) * Area.X, BR.Y - P.Y / VMax * Area.Y); };
	// Grid lines at quarters, with their values.
	for (int32 i = 0; i <= 4; ++i)
	{
		const double Y = BR.Y - Area.Y * i / 4.0;
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geometry.ToPaintGeometry(), {FVector2D(TL.X, Y), FVector2D(BR.X, Y)}, ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, i == 0 ? 0.35f : 0.1f), true, 1.f);
		Label(FString::FromInt(FMath::RoundToInt(VMax * i / 4.0)), FVector2D(4, Y - 8), FSovStyle::Dim, Layer + 1);
	}
	Label(FString::Printf(TEXT("turn %d"), FMath::RoundToInt(T0)), FVector2D(TL.X, BR.Y + 4), FSovStyle::Dim, Layer + 1);
	Label(FString::Printf(TEXT("turn %d"), FMath::RoundToInt(T1)), FVector2D(BR.X - 52, BR.Y + 4), FSovStyle::Dim, Layer + 1);
	// One line per civ, the viewer's thicker and on top.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FSovGraphSeries& Se : Series)
		{
			if (Se.bYou != (Pass == 1) || Se.Points.Num() == 0) continue;
			TArray<FVector2D> Line;
			Line.Reserve(Se.Points.Num() + 1);
			for (const FVector2D& P : Se.Points) Line.Add(At(P));
			if (Line.Num() == 1) Line.Add(Line[0] + FVector2D(2, 0));
			FSlateDrawElement::MakeLines(Out, Layer + 2 + Pass, Geometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, Se.Color, true, Se.bYou ? 3.f : 1.5f);
		}
	}
	// The legend, top left inside the plot: each civ with its last value, best first.
	TArray<const FSovGraphSeries*> Order;
	for (const FSovGraphSeries& Se : Series)
	{
		if (Se.Points.Num() > 0) Order.Add(&Se);
	}
	Order.Sort([](const FSovGraphSeries& A, const FSovGraphSeries& B) { return A.Points.Last().Y > B.Points.Last().Y; });
	for (int32 i = 0; i < Order.Num(); ++i)
	{
		const FVector2D Row(TL.X + 8, TL.Y + 4 + i * 15);
		FSlateDrawElement::MakeBox(Out, Layer + 4, Geometry.ToPaintGeometry(FVector2D(10, 3), FSlateLayoutTransform(Row + FVector2D(0, 6))), White,
			ESlateDrawEffect::None, Order[i]->Color);
		Label(FString::Printf(TEXT("%s  %d"), *Order[i]->Name, FMath::RoundToInt(Order[i]->Points.Last().Y)), Row + FVector2D(14, -1),
			Order[i]->bYou ? FSovStyle::Gold : FSovStyle::Text, Layer + 4);
	}
	return Layer + 5;
}
