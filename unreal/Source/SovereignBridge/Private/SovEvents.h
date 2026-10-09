// What the game's events say to a player (the HUD and the notification feed share it; plan D, step 4).
#pragma once

#include "CoreMinimal.h"

#include "sovereign/state.h"

namespace sov
{
class Game;
}

bool SovEventIsWorldNews(const sov::GameEvent& E);
// Whether this player hears of it: their own doings, gossip as far as their access goes, and world news.
bool SovEventHeard(const sov::Game& G, sov::PlayerId Me, const sov::GameEvent& E);
// One line for it, as the player would hear it (empty: nothing to say).
FString SovEventText(const sov::Game& G, sov::PlayerId Me, const sov::GameEvent& E);
FName SovEventIcon(const sov::GameEvent& E);
