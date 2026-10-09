// The government screen (plan E): the governments that can be adopted with their slots, the current
// government's policy slots as cards coloured by kind, and the cards that fit the chosen slot. Opens in
// place of the F2 list; the number keys still pick from that list.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SWrapBox;

struct FSovGovOption
{
	int32 Index = -1;     // Rules::governments
	FString Name, Detail; // "Tier 1", "+1 favor a turn"
	int32 Slots[4] = {0, 0, 0, 0};  // military, economic, diplomatic, wildcard
	bool bCurrent = false;
	bool bCanAdopt = false;
};

struct FSovGovSlot
{
	int32 Kind = 0;  // sov::PolicySlot: military, economic, diplomatic, wildcard, great person
	FString Policy;  // empty: no card
};

struct FSovGovCard
{
	int32 Index = -1;  // Rules::policies
	FString Name;
	int32 Kind = 0;
};

struct FSovGovModel
{
	bool bOpen = false;
	FString Title, Note;  // "Monarchy", "Anarchy: 2 turns" / "Changes are free this turn"
	TArray<FSovGovOption> Governments;
	TArray<FSovGovSlot> Slots;
	int32 Selected = -1;  // the slot whose cards are listed
	TArray<FSovGovCard> Cards;
	FString BuyChanges;   // "Open changes this turn (120 gold)"; empty: not offered
	TArray<TPair<int32, FString>> Dedications;  // index, name
};

class SSovGovernmentView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovGovernmentView) {}
	SLATE_EVENT(TDelegate<void(int32)>, OnAdopt)       // a government
	SLATE_EVENT(TDelegate<void(int32)>, OnSlot)        // a slot to fill
	SLATE_EVENT(TDelegate<void(int32)>, OnCard)        // a card for the selected slot
	SLATE_EVENT(TDelegate<void(int32)>, OnDedication)
	SLATE_EVENT(TDelegate<void()>, OnBuyChanges)
	SLATE_EVENT(TDelegate<void()>, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetModel(const FSovGovModel& InModel);

	static FLinearColor KindColor(int32 Kind);
	static const TCHAR* KindName(int32 Kind);

private:
	void Rebuild();

	FSovGovModel Model;
	FString Key;
	TSharedPtr<SVerticalBox> GovBox, CardBox, DedicationBox;
	TSharedPtr<SWrapBox> SlotBox;
	TDelegate<void(int32)> OnAdopt, OnSlot, OnCard, OnDedication;
	TDelegate<void()> OnBuyChanges, OnClose;
};
