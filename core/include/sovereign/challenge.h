// The weekly challenge (specs/sovereign/player-retention.md §3): every week everyone gets the same
// map seed, civ and rules, and a goal. A finished game is checked by replaying its command log from the
// week's setup; a live battle's result outside the battle band fails the check (leader doc §9).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sovereign/api.h"
#include "sovereign/state.h"

namespace sov {

class Game;
class Rules;

enum class ChallengeGoal : uint8_t { AnyVictory = 0, Peaceful, Science, Culture, Score };
constexpr int kNumChallengeGoals = 5;

struct Challenge {
    int32_t week = 0;
    GameSetup setup;  // seat 0 is the human; no carried profile, rivals or mods, so every player's game is the same
    ChallengeGoal goal = ChallengeGoal::AnyVictory;
    std::string text;  // "Week 40 challenge: win without declaring a war, as England (Short Reign, Classical start)"
};

// Weeks since Monday 2026-01-05 (UTC).
SOV_API int32_t challengeWeek(int64_t unixSeconds);
SOV_API Challenge weeklyChallenge(const Rules& rules, int32_t week);

// For the board: valid games rank by the goal met, then fewer turns, then a higher score.
struct ChallengeResult {
    bool valid = false;
    std::string error;
    bool goalMet = false;
    int32_t turn = 0;
    int32_t score = 0;
};
// Judges a finished (or abandoned) game of the challenge as it stands, without a replay.
SOV_API ChallengeResult judgeChallenge(const Game& game, const Challenge& challenge);
// Checks a save of the challenge: its setup is the week's, its log replays to the same game, and every
// live battle's result lies within the band; then judges the replayed game.
SOV_API ChallengeResult checkChallenge(const Rules& rules, const Challenge& challenge, const std::vector<uint8_t>& save);

}  // namespace sov
