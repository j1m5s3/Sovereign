// The tech and civic trees (plan D, step 3): every node laid out by era and by its prerequisites, with
// lines between them, what each costs and unlocks, and its boost. Clicking an open node researches it;
// clicking a later one makes it the goal, and the controller picks the path toward it.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SConstraintCanvas;
class SSovTreeLines;

enum class ESovTreeState : uint8
{
	Locked,     // prerequisites missing
	Available,  // can be chosen now
	Current,    // being researched
	Goal,       // on the way to the goal (or the goal itself)
	Done
};

struct FSovTreeNode
{
	FString Name;
	int32 Era = 0;
	TArray<int32> Prereqs;  // node indices
	ESovTreeState State = ESovTreeState::Locked;
	FString Turns;     // "5 turns", or empty when done
	FString Boost;     // the boost's condition (empty: none)
	bool bBoosted = false;
	FString Unlocks;   // what it opens, comma separated
	float Progress = 0.f;
};

struct FSovTreeModel
{
	bool bOpen = false;
	bool bCivics = false;
	FString Title, Detail;  // "Technology", "Researching Pottery: 3 turns. Goal: Writing"
	TArray<FString> Eras;   // era names by index
	TArray<FSovTreeNode> Nodes;
};

class SSovTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovTreeView) {}
	SLATE_EVENT(TDelegate<void(int32)>, OnNode)  // a node clicked
	SLATE_EVENT(TDelegate<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetModel(const FSovTreeModel& InModel);
	// Scrolls so the node being researched (or else the goal, or else the first open one) is in view.
	void ScrollToCurrent();

private:
	void Layout();     // columns and rows, when the tree itself changes
	void Rebuild();    // node widgets, when a node's look changes

	FSovTreeModel Model;
	FString ShapeKey, LookKey;
	TArray<FVector2D> Pos;  // per node, top-left in the canvas
	FVector2D Extent = FVector2D::ZeroVector;
	TArray<TPair<int32, float>> EraStarts;  // era index, x where its first column begins
	TSharedPtr<SConstraintCanvas> Canvas;
	TSharedPtr<SSovTreeLines> Lines;
	TSharedPtr<class SBox> Sizer;
	TSharedPtr<class SScrollBox> HScroll, VScroll;
	TDelegate<void(int32)> OnNode;
	TDelegate<void()> OnClose;
};
