// Rules found missing by an audit of the specs (04, 06, 07, 08, 09): costs by the world era, legacy cards,
// city-state trade routes, Monarchy's envoys, the score's line items, the suzerain's resources.
#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
TypeIndex cityStateOf(CityStateKind kind) {
    for (size_t i = 0; i < rules().cityStates.size(); ++i) {
        if (rules().cityStates[i].kind == kind) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

// Player 0 is a major civ; player 1 a city-state of `kind` with its city at (14,5).
GameState withCityState(CityStateKind kind) {
    GameState s = flatState(22, 12, 2);
    s.players[1].civ = kNone;
    s.players[1].cityState = cityStateOf(kind);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(2, 0);
    }
    addCity(s, 0, {4, 5}, true, 3);
    addCity(s, 1, {14, 5}, true, 2);
    return s;
}
}  // namespace

TEST(nodes_cost_less_behind_the_world_era_and_more_ahead) {
    GameState s = flatState(16, 10, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const TypeIndex pottery = rules().tech("TECH_POTTERY");          // Ancient
    const TypeIndex machinery = rules().tech("TECH_MACHINERY");      // Medieval
    const TypeIndex currency = rules().tech("TECH_CURRENCY");        // Classical
    s.gameEra = rules().techs[static_cast<size_t>(currency)].era;
    auto g = Game::fromScenario(rules(), std::move(s));
    const auto base = [&](TypeIndex t) { return rules().techs[static_cast<size_t>(t)].cost; };
    // Relative to the listed costs at the same speed: -20% behind, +20% ahead, the same in the world's era.
    const int behind = g->techCost(pottery) * 100 / base(pottery), here = g->techCost(currency) * 100 / base(currency),
              ahead = g->techCost(machinery) * 100 / base(machinery);
    CHECK(behind < here);
    CHECK(here < ahead);
}

TEST(legacy_cards_follow_a_government_left_behind) {
    GameState s = flatState(16, 10, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const TypeIndex monarchy = rules().government("GOVERNMENT_MONARCHY");
    TypeIndex legacy = kNone;
    for (size_t i = 0; i < rules().policies.size(); ++i) {
        if (rules().policies[i].id == "POLICY_MONARCHIC_LEGACY") legacy = static_cast<TypeIndex>(i);
    }
    REQUIRE(monarchy != kNone && legacy != kNone);
    auto never = Game::fromScenario(rules(), s);
    CHECK(!never->policyAvailable(0, legacy));
    s.players[0].governmentUses[static_cast<size_t>(monarchy)] = 1;
    auto left = Game::fromScenario(rules(), s);
    CHECK(left->policyAvailable(0, legacy));
    s.players[0].government = monarchy;
    s.players[0].policies.assign(static_cast<size_t>(rules().governments[static_cast<size_t>(monarchy)].totalSlots()), kNone);
    auto still = Game::fromScenario(rules(), std::move(s));
    CHECK(!still->policyAvailable(0, legacy));  // not while it is the government
}

TEST(routes_to_a_city_state_bring_its_kind_of_yield) {
    const auto yieldsTo = [](CityStateKind kind) {
        auto g = Game::fromScenario(rules(), withCityState(kind));
        return g->tradeRouteYields(g->state().cities[0], g->state().cities[1]);
    };
    const Yields sci = yieldsTo(CityStateKind::Scientific), cul = yieldsTo(CityStateKind::Cultural),
                 trade = yieldsTo(CityStateKind::Trade);
    const auto y = [](const Yields& v, YieldType t) { return v[static_cast<size_t>(t)]; };
    CHECK(y(sci, YieldType::Science) == y(cul, YieldType::Science) + Fixed::fromInt(1));
    CHECK(y(cul, YieldType::Culture) == y(sci, YieldType::Culture) + Fixed::fromInt(1));
    CHECK(y(trade, YieldType::Gold) == y(sci, YieldType::Gold) + Fixed::fromInt(2));
}

TEST(the_score_counts_wonders_great_people_and_religion) {
    GameState s = flatState(16, 10, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {5, 5}, true, 3);
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(rules().building("BUILDING_PYRAMIDS"));
    s.players[0].greatPeopleRecruited.assign(rules().greatPersonClasses.size(), 0);
    s.players[0].greatPeopleRecruited[0] = 2;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->score(0), plain->score(0) + 15 + 10);
}

TEST(the_suzerain_gets_the_city_states_luxuries) {
    GameState s = withCityState(CityStateKind::Trade);
    TypeIndex lux = kNone;
    for (size_t i = 0; i < rules().resources.size() && lux == kNone; ++i) {
        if (rules().resources[i].amenityCities > 0 && rules().resources[i].reveal.none()) lux = static_cast<TypeIndex>(i);
    }
    REQUIRE(lux != kNone);
    s.plot({14, 5}).resource = lux;  // on the city center, which counts as improved
    s.plot({14, 5}).owner = 1;
    auto none = Game::fromScenario(rules(), s);
    CHECK(!none->hasLuxury(0, lux));
    s.players[0].envoys[1] = 3;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->suzerainOf(1) == 0);
    CHECK(g->hasLuxury(0, lux));
}
