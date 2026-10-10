// Espionage (08: Espionage): capacity from civics, training spies, travel, the 3d6 ladder and
// what defends against it, each operation's effect, failure and capture, saves, the AI.
#include <algorithm>

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

// A counterspy guards one district and those beside it (08: Counterspy): guarding the Campus it does not reach the
// Commercial Hub two plots away; guarding the City Center, which both touch, it covers both, and so it does when no
// district is chosen.
TEST(a_counterspy_guards_its_district_and_those_beside_it) {
    const auto odds = [](int guard, int* escape = nullptr) {
        GameState s = spyState();
        Agent g;
        g.id = s.nextAgentId++;
        g.owner = 1;
        g.spy = true;
        g.level = 2;
        g.city = s.cities[1].id;
        g.mission = SpyMission::Counterspy;
        g.guard = guard;
        s.agents.push_back(g);
        auto game = Game::fromScenario(rules(), std::move(s));
        if (escape) *escape = game->spyEscapeNeed(game->state().agents[0], game->state().cities[1], SpyMission::SiphonFunds, 1);
        return game->spySuccessPercent(game->state().agents[0].id, SpyMission::SiphonFunds, theirCity(*game));
    };
    const GameState s = spyState();
    CHECK_EQ(odds(s.grid.index({17, 6})), 50);  // the Campus: the Hub is out of its reach
    CHECK_EQ(odds(s.grid.index({15, 6})), 9);   // the Hub itself
    CHECK_EQ(odds(s.grid.index({16, 6})), 9);   // the City Center, beside it
    CHECK_EQ(odds(-1), 9);
    // Escaping too: only a counterspy guarding the target changes it.
    int far = 0, near = 0;
    odds(s.grid.index({17, 6}), &far);
    odds(s.grid.index({16, 6}), &near);
    CHECK_EQ(far, rules().globalInt("ESPIONAGE_ESCAPE_BASE_CHANCE"));
    CHECK_EQ(near, far - 2 * rules().globalInt("ESPIONAGE_ESCAPE_COUNTERSPY_LEVEL_MODIFIER"));
    // With the City Center and one district beside it, either covers both: the center is taken.
    GameState two = spyState();
    two.cities[1].districts.pop_back();
    auto g2 = Game::fromScenario(rules(), std::move(two));
    CHECK(g2->counterspyPlot(g2->state().cities[1]) == (Hex{16, 6}));
    // Sent by command: the district chosen, or the plot covering most when the choice is not one of the city's.
    GameState t = spyState();
    City& home = t.cities[0];  // ours at (4,6): a Campus east and a Commercial Hub west of it
    for (const char* d : {"DISTRICT_CAMPUS", "DISTRICT_COMMERCIAL_HUB"}) {
        CityDistrict cd;
        cd.type = rules().district(d);
        cd.pos = {home.districts.empty() ? 5 : 3, 6};
        cd.complete = true;
        home.districts.push_back(cd);
    }
    const int32_t spy = t.agents[0].id;
    auto g = Game::fromScenario(rules(), std::move(t));
    const CityId ours = g->state().cities[0].id;
    CHECK(g->counterspyPlot(*g->state().city(ours)) == (Hex{4, 6}));
    REQUIRE(g->submit(Command::spyMission(0, spy, SpyMission::Counterspy, ours, Hex{5, 6})) == CommandError::Ok);
    CHECK_EQ(g->agent(spy)->guard, g->state().grid.index({5, 6}));
    REQUIRE(g->submit(Command::spyMission(0, spy, SpyMission::Counterspy, ours, Hex{20, 6})) == CommandError::Ok);
    CHECK_EQ(g->agent(spy)->guard, g->state().grid.index({4, 6}));
    // Kept through a save.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->agent(spy)->guard, g->state().grid.index({4, 6}));
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

TEST(a_breached_dam_leaves_the_floodplains_to_the_builders) {
    GameState s = spyState(4);
    s.plot({16, 7}).feature = rules().feature("FEATURE_FLOODPLAINS_GRASSLAND");
    s.plot({16, 7}).improvement = rules().improvement("IMPROVEMENT_FARM");
    auto g = succeed(s, SpyMission::BreachDam);
    REQUIRE(g);
    CHECK(g->state().plot({16, 7}).pillagedTurns == kPillagedUntilRepaired);  // until a Builder repairs it (05: Pillage)
    sovtest::endTurns(*g, 16);
    CHECK(g->state().plot({16, 7}).pillagedTurns == kPillagedUntilRepaired);
}

TEST(recruited_partisans_carry_the_owners_best_melee_arms) {
    // Two rebels (barbarians) of the strongest melee unit the city's owner can field: with no techs, Warriors, never
    // Lahore's Nihang (25 strength and needing no tech, but the city-state's own; 08).
    GameState s = spyState(4);
    addDistrict(s.cities[1], "DISTRICT_NEIGHBORHOOD", {16, 7});
    std::fill(s.players[1].techs.done.begin(), s.players[1].techs.done.end(), 0);
    Player rebels;
    rebels.id = 2;
    rebels.civ = kNone;
    rebels.barbarian = true;
    s.players.push_back(rebels);
    auto g = succeed(s, SpyMission::RecruitPartisans);
    REQUIRE(g);
    int warriors = 0;
    for (const Unit& u : g->state().units) {
        if (u.owner != 2) continue;
        CHECK(u.type != rules().unit("UNIT_NIHANG"));
        warriors += u.type == rules().unit("UNIT_WARRIOR") ? 1 : 0;
    }
    CHECK(warriors >= 2);  // the two partisans, besides any a barbarian camp sent out
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

TEST(a_captured_spy_is_traded_back) {
    GameState s = spyState(2);
    CapturedSpy held;
    held.spy = s.agents[0];
    held.captor = 1;
    s.capturedSpies.push_back(held);
    s.agents.clear();
    s.players[0].gold = Fixed::fromInt(200);
    auto g = Game::fromScenario(rules(), std::move(s));
    const int32_t id = g->state().capturedSpies[0].spy.id;
    CHECK(g->agent(id) == nullptr);  // held, not at its owner's command
    // Saved while held.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->state().capturedSpies.size() == 1u);
    CHECK_EQ(loaded->state().capturedSpies[0].captor, 1);
    // Only the captor offers it, and only to its owner.
    const std::vector<DealItem> offer = g->offerableItems(1, 0);
    CHECK(std::any_of(offer.begin(), offer.end(), [&](const DealItem& i) { return i.kind == DealItemKind::Captive && i.amount == id; }));
    CHECK(g->dealProblem({0, 0, 1, 0, {{DealItemKind::Captive, 0, id, kNone}}}) != CommandError::Ok);
    // Bought back for gold: it comes home idle, keeping its level.
    const std::vector<DealItem> terms = {{DealItemKind::Captive, 1, id, kNone}, {DealItemKind::Gold, 0, 90, kNone}};
    REQUIRE(g->wouldAccept(1, {0, 0, 1, 0, terms}));
    REQUIRE(g->submit(Command::proposeDeal(0, 1, terms)) == CommandError::Ok);
    REQUIRE(g->agent(id) != nullptr);
    CHECK_EQ(g->agent(id)->level, 2);
    CHECK_EQ(g->agent(id)->owner, 0);
    CHECK(g->agent(id)->city == kNoCity);
    CHECK(g->state().capturedSpies.empty());
}

TEST(cryptography_sharpens_our_spies_and_blunts_theirs) {
    GameState s = spyState(1);
    for (Player& p : s.players) {
        p.government = rules().government("GOVERNMENT_CHIEFDOM");
        p.policies.assign(2, kNone);
    }
    auto plain = Game::fromScenario(rules(), s);
    const int32_t spy = s.agents[0].id;
    const CityId target = s.cities[1].id;
    const int base = plain->spySuccessPercent(spy, SpyMission::SiphonFunds, target);
    GameState ours = s;
    ours.players[0].policies[0] = rules().policy("POLICY_CRYPTOGRAPHY");
    auto sharp = Game::fromScenario(rules(), std::move(ours));
    CHECK(sharp->spySuccessPercent(spy, SpyMission::SiphonFunds, target) > base);
    s.players[1].policies[0] = rules().policy("POLICY_CRYPTOGRAPHY");
    auto blunt = Game::fromScenario(rules(), std::move(s));
    CHECK(blunt->spySuccessPercent(spy, SpyMission::SiphonFunds, target) < base);
}

TEST(a_diplomatic_quarter_shields_the_districts_beside_it) {
    // [GS] Foreign spies work two levels lower against the Diplomatic Quarter and the districts beside it; a
    // mission with no target district works from the City Center. Their city is at (16,6), its Campus at (17,6)
    // and its Commercial Hub at (15,6).
    const auto odds = [](Hex quarter, bool complete, SpyMission m) {
        GameState s = spyState();
        CityDistrict dq;
        dq.type = rules().district("DISTRICT_DIPLOMATIC_QUARTER");
        dq.pos = quarter;
        dq.complete = complete;
        s.cities[1].districts.push_back(dq);
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->spySuccessPercent(g->state().agents[0].id, m, theirCity(*g));
    };
    const Hex far{16, 10};
    CHECK_EQ(odds(far, true, SpyMission::StealTechBoost), 37);
    CHECK_EQ(odds({18, 6}, true, SpyMission::StealTechBoost), 16);  // beside the Campus: two steps down
    CHECK_EQ(odds({18, 6}, false, SpyMission::StealTechBoost), 37);  // not built yet
    CHECK_EQ(odds({18, 6}, true, SpyMission::SiphonFunds), 50);     // the Commercial Hub is out of reach
    CHECK_EQ(odds({18, 6}, true, SpyMission::FomentUnrest), odds(far, true, SpyMission::FomentUnrest));
    CHECK_EQ(odds({16, 7}, true, SpyMission::FomentUnrest), 25);    // beside the City Center
    CHECK_EQ(odds(far, true, SpyMission::FomentUnrest), 50);
}

TEST(a_consulate_lowers_foreign_spies_in_its_city_and_encampment_cities) {
    // [GS] Foreign spies work a level lower in the Consulate's city and in its owner's cities with an Encampment.
    // Their capital at (16,6) and a second city at (24,6) each have a Commercial Hub.
    const auto odds = [](bool consulate, bool camp, bool atCapital) {
        GameState s = spyState();
        CityDistrict dq;
        dq.type = rules().district("DISTRICT_DIPLOMATIC_QUARTER");
        dq.pos = {16, 10};  // too far to shield the Commercial Hub itself
        dq.complete = true;
        s.cities[1].districts.push_back(dq);
        if (consulate) {
            s.cities[1].buildings.push_back(rules().building("BUILDING_CONSULATE"));
            std::sort(s.cities[1].buildings.begin(), s.cities[1].buildings.end());
        }
        const CityId other = addCity(s, 1, {24, 6}, false, 8);
        City& c = *s.city(other);
        CityDistrict hub;
        hub.type = rules().district("DISTRICT_COMMERCIAL_HUB");
        hub.pos = {25, 6};
        hub.complete = true;
        c.districts.push_back(hub);
        if (camp) {
            CityDistrict e;
            e.type = rules().district("DISTRICT_ENCAMPMENT");
            e.pos = {24, 8};
            e.complete = true;
            c.districts.push_back(e);
        }
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->spySuccessPercent(g->state().agents[0].id, SpyMission::SiphonFunds, atCapital ? theirCity(*g) : other);
    };
    CHECK_EQ(odds(false, false, true), 50);
    CHECK_EQ(odds(true, false, true), 37);   // the Consulate's city: a step down
    CHECK_EQ(odds(true, false, false), 50);  // another city without an Encampment
    CHECK_EQ(odds(true, true, false), 37);   // another city with one
    CHECK_EQ(odds(false, true, false), 50);  // no Consulate anywhere
}

// Spies cost their upkeep though they are off the map (08: Espionage, 4 Gold), and so do assassins (2); Conscription
// takes 1 off each, as off a unit's.
TEST(spies_and_assassins_cost_their_upkeep) {
    const auto income = [](int spies, int assassins, bool conscription, bool foreignSpy = true) {
        GameState s = spyState();
        s.agents.clear();
        for (int k = 0; k < spies + assassins; ++k) {
            Agent a;
            a.id = s.nextAgentId++;
            a.owner = 0;
            a.spy = k < spies;
            s.agents.push_back(a);
        }
        if (foreignSpy) {
            Agent theirs;  // another civ's spy costs player 0 nothing
            theirs.id = s.nextAgentId++;
            theirs.owner = 1;
            theirs.spy = true;
            s.agents.push_back(theirs);
        }
        if (conscription) {
            Player& p = s.players[0];
            p.government = rules().government("GOVERNMENT_CHIEFDOM");
            p.policies.assign(static_cast<size_t>(rules().governments[at(p.government)].totalSlots()), kNone);
            p.policies[0] = rules().policy("POLICY_CONSCRIPTION");
        }
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->goldPerTurn(0);
    };
    const Fixed none = income(0, 0, false);
    CHECK_EQ(income(0, 0, false, false), none);
    CHECK_EQ(income(1, 0, false), none - Fixed::fromInt(4));
    CHECK_EQ(income(2, 0, false), none - Fixed::fromInt(8));
    CHECK_EQ(income(1, 1, false), none - Fixed::fromInt(6));
    CHECK_EQ(income(1, 1, true), income(0, 0, true) - Fixed::fromInt(4));
}
