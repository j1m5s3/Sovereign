// The minimap (plan D, step 6): every plot the viewer has revealed, in its owner's colour or the lens's,
// with the camera's place marked; clicking or dragging on it moves the camera.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

struct FSovMinimapData
{
	int32 Width = 0, Height = 0;
	TArray<FLinearColor> Colors;  // per plot (row-major); alpha 0: not revealed
};

class SSovMinimap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSovMinimap) {}
	SLATE_ATTRIBUTE(TSharedPtr<const FSovMinimapData>, Data)
	SLATE_ATTRIBUTE(FVector2D, Focus)                   // the camera's place, 0..1 across and down the map
	SLATE_EVENT(TDelegate<void(FVector2D)>, OnPoint)  // clicked or dragged to, 0..1
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(280.f, 180.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer,
		const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	// The map's drawn rectangle inside the widget: its origin and its scale (pixels per map unit).
	void Fit(const FVector2D& Size, const FSovMinimapData& D, FVector2D& Origin, double& Scale) const;
	FReply Point(const FGeometry& Geometry, const FPointerEvent& Event);

	TAttribute<TSharedPtr<const FSovMinimapData>> Data;
	TAttribute<FVector2D> Focus;
	TDelegate<void(FVector2D)> OnPoint;
	bool bDragging = false;
};
