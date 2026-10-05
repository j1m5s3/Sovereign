// Emergencies [R&F/GS] (08-diplomacy-city-states-governors.md: Emergencies).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
// Three civs who have all met, each with a capital: 0 at (4,6), 1 at (14,6), 2 at (24,6).
GameState trio() {
    GameState s = flatState(32, 14, 3);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(3, uint8_t{1});
        p.relations.resize(3);
        p.gold = Fixed::fromInt(300);
    }
    for (int i = 0; i < 3; ++i) addCity(s, static_cast<PlayerId>(i), {4 + 10 * i, 6}, true, 3);
    s.majorsAtStart = 3;
    return s;
}
}  // namespace

TEST(taking_a_city_calls_a_military_emergency) {
    GameState s = trio();
    auto g = Game::fromScenario(rules(), s);
    const CityId taken = g->state().cities[1].id;
    g->triggerEmergency(EmergencyKind::Military, 0, taken, 1);  // as captureCity calls it
    REQUIRE(g->state().emergencies.size() == 1u);
    const Emergency& e = g->state().emergencies[0];
    CHECK_EQ(e.target, 0);
    CHECK_EQ(e.endTurn, g->state().turn + 30);
    CHECK(e.members[1]);                  // the civ wronged joins at once
    CHECK(!g->canJoinEmergency(0, 0));    // not the target
    CHECK(g->canJoinEmergency(2, 0));
    sovtest::endTurns(*g, 2);  // joining happens on the member's own turn
    REQUIRE(g->submit(Command::joinEmergency(2, 0)) == CommandError::Ok);
    CHECK(g->inEmergencyAgainst(2, 0));
    // Friends of the target may not join; one emergency of a kind at a time.
    g->triggerEmergency(EmergencyKind::Military, 0, taken, 1);
    CHECK_EQ(g->state().emergencies.size(), 1u);
}

TEST(an_emergency_ends_in_success_or_failure) {
    GameState s = trio();
    // Player 0 holds player 1's capital; the members take it back.
    s.cities[1].owner = 0;
    auto g = Game::fromScenario(rules(), s);
    const CityId taken = g->state().cities[1].id;
    g->triggerEmergency(EmergencyKind::Military, 0, taken, 1);
    sovtest::endTurns(*g, 2);
    REQUIRE(g->submit(Command::joinEmergency(2, 0)) == CommandError::Ok);
    g->processEmergencies();
    CHECK_EQ(g->state().emergencies[0].outcome, 0);  // still running
    GameState back = g->state();
    back.cities[1].owner = 1;
    const int favor1 = back.players[1].favor, favor2 = back.players[2].favor;
    auto won = Game::fromScenario(rules(), std::move(back));
    won->processEmergencies();
    CHECK_EQ(won->state().emergencies[0].outcome, 1);
    CHECK_EQ(won->state().players[1].favor, favor1 + 100);
    CHECK_EQ(won->state().players[2].favor, favor2 + 100);
    // Time runs out with the city still held: the target is rewarded.
    GameState late = g->state();
    late.turn = late.emergencies[0].endTurn;
    const int favor0 = late.players[0].favor;
    auto lost = Game::fromScenario(rules(), std::move(late));
    lost->processEmergencies();
    CHECK_EQ(lost->state().emergencies[0].outcome, 2);
    CHECK_EQ(lost->state().players[0].favor, favor0 + 200);
}

TEST(war_on_a_friend_is_a_betrayal_and_members_war_freely) {
    GameState s = trio();
    s.players[0].relations[1].friendsUntil = s.players[1].relations[0].friendsUntil = 40;
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    REQUIRE(g->state().emergencies.size() == 1u);
    CHECK(g->state().emergencies[0].kind == EmergencyKind::Betrayal);
    CHECK_EQ(g->state().emergencies[0].endTurn, g->state().turn + 60);
    // An Emergency War: on its turn player 2 joins and declares on the target without new grievances against it.
    const int before = g->grievances(0, 2);
    sovtest::endTurns(*g, 2);
    REQUIRE(g->state().currentPlayer == 2);
    REQUIRE(g->submit(Command::joinEmergency(2, 0)) == CommandError::Ok);
    REQUIRE(g->submit(Command::declareWar(2, 0)) == CommandError::Ok);
    CHECK(g->grievances(0, 2) <= before);
}

TEST(emergencies_survive_a_save) {
    GameState s = trio();
    auto g = Game::fromScenario(rules(), std::move(s));
    g->triggerEmergency(EmergencyKind::Nuclear, 0, kNoCity, 1);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->state().emergencies.size() == 1u);
    CHECK(loaded->state().emergencies[0].kind == EmergencyKind::Nuclear);
    CHECK(loaded->state().emergencies[0].members[1]);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
