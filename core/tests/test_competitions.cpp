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

TEST(a_grievous_war_calls_a_military_aid_request_and_sessions_keep_their_distance) {
    GameState s = world("ERA_MEDIEVAL");
    s.turn = 40;
    for (Player& p : s.players) p.relations.resize(3);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    s.players[0].grievances.assign(3, 0);
    s.players[1].grievances.assign(3, 0);
    s.players[0].grievances[1] = 250;  // player 0 holds 250 grievances against player 1
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->grievances(0, 1) >= 200);
    g->checkMilitaryAid();
    const Competition* aid = g->runningAidRequest();
    REQUIRE(aid);
    CHECK(aid->kind == CompetitionKind::MilitaryAidRequest);
    CHECK_EQ(aid->beneficiary, 0);
    const ProductionItem send{ProductionKind::Project, rules().project("PROJECT_SEND_AID")};
    CHECK(g->canProduce(g->state().cities[2], send));
    CHECK(!g->canProduce(g->state().cities[1], send));  // at war with the civ asking
    // Another special session must wait WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS turns.
    CHECK(!g->specialSessionDue());
    g->triggerEmergency(EmergencyKind::Military, 1, kNoCity, 0);
    CHECK(g->state().emergencies.empty());
}

TEST(train_athletes_scores_the_world_games_and_a_plant_can_be_decommissioned) {
    GameState s = world("ERA_MODERN");
    const TypeIndex coal = rules().building("BUILDING_COAL_POWER_PLANT");
    s.cities[1].buildings.push_back(coal);
    std::sort(s.cities[1].buildings.begin(), s.cities[1].buildings.end());
    CityDistrict zone;
    zone.type = rules().district("DISTRICT_INDUSTRIAL_ZONE");
    zone.pos = {15, 7};
    zone.complete = true;
    s.cities[1].districts.push_back(zone);
    auto g = Game::fromScenario(rules(), s);
    const ProductionItem athletes{ProductionKind::Project, rules().project("PROJECT_TRAIN_ATHLETES")};
    const ProductionItem decommission{ProductionKind::Project, rules().project("PROJECT_DECOMMISSION_COAL_POWER_PLANT")};
    CHECK(rules().projects[at(athletes.type)].modelled);
    CHECK(!g->canProduce(g->state().cities[1], athletes));  // no World Games running
    CHECK(g->canProduce(g->state().cities[1], decommission));
    CHECK(!g->canProduce(g->state().cities[0], decommission));  // no plant there
    s.competitions.push_back(running(CompetitionKind::WorldGames, 60, 3));
    auto h = Game::fromScenario(rules(), std::move(s));
    REQUIRE(h->canProduce(h->state().cities[1], athletes));
    const int before = h->competitionStanding(h->state().competitions[0], 1);
    h->completeProject(h->stateMutForTests().cities[1], athletes.type);
    CHECK_EQ(h->competitionStanding(h->state().competitions[0], 1), before + 50);
    h->completeProject(h->stateMutForTests().cities[1], decommission.type);
    CHECK(!h->state().cities[1].has(coal));
}
