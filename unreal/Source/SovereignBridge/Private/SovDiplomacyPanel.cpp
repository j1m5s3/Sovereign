#include "SovDiplomacyPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "SovDiplomacy.h"

#define LOCTEXT_NAMESPACE "SovDiplomacy"

namespace
{
const FLinearColor kPanel(0.015f, 0.015f, 0.02f, 0.97f);
const FLinearColor kGold(1.f, 0.85f, 0.45f);
const FLinearColor kPlayer(0.75f, 0.85f, 1.f);
const FLinearColor kNote(0.65f, 0.65f, 0.65f);

FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
}  // namespace

void SSovDiplomacyPanel::Construct(const FArguments& InArgs)
{
	Args = InArgs;
	auto Button = [](const FText& Label, TFunction<void()> Click, TFunction<bool()> Enabled, TFunction<bool()> Shown) {
		return SNew(SBox).Padding(FMargin(4.f, 2.f))[
			SNew(SButton)
				.Text(Label)
				.IsEnabled_Lambda([Enabled]() { return !Enabled || Enabled(); })
				.Visibility_Lambda([Shown]() { return !Shown || Shown() ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked_Lambda([Click]() {
					if (Click) Click();
					return FReply::Handled();
				})];
	};
	FSovDiplomacyTalk* Talk = Args._Talk;
	const auto NotBusy = [Talk]() { return Talk && !Talk->IsBusy(); };
	const auto HasOffer = [this]() { return Args._TheirOffer && !Args._TheirOffer().IsEmpty(); };

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(1100.f).HeightOverride(640.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(kPanel)
			.Padding(16.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(16)).ColorAndOpacity(kGold).AutoWrapText(true)
					.Text_Lambda([this]() { return Args._Header ? Args._Header() : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 10.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.66f).Padding(0.f, 0.f, 12.f, 0.f)
					[
						SAssignNew(Scroll, SScrollBox)
					]
					+ SHorizontalBox::Slot().FillWidth(0.34f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(STextBlock).Font(Font(11)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f))
							.Text_Lambda([this]() { return Args._Reasons ? Args._Reasons() : FText::GetEmpty(); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(STextBlock).Font(Font(12)).AutoWrapText(true).ColorAndOpacity(kGold)
					.Text_Lambda([this]() { return Args._Proposal ? Args._Proposal() : FText::GetEmpty(); })
					.Visibility_Lambda([this]() { return Args._Proposal && !Args._Proposal().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(STextBlock).Font(Font(12)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.6f, 1.f, 0.7f))
					.Text_Lambda([this]() { return Args._TheirOffer ? Args._TheirOffer() : FText::GetEmpty(); })
					.Visibility_Lambda([HasOffer]() { return HasOffer() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					SAssignNew(Input, SEditableTextBox)
					.Font(Font(13))
					.HintText(LOCTEXT("Hint", "Speak to the leader, then press Enter (for example: \"I offer my Wine for 40 gold\")"))
					.IsEnabled_Lambda(NotBusy)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How) {
						if (How != ETextCommit::OnEnter || !Args._OnSay)
						{
							return;
						}
						Args._OnSay(Text.ToString());
						Input->SetText(FText::GetEmpty());
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Propose", "Put the proposal forward"), Args._OnPropose, Args._CanPropose, nullptr)]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("AcceptOffer", "Accept their offer"),
						[this]() { if (Args._OnAnswerOffer) Args._OnAnswerOffer(true); }, nullptr, HasOffer)]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("RejectOffer", "Reject their offer"),
						[this]() { if (Args._OnAnswerOffer) Args._OnAnswerOffer(false); }, nullptr, HasOffer)]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Denounce", "Denounce"), Args._OnDenounce, Args._CanDenounce, nullptr)]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Font(Font(10)).ColorAndOpacity(kNote)
						.Text_Lambda([Talk]() {
							if (!Talk) return FText::GetEmpty();
							if (Talk->IsBusy()) return LOCTEXT("Thinking", "The leader considers your words...");
							if (!Talk->Checked()) return FText::GetEmpty();
							return Talk->UsingModel() ? LOCTEXT("Model", "Voice: local model") : LOCTEXT("Scripted", "Voice: scripted (no model server)");
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Leave", "Leave (Esc)"), Args._OnLeave, NotBusy, nullptr)]
				]
			]
		]
	];
	RebuildLines();
}

void SSovDiplomacyPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (Args._Talk && Args._Talk->Lines().Num() != ShownLines)
	{
		RebuildLines();
	}
}

void SSovDiplomacyPanel::RebuildLines()
{
	if (!Scroll.IsValid() || !Args._Talk)
	{
		return;
	}
	Scroll->ClearChildren();
	const FString LeaderName = FString(UTF8_TO_TCHAR(Args._Talk->GetPersona().leaderName.c_str()));
	for (const FSovTalkLine& L : Args._Talk->Lines())
	{
		FString Text = L.Text;
		FLinearColor Color = kNote;
		if (L.Kind == FSovTalkLine::EKind::Player)
		{
			Text = TEXT("You: ") + L.Text;
			Color = kPlayer;
		}
		else if (L.Kind == FSovTalkLine::EKind::Leader)
		{
			Text = LeaderName + TEXT(": ") + L.Text;
			Color = FLinearColor::White;
		}
		Scroll->AddSlot().Padding(0.f, 3.f)[SNew(STextBlock).Font(Font(12)).AutoWrapText(true).ColorAndOpacity(Color).Text(FText::FromString(Text))];
	}
	ShownLines = Args._Talk->Lines().Num();
	Scroll->ScrollToEnd();
}

#undef LOCTEXT_NAMESPACE
