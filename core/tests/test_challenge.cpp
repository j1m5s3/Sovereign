// The weekly challenge (player-retention §3): the week's setup, and the check of a game by replay.
#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/challenge.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::rules;

TEST(the_weekly_challenge_is_the_same_for_everyone) {
    CHECK_EQ(challengeWeek(1767571200), 0);                 // Monday 2026-01-05
    CHECK_EQ(challengeWeek(1767571200 + 7 * 86400 - 1), 0);
    CHECK_EQ(challengeWeek(1767571200 + 7 * 86400), 1);
    const Challenge a = weeklyChallenge(rules(), 40), b = weeklyChallenge(rules(), 40), c = weeklyChallenge(rules(), 41);
    CHECK_EQ(a.setup.seed, b.setup.seed);
    CHECK_EQ(a.text, b.text);
    CHECK(a.setup.seed != c.setup.seed);
    REQUIRE(a.setup.players.size() == 4u);
    CHECK(a.setup.players[0].human && !a.setup.players[1].human);
    CHECK(!a.setup.rivalMemory && a.setup.mods.empty());
    CHECK_EQ(a.setup.speed, std::string("GAMESPEED_SHORT_REIGN"));
    CHECK(a.goal != c.goal);
}

TEST(a_challenge_game_is_checked_by_replay) {
    const Challenge ch = weeklyChallenge(rules(), 7);
    std::string err;
    auto g = Game::create(rules(), ch.setup, &err);
    REQUIRE(g);
    // The AI plays the human's seat too, for a few turns.
    while (g->state().turn < 12 && !g->gameOver()) ai::playTurn(*g);
    const std::vector<uint8_t> save = saveGame(*g);
    const ChallengeResult r = checkChallenge(rules(), ch, save);
    CHECK(r.valid);
    CHECK_EQ(r.error, std::string());
    CHECK_EQ(r.turn, g->state().turn);
    CHECK_EQ(r.score, g->score(0));
    // Another week's challenge, or another game, is refused.
    CHECK(!checkChallenge(rules(), weeklyChallenge(rules(), 8), save).valid);
    GameSetup other = ch.setup;
    other.seed += 1;
    auto h = Game::create(rules(), other, &err);
    REQUIRE(h);
    const ChallengeResult wrong = checkChallenge(rules(), ch, saveGame(*h));
    CHECK(!wrong.valid);
    CHECK_EQ(wrong.error, std::string("not this week's challenge"));
}