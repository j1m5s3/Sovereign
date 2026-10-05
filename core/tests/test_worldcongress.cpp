// Grievances, Diplomatic Favor and the World Congress (08 [GS]): where grievances come from and
// how they fade, favor per turn, convening, voting with favor, resolutions in force, the
// Diplomatic Victory, saves.
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

GameState wcState() {
    GameState s = flatState(30, 14, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(2, 1);
        p.grievances.assign(2, 0);
    }
    s.players[0].human = true;
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 1, {16, 6}, true, 5);
    s.majorsAtStart = 2;
    return s;
}

// A session with one resolution and votes already cast.
GameState inSession(const char* resolution, uint8_t option, int32_t candidate) {
    GameState s = wcState();
    CongressItem item;
    item.resolution = rules().resolution(resolution);
    item.candidates = {0, 1};
    item.votes.push_back({0, option, candidate, 2});
    item.votes.push_back({1, option, candidate, 1});
    s.congress.push_back(item);
    s.congressOpenedTurn = s.turn;
    s.nextCongressTurn = s.turn + 30;
    return s;
}
}  // namespace

TEST(congress_rules_data) {
    const Rules& r = rules();
    int supported = 0;
    for (const ResolutionType& res : r.resolutions) supported += res.kind != ResolutionKind::Unsupported ? 1 : 0;
    CHECK_EQ(supported, 7);
    CHECK_EQ(r.resolutions[at(r.resolution("RESOLUTION_DIPLOMATIC_VICTORY"))].minEra, r.era("ERA_MODERN"));
    CHECK_EQ(r.eras[0].grievanceDecay, 10);
    CHECK_EQ(r.governments[at(r.government("GOVERNMENT_MONARCHY"))].favor, 2);
    CHECK(!r.promotionClasses.empty());
}

TEST(wars_and_denunciations_breed_grievances_that_fade) {
    auto g = Game::fromScenario(rules(), wcState());
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK_EQ(g->grievances(1, 0), 150);  // a surprise war
    sovtest::endTurns(*g, 2);            // one world turn: Ancient decay 10
    CHECK_EQ(g->grievances(1, 0), 140);
    bool weighs = false;
    for (const OpinionReason& r : g->opinionReasons(1, 0)) weighs |= r.kind == OpinionReasonKind::Grievances && r.value == -14;
    CHECK(weighs);
    auto g2 = Game::fromScenario(rules(), wcState());
    REQUIRE(g2->submit(Command::denounce(0, 1)) == CommandError::Ok);
    CHECK_EQ(g2->grievances(1, 0), 25);
}

TEST(holding_a_capital_breeds_grievances) {
    GameState s = wcState();
    addCity(s, 1, {10, 10}, false, 3);
    s.cities.back().originalOwner = 0;
    s.cities.back().originalCapital = true;  // once 0's capital
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK_EQ(g->grievances(0, 1), 0);  // +3, then the Ancient era's decay of 10
    GameState s2 = g->state();
    s2.gameEra = 8;  // the Information era decays 3 a turn
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    sovtest::endTurns(*g2, 2);
    CHECK_EQ(g2->grievances(0, 1), 0);
}

TEST(favor_comes_from_government_and_suffers_from_grievances) {
    GameState s = wcState();
    s.players[0].government = rules().government("GOVERNMENT_MONARCHY");
    s.players[1].grievances[0] = 450;  // 250 over the start: -5
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->favorPerTurn(0), 2 - 5);
    CHECK_EQ(g->favorPerTurn(1), 0);
}

TEST(the_congress_convenes_in_the_medieval_era) {
    GameState s = wcState();
    // A Medieval tech makes player 1 a Medieval civ.
    for (size_t i = 0; i < rules().techs.size(); ++i) {
        if (rules().techs[i].era == 2) {
            s.players[1].techs.done[i] = 1;
            break;
        }
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(!g->congressInSession());
    sovtest::endTurns(*g, 2);
    REQUIRE(g->congressInSession());
    CHECK_EQ(g->state().congress.size(), 2u);
    CHECK(g->hasVoted(1, 0));   // the AI votes as the session opens
    CHECK(!g->hasVoted(0, 0));  // the human votes on their turn
    REQUIRE(g->submit(Command::congressVote(0, 0, 0, 0)) == CommandError::Ok);
    CHECK(g->submit(Command::congressVote(0, 0, 0, 0)) == CommandError::CannotVote);  // once
    CHECK(g->submit(Command::congressVote(0, 1, 0, 0, 3)) == CommandError::CannotVote);  // 60 favor it lacks
    sovtest::endTurns(*g, 2);
    CHECK(!g->congressInSession());
    CHECK(g->state().nextCongressTurn > g->state().turn);
}

TEST(trade_policy_and_patronage_take_effect) {
    auto g = Game::fromScenario(rules(), inSession("RESOLUTION_TRADE_POLICY", 0, 0));
    const int before = g->tradeRouteCapacity(0);
    sovtest::endTurns(*g, 2);
    REQUIRE(g->passed(ResolutionKind::TradePolicy));
    CHECK_EQ(g->tradeRouteCapacity(0), before + 1);
    auto g2 = Game::fromScenario(rules(), inSession("RESOLUTION_TRADE_POLICY", 1, 1));
    sovtest::endTurns(*g2, 2);
    CHECK_EQ(g2->tradeRouteCapacity(1), 0);
}

TEST(diplomatic_victory_points_win_the_game) {
    GameState s = inSession("RESOLUTION_DIPLOMATIC_VICTORY", 0, 0);
    s.players[0].diplomaticVictoryPoints = 18;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK_EQ(g->state().players[0].diplomaticVictoryPoints, 20);
    CHECK(g->state().victory == Victory::Diplomatic);
    CHECK_EQ(g->state().winner, 0);
}

TEST(congress_survives_a_save) {
    GameState s = inSession("RESOLUTION_PATRONAGE", 0, 1);
    s.players[0].favor = 40;
    s.players[1].grievances[0] = 77;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->congressInSession());
    CHECK_EQ(loaded->state().congress[0].votes.size(), 2u);
    CHECK_EQ(loaded->state().players[0].favor, 40);
    CHECK_EQ(loaded->grievances(1, 0), 77);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
