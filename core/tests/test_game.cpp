#include "helpers.h"
#include "../tools/random_bot.h"
#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"

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

// Cities are found by id after one is lost, when their ids no longer run one after another; a lost id finds none.
TEST(game_finds_cities_by_id_after_one_is_lost) {
    GameState s = flatState(20, 10, 1);
    const CityId a = sovtest::addCity(s, 0, {3, 3}, true), b = sovtest::addCity(s, 0, {9, 3}, false);
    const CityId c = sovtest::addCity(s, 0, {15, 3}, false);
    s.cities.erase(s.cities.begin() + 1);  // b razed
    const GameState& seen = s;
    CHECK(s.city(a) == &s.cities[0]);
    CHECK(s.city(b) == nullptr);
    CHECK(seen.city(b) == nullptr);
    CHECK(s.city(c) == &s.cities[1]);
    CHECK(seen.city(c) == &s.cities[1]);
    CHECK(s.city(c + 1) == nullptr);
    CHECK(s.city(kNoCity) == nullptr);
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

// A pillaged road counts for nothing at either end of a step until it is repaired (01: Routes).
TEST(game_a_pillaged_road_counts_for_nothing) {
    GameState s = flatState(16, 12, 1);
    setTerrain(s, {5, 4}, "TERRAIN_GRASS_HILLS");
    for (const Hex& h : {Hex{4, 4}, Hex{5, 4}}) s.plot(h).route = 0;  // an Ancient Road
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 4});
    auto g = Game::fromScenario(rules(), s);
    const Fixed road = rules().routes[0].moveCost;
    REQUIRE(road < Fixed::fromInt(2));
    CHECK_EQ(*g->moveCost(*g->state().unit(w), {4, 4}, {5, 4}), road);
    for (const Hex& h : {Hex{4, 4}, Hex{5, 4}}) {
        GameState t = s;
        t.plot(h).routePillaged = true;
        auto p = Game::fromScenario(rules(), t);
        CHECK_EQ(*p->moveCost(*p->state().unit(w), {4, 4}, {5, 4}), Fixed::fromInt(2));  // up the hills
        CHECK_EQ(*p->moveCost(*p->state().unit(w), {5, 4}, {4, 4}), Fixed::fromInt(1));  // down onto flat ground
    }
}

// A river on any of a plot's six edges costs its crossing both ways, and only across that edge; an Ancient Road
// pays it too, a Classical Road bridges it (01: Routes).
TEST(game_river_crossings_in_every_direction) {
    const Hex from{6, 6};
    for (int d = 0; d < kNumDirs; ++d) {
        for (int road = -1; road <= 1; ++road) {  // none, then Rules::routes[0] (Ancient) and [1] (Classical)
            GameState s = flatState(16, 12, 1);
            setRiver(s, from, static_cast<Dir>(d));
            s.plot(from).route = static_cast<int8_t>(road);
            for (int e = 0; e < kNumDirs; ++e) s.plot(*s.grid.neighbor(from, static_cast<Dir>(e))).route = static_cast<int8_t>(road);
            UnitId w = addUnit(s, "UNIT_WARRIOR", 0, from);
            auto g = Game::fromScenario(rules(), std::move(s));
            const Unit& u = *g->state().unit(w);
            const Hex to = *g->state().grid.neighbor(from, static_cast<Dir>(d));
            const Fixed across = Fixed::fromInt(road == 1 ? 1 : 3);  // flat ground or a road, plus 2 to cross
            CHECK_EQ(*g->moveCost(u, from, to), across);
            CHECK_EQ(*g->moveCost(u, to, from), across);
            for (int e = 0; e < kNumDirs; ++e) {
                if (e != d) CHECK_EQ(*g->moveCost(u, from, *g->state().grid.neighbor(from, static_cast<Dir>(e))), Fixed::fromInt(1));
            }
        }
    }
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
    // Another player's unit (no war yet) may be passed, but no move ends on it.
    CHECK_EQ(g->moveCost(*g->state().unit(b), {4, 5}, {4, 6}).value_or(Fixed()), Fixed::fromInt(1));
    CHECK_EQ(g->submit(Command::move(0, b, {4, 6})), CommandError::NoPath);
}

// A unit may pass its owner's units of its layer but never end a turn on one (05: Stacking): its path plans no turn's
// end there, so it stops short and passes next turn, goes around, or has no path.
TEST(game_moves_never_end_on_our_units) {
    // A wall of mountains at x = 5 with one gap, (5,6), where one of our Builders stands; hills beyond it if asked.
    const auto wall = [](bool hills) {
        GameState s = flatState(20, 14, 1);
        s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        for (int y = 0; y < 14; ++y) {
            if (y != 6) setTerrain(s, {5, y}, "TERRAIN_GRASS_MOUNTAIN");
            if (hills) setTerrain(s, {6, y}, "TERRAIN_GRASS_HILLS");
        }
        return s;
    };
    {
        // In the gap a Builder has 1 MP left, too little for the hills (2): it would end its turn on the other one.
        GameState s = wall(true);
        const UnitId a = addUnit(s, "UNIT_BUILDER", 0, {5, 6});
        const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {4, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK_EQ(g->submit(Command::move(0, b, {7, 6})), CommandError::NoPath);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{4, 6}));
        // The other one goes back through b's plot, which it gets past.
        REQUIRE(g->submit(Command::move(0, a, {3, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(a)->pos, (Hex{3, 6}));
        REQUIRE(g->submit(Command::move(0, b, {7, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{5, 6}));
    }
    {
        // One of another layer is no obstacle: a Warrior's move ends in the gap with the Builder.
        GameState s = wall(true);
        addUnit(s, "UNIT_BUILDER", 0, {5, 6});
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::move(0, w, {7, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(w)->pos, (Hex{5, 6}));
    }
    {
        // From (3,6) a Builder would reach the gap with no moves left: it waits a plot short and passes next turn,
        // as its path says.
        GameState s = wall(false);
        const UnitId a = addUnit(s, "UNIT_BUILDER", 0, {5, 6});
        const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {3, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        const std::optional<std::vector<PathStep>> path = g->findPath(b, {7, 6});
        REQUIRE(path.has_value());
        CHECK_EQ(path->back().turn, 2);
        REQUIRE(g->submit(Command::move(0, b, {7, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{4, 6}));
        REQUIRE(g->submit(Command::setActivity(0, a, Activity::Sleep)) == CommandError::Ok);
        REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{6, 6}));
        REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{7, 6}));
    }
    {
        // Meanwhile another of ours took the order's end, beyond the one in the gap: the order ends where b stands.
        GameState s = wall(false);
        const UnitId a = addUnit(s, "UNIT_BUILDER", 0, {5, 6});
        const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {3, 6});
        const UnitId c = addUnit(s, "UNIT_BUILDER", 0, {7, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::move(0, b, {6, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{4, 6}));
        REQUIRE(g->submit(Command::move(0, c, {6, 6})) == CommandError::Ok);
        REQUIRE(g->submit(Command::setActivity(0, a, Activity::Sleep)) == CommandError::Ok);
        REQUIRE(g->submit(Command::setActivity(0, c, Activity::Sleep)) == CommandError::Ok);
        REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{4, 6}));
        CHECK(!g->state().unit(b)->moveTarget.has_value());
    }
    for (const char* mover : {"UNIT_SCOUT", "UNIT_WARRIOR"}) {
        // Two of ours in a row, before the gap and in it: a Scout (3 MP) gets past both in one move; a Warrior (2 MP)
        // may stop on neither, so it goes around the first and passes the second next turn.
        GameState s = wall(false);
        const UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        const UnitId b = addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
        const UnitId m = addUnit(s, mover, 0, {3, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        const bool scout = std::string(mover) == "UNIT_SCOUT";
        const std::optional<std::vector<PathStep>> path = g->findPath(m, {6, 6});
        REQUIRE(path.has_value());
        CHECK_EQ(path->back().turn, scout ? 0 : 1);
        if (scout) {
            REQUIRE(path->size() == 4u);
            CHECK_EQ((*path)[1].pos, (Hex{4, 6}));
            CHECK_EQ((*path)[2].pos, (Hex{5, 6}));
        }
        REQUIRE(g->submit(Command::move(0, m, {6, 6})) == CommandError::Ok);
        REQUIRE(g->submit(Command::setActivity(0, a, Activity::Fortify)) == CommandError::Ok);
        REQUIRE(g->submit(Command::setActivity(0, b, Activity::Fortify)) == CommandError::Ok);
        if (!scout) {
            CHECK(g->state().unit(m)->pos != (Hex{4, 6}));
            REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
        }
        CHECK_EQ(g->state().unit(m)->pos, (Hex{6, 6}));
    }
    {
        // The wall at x = 6 now, one of ours in its gap (6,6). A Horseman (4 MP) a hill away from the gap reaches it
        // with 1 MP from (5,6), or the turn after from (5,5), across a river: that later arrival with more moves does
        // not stand for the earlier one, which goes on this turn.
        GameState s = flatState(20, 14, 1);
        s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        for (int y = 0; y < 14; ++y) {
            if (y != 6) setTerrain(s, {6, y}, "TERRAIN_GRASS_MOUNTAIN");
        }
        setTerrain(s, {5, 5}, "TERRAIN_GRASS_HILLS");
        setTerrain(s, {5, 6}, "TERRAIN_GRASS_HILLS");
        setTerrain(s, {5, 7}, "TERRAIN_GRASS_MOUNTAIN");
        s.plot({5, 5}).riverEdges = kRiverSE;  // between (5,5) and (6,6)
        addUnit(s, "UNIT_WARRIOR", 0, {6, 6});
        const UnitId h = addUnit(s, "UNIT_HORSEMAN", 0, {4, 5});
        auto g = Game::fromScenario(rules(), std::move(s));
        const std::optional<std::vector<PathStep>> path = g->findPath(h, {7, 6});
        REQUIRE(path.has_value());
        CHECK_EQ(path->back().turn, 0);
        CHECK_EQ((*path)[1].pos, (Hex{5, 6}));
    }
    {
        // On open ground a Warrior goes around one of ours standing before woods rather than stop on it.
        GameState s = flatState(20, 14, 1);
        s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        setFeature(s, {5, 6}, "FEATURE_FOREST");
        const UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {3, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::setActivity(0, a, Activity::Fortify)) == CommandError::Ok);
        REQUIRE(g->submit(Command::move(0, w, {7, 6})) == CommandError::Ok);
        for (int turn = 0; turn < 3 && g->state().unit(w)->moveTarget; ++turn) {
            CHECK(g->state().unit(w)->pos != g->state().unit(a)->pos);
            REQUIRE(g->submit(Command::endTurn(0)) == CommandError::Ok);
        }
        CHECK_EQ(g->state().unit(w)->pos, (Hex{7, 6}));
    }
}

// A unit may pass another player's units as well while not at war with it, but never end a move on one, of any
// layer (05: Stacking); at war it keeps out of their plots (attacks and captures are their own commands).
TEST(game_moves_pass_other_players_units_at_peace) {
    // The wall of mountains at x = 5 again, another player's Warrior in its gap (5,6); hills beyond it if asked.
    const auto wall = [](bool hills, bool war) {
        GameState s = flatState(20, 14, 2);
        s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        for (int y = 0; y < 14; ++y) {
            if (y != 6) setTerrain(s, {5, y}, "TERRAIN_GRASS_MOUNTAIN");
            if (hills) setTerrain(s, {6, y}, "TERRAIN_GRASS_HILLS");
        }
        addUnit(s, "UNIT_WARRIOR", 1, {5, 6});
        s.units.back().activity = Activity::Sleep;
        for (Player& p : s.players) p.relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = war;
        return s;
    };
    {
        // Neither a Warrior nor a Builder may stop on it. The Warrior passes it to the plot beyond; the Builder, two
        // plots back, would reach it with no moves left, so it waits a plot short and passes next turn.
        GameState s = wall(false, false);
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {3, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK_EQ(g->moveCost(*g->state().unit(w), {4, 6}, {5, 6}).value_or(Fixed()), Fixed::fromInt(1));
        CHECK_EQ(g->submit(Command::move(0, w, {5, 6})), CommandError::NoPath);
        CHECK_EQ(g->submit(Command::move(0, b, {5, 6})), CommandError::NoPath);
        CHECK(!g->moveReach(b)[static_cast<size_t>(g->state().grid.index({5, 6}))]);
        REQUIRE(g->submit(Command::move(0, w, {7, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(w)->pos, (Hex{6, 6}));
        REQUIRE(g->submit(Command::move(0, b, {6, 7})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{4, 6}));
        sovtest::endTurns(*g, 2);
        CHECK_EQ(g->state().unit(b)->pos, (Hex{6, 6}));
    }
    {
        // With hills beyond, a Warrior would stop on it with 1 MP left: no path. A Scout gets past with 3.
        GameState s = wall(true, false);
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        const UnitId sc = addUnit(s, "UNIT_SCOUT", 0, {4, 5});
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK_EQ(g->submit(Command::move(0, w, {7, 6})), CommandError::NoPath);
        REQUIRE(g->submit(Command::move(0, sc, {6, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(sc)->pos, (Hex{6, 6}));
    }
    {
        // At war the gap is closed to moves.
        GameState s = wall(false, true);
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK(!g->moveCost(*g->state().unit(w), {4, 6}, {5, 6}).has_value());
        CHECK_EQ(g->submit(Command::move(0, w, {7, 6})), CommandError::NoPath);
    }
    {
        // Two walls, at x = 5 and x = 7, forest in both gaps, our Warrior in the second. A Heavy Chariot has 3 MP where
        // its turn starts on open ground, 2 in forest. It passes the other player's Warrior with 1 MP left: from that
        // forest the search finds no way on, as 2 MP a turn cannot pass ours, so it goes on as its path said, and
        // passes ours next turn.
        GameState s = wall(false, false);
        for (int y = 0; y < 14; ++y) {
            if (y != 6) setTerrain(s, {7, y}, "TERRAIN_GRASS_MOUNTAIN");
        }
        setFeature(s, {5, 6}, "FEATURE_FOREST");
        setFeature(s, {7, 6}, "FEATURE_FOREST");
        addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
        const UnitId c = addUnit(s, "UNIT_HEAVY_CHARIOT", 0, {4, 6});
        s.units.back().movesLeft = Fixed::fromInt(3);
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->maxMoves(*g->state().unit(c)) == 3);
        REQUIRE(g->submit(Command::move(0, c, {8, 6})) == CommandError::Ok);
        CHECK_EQ(g->state().unit(c)->pos, (Hex{6, 6}));
        CHECK(g->state().unit(c)->moveTarget.has_value());
        for (const Unit& u : g->state().units) {
            if (u.owner == 0 && u.id != c) REQUIRE(g->submit(Command::setActivity(0, u.id, Activity::Sleep)) == CommandError::Ok);
        }
        sovtest::endTurns(*g, 2);
        CHECK_EQ(g->state().unit(c)->pos, (Hex{8, 6}));
    }
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

// An Observation Balloon's own ability (Unobstructed View) sees through woods, as Sentry does.
TEST(observation_balloon_sees_through_woods) {
    GameState s = flatState(24, 16, 1);
    setFeature(s, {5, 8}, "FEATURE_FOREST");
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {4, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->visibility(0, {6, 8}) == Visibility::Visible);  // behind the woods
    CHECK(g->visibility(0, {7, 8}) == Visibility::Visible);
}

// Units on one plot each see their own way: a Sentry Scout beside a Warrior sees through the woods the Warrior does
// not, and a Settler (sight 3) a plot further than both.
TEST(units_on_one_plot_each_see_their_own_way) {
    GameState s = flatState(24, 16, 1);
    setFeature(s, {5, 8}, "FEATURE_FOREST");
    addUnit(s, "UNIT_WARRIOR", 0, {4, 8});
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {4, 8});
    s.unit(scout)->promotions = {rules().promotion("PROMOTION_SENTRY")};
    addUnit(s, "UNIT_SETTLER", 0, {4, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->visibility(0, {6, 8}) == Visibility::Visible);  // behind the woods
    CHECK(g->visibility(0, {1, 8}) == Visibility::Visible);  // three plots west
    CHECK(g->visibility(0, {0, 8}) == Visibility::Unrevealed);
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

// A human player sees fogged plots as it last saw them (world-scale: "revealed hexes use their last-seen state"): a
// city that grew or was razed in the fog, or a farm taken away, stays as it was until seen again.
TEST(fogged_plots_keep_their_last_seen_state) {
    GameState s = flatState(30, 14, 2);
    s.players[0].human = true;
    const CityId theirs = sovtest::addCity(s, 1, {12, 6}, true, 3);
    const TypeIndex farm = rules().improvement("IMPROVEMENT_FARM");
    s.plot({11, 7}).improvement = farm;
    sovtest::claimFor(s, *s.city(theirs), {11, 7});
    CityDistrict campus;
    campus.type = rules().district("DISTRICT_CAMPUS");
    campus.pos = {11, 6};
    campus.complete = true;
    s.city(theirs)->districts.push_back(campus);
    sovtest::claimFor(s, *s.city(theirs), {11, 6});
    const UnitId scout = addUnit(s, "UNIT_WARRIOR", 0, {10, 6});
    for (Unit& u : s.units) u.activity = Activity::Sleep;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->visibility(0, {12, 6}) == Visibility::Visible);
    REQUIRE(g->visibility(0, {11, 7}) == Visibility::Visible);
    CHECK(g->lastSeen(0, {12, 6}) == nullptr);  // in sight: the live plot
    const std::string name = g->state().city(theirs)->name;
    // The scout walks off; the city and its lands fall out of sight.
    g->stateMutForTests().unit(scout)->pos = {3, 6};
    sovtest::endTurns(*g, 2);
    REQUIRE(g->visibility(0, {12, 6}) == Visibility::Revealed);
    const int popThen = g->state().city(theirs)->population;
    // Meanwhile the city grows and is renamed, the farm goes and the campus is pillaged, all out of sight.
    GameState& live = g->stateMutForTests();
    live.city(theirs)->population = 9;
    live.city(theirs)->name = "Elsewhere";
    live.plot({11, 7}).improvement = kNone;
    live.city(theirs)->districts.back().pillagedTurns = 30;
    const PlotMemory* center = g->lastSeen(0, {12, 6});
    REQUIRE(center != nullptr);
    CHECK_EQ(center->city, theirs);
    CHECK_EQ(center->cityOwner, 1);
    CHECK_EQ(center->cityName, name);
    CHECK_EQ(center->cityPopulation, popThen);
    CHECK(center->capital);
    CHECK_EQ(center->owner, 1);
    const PlotMemory* field = g->lastSeen(0, {11, 7});
    REQUIRE(field != nullptr);
    CHECK_EQ(field->improvement, farm);
    CHECK_EQ(field->city, kNoCity);
    const PlotMemory* lab = g->lastSeen(0, {11, 6});
    REQUIRE(lab != nullptr);
    CHECK_EQ(lab->district, rules().district("DISTRICT_CAMPUS"));
    CHECK(lab->districtComplete && !lab->districtPillaged);
    CHECK(g->lastSeen(0, {28, 6}) == nullptr);  // never revealed
    CHECK(g->lastSeen(1, {10, 6}) == nullptr);  // an AI keeps no memory
    // The memory lives in the save.
    std::string err;
    auto back = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(back);
    CHECK_EQ(back->stateHash(), g->stateHash());
    REQUIRE(back->lastSeen(0, {12, 6}) != nullptr);
    CHECK_EQ(back->lastSeen(0, {12, 6})->cityName, name);
    CHECK_EQ(back->lastSeen(0, {11, 7})->improvement, farm);
    // Further turns out of sight keep the old snapshot.
    sovtest::endTurns(*g, 2);
    REQUIRE(g->lastSeen(0, {12, 6}) != nullptr);
    CHECK_EQ(g->lastSeen(0, {12, 6})->cityName, name);
    CHECK_EQ(g->lastSeen(0, {11, 7})->improvement, farm);
    // Seen again, the plot is live once more, and leaving sight again takes a new snapshot.
    g->stateMutForTests().unit(scout)->pos = {10, 6};
    sovtest::endTurns(*g, 2);
    CHECK(g->lastSeen(0, {12, 6}) == nullptr);
    g->stateMutForTests().unit(scout)->pos = {3, 6};
    sovtest::endTurns(*g, 2);
    REQUIRE(g->lastSeen(0, {12, 6}) != nullptr);
    CHECK_EQ(g->lastSeen(0, {12, 6})->cityPopulation, g->state().city(theirs)->population);
    CHECK_EQ(g->lastSeen(0, {11, 7})->improvement, kNone);
    CHECK(g->lastSeen(0, {11, 6})->districtPillaged);
}
