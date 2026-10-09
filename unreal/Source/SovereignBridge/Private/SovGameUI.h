// The game screen's widgets over the map (plan D, step 1): the top bar, the unit panel, the chooser
// list and the end-turn button. The controller fills an FSovUIModel each frame and answers the
// widgets' clicks; a button presses the key that already does its job, so keys and clicks agree.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "SovLens.h"
#include "SovMinimap.h"
#include "SovTreeView.h"

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

// A notification: an event heard, or something waiting on the player (plan D, step 4).
struct FSovUINotice
{
	FName Icon;
	FString Text;
	FString Sub;  // "Turn 12", "Click to choose"
	bool bUrgent = false;  // waits on the player
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
	// The selected city (plan D, step 2): what it makes, how it grows, what it builds and holds.
	bool bCity = false;
	FString CityName, CitySub;
	float CityHealth = 1.f;
	TArray<FSovUIStat> CityStats;      // yields per turn
	float GrowthProgress = 0.f;
	FString GrowthText;
	TArray<FSovUIStat> CityLiving;     // housing, amenities
	FName ProductionIcon;
	FString ProductionName, ProductionText;
	float ProductionProgress = 0.f;
	FString BuyGold, BuyFaith;         // "Buy: 120" for what is being built (empty: no button)
	bool bBuyGold = false, bBuyFaith = false;  // affordable and allowed now
	int32 CityFocus = 0;               // sov::CityFocus: the yield its citizens favour
	TArray<FString> CityLines;         // queue, buildings, loyalty, religion...
	TArray<FSovUIAction> CityActions;
	// The open chooser.
	bool bChooser = false;
	FString ChooserTitle;
	TArray<FString> Choices;
	// The tech or civic tree, open in place of their list (plan D, step 3).
	FSovTreeModel Tree;
	// The end of the game (plan D, step 5): who won and how, the scores, and the player's chronicle.
	bool bEnd = false;
	bool bWon = false;
	FString EndTitle, EndSub;
	TArray<FString> EndScores;     // "Egypt (Ramesses II)|812", best first
	TArray<FString> EndChronicle;  // the reign's key lines, latest last
	// The latest message (saved, bought, refused...), under the top bar for a few seconds; Toast fades it out.
	FString Message;
	float MessageAlpha = 0.f;
	// The plot under the cursor (plan E, step 3): terrain, owner, yields and units; shown only over the map.
	TArray<FString> Hover;
	// The Empire panel (plan E, step 1): the empire's standing, line by line (Text and Color used).
	bool bEmpire = false;
	TArray<FSovUIStat> EmpireLines;
	// The map lens (ESovLens) with its legend, and the minimap with the camera's place on it (plan D, step 6).
	int32 Lens = 0;
	TArray<FSovLensKey> LensLegend;
	TSharedPtr<const FSovMinimapData> Minimap;
	FVector2D MinimapFocus = FVector2D(0.5, 0.5);
	// Notifications, newest first, above the end-turn button.
	TArray<FSovUINotice> Notices;
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
	SLATE_EVENT(TDelegate<void(int32)>, OnFocus)    // a city focus (sov::CityFocus)
	SLATE_EVENT(TDelegate<void(bool)>, OnBuy)       // buy what the city builds: true with faith
	SLATE_EVENT(TDelegate<void(int32)>, OnTreeNode) // a tech or civic clicked in the tree
	SLATE_EVENT(TDelegate<void(int32)>, OnNotice)   // a notification clicked
	SLATE_EVENT(TDelegate<void(int32)>, OnDismiss)  // a notification's X
	SLATE_EVENT(TDelegate<void()>, OnEndClose)      // look at the map after the game
	SLATE_EVENT(TDelegate<void(int32)>, OnLens)     // a lens button (ESovLens)
	SLATE_EVENT(TDelegate<void(FVector2D)>, OnMinimap)  // the minimap clicked, 0..1 across and down
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual void Tick(const FGeometry& Geometry, const double Time, const float Delta) override;
	// The latest state; rebuilds only the lists whose contents changed.
	void SetModel(const FSovUIModel& InModel);

private:
	TSharedRef<SWidget> StatWidget(const FSovUIStat& Stat, int32 Size = 14);
	void RebuildStats();
	void RebuildUnit();
	void RebuildChooser();
	void RebuildCity();
	void RebuildNotices();
	void RebuildEnd();

	FSovUIModel Model;
	FString StatsKey, UnitKey, ChooserKey;  // what each list was last built from
	TSharedPtr<SHorizontalBox> StatsBox;
	TSharedPtr<SHorizontalBox> UnitStatsBox;
	TSharedPtr<SWrapBox> ActionsBox;
	TSharedPtr<SVerticalBox> ChoicesBox;
	TSharedPtr<SHorizontalBox> CityStatsBox, CityLivingBox;
	TSharedPtr<SVerticalBox> CityLinesBox;
	TSharedPtr<SWrapBox> CityActionsBox;
	FString CityKey;
	TDelegate<void(FKey)> OnKey;
	TDelegate<void(int32)> OnPick;
	TDelegate<void()> OnEndTurn;
	TDelegate<void(int32)> OnFocus;
	TDelegate<void(bool)> OnBuy;
	TDelegate<void(int32)> OnTreeNode;
	TDelegate<void(int32)> OnNotice, OnDismiss;
	TSharedPtr<SVerticalBox> NoticesBox;
	TSharedPtr<SVerticalBox> EndScoresBox, EndChronicleBox;
	FString EndKey;
	TSharedPtr<SVerticalBox> LegendBox;
	TSharedPtr<SVerticalBox> EmpireBox;
	TSharedPtr<SVerticalBox> HoverBox;
	FString HoverKey;
	FGeometry LastGeometry;   // for placing the tooltip by the cursor
	bool bOverMap = false;    // the cursor is over the map, not one of the panels
	FString EmpireKey;
	FString LegendKey;
	TDelegate<void(int32)> OnLens;
	TDelegate<void(FVector2D)> OnMinimap;
	TDelegate<void()> OnEndClose;
	FString NoticesKey;
	TSharedPtr<SSovTreeView> TreeView;
};
