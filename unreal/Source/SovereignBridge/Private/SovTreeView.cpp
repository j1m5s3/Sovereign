#include "SovTreeView.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr float NodeW = 224.f, NodeH = 96.f;  // one node
constexpr float ColW = 272.f, RowH = 110.f;   // the grid it sits on
constexpr float PadX = 16.f, PadY = 34.f;      // room for the era names

// A button whose fill is the tint it is given (the shared buttons are dark, and tinting only darkens).
const FButtonStyle& NodeStyle()
{
	static FButtonStyle Style = [] {
		FButtonStyle B = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		B.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Edge, 1.f));
		B.SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Gold, 1.5f));
		B.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.85f, 0.85f, 1.f), 5.f, FSovStyle::Gold, 1.5f));
		B.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Edge * 0.6f, 1.f));
		B.SetNormalPadding(FMargin(7.f, 5.f));
		B.SetPressedPadding(FMargin(7.f, 6.f, 7.f, 4.f));
		return B;
	}();
	return Style;
}

FLinearColor Fill(ESovTreeState S)
{
	switch (S)
	{
		case ESovTreeState::Done: return FLinearColor(0.07f, 0.13f, 0.07f, 0.97f);
		case ESovTreeState::Current: return FLinearColor(0.36f, 0.25f, 0.07f, 1.f);
		case ESovTreeState::Goal: return FLinearColor(0.17f, 0.12f, 0.24f, 1.f);
		case ESovTreeState::Available: return FLinearColor(0.15f, 0.12f, 0.09f, 1.f);
		default: return FLinearColor(0.055f, 0.05f, 0.045f, 0.97f);
	}
}
}  // namespace

// The prerequisite lines under the nodes.
class SSovTreeLines : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSovTreeLines) {}
	SLATE_END_ARGS()
	void Construct(const FArguments&) {}

	struct FLine
	{
		TArray<FVector2D> Points;
		FLinearColor Color;
	};
	TArray<FLine> Paths;
	FVector2D Size = FVector2D(1, 1);

	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&,
		bool) const override
	{
		for (const FLine& L : Paths)
		{
			FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), L.Points, ESlateDrawEffect::None, L.Color, true, 2.f);
		}
		return Layer + 1;
	}
};

void SSovTreeView::Construct(const FArguments& Args)
{
	OnNode = Args._OnNode;
	OnClose = Args._OnClose;
	ChildSlot
	[
		// A solid backdrop: the map should not show through the tree.
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.022f, 0.02f, 0.018f, 0.985f)).Padding(0)
		[
		SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(14, 10))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[SNew(SBox).WidthOverride(30).HeightOverride(30)[SNew(SImage).Image_Lambda([this]() { return FSovStyle::Icon(Model.bCivics ? "civic" : "research"); })]]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(18, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.Title); })]
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this]() { return FText::FromString(Model.Detail); })]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Close (Esc)")))
					.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
					[SNew(STextBlock).Font(FSovStyle::Font(13, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("X")))]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(VScroll, SScrollBox).Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SAssignNew(HScroll, SScrollBox).Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SAssignNew(Sizer, SBox)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[SAssignNew(Lines, SSovTreeLines)]
							+ SOverlay::Slot()[SAssignNew(Canvas, SConstraintCanvas)]
						]
					]
				]
			]
		]
		]
	];
}

void SSovTreeView::SetModel(const FSovTreeModel& InModel)
{
	Model = InModel;
	FString Shape = Model.bCivics ? TEXT("C") : TEXT("T");
	for (const FSovTreeNode& N : Model.Nodes)
	{
		Shape += FString::Printf(TEXT("%s%d"), *N.Name, N.Era);
		for (int32 P : N.Prereqs) Shape += FString::Printf(TEXT(",%d"), P);
		Shape += TEXT("|");
	}
	const bool bShape = Shape != ShapeKey;
	if (bShape)
	{
		ShapeKey = Shape;
		Layout();
	}
	FString Look;
	for (const FSovTreeNode& N : Model.Nodes)
		Look += FString::Printf(TEXT("%d%s%d%.2f|"), static_cast<int32>(N.State), *N.Turns, N.bBoosted ? 1 : 0, N.Progress);
	if (bShape || Look != LookKey)
	{
		LookKey = Look;
		Rebuild();
	}
}

void SSovTreeView::ScrollToCurrent()
{
	int32 Pick = INDEX_NONE;
	for (const ESovTreeState Want : {ESovTreeState::Current, ESovTreeState::Goal, ESovTreeState::Available})
	{
		for (int32 i = 0; i < Model.Nodes.Num() && Pick == INDEX_NONE; ++i)
			if (Model.Nodes[i].State == Want) Pick = i;
		if (Pick != INDEX_NONE) break;
	}
	if (!Pos.IsValidIndex(Pick)) return;
	// A column of room to its left, so its prerequisites show too.
	if (HScroll.IsValid()) HScroll->SetScrollOffset(FMath::Max(0.f, static_cast<float>(Pos[Pick].X) - ColW * 1.2f));
	if (VScroll.IsValid()) VScroll->SetScrollOffset(FMath::Max(0.f, static_cast<float>(Pos[Pick].Y) - RowH * 2.f));
}

void SSovTreeView::Layout()
{
	const int32 N = Model.Nodes.Num();
	int32 MaxEra = 0;
	for (const FSovTreeNode& Node : Model.Nodes) MaxEra = FMath::Max(MaxEra, Node.Era);
	// Columns: after every prerequisite, and every era after the one before it.
	TArray<int32> Col;
	Col.Init(0, N);
	TArray<int32> EraFirst;
	EraFirst.Init(0, MaxEra + 2);
	for (int32 Pass = 0; Pass < 64; ++Pass)
	{
		bool bChanged = false;
		for (int32 E = 1; E <= MaxEra; ++E)
		{
			int32 First = EraFirst[E - 1];
			for (int32 i = 0; i < N; ++i)
				if (Model.Nodes[i].Era < E) First = FMath::Max(First, Col[i] + 1);
			EraFirst[E] = First;
		}
		for (int32 i = 0; i < N; ++i)
		{
			int32 C = EraFirst[FMath::Clamp(Model.Nodes[i].Era, 0, MaxEra)];
			for (int32 P : Model.Nodes[i].Prereqs)
				if (Model.Nodes.IsValidIndex(P)) C = FMath::Max(C, Col[P] + 1);
			if (C != Col[i])
			{
				Col[i] = C;
				bChanged = true;
			}
		}
		if (!bChanged) break;
	}
	// Rows: each node near the average row of its prerequisites, on the nearest free one.
	int32 MaxCol = 0;
	for (int32 C : Col) MaxCol = FMath::Max(MaxCol, C);
	TArray<int32> Row;
	Row.Init(0, N);
	int32 MaxRow = 0;
	for (int32 C = 0; C <= MaxCol; ++C)
	{
		TArray<TPair<float, int32>> Here;  // wanted row, node
		for (int32 i = 0; i < N; ++i)
		{
			if (Col[i] != C) continue;
			float Want = 0.f;
			int32 Count = 0;
			for (int32 P : Model.Nodes[i].Prereqs)
				if (Model.Nodes.IsValidIndex(P)) Want += Row[P], ++Count;
			Here.Add({Count > 0 ? Want / Count : static_cast<float>(Here.Num()), i});
		}
		Here.StableSort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		TSet<int32> Taken;
		for (const TPair<float, int32>& H : Here)
		{
			const int32 Want = FMath::Max(0, FMath::RoundToInt(H.Key));
			int32 R = Want;
			for (int32 d = 0; d < 64; ++d)
			{
				if (!Taken.Contains(Want + d)) { R = Want + d; break; }
				if (Want - d >= 0 && !Taken.Contains(Want - d)) { R = Want - d; break; }
			}
			Taken.Add(R);
			Row[H.Value] = R;
			MaxRow = FMath::Max(MaxRow, R);
		}
	}
	Pos.SetNum(N);
	for (int32 i = 0; i < N; ++i) Pos[i] = FVector2D(PadX + Col[i] * ColW, PadY + Row[i] * RowH);
	Extent = FVector2D(PadX * 2 + MaxCol * ColW + NodeW, PadY + MaxRow * RowH + NodeH + 16.f);
	EraStarts.Reset();
	for (int32 E = 0; E <= MaxEra; ++E)
	{
		int32 First = INT32_MAX;
		for (int32 i = 0; i < N; ++i)
			if (Model.Nodes[i].Era == E) First = FMath::Min(First, Col[i]);
		if (First != INT32_MAX) EraStarts.Add({E, PadX + First * ColW});
	}
	Sizer->SetWidthOverride(Extent.X);
	Sizer->SetHeightOverride(Extent.Y);
	Lines->Size = Extent;
}

void SSovTreeView::Rebuild()
{
	// The lines: lit when the prerequisite is done.
	Lines->Paths.Reset();
	for (int32 i = 0; i < Model.Nodes.Num(); ++i)
	{
		for (int32 P : Model.Nodes[i].Prereqs)
		{
			if (!Pos.IsValidIndex(P)) continue;
			const FVector2D A(Pos[P].X + NodeW, Pos[P].Y + NodeH * 0.5f), B(Pos[i].X, Pos[i].Y + NodeH * 0.5f);
			const float Mid = B.X - (ColW - NodeW) * 0.5f;
			const bool bLit = Model.Nodes[P].State == ESovTreeState::Done;
			const bool bGoal = Model.Nodes[i].State == ESovTreeState::Goal || Model.Nodes[i].State == ESovTreeState::Current;
			Lines->Paths.Add({{A, FVector2D(Mid, A.Y), FVector2D(Mid, B.Y), B},
				bGoal ? FLinearColor(0.6f, 0.45f, 0.9f, 0.9f) : bLit ? FSovStyle::Edge : FLinearColor(0.3f, 0.28f, 0.25f, 0.6f)});
		}
	}
	Canvas->ClearChildren();
	for (const TPair<int32, float>& E : EraStarts)
	{
		const FString Name = Model.Eras.IsValidIndex(E.Key) ? Model.Eras[E.Key] : FString::Printf(TEXT("Era %d"), E.Key + 1);
		Canvas->AddSlot().Offset(FMargin(E.Value, 4.f, ColW, 24.f)).Anchors(FAnchors(0.f, 0.f)).Alignment(FVector2D(0.f, 0.f))
		[SNew(STextBlock).Font(FSovStyle::Font(13, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(Name.ToUpper()))];
	}
	for (int32 i = 0; i < Model.Nodes.Num(); ++i)
	{
		const FSovTreeNode& N = Model.Nodes[i];
		const bool bDone = N.State == ESovTreeState::Done;
		FString Tip = N.Name;
		if (!N.Unlocks.IsEmpty()) Tip += TEXT("\nUnlocks: ") + N.Unlocks;
		if (!N.Boost.IsEmpty()) Tip += FString::Printf(TEXT("\nBoost%s: %s"), N.bBoosted ? TEXT(" (earned)") : TEXT(""), *N.Boost);
		Tip += bDone ? TEXT("\nDone.") : N.State == ESovTreeState::Available
			? TEXT("\nClick to research it.") : TEXT("\nClick to make it your goal: the path to it is chosen as each step finishes.");
		const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(bDone ? FSovStyle::Good : N.State == ESovTreeState::Locked ? FSovStyle::Dim : FSovStyle::Text)
					.Text(FText::FromString(N.Name))]
				+ SHorizontalBox::Slot().AutoWidth()
				[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(N.Turns))]
			];
		if (N.Progress > 0.f && !bDone)
		{
			Body->AddSlot().AutoHeight().Padding(0, 2)
			[SNew(SBox).HeightOverride(3)[SNew(SProgressBar).Style(&FSovStyle::Progress()).Percent(N.Progress)
				.FillColorAndOpacity(Model.bCivics ? FLinearColor(0.7f, 0.45f, 0.9f) : FLinearColor(0.3f, 0.65f, 0.95f))]];
		}
		if (!N.Boost.IsEmpty())
		{
			Body->AddSlot().AutoHeight().Padding(0, 2, 0, 0)
			[SNew(STextBlock).Font(FSovStyle::Font(8)).ColorAndOpacity(N.bBoosted ? FSovStyle::Good : FLinearColor(0.85f, 0.7f, 0.45f)).AutoWrapText(true)
				.Text(FText::FromString((N.bBoosted ? TEXT("Boosted: ") : TEXT("Boost: ")) + N.Boost))];
		}
		if (!N.Unlocks.IsEmpty())
		{
			Body->AddSlot().AutoHeight().Padding(0, 2, 0, 0)
			[SNew(STextBlock).Font(FSovStyle::Font(8)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text(FText::FromString(N.Unlocks))];
		}
		Canvas->AddSlot().Offset(FMargin(Pos[i].X, Pos[i].Y, NodeW, NodeH)).Anchors(FAnchors(0.f, 0.f)).Alignment(FVector2D(0.f, 0.f))
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&NodeStyle()).ButtonColorAndOpacity(Fill(N.State)).IsEnabled(!bDone)
			.ToolTipText(FText::FromString(Tip))
			.OnClicked_Lambda([this, i]() { OnNode.ExecuteIfBound(i); return FReply::Handled(); })
			[SNew(SBox).Clipping(EWidgetClipping::ClipToBounds)[Body]]
		];
	}
}
