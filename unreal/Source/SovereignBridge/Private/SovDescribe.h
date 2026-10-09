// Rules effects in words (plan E): a modifier as a short phrase ("+1 Production in every city with a
// garrison"), and everything a policy card does. The rules data has no descriptions for most cards, so the
// screens build them from the typed modifiers.
#pragma once

#include "CoreMinimal.h"

#include "sovereign/rules.h"

FString SovModifierText(const sov::Rules& R, const sov::Modifier& M);
// What a source does: its modifiers' phrases, joined ("; "); empty when it has none.
FString SovSourceText(const sov::Rules& R, sov::ModSource Kind, sov::TypeIndex Index);
