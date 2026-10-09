#include "SovMinimap.h"

#include "Rendering/DrawElements.h"
#include "SovHexLayout.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"

void SSovMinimap::Construct(const FArguments& Args)
{
	Data = Args._Data;
	Focus = Args._Focus;
	OnPoint = Args._OnPoint;
}

void SSovMinimap::Fit(const FVector2D& Size, const FSovMinimapData& D, FVector2D& Origin, double& Scale) const
{
	// The map's extent in map units: hex centres plus a hex's half width and height.
	const double W = SovHex::Size * SovHex::Sqrt3 * (D.Width + 0.5);
	const double H = SovHex::Size * (1.5 * FMath::Max(0, D.Height - 1) + 2.0);
	Scale = FMath::Min((Size.X - 8.0) / W, (Size.Y - 8.0) / H);
	Origin = FVector2D((Size.X - W * Scale) * 0.5, (Size.Y - H * Scale) * 0.5);
}

int32 SSovMinimap::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer,
	const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2D Size = Geometry.GetLocalSize();
	// A dark ground with a bronze edge.
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0.012f, 0.011f, 0.01f, 0.95f));
	const TArray<FVector2D> Edge = {FVector2D(0, 0), FVector2D(Size.X, 0), Size, FVector2D(0, Size.Y), FVector2D(0, 0)};
	FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Edge, ESlateDrawEffect::None, FSovStyle::Edge, true, 1.5f);
	const TSharedPtr<const FSovMinimapData> D = Data.Get();
	if (!D.IsValid() || D->Width <= 0 || D->Height <= 0) return Layer + 1;
	FVector2D Origin;
	double Scale;
	Fit(Size, *D, Origin, Scale);
	// One small box per revealed plot.
	const FVector2D Cell(SovHex::Size * SovHex::Sqrt3 * Scale + 0.6, SovHex::Size * 1.5 * Scale + 0.6);
	const FVector2D Half(SovHex::Size * SovHex::Sqrt3 * 0.5, SovHex::Size);
	for (int32 Y = 0; Y < D->Height; ++Y)
	{
		for (int32 X = 0; X < D->Width; ++X)
		{
			const FLinearColor& C = D->Colors[Y * D->Width + X];
			if (C.A <= 0.f) continue;
			const FVector2D At = Origin + (SovHex::MapPos(X, Y) + Half) * Scale - Cell * 0.5;
			FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(Cell, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, C);
		}
	}
	// The camera: a frame about a screen's worth of plots around its focus.
	const FVector2D F = Focus.Get();
	const FVector2D MapSize(SovHex::Size * SovHex::Sqrt3 * (D->Width + 0.5), SovHex::Size * (1.5 * FMath::Max(0, D->Height - 1) + 2.0));
	const FVector2D View(SovHex::Size * SovHex::Sqrt3 * 14.0 * Scale, SovHex::Size * 1.5 * 9.0 * Scale);
	const FVector2D Center = Origin + F * MapSize * Scale;
	const FVector2D TL = Center - View * 0.5;
	TArray<FVector2D> Frame = {TL, TL + FVector2D(View.X, 0), TL + View, TL + FVector2D(0, View.Y), TL};
	FSlateDrawElement::MakeLines(Out, Layer + 2, Geometry.ToPaintGeometry(), Frame, ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);
	return Layer + 3;
}

FReply SSovMinimap::Point(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const TSharedPtr<const FSovMinimapData> D = Data.Get();
	if (!D.IsValid() || D->Width <= 0) return FReply::Handled();
	FVector2D Origin;
	double Scale;
	Fit(Geometry.GetLocalSize(), *D, Origin, Scale);
	const FVector2D MapSize(SovHex::Size * SovHex::Sqrt3 * (D->Width + 0.5), SovHex::Size * (1.5 * FMath::Max(0, D->Height - 1) + 2.0));
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FVector2D N = (Local - Origin) / (MapSize * Scale);
	OnPoint.ExecuteIfBound(FVector2D(FMath::Clamp(N.X, 0.0, 1.0), FMath::Clamp(N.Y, 0.0, 1.0)));
	return FReply::Handled();
}

FReply SSovMinimap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDragging = true;
	Point(Geometry, Event);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SSovMinimap::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bDragging) return FReply::Unhandled();
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SSovMinimap::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return bDragging ? Point(Geometry, Event) : FReply::Unhandled();
}
