// Victories and score (09-civs-eras-victory-climate.md, Victory conditions; 00-overview.md, Score).
#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

// Player 0's warrior stands next to player 1's beaten capital at (8,5); a third civ
// (player 2) holds a capital at (14,2) unless `third` is false.
std::unique_ptr<Game> lastStand(bool domination, bool third, UnitId* warrior) {
    GameState s = flatState(16, 12, third ? 3 : 2);
    s.setup.dominationVictory = domination;
    addCity(s, 0, {2, 5}, true);
    addCity(s, 1, {8, 5}, true, 4);
    s.city(2)->hp = 1;
    if (third) addCity(s, 2, {14, 2}, true);
    *warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    auto g = Game::fromScenario(rules(), std::move(s));
    g->submit(Command::declareWar(0, 1));
    return g;
}
}  // namespace

TEST(score_counts_civ_line_items) {
    GameState s = flatState(20, 14, 2);
    addCity(s, 0, {4, 4}, true, 3);
    const CityId second = addCity(s, 0, {10, 4}, false, 1);
    s.city(second)->districts.push_back({rules().district("DISTRICT_CAMPUS"), {12, 4}, true});
    s.city(second)->districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {11, 6}, false});
    Player& p = s.players[0];
    p.techs.resize(rules().techs.size());
    p.civics.resize(rules().civics.size());
    p.techs.done[at(rules().tech("TECH_POTTERY"))] = 1;
    p.techs.done[at(rules().tech("TECH_MINING"))] = 1;
    p.civics.done[at(rules().civic("CIVIC_CODE_OF_LAWS"))] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    // 3 x 1 civic + 2 x 2 techs + 5 x 2 cities + 2 x 1 finished district + 4 population.
    CHECK_EQ(g->score(0), 3 + 4 + 10 + 2 + 4);
    CHECK_EQ(g->score(1), 0);
    CHECK_EQ(g->turnLimit(), 500);  // Standard speed calendar
}

TEST(domination_needs_every_rival_capital) {
    UnitId warrior = kNoUnit;
    auto g = lastStand(true, true, &warrior);
    CHECK_EQ(g->submit(Command::attack(0, warrior, {8, 5})), CommandError::Ok);
    CHECK_EQ(g->state().city(2)->owner, 0);
    CHECK(!g->gameOver());  // player 2 still holds its capital

    auto duel = lastStand(true, false, &warrior);
    CHECK_EQ(duel->submit(Command::attack(0, warrior, {8, 5})), CommandError::Ok);
    REQUIRE(duel->gameOver());
    CHECK_EQ(duel->state().winner, 0);
    CHECK(duel->state().victory == Victory::Domination);
    // The game is over: every command is refused.
    CHECK_EQ(duel->submit(Command::endTurn(0)), CommandError::GameOver);
    CHECK_EQ(duel->submit(Command::setActivity(0, warrior, Activity::Sleep)), CommandError::GameOver);
    ai::playTurn(*duel);  // the AI does nothing either
    CHECK_EQ(duel->log().size(), 2u);  // declare war, attack
}

// A civ that fell before founding a city has no capital to take; one still alive without a city holds out.
TEST(domination_skips_a_civ_that_fell_without_a_city) {
    const auto won = [](bool thirdAlive) {
        GameState s = flatState(16, 12, 3);
        s.setup.dominationVictory = true;
        addCity(s, 0, {2, 5}, true);
        const CityId theirs = addCity(s, 1, {8, 5}, true, 4);
        s.city(theirs)->hp = 1;
        s.players[2].alive = thirdAlive;
        if (thirdAlive) addUnit(s, "UNIT_SETTLER", 2, {14, 2});
        const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        auto g = Game::fromScenario(rules(), std::move(s));
        g->submit(Command::declareWar(0, 1));
        CHECK_EQ(g->submit(Command::attack(0, warrior, {8, 5})), CommandError::Ok);
        CHECK_EQ(g->state().players[2].alive, thirdAlive);
        return g->gameOver() && g->state().victory == Victory::Domination;
    };
    CHECK(won(false));
    CHECK(!won(true));
}

TEST(last_civ_standing_wins_without_domination) {
    UnitId warrior = kNoUnit;
    auto g = lastStand(false, false, &warrior);
    CHECK_EQ(g->submit(Command::attack(0, warrior, {8, 5})), CommandError::Ok);
    CHECK(!g->state().players[1].alive);
    REQUIRE(g->gameOver());
    CHECK_EQ(g->state().winner, 0);
    CHECK(g->state().victory == Victory::LastStanding);
}

TEST(score_decides_at_the_turn_limit) {
    GameState s = flatState(20, 14, 2);
    s.setup.turnLimit = 2;
    addCity(s, 0, {4, 4}, true, 2);
    addCity(s, 1, {12, 8}, true, 5);
    auto g = Game::fromScenario(rules(), std::move(s));
    endTurns(*g, 2);  // turn 1 played
    CHECK(!g->gameOver());
    CHECK_EQ(g->state().turn, 2);
    endTurns(*g, 2);  // turn 2, the last, played
    REQUIRE(g->gameOver());
    CHECK_EQ(g->state().winner, 1);  // bigger city
    CHECK(g->state().victory == Victory::Score);

    // The result survives a save round trip.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
    CHECK_EQ(loaded->state().winner, 1);
    CHECK(loaded->state().victory == Victory::Score);
    CHECK_EQ(loaded->state().setup.turnLimit, 2);
}

TEST(all_ai_game_runs_to_a_victory) {
    GameSetup setup;
    setup.seed = 2;
    setup.mapSize = "MAPSIZE_TINY";
    setup.turnLimit = 150;
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    for (int guard = 0; guard < 2000 && !g->gameOver(); ++guard) ai::playTurn(*g);
    REQUIRE(g->gameOver());
    const PlayerId w = g->state().winner;
    for (const Player& p : g->state().players) {
        if (!p.barbarian && p.alive && g->state().victory == Victory::Score) CHECK(g->score(w) >= g->score(p.id));
    }
    auto again = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(again);
    CHECK_EQ(again->stateHash(), g->stateHash());
    CHECK_EQ(again->state().winner, w);
}
