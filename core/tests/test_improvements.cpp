// Builders, improvements, harvests, luxuries and strategic resources.
#include <algorithm>

#include "helpers.h"

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
    CHECK_EQ(r.improvements.size(), 24u);  // 17 from the Civ tables, 4 Military Engineer ones, 3 civ uniques
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
    REQUIRE(g->submit(Command::harvest(0, b)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({7, 6}).feature, kNone);
    CHECK_EQ(g->state().cities[0].overflow, overflow + Fixed::fromInt(20));
    CHECK_EQ(g->state().unit(b)->charges, 2);

    GameState s = g->state();
    s.unit(b)->pos = {5, 6};
    s.unit(b)->movesLeft = Fixed::fromInt(2);
    auto g2 = Game::fromScenario(rules(), s);
    const Fixed food = g2->state().cities[0].food;
    REQUIRE(g2->submit(Command::harvest(0, b)) == CommandError::Ok);
    CHECK_EQ(g2->state().plot({5, 6}).resource, kNone);
    CHECK_EQ(g2->state().cities[0].food, food + Fixed::fromInt(20));
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
