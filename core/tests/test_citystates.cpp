// City-states and envoys (08-diplomacy-city-states-governors.md, City-States).
#include "helpers.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
size_t yi(YieldType y) { return static_cast<size_t>(y); }

TypeIndex cityStateOf(CityStateKind kind) {
    for (size_t i = 0; i < rules().cityStates.size(); ++i) {
        if (rules().cityStates[i].kind == kind) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

// Players 0 and 1 are majors; player 2 is a Scientific city-state with its city at (16,6).
GameState csState() {
    GameState s = flatState(24, 14, 3);
    s.players[2].civ = kNone;
    s.players[2].cityState = cityStateOf(CityStateKind::Scientific);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(3, 0);
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    }
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 1, {10, 2}, true, 3);
    addCity(s, 2, {16, 6}, true, 2);
    s.cities.back().name = rules().cityStates[at(s.players[2].cityState)].name;
    s.players[2].firstMetBy = 1;  // already met, so no free envoy surprises
    return s;
}
}  // namespace

TEST(city_state_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.cityStates.size(), 48u);
    CHECK(!r.envoyBonuses.empty());
    const GovernmentType& chiefdom = r.governments[at(r.government("GOVERNMENT_CHIEFDOM"))];
    CHECK_EQ(chiefdom.influencePerTurn, 1);
    CHECK_EQ(chiefdom.influenceThreshold, 100);
    CHECK_EQ(chiefdom.envoysPerThreshold, 1);
    CHECK_EQ(r.civics[at(r.civic("CIVIC_MYSTICISM"))].envoys, 1);
    CHECK_EQ(r.mapSizes[at(r.mapSize("MAPSIZE_TINY"))].defaultCityStates, 6);
}

TEST(new_games_place_city_states_with_a_city_each) {
    GameSetup setup;
    setup.seed = 4;
    setup.mapSize = "MAPSIZE_TINY";
    setup.cityStates = 3;
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[static_cast<size_t>(i)].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    int states = 0;
    for (const Player& p : g->state().players) {
        if (p.cityState == kNone) continue;
        ++states;
        int cities = 0, warriors = 0;
        for (const City& c : g->state().cities) cities += c.owner == p.id;
        for (const Unit& u : g->state().units) warriors += u.owner == p.id && rules().units[at(u.type)].id == "UNIT_WARRIOR";
        CHECK_EQ(cities, 1);
        CHECK_EQ(warriors, 2);
        CHECK(!g->leaderOf(p.id));
        for (const Player& m : g->state().players) {
            if (m.cityState == kNone && !m.barbarian) CHECK(g->state().grid.distance(m.startPos, p.startPos) >= 6);
        }
    }
    CHECK_EQ(states, 3);
}

TEST(envoys_come_from_meeting_civics_and_influence) {
    GameState s = csState();
    s.players[2].firstMetBy = kNoPlayer;
    s.players[1].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Unrevealed));  // only player 0 has seen it
    s.players[0].government = rules().government("GOVERNMENT_CHIEFDOM");
    s.players[0].influence = 99;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 3);  // round to player 0's next turn
    // First to meet the city-state: an envoy there; influence 99 + 1 reaches the threshold.
    CHECK_EQ(g->envoysAt(0, 2), 1);
    CHECK_EQ(g->state().players[0].envoyTokens, 1);
}

TEST(envoys_make_a_suzerain_and_pay_tier_bonuses) {
    GameState s = csState();
    s.players[0].envoyTokens = 4;
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId capital = g->state().cities[0].id;
    const Fixed before = g->cityReport(capital).yields[yi(YieldType::Science)];
    REQUIRE(g->submit(Command::sendEnvoy(0, 2)) == CommandError::Ok);
    // One envoy to a Scientific city-state: +1 Science in the capital and +1 per Library.
    CHECK_EQ(g->cityReport(capital).yields[yi(YieldType::Science)], before + Fixed::fromInt(2));
    CHECK_EQ(g->suzerainOf(2), kNoPlayer);
    REQUIRE(g->submit(Command::sendEnvoy(0, 2)) == CommandError::Ok);
    REQUIRE(g->submit(Command::sendEnvoy(0, 2)) == CommandError::Ok);
    CHECK_EQ(g->suzerainOf(2), 0);  // three envoys and nobody close
    CHECK_EQ(g->state().players[0].envoyTokens, 1);
    CHECK_EQ(g->submit(Command::sendEnvoy(0, 1)), CommandError::CannotSendEnvoy);  // a major is no city-state
    // At war, the city-state's bonuses stop.
    REQUIRE(g->submit(Command::declareWar(0, 2)) == CommandError::Ok);
    CHECK_EQ(g->cityReport(capital).yields[yi(YieldType::Science)], before);
}

TEST(city_states_never_win_or_score) {
    GameState s = csState();
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->score(2), 0);
    CHECK(g->isCityState(2));
    CHECK(!g->isCityState(0));
}

TEST(envoys_survive_a_save) {
    GameState s = csState();
    s.players[0].envoyTokens = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::sendEnvoy(0, 2)) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->envoysAt(0, 2), 1);
    CHECK(loaded->isCityState(2));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(a_suzerain_levies_a_city_states_army) {
    GameState s = csState();
    s.players[0].envoys[2] = 3;  // suzerain
    s.players[0].gold = Fixed::fromInt(1000);
    const UnitId w1 = sovtest::addUnit(s, "UNIT_WARRIOR", 2, {15, 6});
    const UnitId w2 = sovtest::addUnit(s, "UNIT_WARRIOR", 2, {17, 6});
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->suzerainOf(2) == 0);
    const int cost = g->levyCost(0, 2);
    REQUIRE(cost > 0);
    CHECK_EQ(g->levyCost(1, 2), -1);  // not its suzerain
    REQUIRE(g->submit(Command::levyMilitary(0, 2)) == CommandError::Ok);
    CHECK_EQ(g->state().unit(w1)->owner, 0);
    CHECK_EQ(g->state().unit(w2)->owner, 0);
    CHECK(g->levied(*g->state().unit(w1)));
    CHECK(g->state().players[0].gold == Fixed::fromInt(1000 - cost));
    CHECK_EQ(g->levyCost(0, 2), -1);  // once at a time
    // Saved, then the levy runs out and the army goes home.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().levies.size(), 1u);
    GameState t = g->state();
    t.levies[0].until = t.turn;
    for (Unit& u : t.units) u.activity = Activity::Sleep;
    auto h = Game::fromScenario(rules(), std::move(t));
    sovtest::endTurns(*h, 3);
    CHECK_EQ(h->state().unit(w1)->owner, 2);
    CHECK(h->state().levies.empty());
}

TEST(a_suzerain_enjoys_its_city_states_bonus) {
    // Geneva: +15% Science in every city while at peace with all majors.
    GameState s = csState();
    s.players[2].cityState = rules().cityState("CITYSTATE_GENEVA");
    s.players[0].envoys[2] = 3;
    for (Player& p : s.players) p.relations.resize(3);
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->suzerainOf(2) == 0);
    CHECK(enjoysSuzerainBonus(g->state(), rules(), 0, rules().cityState("CITYSTATE_GENEVA")));
    CHECK(!enjoysSuzerainBonus(g->state(), rules(), 1, rules().cityState("CITYSTATE_GENEVA")));
    GameState none = s;
    none.players[2].cityState = rules().cityState("CITYSTATE_MITLA");  // the same envoy bonuses, no science of its own
    auto plain = Game::fromScenario(rules(), std::move(none));
    const CityId mine = g->state().cities[0].id;
    CHECK(g->cityReport(mine).yields[yi(YieldType::Science)] > plain->cityReport(mine).yields[yi(YieldType::Science)]);
    // At war with a major, the bonus lapses.
    GameState war = s;
    war.players[0].relations[1].war = war.players[1].relations[0].war = true;
    auto w = Game::fromScenario(rules(), std::move(war));
    CHECK_EQ(w->cityReport(mine).yields[yi(YieldType::Science)], plain->cityReport(mine).yields[yi(YieldType::Science)]);
    // A level-3 Economic ally of the suzerain shares it.
    GameState allied = s;
    for (auto [a, b] : {std::pair<int, int>{0, 1}, {1, 0}}) {
        Relation& r = allied.players[static_cast<size_t>(a)].relations[static_cast<size_t>(b)];
        r.alliance = AllianceType::Economic;
        r.allianceUntil = 1000;
        r.alliancePoints = 960;
    }
    auto e = Game::fromScenario(rules(), std::move(allied));
    CHECK(enjoysSuzerainBonus(e->state(), rules(), 1, rules().cityState("CITYSTATE_GENEVA")));
}

TEST(gunboat_diplomacy_opens_city_state_borders) {
    GameState s = csState();
    s.players[0].envoys[2] = 1;
    s.players[0].government = rules().government("GOVERNMENT_CHIEFDOM");
    s.players[0].policies.assign(static_cast<size_t>(rules().governments[at(s.players[0].government)].totalSlots()), kNone);
    for (Player& p : s.players) p.relations.resize(3);
    auto plain = Game::fromScenario(rules(), s);
    CHECK(!plain->grantsOpenBorders(2, 0));
    s.players[0].policies[0] = rules().policy("POLICY_GUNBOAT_DIPLOMACY");
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->grantsOpenBorders(2, 0));
    CHECK(!g->grantsOpenBorders(2, 1));  // no envoy, no card
}

TEST(city_states_hold_their_loyalty) {
    auto g = Game::fromScenario(rules(), csState());
    // Pressed by a bigger neighbour, a city-state still gains loyalty (+20 a turn toward itself).
    CHECK(g->loyaltyPerTurn(g->state().cities[2].id) > Fixed());
}

TEST(kilwa_kisiwani_counts_suzerainties) {
    GameState s = csState();
    s.players[0].envoys[2] = 3;  // suzerain of the Scientific city-state
    s.cities[0].buildings.push_back(rules().building("BUILDING_KILWA_KISIWANI"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->suzerainOf(2) == 0);
    CHECK_EQ(g->kilwaPercent(g->state().cities[0], CityStateKind::Scientific), 15);
    CHECK_EQ(g->kilwaPercent(g->state().cities[0], CityStateKind::Cultural), 0);
    CHECK_EQ(g->kilwaPercent(g->state().cities[1], CityStateKind::Scientific), 0);  // not its owner's
}

TEST(suzerain_bonuses_in_code) {
    auto suzerainOfType = [](const char* id) {
        GameState s = csState();
        s.players[2].cityState = rules().cityState(id);
        s.players[0].envoys[2] = 3;
        for (Player& p : s.players) p.relations.resize(3);
        return s;
    };
    const CityId mine = csState().cities[0].id;
    // Mohenjo-Daro: every city houses as if on a river.
    {
        auto g = Game::fromScenario(rules(), suzerainOfType("CITYSTATE_MOHENJO_DARO"));
        auto plain = Game::fromScenario(rules(), suzerainOfType("CITYSTATE_MITLA"));
        CHECK(g->cityReport(mine).housing >= plain->cityReport(mine).housing);
        CHECK_EQ(g->cityReport(mine).housing - plain->cityReport(mine).housing,
                 rules().global("CITY_POPULATION_RIVER_LAKE") - rules().global("CITY_POPULATION_NO_WATER"));
    }
    // Valletta: City Center buildings for Faith, at their Gold price.
    {
        auto g = Game::fromScenario(rules(), suzerainOfType("CITYSTATE_VALLETTA"));
        auto plain = Game::fromScenario(rules(), suzerainOfType("CITYSTATE_MITLA"));
        const ProductionItem monument{ProductionKind::Building, rules().building("BUILDING_MONUMENT")};
        const City& c = *g->state().city(mine);
        CHECK_EQ(plain->faithPurchaseCost(0, *plain->state().city(mine), monument), -1);
        CHECK_EQ(g->faithPurchaseCost(0, c, monument), g->purchaseCost(0, monument));
    }
}

TEST(samarkands_trading_domes_pay_international_routes) {
    // Samarkand (08): its suzerain's international routes earn +1 Gold per Trading Dome of the origin city.
    const auto routeGold = [](const char* cityState, int domes, bool pillageOne, bool domestic = false) {
        GameState s = csState();
        s.players[2].cityState = rules().cityState(cityState);
        s.players[0].envoys[2] = 3;
        for (Player& p : s.players) p.relations.resize(3);
        const Hex spots[] = {{5, 6}, {4, 7}};
        for (int i = 0; i < domes; ++i) {
            Plot& p = s.plot(spots[i]);
            p.owner = 0;
            p.city = s.cities[0].id;
            p.improvement = rules().improvement("IMPROVEMENT_TRADING_DOME");
            p.pillagedTurns = pillageOne && i == 0 ? 3 : 0;
        }
        auto g = Game::fromScenario(rules(), std::move(s));
        const City& home = g->state().cities[0];
        return g->tradeRouteYields(home, domestic ? home : g->state().cities[1])[yi(YieldType::Gold)];
    };
    CHECK_EQ(routeGold("CITYSTATE_SAMARKAND", 0, false), routeGold("CITYSTATE_MITLA", 0, false));
    CHECK_EQ(routeGold("CITYSTATE_SAMARKAND", 1, false), routeGold("CITYSTATE_MITLA", 1, false) + Fixed::fromInt(1));
    CHECK_EQ(routeGold("CITYSTATE_SAMARKAND", 2, false), routeGold("CITYSTATE_MITLA", 2, false) + Fixed::fromInt(2));
    CHECK_EQ(routeGold("CITYSTATE_SAMARKAND", 2, true), routeGold("CITYSTATE_MITLA", 2, true) + Fixed::fromInt(1));  // not while pillaged
    CHECK_EQ(routeGold("CITYSTATE_SAMARKAND", 2, false, true), routeGold("CITYSTATE_MITLA", 2, false, true));  // nor at home
}
