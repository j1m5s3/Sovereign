// World wonders (03-districts-buildings-wonders.md, Wonders).
#include <algorithm>

#include "helpers.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex wonder(const char* id) { return rules().building(id); }

void giveTech(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[at(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.techs.done[at(rules().tech(tech))] = 1;
}

// Two players with a city each on desert land; both know Masonry (Pyramids) and Irrigation.
GameState wonderState() {
    GameState s = flatState(24, 14, 2);
    for (Plot& p : s.plots) p.terrain = rules().terrain("TERRAIN_DESERT");
    for (PlayerId p = 0; p < 2; ++p) {
        giveTech(s, p, "TECH_MASONRY");
        giveTech(s, p, "TECH_IRRIGATION");
    }
    const CityId a = addCity(s, 0, {5, 6}, true, 3);
    const CityId b = addCity(s, 1, {15, 6}, true, 3);
    for (const Hex& h : s.grid.within({5, 6}, 3)) s.plot(h).city = a, s.plot(h).owner = 0;
    for (const Hex& h : s.grid.within({15, 6}, 3)) s.plot(h).city = b, s.plot(h).owner = 1;
    return s;
}
}  // namespace

TEST(wonder_rules_data) {
    const Rules& r = rules();
    int wonders = 0;
    for (const BuildingType& b : r.buildings) wonders += b.wonder;
    CHECK_EQ(wonders, 53);
    CHECK(r.buildings[at(wonder("BUILDING_PANAMA_CANAL"))].placement.canal);
    const BuildingType& stonehenge = r.buildings[at(wonder("BUILDING_STONEHENGE"))];
    CHECK_EQ(stonehenge.placement.nextToResource, r.resource("RESOURCE_STONE"));
    CHECK_EQ(stonehenge.placement.terrains.size(), 5u);
    const BuildingType& colossus = r.buildings[at(wonder("BUILDING_COLOSSUS"))];
    CHECK_EQ(colossus.tradeCapacity, 1);
    REQUIRE(colossus.wonderEffects.size() == 1u);
    CHECK_EQ(colossus.wonderEffects[0].ref, r.unit("UNIT_TRADER"));
    CHECK(r.buildings[at(wonder("BUILDING_HANGING_GARDENS"))].placement.river);
}

TEST(the_panama_canal_links_water_and_lets_ships_through) {
    // 03: "as a canal": flat land between two bodies of water, here (3,8) and (5,8), or between water and the City Center.
    const TypeIndex panama = wonder("BUILDING_PANAMA_CANAL");
    GameState s = wonderState();
    giveTech(s, 0, "TECH_STEAM_POWER");
    for (const Hex& h : {Hex{3, 8}, Hex{5, 8}}) s.plot(h).terrain = rules().terrain("TERRAIN_COAST");
    GameState hills = s;
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& c = g->state().cities[0];
    CHECK(g->canPlaceWonder(c, panama, {4, 8}));
    CHECK(g->canPlaceWonder(c, panama, {4, 7}));   // water and the City Center
    CHECK(!g->canPlaceWonder(c, panama, {7, 6}));  // dry land
    hills.plot({4, 8}).terrain = rules().terrain("TERRAIN_DESERT_HILLS");
    auto gh = Game::fromScenario(rules(), std::move(hills));
    CHECK(!gh->canPlaceWonder(gh->state().cities[0], panama, {4, 8}));  // flat land only
    // Finished, it carries a ship across the land; reserved but unfinished, it does not.
    GameState built = g->state();
    built.cities[0].wonders.push_back({panama, {4, 8}});
    for (Player& p : built.players) p.techs.done[at(rules().tech("TECH_SAILING"))] = 1;
    const UnitId ship = sovtest::addUnit(built, "UNIT_GALLEY", 0, {3, 8});
    GameState unfinished = built;
    built.cities[0].buildings.insert(std::lower_bound(built.cities[0].buildings.begin(), built.cities[0].buildings.end(), panama), panama);
    auto g2 = Game::fromScenario(rules(), std::move(built));
    auto path = g2->findPath(ship, {5, 8}, false);
    REQUIRE(path.has_value());
    CHECK(std::any_of(path->begin(), path->end(), [](const PathStep& st) { return st.pos == Hex{4, 8}; }));
    auto g3 = Game::fromScenario(rules(), std::move(unfinished));
    CHECK(!g3->findPath(ship, {5, 8}, false).has_value());
}

TEST(wonders_need_their_ground) {
    GameState s = wonderState();
    setRiver(s, {6, 6}, Dir::E);
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& c = g->state().cities[0];
    const TypeIndex gardens = wonder("BUILDING_HANGING_GARDENS");
    CHECK(g->canPlaceWonder(c, gardens, {6, 6}));   // on the river
    CHECK(!g->canPlaceWonder(c, gardens, {4, 6}));  // dry land
    CHECK(!g->canPlaceWonder(c, gardens, {5, 6}));  // the city itself
    CHECK(g->canPlaceWonder(c, wonder("BUILDING_PYRAMIDS"), {4, 6}));   // desert
    CHECK(!g->canPlaceWonder(c, wonder("BUILDING_PYRAMIDS"), {5, 6}));  // not the city's own plot
    CHECK(g->wonderPlots(c.id, rules().building("BUILDING_LIBRARY")).empty());  // a Library is no wonder
    const TypeIndex apadana = wonder("BUILDING_APADANA");
    CHECK(g->canPlaceWonder(c, apadana, {4, 6}));   // beside the capital
    CHECK(!g->canPlaceWonder(c, apadana, {3, 6}));  // two plots away
}

// A plot holding a district or another wonder, finished or not, takes no wonder.
TEST(wonders_need_a_plot_with_nothing_built) {
    GameState s = wonderState();
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {4, 6}, true});
    s.cities[0].wonders.push_back({wonder("BUILDING_APADANA"), {6, 6}});  // being built
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& c = g->state().cities[0];
    const TypeIndex pyramids = wonder("BUILDING_PYRAMIDS");
    CHECK(g->canPlaceWonder(c, pyramids, {4, 7}));   // open desert
    CHECK(!g->canPlaceWonder(c, pyramids, {4, 6}));  // the Campus
    CHECK(!g->canPlaceWonder(c, pyramids, {6, 6}));  // the Apadana's plot
    // Nor a barbarian camp's.
    GameState camped = wonderState();
    Camp camp;
    camp.id = camped.nextCampId++;
    camp.pos = {4, 7};
    camped.camps.push_back(camp);
    auto g2 = Game::fromScenario(rules(), std::move(camped));
    CHECK(!g2->canPlaceWonder(g2->state().cities[0], pyramids, {4, 7}));
    // Nor a resource the owner can see, or a natural wonder.
    GameState other = wonderState();
    other.plot({4, 7}).resource = rules().resource("RESOURCE_STONE");
    other.plot({6, 6}).feature = rules().feature("FEATURE_EYE_OF_THE_SAHARA");
    auto g3 = Game::fromScenario(rules(), std::move(other));
    CHECK(!g3->canPlaceWonder(g3->state().cities[0], pyramids, {4, 7}));
    CHECK(!g3->canPlaceWonder(g3->state().cities[0], pyramids, {6, 6}));
    CHECK(g3->canPlaceWonder(g3->state().cities[0], pyramids, {4, 6}));
}

TEST(a_wonder_is_built_once_and_rivals_keep_half) {
    GameState s = wonderState();
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex pyramids = wonder("BUILDING_PYRAMIDS");
    const CityId mine = g->state().cities[0].id, theirs = g->state().cities[1].id;
    const ProductionItem item{ProductionKind::Building, pyramids};
    const auto offered = [&](const Game& game, CityId city) {
        const std::vector<ProductionItem> list = game.buildableItems(city);
        return std::find(list.begin(), list.end(), item) != list.end();
    };
    CHECK(offered(*g, theirs));
    REQUIRE(g->submit(Command::setProduction(0, mine, item, {4, 6})) == CommandError::Ok);
    CHECK_EQ(g->state().wonderAt({4, 6}), pyramids);
    sovtest::endTurns(*g, 1);
    REQUIRE(g->submit(Command::setProduction(1, theirs, item, {16, 6})) == CommandError::Ok);
    // Nearly done in our city, half-way in theirs.
    GameState mid = g->state();
    const int cost = g->productionCost(0, item);
    mid.city(mine)->progress.push_back({item, Fixed::fromInt(cost - 1)});
    mid.city(theirs)->progress.push_back({item, Fixed::fromInt(100)});
    auto g2 = Game::fromScenario(rules(), std::move(mid));
    const size_t units = g2->state().units.size();
    sovtest::endTurns(*g2, 2);  // round to our turn: the Pyramids complete
    CHECK(g2->wonderBuilt(pyramids));
    CHECK(g2->state().city(mine)->has(pyramids));
    CHECK_EQ(g2->state().units.size(), units + 1);  // the free Builder
    const City& rival = *g2->state().city(theirs);
    CHECK(std::find(rival.queue.begin(), rival.queue.end(), item) == rival.queue.end());
    CHECK(rival.wonders.empty());
    CHECK(rival.overflow >= Fixed::fromInt(50));
    CHECK(!g2->canProduce(rival, item));
    CHECK(!offered(*g2, theirs));  // off the list of what the city can make
}

TEST(wonder_effects_reach_every_city) {
    GameState s = wonderState();
    s.cities[0].buildings.push_back(wonder("BUILDING_HANGING_GARDENS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    // +15% growth in all of the owner's cities.
    CHECK_EQ(sumCityModifiers(g->state(), rules(), g->state().cities[0], ModEffect::CityGrowthPercent), Fixed::fromInt(15));
    CHECK_EQ(sumCityModifiers(g->state(), rules(), g->state().cities[1], ModEffect::CityGrowthPercent), Fixed());
}

TEST(wonder_effects_are_generated) {
    const Rules& r = rules();
    const BuildingType& big = r.buildings[at(wonder("BUILDING_BIG_BEN"))];
    bool treasury = false;
    for (const GreatPersonEffect& fx : big.wonderEffects) treasury = treasury || (fx.kind == GreatPersonEffectKind::TreasuryPercent && fx.amount == 50);
    CHECK(treasury);
    CHECK_EQ(r.buildings[at(wonder("BUILDING_HAGIA_SOPHIA"))].spreadCharges, 1);
    // Kotoku-in's Faith percent in its own city comes as a modifier.
    GameState s = wonderState();
    s.cities[0].buildings.push_back(wonder("BUILDING_KOTOKU_IN"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(sumCityModifiers(g->state(), rules(), g->state().cities[0], ModEffect::CityYieldPercent) > Fixed());
}

TEST(wonder_one_time_effects_apply_on_completion) {
    GameState s = wonderState();
    s.players[0].gold = Fixed::fromInt(300);
    const int pop = s.cities[0].population;
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId mine = g->state().cities[0].id;
    // Big Ben adds half the treasury; Angkor Wat adds a citizen to every city of its owner.
    g->wonderCompleted(mine, wonder("BUILDING_BIG_BEN"));
    CHECK(g->state().players[0].gold == Fixed::fromInt(450));
    g->wonderCompleted(mine, wonder("BUILDING_ANGKOR_WAT"));
    CHECK_EQ(g->state().cities[0].population, pop + 1);
    CHECK_EQ(g->state().cities[1].population, 3);  // not ours
}

TEST(wonders_add_policy_slots) {
    GameState s = wonderState();
    Player& me = s.players[0];
    me.government = rules().government("GOVERNMENT_CHIEFDOM");
    const int base = rules().governments[at(me.government)].totalSlots();
    me.policies.assign(static_cast<size_t>(base), kNone);
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId mine = g->state().cities[0].id;
    g->wonderCompleted(mine, wonder("BUILDING_ALHAMBRA"));

    REQUIRE(g->state().players[0].policies.size() == static_cast<size_t>(base + 1));
    CHECK(g->policySlotType(0, base) == PolicySlot::Military);
    // Saved and loaded with the extra slot.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].policies.size(), static_cast<size_t>(base + 1));
}

TEST(wonder_effects_in_code) {
    // Machu Picchu: a Commercial Hub next to a Mountain gains +1 Gold.
    GameState s = wonderState();
    s.plot({7, 6}).terrain = rules().terrain("TERRAIN_DESERT_MOUNTAIN");
    const TypeIndex hub = rules().district("DISTRICT_COMMERCIAL_HUB");
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(wonder("BUILDING_MACHU_PICCHU"));
    s.cities[0].buildings.push_back(wonder("BUILDING_COLOSSEUM"));
    s.cities[0].buildings.push_back(wonder("BUILDING_TAJ_MAHAL"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    const size_t gold = static_cast<size_t>(YieldType::Gold);
    CHECK_EQ(g->districtAdjacency(0, hub, {6, 6})[gold], plain->districtAdjacency(0, hub, {6, 6})[gold] + Fixed::fromInt(1));
    // The Colosseum: +2 loyalty a turn in the owner's cities within 6 tiles.
    const CityId mine = g->state().cities[0].id;
    CHECK(g->loyaltyPerTurn(mine) >= plain->loyaltyPerTurn(mine) + Fixed::fromInt(2));  // and its Amenities
    CHECK(g->nearOwnWonder(g->state().cities[0], wonder("BUILDING_COLOSSEUM"), 6));
    CHECK(!g->nearOwnWonder(g->state().cities[1], wonder("BUILDING_COLOSSEUM"), 6));
}

TEST(wonders_of_great_people_trade_and_envoys) {
    // The Oracle: +2 Great Scientist points from the Campus of its city; Faith patronage a quarter cheaper.
    GameState s = wonderState();
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {6, 6}, true});
    const TypeIndex scientist = rules().greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(wonder("BUILDING_ORACLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), s);
    CHECK_EQ(g->greatPersonPointsPerTurn(0, scientist), plain->greatPersonPointsPerTurn(0, scientist) + 2);
    if (plain->patronageCost(0, scientist, true) > 0) CHECK_EQ(g->patronageCost(0, scientist, true), plain->patronageCost(0, scientist, true) * 75 / 100);
    // Apadana: +2 envoys for each wonder completed in its city.
    const int tokens = g->state().players[0].envoyTokens;
    g->wonderCompleted(g->state().cities[0].id, wonder("BUILDING_APADANA"));
    CHECK_EQ(g->state().players[0].envoyTokens, tokens + 2);
    // University of Sankore: other civs' routes to it carry +1 Science and +1 Gold.
    GameState t = wonderState();
    auto before = Game::fromScenario(rules(), t);
    t.cities[1].buildings.push_back(wonder("BUILDING_UNIVERSITY_OF_SANKORE"));
    std::sort(t.cities[1].buildings.begin(), t.cities[1].buildings.end());
    auto after = Game::fromScenario(rules(), std::move(t));
    const size_t sci = static_cast<size_t>(YieldType::Science);
    CHECK_EQ(after->tradeRouteYields(after->state().cities[0], after->state().cities[1])[sci],
             before->tradeRouteYields(before->state().cities[0], before->state().cities[1])[sci] + Fixed::fromInt(1));
}

TEST(terrain_and_tourism_wonders) {
    // The Eiffel Tower's Appeal reaches the owner's plots; Petra adds Food on desert.
    GameState s = wonderState();
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(wonder("BUILDING_EIFFEL_TOWER"));
    s.cities[0].buildings.push_back(wonder("BUILDING_PETRA"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->plotAppeal({6, 6}), plain->plotAppeal({6, 6}) + 2);
    CHECK_EQ(g->plotAppeal({16, 6}), plain->plotAppeal({16, 6}));  // not ours
    const size_t food = static_cast<size_t>(YieldType::Food);
    const City& mine = g->state().cities[0];
    CHECK_EQ(g->plotYields({6, 6}, mine)[food], plain->plotYields({6, 6}, plain->state().cities[0])[food] + Fixed::fromInt(2));
}

namespace {
// The first player's city with a lake of two plots east of it and the sea along column 2; a second city of
// theirs with a lake of its own, and a lake in the rival's land.
GameState lakeState() {
    GameState s = wonderState();
    const TypeIndex coast = rules().terrain("TERRAIN_COAST");
    s.plot({7, 6}).terrain = coast;
    s.plot({8, 6}).terrain = coast;
    for (int y = 0; y < 14; ++y) s.plot({2, y}).terrain = coast;
    const CityId second = addCity(s, 0, {9, 11}, false, 1);
    for (const Hex& h : s.grid.within({9, 11}, 1)) s.plot(h).city = second, s.plot(h).owner = 0;
    s.plot({10, 11}).terrain = coast;
    s.plot({17, 6}).terrain = coast;
    return s;
}
}  // namespace

TEST(huey_teocalli_and_the_lakes) {
    GameState s = lakeState();
    auto plain = Game::fromScenario(rules(), s);
    const TypeIndex huey = wonder("BUILDING_HUEY_TEOCALLI"), bridge = wonder("BUILDING_GOLDEN_GATE_BRIDGE");
    // Huey Teocalli on a lake; the Golden Gate Bridge, like the harbour wonders, on the sea (01: Lake).
    const City& a = plain->state().cities[0];
    CHECK(plain->canPlaceWonder(a, huey, {7, 6}));
    CHECK(!plain->canPlaceWonder(a, huey, {2, 6}));
    CHECK(plain->canPlaceWonder(a, bridge, {2, 6}));
    CHECK(!plain->canPlaceWonder(a, bridge, {7, 6}));
    // Built: +1 Amenity for the lake plot beside it, +1 Food and +1 Production on lakes in all the owner's cities.
    s.cities[0].buildings.push_back(huey);
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.cities[0].wonders.push_back({huey, {7, 6}});
    auto g = Game::fromScenario(rules(), std::move(s));
    const GameState& gs = g->state();
    const GameState& ps = plain->state();
    CHECK_EQ(g->cityReport(gs.cities[0].id).amenities, plain->cityReport(ps.cities[0].id).amenities + 1);
    const size_t food = static_cast<size_t>(YieldType::Food), prod = static_cast<size_t>(YieldType::Production);
    for (const auto& [plot, city] : {std::pair<Hex, size_t>{{8, 6}, 0}, {{10, 11}, 2}}) {
        CHECK_EQ(g->plotYields(plot, gs.cities[city])[food], plain->plotYields(plot, ps.cities[city])[food] + Fixed::fromInt(1));
        CHECK_EQ(g->plotYields(plot, gs.cities[city])[prod], plain->plotYields(plot, ps.cities[city])[prod] + Fixed::fromInt(1));
    }
    CHECK_EQ(g->plotYields({2, 6}, gs.cities[0])[food], plain->plotYields({2, 6}, ps.cities[0])[food]);    // the sea
    CHECK_EQ(g->plotYields({6, 5}, gs.cities[0])[food], plain->plotYields({6, 5}, ps.cities[0])[food]);    // land by the lake
    CHECK_EQ(g->plotYields({17, 6}, gs.cities[1])[food], plain->plotYields({17, 6}, ps.cities[1])[food]);  // a rival's lake
}

// The Golden Gate Bridge spans two opposite land plots (03: "coast tile spanning two opposite land tiles").
TEST(the_golden_gate_bridge_spans_opposite_land) {
    GameState s = lakeState();
    const TypeIndex bridge = wonder("BUILDING_GOLDEN_GATE_BRIDGE");
    auto across = Game::fromScenario(rules(), s);
    CHECK(across->canPlaceWonder(across->state().cities[0], bridge, {2, 6}));  // desert west and east of it
    s.plot({3, 6}).terrain = rules().terrain("TERRAIN_COAST");                // now land on the west side only
    auto shore = Game::fromScenario(rules(), std::move(s));
    CHECK(!shore->canPlaceWonder(shore->state().cities[0], bridge, {2, 6}));
}

TEST(the_mausoleum_reaches_the_sea_not_lakes) {
    GameState s = lakeState();
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(wonder("BUILDING_MAUSOLEUM_AT_HALICARNASSUS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    const size_t science = static_cast<size_t>(YieldType::Science);
    const City& mine = g->state().cities[0];
    const City& before = plain->state().cities[0];
    CHECK_EQ(g->plotYields({2, 6}, mine)[science], plain->plotYields({2, 6}, before)[science] + Fixed::fromInt(1));
    CHECK_EQ(g->plotYields({8, 6}, mine)[science], plain->plotYields({8, 6}, before)[science]);
}

TEST(jebel_barkal_gives_iron_while_it_stands) {
    GameState s = wonderState();
    s.cities[0].buildings.push_back(wonder("BUILDING_JEBEL_BARKAL"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->greatPersonEffectTotal(0, GreatPersonEffectKind::ResourcePerTurn, rules().resource("RESOURCE_IRON")), 6);
    CHECK_EQ(g->greatPersonEffectTotal(1, GreatPersonEffectKind::ResourcePerTurn, rules().resource("RESOURCE_IRON")), 0);
}

TEST(wonder_sites_survive_a_save) {
    GameState s = wonderState();
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::setProduction(0, g->state().cities[0].id, {ProductionKind::Building, wonder("BUILDING_PYRAMIDS")}, {4, 6})) ==
            CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().cities[0].wonders.size(), 1u);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(a_wonder_needs_its_building_first) {
    // The Great Library: next to a Campus, in a city with a Library (03: BuildingPrereqs).
    GameState s = wonderState();
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {6, 6}, true});
    s.players[0].civics.done[at(rules().civic("CIVIC_RECORDED_HISTORY"))] = 1;
    auto without = Game::fromScenario(rules(), s);
    const TypeIndex library = wonder("BUILDING_GREAT_LIBRARY");
    const ProductionItem item{ProductionKind::Building, library};
    REQUIRE(!rules().buildings[at(library)].prereqsAny.empty());
    CHECK(!without->canPlaceWonder(without->state().cities[0], library, {7, 6}));
    CHECK(!without->canProduce(without->state().cities[0], item));
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto with = Game::fromScenario(rules(), std::move(s));
    CHECK(with->canPlaceWonder(with->state().cities[0], library, {7, 6}));
    CHECK(with->canProduce(with->state().cities[0], item));
    // A civ's unique building counts as the one it replaces: the Aztecs' Calmecac is their Library.
    GameState t = without->state();
    t.cities[0].buildings.push_back(rules().building("BUILDING_CALMECAC"));
    std::sort(t.cities[0].buildings.begin(), t.cities[0].buildings.end());
    auto aztec = Game::fromScenario(rules(), std::move(t));
    CHECK(aztec->canPlaceWonder(aztec->state().cities[0], library, {7, 6}));
    CHECK(aztec->canProduce(aztec->state().cities[0], item));
}

TEST(the_great_library_gives_a_eureka_when_a_rival_recruits_a_great_scientist) {
    // 03: a random Eureka whenever another civ recruits a Great Scientist. Player 1 has the points to recruit one.
    const TypeIndex scientist = rules().greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
    GameState s = wonderState();
    s.players[1].greatPersonPoints[at(scientist)] = 10000;
    // The Great Library is itself a Classical wonder, which earns Buttress's Eureka (04): count only its gift.
    for (Player& p : s.players) p.techs.boosted[at(rules().tech("TECH_BUTTRESS"))] = 1;
    auto withLibrary = [&](size_t city) {
        GameState t = s;
        t.cities[city].buildings.push_back(wonder("BUILDING_GREAT_LIBRARY"));
        std::sort(t.cities[city].buildings.begin(), t.cities[city].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(t));
        sovtest::endTurns(*g, 1);  // player 1's turn begins: it recruits
        return g;
    };
    auto plain = Game::fromScenario(rules(), s);
    sovtest::endTurns(*plain, 1);
    auto rival = withLibrary(0);  // player 0 holds the Great Library
    auto own = withLibrary(1);    // the recruiter holds it
    const auto boosted = [](const Game& g, PlayerId p) {
        const std::vector<uint8_t>& b = g.state().players[at(p)].techs.boosted;
        return static_cast<int>(std::count(b.begin(), b.end(), uint8_t{1}));
    };
    for (const Game* g : {plain.get(), rival.get(), own.get()}) CHECK_EQ(g->state().players[1].greatPeopleRecruited[at(scientist)], 1);
    REQUIRE(boosted(*rival, 0) == boosted(*plain, 0) + 1);
    const Player& p = rival->state().players[0];
    for (size_t t = 0; t < p.techs.boosted.size(); ++t) {
        if (!p.techs.boosted[t] || plain->state().players[0].techs.boosted[t]) continue;
        CHECK(!p.techs.done[t]);
        const int pct = rules().techs[t].boost.percent > 0 ? rules().techs[t].boost.percent : 40;
        CHECK_EQ(p.techs.progress[t], plain->state().players[0].techs.progress[t] + Fixed::fromInt(rival->techCost(static_cast<TypeIndex>(t))) * pct / 100);
    }
    // The recruiter's own Great Library gives it nothing.
    CHECK_EQ(boosted(*own, 1), boosted(*plain, 1));
    CHECK_EQ(boosted(*own, 0), boosted(*plain, 0));
}

TEST(regional_wonders_reach_the_owners_cities_from_their_plot) {
    // 03: the Colosseum's +2 Culture and +2 Amenity reach the owner's cities within 6 tiles of its plot, as Jebel Barkal's
    // +4 Faith does; the Estadio do Maracana's +6 Culture and +2 Amenity reach every city of the owner. A rival's never.
    GameState s = flatState(40, 14, 2);
    for (PlayerId p = 0; p < 2; ++p) Game::fitPlayerToRules(s.players[at(p)], rules());
    const Hex site{8, 5};
    const CityId host = addCity(s, 0, {5, 5}, true, 3);
    const CityId near = addCity(s, 0, {13, 5}, false, 3);
    const CityId far = addCity(s, 0, {32, 5}, false, 3);
    const CityId rival = addCity(s, 1, {9, 11}, true, 3);
    REQUIRE(s.grid.distance(site, {13, 5}) <= 6);
    REQUIRE(s.grid.distance({5, 5}, {13, 5}) > 6);  // in reach of the wonder's plot, not of its city
    REQUIRE(s.grid.distance(site, {32, 5}) > 6);
    REQUIRE(s.grid.distance(site, {9, 11}) <= 6);
    auto plain = Game::fromScenario(rules(), s);
    const auto with = [&](const char* id) {
        GameState t = s;
        City& c = *t.city(host);
        c.buildings.push_back(wonder(id));
        std::sort(c.buildings.begin(), c.buildings.end());
        c.wonders.push_back({wonder(id), site});
        return Game::fromScenario(rules(), std::move(t));
    };
    const size_t culture = static_cast<size_t>(YieldType::Culture), faith = static_cast<size_t>(YieldType::Faith);
    const auto amenities = [](const Game& g, CityId c) { return g.cityReport(c).amenities; };
    const auto yield = [](const Game& g, CityId c, size_t y) { return g.cityReport(c).yields[y]; };
    auto colosseum = with("BUILDING_COLOSSEUM");
    CHECK_EQ(amenities(*colosseum, host), amenities(*plain, host) + 2);  // once in its own city
    CHECK_EQ(amenities(*colosseum, near), amenities(*plain, near) + 2);
    CHECK(yield(*colosseum, near, culture) > yield(*plain, near, culture));
    CHECK_EQ(amenities(*colosseum, far), amenities(*plain, far));
    CHECK_EQ(yield(*colosseum, far, culture), yield(*plain, far, culture));
    CHECK_EQ(amenities(*colosseum, rival), amenities(*plain, rival));
    CHECK_EQ(yield(*colosseum, rival, culture), yield(*plain, rival, culture));
    auto jebel = with("BUILDING_JEBEL_BARKAL");
    CHECK(yield(*jebel, near, faith) > yield(*plain, near, faith));
    CHECK_EQ(yield(*jebel, far, faith), yield(*plain, far, faith));
    CHECK_EQ(yield(*jebel, rival, faith), yield(*plain, rival, faith));
    auto maracana = with("BUILDING_EST_DIO_DO_MARACAN");
    CHECK_EQ(amenities(*maracana, far), amenities(*plain, far) + 2);
    CHECK(yield(*maracana, far, culture) > yield(*plain, far, culture));
    CHECK_EQ(amenities(*maracana, rival), amenities(*plain, rival));
    CHECK_EQ(yield(*maracana, rival, culture), yield(*plain, rival, culture));
}

TEST(the_temple_of_artemis_gives_amenities_for_camps_pastures_and_plantations_near_it) {
    // 03: each Camp, Pasture and Plantation within 4 tiles of the Temple of Artemis gives +1 Amenity to the city whose land
    // it is on, a rival's too (the data checks only the distance). A pillaged one, a Farm, one farther away, or a Temple
    // still being built gives nothing.
    GameState s = wonderState();
    const Hex site{8, 6};
    const TypeIndex temple = wonder("BUILDING_TEMPLE_OF_ARTEMIS");
    const auto improve = [&](Hex h, const char* id) { s.plot(h).improvement = rules().improvement(id); };
    improve({7, 6}, "IMPROVEMENT_CAMP");
    improve({4, 6}, "IMPROVEMENT_PLANTATION");
    improve({6, 8}, "IMPROVEMENT_PASTURE");
    improve({2, 6}, "IMPROVEMENT_CAMP");        // 6 tiles from the Temple
    improve({6, 5}, "IMPROVEMENT_PLANTATION");  // pillaged below
    improve({5, 7}, "IMPROVEMENT_FARM");
    improve({12, 6}, "IMPROVEMENT_CAMP");       // the rival's
    s.plot({6, 5}).pillagedTurns = 1;
    REQUIRE(s.grid.distance(site, {4, 6}) == 4);
    REQUIRE(s.grid.distance(site, {6, 8}) <= 4);
    REQUIRE(s.grid.distance(site, {2, 6}) > 4);
    REQUIRE(s.grid.distance(site, {12, 6}) == 4);
    for (Hex h : {Hex{7, 6}, Hex{4, 6}, Hex{6, 8}, Hex{2, 6}, Hex{6, 5}, Hex{5, 7}}) REQUIRE(s.plot(h).city == s.cities[0].id);
    REQUIRE(s.plot({12, 6}).city == s.cities[1].id);
    auto plain = Game::fromScenario(rules(), s);
    s.cities[0].wonders.push_back({temple, site});
    auto building = Game::fromScenario(rules(), s);
    s.cities[0].buildings.push_back(temple);
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    const auto amenities = [](const Game& game, size_t c) { return game.cityReport(game.state().cities[c].id).amenities; };
    CHECK_EQ(amenities(*g, 0), amenities(*plain, 0) + 3);
    CHECK_EQ(amenities(*g, 1), amenities(*plain, 1) + 1);
    CHECK_EQ(amenities(*building, 0), amenities(*plain, 0));
    CHECK_EQ(amenities(*building, 1), amenities(*plain, 1));
}

// The Golden Gate Bridge (03; data: roads within 1 tile): a land bridge carrying the owner's road. Land units
// cross its plot dry and fight there; ships still sail through; the land on either side gets the road.
TEST(the_golden_gate_bridge_carries_land_units_over_the_water) {
    const TypeIndex ggb = wonder("BUILDING_GOLDEN_GATE_BRIDGE");
    const auto strait = [&]() {
        GameState s = flatState(24, 14, 2);
        for (Player& p : s.players) {
            Game::fitPlayerToRules(p, rules());
            p.relations.resize(s.players.size());
        }
        for (int y = 0; y < 14; ++y) s.plot({10, y}).terrain = rules().terrain("TERRAIN_COAST");
        addCity(s, 0, {6, 6}, true, 3);
        s.cities[0].wonders.push_back({ggb, {10, 6}});
        addCity(s, 1, {16, 6}, true, 3);
        return s;
    };
    GameState s = strait();
    const UnitId warrior = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {9, 6});
    const UnitId galley = sovtest::addUnit(s, "UNIT_GALLEY", 0, {10, 5});
    auto g = Game::fromScenario(rules(), std::move(s));
    const Unit& w = *g->state().unit(warrior);
    const TypeIndex trader = rules().unit("UNIT_TRADER");
    CHECK(!g->bridgeAt({10, 6}));
    CHECK(!g->moveCost(w, {9, 6}, {10, 6}));  // no embarking yet
    CHECK(g->tradePath(0, trader, g->state().cities[0], g->state().cities[1]).empty());
    g->wonderCompleted(g->state().cities[0].id, ggb);
    CHECK(g->bridgeAt({10, 6}));
    CHECK(g->state().plot({9, 6}).route >= 0);  // the land on either side
    CHECK(g->state().plot({11, 6}).route >= 0);
    CHECK(!g->bridgeAt({10, 7}));  // the rest of the strait stays water
    CHECK(!g->moveCost(w, {9, 7}, {10, 7}));
    CHECK(!g->isEmbarkTransition(w, {9, 6}, {10, 6}));
    CHECK(g->moveCost(*g->state().unit(galley), {10, 5}, {10, 6}).has_value());  // ships pass under it
    const auto path = g->findPath(warrior, {12, 6}, true);  // overland: no embarking
    REQUIRE(path.has_value());
    CHECK(std::any_of(path->begin(), path->end(), [](const PathStep& st) { return st.pos == Hex{10, 6}; }));
    REQUIRE(g->submit(Command::move(0, warrior, {10, 6})) == CommandError::Ok);
    REQUIRE((g->state().unit(warrior)->pos == Hex{10, 6}));
    CHECK(!g->isEmbarked(*g->state().unit(warrior)));
    CHECK(!g->isEmbarkTransition(*g->state().unit(warrior), {10, 6}, {11, 6}));
    const Fixed road = rules().routes[static_cast<size_t>(g->state().plot({11, 6}).route)].moveCost;
    CHECK(g->moveCost(*g->state().unit(warrior), {10, 6}, {11, 6}) == std::optional<Fixed>(road));  // ashore along the road
    // A Trader's way runs over it to the far shore.
    const std::vector<Hex> way = g->tradePath(0, trader, g->state().cities[0], g->state().cities[1]);
    CHECK(std::find(way.begin(), way.end(), Hex{10, 6}) != way.end());

    // A land unit attacks an enemy standing on the bridge.
    GameState t = strait();
    t.players[0].relations[1].war = t.players[1].relations[0].war = true;
    const UnitId attacker = sovtest::addUnit(t, "UNIT_WARRIOR", 0, {9, 6});
    sovtest::addUnit(t, "UNIT_WARRIOR", 1, {10, 6});
    auto h = Game::fromScenario(rules(), std::move(t));
    h->wonderCompleted(h->state().cities[0].id, ggb);
    CHECK(h->submit(Command::attack(0, attacker, {10, 6})) == CommandError::Ok);
}
