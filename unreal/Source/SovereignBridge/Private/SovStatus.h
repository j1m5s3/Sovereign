// The empire's standing as plain lines: its identity, faith, era and age, diplomacy, the world's contests,
// climate, governors, the leader (plan E, step 1). The canvas HUD prints them without the widgets; the game
// screen shows them in its Empire panel.
#pragma once

#include "CoreMinimal.h"

#include "sovereign/state.h"

namespace sov
{
class Game;
}

struct FSovStatusLine
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
};

TArray<FSovStatusLine> SovStatusLines(const sov::Game& G, sov::PlayerId Me);
// How to play, a line each (the first is the title).
TArray<FString> SovHelpLines();
