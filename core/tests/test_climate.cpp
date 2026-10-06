// Climate change and natural disasters (09 [GS]): CO2 from fuel, climate phases and the sea
// rising over coastal lowlands, disasters striking (damage and fertility), droughts, the favor
// cost of emissions, the world turn's rolls, saves.
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

TypeIndex disaster(const char* id) {
    for (size_t i = 0; i < rules().disasters.size(); ++i) {
        if (rules().disasters[i].id == id) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

int countEvents(const Game& g, EventKind kind) {
    int n = 0;
    for (const GameEvent& e : g.state().events) n += e.kind == kind ? 1 : 0;
    return n;
}

// Ocean down the west edge so the first land column is coastal.
GameState coastState(int w, int h, int players) {
    GameState s = flatState(w, h, players);
    for (int y = 0; y < h; ++y) s.plot({0, y}).terrain = rules().terrain("TERRAIN_COAST");
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    return s;
}
}  // namespace

TEST(climate_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.disasters.size(), 21u);  // 18 natural disasters (meteor showers too) and 3 nuclear accidents
    CHECK_EQ(r.climatePhases.size(), 7u);
    CHECK_EQ(r.disasterIntensities.size(), 5u);
    CHECK_EQ(r.climatePhases[6].points, 8);
    CHECK_EQ(r.disasterIntensities[2].activeVolcanoes, 70);
    CHECK_EQ(r.mapSizes[at(r.mapSize("MAPSIZE_DUEL"))].co2PerDegree, 500000);
    const DisasterType& flood = r.disasters[at(disaster("DISASTER_MAJOR_FLOOD"))];
    CHECK(flood.kind == DisasterKind::Flood);
    CHECK_EQ(flood.frequencyTenths[2], 15);
    CHECK(!flood.damage.empty());
    CHECK(!flood.fertility.empty());
    CHECK(r.disasters[at(disaster("DISASTER_MAJOR_DROUGHT"))].damage.size() == 1u);  // the farm-specific pillage
}

TEST(power_plants_burn_fuel_and_emissions_cost_favor) {
    // (Power [GS]: the plant burns only what its region needs.)
    GameState s = coastState(20, 12, 2);
    for (Player& p : s.players) {
        p.met.assign(2, 1);
        p.grievances.assign(2, 0);
    }
    addCity(s, 0, {6, 6}, true, 5);
    addCity(s, 1, {14, 6}, true, 5);
    s.majorsAtStart = 2;
    City& c = s.cities[0];
    c.buildings.push_back(rules().building("BUILDING_COAL_POWER_PLANT"));
    c.buildings.push_back(rules().building("BUILDING_FACTORY"));  // needs 2 power: one Coal (4 power) burns
    std::sort(c.buildings.begin(), c.buildings.end());
    const TypeIndex coal = rules().resource("RESOURCE_COAL");
    s.players[0].stockpile[at(coal)] = 5;
    auto g = Game::fromScenario(rules(), std::move(s));
    const int before = g->favorPerTurn(0);
    sovtest::endTurns(*g, 2);  // player 1's turn, then player 0's: one ton of coal burned
    CHECK_EQ(g->state().players[0].stockpile[at(coal)], 4);
    CHECK_EQ(g->state().players[0].co2, 820);
    CHECK_EQ(g->state().co2, 820);
    // All of the world's CO2: 100% / 3, held to -20.
    CHECK_EQ(g->favorPerTurn(0), before - 20);
    GameState s2 = g->state();
    s2.players[1].co2 = 820 * 3;  // now 25% of it
    s2.co2 = 820 * 4;
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK_EQ(g2->favorPerTurn(0), before - 8);
}

TEST(climate_phases_warm_the_world_and_the_sea_takes_lowlands) {
    GameState s = coastState(12, 24, 1);
    // Find a 1 m lowland in the first land column.
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1};
    for (int y = 0; y < 24 && low.x < 0; ++y) {
        if (probe->lowlandBand({1, y}) == 1) low = {1, y};
    }
    REQUIRE(low.x == 1);
    s.plot(low).improvement = rules().improvement("IMPROVEMENT_FARM");
    addUnit(s, "UNIT_WARRIOR", 0, low);
    s.units.back().activity = Activity::Sleep;
    s.co2 = 3500000;  // 14 points on a Duel map: phases I to IV
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->climateChangePoints(), 14);
    CHECK_EQ(g->temperatureTenths(), 70);
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().climatePhase, 4);
    CHECK_EQ(countEvents(*g, EventKind::ClimatePhase), 4);
    const Plot& p = g->state().plot(low);
    CHECK(p.terrain == rules().terrain("TERRAIN_COAST"));  // drowned in phase IV
    CHECK(p.improvement == kNone);
    bool unitLeft = false;
    for (const Unit& u : g->state().units) unitLeft |= u.pos == low;
    CHECK(!unitLeft);
}

TEST(a_flood_barrier_holds_back_the_sea) {
    GameState s = coastState(12, 24, 1);
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1};
    for (int y = 1; y < 23 && low.x < 0; ++y) {
        if (probe->lowlandBand({1, y}) == 1) low = {1, y};
    }
    REQUIRE(low.x == 1);
    addCity(s, 0, {2, low.y}, true, 3);
    s.cities[0].buildings.push_back(rules().building("BUILDING_FLOOD_BARRIER"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    REQUIRE(s.plot(low).city == s.cities[0].id);
    s.co2 = 3500000;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().climatePhase, 4);
    CHECK(g->state().plot(low).terrain == rules().terrain("TERRAIN_GRASS"));
}

TEST(an_eruption_wrecks_and_enriches_the_land_around_it) {
    GameState s = coastState(16, 12, 1);
    const Hex volcano{8, 6};
    s.plot(volcano).feature = rules().feature("FEATURE_VOLCANO");
    addCity(s, 0, {9, 6}, true, 8);
    for (const Hex& h : s.grid.within(volcano, 1)) {
        if (h != volcano && h != Hex{9, 6}) s.plot(h).improvement = rules().improvement("IMPROVEMENT_FARM");
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    g->stateMutForTests().setup.disasterIntensity = 2;
    g->strikeDisaster(disaster("DISASTER_MEGACOLOSSAL_ERUPTION"), volcano);
    CHECK_EQ(countEvents(*g, EventKind::Disaster), 1);
    CHECK(g->state().cities[0].population < 8);  // population loss in the city beside it
    int fertile = 0, soil = 0, hurt = 0;
    for (const Hex& h : g->state().grid.within(volcano, 1)) {
        const Plot& p = g->state().plot(h);
        for (int8_t f : p.fertility) fertile += f > 0 ? 1 : 0;
        soil += p.feature == rules().feature("FEATURE_VOLCANIC_SOIL") ? 1 : 0;
        hurt += p.improvement == kNone || p.pillagedTurns > 0 ? 1 : 0;
    }
    CHECK(fertile > 0);
    CHECK(soil > 0);
    CHECK(hurt > 2);
    CHECK(g->state().plot(volcano).feature == rules().feature("FEATURE_VOLCANO"));
}

TEST(a_drought_parches_the_land_until_it_passes) {
    GameState s = coastState(16, 12, 1);
    addCity(s, 0, {8, 6}, true, 3);
    auto g = Game::fromScenario(rules(), std::move(s));
    const Hex plot{9, 6};
    const City& c = g->state().cities[0];
    const Fixed food = g->plotYields(plot, c)[static_cast<size_t>(YieldType::Food)];
    const TypeIndex d = disaster("DISASTER_MAJOR_DROUGHT");
    g->strikeDisaster(d, {8, 6});
    REQUIRE(g->inDrought(plot));
    CHECK(g->plotYields(plot, g->state().cities[0])[static_cast<size_t>(YieldType::Food)] == food - Fixed::fromInt(1));
    sovtest::endTurns(*g, rules().disasters[at(d)].duration);
    CHECK(!g->inDrought(plot));
}

TEST(disasters_strike_on_their_own_at_high_intensity) {
    GameState s = coastState(30, 20, 1);
    for (Plot& p : s.plots) {
        if (p.terrain == rules().terrain("TERRAIN_GRASS")) p.feature = rules().feature("FEATURE_FOREST");
    }
    s.setup.disasterIntensity = 4;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 150);
    CHECK(countEvents(*g, EventKind::Disaster) > 0);
    int burnt = 0;
    for (const Plot& p : g->state().plots) burnt += p.feature == rules().feature("FEATURE_BURNT_FOREST") ? 1 : 0;
    CHECK(burnt > 0);
}

TEST(climate_survives_a_save) {
    GameState s = coastState(16, 12, 1);
    addCity(s, 0, {8, 6}, true, 3);
    s.co2 = 1234567;
    s.players[0].co2 = 1234567;
    s.climatePhase = 2;
    s.plot({9, 6}).fertility[1] = 2;
    s.plot({9, 6}).pillagedTurns = 3;
    s.droughts.push_back({{8, 6}, 1, 4});
    s.setup.disasterIntensity = 3;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().co2, 1234567);
    CHECK_EQ(loaded->state().players[0].co2, 1234567);
    CHECK_EQ(loaded->state().climatePhase, 2);
    CHECK_EQ(loaded->state().plot({9, 6}).fertility[1], 2);
    CHECK_EQ(loaded->state().plot({9, 6}).pillagedTurns, 3);
    CHECK_EQ(loaded->state().droughts.size(), 1u);
    CHECK_EQ(loaded->state().setup.disasterIntensity, 3);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(power_feeds_buildings_and_a_shortfall_costs_production) {
    GameState s = coastState(24, 12, 1);
    addCity(s, 0, {6, 6}, true, 8);
    addCity(s, 0, {11, 6}, false, 6);   // 5 away: within a plant's reach
    for (City& c : s.cities) {
        c.buildings.push_back(rules().building("BUILDING_FACTORY"));
        std::sort(c.buildings.begin(), c.buildings.end());
    }
    auto unpowered = Game::fromScenario(rules(), s);
    sovtest::endTurns(*unpowered, 1);
    const City& u = unpowered->state().cities[1];
    CHECK_EQ(u.powerDemand, 2);
    CHECK_EQ(u.powerSupply, 0);
    const Fixed weak = unpowered->cityReport(u.id).yields[static_cast<size_t>(YieldType::Production)];
    // A Coal plant in the capital powers both cities; the Factory's +3 when powered and no penalty.
    s.cities[0].buildings.push_back(rules().building("BUILDING_COAL_POWER_PLANT"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].stockpile[at(rules().resource("RESOURCE_COAL"))] = 10;
    auto powered = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*powered, 1);
    const City& p = powered->state().cities[1];
    CHECK(p.powerSupply >= p.powerDemand);
    CHECK_EQ(powered->state().players[0].stockpile[at(rules().resource("RESOURCE_COAL"))], 8);  // one Coal per city
    CHECK(powered->cityReport(p.id).yields[static_cast<size_t>(YieldType::Production)] > weak);
}

TEST(renewables_give_free_power) {
    GameState s = coastState(24, 12, 1);
    addCity(s, 0, {6, 6}, true, 8);
    s.cities[0].buildings.push_back(rules().building("BUILDING_FOOD_MARKET"));  // needs 1
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.plot({7, 6}).improvement = rules().improvement("IMPROVEMENT_SOLAR_FARM");
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().cities[0].powerSupply, 2);
    CHECK_EQ(g->state().co2, 0);
}

// ---- nuclear accidents (09: Climate and disasters)

TEST(an_old_reactor_can_melt_down_and_recommissioning_renews_it) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    s.turn = 100;
    addCity(s, 0, {6, 6}, true, 6);
    City& c = s.cities[0];
    CityDistrict zone;
    zone.type = rules().district("DISTRICT_INDUSTRIAL_ZONE");
    zone.pos = {7, 6};
    zone.complete = true;
    c.districts.push_back(zone);
    c.buildings = {rules().building("BUILDING_NUCLEAR_POWER_PLANT")};
    c.reactorSince = 60;
    const TypeIndex meltdown = disaster("DISASTER_NUCLEAR_MELTDOWN");
    CHECK_EQ(rules().disasters[at(meltdown)].minTurnAtRisk, 30);
    CHECK_EQ(rules().disasters[at(meltdown)].fallout, 20);
    auto g = Game::fromScenario(rules(), s);
    g->strikeDisaster(meltdown, {7, 6});
    CHECK_EQ(g->state().plot({7, 6}).fallout, 20);
    CHECK_EQ(g->state().plot({9, 6}).fallout, 20);  // two rings out (severity 2)
    // Recommissioning starts the reactor's age over.
    const ProductionItem renew{ProductionKind::Project, rules().project("PROJECT_RECOMMISSION_NUCLEAR_REACTOR")};
    s.players[0].techs.done[at(rules().tech("TECH_NUCLEAR_FISSION"))] = 1;
    auto h = Game::fromScenario(rules(), std::move(s));
    REQUIRE(rules().projects[at(renew.type)].modelled);
    REQUIRE(h->canProduce(h->state().cities[0], renew));
    h->completeProject(h->stateMutForTests().cities[0], renew.type);
    CHECK_EQ(h->state().cities[0].reactorSince, 100);
}

TEST(a_plant_converts_and_synthetic_technocracy_powers_every_city) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    s.turn = 50;
    addCity(s, 0, {6, 6}, true, 6);
    City& c = s.cities[0];
    CityDistrict zone;
    zone.type = rules().district("DISTRICT_INDUSTRIAL_ZONE");
    zone.pos = {7, 6};
    zone.complete = true;
    c.districts.push_back(zone);
    const TypeIndex coal = rules().building("BUILDING_COAL_POWER_PLANT"), nuclear = rules().building("BUILDING_NUCLEAR_POWER_PLANT");
    c.buildings = {coal};
    s.players[0].techs.done[at(rules().tech("TECH_NUCLEAR_FISSION"))] = 1;
    auto g = Game::fromScenario(rules(), s);
    const ProductionItem convert{ProductionKind::Project, rules().project("PROJECT_CONVERT_TO_NUCLEAR_POWER")};
    REQUIRE(rules().projects[at(convert.type)].modelled);
    REQUIRE(g->canProduce(g->state().cities[0], convert));
    g->completeProject(g->stateMutForTests().cities[0], convert.type);
    CHECK(!g->state().cities[0].has(coal));
    CHECK(g->state().cities[0].has(nuclear));
    CHECK_EQ(g->state().cities[0].reactorSince, 50);
    CHECK(!g->canProduce(g->state().cities[0], convert));  // already nuclear
    // Synthetic Technocracy: +3 Power in every city.
    s.players[0].government = rules().government("GOVERNMENT_SYNTHETIC_TECHNOCRACY");
    s.cities[0].buildings.clear();
    s.cities[0].laserStations = 1;  // a Terrestrial Laser Station wants 5 Power
    auto h = Game::fromScenario(rules(), std::move(s));
    REQUIRE(h->governmentIs(0, "GOVERNMENT_SYNTHETIC_TECHNOCRACY"));
    sovtest::endTurns(*h, 1);
    CHECK_EQ(h->state().cities[0].powerSupply, 3);
    CHECK_EQ(h->state().cities[0].powerDemand, 5);
}

TEST(storms_move_on_and_wreck_districts) {
    GameState s = coastState(30, 20, 1);
    addCity(s, 0, {12, 10}, true, 4);
    CityDistrict campus;
    campus.type = rules().district("DISTRICT_CAMPUS");
    campus.pos = {13, 10};
    campus.complete = true;
    s.cities[0].districts.push_back(campus);
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    // A tornado outbreak on the campus: districts pillaged at 75%, buildings at 100% (09).
    g->strikeDisaster(disaster("DISASTER_TORNADO_OUTBREAK"), {13, 10});
    CHECK(g->state().cities[0].districts.back().pillagedTurns > 0);
    REQUIRE(g->state().ongoing.size() == 1u);  // it moves on for two more turns
    const Hex first = g->state().ongoing[0].center;
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().ongoing.size() == 1u);
    CHECK(g->state().grid.distance(g->state().ongoing[0].center, first) == 1);
    sovtest::endTurns(*g, 2);
    CHECK(g->state().ongoing.empty());
    // A meltdown destroys the Industrial Zone's buildings... and the Campus is no Industrial Zone: the Library stays.
    CHECK(g->state().cities[0].has(rules().building("BUILDING_LIBRARY")));
}

TEST(deforestation_scales_co2) {
    GameState s = coastState(20, 12, 1);
    for (Plot& p : s.plots) {
        if (p.terrain == rules().terrain("TERRAIN_GRASS")) p.feature = rules().feature("FEATURE_FOREST");
    }
    s.co2 = 4000000;
    auto g = Game::fromScenario(rules(), s);
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->deforestationPercent(), -20);  // little lost
    GameState cut = g->state();
    int n = 0;
    for (Plot& p : cut.plots) {
        if (p.feature == rules().feature("FEATURE_FOREST") && n++ % 2 == 0) p.feature = kNone;
    }
    auto h = Game::fromScenario(rules(), std::move(cut));
    CHECK_EQ(h->deforestationPercent(), 50);  // half the woods gone
    CHECK(h->climateChangePoints() > g->climateChangePoints());
}

TEST(a_meteor_shower_pillages_its_plot) {
    GameState s = coastState(16, 12, 1);
    addCity(s, 0, {8, 6}, true, 3);
    s.plot({9, 6}).improvement = rules().improvement("IMPROVEMENT_FARM");
    auto g = Game::fromScenario(rules(), std::move(s));
    g->strikeDisaster(disaster("DISASTER_METEOR_SHOWER"), {9, 6});
    CHECK(g->state().plot({9, 6}).pillagedTurns > 0);
    CHECK(g->state().plot({10, 6}).pillagedTurns == 0);  // one plot only
}

TEST(a_flood_barrier_costs_more_with_more_lowland) {
    GameState s = coastState(12, 24, 1);
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1};
    for (int y = 1; y < 23 && low.x < 0; ++y) {
        if (probe->lowlandBand({1, y}) == 1) low = {1, y};
    }
    REQUIRE(low.x == 1);
    addCity(s, 0, {2, low.y}, true, 3);
    addCity(s, 0, {8, 12}, false, 3);  // inland
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem barrier{ProductionKind::Building, rules().building("BUILDING_FLOOD_BARRIER")};
    const int base = g->productionCost(0, barrier);
    CHECK_EQ(g->productionCost(0, barrier, &g->state().cities[1]), base);  // no lowland: the base cost
    int lowland = 0;  // x its lowland plots
    for (const Hex& h : g->state().grid.within(g->state().cities[0].pos, 3)) lowland += g->state().plot(h).city == g->state().cities[0].id && g->lowlandBand(h) > 0 ? 1 : 0;
    REQUIRE(lowland >= 1);
    CHECK_EQ(g->productionCost(0, barrier, &g->state().cities[0]), base * lowland);
}
