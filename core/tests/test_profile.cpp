// Player modelling (leader doc §10, AI layer 2): the profile each major civ builds up from play.
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
size_t cls(ProfileClass c) { return static_cast<size_t>(c); }

GameState twoCivs() {
    GameState s = flatState(30, 14, 2);
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 1, {20, 6}, true, 3);
    return s;
}

void sleepAll(GameState& s) {
    for (Unit& u : s.units) u.activity = Activity::Sleep;
}
}  // namespace

TEST(profile_tracks_army_mix_and_militarism) {
    GameState s = twoCivs();
    for (int i = 0; i < 3; ++i) addUnit(s, "UNIT_HORSEMAN", 0, {4, static_cast<int32_t>(8 + i)});
    addUnit(s, "UNIT_WARRIOR", 0, {5, 8});
    addUnit(s, "UNIT_WARRIOR", 1, {20, 8});
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->profile(0) == nullptr || g->profile(0)->turnsObserved == 0);
    endTurns(*g, 2 * 6);  // six world turns
    const PlayerProfile* p = g->profile(0);
    REQUIRE(p != nullptr);
    CHECK_EQ(p->turnsObserved, 6);
    CHECK(p->army[cls(ProfileClass::LightCavalry)] > p->army[cls(ProfileClass::Melee)]);
    CHECK(p->army[cls(ProfileClass::Melee)] > 0);
    CHECK(p->militarism > g->profile(1)->militarism);
}

TEST(profile_remembers_wars_declared_and_armies_massed) {
    GameState s = twoCivs();
    for (int i = 0; i < 3; ++i) addUnit(s, "UNIT_WARRIOR", 0, {static_cast<int32_t>(17 + i), 4});  // camped by player 1's city
    sleepAll(s);
    auto g = Game::fromScenario(rules(), s);
    endTurns(*g, 4);
    const int massed = g->profile(0)->aggression;
    CHECK(massed > 0);
    CHECK_EQ(g->profile(1)->aggression, 0);
    auto g2 = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g2->submit(Command::declareWar(0, 1)) == CommandError::Ok);  // a surprise war
    endTurns(*g2, 4);
    CHECK_EQ(g2->profile(0)->warsDeclared, 1);
    CHECK_EQ(g2->profile(0)->surpriseWars, 1);
    CHECK(g2->profile(0)->aggression > massed);
}

TEST(profile_watches_the_leader) {
    if (rules().leaderUnit == kNone) return;
    GameState s = twoCivs();
    addUnit(s, rules().units[static_cast<size_t>(rules().leaderUnit)].id.c_str(), 0, {9, 6});  // out in the field
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    endTurns(*g, 6);
    CHECK(g->profile(0)->leaderOutside > 0);
}

TEST(profiles_survive_a_save) {
    GameState s = twoCivs();
    addUnit(s, "UNIT_HORSEMAN", 0, {5, 8});
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    endTurns(*g, 6);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->profile(0) != nullptr);
    CHECK_EQ(loaded->profile(0)->army[cls(ProfileClass::LightCavalry)], g->profile(0)->army[cls(ProfileClass::LightCavalry)]);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(ai_counters_a_cavalry_neighbour_with_pikes) {
    // Player 0 (AI) has two Warriors (one garrisoned) and knows Archery and Bronze Working: it would
    // add an Archer for balance, but a horse-heavy neighbour makes it train Spearmen instead.
    auto scene = [](bool cavalryNeighbour, int difficulty) {
        GameState s = twoCivs();
        s.cities.erase(s.cities.begin() + 1);
        addCity(s, 1, {14, 6}, true, 3);
        s.setup.difficulty = difficulty;
        Player& p = s.players[0];
        p.techs.resize(rules().techs.size());
        for (const char* t : {"TECH_ARCHERY", "TECH_BRONZE_WORKING"}) p.techs.done[static_cast<size_t>(rules().tech(t))] = 1;
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
        s.cities[0].queue.clear();
        s.cities[0].population = 1;  // no Settler yet
        // At war, so a soldier outranks the builders and buildings.
        s.players[0].relations.resize(2);
        s.players[1].relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        s.profiles.resize(s.players.size());
        PlayerProfile& theirs = s.profiles[1];
        theirs.turnsObserved = 10;
        theirs.army[static_cast<size_t>(cavalryNeighbour ? ProfileClass::LightCavalry : ProfileClass::Melee)] = 900;
        sleepAll(s);
        return Game::fromScenario(rules(), std::move(s));
    };
    auto produces = [](const Game& g) {
        const City& c = g.state().cities[0];
        return c.queue.empty() || c.queue.front().kind != ProductionKind::Unit ? std::string() : rules().units[static_cast<size_t>(c.queue.front().type)].id;
    };
    auto g = scene(true, 3);
    ai::playTurn(*g);
    auto plain = scene(false, 3);
    ai::playTurn(*plain);
    CHECK_EQ(produces(*plain), std::string("UNIT_ARCHER"));
    CHECK_EQ(produces(*g), std::string("UNIT_SPEARMAN"));
    // Settler and Chieftain AIs ignore the profile.
    auto low = scene(true, 0);
    ai::playTurn(*low);
    CHECK(produces(*low) != "UNIT_SPEARMAN");
}
