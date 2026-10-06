// Scored competitions [GS] (08-diplomacy-city-states-governors.md: Scored Competitions).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

// Three majors with a capital each, the world in the given era.
GameState world(const char* era) {
    GameState s = flatState(32, 14, 3);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(3, uint8_t{1});
    }
    for (int i = 0; i < 3; ++i) addCity(s, static_cast<PlayerId>(i), {4 + 10 * i, 6}, true, 5);
    s.gameEra = rules().era(era);
    s.majorsAtStart = 3;
    return s;
}

Competition running(CompetitionKind kind, int endTurn, size_t players) {
    Competition c;
    c.kind = kind;
    c.endTurn = endTurn;
    c.scores.assign(players, 0);
    c.baseline.assign(players, 0);
    return c;
}
}  // namespace

TEST(a_congress_calls_a_competition_the_era_allows) {
    auto early = Game::fromScenario(rules(), world("ERA_MEDIEVAL"));
    early->startCompetition();
    CHECK(early->state().competitions.empty());
    auto g = Game::fromScenario(rules(), world("ERA_INDUSTRIAL"));
    g->startCompetition();
    REQUIRE(g->state().competitions.size() == 1u);
    CHECK(g->state().competitions[0].kind == CompetitionKind::WorldsFair);  // the only one the Industrial era allows
    CHECK_EQ(g->state().competitions[0].endTurn, g->state().turn + 29);
    g->startCompetition();  // one at a time
    CHECK_EQ(g->state().competitions.size(), 1u);
}

TEST(the_winner_takes_diplomatic_victory_points) {
    GameState s = world("ERA_INDUSTRIAL");
    Competition fair = running(CompetitionKind::WorldsFair, s.turn, 3);
    fair.scores = {40, 90, 10};
    s.competitions.push_back(fair);
    const int dvp1 = s.players[1].diplomaticVictoryPoints;
    const int favor0 = s.players[0].favor, favor2 = s.players[2].favor;
    const int gpp1 = s.players[1].greatPersonPoints[0];
    auto g = Game::fromScenario(rules(), std::move(s));
    g->processCompetitions();
    const GameState& st = g->state();
    CHECK(st.competitions[0].settled);
    CHECK_EQ(st.players[1].diplomaticVictoryPoints, dvp1 + 1);      // first place
    CHECK_EQ(st.players[1].greatPersonPoints[0], gpp1 + 100);        // and +100 toward every class
    CHECK_EQ(st.players[0].favor, favor0 + 50);                      // the rest of the top half
    CHECK_EQ(st.players[2].favor, favor2);                           // low tier: no favor at the Fair
}

TEST(the_climate_accords_reward_the_cleanest) {
    GameState s = world("ERA_ATOMIC");
    Competition accords = running(CompetitionKind::ClimateAccords, s.turn, 3);
    accords.baseline = {0, 0, 0};
    s.competitions.push_back(accords);
    s.players[0].co2 = 500;
    s.players[1].co2 = 0;
    s.players[2].co2 = 2000;
    const int dvp1 = s.players[1].diplomaticVictoryPoints;
    auto g = Game::fromScenario(rules(), std::move(s));
    g->processCompetitions();
    CHECK_EQ(g->state().players[1].diplomaticVictoryPoints, dvp1 + 2);
}

TEST(great_people_score_the_nobel_prizes) {
    GameState s = world("ERA_MODERN");
    s.competitions.push_back(running(CompetitionKind::NobelPhysics, s.turn + 29, 3));
    auto g = Game::fromScenario(rules(), std::move(s));
    g->competitionScore(0, CompetitionKind::NobelPhysics, 1);
    g->competitionScore(0, CompetitionKind::NobelLiterature, 1);  // no such competition running
    CHECK_EQ(g->competitionStanding(g->state().competitions[0], 0), 1);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->state().competitions.size() == 1u);
    CHECK_EQ(loaded->state().competitions[0].scores[0], 1);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(an_aid_request_opens_the_send_aid_project) {
    GameState s = world("ERA_MEDIEVAL");
    auto g = Game::fromScenario(rules(), s);
    const ProductionItem aid{ProductionKind::Project, rules().project("PROJECT_SEND_AID")};
    CHECK(!g->canProduce(g->state().cities[1], aid));  // no one asks yet
    g->requestAid(0);
    REQUIRE(g->runningAidRequest() != nullptr);
    CHECK(g->canProduce(g->state().cities[1], aid));
    CHECK(!g->canProduce(g->state().cities[0], aid));  // not the struck civ itself
    const Fixed gold = g->state().players[0].gold;
    g->completeProject(g->stateMutForTests().cities[1], aid.type);
    CHECK(g->state().players[0].gold == gold + Fixed::fromInt(200));
    CHECK_EQ(g->competitionStanding(*g->runningAidRequest(), 1), 200);
    GameState late = g->state();
    late.turn = late.competitions.back().endTurn;
    const int dvp = late.players[1].diplomaticVictoryPoints;
    auto h = Game::fromScenario(rules(), std::move(late));
    h->processCompetitions();
    CHECK_EQ(h->state().players[1].diplomaticVictoryPoints, dvp + 2);
}
