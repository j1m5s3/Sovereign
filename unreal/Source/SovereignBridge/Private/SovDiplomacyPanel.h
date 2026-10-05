// The diplomacy screen (leader doc §10): the leader's relationship, agenda and reasons on the
// right, the conversation in the middle, a line to type in, and buttons for what the rules
// allow now (put the proposal forward, answer the leader's own offer, denounce, leave).
// Plain Slate over the viewport; every action goes back to the player controller.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SScrollBox;
class FSovDiplomacyTalk;

class SSovDiplomacyPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovDiplomacyPanel) {}
	SLATE_ARGUMENT(FSovDiplomacyTalk*, Talk)
	SLATE_ARGUMENT(TFunction<FText()>, Header)       // name, relationship, agenda
	SLATE_ARGUMENT(TFunction<FText()>, Reasons)      // why they feel as they do
	SLATE_ARGUMENT(TFunction<FText()>, Proposal)     // the last proposal and the rules' verdict ("" when none)
	SLATE_ARGUMENT(TFunction<FText()>, TheirOffer)   // the leader's waiting offer ("" when none)
	SLATE_ARGUMENT(TFunction<bool()>, CanPropose)
	SLATE_ARGUMENT(TFunction<bool()>, CanDenounce)
	SLATE_ARGUMENT(TFunction<void(const FString&)>, OnSay)
	SLATE_ARGUMENT(TFunction<void()>, OnPropose)
	SLATE_ARGUMENT(TFunction<void(bool)>, OnAnswerOffer)
	SLATE_ARGUMENT(TFunction<void()>, OnDenounce)
	SLATE_ARGUMENT(TFunction<void()>, OnLeave)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	TSharedPtr<SEditableTextBox> GetInput() const { return Input; }

private:
	void RebuildLines();

	FArguments Args;
	TSharedPtr<SEditableTextBox> Input;
	TSharedPtr<SScrollBox> Scroll;
	int32 ShownLines = -1;
};
