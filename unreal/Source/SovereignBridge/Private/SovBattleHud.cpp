#include "SovBattleHud.h"

#include "SovStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> SSovBattleHud::KeyButton(const FString& Label, const FKey& Key, TFunction<bool()> Lit, const FString& Tip)
{
	return SNew(SBox).Padding(2)
	[
		SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button()).ToolTipText(FText::FromString(Tip))
		.ButtonColorAndOpacity_Lambda([Lit]() { return Lit && Lit() ? FSovStyle::Gold : FLinearColor::White; })
		.OnClicked_Lambda([this, Key]() { OnKey.ExecuteIfBound(Key); return FReply::Handled(); })
		[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Text).Justification(ETextJustify::Center).Text(FText::FromString(Label))]
	];
}

void SSovBattleHud::Construct(const FArguments& Args)
{
	OnKey = Args._OnKey;
	auto Side = [this](bool bAttacker) {
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(bAttacker ? HAlign_Right : HAlign_Left)
			[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this, bAttacker]() {
				return FText::FromString(FString::Printf(TEXT("%s  %d/%d"), bAttacker ? *Model.Attacker : *Model.Defender,
					bAttacker ? Model.AttackerAlive : Model.DefenderAlive, bAttacker ? Model.AttackerStarted : Model.DefenderStarted));
			})]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
			[SNew(SBox).WidthOverride(260).HeightOverride(8)[SNew(SProgressBar).Style(&FSovStyle::Progress())
				.BarFillType(bAttacker ? EProgressBarFillType::RightToLeft : EProgressBarFillType::LeftToRight)
				.FillColorAndOpacity(bAttacker ? FLinearColor(0.85f, 0.3f, 0.2f) : FLinearColor(0.3f, 0.55f, 0.9f))
				.Percent_Lambda([this, bAttacker]() {
					return bAttacker ? static_cast<float>(Model.AttackerAlive) / FMath::Max(1, Model.AttackerStarted)
									 : static_cast<float>(Model.DefenderAlive) / FMath::Max(1, Model.DefenderStarted);
				})]];
	};
	static const TCHAR* const OrderNames[] = {TEXT("Advance"), TEXT("Hold"), TEXT("Flank left"), TEXT("Flank right"), TEXT("Fall back"), TEXT("Hunt leader")};
	static const FKey OrderKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
	static const FKey PickKeys[] = {EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
	static const TCHAR* const SquadNames[] = {TEXT("Left"), TEXT("Centre"), TEXT("Right"), TEXT("All")};
	TSharedRef<SHorizontalBox> Squads = SNew(SHorizontalBox);
	for (int32 s = 0; s < 4; ++s)
	{
		Squads->AddSlot().AutoWidth()
		[
			SNew(SBox).WidthOverride(120)
			[
				SNew(SButton).IsFocusable(false).ButtonStyle(&FSovStyle::Button())
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Order the %s squad (%s)"), s < 3 ? SquadNames[s] : TEXT("whole army"), *PickKeys[s].GetDisplayName().ToString())))
				.ButtonColorAndOpacity_Lambda([this, s]() { return (s < 3 ? Model.Squad == s : Model.Squad < 0) ? FSovStyle::Gold : FLinearColor::White; })
				.OnClicked_Lambda([this, s]() { OnKey.ExecuteIfBound(PickKeys[s]); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Text).Text(FText::FromString(SquadNames[s]))]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[SNew(STextBlock).Font(FSovStyle::Font(8)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this, s]() { return FText::FromString(s < 3 ? Model.Orders[s] : FString(TEXT("every squad"))); })]
				]
			]
		];
	}
	TSharedRef<SHorizontalBox> Orders = SNew(SHorizontalBox);
	for (int32 o = 0; o < 6; ++o)
		Orders->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(110)[KeyButton(OrderNames[o], OrderKeys[o], nullptr, FString::Printf(TEXT("%s (%d)"), OrderNames[o], o + 1))]];
	ChildSlot
	[
		SNew(SOverlay)
		// The armies and the clock.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 12, 0, 0)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(14, 8))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[SNew(STextBlock).Font(FSovStyle::Font(12, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() { return FText::FromString(Model.Title); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Side(true)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0)
					[SNew(STextBlock).Font(FSovStyle::Font(16, true)).ColorAndOpacity(FSovStyle::Text).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%d s"), Model.Seconds)); })]
					+ SHorizontalBox::Slot().AutoWidth()[Side(false)]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
				[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim).Text_Lambda([this]() { return FText::FromString(Model.Foe); })]
			]
		]
		// The leader.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16, 16, 0, 0)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(10, 6))
			.Visibility_Lambda([this]() { return Model.LeaderHealth >= 0.f && !Model.bReplay ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(FSovStyle::Font(10, true)).ColorAndOpacity(FSovStyle::Gold).Text_Lambda([this]() {
					return FText::FromString(Model.LeaderHealth > 0.f ? FString::Printf(TEXT("Your leader: %d%%"), FMath::RoundToInt(Model.LeaderHealth * 100.f)) : FString(TEXT("Your leader is down!")));
				})]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
				[SNew(SBox).WidthOverride(200).HeightOverride(8)[SNew(SProgressBar).Style(&FSovStyle::Progress()).Percent_Lambda([this]() { return FMath::Max(0.f, Model.LeaderHealth); })
					.FillColorAndOpacity_Lambda([this]() { return Model.LeaderHealth > 0.5f ? FSovStyle::Good : Model.LeaderHealth > 0.25f ? FSovStyle::Gold : FSovStyle::Bad; })]]
			]
		]
		// The squads and their orders.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 14)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(10, 8))
			.Visibility_Lambda([this]() { return Model.bReplay || !Model.Result.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Squads]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)[Orders]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(160)[KeyButton(TEXT("Charge / hold (Tab)"), EKeys::Tab, [this]() { return Model.bCharging; }, TEXT("Every squad charges, or holds"))]]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(160)
						[SNew(SBox).Visibility_Lambda([this]() { return Model.bRemoteView ? EVisibility::Collapsed : EVisibility::Visible; })[KeyButton(TEXT("Strike (F)"), EKeys::F, nullptr, TEXT("Your leader strikes (also left click)"))]]]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(160)[KeyButton(TEXT("Settle now (Esc)"), EKeys::Escape, nullptr, TEXT("End the battle by the numbers"))]]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 0)
				[SNew(STextBlock).Font(FSovStyle::Font(9)).ColorAndOpacity(FSovStyle::Dim)
					.Text(FText::FromString(TEXT("WASD move your leader, hold right mouse or Q/E to look")))]
			]
		]
		// A replay: how to leave.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 14)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(12, 6))
			.Visibility_Lambda([this]() { return Model.bReplay ? EVisibility::Visible : EVisibility::Collapsed; })
			[KeyButton(TEXT("Leave the replay (Esc)"), EKeys::Escape, nullptr, TEXT("Back to where you were"))]
		]
		// The result.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(FSovStyle::Panel()).Padding(FMargin(26, 16))
			.Visibility_Lambda([this]() { return Model.Result.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[SNew(STextBlock).Font(FSovStyle::Font(28, true)).ColorAndOpacity_Lambda([this]() { return Model.bWon ? FSovStyle::Gold : FSovStyle::Bad; })
					.Text_Lambda([this]() { return FText::FromString(Model.ResultTitle); })]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)
				[SNew(SBox).MaxDesiredWidth(640)[SNew(STextBlock).Font(FSovStyle::Font(11)).ColorAndOpacity(FSovStyle::Text).AutoWrapText(true).Justification(ETextJustify::Center)
					.Text_Lambda([this]() { return FText::FromString(Model.Result); })]]
			]
		]
	];
}
