// Loyalty and the Free Cities [R&F] (02-cities.md, Loyalty).
#include <algorithm>

#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
template <typename Edit>
std::unique_ptr<Game> world(Edit edit) {
    GameState s = flatState(24, 14, 2);
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}

void pass(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        const PlayerId me = g.state().currentPlayer;
        for (UnitId id : g.unitsNeedingOrders(me)) g.submit(Command::setActivity(me, id, Activity::Skip));
        for (CityId id : g.citiesNeedingProduction(me)) g.submit(Command::setProduction(me, id, g.buildableItems(id).front()));
        sovtest::endTurns(g, 1);
    }
}

City& cityRef(GameState& s, CityId id) {
    return *std::find_if(s.cities.begin(), s.cities.end(), [&](const City& c) { return c.id == id; });
}
}  // namespace

TEST(loyalty_tables_from_civ_data) {
    REQUIRE(rules().loyaltyLevels.size() == 4u);
    CHECK_EQ(rules().loyaltyLevels.front().minLoyalty, 0);
    CHECK_EQ(rules().loyaltyLevels.front().yieldPercent, -100);    // Unrest
    CHECK_EQ(rules().loyaltyLevels[1].growthPercent, 25);          // Disloyal
    CHECK_EQ(rules().loyaltyLevels.back().minLoyalty, 76);         // Loyal
    int ecstatic = 0;
    for (const HappinessLevel& h : rules().happiness) if (h.id == "HAPPINESS_ECSTATIC") ecstatic = h.loyaltyPerTurn;
    CHECK_EQ(ecstatic, 6);
}

TEST(citizen_pressure_follows_the_civ_formula) {
    CityId small = kNoCity, home = kNoCity;
    auto g = world([&](GameState& s) {
        home = addCity(s, 0, {2, 7}, true, 3);
        small = addCity(s, 0, {9, 7}, false, 1);
        addCity(s, 1, {12, 7}, true, 8);  // a big foreign capital three plots away
    });
    // Small city: domestic = 1 x 10 + 3 x (10 - 7) x 2 (capital) = 28; foreign = 8 x 7 x 2 = 112.
    // 10 x (28 - 112) / (28 + 0.5) = -29.5, capped at -20.
    CHECK_EQ(g->loyaltyPressure(*g->state().city(small)), Fixed::fromInt(-20));
    CHECK(g->loyaltyPressure(*g->state().city(home)) > Fixed());
}

TEST(monument_and_loyalty_levels) {
    CityId c = kNoCity;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {5, 7}, true, 3);
        cityRef(s, c).buildings.push_back(rules().building("BUILDING_MONUMENT"));
        std::sort(cityRef(s, c).buildings.begin(), cityRef(s, c).buildings.end());
    });
    const Fixed withMonument = g->loyaltyPerTurn(c);
    GameState s = g->state();
    auto& b = cityRef(s, c).buildings;
    b.erase(std::remove(b.begin(), b.end(), rules().building("BUILDING_MONUMENT")), b.end());
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(withMonument, g2->loyaltyPerTurn(c) + Fixed::fromInt(1));

    // A Disloyal city (26-50) loses half its yields.
    const Fixed loyalProd = g->cityReport(c).yields[static_cast<size_t>(YieldType::Production)];
    GameState s3 = g->state();
    cityRef(s3, c).loyalty = 40;
    auto g3 = Game::fromScenario(rules(), std::move(s3));
    CHECK_EQ(g3->loyaltyLevel(*g3->state().city(c))->id, std::string("LOYALTY_DISLOYAL"));
    CHECK_EQ(g3->cityReport(c).yields[static_cast<size_t>(YieldType::Production)], loyalProd / 2);
}

TEST(a_city_at_zero_loyalty_revolts_to_the_free_cities) {
    CityId small = kNoCity;
    UnitId guard = 0;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        small = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, small).loyalty = 10;
        guard = addUnit(s, "UNIT_WARRIOR", 0, {9, 7});
        addCity(s, 1, {12, 7}, true, 8);
    });
    CHECK(g->freeCityPlayer() == kNoPlayer);
    pass(*g, 2);  // back to player 0: its cities' loyalty is processed
    const PlayerId fc = g->freeCityPlayer();
    REQUIRE(fc != kNoPlayer);
    const Player& f = g->state().players[static_cast<size_t>(fc)];
    CHECK(f.freeCity && f.barbarian);
    CHECK_EQ(g->state().city(small)->owner, fc);
    CHECK_EQ(g->state().unit(guard)->owner, fc);  // the garrison goes with it
    CHECK(g->atWar(0, fc) && g->atWar(1, fc));
    CHECK(g->barbarianPlayer() != fc);
}

TEST(free_cities_join_the_strongest_neighbour) {
    CityId fcity = kNoCity;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        fcity = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, fcity).loyalty = 1;
        addCity(s, 1, {12, 7}, true, 8);
    });
    pass(*g, 2);  // it revolts
    const PlayerId fc = g->freeCityPlayer();
    REQUIRE(fc != kNoPlayer && g->state().city(fcity)->owner == fc);
    CHECK(g->loyaltyPerTurn(fcity) < Fixed());  // +10 for itself, -20 from the big neighbour
    for (int i = 0; i < 12 && g->state().city(fcity)->owner == fc; ++i) pass(*g, 2);
    CHECK_EQ(g->state().city(fcity)->owner, 1);
    CHECK_EQ(g->state().city(fcity)->loyalty, rules().globalInt("LOYALTY_AFTER_TRANSFERRED_BY_CULTURAL_IDENTITY"));
}

TEST(captured_cities_start_half_loyal) {
    CityId target = kNoCity;
    UnitId sword = 0;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        target = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, target).hp = 1;
        addCity(s, 1, {20, 7}, true, 3);
        sword = addUnit(s, "UNIT_SWORDSMAN", 1, {10, 7});
    });
    g->submit(Command::declareWar(0, 1));
    pass(*g, 1);
    REQUIRE(g->submit(Command::attack(1, sword, {9, 7})) == CommandError::Ok);
    CHECK_EQ(g->state().city(target)->owner, 1);
    CHECK_EQ(g->state().city(target)->loyalty, rules().globalInt("LOYALTY_AFTER_TRANSFERRED_BY_COMBAT"));
}
