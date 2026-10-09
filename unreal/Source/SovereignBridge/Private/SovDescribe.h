// Rules effects in words (plan E): a modifier as a short phrase ("+1 Production in every city with a
// garrison"), and everything a policy card does. The rules data has no descriptions for most cards, so the
// screens build them from the typed modifiers.
#pragma once

#include "CoreMinimal.h"

#include "sovereign/rules.h"

FString SovModifierText(const sov::Rules& R, const sov::Modifier& M);
// What a source does: its modifiers' phrases, joined ("; "); empty when it has none.
FString SovSourceText(const sov::Rules& R, sov::ModSource Kind, sov::TypeIndex Index);
// A building in words: its yields, housing, amenities, slots, upkeep and modifiers.
FString SovBuildingText(const sov::Rules& R, sov::TypeIndex Building);
// A unit in words: strength, ranged strength and range, moves, and what it replaces.
FString SovUnitText(const sov::Rules& R, sov::TypeIndex Unit);
// A promotion in words: each effect, with the situations it holds in ("+7 Combat Strength when attacking").
FString SovPromotionText(const sov::Rules& R, sov::TypeIndex Promotion);
// An improvement in words: its yields, housing and appeal.
FString SovImprovementText(const sov::Rules& R, sov::TypeIndex Improvement);
