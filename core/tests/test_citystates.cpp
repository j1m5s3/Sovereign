// City-states and envoys (08-diplomacy-city-states-governors.md, City-States).
#include <algorithm>

#include "helpers.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::claimFor;
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

// Players 0 and 1 are majors; player 2 is a Scientific city-state with its city at (16,6); player 3, if asked for, the barbarians.
GameState csState(bool barbarians = false) {
    GameState s = flatState(24, 14, barbarians ? 4 : 3);
    s.players[2].civ = kNone;
    s.players[2].cityState = cityStateOf(CityStateKind::Scientific);
    if (barbarians) {
        s.players[3].civ = kNone;
        s.players[3].barbarian = true;
    }
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(s.players.size(), 0);
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

TEST(a_civ_s_unique_building_earns_its_base_s_envoy_bonus) {
    // One envoy at a Scientific city-state: +1 Science in the capital and +1 per Library; the Aztecs' Calmecac is their Library.
    const auto science = [](const char* building) {
        GameState s = csState();
        s.players[0].envoyTokens = 1;
        if (building) s.cities[0].buildings.push_back(rules().building(building));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(s));
        const CityId capital = g->state().cities[0].id;
        const Fixed before = g->cityReport(capital).yields[yi(YieldType::Science)];
        REQUIRE(g->submit(Command::sendEnvoy(0, 2)) == CommandError::Ok);
        return g->cityReport(capital).yields[yi(YieldType::Science)] - before;
    };
    CHECK_EQ(science(nullptr), Fixed::fromInt(1));
    CHECK_EQ(science("BUILDING_LIBRARY"), Fixed::fromInt(2));
    CHECK_EQ(science("BUILDING_CALMECAC"), Fixed::fromInt(2));
    // Three envoys at an Industrial city-state: +2 Production toward buildings in a city with a Factory; England's
    // Mill Town is its Factory.
    const auto production = [](const char* building) {
        GameState s = csState();
        s.players[2].cityState = cityStateOf(CityStateKind::Industrial);
        s.players[0].envoys[2] = 3;
        if (building) s.cities[0].buildings.push_back(rules().building(building));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->envoyProduction(g->state().cities[0], ProductionItem{ProductionKind::Building, rules().building("BUILDING_MONUMENT")});
    };
    CHECK_EQ(production("BUILDING_FACTORY") - production(nullptr), 2);
    CHECK_EQ(production("BUILDING_MILL_TOWN") - production(nullptr), 2);
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
    // Valletta: City Center and Encampment buildings for Faith, at their Gold price.
    {
        const auto withCamp = [&](const char* id) {
            GameState s = suzerainOfType(id);
            CityDistrict camp;
            camp.type = rules().district("DISTRICT_ENCAMPMENT");
            camp.pos = {5, 6};
            camp.complete = true;
            s.city(mine)->districts.push_back(camp);
            s.players[0].techs.done[at(rules().tech("TECH_BRONZE_WORKING"))] = 1;
            return s;
        };
        auto g = Game::fromScenario(rules(), withCamp("CITYSTATE_VALLETTA"));
        auto plain = Game::fromScenario(rules(), withCamp("CITYSTATE_MITLA"));
        const ProductionItem monument{ProductionKind::Building, rules().building("BUILDING_MONUMENT")};
        const ProductionItem barracks{ProductionKind::Building, rules().building("BUILDING_BARRACKS")};
        const City& c = *g->state().city(mine);
        CHECK_EQ(plain->faithPurchaseCost(0, *plain->state().city(mine), monument), -1);
        CHECK_EQ(g->faithPurchaseCost(0, c, monument), g->purchaseCost(0, monument));
        CHECK_EQ(plain->faithPurchaseCost(0, *plain->state().city(mine), barracks), -1);
        CHECK(g->purchaseCost(0, barracks) > 0);
        CHECK_EQ(g->faithPurchaseCost(0, c, barracks), g->purchaseCost(0, barracks));
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

namespace {
// Player 0 is the suzerain of the city-state, which takes the given kind.
GameState suzerainState(const char* cityState, bool barbarians = false) {
    GameState s = csState(barbarians);
    s.players[2].cityState = rules().cityState(cityState);
    s.players[0].envoys[2] = 3;
    for (Player& p : s.players) p.relations.resize(s.players.size());
    return s;
}

// Buddhism, founded by the given player in its first city.
void foundBuddhism(GameState& s, PlayerId founder) {
    const size_t f = static_cast<size_t>(founder);
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), founder, s.cities[f].id, {rules().belief("BELIEF_TITHE")}});
    s.players[f].religion = 0;
}

int32_t buddhistPressure(const Game& g, size_t city) {
    const std::vector<int32_t>& p = g.state().cities[city].pressure;
    return p.empty() ? 0 : p[0];
}
}  // namespace

TEST(chinguettis_routes_earn_faith_per_follower_at_home) {
    // Chinguetti (08): its suzerain's routes earn +1 Faith per follower of its founded (or majority) religion in the origin city.
    const auto routeFaith = [](const char* cityState, PlayerId founder, bool follows, bool domestic) {
        GameState s = suzerainState(cityState);
        foundBuddhism(s, founder);
        if (follows) s.cities[0].pressure = {10000};  // all 3 citizens of player 0's capital
        auto g = Game::fromScenario(rules(), std::move(s));
        const City& home = g->state().cities[0];
        return g->tradeRouteYields(home, domestic ? home : g->state().cities[1])[yi(YieldType::Faith)];
    };
    CHECK_EQ(routeFaith("CITYSTATE_CHINGUETTI", 0, true, false), routeFaith("CITYSTATE_MITLA", 0, true, false) + Fixed::fromInt(3));
    CHECK_EQ(routeFaith("CITYSTATE_CHINGUETTI", 0, true, true), routeFaith("CITYSTATE_MITLA", 0, true, true) + Fixed::fromInt(3));
    // Without a religion of its own, the one its capital follows counts.
    CHECK_EQ(routeFaith("CITYSTATE_CHINGUETTI", 1, true, false), routeFaith("CITYSTATE_MITLA", 1, true, false) + Fixed::fromInt(3));
    CHECK_EQ(routeFaith("CITYSTATE_CHINGUETTI", 0, false, false), routeFaith("CITYSTATE_MITLA", 0, false, false));  // no followers at home
}

TEST(fez_pays_science_the_first_time_a_city_is_converted) {
    // Fez (08): the first time its suzerain's religious unit converts a city, 20 Science per citizen of that city.
    const auto science = [](const char* cityState, bool convertedBefore) {
        GameState s = suzerainState(cityState);
        foundBuddhism(s, 0);
        const City& target = s.cities[1];  // player 1's capital, 3 citizens
        const CityId id = target.id;
        if (convertedBefore) s.players[0].convertedCities.push_back(id);
        Hex spot = target.pos;
        for (const Hex& h : s.grid.within(target.pos, 1)) {
            if (h != target.pos) {
                spot = h;
                break;
            }
        }
        const UnitId missionary = addUnit(s, "UNIT_MISSIONARY", 0, spot);
        s.units.back().religion = 0;
        s.units.back().charges = 3;
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::spreadReligion(0, missionary)) == CommandError::Ok);
        REQUIRE(g->cityMajorityReligion(*g->state().city(id)) == 0);
        const std::vector<CityId>& done = g->state().players[0].convertedCities;
        CHECK_EQ(std::count(done.begin(), done.end(), id), 1);
        return g->state().players[0].techs.overflow;
    };
    CHECK_EQ(science("CITYSTATE_FEZ", false), science("CITYSTATE_MITLA", false) + Fixed::fromInt(60));
    CHECK_EQ(science("CITYSTATE_FEZ", true), science("CITYSTATE_MITLA", true));  // not the first time
}

TEST(jerusalem_makes_holy_site_cities_press_like_holy_cities) {
    // Jerusalem (08): its suzerain's cities with a Holy Site press as if they were Holy Cities (x4).
    const auto pressed = [](const char* cityState, bool holySite) {
        GameState s = suzerainState(cityState);
        foundBuddhism(s, 1);  // player 1's Holy City no longer follows it; player 0's capital does
        s.cities[0].pressure = {10000};
        if (holySite) s.cities[0].districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {5, 6}, true});
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->state().grid.distance(g->state().cities[0].pos, g->state().cities[1].pos) <= 10);
        sovtest::endTurns(*g, 3);  // one world turn
        return buddhistPressure(*g, 1);
    };
    CHECK(pressed("CITYSTATE_MITLA", true) > 0);
    CHECK_EQ(pressed("CITYSTATE_JERUSALEM", true), 4 * pressed("CITYSTATE_MITLA", true));
    CHECK_EQ(pressed("CITYSTATE_JERUSALEM", false), pressed("CITYSTATE_MITLA", false));
}

TEST(vatican_city_spreads_pressure_from_great_people) {
    // Vatican City (08): a great person used spreads 400 pressure of its suzerain's religion on the cities within 10 tiles.
    const auto spread = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        foundBuddhism(s, 0);
        const UnitId homer = addUnit(s, "UNIT_GREAT_WRITER", 0, s.cities[0].pos);
        s.units.back().greatPerson = rules().greatPerson("GREAT_PERSON_HOMER");
        s.units.back().charges = 2;
        auto g = Game::fromScenario(rules(), std::move(s));
        const Hex home = g->state().cities[0].pos;
        REQUIRE(g->state().grid.distance(home, g->state().cities[1].pos) <= 10);
        REQUIRE(g->state().grid.distance(home, g->state().cities[2].pos) > 10);
        REQUIRE(g->submit(Command::activateGreatPerson(0, homer)) == CommandError::Ok);
        return std::vector<int32_t>{buddhistPressure(*g, 0), buddhistPressure(*g, 1), buddhistPressure(*g, 2)};
    };
    CHECK(spread("CITYSTATE_VATICAN_CITY") == (std::vector<int32_t>{400, 400, 0}));
    CHECK(spread("CITYSTATE_MITLA") == (std::vector<int32_t>{0, 0, 0}));
}

TEST(zanzibar_and_buenos_aires_add_amenities) {
    // Zanzibar (08): Cinnamon and Cloves, 6 cities each. Buenos Aires: each kind of improved bonus resource is an Amenity.
    const auto amenities = [](const char* cityState, bool improve) {
        GameState s = suzerainState(cityState);
        const TypeIndex wheat = rules().resource("RESOURCE_WHEAT"), farm = rules().improvement("IMPROVEMENT_FARM");
        for (Hex h : {Hex{5, 6}, Hex{3, 6}}) {  // two Wheat farms: one kind
            s.plot(h).resource = wheat;
            s.plot(h).improvement = improve ? farm : kNone;
        }
        s.plot({4, 7}).resource = rules().resource("RESOURCE_CATTLE");  // no Pasture
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->luxuryAmenities(g->state().cities[0]);
    };
    CHECK_EQ(amenities("CITYSTATE_ZANZIBAR", true), amenities("CITYSTATE_MITLA", true) + 2);
    CHECK_EQ(amenities("CITYSTATE_BUENOS_AIRES", true), amenities("CITYSTATE_MITLA", true) + 1);
    CHECK_EQ(amenities("CITYSTATE_BUENOS_AIRES", false), amenities("CITYSTATE_MITLA", false));
    auto g = Game::fromScenario(rules(), suzerainState("CITYSTATE_ZANZIBAR"));
    CHECK(g->hasLuxury(0, rules().resource("RESOURCE_CINNAMON")));
    CHECK(g->hasLuxury(0, rules().resource("RESOURCE_CLOVES")));
    CHECK(!g->hasLuxury(1, rules().resource("RESOURCE_CLOVES")));
}

TEST(mexico_city_extends_regional_reach) {
    // Mexico City (08): Industrial Zone, Entertainment Complex and Water Park buildings reach 3 tiles farther.
    const auto setup = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        addCity(s, 0, {12, 6}, false, 3);  // 8 tiles from the capital
        City& capital = s.cities[0];
        for (const char* b : {"BUILDING_ZOO", "BUILDING_COAL_POWER_PLANT"}) capital.buildings.push_back(rules().building(b));
        std::sort(capital.buildings.begin(), capital.buildings.end());
        s.cities.back().buildings.push_back(rules().building("BUILDING_RESEARCH_LAB"));  // needs 3 power
        s.players[0].stockpile[at(rules().resource("RESOURCE_COAL"))] = 10;
        return s;
    };
    auto plain = Game::fromScenario(rules(), setup("CITYSTATE_MITLA"));
    auto g = Game::fromScenario(rules(), setup("CITYSTATE_MEXICO_CITY"));
    const CityId far = g->state().cities.back().id;
    REQUIRE(g->state().grid.distance(g->state().cities[0].pos, g->state().city(far)->pos) == 8);
    CHECK_EQ(g->cityReport(far).amenities, plain->cityReport(far).amenities + 1);  // the Zoo
    sovtest::endTurns(*plain, 3);
    sovtest::endTurns(*g, 3);
    CHECK_EQ(plain->state().city(far)->powerSupply, 0);
    CHECK(g->state().city(far)->powerSupply >= 3);  // the Coal Power Plant
}

// Mexico City reaches farther from Industrial Zone and Water Park buildings too: a Factory's Production 8 tiles away,
// an Aquarium's Amenity 11 tiles away.
TEST(mexico_city_extends_industrial_and_water_park_reach) {
    const auto setup = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        addCity(s, 0, {12, 6}, false, 3);
        addCity(s, 0, {12, 11}, false, 3);
        City& capital = s.cities[0];
        for (const char* b : {"BUILDING_FACTORY", "BUILDING_AQUARIUM"}) capital.buildings.push_back(rules().building(b));
        std::sort(capital.buildings.begin(), capital.buildings.end());
        return s;
    };
    auto plain = Game::fromScenario(rules(), setup("CITYSTATE_MITLA"));
    auto g = Game::fromScenario(rules(), setup("CITYSTATE_MEXICO_CITY"));
    const CityId eight = g->state().cities[3].id, eleven = g->state().cities[4].id;
    REQUIRE(g->state().grid.distance(g->state().cities[0].pos, g->state().city(eight)->pos) == 8);
    REQUIRE(g->state().grid.distance(g->state().cities[0].pos, g->state().city(eleven)->pos) == 11);
    const size_t production = static_cast<size_t>(YieldType::Production);
    CHECK_EQ(g->cityReport(eight).yields[production], plain->cityReport(eight).yields[production] + Fixed::fromInt(3));  // the Factory
    CHECK_EQ(g->cityReport(eleven).amenities, plain->cityReport(eleven).amenities + 1);  // the Aquarium
}

TEST(bandar_brunei_and_mogadishu_help_traders) {
    // A route from player 0's capital to the city-state, through player 1's city where player 0 has a Trading Post.
    const auto withRoute = [](const char* cityState, bool post = true) {
        GameState s = suzerainState(cityState);
        s.cities[1].tradingPosts = {post ? uint8_t{1} : uint8_t{0}, 0, 0};
        TradeRoute r;
        r.id = 1;
        r.owner = 0;
        r.origin = s.cities[0].id;
        r.destination = s.cities[2].id;
        r.traderType = rules().unit("UNIT_TRADER");
        for (Hex h : {s.cities[0].pos, Hex{8, 6}, s.cities[1].pos, s.cities[2].pos}) r.path.push_back(s.grid.index(h));
        r.turnsLeft = 20;
        s.tradeRoutes.push_back(r);
        return s;
    };
    // Bandar Brunei (08): each foreign Trading Post the route passes pays 1 Gold more.
    const auto postGold = [&](const char* cityState) {
        auto with = Game::fromScenario(rules(), withRoute(cityState));
        auto without = Game::fromScenario(rules(), withRoute(cityState, false));
        const CityId home = with->state().cities[0].id;
        return with->cityReport(home).yields[yi(YieldType::Gold)] - without->cityReport(home).yields[yi(YieldType::Gold)];
    };
    CHECK_EQ(postGold("CITYSTATE_MITLA"), Fixed::fromInt(rules().globalInt("TRADING_POST_GOLD_IN_FOREIGN_CITY")));
    CHECK_EQ(postGold("CITYSTATE_BANDAR_BRUNEI"), postGold("CITYSTATE_MITLA") + Fixed::fromInt(1));
    // Mogadishu (08): an enemy on the water does not plunder the route; one on land still does.
    const auto plundered = [&](const char* cityState, bool water) {
        GameState s = withRoute(cityState);
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        if (water) s.plot({8, 6}).terrain = rules().terrain("TERRAIN_COAST");
        addUnit(s, water ? "UNIT_GALLEY" : "UNIT_WARRIOR", 1, {8, 6});
        s.units.back().activity = Activity::Sleep;
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 3);  // player 0's next turn
        return g->state().tradeRoutes.empty();
    };
    CHECK(plundered("CITYSTATE_MITLA", true));
    CHECK(!plundered("CITYSTATE_MOGADISHU", true));
    CHECK(plundered("CITYSTATE_MOGADISHU", false));
}

TEST(wolin_earns_great_general_and_admiral_points_from_victories) {
    // Wolin (08): a quarter of the beaten unit's strength, as Great General points on land and Great Admiral points at sea,
    // for victories over civs' and city-states' units, not barbarians'.
    const auto points = [](const char* cityState) {
        GameState s = suzerainState(cityState, true);
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        for (Hex h : {Hex{8, 10}, Hex{9, 10}}) s.plot(h).terrain = rules().terrain("TERRAIN_COAST");
        const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
        addUnit(s, "UNIT_WARRIOR", 1, {8, 6});
        s.units.back().hp = 1;
        const UnitId slinger = addUnit(s, "UNIT_WARRIOR", 0, {7, 8});
        addUnit(s, "UNIT_WARRIOR", 3, {8, 8});
        s.units.back().hp = 1;
        const UnitId galley = addUnit(s, "UNIT_GALLEY", 0, {8, 10});
        addUnit(s, "UNIT_GALLEY", 1, {9, 10});
        s.units.back().hp = 1;
        auto g = Game::fromScenario(rules(), std::move(s));
        const size_t general = at(rules().greatPersonClass("GREAT_PERSON_CLASS_GENERAL"));
        const size_t admiral = at(rules().greatPersonClass("GREAT_PERSON_CLASS_ADMIRAL"));
        std::vector<int> out;
        for (const auto& [unit, target] : {std::pair<UnitId, Hex>{warrior, {8, 6}}, {slinger, {8, 8}}, {galley, {9, 10}}}) {
            REQUIRE(g->submit(Command::attack(0, unit, target)) == CommandError::Ok);
            REQUIRE(!g->state().unitAt(target, UnitLayer::Military, rules()) || g->state().unitAt(target, UnitLayer::Military, rules())->owner == 0);
            out.push_back(g->state().players[0].greatPersonPoints[general]);
            out.push_back(g->state().players[0].greatPersonPoints[admiral]);
        }
        return out;
    };
    const int warrior = rules().units[at(rules().unit("UNIT_WARRIOR"))].combat / 4;
    const int galley = rules().units[at(rules().unit("UNIT_GALLEY"))].combat / 4;
    CHECK(points("CITYSTATE_MITLA") == (std::vector<int>{0, 0, 0, 0, 0, 0}));
    CHECK(points("CITYSTATE_WOLIN") == (std::vector<int>{warrior, 0, warrior, 0, warrior, galley}));
}

TEST(yerevans_apostles_choose_their_promotion) {
    // Yerevan (08): a new Apostle takes the promotion its player chooses instead of a random one.
    const auto bought = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        foundBuddhism(s, 0);
        s.cities[0].pressure = {10000};  // the Holy City follows it
        s.cities[0].districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {5, 6}, true});
        for (const char* b : {"BUILDING_SHRINE", "BUILDING_TEMPLE"}) s.cities[0].buildings.push_back(rules().building(b));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        s.players[0].faith = Fixed::fromInt(2000);
        auto g = Game::fromScenario(rules(), std::move(s));
        const CommandError e = g->submit(Command::purchaseWithFaith(0, g->state().cities[0].id, {ProductionKind::Unit, rules().unit("UNIT_APOSTLE")}));
        if (e != CommandError::Ok) std::printf("  purchase: %s\n", commandErrorName(e));
        REQUIRE(e == CommandError::Ok);
        return g;
    };
    size_t kinds = 0;
    for (const PromotionType& p : rules().promotions) kinds += p.promotionClass == "PROMOTION_CLASS_RELIGIOUS_APOSTLE" ? 1 : 0;
    auto plain = bought("CITYSTATE_MITLA");
    CHECK_EQ(plain->state().units.back().promotions.size(), 1u);
    CHECK(plain->availablePromotions(plain->state().units.back().id).empty());
    auto g = bought("CITYSTATE_YEREVAN");
    const Unit& apostle = g->state().units.back();
    const UnitId id = apostle.id;
    const int charges = apostle.charges;
    CHECK(apostle.promotions.empty());
    CHECK_EQ(g->availablePromotions(id).size(), kinds);
    REQUIRE(g->submit(Command::promote(0, id, rules().promotion("PROMOTION_ORATOR"))) == CommandError::Ok);
    CHECK_EQ(g->state().unit(id)->charges, charges + 2);  // Orator's two more spreads
    CHECK(g->availablePromotions(id).empty());
}

TEST(ngazargamu_cheapens_land_units_in_military_cities) {
    // Ngazargamu (08): land units 20% cheaper with a Barracks or Stable, 40% with an Armory too, 60% with a Military Academy.
    const ProductionItem warrior{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")};
    const ProductionItem galley{ProductionKind::Unit, rules().unit("UNIT_GALLEY")};
    const auto game = [](const char* cityState, std::initializer_list<const char*> buildings, int gold = 500) {
        GameState s = suzerainState(cityState);
        for (const char* b : buildings) s.cities[0].buildings.push_back(rules().building(b));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        s.players[0].gold = Fixed::fromInt(gold);
        return Game::fromScenario(rules(), std::move(s));
    };
    const auto cost = [&](const char* cityState, std::initializer_list<const char*> buildings, ProductionItem item) {
        auto g = game(cityState, buildings);
        return g->purchaseCost(0, item, &g->state().cities[0]);
    };
    const int plain = cost("CITYSTATE_GRANADA", {"BUILDING_BARRACKS"}, warrior);
    REQUIRE(plain > 0);
    REQUIRE(plain % 5 == 0);
    CHECK_EQ(cost("CITYSTATE_NGAZARGAMU", {}, warrior), plain);
    CHECK_EQ(cost("CITYSTATE_NGAZARGAMU", {"BUILDING_STABLE"}, warrior), plain * 80 / 100 / 5 * 5);
    CHECK_EQ(cost("CITYSTATE_NGAZARGAMU", {"BUILDING_BARRACKS", "BUILDING_ARMORY"}, warrior), plain * 60 / 100 / 5 * 5);
    CHECK_EQ(cost("CITYSTATE_NGAZARGAMU", {"BUILDING_BARRACKS", "BUILDING_ARMORY", "BUILDING_MILITARY_ACADEMY"}, warrior), plain * 40 / 100 / 5 * 5);
    CHECK_EQ(cost("CITYSTATE_NGAZARGAMU", {"BUILDING_BARRACKS"}, galley), cost("CITYSTATE_GRANADA", {"BUILDING_BARRACKS"}, galley));  // land units only
    // Buying one asks and pays the city's price: short of the full price is enough.
    const int price = plain * 80 / 100 / 5 * 5;
    auto g = game("CITYSTATE_NGAZARGAMU", {"BUILDING_BARRACKS"}, plain - 5);
    REQUIRE(g->submit(Command::purchase(0, g->state().cities[0].id, warrior)) == CommandError::Ok);
    CHECK(g->state().players[0].gold == Fixed::fromInt(plain - 5 - price));
}

TEST(nan_madol_pays_culture_for_districts_by_the_coast) {
    // Nan Madol (08): +2 Culture for each finished, unpillaged district on or next to Coast, the City Center included.
    const auto culture = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        s.plot({3, 6}).terrain = rules().terrain("TERRAIN_COAST");  // beside the City Center at (4,6)
        std::vector<CityDistrict>& ds = s.cities[0].districts;
        ds.push_back({rules().district("DISTRICT_CAMPUS"), {3, 7}, true});           // next to the Coast
        ds.push_back({rules().district("DISTRICT_COMMERCIAL_HUB"), {5, 6}, true});   // inland
        ds.push_back({rules().district("DISTRICT_ENCAMPMENT"), {3, 5}, false});      // unfinished
        ds.push_back({rules().district("DISTRICT_HOLY_SITE"), {2, 6}, true});        // pillaged
        ds.back().pillagedTurns = kPillagedDistrictTurns;
        for (const CityDistrict& d : ds) claimFor(s, s.cities[0], d.pos);
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->cityReport(g->state().cities[0].id).yields[yi(YieldType::Culture)];
    };
    CHECK(culture("CITYSTATE_NAN_MADOL") == culture("CITYSTATE_RAPA_NUI") + Fixed::fromInt(4));
}

TEST(kandy_finds_relics_and_raises_their_faith) {
    // Kandy (08): a Relic for each natural wonder its suzerain discovers; relics yield +50% Faith.
    const auto setup = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));  // a relic slot
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        s.plot({8, 6}).feature = rules().feature("FEATURE_FOUNTAIN_OF_YOUTH");
        for (Player& p : s.players) p.visibility[static_cast<size_t>(s.grid.index({8, 6}))] = static_cast<uint8_t>(Visibility::Unrevealed);
        addUnit(s, "UNIT_SCOUT", 0, {5, 6});
        return s;
    };
    const auto discover = [&](const char* cityState) {
        auto g = Game::fromScenario(rules(), setup(cityState));
        REQUIRE(g->state().cities[0].greatWorks.empty());
        REQUIRE(g->submit(Command::move(0, g->state().units.back().id, {6, 6})) == CommandError::Ok);  // now it sees the Fountain of Youth
        return g;
    };
    auto g = discover("CITYSTATE_KANDY");
    REQUIRE(g->state().cities[0].greatWorks.size() == 1u);
    CHECK(g->state().cities[0].greatWorks[0].type == rules().greatWorkType("RELIC"));
    auto plain = discover("CITYSTATE_NAZCA");
    CHECK(plain->state().cities[0].greatWorks.empty());
    // A relic in the Temple: 4 Faith, 6 for Kandy's suzerain.
    const auto faith = [](const char* cityState) {
        GameState s = suzerainState(cityState);
        const TypeIndex temple = rules().building("BUILDING_TEMPLE");
        s.cities[0].buildings.push_back(temple);
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        GreatWork relic;
        relic.type = rules().greatWorkType("RELIC");
        relic.building = temple;
        s.cities[0].greatWorks.push_back(relic);
        auto h = Game::fromScenario(rules(), std::move(s));
        return h->cityReport(h->state().cities[0].id).yields[yi(YieldType::Faith)];
    };
    CHECK(faith("CITYSTATE_KANDY") == faith("CITYSTATE_NAZCA") + Fixed::fromInt(2));
}

TEST(anshan_adds_science_to_writing_and_relics) {
    // Anshan (08): +2 Science from each of its suzerain's Great Works of Writing, +1 from each artifact and relic.
    const auto science = [](const char* cityState, const char* work, const char* building) {
        GameState s = suzerainState(cityState);
        const TypeIndex b = rules().building(building);
        if (!s.cities[0].has(b)) {
            s.cities[0].buildings.push_back(b);
            std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        }
        GreatWork w;
        w.type = rules().greatWorkType(work);
        w.building = b;
        s.cities[0].greatWorks.push_back(w);
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->cityReport(g->state().cities[0].id).yields[yi(YieldType::Science)];
    };
    // Against Hattusa, a Scientific city-state too, so the envoys' own bonuses are the same.
    CHECK(science("CITYSTATE_ANSHAN", "WRITING", "BUILDING_PALACE") == science("CITYSTATE_HATTUSA", "WRITING", "BUILDING_PALACE") + Fixed::fromInt(2));
    CHECK(science("CITYSTATE_ANSHAN", "RELIC", "BUILDING_TEMPLE") == science("CITYSTATE_HATTUSA", "RELIC", "BUILDING_TEMPLE") + Fixed::fromInt(1));
}

TEST(vilnius_raises_theater_squares_with_each_alliance_level) {
    // Vilnius (08): +50% Theater Square adjacency for each level (1+, 2+, 3+) of its suzerain's best alliance.
    const auto adjacency = [](const char* cityState, int alliancePoints) {
        GameState s = suzerainState(cityState);
        s.cities[0].districts.push_back({rules().district("DISTRICT_ENTERTAINMENT_COMPLEX"), {6, 6}, true});
        claimFor(s, s.cities[0], {6, 6});
        if (alliancePoints >= 0) {
            Relation& r = s.players[0].relations[1];
            r.alliance = AllianceType::Cultural;
            r.allianceUntil = 100;
            r.alliancePoints = alliancePoints;
        }
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->districtAdjacency(0, rules().district("DISTRICT_THEATER_SQUARE"), {5, 6})[yi(YieldType::Culture)];
    };
    const Fixed base = adjacency("CITYSTATE_RAPA_NUI", 0);
    REQUIRE(base > Fixed());
    CHECK(adjacency("CITYSTATE_VILNIUS", -1) == base);  // no alliance
    CHECK(adjacency("CITYSTATE_VILNIUS", 0) == base * 3 / 2);
    CHECK(adjacency("CITYSTATE_VILNIUS", rules().globalInt("ALLIANCE_LEVEL_TWO_XP")) == base * 2);
    CHECK(adjacency("CITYSTATE_VILNIUS", rules().globalInt("ALLIANCE_LEVEL_THREE_XP")) == base * 5 / 2);
}

TEST(the_consulate_and_the_chancery_add_influence) {
    // [GS] Consulate +2 Influence points a turn, Chancery +3 more.
    const auto influence = [](std::initializer_list<const char*> buildings) {
        GameState s = csState();
        s.players[0].government = rules().government("GOVERNMENT_CHIEFDOM");
        CityDistrict dq;
        dq.type = rules().district("DISTRICT_DIPLOMATIC_QUARTER");
        dq.pos = {5, 6};
        dq.complete = true;
        s.cities[0].districts.push_back(dq);
        for (const char* b : buildings) s.cities[0].buildings.push_back(rules().building(b));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 3);  // round to player 0's next turn
        return g->state().players[0].influence;
    };
    const int none = influence({});
    CHECK_EQ(influence({"BUILDING_CONSULATE"}), none + 2);
    CHECK_EQ(influence({"BUILDING_CONSULATE", "BUILDING_CHANCERY"}), none + 5);
}

TEST(hattusa_gives_strategics_its_suzerain_has_not_improved) {
    // Player 0's Horses, revealed, on its own land but not improved: Hattusa's suzerain gets 2 a turn of them.
    const auto horsesAfterATurn = [](const char* id) {
        GameState s = csState();
        s.players[2].cityState = rules().cityState(id);
        s.players[0].envoys[2] = 3;
        for (Player& p : s.players) p.relations.resize(3);
        s.players[0].techs.done[at(rules().tech("TECH_ANIMAL_HUSBANDRY"))] = 1;
        s.plot({5, 6}).resource = rules().resource("RESOURCE_HORSES");
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 3);  // round to player 0's next turn
        return g->state().players[0].stockpile[at(rules().resource("RESOURCE_HORSES"))];
    };
    CHECK_EQ(horsesAfterATurn("CITYSTATE_HATTUSA"), horsesAfterATurn("CITYSTATE_MITLA") + 2);
}
