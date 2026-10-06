// World wonders (03-districts-buildings-wonders.md, Wonders).
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
    CHECK(g->nearOwnWonder(g->state().cities[0], "BUILDING_COLOSSEUM", 6));
    CHECK(!g->nearOwnWonder(g->state().cities[1], "BUILDING_COLOSSEUM", 6));
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
