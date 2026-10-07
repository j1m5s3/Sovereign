#include <algorithm>

#include "helpers.h"
#include "sovereign/mapgen.h"

using namespace sov;
using sovtest::addUnit;
using sovtest::capitalScenario;
using sovtest::CityScenario;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
constexpr size_t F = static_cast<size_t>(YieldType::Food);
constexpr size_t P = static_cast<size_t>(YieldType::Production);
constexpr size_t G = static_cast<size_t>(YieldType::Gold);
constexpr size_t C = static_cast<size_t>(YieldType::Culture);

ProductionItem unitItem(const char* id) { return {ProductionKind::Unit, rules().unit(id)}; }
ProductionItem buildingItem(const char* id) { return {ProductionKind::Building, rules().building(id)}; }

}  // namespace

TEST(fixed_pow_matches_civ_curves) {
    CHECK_EQ(Fixed::pow(Fixed::fromInt(4), Fixed::ratio(3, 2)), Fixed::fromInt(8));
    CHECK_EQ(Fixed::pow(Fixed::fromInt(9), Fixed::ratio(3, 2)), Fixed::fromInt(27));
    CHECK_EQ(Fixed::pow(Fixed::fromInt(1), Fixed::ratio(13, 10)), Fixed::fromInt(1));
    CHECK_EQ(Fixed::pow(Fixed(), Fixed::ratio(13, 10)), Fixed());
    CHECK_EQ(Fixed::pow(Fixed::fromInt(6), Fixed::ratio(13, 10)).toString(), std::string("10.2706"));
    CHECK_EQ(Fixed::pow(Fixed::fromInt(2), Fixed::ratio(1, 2)).toString(), std::string("1.4142"));
    for (int n = 1; n <= 60; ++n) {
        // Perfect squares give exact n^1.5.
        CHECK_EQ(Fixed::pow(Fixed::fromInt(n * n), Fixed::ratio(3, 2)), Fixed::fromInt(n * n * n));
    }
}

TEST(city_growth_and_border_thresholds) {
    auto sc = capitalScenario();
    REQUIRE(sc.game);
    const Game& g = *sc.game;
    // 02-cities.md: pop 1->2 needs 15, 2->3 needs 24.
    CHECK_EQ(g.growthThreshold(1), 15);
    CHECK_EQ(g.growthThreshold(2), 24);
    CHECK_EQ(g.growthThreshold(3), 33);   // 15 + 16 + 2.83
    CHECK_EQ(g.growthThreshold(10), 114);  // 15 + 72 + 27
    // Border growth: 10 + (6n)^1.3.
    CHECK_EQ(g.borderGrowthCost(0), 10);
    CHECK_EQ(g.borderGrowthCost(1), 20);
    CHECK_EQ(g.borderGrowthCost(2), 35);
}

TEST(city_founding_center_and_palace) {
    GameState s = flatState(20, 14, 1);
    s.plot({6, 6}).terrain = rules().terrain("TERRAIN_PLAINS");
    s.plot({6, 6}).feature = rules().feature("FEATURE_FOREST");
    auto sc = capitalScenario(std::move(s));
    REQUIRE(sc.city != kNoCity);
    const Game& g = *sc.game;
    const City& c = *g.state().city(sc.city);
    CHECK(c.capital);
    CHECK(c.has(rules().building("BUILDING_PALACE")));
    CHECK_EQ(g.state().plot(c.pos).feature, kNone);  // woods cleared
    Yields center = g.plotYields(c.pos, c);
    CHECK_EQ(center[F], Fixed::fromInt(2));  // plains 1F raised to 2F
    CHECK_EQ(center[P], Fixed::fromInt(1));
    CHECK_EQ(c.worked.size(), 1u);
    CityReport r = g.cityReport(sc.city);
    CHECK_EQ(r.housing, Fixed::fromInt(3));  // no water 2 + Palace 1
    CHECK_EQ(r.amenities, 2);
    CHECK_EQ(r.amenitiesNeeded, 0);
    CHECK_EQ(rules().happiness[static_cast<size_t>(r.happiness)].id, std::string("HAPPINESS_CONTENT"));
    // center 1P + Palace 2P; grassland worked plot 2F.
    CHECK_EQ(r.yields[P], Fixed::fromInt(3));
    CHECK_EQ(r.yields[F], Fixed::fromInt(4));
    CHECK_EQ(r.yields[G], Fixed::fromInt(5));
    CHECK_EQ(r.yields[C].toString(), std::string("1.3"));  // Palace 1 + 0.3 per citizen
    CHECK_EQ(r.defense, 3);                                 // Palace modifier
}

TEST(city_housing_from_water) {
    GameState s = flatState(20, 14, 1);
    setRiver(s, {6, 6}, Dir::E);
    auto river = capitalScenario(std::move(s));
    CHECK_EQ(river.game->cityReport(river.city).housing, Fixed::fromInt(6));  // fresh water 5 + Palace
    // The sea: a body of water larger than LAKE_MAX_AREA_SIZE (9 plots).
    GameState s2 = flatState(20, 14, 1);
    for (int y = 0; y < 14; ++y) s2.plot({7, y}).terrain = rules().terrain("TERRAIN_COAST");
    auto coast = capitalScenario(std::move(s2));
    CHECK_EQ(coast.game->cityReport(coast.city).housing, Fixed::fromInt(4));  // coastal 3 + Palace
    // A lake is fresh water (01: Lake).
    GameState s3 = flatState(20, 14, 1);
    s3.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
    auto lake = capitalScenario(std::move(s3));
    CHECK_EQ(lake.game->cityReport(lake.city).housing, Fixed::fromInt(6));  // fresh water 5 + Palace
}

TEST(city_needs_production_and_grows) {
    auto sc = capitalScenario();
    Game& g = *sc.game;
    CHECK_EQ(g.submit(Command::endTurn(0)), CommandError::ProductionNeeded);
    REQUIRE(g.submit(Command::setProduction(0, sc.city, buildingItem("BUILDING_MONUMENT"))) == CommandError::Ok);
    // 4 food - 2 eaten = 2 surplus; 15 needed: grows on the 8th turn.
    endTurns(g, 7);
    CHECK_EQ(g.state().city(sc.city)->population, 1);
    CHECK_EQ(g.state().city(sc.city)->food, Fixed::fromInt(14));
    endTurns(g, 1);
    CHECK_EQ(g.state().city(sc.city)->population, 2);
    CHECK_EQ(g.state().city(sc.city)->food, Fixed());
    CHECK_EQ(g.state().city(sc.city)->worked.size(), 2u);
}

TEST(city_production_completes_with_overflow) {
    auto sc = capitalScenario();
    Game& g = *sc.game;
    REQUIRE(g.submit(Command::setProduction(0, sc.city, unitItem("UNIT_WARRIOR"))) == CommandError::Ok);
    REQUIRE(g.submit(Command::queueProduction(0, sc.city, buildingItem("BUILDING_MONUMENT"))) == CommandError::Ok);
    CHECK_EQ(g.submit(Command::queueProduction(0, sc.city, buildingItem("BUILDING_MONUMENT"))), CommandError::CannotBuild);
    CHECK_EQ(g.submit(Command::setProduction(0, sc.city, buildingItem("BUILDING_GRANARY"))), CommandError::CannotBuild);  // needs Pottery
    CHECK_EQ(g.submit(Command::setProduction(0, sc.city, buildingItem("BUILDING_PALACE"))), CommandError::CannotBuild);
    // 3 production per turn: the Warrior (40) completes on turn 14 with 2 left over.
    for (int t = 0; t < 14; ++t) {
        for (UnitId id : g.unitsNeedingOrders(0)) g.submit(Command::setActivity(0, id, Activity::Fortify));
        endTurns(g, 1);
    }
    int warriors = 0;
    for (const Unit& u : g.state().units) warriors += u.type == rules().unit("UNIT_WARRIOR");
    CHECK_EQ(warriors, 1);
    const City& c = *g.state().city(sc.city);
    REQUIRE(c.queue.size() == 1u);
    CHECK(c.queue[0] == buildingItem("BUILDING_MONUMENT"));
    CHECK_EQ(c.overflow, Fixed::fromInt(2));
    CHECK_EQ(g.state().players[0].unitsTrained[static_cast<size_t>(rules().unit("UNIT_WARRIOR"))], 1);
}

TEST(city_settler_costs_and_population) {
    auto sc = capitalScenario();
    Game& g = *sc.game;
    CHECK_EQ(g.productionCost(0, unitItem("UNIT_SETTLER")), 80);
    GameState s = g.state();
    s.players[0].unitsTrained[static_cast<size_t>(rules().unit("UNIT_SETTLER"))] = 2;
    s.players[0].gold = Fixed::fromInt(1000);
    auto g2 = Game::fromScenario(rules(), s);
    CHECK_EQ(g2->productionCost(0, unitItem("UNIT_SETTLER")), 140);  // 80 + 2 x 30
    // Pop 1 cannot give up a citizen for a Settler.
    CHECK_EQ(g2->submit(Command::purchase(0, sc.city, unitItem("UNIT_SETTLER"))), CommandError::CannotBuild);
    GameState s3 = g2->state();
    s3.cities[0].population = 3;
    auto g3 = Game::fromScenario(rules(), s3);
    REQUIRE(g3->submit(Command::purchase(0, sc.city, unitItem("UNIT_SETTLER"))) == CommandError::Ok);
    CHECK_EQ(g3->state().city(sc.city)->population, 2);
    CHECK_EQ(g3->state().players[0].gold, Fixed::fromInt(1000 - 560));  // 140 x 4
}

TEST(city_never_trains_great_people_and_needs_unit_buildings) {
    auto sc = capitalScenario();
    GameState s = sc.game->state();
    Player& p = s.players[0];
    p.techs.done.assign(rules().techs.size(), 1);  // everything known
    p.civics.done.assign(rules().civics.size(), 1);
    auto g = Game::fromScenario(rules(), s);
    const City& c = *g->state().city(sc.city);
    for (const char* id : {"UNIT_GREAT_GENERAL", "UNIT_GREAT_SCIENTIST", "UNIT_GREAT_PROPHET"}) {
        CHECK(!rules().units[static_cast<size_t>(rules().unit(id))].trainable);
        CHECK(!g->canProduce(c, unitItem(id)));
    }
    // A Military Engineer needs an Armory in the city.
    CHECK(!g->canProduce(c, unitItem("UNIT_MILITARY_ENGINEER")));
    GameState armed = g->state();
    std::vector<TypeIndex>& b = armed.cities[0].buildings;
    b.push_back(rules().building("BUILDING_ARMORY"));
    std::sort(b.begin(), b.end());
    auto g2 = Game::fromScenario(rules(), armed);
    CHECK(g2->canProduce(*g2->state().city(sc.city), unitItem("UNIT_MILITARY_ENGINEER")));
}

TEST(city_purchases_with_gold) {
    auto sc = capitalScenario();
    GameState s = sc.game->state();
    s.players[0].gold = Fixed::fromInt(300);
    auto g = Game::fromScenario(rules(), s);
    CHECK_EQ(g->purchaseCost(0, buildingItem("BUILDING_MONUMENT")), 240);
    CHECK_EQ(g->purchaseCost(0, unitItem("UNIT_WARRIOR")), 160);
    REQUIRE(g->submit(Command::setProduction(0, sc.city, buildingItem("BUILDING_MONUMENT"))) == CommandError::Ok);
    REQUIRE(g->submit(Command::purchase(0, sc.city, buildingItem("BUILDING_MONUMENT"))) == CommandError::Ok);
    CHECK(g->state().city(sc.city)->has(rules().building("BUILDING_MONUMENT")));
    CHECK(g->state().city(sc.city)->queue.empty());
    CHECK_EQ(g->state().players[0].gold, Fixed::fromInt(60));
    CHECK_EQ(g->submit(Command::purchase(0, sc.city, unitItem("UNIT_WARRIOR"))), CommandError::NotEnoughGold);
    CHECK_EQ(g->submit(Command::purchase(0, sc.city, buildingItem("BUILDING_MONUMENT"))), CommandError::CannotBuild);
}

TEST(city_buy_plot_and_lock_citizen) {
    auto sc = capitalScenario();
    GameState s = sc.game->state();
    s.players[0].gold = Fixed::fromInt(200);
    auto g = Game::fromScenario(rules(), s);
    // (8,6) is two plots east, next to the city's first ring.
    CHECK_EQ(g->plotPurchaseCost(sc.city, {8, 6}), 50);
    CHECK_EQ(g->plotPurchaseCost(sc.city, {9, 6}), -1);  // not adjacent to territory
    CHECK_EQ(g->plotPurchaseCost(sc.city, {7, 6}), -1);  // already owned
    REQUIRE(g->submit(Command::buyPlot(0, sc.city, {8, 6})) == CommandError::Ok);
    CHECK_EQ(g->state().plot({8, 6}).city, sc.city);
    CHECK_EQ(g->state().players[0].gold, Fixed::fromInt(150));
    CHECK_EQ(g->plotPurchaseCost(sc.city, {9, 6}), 75);  // three out, now adjacent
    REQUIRE(g->submit(Command::lockPlot(0, sc.city, {8, 6}, true)) == CommandError::Ok);
    const City& c = *g->state().city(sc.city);
    REQUIRE(c.worked.size() == 1u);
    CHECK_EQ(c.worked[0], g->state().grid.index({8, 6}));
    CHECK_EQ(g->submit(Command::lockPlot(0, sc.city, {5, 6}, true)), CommandError::CannotWorkPlot);  // pop 1
    CHECK_EQ(g->submit(Command::lockPlot(0, sc.city, {12, 6}, true)), CommandError::CannotWorkPlot);  // not owned
    REQUIRE(g->submit(Command::lockPlot(0, sc.city, {8, 6}, false)) == CommandError::Ok);
}

TEST(city_borders_grow_with_culture) {
    auto sc = capitalScenario();
    Game& g = *sc.game;
    REQUIRE(g.submit(Command::setProduction(0, sc.city, buildingItem("BUILDING_MONUMENT"))) == CommandError::Ok);
    auto owned = [&] {
        int n = 0;
        for (const Plot& p : g.state().plots) n += p.city == sc.city;
        return n;
    };
    CHECK_EQ(owned(), 7);
    // 1.3 culture per turn reaches 10 on turn 8 (pop grows to 2 on turn 8 too).
    endTurns(g, 7);
    CHECK_EQ(owned(), 7);
    endTurns(g, 1);
    CHECK_EQ(owned(), 8);
    CHECK_EQ(g.state().city(sc.city)->plotsByCulture, 1);
}

TEST(city_amenities_and_mood) {
    auto sc = capitalScenario();
    GameState s = sc.game->state();
    // A second, non-capital city with 5 citizens and no amenities: Displeased.
    s.cities.push_back(s.cities[0]);
    City& other = s.cities.back();
    other.id = 99;
    other.capital = false;
    other.buildings.clear();
    other.pos = {14, 6};
    other.population = 5;
    for (const Hex& h : s.grid.within(other.pos, 1)) {
        s.plot(h).owner = 0;
        s.plot(h).city = 99;
    }
    auto g = Game::fromScenario(rules(), s);
    CityReport r = g->cityReport(99);
    CHECK_EQ(r.amenitiesNeeded, 2);
    CHECK_EQ(r.amenities, 0);
    CHECK_EQ(rules().happiness[static_cast<size_t>(r.happiness)].id, std::string("HAPPINESS_DISPLEASED"));
    // Debt costs amenities: -25 gold is 3 below the line.
    GameState s2 = g->state();
    s2.players[0].gold = Fixed::fromInt(-25);
    auto g2 = Game::fromScenario(rules(), s2);
    CHECK_EQ(g2->cityReport(sc.city).amenities, 2 - 3);
}

TEST(city_modifier_from_building_reaches_plots) {
    GameState s = flatState(20, 14, 1);
    s.plot({7, 6}).resource = rules().resource("RESOURCE_RICE");
    auto sc = capitalScenario(std::move(s));
    GameState st = sc.game->state();
    const City& before = *st.city(sc.city);
    const Fixed riceFood = sc.game->plotYields({7, 6}, before)[F];
    st.cities[0].buildings.push_back(rules().building("BUILDING_WATER_MILL"));
    std::sort(st.cities[0].buildings.begin(), st.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), st);
    const City& after = *g->state().city(sc.city);
    CHECK_EQ(g->plotYields({7, 6}, after)[F], riceFood + Fixed::fromInt(1));
    CHECK_EQ(g->plotYields({5, 6}, after)[F], Fixed::fromInt(2));  // plain grassland unaffected
}

// A building's plot modifiers reach the city's plots of the feature or terrain they name, and no others:
// Chichen Itza on rainforest, the Lighthouse on Coast (03).
TEST(city_modifiers_reach_plots_by_feature_and_terrain) {
    GameState s = flatState(20, 14, 1);
    s.plot({7, 6}).feature = rules().feature("FEATURE_JUNGLE");
    s.plot({5, 6}).terrain = rules().terrain("TERRAIN_COAST");
    auto sc = capitalScenario(std::move(s));
    GameState st = sc.game->state();
    const City& before = *st.city(sc.city);
    const Yields jungle = sc.game->plotYields({7, 6}, before), coast = sc.game->plotYields({5, 6}, before);
    const Yields grass = sc.game->plotYields({6, 7}, before);
    for (const char* b : {"BUILDING_CHICHEN_ITZA", "BUILDING_LIGHTHOUSE"}) st.cities[0].buildings.push_back(rules().building(b));
    std::sort(st.cities[0].buildings.begin(), st.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), st);
    const City& after = *g->state().city(sc.city);
    CHECK_EQ(g->plotYields({7, 6}, after)[C], jungle[C] + Fixed::fromInt(2));
    CHECK_EQ(g->plotYields({7, 6}, after)[P], jungle[P] + Fixed::fromInt(1));
    CHECK_EQ(g->plotYields({7, 6}, after)[F], jungle[F]);
    CHECK_EQ(g->plotYields({5, 6}, after)[F], coast[F] + Fixed::fromInt(1));
    CHECK_EQ(g->plotYields({5, 6}, after)[C], coast[C]);
    CHECK(g->plotYields({6, 7}, after) == grass);
}

TEST(city_bankruptcy_disbands_units) {
    auto sc = capitalScenario();
    GameState s = sc.game->state();
    s.players[0].gold = Fixed::fromInt(-20);
    UnitId spear = addUnit(s, "UNIT_SPEARMAN", 0, {8, 8});  // 1 gold upkeep
    s.cities[0].queue.push_back(buildingItem("BUILDING_MONUMENT"));
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->submit(Command::setActivity(0, spear, Activity::Fortify)) == CommandError::Ok);
    endTurns(*g, 1);
    // Income +5 - 1 upkeep leaves -16, below the -10 line.
    CHECK(g->state().unit(spear) == nullptr);
}

TEST(exclusive_buildings_and_either_prerequisite) {
    // 03: a city holds the Barracks or the Stable, never both, and the Armory needs either; one Government Plaza
    // building a tier, and the next tier after any one of the last; one power plant. A civ's unique building
    // counts as the one it replaces (Rome's Forum as the Market a Bank needs).
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    sovtest::addCity(s, 0, {4, 5}, true, 8);
    for (const char* t : {"TECH_BRONZE_WORKING", "TECH_HORSEBACK_RIDING", "TECH_MILITARY_ENGINEERING", "TECH_INDUSTRIALIZATION", "TECH_ELECTRICITY",
                          "TECH_BANKING"})
        s.players[0].techs.done[static_cast<size_t>(rules().tech(t))] = 1;
    s.cities[0].districts.push_back({rules().district("DISTRICT_ENCAMPMENT"), {6, 5}, true});
    s.cities[0].districts.push_back({rules().district("DISTRICT_GOVERNMENT_PLAZA"), {3, 3}, true});
    s.cities[0].districts.push_back({rules().district("DISTRICT_INDUSTRIAL_ZONE"), {5, 7}, true});
    s.cities[0].districts.push_back({rules().district("DISTRICT_COMMERCIAL_HUB"), {3, 7}, true});
    const auto can = [&](std::vector<const char*> buildings, const char* b) {
        GameState t = s;
        for (const char* x : buildings) t.cities[0].buildings.push_back(rules().building(x));
        std::sort(t.cities[0].buildings.begin(), t.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(t));
        return g->canProduce(g->state().cities[0], buildingItem(b));
    };
    CHECK(can({}, "BUILDING_BARRACKS"));
    CHECK(can({}, "BUILDING_STABLE"));
    CHECK(!can({}, "BUILDING_ARMORY"));
    CHECK(!can({"BUILDING_BARRACKS"}, "BUILDING_STABLE"));
    CHECK(!can({"BUILDING_STABLE"}, "BUILDING_BARRACKS"));
    CHECK(can({"BUILDING_BARRACKS"}, "BUILDING_ARMORY"));
    CHECK(can({"BUILDING_STABLE"}, "BUILDING_ARMORY"));
    CHECK(can({}, "BUILDING_AUDIENCE_CHAMBER"));
    CHECK(!can({}, "BUILDING_FOREIGN_MINISTRY"));
    CHECK(!can({"BUILDING_ANCESTRAL_HALL"}, "BUILDING_AUDIENCE_CHAMBER"));
    CHECK(!can({"BUILDING_ANCESTRAL_HALL"}, "BUILDING_WARLORD_S_THRONE"));
    CHECK(can({"BUILDING_ANCESTRAL_HALL"}, "BUILDING_FOREIGN_MINISTRY"));
    CHECK(can({"BUILDING_ANCESTRAL_HALL"}, "BUILDING_GRAND_MASTER_S_CHAPEL"));
    CHECK(!can({"BUILDING_ANCESTRAL_HALL", "BUILDING_INTELLIGENCE_AGENCY"}, "BUILDING_FOREIGN_MINISTRY"));
    CHECK(can({"BUILDING_ANCESTRAL_HALL", "BUILDING_INTELLIGENCE_AGENCY"}, "BUILDING_WAR_DEPARTMENT"));
    CHECK(!can({"BUILDING_ANCESTRAL_HALL", "BUILDING_INTELLIGENCE_AGENCY", "BUILDING_WAR_DEPARTMENT"}, "BUILDING_ROYAL_SOCIETY"));
    CHECK(can({"BUILDING_WORKSHOP", "BUILDING_FACTORY"}, "BUILDING_COAL_POWER_PLANT"));
    CHECK(can({"BUILDING_WORKSHOP", "BUILDING_FACTORY"}, "BUILDING_OIL_POWER_PLANT"));
    CHECK(!can({"BUILDING_WORKSHOP", "BUILDING_FACTORY", "BUILDING_COAL_POWER_PLANT"}, "BUILDING_OIL_POWER_PLANT"));
    CHECK(!can({}, "BUILDING_BANK"));
    CHECK(can({"BUILDING_MARKET"}, "BUILDING_BANK"));
    CHECK(can({"BUILDING_FORUM"}, "BUILDING_BANK"));
}

TEST(regional_buildings_reach_the_owners_cities_in_range) {
    // A Factory (regional 6) in one city: a second city 4 plots away takes its +3 Production; one 10 away does not.
    GameState s = flatState(30, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const CityId a = sovtest::addCity(s, 0, {5, 5}, true, 3);
    const CityId near = sovtest::addCity(s, 0, {9, 5}, false, 3);
    const CityId far = sovtest::addCity(s, 0, {15, 5}, false, 3);
    auto plain = Game::fromScenario(rules(), s);
    City& host = *s.city(a);
    host.districts.push_back({rules().district("DISTRICT_INDUSTRIAL_ZONE"), {5, 6}, true});
    host.buildings.push_back(rules().building("BUILDING_WORKSHOP"));
    host.buildings.push_back(rules().building("BUILDING_FACTORY"));
    std::sort(host.buildings.begin(), host.buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(rules().buildings[static_cast<size_t>(rules().building("BUILDING_FACTORY"))].regionalRange == 6);
    const size_t prod = static_cast<size_t>(YieldType::Production);
    CHECK(g->cityReport(near).yields[prod] > plain->cityReport(near).yields[prod]);
    CHECK_EQ(g->cityReport(far).yields[prod], plain->cityReport(far).yields[prod]);
}
