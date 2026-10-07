// Nuclear weapons (05-units-and-combat.md: Nuclear weapons; data: units.md, WMDs).
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
ProductionItem project(const char* id) { return {ProductionKind::Project, rules().project(id)}; }
TypeIndex nuke() { return rules().wmd("WMD_NUCLEAR_DEVICE"); }

// Player 0's city at (4,6) holds one Nuclear Device; player 1's at (11,6) has 6 citizens; at war.
// Player 2, at peace with both, has a city at (11,11).
GameState armed() {
    GameState s = flatState(24, 14, 3);
    addCity(s, 0, {4, 6}, true, 8);
    addCity(s, 1, {11, 6}, true, 6);
    addCity(s, 2, {11, 11}, true, 3);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        for (const char* t : {"TECH_FLIGHT", "TECH_ADVANCED_FLIGHT", "TECH_NUCLEAR_FISSION"}) p.techs.done[at(rules().tech(t))] = 1;
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
        p.relations.resize(3);
        p.met.assign(3, uint8_t{1});
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    s.players[0].wmds[at(nuke())] = 1;
    return s;
}
}  // namespace

TEST(wmd_data_and_projects) {
    const Rules& r = rules();
    REQUIRE(nuke() != kNone);
    const WmdType& n = r.wmds[at(nuke())];
    CHECK_EQ(n.blastRadius, 1);
    CHECK_EQ(n.falloutTurns, 10);
    CHECK_EQ(n.icbmRange, 12);
    CHECK_EQ(r.wmds[at(r.wmd("WMD_THERMONUCLEAR_DEVICE"))].blastRadius, 2);
    CHECK(r.units[at(r.unit("UNIT_BOMBER"))].deliversWmd);
    CHECK(r.units[at(r.unit("UNIT_NUCLEAR_SUBMARINE"))].deliversWmd);
    CHECK(r.units[at(r.unit("UNIT_GIANT_DEATH_ROBOT"))].wmdImmune);

    GameState s = armed();
    s.players[0].wmds[at(nuke())] = 0;
    s.players[0].stockpile[at(r.resource("RESOURCE_URANIUM"))] = 10;
    auto g = Game::fromScenario(r, std::move(s));
    const City& c = g->state().cities[0];
    // The device needs the Manhattan Project first; building one spends 10 Uranium and costs 14 Gold a turn.
    CHECK(g->canProduce(c, project("PROJECT_MANHATTAN_PROJECT")));
    CHECK(!g->canProduce(c, project("PROJECT_BUILD_NUCLEAR_DEVICE")));
    g->completeProject(g->stateMutForTests().cities[0], r.project("PROJECT_MANHATTAN_PROJECT"));
    CHECK(g->canProduce(g->state().cities[0], project("PROJECT_BUILD_NUCLEAR_DEVICE")));
    const Fixed gold = g->goldPerTurn(0);
    g->completeProject(g->stateMutForTests().cities[0], r.project("PROJECT_BUILD_NUCLEAR_DEVICE"));
    CHECK_EQ(g->state().players[0].wmds[at(nuke())], 1);
    CHECK_EQ(g->state().players[0].stockpile[at(r.resource("RESOURCE_URANIUM"))], 0);
    CHECK(g->goldPerTurn(0) == gold - Fixed::fromInt(14));
}

TEST(a_bomber_delivers_a_nuclear_device) {
    GameState s = armed();
    const UnitId bomber = addUnit(s, "UNIT_BOMBER", 0, {4, 6});  // range 10
    const UnitId tank = addUnit(s, "UNIT_TANK", 1, {11, 6});
    const UnitId robot = addUnit(s, "UNIT_GIANT_DEATH_ROBOT", 1, {12, 6});
    const UnitId outside = addUnit(s, "UNIT_TANK", 1, {14, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    // Not at a civ at peace, nor beyond the bomber's range.
    CHECK(g->submit(Command::launchWmd(0, bomber, nuke(), {11, 11})) == CommandError::BadTarget);
    CHECK(g->submit(Command::launchWmd(0, bomber, nuke(), {20, 6})) == CommandError::BadTarget);
    REQUIRE(g->submit(Command::launchWmd(0, bomber, nuke(), {11, 6})) == CommandError::Ok);
    const GameState& st = g->state();
    CHECK_EQ(st.players[0].wmds[at(nuke())], 0);
    CHECK_EQ(st.players[0].wmdsLaunched, 1);
    CHECK(st.unit(tank) == nullptr);
    CHECK(st.unit(robot) != nullptr);     // resists
    CHECK(st.unit(outside) != nullptr);   // beyond the blast
    CHECK(st.unit(bomber)->movesLeft == Fixed());
    const City& hit = *st.cityAt({11, 6});
    CHECK_EQ(hit.wallHp, 0);
    CHECK_EQ(hit.population, 1);          // 6 citizens, 7 of its plots in the blast
    CHECK_EQ(st.plot({11, 6}).fallout, 10);
    CHECK_EQ(st.plot({12, 6}).fallout, 10);
    CHECK_EQ(st.plot({14, 6}).fallout, 0);
    // The victim and the onlooker remember.
    bool victim = false, onlooker = false;
    for (const OpinionReason& r : g->opinionReasons(1, 0)) victim |= r.kind == OpinionReasonKind::UsedWmd && r.value < -30;
    for (const OpinionReason& r : g->opinionReasons(2, 0)) onlooker |= r.kind == OpinionReasonKind::UsedWmd && r.value < 0;
    CHECK(victim);
    CHECK(onlooker);
    // No devices left.
    CHECK(g->submit(Command::launchWmd(0, bomber, nuke(), {11, 6})) == CommandError::NotEnoughResources);
}

TEST(fallout_poisons_the_ground) {
    GameState s = armed();
    const UnitId bomber = addUnit(s, "UNIT_BOMBER", 0, {4, 6});
    s.plot({10, 8}).improvement = rules().improvement("IMPROVEMENT_FARM");
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::launchWmd(0, bomber, nuke(), {10, 8})) == CommandError::Ok);
    CHECK(g->state().plot({10, 8}).pillagedTurns == kPillagedUntilRepaired);  // pillaged until a Builder repairs it (05)
    const City& c = *g->state().cityAt({11, 6});
    const Yields y = g->plotYields({10, 8}, c);
    for (size_t i = 0; i < kNumYields; ++i) CHECK(y[i] == Fixed());
    GameState st = g->state();
    const UnitId walker = addUnit(st, "UNIT_TANK", 1, {10, 8});
    auto h = Game::fromScenario(rules(), std::move(st));
    h->processFallout();
    CHECK_EQ(h->state().unit(walker)->hp, 100 - rules().globalInt("PLOT_CONTAMINATION_DAMAGE_BASE"));
    CHECK_EQ(h->state().plot({10, 8}).fallout, 9);
    for (int i = 0; i < 9; ++i) h->processFallout();
    CHECK(h->state().unit(walker) == nullptr);
    CHECK_EQ(h->state().plot({10, 8}).fallout, 0);
}

TEST(missile_silos_launch_within_icbm_range) {
    GameState s = armed();
    s.plot({3, 5}).improvement = rules().improvement("IMPROVEMENT_MISSILE_SILO");
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->submit(Command::launchWmdFromSilo(0, {3, 6}, nuke(), {11, 6})) == CommandError::BadUnit);  // no silo there
    CHECK(g->submit(Command::launchWmdFromSilo(1, {3, 5}, nuke(), {4, 6})) != CommandError::Ok);        // not theirs
    REQUIRE(g->submit(Command::launchWmdFromSilo(0, {3, 5}, nuke(), {11, 6})) == CommandError::Ok);
    CHECK_EQ(g->state().plot({11, 6}).fallout, 10);
}

TEST(nuclear_weapons_survive_a_save) {
    GameState s = armed();
    const UnitId bomber = addUnit(s, "UNIT_BOMBER", 0, {4, 6});
    s.players[0].wmds[at(nuke())] = 2;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::launchWmd(0, bomber, nuke(), {11, 6})) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].wmds[at(nuke())], 1);
    CHECK_EQ(loaded->state().plot({11, 6}).fallout, 10);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(the_ai_answers_a_nuclear_strike_in_kind) {
    // Player 1 (AI) holds a device and a bomber in reach of player 0's city; we play it after player 0's turn.
    const auto playOne = [](GameState s) {
        auto g = Game::fromScenario(rules(), std::move(s));
        for (UnitId id : g->unitsNeedingOrders(0)) g->submit(Command::setActivity(0, id, Activity::Sleep));
        sovtest::endTurns(*g, 1);
        if (g->state().currentPlayer == 1) ai::playTurn(*g);
        return g;
    };
    GameState s = armed();
    addUnit(s, "UNIT_BOMBER", 1, {11, 6});
    s.players[1].wmds[at(nuke())] = 1;
    s.players[1].stockpile[at(rules().resource("RESOURCE_ALUMINUM"))] = 10;  // the bomber's upkeep
    s.players[1].gold = Fixed::fromInt(500);
    CHECK_EQ(playOne(s)->state().players[1].wmdsLaunched, 0);  // never first
    // Struck by player 0 (as its launch leaves it), it strikes back.
    s.players[1].memories.push_back({0, MemoryKind::UsedWmd, -40, 100, s.turn});
    auto g = playOne(s);
    CHECK_EQ(g->state().players[1].wmdsLaunched, 1);
}

TEST(a_blast_pillages_the_districts_in_it) {
    GameState s = armed();
    s.cities[1].districts.push_back({rules().district("DISTRICT_CAMPUS"), {12, 6}, true});
    s.cities[1].districts.push_back({rules().district("DISTRICT_THEATER_SQUARE"), {14, 6}, true});  // outside the blast
    const UnitId bomber = addUnit(s, "UNIT_BOMBER", 0, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::launchWmd(0, bomber, nuke(), {11, 6})) == CommandError::Ok);
    const City& c = *g->state().cityAt({11, 6});
    CHECK_EQ(static_cast<int>(c.districts[0].pillagedTurns), std::max<int>(kPillagedDistrictTurns, rules().wmds[at(nuke())].falloutTurns));
    CHECK_EQ(static_cast<int>(c.districts[1].pillagedTurns), 0);
}
