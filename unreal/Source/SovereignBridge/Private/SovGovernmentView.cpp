#include "SovGovernmentView.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
// A button whose fill is the tint it is given (as the tree's nodes).
const FButtonStyle& CardStyle()
{
	static FButtonStyle Style = [] {
		FButtonStyle B = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		B.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Edge, 1.f));
		B.SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Gold, 2.f));
		B.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.85f, 0.85f, 1.f), 5.f, FSovStyle::Gold, 2.f));
		B.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::White, 5.f, FSovStyle::Edge * 0.6f, 1.f));
		B.SetNormalPadding(FMargin(8.f, 6.f));
		B.SetPressedPadding(FMargin(8.f, 7.f, 8.f, 5.f));
		return B;
	}();
	return Style;
}
}  // namespace

FLinearColor SSovGovernmentView::KindColor(int32 Kind)
{
	switch (Kind)
	{
		case 0: return FLinearColor(0.42f, 0.08f, 0.06f);   // military
		case 1: return FLinearColor(0.45f, 0.33f, 0.05f);   // economic
		case 2: return FLinearColor(0.07f, 0.18f, 0.42f);   // diplomatic
		case 3: return FLinearColor(0.28f, 0.1f, 0.36f);    // wildcard
		default: return FLinearColor(0.08f, 0.3f, 0.12f);   // great person
	}
}

const TCHAR* SSovGovernmentView::KindName(int32 Kind)
{
	static const TCHAR* const Names[] = {TEXT("Military"), TEXT("Economic"), TEXT("Diplomatic"), TEXT("Wildcard"), TEXT("Great Person")};
	return Names[FMath::Clamp(Kind, 0, 4)];
}

void SSovGovernmentView::Construct(const FArguments& Args)
{
	OnAdopt = Args._OnAdopt;
	OnSlot = Args._OnSlot;
	OnCard = Args._OnCard;
	OnDedication = Args._OnDedication;
	OnBuyChanges = Args._OnBuyChanges;
	OnClose = Args._OnClose;
	auto Header = [](const TCHAR* Text) {
		return SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(Text));
	};
	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.012f, 0.011f, 0.01f, 0.75f)).Padding(0)
		.HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1200.f).HeightOverride(700.f)
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(16, 12))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
						[SNew(SBox).WidthOverride(30).HeightOverride(30)[SNew(SImage).Image(FSovStyle::Icon("government"))]]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[SNew(STextBlock).Font(FSovStyle::Font(18, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.Title); })]
							+ SVerticalBox::Slot().AutoHeight()
							[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this]() { return FText::FromString(Model.Note); })]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Primary())
							.Visibility_Lambda([this]() { return Model.BuyChanges.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
							.OnClicked_Lambda([this]() { OnBuyChanges.ExecuteIfBound(); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(Model.BuyChanges); })]
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
						SNew(SHorizontalBox)
						// Governments.
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
						[
							SNew(SBox).WidthOverride(300)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Header(TEXT("GOVERNMENTS"))]
								+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(GovBox, SVerticalBox)]]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)[Header(TEXT("DEDICATIONS"))]
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DedicationBox, SVerticalBox)]
							]
						]
						// The slots.
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 16, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Header(TEXT("POLICY SLOTS: CLICK ONE TO FILL IT"))]
							+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(SlotBox, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8, 8))]]
						]
						// The cards for the chosen slot.
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(300)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() {
									return FText::FromString(Model.Slots.IsValidIndex(Model.Selected)
										? FString::Printf(TEXT("CARDS FOR SLOT %d (%s)"), Model.Selected + 1, *FString(KindName(Model.Slots[Model.Selected].Kind)).ToUpper())
										: FString(TEXT("CARDS")));
								})]
								+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(CardBox, SVerticalBox)]]
							]
						]
					]
				]
			]
		]
	];
}

void SSovGovernmentView::SetModel(const FSovGovModel& InModel)
{
	Model = InModel;
	FString K = FString::FromInt(Model.Selected) + Model.Title + Model.BuyChanges;
	for (const FSovGovOption& G : Model.Governments) K += FString::Printf(TEXT("%s%d%d|"), *G.Name, G.bCurrent ? 1 : 0, G.bCanAdopt ? 1 : 0);
	for (const FSovGovSlot& S : Model.Slots) K += FString::Printf(TEXT("%d%s|"), S.Kind, *S.Policy);
	for (const FSovGovCard& C : Model.Cards) K += C.Name + TEXT("|");
	for (const TPair<int32, FString>& D : Model.Dedications) K += D.Value + TEXT("|");
	if (K != Key)
	{
		Key = K;
		Rebuild();
	}
}

void SSovGovernmentView::Rebuild()
{
	GovBox->ClearChildren();
	for (const FSovGovOption& G : Model.Governments)
	{
		const int32 Index = G.Index;
		const TSharedRef<SHorizontalBox> SlotsRow = SNew(SHorizontalBox);
		for (int32 k = 0; k < 4; ++k)
		{
			for (int32 n = 0; n < G.Slots[k]; ++n)
				SlotsRow->AddSlot().AutoWidth().Padding(0, 0, 3, 0)[SNew(SBox).WidthOverride(12).HeightOverride(16)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(KindColor(k) * 1.6f)]];
		}
		GovBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(G.bCurrent ? &FSovStyle::Primary() : &FSovStyle::Button()).IsEnabled(G.bCurrent || G.bCanAdopt)
			.ToolTipText(FText::FromString(G.bCurrent ? TEXT("Your government") : G.bCanAdopt ? TEXT("Adopt it") : TEXT("Not now: it needs its civic, or changes are closed this turn")))
			.OnClicked_Lambda([this, Index]() { OnAdopt.ExecuteIfBound(Index); return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(G.Name))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[SlotsRow]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text(FText::FromString(G.Detail))]
			]
		];
	}
	DedicationBox->ClearChildren();
	for (const TPair<int32, FString>& D : Model.Dedications)
	{
		const int32 Index = D.Key;
		DedicationBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).OnClicked_Lambda([this, Index]() { OnDedication.ExecuteIfBound(Index); return FReply::Handled(); })
			[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(D.Value))]
		];
	}
	if (Model.Dedications.Num() == 0)
		DedicationBox->AddSlot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(TEXT("None to choose now.")))];
	SlotBox->ClearChildren();
	for (int32 i = 0; i < Model.Slots.Num(); ++i)
	{
		const FSovGovSlot& S = Model.Slots[i];
		const bool bSel = i == Model.Selected;
		SlotBox->AddSlot()
		[
			SNew(SBox).WidthOverride(170).HeightOverride(96)
			[
				SNew(SButton).IsFocusable(false).ButtonStyle(&CardStyle()).ButtonColorAndOpacity(bSel ? KindColor(S.Kind) * 1.8f : KindColor(S.Kind))
				.OnClicked_Lambda([this, i]() { OnSlot.ExecuteIfBound(i); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(9, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(FString(KindName(S.Kind)).ToUpper()))]
					+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center)
					[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(S.Policy.IsEmpty() ? FSovStyle::Dim : FSovStyle::Text).AutoWrapText(true)
						.Text(FText::FromString(S.Policy.IsEmpty() ? FString(TEXT("Empty")) : S.Policy))]
				]
			]
		];
	}
	if (Model.Slots.Num() == 0)
		SlotBox->AddSlot()[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true)
			.Text(FText::FromString(TEXT("No government yet: adopt one on the left once Code of Laws gives you Chiefdom.")))];
	CardBox->ClearChildren();
	for (const FSovGovCard& C : Model.Cards)
	{
		const int32 Index = C.Index;
		CardBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&CardStyle()).ButtonColorAndOpacity(KindColor(C.Kind))
			.OnClicked_Lambda([this, Index]() { OnCard.ExecuteIfBound(Index); return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(8, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(FString(KindName(C.Kind)).ToUpper()))]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(C.Name))]
			]
		];
	}
	if (Model.Cards.Num() == 0)
		CardBox->AddSlot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true)
			.Text(FText::FromString(Model.Slots.IsValidIndex(Model.Selected) ? TEXT("No card fits this slot now (or changes are closed this turn).") : TEXT("Pick a slot to see the cards for it.")))];
}
