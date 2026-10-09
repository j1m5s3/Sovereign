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
    CHECK(r.projects[at(r.project("PROJECT_MANHATTAN_PROJECT"))].modelled);  // it unlocks the Nuclear Device
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

// District projects cost more as the game progresses (03: GAME_PROGRESS 1500): x (1 + 15 x the larger share of the
// tech or civic tree completed), 25 at the start and 400 with a whole tree done.
TEST(district_projects_cost_more_as_the_game_progresses) {
    const auto cost = [](bool civicsDone) {
        GameState s = campusTown();
        Player& p = s.players[0];
        std::fill(p.techs.done.begin(), p.techs.done.end(), static_cast<uint8_t>(0));
        std::fill(p.civics.done.begin(), p.civics.done.end(), static_cast<uint8_t>(civicsDone ? 1 : 0));
        auto g = Game::fromScenario(rules(), s);
        return g->productionCost(0, project("PROJECT_CAMPUS_RESEARCH_GRANTS"));
    };
    CHECK_EQ(cost(false), 25);
    CHECK_EQ(cost(true), 400);
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

TEST(the_court_festival_pays_for_each_spare_luxury_copy) {
    GameState s = campusTown();
    std::vector<TypeIndex> luxuries;
    for (size_t i = 0; i < rules().resources.size(); ++i) {
        if (rules().resources[i].cls == ResourceClass::Luxury && rules().resources[i].reveal.none()) luxuries.push_back(static_cast<TypeIndex>(i));
    }
    REQUIRE(luxuries.size() >= 2);
    // Three copies of one luxury (two lasting grants and the city center's), one of another: two spare.
    s.plot({6, 6}).resource = luxuries[0];
    s.players[0].luxuryGrants = {luxuries[0], luxuries[0], luxuries[1]};
    auto g = Game::fromScenario(rules(), s);
    CHECK_EQ(g->luxuryCopies(0, luxuries[0]), 3);
    CHECK_EQ(g->luxuryCopies(0, luxuries[1]), 1);
    const Fixed before = g->state().players[0].civics.overflow;
    g->completeProject(g->stateMutForTests().cities[0], rules().project("PROJECT_COURT_FESTIVAL"));
    CHECK_EQ(g->state().players[0].civics.overflow, before + Fixed::fromInt(50 * 2));
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

TEST(the_spaceport_needs_flat_land) {
    GameState s = campusTown();
    Player& p = s.players[0];
    p.techs.done[at(rules().tech("TECH_ROCKETRY"))] = 1;
    s.plot({6, 8}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& c = g->state().cities[0];
    const TypeIndex port = rules().district("DISTRICT_SPACEPORT");
    CHECK(g->canPlaceDistrict(c, port, {6, 7}));
    CHECK(!g->canPlaceDistrict(c, port, {6, 8}));  // hills
}

namespace {
// Solar Farms on the first `n` open plots of the city's: 2 Power each.
void solarFarms(GameState& s, int n) {
    for (const Hex& h : s.grid.within(s.cities[0].pos, 2)) {
        if (n == 0) break;
        if (h == s.cities[0].pos || s.districtAt(h)) continue;
        s.plot(h).improvement = rules().improvement("IMPROVEMENT_SOLAR_FARM");
        --n;
    }
}
}  // namespace

TEST(the_space_race_and_the_science_victory) {
    GameState s = campusTown();
    s.cities[0].districts.push_back({rules().district("DISTRICT_SPACEPORT"), {6, 7}, true});
    solarFarms(s, 5);  // 10 Power for two laser stations
    Player& p = s.players[0];
    for (const char* t : {"TECH_ROCKETRY", "TECH_SATELLITES"}) p.techs.done[at(rules().tech(t))] = 1;
    p.visibility.assign(static_cast<size_t>(s.grid.size()), 0);
    auto g = Game::fromScenario(rules(), s);
    CHECK(g->canProduce(g->state().cities[0], project("PROJECT_LAUNCH_EARTH_SATELLITE")));
    CHECK(!g->canProduce(g->state().cities[0], project("PROJECT_LAUNCH_MOON_LANDING")));  // the satellite first
    g->completeProject(g->stateMutForTests().cities[0], rules().project("PROJECT_LAUNCH_EARTH_SATELLITE"));
    CHECK(std::all_of(g->state().players[0].visibility.begin(), g->state().players[0].visibility.end(), [](uint8_t v) { return v != 0; }));
    CHECK(g->canProduce(g->state().cities[0], project("PROJECT_LAUNCH_MOON_LANDING")));
    CHECK(!g->canProduce(g->state().cities[0], project("PROJECT_LAUNCH_EARTH_SATELLITE")));  // once
    CHECK_EQ(g->expeditionSpeed(0), 0);
    // The expedition, two laser stations: 3 light-years a turn; 50 to arrive.
    for (const char* pj : {"PROJECT_LAUNCH_MOON_LANDING", "PROJECT_LAUNCH_MARS_COLONY", "PROJECT_LAUNCH_EXOPLANET_EXPEDITION",
                           "PROJECT_BUILD_TERRESTRIAL_LASER_STATION", "PROJECT_BUILD_TERRESTRIAL_LASER_STATION"})
        g->completeProject(g->stateMutForTests().cities[0], rules().project(pj));
    CHECK_EQ(g->expeditionSpeed(0), 3);
    g->stateMutForTests().players[0].lightYears = 46;
    endTurns(*g, 1);
    CHECK_EQ(g->state().players[0].lightYears, 49);
    CHECK(!g->gameOver());
    endTurns(*g, 1);
    REQUIRE(g->gameOver());
    CHECK(g->state().victory == Victory::Science);
    CHECK_EQ(g->state().winner, 0);
}

// A Terrestrial Laser Station speeds the expedition only while its city is powered (09: "only counts if the city is
// powered"), and a civ counts no more stations than it built.
TEST(a_terrestrial_laser_station_needs_its_city_powered) {
    const auto speed = [](int farms, int stationsHere) {
        GameState s = campusTown();
        s.cities[0].districts.push_back({rules().district("DISTRICT_SPACEPORT"), {6, 7}, true});
        solarFarms(s, farms);
        auto g = Game::fromScenario(rules(), std::move(s));
        for (const char* pj : {"PROJECT_LAUNCH_MOON_LANDING", "PROJECT_LAUNCH_MARS_COLONY", "PROJECT_LAUNCH_EXOPLANET_EXPEDITION",
                               "PROJECT_BUILD_TERRESTRIAL_LASER_STATION", "PROJECT_BUILD_TERRESTRIAL_LASER_STATION"})
            g->completeProject(g->stateMutForTests().cities[0], rules().project(pj));
        g->stateMutForTests().cities[0].laserStations = stationsHere;
        endTurns(*g, 1);  // the city's power is counted as its owner's turn begins
        CHECK_EQ(g->state().cities[0].powerDemand, 5 * stationsHere);
        return g->expeditionSpeed(0);
    };
    CHECK_EQ(speed(5, 2), 3);  // 10 Power for 10 wanted
    CHECK_EQ(speed(4, 2), 1);  // 8 Power: neither station counts
    CHECK_EQ(speed(8, 3), 3);  // a third station here the civ did not build
}

// The Royal Society (03; data: each charge completes 2% of a project): a Builder's charge speeds the project its city
// is building, from the project's district, or from the City Center for a project that needs none.
TEST(builders_speed_projects_with_the_royal_society) {
    const ProductionItem grants = project("PROJECT_CAMPUS_RESEARCH_GRANTS"), manhattan = project("PROJECT_MANHATTAN_PROJECT");
    const TypeIndex society = rules().building("BUILDING_ROYAL_SOCIETY");
    auto town = [&](bool withSociety, ProductionItem item, Hex where, const char* unit, bool campusDone = true) {
        GameState s = campusTown();
        City& c = s.cities[0];
        if (withSociety) {
            c.buildings.push_back(society);
            std::sort(c.buildings.begin(), c.buildings.end());
        }
        c.districts[0].complete = campusDone;
        c.queue = {item};
        const UnitId u = sovtest::addUnit(s, unit, 0, where);
        return std::make_pair(Game::fromScenario(rules(), std::move(s)), u);
    };
    // Each project's share: 2% of its cost, a charge spent, the turn's moves gone.
    for (const auto& [which, where] : {std::pair<ProductionItem, Hex>{grants, {7, 6}}, {manhattan, {6, 6}}}) {
        const ProductionItem item = which;
        auto [g, builder] = town(true, item, where, "UNIT_BUILDER");
        const int charges = g->state().unit(builder)->charges;
        REQUIRE(g->chargeProblem(0, builder) == CommandError::Ok);
        REQUIRE(g->submit(Command::contributeCharge(0, builder)) == CommandError::Ok);
        const City& c = g->state().cities[0];
        auto it = std::find_if(c.progress.begin(), c.progress.end(), [&](const ProductionProgress& pp) { return pp.item == item; });
        REQUIRE(it != c.progress.end());
        CHECK(it->amount == Fixed::fromInt(g->productionCost(0, item, &c)) * 2 / 100);
        CHECK(it->amount > Fixed());
        CHECK_EQ(g->state().unit(builder)->charges, charges - 1);
        CHECK(g->state().unit(builder)->movesLeft == Fixed());
    }
    // Not without the Royal Society, away from where the project runs, on an unfinished Campus, while the city
    // builds something else, or with a Military Engineer.
    const auto refused = [&](bool withSociety, ProductionItem item, Hex where, const char* unit, bool campusDone = true) {
        auto [g, u] = town(withSociety, item, where, unit, campusDone);
        return g->chargeProblem(0, u) != CommandError::Ok;
    };
    CHECK(refused(false, grants, {7, 6}, "UNIT_BUILDER"));
    CHECK(refused(true, grants, {6, 6}, "UNIT_BUILDER"));   // the City Center, for a Campus project
    CHECK(refused(true, grants, {5, 6}, "UNIT_BUILDER"));   // another district
    CHECK(refused(true, manhattan, {7, 6}, "UNIT_BUILDER"));
    CHECK(refused(true, grants, {7, 6}, "UNIT_BUILDER", false));
    CHECK(refused(true, {ProductionKind::Building, rules().building("BUILDING_LIBRARY")}, {6, 6}, "UNIT_BUILDER"));
    CHECK(refused(true, grants, {7, 6}, "UNIT_MILITARY_ENGINEER"));
    // Nor with another civ's Royal Society, nor in another civ's city.
    for (const PlayerId holder : {PlayerId{0}, PlayerId{1}}) {
        GameState s = flatState(24, 14, 2);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        for (const PlayerId p : {PlayerId{0}, PlayerId{1}}) {
            const Hex center{static_cast<int16_t>(4 + 10 * p), 6}, campus{static_cast<int16_t>(5 + 10 * p), 6};
            addCity(s, p, center, true, 6);
            City& c = s.cities.back();
            c.districts.push_back({rules().district("DISTRICT_CAMPUS"), campus, true});
            s.plot(campus).owner = p;
            s.plot(campus).city = c.id;
            c.queue = {grants};
            if (p == holder) {
                c.buildings.push_back(society);
                std::sort(c.buildings.begin(), c.buildings.end());
            }
        }
        // Player 0's Builder: on its own Campus when player 1 holds the Royal Society, on player 1's when it does.
        const UnitId u = sovtest::addUnit(s, "UNIT_BUILDER", 0, {static_cast<int16_t>(holder == 1 ? 5 : 15), 6});
        auto f = Game::fromScenario(rules(), std::move(s));
        CHECK(f->chargeProblem(0, u) != CommandError::Ok);
    }
}
