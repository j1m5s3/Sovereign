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
    CHECK_EQ(wonders, 52);
    const BuildingType& stonehenge = r.buildings[at(wonder("BUILDING_STONEHENGE"))];
    CHECK_EQ(stonehenge.placement.nextToResource, r.resource("RESOURCE_STONE"));
    CHECK_EQ(stonehenge.placement.terrains.size(), 5u);
    const BuildingType& colossus = r.buildings[at(wonder("BUILDING_COLOSSUS"))];
    CHECK_EQ(colossus.tradeCapacity, 1);
    REQUIRE(colossus.wonderEffects.size() == 1u);
    CHECK_EQ(colossus.wonderEffects[0].ref, r.unit("UNIT_TRADER"));
    CHECK(r.buildings[at(wonder("BUILDING_HANGING_GARDENS"))].placement.river);
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
    CHECK(g->canPlaceWonder(c, wonder("BUILDING_PYRAMIDS"), {4, 6}));  // desert
}

TEST(a_wonder_is_built_once_and_rivals_keep_half) {
    GameState s = wonderState();
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex pyramids = wonder("BUILDING_PYRAMIDS");
    const CityId mine = g->state().cities[0].id, theirs = g->state().cities[1].id;
    const ProductionItem item{ProductionKind::Building, pyramids};
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
    auto without = Game::fromScenario(rules(), s);
    const TypeIndex library = wonder("BUILDING_GREAT_LIBRARY");
    REQUIRE(!rules().buildings[at(library)].prereqsAny.empty());
    CHECK(!without->canPlaceWonder(without->state().cities[0], library, {7, 6}));
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto with = Game::fromScenario(rules(), std::move(s));
    CHECK(with->canPlaceWonder(with->state().cities[0], library, {7, 6}));
}

TEST(the_great_library_gives_a_eureka_when_a_rival_recruits_a_great_scientist) {
    // 03: a random Eureka whenever another civ recruits a Great Scientist. Player 1 has the points to recruit one.
    const TypeIndex scientist = rules().greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
    GameState s = wonderState();
    s.players[1].greatPersonPoints[at(scientist)] = 10000;
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
