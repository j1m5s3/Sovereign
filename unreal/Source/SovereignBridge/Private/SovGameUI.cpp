#include "SovGameUI.h"

#include "Framework/Application/SlateApplication.h"
#include "SovStyle.h"
#include "Styling/CoreStyle.h"
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
	OnFocus = Args._OnFocus;
	OnBuy = Args._OnBuy;
	OnTreeNode = Args._OnTreeNode;
	OnNotice = Args._OnNotice;
	OnDismiss = Args._OnDismiss;
	OnEndClose = Args._OnEndClose;
	OnLens = Args._OnLens;
	OnMinimap = Args._OnMinimap;
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

	// Buy what the city builds, with gold or with faith.
	auto Buy = [this](bool bFaith) -> TSharedRef<SWidget> {
		return SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button())
			.Visibility_Lambda([this, bFaith]() { return (bFaith ? Model.BuyFaith : Model.BuyGold).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.IsEnabled_Lambda([this, bFaith]() { return bFaith ? Model.bBuyFaith : Model.bBuyGold; })
			.ToolTipText(FText::FromString(bFaith ? TEXT("Buy it now with faith") : TEXT("Buy it now with gold")))
			.OnClicked_Lambda([this, bFaith]() { OnBuy.ExecuteIfBound(bFaith); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[SNew(SBox).WidthOverride(18).HeightOverride(18)[SNew(SImage).Image(FSovStyle::Icon(bFaith ? "faith" : "gold"))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this, bFaith]() { return FText::FromString(bFaith ? Model.BuyFaith : Model.BuyGold); })]
			];
	};
	// The city focus: which yield its citizens favour when they pick plots.
	TSharedRef<SHorizontalBox> Focus = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
		[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(TEXT("Citizens favour")))];
	static const TPair<FName, const TCHAR*> Focuses[] = {{"civic", TEXT("Balanced: no yield favoured")}, {"food", TEXT("Food")}, {"production", TEXT("Production")},
		{"gold", TEXT("Gold")}, {"science", TEXT("Science")}, {"culture", TEXT("Culture")}, {"faith", TEXT("Faith")}};
	for (int32 f = 0; f < UE_ARRAY_COUNT(Focuses); ++f)
	{
		Focus->AddSlot().AutoWidth().Padding(0, 0, 3, 0)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(Focuses[f].Value))
			.ButtonColorAndOpacity_Lambda([this, f]() { return Model.CityFocus == f ? FSovStyle::Gold : FLinearColor::White; })
			.OnClicked_Lambda([this, f]() { OnFocus.ExecuteIfBound(f); return FReply::Handled(); })
			[SNew(SBox).WidthOverride(18).HeightOverride(18)[SNew(SImage).Image(FSovStyle::Icon(Focuses[f].Key))]]
		];
	}

	// The lens buttons: click one to show it, again to clear it.
	TSharedRef<SHorizontalBox> Lenses = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
		[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(TEXT("Lenses")))];
	for (int32 l = 1; l < static_cast<int32>(ESovLens::Count); ++l)
	{
		Lenses->AddSlot().AutoWidth().Padding(0, 0, 3, 0)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(FString(SovLensName(static_cast<ESovLens>(l))) + TEXT(" lens")))
			.ButtonColorAndOpacity_Lambda([this, l]() { return Model.Lens == l ? FSovStyle::Gold : FLinearColor::White; })
			.OnClicked_Lambda([this, l]() { OnLens.ExecuteIfBound(l); return FReply::Handled(); })
			[SNew(SBox).WidthOverride(18).HeightOverride(18)[SNew(SImage).Image(FSovStyle::Icon(SovLensIcon(static_cast<ESovLens>(l))))]]
		];
	}

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
					SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button())
					.ToolTipText(FText::FromString(TEXT("Empire (F8): your civ, faith, era and age, diplomacy, the world's contests, climate, governors and leader")))
					.ButtonColorAndOpacity_Lambda([this]() { return Model.bEmpire ? FSovStyle::Gold : FLinearColor::White; })
					.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::F8); return FReply::Handled(); })
					[StatWidget(FSovUIStat{"era", TEXT(""), TEXT("")}, 11)]
				]
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
		// The latest message, under the top bar.
		+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Center).Padding(0, 54, 0, 0)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(14, 6))
			.Visibility_Lambda([this]() { return Model.bVisible && Model.MessageAlpha > 0.f && !Model.Message.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Model.MessageAlpha); })
			.BorderBackgroundColor_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Model.MessageAlpha); })
			[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.Message); })]
		]
		// The Empire panel, top left under the bar.
		+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Left).Padding(12, 52, 0, 0)
		[
			SNew(SBox).WidthOverride(560).MaxDesiredHeight(600).Visibility(Visible([this]() { return Model.bVisible && Model.bEmpire && !Model.Tree.bOpen && !Model.bEnd; }))
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[SNew(STextBlock).Font(FSovStyle::Font(14, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(TEXT("Empire")))]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Close (F8)")))
							.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::F8); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("X")))]
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(EmpireBox, SVerticalBox)]]
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
		// The selected city.
		+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Left).Padding(12, 0, 0, 12)
		[
			SNew(SBox).WidthOverride(470).Visibility(Visible([this]() { return Model.bVisible && Model.bCity; }))
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(10)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(18, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.CityName); })]
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Model.CitySub); })]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
					[SNew(SBox).HeightOverride(6)[SNew(SProgressBar).Style(&FSovStyle::Progress()).Percent_Lambda([this]() { return Model.CityHealth; })
						.FillColorAndOpacity_Lambda([this]() { return Model.CityHealth > 0.5f ? FSovStyle::Good : FSovStyle::Bad; })]]
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(CityStatsBox, SHorizontalBox)]
					// Growth.
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)[SNew(SBox).WidthOverride(22).HeightOverride(22)[SNew(SImage).Image(FSovStyle::Icon("food"))]]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(Model.GrowthText); })]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
							[SNew(SBox).HeightOverride(5)[SNew(SProgressBar).Style(&FSovStyle::Progress()).FillColorAndOpacity(FLinearColor(0.5f, 0.85f, 0.35f)).Percent_Lambda([this]() { return Model.GrowthProgress; })]]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)[SAssignNew(CityLivingBox, SHorizontalBox)]
					]
					// Production: click it to choose; buy it beside.
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
						SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Choose what to build, or buy it (P)")))
						.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::P); return FReply::Handled(); })
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
							[SNew(SBox).WidthOverride(28).HeightOverride(28)[SNew(SImage).Image_Lambda([this]() { return FSovStyle::Icon(Model.ProductionIcon); })]]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(Model.ProductionName); })]
									+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this]() { return FText::FromString(Model.ProductionText); })]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
								[SNew(SBox).HeightOverride(5)[SNew(SProgressBar).Style(&FSovStyle::Progress()).FillColorAndOpacity(FLinearColor(0.95f, 0.6f, 0.25f)).Percent_Lambda([this]() { return Model.ProductionProgress; })]]
							]
						]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)[Buy(false)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)[Buy(true)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[Focus]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[SAssignNew(CityLinesBox, SVerticalBox)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[SAssignNew(CityActionsBox, SWrapBox).PreferredSize(450.f).InnerSlotPadding(FVector2D(4, 4))]
				]
			]
		]
		// The open chooser.
		+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(0, 50, 16, 110)
		[
			SNew(SBox).WidthOverride(560).MaxDesiredHeight(640).Visibility(Visible([this]() { return Model.bVisible && Model.bChooser && !Model.Tree.bOpen; }))
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
		// The tech or civic tree, over the map below the top bar.
		+ SOverlay::Slot().Padding(12, 52, 12, 12)
		[
			SAssignNew(TreeView, SSovTreeView)
			.Visibility_Lambda([this]() { return Model.bVisible && Model.Tree.bOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnNode_Lambda([this](int32 Node) { OnTreeNode.ExecuteIfBound(Node); })
			.OnClose_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::Escape); })
		]
		// Notifications, the lenses and the minimap, stacked above the end-turn button.
		+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(0, 0, 16, 92)
		[
			SNew(SBox).WidthOverride(300).Visibility(Visible([this]() { return Model.bVisible && !Model.Tree.bOpen && !Model.bChooser && !Model.bEnd; }))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SAssignNew(NoticesBox, SVerticalBox)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[SAssignNew(LegendBox, SVerticalBox)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[Lenses]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(180)
					[
						SNew(SSovMinimap)
						.Data_Lambda([this]() { return Model.Minimap; })
						.Focus_Lambda([this]() { return Model.MinimapFocus; })
						.OnPoint_Lambda([this](FVector2D At) { OnMinimap.ExecuteIfBound(At); })
					]
				]
			]
		]
		// The end of the game: over everything else.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(900).MaxDesiredHeight(720).Visibility_Lambda([this]() { return Model.bVisible && Model.bEnd ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(22, 16))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[SNew(STextBlock).Font(FSovStyle::Font(30, true)).ColorAndOpacity_Lambda([this]() { return Model.bWon ? FSovStyle::Gold : FSovStyle::Bad; })
						.Text_Lambda([this]() { return FText::FromString(Model.EndTitle); })]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 12)
					[SNew(STextBlock).Font(FSovStyle::Font(13)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(Model.EndSub); })]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 18, 0)
						[
							SNew(SBox).WidthOverride(280)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(TEXT("Scores")))]
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(EndScoresBox, SVerticalBox)]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
							[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(TEXT("The chronicle of your reign")))]
							+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(EndChronicleBox, SVerticalBox)]]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ContentPadding(FMargin(16, 6))
								.OnClicked_Lambda([this]() { OnEndClose.ExecuteIfBound(); return FReply::Handled(); })
								[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Look at the map")))]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ContentPadding(FMargin(16, 6))
								.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::F6); return FReply::Handled(); })
								[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Write the chronicle (F6)")))]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Primary()).ContentPadding(FMargin(16, 6))
								.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::Escape); return FReply::Handled(); })
								[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Main menu")))]
						]
					]
				]
			]
		]
		// A page to read: how to play, or the chronicle.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 50, 0, 20)
		[
			SNew(SBox).WidthOverride(880).MaxDesiredHeight(640).Visibility_Lambda([this]() { return Model.bVisible && Model.bReader && !Model.bEnd ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(16, 12))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[SNew(STextBlock).Font(FSovStyle::Font(16, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.ReaderTitle); })]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Close")))
							.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(Model.ReaderKey); return FReply::Handled(); })
							[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("X")))]
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)[SAssignNew(ReaderScroll, SScrollBox) + SScrollBox::Slot()[SAssignNew(ReaderBox, SVerticalBox)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).AutoWrapText(true).Text_Lambda([this]() { return FText::FromString(Model.ReaderFoot); })]
				]
			]
		]
		// The plot under the cursor, beside it.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() {
			const FVector2D Size = LastGeometry.GetLocalSize();
			const FVector2D At = LastGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()) + FVector2D(18.0, 18.0);
			const double H = 10.0 + 17.0 * Model.Hover.Num();
			return FMargin(FMath::Min(At.X, Size.X - 330.0), FMath::Min(At.Y, Size.Y - H - 8.0), 0.f, 0.f);
		}))
		[
			SNew(SBox).MaxDesiredWidth(320).Visibility_Lambda([this]() {
				return Model.bVisible && bOverMap && Model.Hover.Num() > 0 && !Model.Tree.bOpen && !Model.bEnd ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(8, 5))[SAssignNew(HoverBox, SVerticalBox)]]
		]
		// Hot seat: the next player's turn, the map hidden until they take over.
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.012f, 0.011f, 0.01f, 1.f))
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			.Visibility_Lambda([this]() { return Model.bHandover ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(40, 26))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[SNew(STextBlock).Font(FSovStyle::Font(28, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.HandoverName + TEXT("'s turn")); })]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 18)
					[SNew(STextBlock).Font(FSovStyle::Font(12)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Hand over the seat. The map stays hidden so nobody sees another player's lands.")))]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Primary()).ContentPadding(FMargin(28, 8))
						.OnClicked_Lambda([this]() { OnKey.ExecuteIfBound(EKeys::Enter); return FReply::Handled(); })
						[SNew(STextBlock).Font(FSovStyle::Font(15, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(TEXT("Take over (Enter)")))]
					]
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

void SSovGameUI::Tick(const FGeometry& Geometry, const double Time, const float Delta)
{
	SCompoundWidget::Tick(Geometry, Time, Delta);
	LastGeometry = Geometry;
	// Over the map when nothing of ours lies under the cursor: the topmost widget there is this one (or the viewport).
	bOverMap = false;
	if (FSlateApplication::IsInitialized())
	{
		const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), FSlateApplication::Get().GetInteractiveTopLevelWindows());
		bOverMap = Path.IsValid() && (Path.Widgets.Last().Widget == AsShared() || !Path.ContainsWidget(this));
	}
}

void SSovGameUI::SetModel(const FSovUIModel& InModel)
{
	Model = InModel;
	FString Rd = Model.bReader ? Model.ReaderTitle : FString();
	for (const FString& L : Model.ReaderLines) Rd += L + TEXT("|");
	if (Rd != ReaderKeyText)
	{
		ReaderKeyText = Rd;
		ReaderBox->ClearChildren();
		for (const FString& L : Model.ReaderLines)
			ReaderBox->AddSlot().AutoHeight().Padding(0, 2)[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(L))];
		ReaderScroll->ScrollToEnd();  // the chronicle's latest lines first in view
	}
	FString Hv;
	for (const FString& L : Model.Hover) Hv += L + TEXT("|");
	if (Hv != HoverKey)
	{
		HoverKey = Hv;
		HoverBox->ClearChildren();
		for (int32 i = 0; i < Model.Hover.Num(); ++i)
			HoverBox->AddSlot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(i == 0 ? 10 : 9, i == 0)).ColorAndOpacity(i == 0 ? FSovStyle::Gold : FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(Model.Hover[i]))];
	}
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
	FString Y = Model.bCity ? StatKey(Model.CityStats) + StatKey(Model.CityLiving) : FString();
	for (const FString& L : Model.CityLines) Y += L + TEXT("|");
	for (const FSovUIAction& A : Model.CityActions) Y += A.Icon.ToString() + A.Label + (A.bEnabled ? TEXT("1") : TEXT("0"));
	if (Y != CityKey)
	{
		CityKey = Y;
		RebuildCity();
	}
	if (Model.Tree.bOpen) TreeView->SetModel(Model.Tree);
	FString E = Model.bEnd ? Model.EndTitle + Model.EndSub : FString();
	for (const FString& L : Model.EndScores) E += L + TEXT("|");
	E += FString::FromInt(Model.EndChronicle.Num());
	if (E != EndKey)
	{
		EndKey = E;
		RebuildEnd();
	}
	FString Em = Model.bEmpire ? FString(TEXT("E")) : FString();
	for (const FSovUIStat& L : Model.EmpireLines) Em += L.Text + TEXT("|");
	if (Em != EmpireKey)
	{
		EmpireKey = Em;
		EmpireBox->ClearChildren();
		for (const FSovUIStat& L : Model.EmpireLines)
			EmpireBox->AddSlot().AutoHeight().Padding(0, 2)[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(L.Color).AutoWrapText(true).Text(FText::FromString(L.Text))];
	}
	FString G = FString::FromInt(Model.Lens);
	for (const FSovLensKey& K : Model.LensLegend) G += K.Label + K.Color.ToString();
	if (G != LegendKey)
	{
		LegendKey = G;
		LegendBox->ClearChildren();
		if (Model.Lens > 0)
			LegendBox->AddSlot().AutoHeight()[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(FString(SovLensName(static_cast<ESovLens>(Model.Lens))) + TEXT(" lens")))];
		for (const FSovLensKey& K : Model.LensLegend)
		{
			LegendBox->AddSlot().AutoHeight().Padding(0, 1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
				[SNew(SBox).WidthOverride(12).HeightOverride(12)[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(K.Color)]]
				+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(K.Label))]
			];
		}
	}
	FString N;
	for (const FSovUINotice& No : Model.Notices) N += No.Icon.ToString() + No.Text + No.Sub + (No.bUrgent ? TEXT("!") : TEXT("")) + TEXT("|");
	if (N != NoticesKey)
	{
		NoticesKey = N;
		RebuildNotices();
	}
	FString C = Model.bChooser ? Model.ChooserTitle : FString();
	for (const FSovUIChoice& L : Model.Choices) C += L.Section + L.Icon.ToString() + L.Label + L.Right + TEXT("|");
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

void SSovGameUI::RebuildCity()
{
	CityStatsBox->ClearChildren();
	for (const FSovUIStat& S : Model.CityStats) CityStatsBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)[StatWidget(S, 11)];
	CityLivingBox->ClearChildren();
	for (const FSovUIStat& S : Model.CityLiving) CityLivingBox->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[StatWidget(S, 10)];
	CityLinesBox->ClearChildren();
	for (const FString& L : Model.CityLines)
		CityLinesBox->AddSlot().AutoHeight().Padding(0, 1)[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(L))];
	CityActionsBox->ClearChildren();
	for (const FSovUIAction& A : Model.CityActions)
	{
		const FKey Key = A.Key;
		CityActionsBox->AddSlot()
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).IsEnabled(A.bEnabled).ToolTipText(FText::FromString(A.Label))
			.OnClicked_Lambda([this, Key]() { OnKey.ExecuteIfBound(Key); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(22).HeightOverride(22)[SNew(SImage).Image(FSovStyle::Icon(A.Icon))]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 2, 0)
				[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(A.Label.Left(A.Label.Find(TEXT(" (")) > 0 ? A.Label.Find(TEXT(" (")) : A.Label.Len())))]
			]
		];
	}
}

void SSovGameUI::RebuildEnd()
{
	EndScoresBox->ClearChildren();
	for (int32 i = 0; i < Model.EndScores.Num(); ++i)
	{
		FString Name, Score;
		if (!Model.EndScores[i].Split(TEXT("|"), &Name, &Score)) Name = Model.EndScores[i];
		const FLinearColor Color = i == 0 ? FSovStyle::Gold : FSovStyle::Text;
		EndScoresBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(STextBlock).Font(FSovStyle::Font(11, i == 0)).ColorAndOpacity(Color).Text(FText::FromString(Name))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Font(FSovStyle::Font(11, i == 0)).ColorAndOpacity(Color).Text(FText::FromString(Score))]
		];
	}
	EndChronicleBox->ClearChildren();
	for (const FString& L : Model.EndChronicle)
		EndChronicleBox->AddSlot().AutoHeight().Padding(0, 1)[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(L))];
}

void SSovGameUI::RebuildNotices()
{
	NoticesBox->ClearChildren();
	for (int32 i = 0; i < Model.Notices.Num(); ++i)
	{
		const FSovUINotice& No = Model.Notices[i];
		NoticesBox->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(No.Text + TEXT("\n") + No.Sub))
				.OnClicked_Lambda([this, i]() { OnNotice.ExecuteIfBound(i); return FReply::Handled(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(FSovStyle::Icon(No.Icon))]]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(FSovStyle::Font(10, No.bUrgent)).ColorAndOpacity(No.bUrgent ? FSovStyle::Gold : FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(No.Text))]
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(STextBlock).Font(FSovStyle::Font(8)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(No.Sub))]
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(3, 0, 0, 0)
			[
				SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(TEXT("Dismiss")))
				.Visibility(No.bUrgent ? EVisibility::Collapsed : EVisibility::Visible)
				.OnClicked_Lambda([this, i]() { OnDismiss.ExecuteIfBound(i); return FReply::Handled(); })
				[SNew(STextBlock).Font(FSovStyle::Font(8, true)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(TEXT("X")))]
			]
		];
	}
}

void SSovGameUI::RebuildChooser()
{
	ChoicesBox->ClearChildren();
	FString Section;
	for (int32 i = 0; i < Model.Choices.Num(); ++i)
	{
		const FSovUIChoice& Ch = Model.Choices[i];
		if (!Ch.Section.IsEmpty() && Ch.Section != Section)
		{
			Section = Ch.Section;
			ChoicesBox->AddSlot().AutoHeight().Padding(2, i == 0 ? 0 : 8, 0, 2)
			[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Gold).Text(FText::FromString(Section.ToUpper()))];
		}
		const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0, 8, 0)
			[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(i < 9 ? FString::FromInt(i + 1) : FString(TEXT(" "))))];
		if (!Ch.Icon.IsNone())
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[SNew(SBox).WidthOverride(20).HeightOverride(20)[SNew(SImage).Image(FSovStyle::Icon(Ch.Icon))]];
		Row->AddSlot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Text(FText::FromString(Ch.Label))];
		if (!Ch.Right.IsEmpty())
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 2, 0)[SNew(STextBlock).Font(FSovStyle::Font(10)).ColorAndOpacity(FSovStyle::Dim).Text(FText::FromString(Ch.Right))];
		ChoicesBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).OnClicked_Lambda([this, i]() { OnPick.ExecuteIfBound(i); return FReply::Handled(); })
			[Row]
		];
	}
}
