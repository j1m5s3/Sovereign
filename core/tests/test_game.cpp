#include "helpers.h"
#include "../tools/random_bot.h"
#include "sovereign/mapgen.h"

using namespace sov;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
void setTerrain(GameState& s, Hex h, const char* id) { s.plot(h).terrain = rules().terrain(id); }
void setFeature(GameState& s, Hex h, const char* id) { s.plot(h).feature = rules().feature(id); }
}  // namespace

TEST(game_starts_with_settler_and_warrior) {
    std::string err;
    auto g = Game::create(rules(), sovtest::duelSetup(42), &err);
    REQUIRE(g);
    const GameState& s = g->state();
    CHECK_EQ(s.turn, 1);
    CHECK_EQ(s.currentPlayer, 0);
    CHECK_EQ(s.units.size(), 6u);  // Settler, Warrior and the leader each
    REQUIRE(s.players.size() == 3u);
    CHECK(s.players[2].barbarian && g->atWar(0, 2) && g->atWar(2, 1) && !g->atWar(0, 1));
    for (const Player& p : s.players) {
        if (p.barbarian) continue;
        int settlers = 0, warriors = 0, leaders = 0;
        for (const Unit& u : s.units) {
            if (u.owner != p.id) continue;
            settlers += u.type == rules().unit("UNIT_SETTLER");
            warriors += u.type == rules().unit("UNIT_WARRIOR");
            leaders += u.type == rules().leaderUnit;
            CHECK(g->state().grid.distance(u.pos, p.startPos) <= 2);
            CHECK(g->visibility(p.id, u.pos) == Visibility::Visible);
        }
        CHECK_EQ(settlers, 1);
        CHECK_EQ(warriors, 1);
        CHECK_EQ(leaders, 1);
    }
}

TEST(game_found_city_rules) {
    GameState s = flatState(16, 12, 2);
    UnitId settler = addUnit(s, "UNIT_SETTLER", 0, {4, 4});
    UnitId settler2 = addUnit(s, "UNIT_SETTLER", 0, {6, 4});
    UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
    setTerrain(s, {10, 4}, "TERRAIN_COAST");
    UnitId wet = addUnit(s, "UNIT_SETTLER", 0, {10, 4});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::foundCity(0, warrior)), CommandError::NotASettler);
    CHECK_EQ(g->submit(Command::foundCity(1, settler)), CommandError::NotYourTurn);
    CHECK_EQ(g->submit(Command::foundCity(0, wet)), CommandError::CannotFoundHere);
    REQUIRE(g->submit(Command::foundCity(0, settler)) == CommandError::Ok);
    const GameState& st = g->state();
    REQUIRE(st.cities.size() == 1u);
    CHECK(st.cities[0].capital);
    CHECK_EQ(st.cities[0].name, std::string("London"));
    CHECK(st.unit(settler) == nullptr);
    for (const Hex& h : st.grid.within({4, 4}, 1)) CHECK_EQ(st.plot(h).owner, 0);
    CHECK_EQ(st.plot({4, 6}).owner, kNoPlayer);
    // Within CITY_MIN_RANGE (3) of another city.
    CHECK_EQ(g->submit(Command::foundCity(0, settler2)), CommandError::TooCloseToCity);
    CHECK_EQ(g->log().size(), 1u);  // rejected commands are not logged
}

TEST(game_movement_costs) {
    GameState s = flatState(16, 12, 1);
    setTerrain(s, {5, 4}, "TERRAIN_GRASS_HILLS");
    setTerrain(s, {6, 4}, "TERRAIN_GRASS_HILLS");
    setFeature(s, {6, 4}, "FEATURE_FOREST");
    setTerrain(s, {7, 4}, "TERRAIN_GRASS_MOUNTAIN");
    setRiver(s, {3, 6}, Dir::E);
    UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 4});
    auto g = Game::fromScenario(rules(), std::move(s));
    const Unit& u = *g->state().unit(w);
    CHECK_EQ(*g->moveCost(u, {4, 4}, {3, 4}), Fixed::fromInt(1));
    CHECK_EQ(*g->moveCost(u, {4, 4}, {5, 4}), Fixed::fromInt(2));   // hills
    CHECK_EQ(*g->moveCost(u, {5, 4}, {6, 4}), Fixed::fromInt(3));   // woods on hills
    CHECK(!g->moveCost(u, {6, 4}, {7, 4}).has_value());             // mountain
    CHECK_EQ(*g->moveCost(u, {3, 6}, {4, 6}), Fixed::fromInt(3));   // river crossing + flat
    CHECK_EQ(*g->moveCost(u, {4, 6}, {3, 6}), Fixed::fromInt(3));   // either direction
}

TEST(game_full_moves_can_always_enter_one_plot) {
    GameState s = flatState(16, 12, 1);
    setTerrain(s, {5, 4}, "TERRAIN_GRASS_HILLS");
    setFeature(s, {5, 4}, "FEATURE_FOREST");
    UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 4});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::move(0, w, {5, 4})) == CommandError::Ok);
    CHECK_EQ(g->state().unit(w)->pos, (Hex{5, 4}));
    CHECK_EQ(g->state().unit(w)->movesLeft, Fixed());
}

TEST(game_partial_moves_wait_for_next_turn) {
    GameState s = flatState(16, 12, 1);
    setTerrain(s, {6, 4}, "TERRAIN_GRASS_HILLS");
    UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 4});
    auto g = Game::fromScenario(rules(), std::move(s));
    // 1 MP to (5,4), then 1 MP left is not enough for hills (2): stop and wait.
    REQUIRE(g->submit(Command::move(0, w, {6, 4})) == CommandError::Ok);
    CHECK_EQ(g->state().unit(w)->pos, (Hex{5, 4}));
    CHECK(g->state().unit(w)->moveTarget.has_value());
    REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
    CHECK_EQ(g->state().turn, 2);
    CHECK_EQ(g->state().unit(w)->pos, (Hex{6, 4}));
    CHECK(!g->state().unit(w)->moveTarget.has_value());
}

TEST(game_multi_turn_path) {
    GameState s = flatState(20, 12, 1);
    UnitId sc = addUnit(s, "UNIT_SCOUT", 0, {2, 5});
    // Reveal the whole map so the path can be planned.
    auto g = Game::fromScenario(rules(), std::move(s));
    GameState copy = g->state();
    for (Player& p : copy.players) std::fill(p.visibility.begin(), p.visibility.end(), 1);
    g = Game::fromScenario(rules(), copy);
    auto path = g->findPath(sc, {12, 5});
    REQUIRE(path.has_value());
    CHECK_EQ(path->back().pos, (Hex{12, 5}));
    CHECK_EQ(path->back().turn, 3);  // 10 plots at 3 MP per turn
    REQUIRE(g->submit(Command::move(0, sc, {12, 5})) == CommandError::Ok);
    CHECK_EQ(g->state().unit(sc)->pos, (Hex{5, 5}));
    for (int i = 0; i < 3; ++i) REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
    CHECK_EQ(g->state().unit(sc)->pos, (Hex{12, 5}));
}

TEST(game_one_unit_per_tile) {
    GameState s = flatState(16, 12, 2);
    UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {4, 4});
    UnitId b = addUnit(s, "UNIT_WARRIOR", 0, {5, 4});
    UnitId settler = addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    addUnit(s, "UNIT_WARRIOR", 1, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::move(0, a, {5, 4})), CommandError::BadTarget);  // own military there
    // Pass through a friendly unit to the plot beyond it.
    REQUIRE(g->submit(Command::move(0, a, {6, 4})) == CommandError::Ok);
    CHECK_EQ(g->state().unit(a)->pos, (Hex{6, 4}));
    // A civilian can share a plot with a military unit.
    REQUIRE(g->submit(Command::move(0, settler, {5, 4})) == CommandError::Ok);
    CHECK_EQ(g->state().unit(settler)->pos, (Hex{5, 4}));
    CHECK_EQ(g->state().unit(b)->pos, (Hex{5, 4}));
    // Foreign units block (no war yet).
    CHECK(!g->moveCost(*g->state().unit(b), {5, 4}, {4, 6}).has_value());
}

TEST(game_end_turn_needs_orders_and_cycles_players) {
    GameState s = flatState(16, 12, 2);
    UnitId w0 = addUnit(s, "UNIT_WARRIOR", 0, {3, 3});
    UnitId w1 = addUnit(s, "UNIT_WARRIOR", 1, {12, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::endTurn(0)), CommandError::UnitsNeedOrders);
    REQUIRE(g->submit(Command::setActivity(0, w0, Activity::Fortify)) == CommandError::Ok);
    REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
    CHECK_EQ(g->state().currentPlayer, 1);
    CHECK_EQ(g->state().turn, 1);
    CHECK_EQ(g->submit(Command::endTurn(0)), CommandError::NotYourTurn);
    REQUIRE(g->submit(Command::setActivity(1, w1, Activity::Skip)) == CommandError::Ok);
    REQUIRE(g->submit(Command::endTurn(1)) == CommandError::Ok);
    CHECK_EQ(g->state().turn, 2);
    CHECK_EQ(g->state().currentPlayer, 0);
    // Fortified units stay fortified; skipped units wake up at their owner's next turn.
    CHECK(g->state().unit(w0)->activity == Activity::Fortify);
    CHECK(g->state().unit(w1)->activity == Activity::Skip);
    REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
    CHECK(g->state().unit(w1)->activity == Activity::Awake);
    CHECK_EQ(g->submit(Command::endTurn(1)), CommandError::UnitsNeedOrders);
    // Civilians cannot fortify.
    GameState s2 = flatState(8, 8, 1);
    UnitId civ = addUnit(s2, "UNIT_SETTLER", 0, {3, 3});
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK_EQ(g2->submit(Command::setActivity(0, civ, Activity::Fortify)), CommandError::BadActivity);
}

TEST(game_fog_of_war_and_line_of_sight) {
    GameState s = flatState(24, 16, 1);
    // Woods two plots east block the view beyond them for a unit on flat land.
    setFeature(s, {6, 8}, "FEATURE_FOREST");
    UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->visibility(0, {5, 8}) == Visibility::Visible);
    CHECK(g->visibility(0, {6, 8}) == Visibility::Visible);    // the woods themselves
    CHECK(g->visibility(0, {2, 8}) == Visibility::Visible);
    CHECK(g->visibility(0, {20, 8}) == Visibility::Unrevealed);
    // Can't plan a path into the unknown.
    CHECK_EQ(g->submit(Command::move(0, w, {20, 8})), CommandError::NoPath);
    // Moving away leaves revealed-but-fogged plots behind.
    REQUIRE(g->submit(Command::move(0, w, {3, 9})) == CommandError::Ok);
    REQUIRE(g->submit(Command::setActivity(0, w, Activity::Skip)) == CommandError::Ok);
    REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
    REQUIRE(g->submit(Command::move(0, w, {2, 9})) == CommandError::Ok);
    CHECK(g->visibility(0, {6, 8}) == Visibility::Revealed);
}

TEST(game_hills_see_over_woods) {
    GameState s = flatState(24, 16, 1);
    setFeature(s, {6, 8}, "FEATURE_FOREST");
    setTerrain(s, {4, 8}, "TERRAIN_GRASS_HILLS");
    addUnit(s, "UNIT_WARRIOR", 0, {4, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    // Sight 2 + 1 for hills = 3, and the woods no longer block.
    CHECK(g->visibility(0, {7, 8}) == Visibility::Visible);
    GameState s2 = flatState(24, 16, 1);
    setFeature(s2, {5, 8}, "FEATURE_FOREST");
    addUnit(s2, "UNIT_WARRIOR", 0, {4, 8});
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK(g2->visibility(0, {6, 8}) == Visibility::Unrevealed);
}

// Recon promotions (05): Sentry sees through woods, Spyglass a plot further.
TEST(sentry_sees_through_woods_and_spyglass_a_plot_further) {
    GameState s = flatState(24, 16, 1);
    setFeature(s, {5, 8}, "FEATURE_FOREST");
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {4, 8});
    auto plain = Game::fromScenario(rules(), GameState(s));
    CHECK(plain->visibility(0, {6, 8}) == Visibility::Unrevealed);  // behind the woods
    CHECK(plain->visibility(0, {1, 8}) == Visibility::Unrevealed);  // three plots west, past sight 2
    s.unit(scout)->promotions = {rules().promotion("PROMOTION_SENTRY"), rules().promotion("PROMOTION_SPYGLASS")};
    auto promoted = Game::fromScenario(rules(), std::move(s));
    CHECK(promoted->visibility(0, {6, 8}) == Visibility::Visible);
    CHECK(promoted->visibility(0, {1, 8}) == Visibility::Visible);
    CHECK_EQ(promoted->unitSight(*promoted->state().unit(scout)), 3);
}

TEST(game_replay_reproduces_state) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(77);
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    sovbot::playTurns(*g, 1234, 40);
    CHECK(g->state().turn == 41);
    CHECK(!g->state().cities.empty());
    auto r = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(r);
    CHECK_EQ(r->stateHash(), g->stateHash());
}
