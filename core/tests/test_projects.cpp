// City projects (03-districts-buildings-wonders.md, Projects; data: projects.md).
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
ProductionItem project(const char* id) { return {ProductionKind::Project, rules().project(id)}; }

// A city with a finished Campus and Entertainment Complex.
GameState campusTown() {
    GameState s = flatState(20, 14, 1);
    addCity(s, 0, {6, 6}, true, 6);
    City& c = s.cities[0];
    c.districts.push_back({rules().district("DISTRICT_CAMPUS"), {7, 6}, true});
    c.districts.push_back({rules().district("DISTRICT_ENTERTAINMENT_COMPLEX"), {5, 6}, true});
    for (const Hex& h : s.grid.within({6, 6}, 2)) {
        s.plot(h).owner = 0;
        s.plot(h).city = c.id;
    }
    Game::fitPlayerToRules(s.players[0], rules());
    return s;
}
}  // namespace

TEST(project_data_from_civ_tables) {
    const Rules& r = rules();
    const ProjectType& grants = r.projects[at(r.project("PROJECT_CAMPUS_RESEARCH_GRANTS"))];
    CHECK(grants.converts);
    CHECK(grants.conversionYield == YieldType::Science);
    CHECK_EQ(grants.conversionPercent, 15);
    CHECK_EQ(grants.greatPersonPoints.size(), 1u);
    CHECK(grants.modelled);
    CHECK(!r.projects[at(r.project("PROJECT_MANHATTAN_PROJECT"))].modelled);  // nuclear weapons are not carried
    CHECK_EQ(r.projects[at(r.project("PROJECT_LAUNCH_MOON_LANDING"))].prerequisite, r.project("PROJECT_LAUNCH_EARTH_SATELLITE"));
}

TEST(district_projects_convert_production_and_grant_great_people) {
    GameState s = campusTown();
    auto g = Game::fromScenario(rules(), s);
    const CityId id = g->state().cities[0].id;
    const std::vector<ProductionItem> items = g->buildableItems(id);
    CHECK(std::find(items.begin(), items.end(), project("PROJECT_CAMPUS_RESEARCH_GRANTS")) != items.end());
    CHECK(std::find(items.begin(), items.end(), project("PROJECT_HOLY_SITE_PRAYERS")) == items.end());  // no Holy Site
    CHECK(g->purchaseCost(0, project("PROJECT_CAMPUS_RESEARCH_GRANTS")) < 0);  // never bought
    const Fixed before = g->cityReport(id).yields[static_cast<size_t>(YieldType::Science)];
    REQUIRE(g->submit(Command::setProduction(0, id, project("PROJECT_CAMPUS_RESEARCH_GRANTS"))) == CommandError::Ok);
    const CityReport rep = g->cityReport(id);
    CHECK(rep.yields[static_cast<size_t>(YieldType::Science)] == before + rep.yields[static_cast<size_t>(YieldType::Production)] * 15 / 100);
    const TypeIndex scientist = rules().greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
    const int gppBefore = g->state().players[0].greatPersonPoints[at(scientist)];
    const int cost = g->productionCost(0, project("PROJECT_CAMPUS_RESEARCH_GRANTS"));
    CHECK(cost >= 25);
    for (int t = 0; t < 40 && g->state().players[0].projectsDone[at(rules().project("PROJECT_CAMPUS_RESEARCH_GRANTS"))] == 0; ++t) endTurns(*g, 1);
    CHECK_EQ(g->state().players[0].projectsDone[at(rules().project("PROJECT_CAMPUS_RESEARCH_GRANTS"))], 1);
    CHECK(g->state().players[0].greatPersonPoints[at(scientist)] >= gppBefore + 10);
}

TEST(projects_with_one_time_effects) {
    GameState s = campusTown();
    s.cities[0].loyalty = 60;
    s.co2 = 80000;
    s.players[0].co2 = 80000;
    auto g = Game::fromScenario(rules(), s);
    City& c = g->stateMutForTests().cities[0];
    g->completeProject(c, rules().project("PROJECT_BREAD_AND_CIRCUSES"));
    CHECK_EQ(g->state().cities[0].loyalty, 80);
    const int favor = g->state().players[0].favor;
    g->completeProject(g->stateMutForTests().cities[0], rules().project("PROJECT_CARBON_RECAPTURE"));
    CHECK_EQ(g->state().co2, 30000);
    CHECK_EQ(g->state().players[0].favor, favor + 30);
    // Repair Outer Defenses is offered only while the walls are down.
    CHECK(!g->canProduce(g->state().cities[0], project("PROJECT_REPAIR_OUTER_DEFENSES")));
}

TEST(projects_survive_a_save) {
    GameState s = campusTown();
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::setProduction(0, g->state().cities[0].id, project("PROJECT_CAMPUS_RESEARCH_GRANTS"))) == CommandError::Ok);
    g->completeProject(g->stateMutForTests().cities[0], rules().project("PROJECT_BREAD_AND_CIRCUSES"));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].projectsDone[at(rules().project("PROJECT_BREAD_AND_CIRCUSES"))], 1);
    CHECK(loaded->state().cities[0].queue.front() == project("PROJECT_CAMPUS_RESEARCH_GRANTS"));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}
