// Builders, improvements, harvests, luxuries and strategic resources.
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addUnit;
using sovtest::capitalScenario;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
constexpr size_t F = static_cast<size_t>(YieldType::Food);
constexpr size_t P = static_cast<size_t>(YieldType::Production);

size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex improvement(const char* id) { return rules().improvement(id); }
TypeIndex tech(const char* id) { return rules().tech(id); }

// A capital at (6,6) with research and production running, a Builder on
// (7,6) and `edit` applied to the state before play resumes.
template <typename Edit>
std::unique_ptr<Game> builderGame(Edit edit, GameState base = flatState(20, 14, 1)) {
    auto sc = capitalScenario(std::move(base));
    GameState s = sc.game->state();
    s.cities[0].queue = {{ProductionKind::Building, rules().building("BUILDING_MONUMENT")}};
    s.players[0].techs.current = tech("TECH_ASTROLOGY");
    s.players[0].civics.current = rules().civic("CIVIC_CODE_OF_LAWS");
    addUnit(s, "UNIT_BUILDER", 0, {7, 6});
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}

UnitId builderOf(const Game& g) {
    for (const Unit& u : g.state().units) {
        if (u.type == rules().unit("UNIT_BUILDER")) return u.id;
    }
    return kNoUnit;
}

void know(GameState& s, const char* id) { s.players[0].techs.done[at(tech(id))] = 1; }
}  // namespace

TEST(improvement_data_from_civ_tables) {
    const Rules& r = rules();
    CHECK_EQ(r.improvements.size(), 35u);  // 17 from the Civ tables, 4 Military Engineer ones, 9 city-states', 3 civ uniques, 2 a governor opens
    const ImprovementType& farm = r.improvements[at(improvement("IMPROVEMENT_FARM"))];
    CHECK(farm.unlock.none());
    CHECK_EQ(farm.yields[F], Fixed::fromInt(1));
    CHECK_EQ(farm.housing, Fixed::ratio(1, 2));
    REQUIRE(farm.adjacency.size() == 2u);
    CHECK_EQ(farm.adjacency[0].per, 2);
    CHECK_EQ(farm.adjacency[0].needs.index, r.civic("CIVIC_FEUDALISM"));
    const ImprovementType& mine = r.improvements[at(improvement("IMPROVEMENT_MINE"))];
    CHECK_EQ(mine.unlock.index, tech("TECH_MINING"));
    CHECK_EQ(mine.bonuses.size(), 3u);
    const UnitType& sword = r.units[at(r.unit("UNIT_SWORDSMAN"))];
    CHECK_EQ(sword.strategicResource, r.resource("RESOURCE_IRON"));
    CHECK_EQ(sword.strategicCost, 20);
    const UnitType& warrior = r.units[at(r.unit("UNIT_WARRIOR"))];
    CHECK_EQ(warrior.upgradesTo, r.unit("UNIT_SWORDSMAN"));
    CHECK_EQ(warrior.obsoleteWith.index, tech("TECH_GUNPOWDER"));
    const FeatureType& woods = r.features[at(r.feature("FEATURE_FOREST"))];
    CHECK_EQ(woods.removeTech.index, tech("TECH_MINING"));
    CHECK_EQ(woods.harvest[P], Fixed::fromInt(20));
    const ResourceType& wheat = r.resources[at(r.resource("RESOURCE_WHEAT"))];
    CHECK_EQ(wheat.harvestTech.index, tech("TECH_POTTERY"));
    CHECK_EQ(wheat.harvest[F], Fixed::fromInt(20));
    const ResourceType& iron = r.resources[at(r.resource("RESOURCE_IRON"))];
    CHECK_EQ(iron.accumulation, 2);
    CHECK_EQ(iron.stockpileCap, 50);
    CHECK_EQ(r.resources[at(r.resource("RESOURCE_WINE"))].amenityCities, 4);
}

TEST(builder_builds_farm_with_charges) {
    auto g = builderGame([](GameState&) {});
    const UnitId b = builderOf(*g);
    const City& c = g->state().cities[0];
    const Yields before = g->plotYields({7, 6}, c);
    const Fixed housing = g->cityReport(c.id).housing;
    const std::vector<TypeIndex> options = g->improvementsAt(0, {7, 6});
    CHECK(std::find(options.begin(), options.end(), improvement("IMPROVEMENT_FARM")) != options.end());
    CHECK_EQ(g->submit(Command::buildImprovement(0, b, improvement("IMPROVEMENT_MINE"))), CommandError::CannotImprove);
    REQUIRE(g->submit(Command::buildImprovement(0, b, improvement("IMPROVEMENT_FARM"))) == CommandError::Ok);
    CHECK_EQ(g->state().plot({7, 6}).improvement, improvement("IMPROVEMENT_FARM"));
    CHECK_EQ(g->state().unit(b)->charges, 2);
    CHECK_EQ(g->plotYields({7, 6}, g->state().cities[0])[F], before[F] + Fixed::fromInt(1));
    CHECK_EQ(g->cityReport(c.id).housing, housing + Fixed::ratio(1, 2));
    // Building ends the builder's moves; off-territory plots cannot be improved.
    CHECK_EQ(g->submit(Command::buildImprovement(0, b, improvement("IMPROVEMENT_FARM"))), CommandError::CannotImprove);
    CHECK(!g->canImproveAt(0, {12, 6}, improvement("IMPROVEMENT_FARM")));

    // The last charge uses the builder up.
    GameState s = g->state();
    s.unit(b)->charges = 1;
    s.unit(b)->movesLeft = Fixed::fromInt(2);
    s.unit(b)->pos = {5, 6};
    auto g2 = Game::fromScenario(rules(), s);
    REQUIRE(g2->submit(Command::buildImprovement(0, b, improvement("IMPROVEMENT_FARM"))) == CommandError::Ok);
    CHECK(g2->state().unit(b) == nullptr);
}

TEST(improvements_go_only_on_the_players_own_open_land) {
    // A Farm fits the capital's grassland, but not its city center, a district, a wonder or another civ's land.
    GameState s = flatState(20, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    sovtest::addCity(s, 0, {6, 6}, true, 3);
    sovtest::addCity(s, 1, {12, 6}, true, 3);
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {7, 6}, true});
    s.cities[0].wonders.push_back({rules().building("BUILDING_PYRAMIDS"), {5, 6}});
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex farm = improvement("IMPROVEMENT_FARM");
    auto takesFarm = [&](Hex h) {
        const std::vector<TypeIndex> listed = g->improvementsAt(0, h);
        const bool fits = std::find(listed.begin(), listed.end(), farm) != listed.end();
        CHECK_EQ(fits, g->canImproveAt(0, h, farm));
        return fits;
    };
    CHECK(takesFarm({6, 5}));
    CHECK(!takesFarm({6, 6}));   // the city center
    CHECK(!takesFarm({7, 6}));   // the Campus
    CHECK(!takesFarm({5, 6}));   // the Pyramids
    CHECK(!takesFarm({12, 5}));  // player 1's land
}

TEST(farm_adjacency_after_feudalism) {
    auto g = builderGame([](GameState& s) {
        for (Hex h : {Hex{7, 6}, Hex{7, 7}, Hex{6, 7}}) s.plot(h).improvement = improvement("IMPROVEMENT_FARM");
    });
    // (7,6), (7,7) and (6,7) touch each other: without Feudalism each farm gives +1 Food.
    CHECK_EQ(g->improvementYields({7, 6}, 0)[F], Fixed::fromInt(1));
    auto g2 = builderGame([](GameState& s) {
        for (Hex h : {Hex{7, 6}, Hex{7, 7}, Hex{6, 7}}) s.plot(h).improvement = improvement("IMPROVEMENT_FARM");
        s.players[0].civics.done[at(rules().civic("CIVIC_FEUDALISM"))] = 1;
    });
    CHECK_EQ(g2->improvementYields({7, 6}, 0)[F], Fixed::fromInt(2));  // +1 per 2 adjacent farms
}

TEST(resources_take_only_their_improvement_and_fire_boosts) {
    GameState base = flatState(20, 14, 1);
    base.plot({7, 6}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    base.plot({7, 6}).resource = rules().resource("RESOURCE_IRON");
    // Before Bronze Working the iron is hidden: the hill takes a farm or a mine.
    auto hidden = builderGame([](GameState& s) { know(s, "TECH_MINING"); }, base);
    CHECK(hidden->canImproveAt(0, {7, 6}, improvement("IMPROVEMENT_FARM")));
    CHECK(hidden->canImproveAt(0, {7, 6}, improvement("IMPROVEMENT_MINE")));

    auto g = builderGame([](GameState& s) {
        know(s, "TECH_MINING");
        know(s, "TECH_BRONZE_WORKING");
    }, base);
    CHECK(!g->canImproveAt(0, {7, 6}, improvement("IMPROVEMENT_FARM")));
    REQUIRE(g->submit(Command::buildImprovement(0, builderOf(*g), improvement("IMPROVEMENT_MINE"))) == CommandError::Ok);
    const Player& p = g->state().players[0];
    CHECK_EQ(p.techs.boosted[at(tech("TECH_WHEEL"))], 1);         // a Mine on a resource
    CHECK_EQ(p.techs.boosted[at(tech("TECH_IRON_WORKING"))], 1);  // improve Iron
    CHECK_EQ(p.techs.boosted[at(tech("TECH_MASONRY"))], 0);
    // Improved iron fills the stockpile each turn.
    endTurns(*g, 1);
    CHECK_EQ(g->state().players[0].stockpile[at(rules().resource("RESOURCE_IRON"))], 2);
}

TEST(strategic_cost_and_obsolete_units) {
    auto g = builderGame([](GameState& s) {
        know(s, "TECH_MINING");
        know(s, "TECH_BRONZE_WORKING");
        know(s, "TECH_IRON_WORKING");
        s.players[0].stockpile[at(rules().resource("RESOURCE_IRON"))] = 10;
    });
    const CityId city = g->state().cities[0].id;
    const ProductionItem sword{ProductionKind::Unit, rules().unit("UNIT_SWORDSMAN")};
    const ProductionItem warrior{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")};
    CHECK_EQ(g->submit(Command::setProduction(0, city, sword)), CommandError::NotEnoughResources);
    CHECK(g->canProduce(g->state().cities[0], warrior));  // no iron: Warriors still train

    GameState s = g->state();
    s.players[0].stockpile[at(rules().resource("RESOURCE_IRON"))] = 25;
    s.cities[0].progress = {{sword, Fixed::fromInt(89)}};
    auto g2 = Game::fromScenario(rules(), s);
    CHECK(!g2->canProduce(g2->state().cities[0], warrior));  // the Swordsman replaces it
    REQUIRE(g2->submit(Command::setProduction(0, city, sword)) == CommandError::Ok);
    REQUIRE(g2->submit(Command::setActivity(0, builderOf(*g2), Activity::Sleep)) == CommandError::Ok);
    endTurns(*g2, 1);
    int swords = 0;
    for (const Unit& u : g2->state().units) swords += u.type == sword.type;
    CHECK_EQ(swords, 1);
    CHECK_EQ(g2->state().players[0].stockpile[at(rules().resource("RESOURCE_IRON"))], 5);
}

TEST(luxury_gives_amenities) {
    GameState base = flatState(20, 14, 1);
    base.plot({7, 6}).terrain = rules().terrain("TERRAIN_PLAINS");
    base.plot({7, 6}).resource = rules().resource("RESOURCE_WINE");
    auto g = builderGame([](GameState& s) {
        know(s, "TECH_POTTERY");
        know(s, "TECH_IRRIGATION");
    }, base);
    const CityId city = g->state().cities[0].id;
    const int before = g->cityReport(city).amenities;
    REQUIRE(g->submit(Command::buildImprovement(0, builderOf(*g), improvement("IMPROVEMENT_PLANTATION"))) ==
            CommandError::Ok);
    CHECK_EQ(g->cityReport(city).amenities, before + 1);
}

TEST(builder_harvests_woods_and_bonus_resources) {
    GameState base = flatState(20, 14, 1);
    base.plot({7, 6}).feature = rules().feature("FEATURE_FOREST");
    base.plot({5, 6}).resource = rules().resource("RESOURCE_WHEAT");
    base.plot({5, 6}).terrain = rules().terrain("TERRAIN_PLAINS");
    auto locked = builderGame([](GameState&) {}, base);
    CHECK_EQ(locked->submit(Command::harvest(0, builderOf(*locked))), CommandError::CannotHarvest);  // needs Mining

    auto g = builderGame([](GameState& s) {
        know(s, "TECH_MINING");
        know(s, "TECH_POTTERY");
    }, base);
    const UnitId b = builderOf(*g);
    const Fixed overflow = g->state().cities[0].overflow;
    // 20 at the start, scaled up with the share of the tech tree known (two techs here).
    const int progress = static_cast<int>(2 * 100 / static_cast<int64_t>(rules().techs.size()));
    const auto scaled = [&](int base) { return Fixed::fromInt(base) * (100 + 9 * progress) / 100; };
    REQUIRE(g->submit(Command::harvest(0, b)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({7, 6}).feature, kNone);
    CHECK(progress > 0);
    CHECK_EQ(g->state().cities[0].overflow, overflow + scaled(20));
    CHECK_EQ(g->state().unit(b)->charges, 2);

    GameState s = g->state();
    s.unit(b)->pos = {5, 6};
    s.unit(b)->movesLeft = Fixed::fromInt(2);
    auto g2 = Game::fromScenario(rules(), s);
    const Fixed food = g2->state().cities[0].food;
    REQUIRE(g2->submit(Command::harvest(0, b)) == CommandError::Ok);
    CHECK_EQ(g2->state().plot({5, 6}).resource, kNone);
    CHECK_EQ(g2->state().cities[0].food, food + scaled(20));
}

// ---- Military Engineers [GS]: railroads and Mountain Tunnels (01: Routes, Mountain tunnels)

namespace {
// Player 0's capital at (4,6) with a Military Engineer at (5,6), Steam Power and Chemistry, and
// Iron and Coal in stock; a mountain at (6,6) in its land.
GameState engineerState() {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    for (const char* t : {"TECH_STEAM_POWER", "TECH_CHEMISTRY"}) s.players[0].techs.done[at(rules().tech(t))] = 1;
    for (const char* r : {"RESOURCE_IRON", "RESOURCE_COAL"}) s.players[0].stockpile[at(rules().resource(r))] = 3;
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    s.plot({6, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    s.plot({6, 6}).owner = 0;
    s.plot({6, 6}).city = s.cities[0].id;
    sovtest::addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {5, 6});
    return s;
}
}  // namespace

TEST(military_engineers_lay_railroads) {
    auto g = Game::fromScenario(rules(), engineerState());
    const UnitId eng = g->state().units.front().id;
    const TypeIndex rr = g->railroad();
    REQUIRE(rr != kNone);
    REQUIRE(g->submit(Command::buildRailroad(0, eng)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({5, 6}).route, rr);
    CHECK_EQ(g->state().players[0].stockpile[at(rules().resource("RESOURCE_IRON"))], 2);
    CHECK_EQ(g->state().players[0].stockpile[at(rules().resource("RESOURCE_COAL"))], 2);
    CHECK(g->submit(Command::buildRailroad(0, eng)) != CommandError::Ok);  // its turn is spent, and the plot has one
    // Track to track costs a quarter of a move.
    GameState s = g->state();
    s.plot({5, 7}).route = static_cast<int8_t>(rr);
    auto h = Game::fromScenario(rules(), std::move(s));
    const std::optional<Fixed> cost = h->moveCost(h->state().units.front(), {5, 6}, {5, 7});
    REQUIRE(cost.has_value());
    CHECK_EQ(*cost, Fixed::ratio(1, 4));
    // No Coal, no railroad; Builders never lay one; trade routes never place one.
    GameState t = engineerState();
    t.players[0].stockpile[at(rules().resource("RESOURCE_COAL"))] = 0;
    auto k = Game::fromScenario(rules(), std::move(t));
    CHECK(k->submit(Command::buildRailroad(0, k->state().units.front().id)) == CommandError::NotEnoughResources);
    CHECK(k->roadFor(0) != rr);
}

TEST(a_mountain_tunnel_opens_the_mountain) {
    auto g = Game::fromScenario(rules(), engineerState());
    const UnitId eng = g->state().units.front().id;
    CHECK(!g->moveCost(g->state().units.front(), {5, 6}, {6, 6}).has_value());  // impassable
    const std::vector<Hex> sites = g->tunnelSites(0, eng);
    REQUIRE(std::find(sites.begin(), sites.end(), Hex{6, 6}) != sites.end());
    const TypeIndex tunnel = rules().improvement("IMPROVEMENT_MOUNTAIN_TUNNEL");
    REQUIRE(g->submit(Command::buildTunnel(0, eng, tunnel, {6, 6})) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).improvement, tunnel);
    CHECK_EQ(g->state().plot({5, 6}).improvement, kNone);
    const std::optional<Fixed> through = g->moveCost(g->state().units.front(), {5, 6}, {6, 6});
    REQUIRE(through.has_value());
    CHECK_EQ(*through, Fixed::fromInt(1));
}

// ---- pillage and repair (05: Pillage)

TEST(pillaging_takes_plunder_and_a_builder_repairs) {
    // Player 1's farm and mine beside its city; player 0's Warrior on the mine, at war.
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    sovtest::addCity(s, 1, {10, 6}, true, 4);
    s.plot({11, 6}).improvement = improvement("IMPROVEMENT_MINE");
    s.plot({10, 7}).improvement = improvement("IMPROVEMENT_FARM");
    const UnitId raider = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {11, 6});
    auto g = Game::fromScenario(rules(), s);
    const Fixed gold = g->state().players[0].gold;
    REQUIRE(g->submit(Command::pillage(0, raider)) == CommandError::Ok);
    CHECK(g->state().plot({11, 6}).pillagedTurns > 0);
    CHECK(g->state().players[0].gold == gold + Fixed::fromInt(50));  // a mine: 50 Gold
    CHECK(g->submit(Command::pillage(0, raider)) != CommandError::Ok);  // already pillaged, and no moves to spare
    // A pillaged improvement yields nothing until repaired; a Builder repairs it without a charge.
    const City& c = *g->state().cityAt({10, 6});
    const Yields bare = g->plotYields({11, 6}, c);
    GameState t = g->state();
    t.units.clear();
    const UnitId b = sovtest::addUnit(t, "UNIT_BUILDER", 1, {11, 6});
    t.currentPlayer = 1;
    auto h = Game::fromScenario(rules(), std::move(t));
    sovtest::endTurns(*h, 1);
    REQUIRE(h->state().currentPlayer == 1);
    const int charges = h->state().unit(b)->charges;
    REQUIRE(h->submit(Command::repairImprovement(1, b)) == CommandError::Ok);
    CHECK_EQ(h->state().plot({11, 6}).pillagedTurns, 0);
    CHECK_EQ(h->state().unit(b)->charges, charges);
    CHECK(h->plotYields({11, 6}, *h->state().cityAt({10, 6}))[static_cast<size_t>(YieldType::Production)] >
          bare[static_cast<size_t>(YieldType::Production)]);
    // Not in one's own land, nor at peace.
    s.players[0].relations[1].war = s.players[1].relations[0].war = false;
    auto peace = Game::fromScenario(rules(), std::move(s));
    CHECK(peace->submit(Command::pillage(0, raider)) == CommandError::BadTarget);
}

TEST(a_pillaged_district_idles_then_recovers) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    sovtest::addCity(s, 1, {10, 6}, true, 4);
    CityDistrict campus;
    campus.type = rules().district("DISTRICT_CAMPUS");
    campus.pos = {11, 6};
    campus.complete = true;
    s.cities[0].districts.push_back(campus);
    s.plot({12, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");  // adjacency for the Campus
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    const UnitId raider = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {11, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId cid = g->state().cities[0].id;
    const Fixed science = g->cityReport(cid).yields[static_cast<size_t>(YieldType::Science)];
    REQUIRE(g->submit(Command::pillage(0, raider)) == CommandError::Ok);
    CHECK(g->state().city(cid)->districts[0].pillagedTurns > 0);
    CHECK(g->cityReport(cid).yields[static_cast<size_t>(YieldType::Science)] < science);
}

TEST(ships_raid_the_coast) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    for (int y = 0; y < 12; ++y) s.plot({12, y}).terrain = rules().terrain("TERRAIN_COAST");
    sovtest::addCity(s, 1, {10, 6}, true, 4);
    s.plot({11, 6}).improvement = improvement("IMPROVEMENT_FARM");
    const UnitId galley = sovtest::addUnit(s, "UNIT_GALLEY", 0, {12, 6});
    const UnitId warrior = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {9, 9});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->coastalRaidProblem(0, warrior, {9, 8}) == CommandError::BadUnit);  // land units pillage where they stand
    CHECK(g->coastalRaidProblem(0, galley, {10, 6}) == CommandError::BadTarget);  // not adjacent (and a city)
    REQUIRE(g->submit(Command::coastalRaid(0, galley, {11, 6})) == CommandError::Ok);
    CHECK(g->state().plot({11, 6}).pillagedTurns > 0);
}

TEST(encampment_buildings_raise_the_stockpile_cap) {
    GameState s = sovtest::flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    sovtest::addCity(s, 0, {6, 6}, true, 3);
    const TypeIndex iron = rules().resource("RESOURCE_IRON");
    auto plain = Game::fromScenario(rules(), s);
    CHECK_EQ(plain->stockpileCap(0, iron), 50);
    s.cities[0].buildings = {rules().building("BUILDING_BARRACKS"), rules().building("BUILDING_ARMORY")};
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->stockpileCap(0, iron), 70);
}

TEST(francis_drake_raises_plunder) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    for (int y = 0; y < 12; ++y) s.plot({12, y}).terrain = rules().terrain("TERRAIN_COAST");
    sovtest::addCity(s, 1, {10, 6}, true, 4);
    s.plot({11, 6}).improvement = improvement("IMPROVEMENT_MINE");
    const UnitId galley = sovtest::addUnit(s, "UNIT_GALLEY", 0, {12, 6});
    s.players[0].greatPeopleActivated.push_back(rules().greatPerson("GREAT_PERSON_FRANCIS_DRAKE"));
    auto g = Game::fromScenario(rules(), std::move(s));
    const Fixed gold = g->state().players[0].gold;
    REQUIRE(g->submit(Command::coastalRaid(0, galley, {11, 6})) == CommandError::Ok);
    CHECK(g->state().players[0].gold == gold + Fixed::fromInt(75));  // a mine's 50 Gold, +50%
}

TEST(pillaging_a_road_slows_it_until_repaired) {
    // Player 1's road on (12,6)-(13,6), no improvements there; player 0's Warrior on it, at war.
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    sovtest::addCity(s, 1, {10, 6}, true, 4);
    for (const Hex h : {Hex{12, 6}, Hex{13, 6}}) {
        s.plot(h).route = 0;  // the Ancient Road
        s.plot(h).owner = 1;
    }
    s.plot({13, 6}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");  // 2 moves off the road
    const UnitId raider = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {12, 6});
    auto g = Game::fromScenario(rules(), s);
    const Fixed along = *g->moveCost(*g->state().unit(raider), {12, 6}, {13, 6});
    const Fixed gold = g->state().players[0].gold;
    REQUIRE(g->submit(Command::pillage(0, raider)) == CommandError::Ok);
    CHECK(g->state().plot({12, 6}).routePillaged);
    CHECK(g->state().players[0].gold == gold);  // a road gives no plunder
    CHECK(*g->moveCost(*g->state().unit(raider), {12, 6}, {13, 6}) > along);
    CHECK_EQ(g->pillageProblem(0, raider), CommandError::BadUnit);  // the 3 moves are spent
    // Saved, then the owner's Builder repairs it without a charge.
    std::string err;
    auto h = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(h);
    CHECK(h->state().plot({12, 6}).routePillaged);
    GameState t = h->state();
    t.units.clear();
    const UnitId b = sovtest::addUnit(t, "UNIT_BUILDER", 1, {12, 6});
    t.currentPlayer = 1;
    auto r = Game::fromScenario(rules(), std::move(t));
    sovtest::endTurns(*r, 1);
    REQUIRE(r->state().currentPlayer == 1);
    const int charges = r->state().unit(b)->charges;
    REQUIRE(r->submit(Command::repairImprovement(1, b)) == CommandError::Ok);
    CHECK(!r->state().plot({12, 6}).routePillaged);
    CHECK_EQ(r->state().unit(b)->charges, charges);
    // Depredation: pillaging costs 1 movement.
    s.units.clear();
    s.plot({12, 6}).routePillaged = false;
    const UnitId horse = sovtest::addUnit(s, "UNIT_HORSEMAN", 0, {12, 6});
    s.units.back().promotions.push_back(rules().promotion("PROMOTION_DEPREDATION"));
    auto d = Game::fromScenario(rules(), std::move(s));
    const Fixed moves = d->state().unit(horse)->movesLeft;
    REQUIRE(d->submit(Command::pillage(0, horse)) == CommandError::Ok);
    CHECK(d->state().unit(horse)->movesLeft == moves - Fixed::fromInt(1));
}

TEST(military_engineers_speed_an_aqueduct) {
    GameState s = engineerState();
    CityDistrict aqueduct;
    aqueduct.type = rules().district("DISTRICT_AQUEDUCT");
    aqueduct.pos = {5, 6};
    s.cities[0].districts.push_back(aqueduct);  // placed, not finished
    s.plot({5, 6}).owner = 0;
    s.plot({5, 6}).city = s.cities[0].id;
    auto g = Game::fromScenario(rules(), std::move(s));
    const UnitId eng = g->state().units.front().id;
    const int charges = g->state().unit(eng)->charges;
    REQUIRE(g->chargeProblem(0, eng) == CommandError::Ok);
    REQUIRE(g->submit(Command::contributeCharge(0, eng)) == CommandError::Ok);
    const ProductionItem item{ProductionKind::District, rules().district("DISTRICT_AQUEDUCT")};
    const City& c = g->state().cities[0];
    auto it = std::find_if(c.progress.begin(), c.progress.end(), [&](const ProductionProgress& pp) { return pp.item == item; });
    REQUIRE(it != c.progress.end());
    CHECK(it->amount == Fixed::fromInt(g->productionCost(0, item) * 20 / 100));
    if (charges > 1) CHECK_EQ(g->state().unit(eng)->charges, charges - 1);
    // Not on a finished district, nor with a Builder.
    GameState t = engineerState();
    aqueduct.complete = true;
    t.cities[0].districts.push_back(aqueduct);
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK(h->chargeProblem(0, h->state().units.front().id) != CommandError::Ok);
}

TEST(a_city_states_improvement_for_its_suzerain) {
    // La Venta's Colossal Head: +2 Faith, +1 per two neighbouring woods; only while we hold its suzerain bonus.
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(2, 0);
        p.relations.resize(2);
    }
    s.players[1].civ = kNone;
    s.players[1].cityState = rules().cityState("CITYSTATE_LA_VENTA");
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    sovtest::addCity(s, 1, {14, 6}, true, 2);
    s.plot({6, 6}).owner = 0;
    s.plot({6, 6}).city = s.cities[0].id;
    s.plot({7, 6}).feature = s.plot({6, 7}).feature = rules().feature("FEATURE_FOREST");
    const TypeIndex head = improvement("IMPROVEMENT_COLOSSAL_HEAD");
    REQUIRE(head != kNone);
    {
        auto g = Game::fromScenario(rules(), s);
        CHECK(!g->canImproveAt(0, {6, 6}, head));  // not its suzerain
    }
    s.players[0].envoys[1] = 3;
    auto g = Game::fromScenario(rules(), s);
    CHECK(g->canImproveAt(0, {6, 6}, head));
    GameState t = s;
    t.plot({6, 6}).improvement = head;
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->improvementYields({6, 6}, 0)[static_cast<size_t>(YieldType::Faith)], Fixed::fromInt(3));  // 2, and 1 for two woods
}

TEST(city_state_improvements_add_their_extras) {
    // Player 0's capital at (4,6) owns the land around (6,6), where each improvement goes (08: City-States).
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    for (const Hex& h : s.grid.within({4, 6}, 3)) {
        s.plot(h).owner = 0;
        s.plot(h).city = s.cities[0].id;
    }
    const size_t C = static_cast<size_t>(YieldType::Culture), Fa = static_cast<size_t>(YieldType::Faith);
    const auto with = [](GameState t, Hex h, const char* id) {
        t.plot(h).improvement = improvement(id);
        return t;
    };
    // Rapa Nui's Moai: +1 Culture beside the coast, +2 on or beside Volcanic Soil.
    const auto moaiCulture = [&](GameState t) { return Game::fromScenario(rules(), with(std::move(t), {6, 6}, "IMPROVEMENT_MOAI"))->improvementYields({6, 6}, 0)[C]; };
    CHECK_EQ(moaiCulture(s), Fixed::fromInt(1));
    GameState t = s;
    t.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
    CHECK_EQ(moaiCulture(t), Fixed::fromInt(2));
    t = s;
    t.plot({7, 6}).feature = rules().feature("FEATURE_VOLCANIC_SOIL");
    CHECK_EQ(moaiCulture(t), Fixed::fromInt(3));
    t = s;
    t.plot({6, 6}).feature = rules().feature("FEATURE_VOLCANIC_SOIL");
    CHECK_EQ(moaiCulture(t), Fixed::fromInt(3));
    // Cahokia Mounds: +1 Amenity to its city.
    {
        auto plain = Game::fromScenario(rules(), s);
        auto mounds = Game::fromScenario(rules(), with(s, {6, 6}, "IMPROVEMENT_CAHOKIA_MOUNDS"));
        CHECK_EQ(mounds->cityReport(s.cities[0].id).amenities, plain->cityReport(s.cities[0].id).amenities + 1);
    }
    // Nazca's Nazca Line: its owner's plots beside it gain +1 Faith (+1 more with a resource), +1 Food on desert after
    // Civil Service and +1 Production off the hills after Mass Production; not its own plot, nor plots of no one.
    t = s;
    t.plot({7, 6}).terrain = rules().terrain("TERRAIN_DESERT");
    t.plot({6, 7}).terrain = rules().terrain("TERRAIN_DESERT_HILLS");
    t.plot({5, 6}).resource = rules().resource("RESOURCE_WHEAT");
    t.plot({6, 5}).owner = kNoPlayer;
    t.plot({6, 5}).city = kNoCity;
    const auto gain = [&](const GameState& base, Hex h) {
        auto plain = Game::fromScenario(rules(), base);
        auto line = Game::fromScenario(rules(), with(base, {6, 6}, "IMPROVEMENT_NAZCA_LINE"));
        const Yields a = plain->plotYields(h, plain->state().cities[0]), b = line->plotYields(h, line->state().cities[0]);
        Yields d{};
        for (size_t i = 0; i < kNumYields; ++i) d[i] = b[i] - a[i];
        return d;
    };
    for (size_t i = 0; i < kNumYields; ++i) CHECK_EQ(gain(t, {7, 6})[i], i == Fa ? Fixed::fromInt(1) : Fixed());  // Faith only, for now
    CHECK_EQ(gain(t, {5, 6})[Fa], Fixed::fromInt(2));
    CHECK_EQ(gain(t, {6, 6})[Fa], Fixed());
    CHECK_EQ(gain(t, {6, 5})[Fa], Fixed());
    t.players[0].civics.done[at(rules().civic("CIVIC_CIVIL_SERVICE"))] = 1;
    t.players[0].techs.done[at(tech("TECH_MASS_PRODUCTION"))] = 1;
    CHECK_EQ(gain(t, {7, 6})[F], Fixed::fromInt(1));
    CHECK_EQ(gain(t, {7, 6})[P], Fixed::fromInt(1));
    CHECK_EQ(gain(t, {6, 7})[F], Fixed::fromInt(1));
    CHECK_EQ(gain(t, {6, 7})[P], Fixed());  // hills
    CHECK_EQ(gain(t, {5, 6})[F], Fixed());  // grassland
    GameState u = with(t, {6, 6}, "IMPROVEMENT_NAZCA_LINE");
    u.plot({6, 6}).pillagedTurns = 3;
    auto pillaged = Game::fromScenario(rules(), std::move(u));
    auto plain = Game::fromScenario(rules(), t);
    CHECK(pillaged->plotYields({7, 6}, pillaged->state().cities[0]) == plain->plotYields({7, 6}, plain->state().cities[0]));  // nothing while pillaged
}

TEST(nalandas_first_mahavihara_grants_a_technology) {
    GameState base = flatState(20, 14, 2);
    for (Player& p : base.players) {
        Game::fitPlayerToRules(p, rules());
        p.envoys.assign(2, 0);
        p.relations.resize(2);
    }
    base.players[1].civ = kNone;
    base.players[1].cityState = rules().cityState("CITYSTATE_NALANDA");
    sovtest::addCity(base, 1, {16, 6}, true, 2);
    base.players[0].envoys[1] = 3;  // its suzerain
    auto g = builderGame([](GameState& s) { addUnit(s, "UNIT_BUILDER", 0, {5, 6}); }, base);
    std::vector<UnitId> builders;
    for (const Unit& u : g->state().units) {
        if (u.owner == 0 && u.type == rules().unit("UNIT_BUILDER")) builders.push_back(u.id);
    }
    REQUIRE(builders.size() == 2u);
    const auto techs = [&] {
        const std::vector<uint8_t>& d = g->state().players[0].techs.done;
        return std::count(d.begin(), d.end(), uint8_t{1});
    };
    const auto before = techs();
    const TypeIndex vihara = improvement("IMPROVEMENT_MAHAVIHARA");
    REQUIRE(g->submit(Command::buildImprovement(0, builders[0], vihara)) == CommandError::Ok);
    CHECK_EQ(techs(), before + 1);
    REQUIRE(g->submit(Command::buildImprovement(0, builders[1], vihara)) == CommandError::Ok);
    CHECK_EQ(techs(), before + 1);  // the first one only
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->state().players[0].improvementGrants == std::vector<TypeIndex>{vihara});
}

TEST(military_engineers_lay_roads_until_railroads) {
    GameState s = flatState(18, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(2);
    }
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    sovtest::addCity(s, 1, {13, 6}, true, 4);
    for (const Hex& h : {Hex{6, 6}, Hex{6, 8}}) {
        s.plot(h).owner = 0;
        s.plot(h).city = s.cities[0].id;
    }
    s.plot({6, 8}).route = 0;  // an Ancient Road already
    const UnitId eng = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {6, 6});
    const UnitId wild = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {9, 2});    // no one's land
    const UnitId abroad = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {13, 7}); // a neighbour's land, at peace
    const UnitId paved = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {6, 8});
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {4, 6});
    const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {9, 4});
    s.plot({13, 7}).owner = 1;
    for (Unit& u : s.units) {
        if (u.id == wild) u.charges = 1;
    }
    auto g = Game::fromScenario(rules(), s);
    const TypeIndex road = g->roadFor(0);
    REQUIRE(road != kNone);
    REQUIRE(g->submit(Command::buildRoad(0, eng)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).route, road);
    CHECK_EQ(g->state().unit(eng)->charges, 1);  // a charge each
    CHECK(g->state().unit(eng)->movesLeft == Fixed());
    REQUIRE(g->submit(Command::buildRoad(0, wild)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({9, 2}).route, road);
    CHECK(g->state().unit(wild) == nullptr);  // its last charge
    CHECK(g->submit(Command::buildRoad(0, abroad)) == CommandError::CannotImprove);
    CHECK(g->submit(Command::buildRoad(0, paved)) == CommandError::CannotImprove);
    CHECK(g->submit(Command::buildRoad(0, builder)) == CommandError::CannotImprove);
    CHECK(g->submit(Command::buildRoad(0, warrior)) == CommandError::CannotImprove);
    // A pillaged road is laid again.
    s.plot({6, 8}).routePillaged = true;
    auto p = Game::fromScenario(rules(), s);
    CHECK(p->submit(Command::buildRoad(0, paved)) == CommandError::Ok);
    CHECK(!p->state().plot({6, 8}).routePillaged);
    // With Steam Power railroads replace its roads [GS].
    s.players[0].techs.done[at(rules().tech("TECH_STEAM_POWER"))] = 1;
    auto rail = Game::fromScenario(rules(), std::move(s));
    CHECK(rail->submit(Command::buildRoad(0, eng)) == CommandError::CannotImprove);
}

TEST(builder_charges_and_harvests_are_for_builders) {
    GameState s = flatState(16, 12, 1);
    s.players[0].civ = rules().civ("CIVILIZATION_CHINA");
    Game::fitPlayerToRules(s.players[0], rules());
    s.players[0].techs.done[at(rules().tech("TECH_MINING"))] = 1;
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    s.plot({6, 6}).owner = 0;
    s.plot({6, 6}).city = s.cities[0].id;
    s.plot({6, 6}).feature = rules().feature("FEATURE_FOREST");
    const UnitId woodsman = addUnit(s, "UNIT_BUILDER", 0, {6, 6});
    const UnitId engineer = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {6, 6});
    s.plot({6, 8}).owner = 0;
    s.plot({6, 8}).city = s.cities[0].id;
    const UnitId farmer = addUnit(s, "UNIT_BUILDER", 0, {6, 8});
    const UnitId digger = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {6, 8});
    const UnitId sapper = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {6, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    // Only a Builder builds a farm (an Archaeologist's charges are digs).
    const TypeIndex farm = rules().improvement("IMPROVEMENT_FARM");
    CHECK(g->validate(Command::buildImprovement(0, farmer, farm)) == CommandError::Ok);
    CHECK(g->validate(Command::buildImprovement(0, digger, farm)) == CommandError::CannotImprove);
    CHECK(g->validate(Command::buildImprovement(0, sapper, farm)) == CommandError::CannotImprove);
    // Qin's +1 charge goes to Builders, not to Military Engineers.
    City& city = g->stateMutForTests().cities[0];
    REQUIRE(g->completeItem(city, {ProductionKind::Unit, rules().unit("UNIT_BUILDER")}));
    REQUIRE(g->completeItem(g->stateMutForTests().cities[0], {ProductionKind::Unit, rules().unit("UNIT_MILITARY_ENGINEER")}));
    int builderCharges = -1, engineerCharges = -1;
    for (const Unit& u : g->state().units) {
        if (u.id == woodsman || u.id == engineer || u.id == farmer || u.id == sapper) continue;
        if (u.type == rules().unit("UNIT_BUILDER")) builderCharges = u.charges;
        if (u.type == rules().unit("UNIT_MILITARY_ENGINEER")) engineerCharges = u.charges;
    }
    CHECK_EQ(builderCharges, rules().units[at(rules().unit("UNIT_BUILDER"))].buildCharges + 1);
    CHECK_EQ(engineerCharges, rules().units[at(rules().unit("UNIT_MILITARY_ENGINEER"))].buildCharges);
    // Only a Builder harvests the woods.
    CHECK(g->validate(Command::harvest(0, engineer)) == CommandError::CannotHarvest);
    CHECK(g->validate(Command::harvest(0, woodsman)) == CommandError::Ok);
}

TEST(seasteads_count_reefs_and_fisheries_count_sea_resources) {
    // An improvement on the coast at (8,6), beside the coast at (9,6) and (8,5) and the land at (7,6).
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    sovtest::addCity(s, 0, {4, 6}, true, 4);
    for (Hex h : {Hex{8, 6}, Hex{9, 6}, Hex{8, 5}}) s.plot(h).terrain = rules().terrain("TERRAIN_COAST");
    const auto yieldsOf = [](GameState t, const char* id) {
        t.plot({8, 6}).improvement = improvement(id);
        return Game::fromScenario(rules(), std::move(t))->improvementYields({8, 6}, 0);
    };
    // The Seastead: +1 Culture per adjacent Reef.
    const size_t C = static_cast<size_t>(YieldType::Culture);
    CHECK_EQ(yieldsOf(s, "IMPROVEMENT_SEASTEAD")[C], Fixed::fromInt(0));
    GameState reefs = s;
    reefs.plot({9, 6}).feature = rules().feature("FEATURE_REEF");
    reefs.plot({8, 5}).feature = rules().feature("FEATURE_REEF");
    CHECK_EQ(yieldsOf(reefs, "IMPROVEMENT_SEASTEAD")[C], Fixed::fromInt(2));
    // The Fishery: 1 Food, +1 per adjacent resource on water its owner can see; one on land doesn't count.
    CHECK_EQ(yieldsOf(s, "IMPROVEMENT_FISHERY")[F], Fixed::fromInt(1));
    GameState sea = s;
    sea.plot({9, 6}).resource = rules().resource("RESOURCE_FISH");
    sea.plot({8, 5}).resource = rules().resource("RESOURCE_OIL");  // hidden until Refining
    sea.plot({7, 6}).resource = rules().resource("RESOURCE_WHEAT");
    CHECK_EQ(yieldsOf(sea, "IMPROVEMENT_FISHERY")[F], Fixed::fromInt(2));
    know(sea, "TECH_REFINING");
    CHECK_EQ(yieldsOf(sea, "IMPROVEMENT_FISHERY")[F], Fixed::fromInt(3));
}
