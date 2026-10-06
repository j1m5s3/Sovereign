// Barbarian Clans mode (01-map-and-terrain.md, Barbarians: camps bribed, hired, incited, turned city-state).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
// Players 0 and 1 with a city each, player 2 the barbarians with a camp at (12,5) that player 0 has seen.
GameState clanState() {
    GameState s = flatState(24, 12, 3);
    s.setup.barbarianClans = true;
    s.players[2].barbarian = true;
    s.players[2].civ = kNone;
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    addCity(s, 0, {3, 5}, true, 3);
    addCity(s, 1, {20, 5}, true, 3);
    s.players[0].gold = Fixed::fromInt(1000);
    s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    s.players[0].relations.resize(3);
    s.players[1].relations.resize(3);
    s.players[0].met.assign(3, 0);
    s.players[1].met.assign(3, 0);
    s.players[0].met[1] = 1;
    s.players[1].met[0] = 1;
    Camp c;
    c.id = s.nextCampId++;
    c.pos = {12, 5};
    c.tribe = 0;
    c.spawnTimer = 99;
    s.camps.push_back(c);
    return s;
}
}  // namespace

TEST(clans_need_their_mode_and_a_seen_camp) {
    GameState s = clanState();
    s.setup.barbarianClans = false;
    auto off = Game::fromScenario(rules(), s);
    const int32_t camp = off->state().camps[0].id;
    CHECK_EQ(off->validate(Command::bribeCamp(0, camp)), CommandError::CannotTreatWithClan);
    auto on = Game::fromScenario(rules(), clanState());
    CHECK_EQ(on->validate(Command::bribeCamp(0, camp)), CommandError::Ok);
    CHECK_EQ(on->clanProblem(1, camp, CommandType::BribeCamp, kNoPlayer), CommandError::CannotTreatWithClan);  // player 1 has never seen it
}

TEST(a_bribe_buys_peace_and_a_hire_a_unit) {
    auto g = Game::fromScenario(rules(), clanState());
    const int32_t camp = g->state().camps[0].id;
    const int bribe = g->clanCost(0, camp, CommandType::BribeCamp);
    REQUIRE(bribe > 0);
    REQUIRE(g->submit(Command::bribeCamp(0, camp)) == CommandError::Ok);
    CHECK(g->state().players[0].gold == Fixed::fromInt(1000 - bribe));
    CHECK(g->campLeavesAlone(g->state().camps[0], 0));
    CHECK(!g->campLeavesAlone(g->state().camps[0], 1));
    CHECK_EQ(g->validate(Command::bribeCamp(0, camp)), CommandError::CannotTreatWithClan);  // already bribed
    const size_t units = g->state().units.size();
    REQUIRE(g->submit(Command::hireFromCamp(0, camp)) == CommandError::Ok);
    REQUIRE(g->state().units.size() == units + 1);
    CHECK_EQ(g->state().units.back().owner, 0);
    CHECK_EQ(g->validate(Command::hireFromCamp(0, camp)), CommandError::CannotTreatWithClan);  // once every few turns
    CHECK_EQ(g->state().camps[0].progress, 20);  // a bribe and a hire bring it closer to a city-state
}

TEST(an_incited_camp_spares_everyone_but_its_target) {
    auto g = Game::fromScenario(rules(), clanState());
    const int32_t camp = g->state().camps[0].id;
    CHECK_EQ(g->validate(Command::inciteCamp(0, camp, 0)), CommandError::CannotTreatWithClan);  // not against oneself
    REQUIRE(g->submit(Command::inciteCamp(0, camp, 1)) == CommandError::Ok);
    CHECK(g->campLeavesAlone(g->state().camps[0], 0));
    CHECK(!g->campLeavesAlone(g->state().camps[0], 1));
    // Saved and loaded with its dealings.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().camps[0].incitedAgainst, 1);
    CHECK(loaded->state().setup.barbarianClans);
}

TEST(a_clan_becomes_a_city_state) {
    GameState s = clanState();
    s.camps[0].progress = 99;
    const UnitId raider = addUnit(s, "UNIT_WARRIOR", 2, {12, 6});
    s.units.back().camp = s.camps[0].id;
    auto g = Game::fromScenario(rules(), std::move(s));
    const size_t players = g->state().players.size();
    sovtest::endTurns(*g, 2);  // the world turn follows
    REQUIRE(g->state().players.size() == players + 1);
    const Player& cs = g->state().players.back();
    CHECK(cs.cityState != kNone);
    for (const Camp& c : g->state().camps) CHECK((c.pos != Hex{12, 5}));  // the clan's camp is gone (new camps may appear elsewhere)
    REQUIRE(g->state().cityAt({12, 5}));
    CHECK_EQ(g->state().cityAt({12, 5})->owner, cs.id);
    CHECK_EQ(g->state().unit(raider)->owner, cs.id);  // the clan's warriors serve it
    CHECK(g->atWar(2, cs.id));                         // the barbarians are at war with it
}
