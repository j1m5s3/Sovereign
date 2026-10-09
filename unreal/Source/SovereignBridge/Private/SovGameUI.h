// The game screen's widgets over the map (plan D, step 1): the top bar, the unit panel, the chooser
// list and the end-turn button. The controller fills an FSovUIModel each frame and answers the
// widgets' clicks; a button presses the key that already does its job, so keys and clicks agree.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SHorizontalBox;
class SWrapBox;

struct FSovUIStat
{
	FName Icon;
	FString Text;
	FString Tip;
	FLinearColor Color = FLinearColor::White;
	FKey Key;  // pressed when clicked (none: not clickable)
};

struct FSovUIAction
{
	FName Icon;
	FString Label;  // the tooltip: what it does, and its key
	FKey Key;
	bool bEnabled = true;
};

struct FSovUIModel
{
	bool bVisible = false;
	// Top bar.
	TArray<FSovUIStat> Stats;  // yields and banks, left to right
	FSovUIStat Research, Civic, Government, Turn;
	float ResearchProgress = 0.f, CivicProgress = 0.f;
	// The selected unit.
	bool bUnit = false;
	FString UnitName, UnitSub;
	float UnitHealth = 1.f;
	TArray<FSovUIStat> UnitStats;
	TArray<FSovUIAction> UnitActions;
	// The open chooser.
	bool bChooser = false;
	FString ChooserTitle;
	TArray<FString> Choices;
	// End turn.
	bool bMyTurn = false;
	bool bTurnReady = false;  // nothing blocks it
	FString TurnLabel, TurnDetail;
};

class SSovGameUI : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovGameUI) {}
	SLATE_EVENT(TDelegate<void(FKey)>, OnKey)       // a button pressed a key
	SLATE_EVENT(TDelegate<void(int32)>, OnPick)     // a chooser line, by index
	SLATE_EVENT(TDelegate<void()>, OnEndTurn)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	// The latest state; rebuilds only the lists whose contents changed.
	void SetModel(const FSovUIModel& InModel);

private:
	TSharedRef<SWidget> StatWidget(const FSovUIStat& Stat, int32 Size = 14);
	void RebuildStats();
	void RebuildUnit();
	void RebuildChooser();

	FSovUIModel Model;
	FString StatsKey, UnitKey, ChooserKey;  // what each list was last built from
	TSharedPtr<SHorizontalBox> StatsBox;
	TSharedPtr<SHorizontalBox> UnitStatsBox;
	TSharedPtr<SWrapBox> ActionsBox;
	TSharedPtr<SVerticalBox> ChoicesBox;
	TDelegate<void(FKey)> OnKey;
	TDelegate<void(int32)> OnPick;
	TDelegate<void()> OnEndTurn;
};
