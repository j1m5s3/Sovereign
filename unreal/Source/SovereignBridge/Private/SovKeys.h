// Key rebinding (plan E, step 2). The controller reads its order keys by their default ("logical") key;
// this maps each to the physical key that triggers it now. Rebinding swaps, so no two actions share a key,
// and the bindings are kept in GameUserSettings.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

struct FSovKeyAction
{
	FKey Logical;   // the default key, as the controller reads it
	FString Label;  // what it does
};

namespace SovKeys
{
// The actions that can be rebound, in the order the settings screen lists them.
const TArray<FSovKeyAction>& Actions();
// The physical key for an action's default key (itself when not rebound or not rebindable).
FKey Physical(const FKey& Logical);
// Whether a physical key may take an action (not movement, digits, Esc, Enter, Space, Tab, modifiers or the mouse).
bool CanBind(const FKey& Key);
// Binds an action to a key; an action already on that key takes this one's old key.
void Bind(const FKey& Logical, const FKey& Key);
void ResetAll();
void Load();  // from GameUserSettings (once, at start-up)
void Save();
}  // namespace SovKeys
