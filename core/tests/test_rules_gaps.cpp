// Rules found missing by an audit of the specs (01, 02, 03, 05): city spacing across water, occupied cities,
// Theocracy's Faith purchases, the Heavy Chariot, Observation units, the Giant Death Robot.
#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

TEST(cities_may_stand_closer_across_water) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {5, 5}, true, 3);
    for (Plot& p : s.plots) p.continent = 0;
    s.plot({8, 5}).continent = 1;  // three plots away, on another landmass
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->canFoundCityAt(0, {8, 5}));
    CommandError why = CommandError::Ok;
    CHECK(!g->canFoundCityAt(0, {5, 8}, &why));  // three away on the same landmass
    CHECK_EQ(why, CommandError::TooCloseToCity);
}

TEST(occupied_cities_do_not_grow) {
    auto foodAfter = [](bool war) {
        GameState s = flatState(20, 12, 2);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        addCity(s, 0, {4, 5}, true, 3);
        const CityId taken = addCity(s, 0, {10, 5}, false, 2);
        addCity(s, 1, {16, 5}, true, 3);
        s.city(taken)->originalOwner = 1;
        s.city(taken)->population = 1;
        for (const Hex& h : s.grid.within({10, 5}, 1)) s.plot(h).owner = 0, s.plot(h).city = taken;
        s.city(taken)->worked = {s.grid.index({11, 5})};
        s.players[0].relations.resize(2);
        s.players[1].relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = war;
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 2);
        return g->state().city(taken)->food;
    };
    CHECK(foodAfter(true) < foodAfter(false));
}

TEST(theocracy_buys_land_units_with_faith) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {5, 5}, true, 3);
    auto plain = Game::fromScenario(rules(), s);
    s.players[0].government = rules().government("GOVERNMENT_THEOCRACY");
    s.players[0].policies.assign(static_cast<size_t>(rules().governments[static_cast<size_t>(s.players[0].government)].totalSlots()), kNone);
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem warrior{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")};
    CHECK_EQ(plain->faithPurchaseCost(0, plain->state().cities[0], warrior), -1);
    CHECK_EQ(g->faithPurchaseCost(0, g->state().cities[0], warrior), g->purchaseCost(0, warrior) * 85 / 100);
}

TEST(support_units_and_open_ground) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const UnitId chariot = addUnit(s, "UNIT_HEAVY_CHARIOT", 0, {3, 3});
    const UnitId catapult = addUnit(s, "UNIT_CATAPULT", 0, {8, 8});
    const UnitId lone = addUnit(s, "UNIT_CATAPULT", 0, {14, 8});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {9, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    const UnitType& ct = rules().units[static_cast<size_t>(rules().unit("UNIT_HEAVY_CHARIOT"))];
    CHECK_EQ(g->maxMoves(*g->state().unit(chariot)), ct.moves + 1);  // flat grassland, no feature
    CHECK_EQ(g->unitRange(*g->state().unit(catapult)), g->unitRange(*g->state().unit(lone)) + 1);
}
