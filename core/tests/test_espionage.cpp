// Espionage (08: Espionage): capacity from civics, training spies, travel, the 3d6 ladder and
// what defends against it, each operation's effect, failure and capture, saves, the AI.
#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

// Player 0 (human) has Diplomatic Service and one spy; both civs have met; player 1 has a
// Campus, a Commercial Hub and some gold.
GameState spyState(int spyLevel = 1) {
    GameState s = flatState(30, 14, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(2, 1);
    }
    s.players[0].human = true;
    s.players[0].civics.done[at(rules().civic("CIVIC_DIPLOMATIC_SERVICE"))] = 1;
    s.players[1].gold = Fixed::fromInt(500);
    addCity(s, 0, {4, 6}, true, 5);
    const CityId theirs = addCity(s, 1, {16, 6}, true, 8);
    City& c = *s.city(theirs);
    for (const char* d : {"DISTRICT_CAMPUS", "DISTRICT_COMMERCIAL_HUB"}) {
        CityDistrict cd;
        cd.type = rules().district(d);
        cd.pos = {static_cast<int>(c.districts.size()) == 0 ? 17 : 15, 6};
        cd.complete = true;
        c.districts.push_back(cd);
    }
    for (size_t t = 0; t < 6; ++t) s.players[1].techs.done[t] = 1;
    Agent spy;
    spy.id = s.nextAgentId++;
    spy.owner = 0;
    spy.spy = true;
    spy.level = spyLevel;
    s.agents.push_back(spy);
    s.majorsAtStart = 2;
    return s;
}

CityId theirCity(const Game& g) { return g.state().cities[1].id; }
}  // namespace

TEST(spy_rules_data) {
    const Rules& r = rules();
    CHECK(r.units[at(r.unit("UNIT_SPY"))].spy);
    CHECK_EQ(r.civics[at(r.civic("CIVIC_DIPLOMATIC_SERVICE"))].spies, 1);
    CHECK_EQ(r.techs[at(r.tech("TECH_COMPUTERS"))].spies, 1);
    const SpyOperationType& siphon = r.spyOperations[at(r.spyOperation("SPYOP_SIPHON_FUNDS"))];
    CHECK_EQ(siphon.base, 13);
    CHECK_EQ(siphon.district, r.district("DISTRICT_COMMERCIAL_HUB"));
}

TEST(capacity_comes_from_civics) {
    auto g = Game::fromScenario(rules(), spyState());
    CHECK_EQ(g->spyCapacity(0), 1);
    CHECK_EQ(g->spiesOf(0), 1);
    CHECK_EQ(g->spyCapacity(1), 0);
    CHECK_EQ(g->agentsOf(0), 0);  // spies do not count against assassins
}

TEST(the_ladder_matches_the_spec) {
    auto g = Game::fromScenario(rules(), spyState());
    const int32_t spy = g->state().agents[0].id;
    // A recruit: 50% on Siphon Funds, 37% on Steal Tech Boost (08: Espionage, Success).
    CHECK_EQ(g->spySuccessPercent(spy, SpyMission::SiphonFunds, theirCity(*g)), 50);
    CHECK_EQ(g->spySuccessPercent(spy, SpyMission::StealTechBoost, theirCity(*g)), 37);
    GameState s = spyState(3);
    auto g3 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g3->spySuccessPercent(spy, SpyMission::SiphonFunds, theirCity(*g3)), 74);  // two steps up
}

TEST(a_counterspy_makes_it_harder) {
    GameState s = spyState();
    Agent guard;
    guard.id = s.nextAgentId++;
    guard.owner = 1;
    guard.spy = true;
    guard.level = 2;
    guard.city = s.cities[1].id;
    guard.mission = SpyMission::Counterspy;
    s.agents.push_back(guard);
    auto g = Game::fromScenario(rules(), std::move(s));
    // +3 for a counterspy and +1 for each level above the first: a 15 or better on 3d6, 9%.
    CHECK_EQ(g->spySuccessPercent(g->state().agents[0].id, SpyMission::SiphonFunds, theirCity(*g)), 9);
}

TEST(operations_need_their_district_and_a_met_rival) {
    auto g = Game::fromScenario(rules(), spyState());
    const int32_t spy = g->state().agents[0].id;
    CHECK(g->canSpyMission(0, spy, SpyMission::SiphonFunds, theirCity(*g)));
    CHECK(!g->canSpyMission(0, spy, SpyMission::SabotageProduction, theirCity(*g)));  // no Industrial Zone
    CHECK(!g->canSpyMission(0, spy, SpyMission::NeutralizeGovernor, theirCity(*g)));  // no governor
    CHECK(!g->canSpyMission(0, spy, SpyMission::SiphonFunds, g->state().cities[0].id));  // our own city
    CHECK(g->canSpyMission(0, spy, SpyMission::Counterspy, g->state().cities[0].id));
}

TEST(a_spy_travels_then_works_then_reports) {
    auto g = Game::fromScenario(rules(), spyState(4));  // a Master Spy: 84% on Siphon Funds
    const int32_t spy = g->state().agents[0].id;
    REQUIRE(g->submit(Command::spyMission(0, spy, SpyMission::SiphonFunds, theirCity(*g))) == CommandError::Ok);
    CHECK_EQ(g->agent(spy)->travel, 3);
    CHECK_EQ(g->agent(spy)->missionTurns, 8);
    // The city it works in is visible to its owner once it arrives.
    sovtest::endTurns(*g, 2 * 3);
    CHECK(g->visibility(0, g->state().cities[1].pos) == Visibility::Visible);
    const Fixed before = g->state().players[0].gold;
    sovtest::endTurns(*g, 2 * 8);
    bool reported = false;
    for (const GameEvent& e : g->state().events) {
        reported |= (e.kind == EventKind::SpyOperation || e.kind == EventKind::SpyCaught) && e.actor == 0 && e.target == 1;
    }
    CHECK(reported);
    if (const Agent* a = g->agent(spy); a && a->city != kNoCity) {
        CHECK(g->state().players[0].gold > before);  // funds siphoned (the spy stayed)
        CHECK(a->mission == SpyMission::None);
    }
}

TEST(each_operation_does_its_work) {
    // Neutralize Governor: their established governor starts establishing again.
    GameState s = spyState(4);
    Governor gov;
    gov.type = rules().governor("GOVERNOR_VICTOR");
    gov.city = s.cities[1].id;
    gov.promotions.push_back(rules().governors[at(gov.type)].promotions.front());
    s.players[1].governors.push_back(gov);
    for (int seed = 1; seed <= 20; ++seed) {
        GameState t = s;
        t.rng.seed(static_cast<uint64_t>(seed));
        Agent& a = t.agents[0];
        a.city = t.cities[1].id;
        a.mission = SpyMission::NeutralizeGovernor;
        a.missionTurns = 1;
        auto g = Game::fromScenario(rules(), std::move(t));
        sovtest::endTurns(*g, 2);
        const bool hit = g->state().players[1].governors[0].establishTurns > 0;
        const bool caught = !g->agent(g->state().agents.empty() ? -1 : 1) || g->state().events.back().kind == EventKind::SpyCaught;
        CHECK(hit || caught);
        if (hit) {
            CHECK_EQ(g->state().players[1].governors[0].establishTurns, 6);  // ESPIONAGE_NEUTRALIZE_GOVERNOR_BASE_TURNS
            break;
        }
    }
}

TEST(a_caught_spy_is_remembered) {
    GameState s = spyState(1);
    // A level-4 counterspy makes failure almost certain.
    Agent guard;
    guard.id = s.nextAgentId++;
    guard.owner = 1;
    guard.spy = true;
    guard.level = 4;
    guard.city = s.cities[1].id;
    guard.mission = SpyMission::Counterspy;
    s.agents.push_back(guard);
    Agent& a = s.agents[0];
    a.city = s.cities[1].id;
    a.mission = SpyMission::StealTechBoost;
    a.missionTurns = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK(g->state().events.back().kind == EventKind::SpyCaught);
    bool remembered = false;
    for (const OpinionReason& r : g->opinionReasons(1, 0)) remembered |= r.kind == OpinionReasonKind::SpyCaught && r.value < 0;
    CHECK(remembered);
}

TEST(spies_survive_a_save) {
    auto g = Game::fromScenario(rules(), spyState());
    const int32_t spy = g->state().agents[0].id;
    REQUIRE(g->submit(Command::spyMission(0, spy, SpyMission::ListeningPost, theirCity(*g))) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->agent(spy));
    CHECK(loaded->agent(spy)->spy);
    CHECK(loaded->agent(spy)->mission == SpyMission::ListeningPost);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(the_ai_sends_idle_spies_out) {
    GameState s = spyState();
    s.players[0].human = false;
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    const Agent* a = g->agent(g->state().agents[0].id);
    REQUIRE(a);
    CHECK(a->mission != SpyMission::None);
}

// ---- the remaining operations and spy promotions (08: Espionage)

namespace {
void addDistrict(City& c, const char* id, Hex at) {
    CityDistrict cd;
    cd.type = rules().district(id);
    cd.pos = at;
    cd.complete = true;
    c.districts.push_back(cd);
}

// Runs the operation in player 1's capital over a few seeds; returns the game after the first success.
std::unique_ptr<Game> succeed(GameState s, SpyMission m) {
    for (int seed = 1; seed <= 40; ++seed) {
        GameState t = s;
        t.rng.seed(static_cast<uint64_t>(seed));
        Agent& a = t.agents[0];
        a.city = t.cities[1].id;
        a.mission = m;
        a.missionTurns = 1;
        auto g = Game::fromScenario(rules(), std::move(t));
        sovtest::endTurns(*g, 2);
        if (!g->state().events.empty() && g->state().events.back().kind == EventKind::SpyOperation) return g;
    }
    return nullptr;
}
}  // namespace

TEST(a_great_work_heist_and_disrupted_rocketry) {
    GameState s = spyState(4);
    City& theirs = s.cities[1];
    addDistrict(theirs, "DISTRICT_THEATER_SQUARE", {16, 7});
    // A Great Work in their Palace that our own Palace has room for.
    auto probe = Game::fromScenario(rules(), s);
    TypeIndex work = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size() && work == kNone; ++w) {
        if (probe->freeGreatWorkSlot(probe->state().cities[0], static_cast<TypeIndex>(w)) != kNone) work = static_cast<TypeIndex>(w);
    }
    REQUIRE(work != kNone);
    GreatWork gw;
    gw.type = work;
    gw.building = probe->freeGreatWorkSlot(probe->state().cities[1], work);
    REQUIRE(gw.building != kNone);
    theirs.greatWorks.push_back(gw);
    auto g = succeed(s, SpyMission::GreatWorkHeist);
    REQUIRE(g);
    CHECK(g->state().cities[1].greatWorks.empty());
    CHECK_EQ(g->state().cities[0].greatWorks.size(), 1u);

    // Disrupt Rocketry: the space race project's progress is lost.
    GameState r = spyState(4);
    addDistrict(r.cities[1], "DISTRICT_SPACEPORT", {16, 7});
    r.players[1].techs.done[at(rules().tech("TECH_ROCKETRY"))] = 1;  // so the satellite stays buildable
    const ProductionItem launch{ProductionKind::Project, rules().project("PROJECT_LAUNCH_EARTH_SATELLITE")};
    r.cities[1].queue = {launch};
    r.cities[1].progress.push_back({launch, Fixed::fromInt(400)});
    auto h = succeed(r, SpyMission::DisruptRocketry);
    REQUIRE(h);
    for (const ProductionProgress& pp : h->state().cities[1].progress) {
        if (pp.item == launch) CHECK(pp.amount < Fixed::fromInt(400));
    }
}

TEST(a_successful_spy_earns_a_promotion) {
    GameState s = spyState(1);
    auto g = succeed(s, SpyMission::SiphonFunds);
    REQUIRE(g);
    const Agent* a = g->agent(s.agents[0].id);
    REQUIRE(a);
    CHECK_EQ(a->level, 2);
    CHECK_EQ(a->promotionsPending, 1);
    // Con Artist: +2 levels on Siphon Funds, so better odds there.
    TypeIndex con = kNone;
    for (size_t i = 0; i < rules().spyPromotions.size(); ++i) {
        if (rules().spyPromotions[i].id == "SPY_PROMOTION_CON_ARTIST") con = static_cast<TypeIndex>(i);
    }
    REQUIRE(con != kNone);
    const int before = g->spySuccessPercent(a->id, SpyMission::SiphonFunds, theirCity(*g));
    while (g->state().currentPlayer != 0) sovtest::endTurns(*g, 1);
    REQUIRE(g->submit(Command::promoteSpy(0, a->id, con)) == CommandError::Ok);
    CHECK(g->spySuccessPercent(a->id, SpyMission::SiphonFunds, theirCity(*g)) > before);
    CHECK(g->submit(Command::promoteSpy(0, a->id, con)) != CommandError::Ok);  // none pending now
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->agent(a->id)->promotions.size(), 1u);
}

TEST(a_fabricated_scandal_costs_the_suzerain_envoys) {
    // A Scientific city-state (player 2) whose suzerain is player 1 (4 envoys); player 0's spy works there.
    GameState s = flatState(30, 14, 3);
    s.players[2].civ = kNone;
    for (size_t i = 0; i < rules().cityStates.size(); ++i) {
        if (rules().cityStates[i].kind == CityStateKind::Scientific) s.players[2].cityState = static_cast<TypeIndex>(i);
    }
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(3, uint8_t{1});
        p.envoys.assign(3, 0);
    }
    s.players[0].human = true;
    s.players[0].civics.done[at(rules().civic("CIVIC_DIPLOMATIC_SERVICE"))] = 1;
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 1, {16, 6}, true, 5);
    addCity(s, 2, {24, 6}, true, 2);
    s.players[1].envoys[2] = 4;
    Agent spy;
    spy.id = s.nextAgentId++;
    spy.owner = 0;
    spy.spy = true;
    spy.level = 4;
    s.agents.push_back(spy);
    s.majorsAtStart = 2;
    auto probe = Game::fromScenario(rules(), s);
    REQUIRE(probe->suzerainOf(2) == 1);
    CHECK(probe->canSpyMission(0, spy.id, SpyMission::FabricateScandal, probe->state().cities[2].id));
    for (int seed = 1; seed <= 40; ++seed) {
        GameState t = s;
        t.rng.seed(static_cast<uint64_t>(seed));
        t.agents[0].city = t.cities[2].id;
        t.agents[0].mission = SpyMission::FabricateScandal;
        t.agents[0].missionTurns = 1;
        auto g = Game::fromScenario(rules(), std::move(t));
        while (g->state().currentPlayer != 0 || g->state().turn == s.turn) {
            const int before = g->state().turn * 10 + g->state().currentPlayer;
            sovtest::endTurns(*g, 1);
            if (g->state().turn * 10 + g->state().currentPlayer == before) break;
        }
        if (g->state().events.empty() || g->state().events.back().kind != EventKind::SpyOperation) continue;
        CHECK_EQ(g->envoysAt(1, 2), 0);  // 4 envoys, less 1 + the spy's level 4, never below 0
        return;
    }
    CHECK(false);  // never succeeded
}

// ---- the Intelligence Agency, the Chancery and Listening Posts (08: Espionage)

TEST(an_intelligence_agency_adds_a_spy_and_a_listening_post_hears_all) {
    GameState s = spyState();
    const int before = Game::fromScenario(rules(), s)->spyCapacity(0);
    s.cities[0].buildings.push_back(rules().building("BUILDING_INTELLIGENCE_AGENCY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    // A spy in a Listening Post in player 1's city.
    s.agents[0].city = s.cities[1].id;
    s.agents[0].mission = SpyMission::ListeningPost;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->spyCapacity(0), before + 1);
    const GameEvent work{1, EventKind::SpyOperation, 1, kNoPlayer, 0};  // needs Top Secret otherwise
    CHECK(g->accessLevel(0, 1) < 4);
    CHECK(g->hearsOf(0, work));
}
