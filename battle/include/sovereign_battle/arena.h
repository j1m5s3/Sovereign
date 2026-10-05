// Headless battles for the trainer and the tests: play a fight to the end with a commander
// on each side, draw random matchups, and score a result for one side.
#pragma once

#include "sovereign/api.h"
#include "sovereign_battle/commander.h"
#include "sovereign_battle/sim.h"

namespace sov::battle {

// Plays the battle to its end (rout, a fallen lone leader or the time cap).
SOV_API Result playOut(const Spec& spec, Commander& side0, Commander& side1, float dt = 0.1f);

// A random matchup like the ones the game produces: era-like strengths with a strength gap,
// damaged units, an escorted leader on one side most of the time, now and then a lone
// leader. Nobody at the controls.
SOV_API Spec randomScenario(Rng& rng, int maxStrengthGap = 15);

// How well a battle went for one side: the share of the enemy's HP taken minus the share of
// its own lost, with a large swing for a leader killed (the leader doc's big risk).
SOV_API float score(const Spec& spec, const Result& r, int side);

}  // namespace sov::battle
