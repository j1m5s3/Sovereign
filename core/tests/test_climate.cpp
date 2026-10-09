// Climate change and natural disasters (09 [GS]): CO2 from fuel, climate phases and the sea
// rising over coastal lowlands, disasters striking (damage and fertility), droughts, the favor
// cost of emissions, the world turn's rolls, saves.
#include <algorithm>
#include <functional>

#include "helpers.h"
#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::claimFor;
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
    CHECK_EQ(r.disasters.size(), 26u);  // 18 natural disasters (meteor showers too), 3 nuclear accidents, 5 natural-wonder eruptions
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

TEST(lake_shores_are_not_coastal_lowlands) {
    GameState s = coastState(12, 24, 1);
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1};
    for (int y = 0; y < 24 && low.x < 0; ++y) {
        if (probe->lowlandBand({1, y}) > 0) low = {1, y};
    }
    REQUIRE(low.x == 1);
    // The same plot beside a lake instead of the sea: the rising sea never reaches it (01: Lake).
    GameState lake = flatState(12, 24, 1);
    lake.plot({0, low.y}).terrain = rules().terrain("TERRAIN_COAST");
    auto g = Game::fromScenario(rules(), std::move(lake));
    CHECK_EQ(g->lowlandBand(low), 0);
}

TEST(climate_phases_warm_the_world_and_the_sea_takes_lowlands) {
    GameState s = coastState(12, 24, 1);
    // Find a 1 m and a 2 m lowland in the first land column.
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1}, flooded{-1, -1};
    for (int y = 0; y < 24; ++y) {
        if (low.x < 0 && probe->lowlandBand({1, y}) == 1) low = {1, y};
        if (flooded.x < 0 && probe->lowlandBand({1, y}) == 2) flooded = {1, y};
    }
    REQUIRE(low.x == 1);
    REQUIRE(flooded.x == 1);
    s.plot(low).improvement = rules().improvement("IMPROVEMENT_FARM");
    s.plot(flooded).improvement = rules().improvement("IMPROVEMENT_FARM");
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
    // The 2 m band flooded in phase III: its farm is pillaged until a Builder repairs it.
    CHECK(g->state().plot(flooded).improvement == rules().improvement("IMPROVEMENT_FARM"));
    CHECK(g->state().plot(flooded).pillagedTurns == kPillagedUntilRepaired);
}

// The sea taking a lowland between it and a lake joins the lake to the sea (01: Lake), and the game sees the lake
// gone at once: every plot's appeal (+1 beside a lake) is what a game made afresh from the drowned map finds.
TEST(the_sea_taking_a_lowland_can_join_a_lake_to_it) {
    GameState s = coastState(12, 24, 1);
    auto probe = Game::fromScenario(rules(), s);
    Hex low{-1, -1};
    for (int y = 1; y < 23 && low.x < 0; ++y) {
        if (probe->lowlandBand({1, y}) == 1) low = {1, y};
    }
    REQUIRE(low.x == 1);
    const Hex lake{2, low.y};
    s.plot(lake).terrain = rules().terrain("TERRAIN_COAST");
    s.co2 = 3500000;  // phases I to IV: the 1 m band drowns in phase IV
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->lowlandBand(low) == 1);
    REQUIRE(isLake(g->state(), rules(), lake));
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().plot(low).terrain == rules().terrain("TERRAIN_COAST"));
    CHECK(!isLake(g->state(), rules(), lake));
    auto fresh = Game::fromScenario(rules(), g->state());
    bool same = true;
    for (int i = 0; i < g->state().grid.size(); ++i) same = same && g->plotAppeal(g->state().grid.at(i)) == fresh->plotAppeal(g->state().grid.at(i));
    CHECK(same);
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

// A pillaged district's buildings stand idle (03: Pillage): a Factory there draws no Power, a power plant there powers
// no city and burns nothing, and the Hydroelectric Dam of a pillaged Dam gives none.
TEST(pillaged_districts_draw_and_give_no_power) {
    const TypeIndex coal = rules().resource("RESOURCE_COAL");
    const auto play = [&](bool pillageCapital, bool pillageTown) {
        GameState s = coastState(24, 12, 1);
        addCity(s, 0, {6, 6}, true, 8);
        addCity(s, 0, {11, 6}, false, 6);  // 5 away: within the plant's reach
        const Hex zone[] = {{6, 7}, {11, 7}};
        for (size_t i = 0; i < 2; ++i) {
            CityDistrict d{rules().district("DISTRICT_INDUSTRIAL_ZONE"), zone[i], true};
            d.pillagedTurns = (i == 0 ? pillageCapital : pillageTown) ? 3 : 0;
            s.cities[i].districts.push_back(d);
            s.cities[i].buildings.push_back(rules().building("BUILDING_FACTORY"));
        }
        s.cities[0].buildings.push_back(rules().building("BUILDING_COAL_POWER_PLANT"));
        for (City& c : s.cities) std::sort(c.buildings.begin(), c.buildings.end());
        s.players[0].stockpile[at(coal)] = 10;
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 1);
        return g;
    };
    auto working = play(false, false);
    CHECK_EQ(working->state().cities[1].powerDemand, 2);
    CHECK(working->state().cities[1].powerSupply >= 2);
    CHECK_EQ(working->state().players[0].stockpile[at(coal)], 8);
    auto idleTown = play(false, true);
    CHECK_EQ(idleTown->state().cities[1].powerDemand, 0);
    CHECK_EQ(idleTown->state().players[0].stockpile[at(coal)], 9);  // the capital's Factory only
    auto idlePlant = play(true, false);
    CHECK_EQ(idlePlant->state().cities[0].powerDemand, 0);
    CHECK_EQ(idlePlant->state().cities[1].powerSupply, 0);
    CHECK_EQ(idlePlant->state().players[0].stockpile[at(coal)], 10);
    const auto dam = [](bool pillaged) {
        GameState s = coastState(24, 12, 1);
        addCity(s, 0, {6, 6}, true, 8);
        CityDistrict d{rules().district("DISTRICT_DAM"), {7, 6}, true};
        d.pillagedTurns = pillaged ? 3 : 0;
        s.cities[0].districts.push_back(d);
        s.cities[0].buildings.push_back(rules().building("BUILDING_FOOD_MARKET"));  // needs 1
        s.cities[0].buildings.push_back(rules().building("BUILDING_HYDROELECTRIC_DAM"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 1);
        return g->state().cities[0].powerSupply;
    };
    CHECK(dam(false) > 0);
    CHECK_EQ(dam(true), 0);
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

// The Biosphère (03): renewable Power +200% in its holder's cities, and as much tourism.
TEST(the_biosphere_triples_renewable_power_and_draws_tourism_from_it) {
    const auto play = [](bool biosphere, bool farm) {
        GameState s = coastState(24, 12, 1);
        addCity(s, 0, {6, 6}, true, 8);
        s.cities[0].buildings.push_back(rules().building("BUILDING_FOOD_MARKET"));  // needs 1
        if (biosphere) s.cities[0].buildings.push_back(rules().building("BUILDING_BIOSPH_RE"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        if (farm) s.plot({7, 6}).improvement = rules().improvement("IMPROVEMENT_SOLAR_FARM");
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 1);
        return g;
    };
    CHECK_EQ(play(true, true)->state().cities[0].powerSupply, 6);
    CHECK_EQ(play(true, true)->tourismPerTurn(0) - play(true, false)->tourismPerTurn(0), 6);
    CHECK_EQ(play(false, true)->tourismPerTurn(0) - play(false, false)->tourismPerTurn(0), 0);
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

// It leaves a meteor site, a goody hut of its own, where one may stand (data: improvements, Meteor Site: open land, bare
// or with woods, rainforest or marsh) and nothing else is.
TEST(a_meteor_shower_leaves_a_meteor_site) {
    const auto site = [](const std::function<void(GameState&)>& setUp) {
        GameState s = coastState(16, 12, 1);
        addCity(s, 0, {8, 6}, true, 3);
        setUp(s);
        auto g = Game::fromScenario(rules(), std::move(s));
        g->strikeDisaster(disaster("DISASTER_METEOR_SHOWER"), {10, 6});
        const Plot& p = g->state().plot({10, 6});
        CHECK_EQ(p.village, p.meteorSite);
        CHECK(!g->state().plot({11, 6}).meteorSite);
        return p.meteorSite;
    };
    const auto feature = [](const char* id) { return [id](GameState& s) { s.plot({10, 6}).feature = rules().feature(id); }; };
    CHECK(site([](GameState&) {}));
    CHECK(site(feature("FEATURE_FOREST")));
    CHECK(site(feature("FEATURE_JUNGLE")));
    CHECK(site(feature("FEATURE_MARSH")));
    CHECK(site([](GameState& s) { s.plot({10, 6}).terrain = rules().terrain("TERRAIN_TUNDRA_HILLS"); }));
    CHECK(!site(feature("FEATURE_FLOODPLAINS")));
    CHECK(!site([](GameState& s) { s.plot({10, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN"); }));
    CHECK(!site([](GameState& s) { s.plot({10, 6}).improvement = rules().improvement("IMPROVEMENT_FARM"); }));  // pillaged instead
    CHECK(!site([](GameState& s) { addUnit(s, "UNIT_WARRIOR", 0, {10, 6}); }));
    CHECK(!site([](GameState& s) { s.plot({10, 6}).terrain = rules().terrain("TERRAIN_COAST"); }));
    CHECK(!site([](GameState& s) { addCity(s, 0, {10, 6}, false); }));
    CHECK(!site([](GameState& s) {
        claimFor(s, s.cities[0], {10, 6});
        CityDistrict campus;
        campus.type = rules().district("DISTRICT_CAMPUS");
        campus.pos = {10, 6};
        campus.complete = true;
        s.cities[0].districts.push_back(campus);
    }));
    CHECK(!site([](GameState& s) {
        claimFor(s, s.cities[0], {10, 6});
        s.cities[0].wonders.push_back({rules().building("BUILDING_PYRAMIDS"), {10, 6}});
    }));
    CHECK(!site([](GameState& s) { s.plot({10, 6}).antiquity = 1; }));
    CHECK(!site([](GameState& s) { s.plot({10, 6}).park = true; }));
    CHECK(!site([](GameState& s) {
        Camp camp;
        camp.id = s.nextCampId++;
        camp.pos = {10, 6};
        camp.tribe = 0;
        s.camps.push_back(camp);
    }));
    // A tribal village there stays one.
    GameState s = coastState(16, 12, 1);
    addCity(s, 0, {8, 6}, true, 3);
    s.plot({10, 6}).village = true;
    auto g = Game::fromScenario(rules(), std::move(s));
    g->strikeDisaster(disaster("DISASTER_METEOR_SHOWER"), {10, 6});
    CHECK(g->state().plot({10, 6}).village);
    CHECK(!g->state().plot({10, 6}).meteorSite);
    // Other disasters leave none.
    GameState t = coastState(16, 12, 1);
    addCity(t, 0, {8, 6}, true, 3);
    auto storm = Game::fromScenario(rules(), std::move(t));
    storm->strikeDisaster(disaster("DISASTER_TORNADO_OUTBREAK"), {10, 6});
    CHECK(!storm->state().plot({10, 6}).village);
}

TEST(disaster_damage_waits_for_a_builder) {
    // A plot a disaster pillaged stays so until a Builder repairs it, as in war (05: Pillage).
    GameState s = coastState(16, 12, 1);
    addCity(s, 0, {8, 6}, true, 3);
    s.plot({9, 6}).improvement = rules().improvement("IMPROVEMENT_FARM");
    auto g = Game::fromScenario(rules(), std::move(s));
    g->strikeDisaster(disaster("DISASTER_METEOR_SHOWER"), {9, 6});
    REQUIRE(g->state().plot({9, 6}).pillagedTurns > 0);
    const City& c = g->state().cities[0];
    const Yields pillaged = g->plotYields({9, 6}, c);
    sovtest::endTurns(*g, 8);
    CHECK(g->state().plot({9, 6}).pillagedTurns == kPillagedUntilRepaired);
    GameState t = g->state();
    const UnitId builder = addUnit(t, "UNIT_BUILDER", 0, {9, 6});
    auto h = Game::fromScenario(rules(), std::move(t));
    REQUIRE(h->submit(Command::repairImprovement(0, builder)) == CommandError::Ok);
    CHECK_EQ(h->state().plot({9, 6}).pillagedTurns, 0);
    CHECK(h->plotYields({9, 6}, h->state().cities[0])[static_cast<size_t>(YieldType::Food)] > pillaged[static_cast<size_t>(YieldType::Food)]);
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

TEST(volcano_natural_wonders_erupt_with_their_own_numbers) {
    // The data lists each volcano natural wonder's eruptions apart from any volcano's (09 [GS]).
    const Rules& r = rules();
    const auto loss = [&](const DisasterType& d) {
        for (const DisasterDamage& dd : d.damage) {
            if (dd.type == DisasterDamageType::PopulationLoss) return dd.percent;
        }
        return -1;
    };
    const auto culture = [&](const DisasterType& d) {
        return std::any_of(d.fertility.begin(), d.fertility.end(), [](const DisasterFertility& f) { return f.yield == YieldType::Culture; });
    };
    const DisasterType& any = r.disasters[at(disaster("DISASTER_MEGACOLOSSAL_ERUPTION"))];
    const DisasterType& vesuvius = r.disasters[at(disaster("DISASTER_MEGACOLOSSAL_ERUPTION_MOUNT_VESUVIUS"))];
    CHECK(any.naturalWonder == kNone);
    CHECK_EQ(any.frequencyTenths[0], 5);  // 0.5 a game at Minimal
    CHECK_EQ(loss(any), 35);
    CHECK(!culture(any));
    CHECK(vesuvius.naturalWonder == r.feature("FEATURE_MOUNT_VESUVIUS"));
    CHECK_EQ(vesuvius.frequencyTenths[0], 30);
    CHECK_EQ(loss(vesuvius), 100);
    CHECK(culture(vesuvius));
    CHECK_EQ(loss(r.disasters[at(disaster("DISASTER_CATASTROPHIC_ERUPTION"))]), 20);
    CHECK_EQ(loss(r.disasters[at(disaster("DISASTER_CATASTROPHIC_ERUPTION_EYJAFJALLAJOKULL"))]), 30);
    CHECK(disaster("DISASTER_GENTLE_ERUPTION_MOUNT_KILIMANJARO") != kNone);

    // Mount Vesuvius erupts on its own, with no volcano on the map; Kilimanjaro, not on the map, never does.
    GameState s = coastState(24, 16, 1);
    const Hex mountain{12, 8};
    s.plot(mountain).feature = r.feature("FEATURE_MOUNT_VESUVIUS");
    s.setup.disasterIntensity = 4;
    s.setup.turnLimit = 10;
    s.setup.scoreVictory = false;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 6);
    int fromVesuvius = 0, fromKilimanjaro = 0;
    for (const GameEvent& e : g->state().events) {
        if (e.kind != EventKind::Disaster) continue;
        fromVesuvius += e.value == disaster("DISASTER_MEGACOLOSSAL_ERUPTION_MOUNT_VESUVIUS") ? 1 : 0;
        fromKilimanjaro += e.value == disaster("DISASTER_GENTLE_ERUPTION_MOUNT_KILIMANJARO") ? 1 : 0;
    }
    CHECK(fromVesuvius > 0);
    CHECK_EQ(fromKilimanjaro, 0);
    int soil = 0;
    for (const Hex& h : g->state().grid.within(mountain, 1)) soil += g->state().plot(h).feature == r.feature("FEATURE_VOLCANIC_SOIL") ? 1 : 0;
    CHECK(soil > 0);
    CHECK(g->state().plot(mountain).feature == r.feature("FEATURE_MOUNT_VESUVIUS"));
}
