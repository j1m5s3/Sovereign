#include "SovGameUI.h"

#include "SovStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FString StatKey(const TArray<FSovUIStat>& Stats)
{
	FString K;
	for (const FSovUIStat& S : Stats) K += S.Icon.ToString() + S.Text + S.Tip + TEXT("|");
	return K;
}
}  // namespace

void SSovGameUI::Construct(const FArguments& Args)
{
	OnKey = Args._OnKey;
	OnPick = Args._OnPick;
	OnEndTurn = Args._OnEndTurn;
	auto Visible = [this](TFunction<bool()> Test) {
		return TAttribute<EVisibility>::CreateLambda([Test]() { return Test() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; });
	};
	// A top bar button: an icon, its text, and below it a thin progress line.
	auto Progress = [this](FSovUIStat FSovUIModel::*Stat, float FSovUIModel::*Value, FLinearColor Fill) -> TSharedRef<SWidget> {
		return SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).OnClicked_Lambda([this, Stat]() {
			OnKey.ExecuteIfBound((Model.*Stat).Key);
			return FReply::Handled();
		}).ToolTipText_Lambda([this, Stat]() { return FText::FromString((Model.*Stat).Tip); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
				[SNew(SBox).WidthOverride(20).HeightOverride(20)[SNew(SImage).Image_Lambda([this, Stat]() { return FSovStyle::Icon((Model.*Stat).Icon); })]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this, Stat]() { return FText::FromString((Model.*Stat).Text); })]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
			[SNew(SBox).HeightOverride(3)[SNew(SProgressBar).Style(&FSovStyle::Progress()).FillColorAndOpacity(Fill).Percent_Lambda([this, Value]() { return Model.*Value; })]]
		];
	};

	ChildSlot
	[
		SNew(SOverlay)
		// The top bar.
		+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Fill)
		[
			SNew(SBorder).BorderImage(FSovStyle::Bar()).Padding(FMargin(10, 4)).Visibility(Visible([this]() { return Model.bVisible; }))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(StatsBox, SHorizontalBox)]
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)[Progress(&FSovUIModel::Research, &FSovUIModel::ResearchProgress, FLinearColor(0.3f, 0.65f, 0.95f))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)[Progress(&FSovUIModel::Civic, &FSovUIModel::CivicProgress, FLinearColor(0.7f, 0.45f, 0.9f))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
				[
					SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText_Lambda([this]() { return FText::FromString(Model.Government.Tip); })
					.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(Model.Government.Key); return FReply::Handled(); })
					[StatWidget(FSovUIStat{"government", TEXT(""), TEXT("")}, 11)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0)
				[SNew(STextBlock).Font(FSovStyle::Font(11, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.Turn.Text); })
					.ToolTipText_Lambda([this]() { return FText::FromString(Model.Turn.Tip); })]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Menu: save, load, new game, quit (Esc)")))
					.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::Escape); return FReply::Handled(); })
					[SNew(SBox).WidthOverride(20).HeightOverride(20)[SNew(SImage).Image(FSovStyle::Icon("menu"))]]
				]
			]
		]
		// The selected unit.
		+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Left).Padding(12, 0, 0, 12)
		[
			SNew(SBox).WidthOverride(470).Visibility(Visible([this]() { return Model.bVisible && Model.bUnit; }))
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(16, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.UnitName); })]
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Model.UnitSub); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
					[SNew(SBox).HeightOverride(8)[SNew(SProgressBar).Style(&FSovStyle::Progress()).Percent_Lambda([this]() { return Model.UnitHealth; })
						.FillColorAndOpacity_Lambda([this]() { return Model.UnitHealth > 0.5f ? FSovStyle::Good : Model.UnitHealth > 0.25f ? FSovStyle::Gold : FSovStyle::Bad; })]]
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(UnitStatsBox, SHorizontalBox)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[SAssignNew(ActionsBox, SWrapBox).PreferredSize(450.f).InnerSlotPadding(FVector2D(4, 4))]
				]
			]
		]
		// The open chooser.
		+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(0, 50, 16, 110)
		[
			SNew(SBox).WidthOverride(560).MaxDesiredHeight(640).Visibility(Visible([this]() { return Model.bVisible && Model.bChooser; }))
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[SNew(STextBlock).Font(FSovStyle::Font(14, true)).ColorAndOpacity(FSovStyle::Gold).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Model.ChooserTitle); })]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Close (Esc)")))
							.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::Escape); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("X")))]
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(ChoicesBox, SVerticalBox)]]
				]
			]
		]
		// End turn.
		+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(0, 0, 16, 16)
		[
			SNew(SBox).WidthOverride(300).Visibility(Visible([this]() { return Model.bVisible; }))
			[
				SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Primary())
				.ButtonColorAndOpacity_Lambda([this]() { return Model.bTurnReady ? FLinearColor::White : FLinearColor(0.45f, 0.42f, 0.4f, 1.f); })
				.IsEnabled_Lambda([this]() { return Model.bMyTurn; })
				.ToolTipText(FText::FromString(TEXT("End the turn (Space or Enter); when something needs your choice first, it opens")))
				.OnClicked_Lambda([this]() { OnEndTurn.ExecuteIfBound(); return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 6, 10, 6)
					[SNew(SBox).WidthOverride(36).HeightOverride(36)[SNew(SImage).Image(FSovStyle::Icon("endturn"))]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(FSovStyle::Font(15, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(Model.TurnLabel); })]
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Model.TurnDetail); })]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SSovGameUI::StatWidget(const FSovUIStat& Stat, int32 Size)
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	Box->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[SNew(SBox).WidthOverride(Size + 8).HeightOverride(Size + 8)[SNew(SImage).Image(FSovStyle::Icon(Stat.Icon))]];
	if (!Stat.Text.IsEmpty())
		Box->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(FSovStyle::Font(Size)).ColorAndOpacity(Stat.Color).Text(FText::FromString(Stat.Text))];
	TSharedRef<SWidget> Out = Box;
	Out->SetToolTipText(FText::FromString(Stat.Tip));
	if (Stat.Key.IsValid())
	{
		const FKey Key = Stat.Key;
		return SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(Stat.Tip))
			.OnClicked_Lambda([this, Key]() { OnKey.ExecuteIfBound(Key); return FReply::Handled(); })[Box];
	}
	return Out;
}

void SSovGameUI::SetModel(const FSovUIModel& InModel)
{
	Model = InModel;
	if (const FString K = StatKey(Model.Stats); K != StatsKey)
	{
		StatsKey = K;
		RebuildStats();
	}
	FString U = Model.bUnit ? StatKey(Model.UnitStats) : FString();
	for (const FSovUIAction& A : Model.UnitActions) U += A.Icon.ToString() + A.Label + (A.bEnabled ? TEXT("1") : TEXT("0"));
	if (U != UnitKey)
	{
		UnitKey = U;
		RebuildUnit();
	}
	FString C = Model.bChooser ? Model.ChooserTitle : FString();
	for (const FString& L : Model.Choices) C += L + TEXT("|");
	if (C != ChooserKey)
	{
		ChooserKey = C;
		RebuildChooser();
	}
}

void SSovGameUI::RebuildStats()
{
	StatsBox->ClearChildren();
	for (const FSovUIStat& S : Model.Stats) StatsBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[StatWidget(S)];
}

void SSovGameUI::RebuildUnit()
{
	UnitStatsBox->ClearChildren();
	for (const FSovUIStat& S : Model.UnitStats) UnitStatsBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[StatWidget(S, 11)];
	ActionsBox->ClearChildren();
	for (const FSovUIAction& A : Model.UnitActions)
	{
		const FKey Key = A.Key;
		ActionsBox->AddSlot()
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).IsEnabled(A.bEnabled).ToolTipText(FText::FromString(A.Label))
			.OnClicked_Lambda([this, Key]() { OnKey.ExecuteIfBound(Key); return FReply::Handled(); })
			[SNew(SBox).WidthOverride(34).HeightOverride(34)[SNew(SImage).Image(FSovStyle::Icon(A.Icon))]]
		];
	}
}

void SSovGameUI::RebuildChooser()
{
	ChoicesBox->ClearChildren();
	for (int32 i = 0; i < Model.Choices.Num(); ++i)
	{
		ChoicesBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).OnClicked_Lambda([this, i]() { OnPick.ExecuteIfBound(i); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(2, 0, 8, 0)
				[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(i < 9 ? FString::FromInt(i + 1) : FString(TEXT(" "))))]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(Model.Choices[i]))]
			]
		];
	}
}
