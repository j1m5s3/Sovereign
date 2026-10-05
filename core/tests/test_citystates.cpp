// City-states and envoys (08-diplomacy-city-states-governors.md, City-States).
#include "helpers.h"
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
