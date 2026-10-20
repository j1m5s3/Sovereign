#include "SovKeys.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
const TCHAR* const kSection = TEXT("Sovereign.Keys");
TMap<FKey, FKey> GBound;  // logical -> physical, only where they differ
}  // namespace

const TArray<FSovKeyAction>& SovKeys::Actions()
{
	static const TArray<FSovKeyAction> List = {
		{EKeys::F, TEXT("Found a city, use a great person, start a trade route")},
		{EKeys::B, TEXT("Build an improvement; fight a battle live")},
		{EKeys::K, TEXT("Skip the unit this turn")},
		{EKeys::G, TEXT("Fortify or sleep")},
		{EKeys::U, TEXT("Promote")},
		{EKeys::E, TEXT("The leader's gear")},
		{EKeys::L, TEXT("Escort link")},
		{EKeys::H, TEXT("The throne and bodyguards")},
		{EKeys::Q, TEXT("Walk the City Center")},
		{EKeys::V, TEXT("Benevolence")},
		{EKeys::X, TEXT("Fear")},
		{EKeys::R, TEXT("Auto-resolve a battle")},
		{EKeys::P, TEXT("Production")},
		{EKeys::T, TEXT("Research tree")},
		{EKeys::C, TEXT("Civics tree")},
		{EKeys::Y, TEXT("Great people")},
		{EKeys::Z, TEXT("Governors")},
		{EKeys::J, TEXT("Agents")},
		{EKeys::I, TEXT("Pantheon")},
		{EKeys::N, TEXT("Diplomacy")},
		{EKeys::O, TEXT("City-states")},
		{EKeys::Comma, TEXT("World Congress")},
		{EKeys::Period, TEXT("Next unit")},
		{EKeys::M, TEXT("Chat")},
		{EKeys::F1, TEXT("How to play")},
		{EKeys::F2, TEXT("Government and policies")},
		{EKeys::F3, TEXT("Plot yields")},
		{EKeys::F4, TEXT("The chronicle")},
		{EKeys::F5, TEXT("Quicksave")},
		{EKeys::F6, TEXT("Write the chronicle up")},
		{EKeys::F7, TEXT("Next map lens")},
		{EKeys::F8, TEXT("Empire panel")},
		{EKeys::F9, TEXT("Quickload")}};
	return List;
}

FKey SovKeys::Physical(const FKey& Logical)
{
	const FKey* Found = GBound.Find(Logical);
	return Found ? *Found : Logical;
}

bool SovKeys::CanBind(const FKey& Key)
{
	static const FKey Fixed[] = {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::Escape, EKeys::Enter,
		EKeys::SpaceBar, EKeys::Tab, EKeys::Home, EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven,
		EKeys::Eight, EKeys::Nine};
	if (!Key.IsValid() || Key.IsMouseButton() || Key.IsModifierKey() || Key.IsGamepadKey() || Key.IsAxis1D() || Key.IsAxis2D()) return false;
	for (const FKey& F : Fixed)
		if (Key == F) return false;
	return true;
}

void SovKeys::Bind(const FKey& Logical, const FKey& Key)
{
	if (!CanBind(Key)) return;
	const FKey Old = Physical(Logical);
	if (Old == Key) return;
	// Whoever holds Key now takes Old: the bindings stay one to one.
	for (const FSovKeyAction& A : Actions())
	{
		if (A.Logical != Logical && Physical(A.Logical) == Key)
		{
			if (Old == A.Logical) GBound.Remove(A.Logical);
			else GBound.Add(A.Logical, Old);
		}
	}
	if (Key == Logical) GBound.Remove(Logical);
	else GBound.Add(Logical, Key);
}

void SovKeys::ResetAll()
{
	GBound.Reset();
}

void SovKeys::Load()
{
	GBound.Reset();
	if (!GConfig) return;
	for (const FSovKeyAction& A : Actions())
	{
		FString Name;
		if (GConfig->GetString(kSection, *A.Logical.GetFName().ToString(), Name, GGameUserSettingsIni))
		{
			const FKey Key(*Name);
			if (CanBind(Key) && Key != A.Logical) GBound.Add(A.Logical, Key);
		}
	}
}

void SovKeys::Save()
{
	if (!GConfig) return;
	GConfig->EmptySection(kSection, GGameUserSettingsIni);
	for (const TPair<FKey, FKey>& B : GBound) GConfig->SetString(kSection, *B.Key.GetFName().ToString(), *B.Value.GetFName().ToString(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString SovKeys::KeyText(const FKey& Key)
{
	if (Key == EKeys::Comma) return TEXT(",");
	if (Key == EKeys::Period) return TEXT(".");
	return Key.GetDisplayName(false).ToString();
}

FString SovKeys::Expand(const FString& Text)
{
	// In two passes, through placeholders, so a swap (F <-> G) does not turn a key back into itself.
	FString Out = Text;
	const TArray<FSovKeyAction>& All = Actions();
	auto Mark = [](int32 i) { return FString::Printf(TEXT("%d"), i); };
	for (int32 i = 0; i < All.Num(); ++i)
	{
		Out.ReplaceInline(*(FString(TEXT("{")) + All[i].Logical.GetFName().ToString() + TEXT("}")), *Mark(i), ESearchCase::CaseSensitive);
		Out.ReplaceInline(*(FString(TEXT("(")) + KeyText(All[i].Logical) + TEXT(")")), *(TEXT("(") + Mark(i) + TEXT(")")), ESearchCase::CaseSensitive);
	}
	for (int32 i = 0; i < All.Num(); ++i) Out.ReplaceInline(*Mark(i), *KeyText(Physical(All[i].Logical)), ESearchCase::CaseSensitive);
	return Out;
}
