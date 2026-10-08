// Religion (06-religion.md): pantheons, founding, pressure and followers, religious units,
// theological combat, founder beliefs, worship buildings, the religious victory.
#include <algorithm>

#include "helpers.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex belief(const char* id) { return rules().belief(id); }

// Two players: player 0's capital at (5,6) with a finished Holy Site, a second city at (11,6),
// player 1's city at (17,6).
GameState religionState() {
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId cap = addCity(s, 0, {5, 6}, true, 4);
    addCity(s, 0, {11, 6}, false, 4);
    addCity(s, 1, {17, 6}, true, 4);
    s.cities[0].districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {6, 6}, true});
    s.cities[0].buildings.push_back(rules().building("BUILDING_SHRINE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    for (const Hex& h : s.grid.within({5, 6}, 2)) {
        s.plot(h).owner = 0;
        s.plot(h).city = cap;
    }
    for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    return s;
}

UnitId addProphet(GameState& s, Hex at) {
    const UnitId id = addUnit(s, "UNIT_GREAT_PROPHET", 0, at);
    s.units.back().greatPerson = rules().greatPerson("GREAT_PERSON_CONFUCIUS");
    return id;
}

// Player 0 founds Buddhism with Tithe and Feed the World on the capital's Holy Site.
std::unique_ptr<Game> withReligion(GameState s) {
    const UnitId prophet = addProphet(s, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CommandError e = g->submit(Command::foundReligion(0, prophet, rules().religion("RELIGION_BUDDHISM"), belief("BELIEF_TITHE"),
                                                           belief("BELIEF_FEED_THE_WORLD")));
    if (e != CommandError::Ok) std::printf("  foundReligion: %s\n", commandErrorName(e));
    return g;
}

// Player 0 founds Buddhism with Tithe and the given follower belief.
std::unique_ptr<Game> withFollowerBelief(GameState s, const char* follower) {
    const UnitId prophet = addProphet(s, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CommandError e = g->submit(Command::foundReligion(0, prophet, rules().religion("RELIGION_BUDDHISM"), belief("BELIEF_TITHE"), belief(follower)));
    if (e != CommandError::Ok) std::printf("  foundReligion: %s\n", commandErrorName(e));
    return g;
}
}  // namespace

TEST(religion_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.beliefs.size(), 59u);
    CHECK_EQ(r.religions.size(), 12u);
    const UnitType& m = r.units[at(r.unit("UNIT_MISSIONARY"))];
    CHECK_EQ(m.religiousStrength, 100);
    CHECK_EQ(m.spreadCharges, 3);
    CHECK_EQ(m.evictPercent, 10);
    CHECK(r.units[at(r.unit("UNIT_GREAT_PROPHET"))].foundReligion);
    CHECK(r.buildings[at(r.building("BUILDING_CATHEDRAL"))].faithOnly);
    CHECK_EQ(r.beliefs[at(belief("BELIEF_CATHEDRAL"))].worshipBuilding, r.building("BUILDING_CATHEDRAL"));
    CHECK_EQ(r.beliefs[at(belief("BELIEF_FERTILITY_RITES"))].grantUnit, r.unit("UNIT_BUILDER"));
    CHECK_EQ(r.mapSizes[at(r.mapSize("MAPSIZE_TINY"))].maxReligions, 3);
}

TEST(a_pantheon_needs_faith_and_is_unique) {
    GameState s = religionState();
    s.players[0].faith = Fixed::fromInt(20);
    s.players[1].faith = Fixed::fromInt(40);
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex rites = belief("BELIEF_FERTILITY_RITES");
    CHECK(!g->canFoundPantheon(0, rites));  // 20 < 25
    CHECK(!g->canFoundPantheon(0, belief("BELIEF_TITHE")));  // not a pantheon belief
    const size_t units = g->state().units.size();
    REQUIRE(g->submit(Command::foundPantheon(0, rites)) == CommandError::CannotFoundReligion);
    sovtest::endTurns(*g, 1);  // player 1's turn
    REQUIRE(g->submit(Command::foundPantheon(1, rites)) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].faith, Fixed::fromInt(15));
    CHECK_EQ(g->state().units.size(), units + 1);  // the free Builder
    CHECK(!g->canFoundPantheon(0, rites));         // taken
}

TEST(a_great_prophet_founds_a_religion_on_a_holy_site) {
    GameState s = religionState();
    s.players[0].pantheon = belief("BELIEF_GOD_OF_THE_OPEN_SKY");
    const UnitId elsewhere = addProphet(s, {4, 6});
    auto g = withReligion(std::move(s));
    REQUIRE(g->state().religions.size() == 1u);
    const FoundedReligion& r = g->state().religions[0];
    CHECK_EQ(r.founder, 0);
    CHECK_EQ(r.holyCity, g->state().cities[0].id);
    CHECK_EQ(r.beliefs.size(), 3u);  // the pantheon joins the religion
    CHECK_EQ(g->state().players[0].religion, 0);
    CHECK_EQ(g->cityMajorityReligion(g->state().cities[0]), 0);  // the Holy City converts at once
    CHECK_EQ(g->cityMajorityReligion(g->state().cities[2]), -1);
    // One religion each.
    CHECK(!g->canFoundReligion(elsewhere, rules().religion("RELIGION_ISLAM"), belief("BELIEF_PILGRIMAGE"), belief("BELIEF_CHORAL_MUSIC")));
}

TEST(passive_pressure_spreads_from_the_holy_city) {
    auto g = withReligion(religionState());
    const CityId near = g->state().cities[1].id;
    const int before = g->state().city(near)->pressure.empty() ? 0 : g->state().city(near)->pressure[0];
    sovtest::endTurns(*g, 2);  // one world turn
    // Holy City with a Holy Site: 1 x2 x4 per turn to cities within 10 tiles.
    CHECK_EQ(g->state().city(near)->pressure[0] - before, 8);
}

TEST(missionaries_bought_with_faith_convert_cities) {
    GameState s = religionState();
    s.players[0].faith = Fixed::fromInt(500);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem missionary{ProductionKind::Unit, rules().unit("UNIT_MISSIONARY")};
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), missionary), 75);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, missionary)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].faith, Fixed::fromInt(425));
    const Unit& m = g->state().units.back();
    CHECK_EQ(m.religion, 0);
    CHECK_EQ(m.charges, 3);
    const UnitId id = m.id;
    // Walk next to the second city, then spread: 2 x 100 pressure at full health.
    Game& game = *g;
    REQUIRE(game.submit(Command::move(0, id, {10, 6})) == CommandError::Ok);
    sovtest::endTurns(game, 2);
    if (game.state().unit(id)->pos != Hex{10, 6}) sovtest::endTurns(game, 2);
    REQUIRE(game.state().unit(id)->pos == (Hex{10, 6}));
    const City& target = *game.state().city(game.state().cities[1].id);
    CHECK(game.canSpreadReligion(id));
    REQUIRE(game.submit(Command::spreadReligion(0, id)) == CommandError::Ok);
    CHECK_EQ(game.cityMajorityReligion(*game.state().city(target.id)), 0);
    CHECK_EQ(game.state().unit(id)->charges, 2);
}

// Exodus of the Evangelists in a Golden Age (09): Missionaries, Apostles and Inquisitors bought with Faith carry 2 more
// spreads; nothing else Faith buys gains charges from it (a Naturalist here).
TEST(exodus_of_the_evangelists_adds_spreads_to_evangelists_only) {
    const auto charges = [](bool exodus, const char* unit) {
        GameState s = religionState();
        s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        Player& p = s.players[0];
        p.faith = Fixed::fromInt(2000);
        p.civics.done[at(rules().civic("CIVIC_CONSERVATION"))] = 1;
        p.inquisition = true;
        if (exodus) {
            p.age = Age::Golden;
            p.dedications = {rules().dedication("DEDICATION_EXODUS_OF_THE_EVANGELISTS")};
        }
        auto g = withReligion(std::move(s));
        const ProductionItem item{ProductionKind::Unit, rules().unit(unit)};
        REQUIRE(g->submit(Command::purchaseWithFaith(0, g->state().cities[0].id, item)) == CommandError::Ok);
        return g->state().units.back().charges;
    };
    for (const char* unit : {"UNIT_MISSIONARY", "UNIT_APOSTLE", "UNIT_INQUISITOR"}) CHECK_EQ(charges(true, unit), charges(false, unit) + 2);
    CHECK_EQ(charges(true, "UNIT_NATURALIST"), 0);
}

TEST(founder_beliefs_pay_per_following_city) {
    auto g = withReligion(religionState());
    // Tithe: +3 Gold for each city following the religion (the Holy City so far).
    CHECK_EQ(g->founderYields(0)[static_cast<size_t>(YieldType::Gold)], Fixed::fromInt(3));
    // Feed the World: +3 Food from the Shrine in a following city.
    const Fixed food = g->cityReport(g->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)];
    GameState plain = religionState();
    auto g2 = Game::fromScenario(rules(), std::move(plain));
    CHECK_EQ(food - g2->cityReport(g2->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)], Fixed::fromInt(3));
}

TEST(worship_buildings_need_their_belief_and_faith) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(1000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem cathedral{ProductionKind::Building, rules().building("BUILDING_CATHEDRAL")};
    CHECK(!g->canProduce(*g->state().city(holy), cathedral));         // never built with production
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), cathedral), -1);  // no Cathedral belief yet
    // An Apostle adds the Cathedral belief.
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const UnitId a = g->state().units.back().id;
    REQUIRE(g->submit(Command::evangelizeBelief(0, a, belief("BELIEF_CATHEDRAL"))) == CommandError::Ok);
    CHECK(!g->state().unit(a));
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), cathedral), 190);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, cathedral)) == CommandError::Ok);
    CHECK(g->state().city(holy)->has(cathedral.type));
}

TEST(a_temple_s_replacement_buys_apostles_and_worship_buildings) {
    // Mali's Sahel Mosque counts as the Temple an Apostle and a worship building need (leaders-and-art-style; 06).
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_SAHEL_MOSQUE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(1000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const UnitId a = g->state().units.back().id;
    REQUIRE(g->submit(Command::evangelizeBelief(0, a, belief("BELIEF_CATHEDRAL"))) == CommandError::Ok);
    const ProductionItem cathedral{ProductionKind::Building, rules().building("BUILDING_CATHEDRAL")};
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), cathedral), 190);
}

TEST(religious_community_counts_a_temple_s_replacement) {
    // Religious Community (06): international routes +2 Gold for the origin's Temple; Mali's Sahel Mosque is its Temple.
    const auto gold = [](const char* temple) {
        GameState s = religionState();
        if (temple) s.cities[0].buildings.push_back(rules().building(temple));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        auto g = withFollowerBelief(std::move(s), "BELIEF_RELIGIOUS_COMMUNITY");
        return g->tradeRouteYields(g->state().cities[0], g->state().cities[2])[static_cast<size_t>(YieldType::Gold)];
    };
    CHECK_EQ(gold("BUILDING_TEMPLE") - gold(nullptr), Fixed::fromInt(2));
    CHECK_EQ(gold("BUILDING_SAHEL_MOSQUE") - gold(nullptr), Fixed::fromInt(2));
}

TEST(theological_combat_needs_no_war) {
    GameState s = religionState();
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
    s.religions.push_back({rules().religion("RELIGION_ISLAM"), 1, s.cities[2].id, {belief("BELIEF_PILGRIMAGE")}});
    s.players[0].religion = 0;
    s.players[1].religion = 1;
    const UnitId apostle = addUnit(s, "UNIT_APOSTLE", 0, {13, 6});
    s.units.back().religion = 0;
    s.units.back().charges = 3;
    const UnitId missionary = addUnit(s, "UNIT_MISSIONARY", 1, {14, 6});
    s.units.back().religion = 1;
    s.units.back().charges = 3;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(!g->atWar(0, 1));
    REQUIRE(g->submit(Command::attack(0, apostle, {14, 6})) == CommandError::Ok);
    const Unit* m = g->state().unit(missionary);
    CHECK(!m || m->hp < 100);
    // A unit of the same religion is no target.
    CHECK_EQ(g->submit(Command::attack(0, apostle, {12, 6})), CommandError::CannotAttack);
}

TEST(a_religion_followed_across_every_civ_wins) {
    GameState s = religionState();
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
    s.players[0].religion = 0;
    for (City& c : s.cities) c.pressure = {10000};
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->religiousVictor(), 0);
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().winner, 0);
    CHECK(g->state().victory == Victory::Religious);
}

// A civ counts once more than half its cities follow the religion: half of them is not enough.
TEST(half_a_civs_cities_is_not_enough_for_a_religious_victory) {
    const auto victor = [](bool both) {
        GameState s = religionState();
        s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
        s.players[0].religion = 0;
        s.cities[0].pressure = {10000};
        s.cities[2].pressure = {10000};
        if (both) s.cities[1].pressure = {10000};
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->religiousVictor();
    };
    CHECK_EQ(victor(false), kNoPlayer);  // one of player 0's two cities
    CHECK_EQ(victor(true), 0);
}

// Each civ counts its own cities, wherever they sit in the game's list: player 1's second city following the
// religion can't make up for its capital not following it.
TEST(each_civ_counts_its_own_cities_for_a_religious_victory) {
    const auto victor = [](bool capital) {
        GameState s = religionState();
        addCity(s, 1, {21, 10}, false, 4);
        s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
        s.players[0].religion = 0;
        for (City& c : s.cities) c.pressure = {10000};
        if (!capital) s.cities[2].pressure = {};
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->religiousVictor();
    };
    CHECK_EQ(victor(false), kNoPlayer);  // one of player 1's two cities
    CHECK_EQ(victor(true), 0);
}

// The first religion, short of half of one civ's cities, is out; the second, followed everywhere, still wins.
TEST(a_second_religion_wins_where_the_first_falls_short) {
    GameState s = religionState();
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
    s.religions.push_back({rules().religion("RELIGION_HINDUISM"), 1, s.cities[2].id, {belief("BELIEF_PILGRIMAGE")}});
    s.players[0].religion = 0;
    s.players[1].religion = 1;
    for (City& c : s.cities) c.pressure = {0, 10000};
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->religiousVictor(), 1);
}

TEST(pantheon_beliefs_take_effect) {
    // Desert Folklore: the Holy Site gains +1 Faith per adjacent Desert.
    GameState s = religionState();
    for (const Hex& h : s.grid.within({6, 6}, 1)) {
        if (h != Hex{6, 6} && h != Hex{5, 6}) s.plot(h).terrain = rules().terrain("TERRAIN_DESERT");
    }
    const TypeIndex holy = rules().district("DISTRICT_HOLY_SITE");
    const size_t faith = static_cast<size_t>(YieldType::Faith);
    auto plain = Game::fromScenario(rules(), s);
    GameState t = s;
    t.players[0].pantheon = belief("BELIEF_DESERT_FOLKLORE");
    auto g = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(g->districtAdjacency(0, holy, {6, 6})[faith], plain->districtAdjacency(0, holy, {6, 6})[faith] + Fixed::fromInt(5));
    // God of the Forge: +25% toward military units in all the player's cities; City Patron Goddess: +25% toward
    // districts in a city with no specialty district yet.
    GameState u = s;
    u.players[0].pantheon = belief("BELIEF_GOD_OF_THE_FORGE");
    auto forge = Game::fromScenario(rules(), std::move(u));
    const TypeIndex warrior = rules().unit("UNIT_WARRIOR");
    CHECK_EQ(sumUnitProductionPercent(forge->state(), rules(), forge->state().cities[1], warrior), Fixed::fromInt(25));
    CHECK_EQ(sumUnitProductionPercent(forge->state(), rules(), forge->state().cities[2], warrior), Fixed());
    GameState v = s;
    v.players[0].pantheon = belief("BELIEF_CITY_PATRON_GODDESS");
    auto patron = Game::fromScenario(rules(), std::move(v));
    CHECK_EQ(sumCityModifiers(patron->state(), rules(), patron->state().cities[1], ModEffect::CityDistrictProductionPercent), Fixed::fromInt(25));
    CHECK_EQ(sumCityModifiers(patron->state(), rules(), patron->state().cities[0], ModEffect::CityDistrictProductionPercent), Fixed());
}

// God of Craftsmen (+1 Production and Faith on improved strategic resources) and Religious Idols (+2 Faith on mines
// over bonus and luxury resources) reach a plot by its resource's class.
TEST(pantheons_reach_plots_by_resource_class) {
    GameState s = religionState();
    const Hex plots[] = {{4, 6}, {4, 7}, {4, 5}, {5, 7}};  // mined Iron, Copper and Diamonds; Iron unimproved
    const char* resources[] = {"RESOURCE_IRON", "RESOURCE_COPPER", "RESOURCE_DIAMONDS", "RESOURCE_IRON"};
    for (size_t i = 0; i < 4; ++i) {
        s.plot(plots[i]).resource = rules().resource(resources[i]);
        s.plot(plots[i]).improvement = i < 3 ? rules().improvement("IMPROVEMENT_MINE") : kNone;
    }
    auto plain = Game::fromScenario(rules(), s);
    // The Production and Faith the pantheon adds on each plot, in turn.
    const auto added = [&](const char* pantheon) {
        GameState t = s;
        t.players[0].pantheon = belief(pantheon);
        auto g = Game::fromScenario(rules(), std::move(t));
        std::vector<int> out;
        for (const Hex& h : plots) {
            const Yields a = g->plotYields(h, g->state().cities[0]), b = plain->plotYields(h, plain->state().cities[0]);
            for (const size_t y : {static_cast<size_t>(YieldType::Production), static_cast<size_t>(YieldType::Faith)}) out.push_back(static_cast<int>((a[y] - b[y]).toInt()));
        }
        return out;
    };
    CHECK(added("BELIEF_GOD_OF_CRAFTSMEN") == std::vector<int>({1, 1, 0, 0, 0, 0, 0, 0}));
    CHECK(added("BELIEF_RELIGIOUS_IDOLS") == std::vector<int>({0, 0, 0, 2, 0, 2, 0, 0}));
}

// Earth Goddess: +1 Faith on a plot of Breathtaking appeal (4 or more), on the plot as in its city's yields.
TEST(earth_goddess_adds_faith_on_breathtaking_plots) {
    GameState s = religionState();
    const Hex ringed = {3, 6}, center = s.cities[0].pos;
    for (const Hex& h : s.grid.within(ringed, 1)) {
        if (h != ringed) s.plot(h).feature = rules().feature("FEATURE_FOREST");
    }
    s.cities[0].worked = {s.grid.index(ringed)};
    auto plain = Game::fromScenario(rules(), s);
    s.players[0].pantheon = belief("BELIEF_EARTH_GODDESS");
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->plotAppeal(ringed) >= 4);  // Woods all round
    REQUIRE(g->plotAppeal(center) < 4);
    const size_t faith = static_cast<size_t>(YieldType::Faith);
    const City& with = g->state().cities[0];
    const City& without = plain->state().cities[0];
    CHECK_EQ(g->plotYields(ringed, with)[faith], plain->plotYields(ringed, without)[faith] + Fixed::fromInt(1));
    CHECK_EQ(g->plotYields(center, with)[faith], plain->plotYields(center, without)[faith]);
    CHECK_EQ(g->cityReport(with.id).yields[faith], plain->cityReport(without.id).yields[faith] + Fixed::fromInt(1));
}

TEST(god_of_healing_heals_next_to_a_holy_site) {
    auto hpAfter = [](bool healing) {
        GameState s = religionState();
        if (healing) s.players[0].pantheon = belief("BELIEF_GOD_OF_HEALING");
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
        s.units.back().hp = 40;
        s.units.back().activity = Activity::Sleep;
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 2);
        return g->state().unit(w)->hp;
    };
    CHECK_EQ(hpAfter(true), hpAfter(false) + 30);
}

TEST(follower_beliefs_take_effect) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    s.cities[0].buildings.push_back(rules().building("BUILDING_PYRAMIDS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    const size_t faith = static_cast<size_t>(YieldType::Faith);
    // Divine Inspiration: +4 Faith per wonder in a following city.
    auto plain = withFollowerBelief(s, "BELIEF_FEED_THE_WORLD");
    auto inspired = withFollowerBelief(s, "BELIEF_DIVINE_INSPIRATION");
    const CityId holy = plain->state().cities[0].id;
    CHECK_EQ(inspired->cityReport(holy).yields[faith], plain->cityReport(holy).yields[faith] + Fixed::fromInt(4));
    // Warrior Monks: bought with Faith only where the city follows a religion with the belief.
    const ProductionItem monk{ProductionKind::Unit, rules().unit("UNIT_WARRIOR_MONK")};
    auto monks = withFollowerBelief(s, "BELIEF_WARRIOR_MONKS");
    CHECK_EQ(plain->faithPurchaseCost(0, *plain->state().city(holy), monk), -1);
    const int monkPrice = monks->faithPurchaseCost(0, *monks->state().city(holy), monk);
    CHECK(monkPrice > 0);
    // Mercenary Companies on Faith (World Congress): twice the Faith.
    monks->stateMutForTests().passedResolutions.push_back({rules().resolution("RESOLUTION_MERCENARY_COMPANIES"), 0, static_cast<int32_t>(YieldType::Faith)});
    CHECK_EQ(monks->faithPurchaseCost(0, *monks->state().city(holy), monk), 2 * monkPrice);
}

TEST(missionary_zeal_lets_religious_units_ignore_terrain) {
    GameState s = religionState();
    s.plot({8, 9}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    s.plot({9, 9}).terrain = rules().terrain("TERRAIN_COAST");
    const UnitId missionary = addUnit(s, "UNIT_MISSIONARY", 0, {7, 9});
    auto g = withReligion(std::move(s));
    const int religion = g->state().players[0].religion;
    REQUIRE(religion >= 0);
    CHECK_EQ(g->moveCost(*g->state().unit(missionary), {7, 9}, {8, 9}).value_or(Fixed()), Fixed::fromInt(2));  // hills
    g->stateMutForTests().religions[static_cast<size_t>(religion)].beliefs.push_back(belief("BELIEF_MISSIONARY_ZEAL"));
    CHECK_EQ(g->moveCost(*g->state().unit(missionary), {7, 9}, {8, 9}).value_or(Fixed()), Fixed::fromInt(1));
    // Stepping ashore still costs what disembarking costs.
    const int ashore = rules().globalInt("MOVEMENT_EMBARK_COST") + 1;
    CHECK_EQ(g->moveCost(*g->state().unit(missionary), {9, 9}, {8, 9}).value_or(Fixed()), Fixed::fromInt(ashore));
}

TEST(sacred_places_pays_for_wonders_in_following_cities) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_PYRAMIDS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    const UnitId prophet = addProphet(s, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::foundReligion(0, prophet, rules().religion("RELIGION_BUDDHISM"), belief("BELIEF_SACRED_PLACES"),
                                             belief("BELIEF_FEED_THE_WORLD"))) == CommandError::Ok);
    CHECK_EQ(g->founderYields(0)[static_cast<size_t>(YieldType::Science)], Fixed::fromInt(2));  // the Holy City has the Pyramids
}

TEST(a_founded_religion_steadies_loyalty) {
    auto plain = Game::fromScenario(rules(), religionState());
    auto g = withReligion(religionState());
    const CityId holy = g->state().cities[0].id;
    CHECK_EQ(g->loyaltyPerTurn(holy), plain->loyaltyPerTurn(holy) + Fixed::fromInt(3));  // its own founded religion
}

TEST(religion_survives_a_save) {
    GameState s = religionState();
    s.players[0].pantheon = belief("BELIEF_STONE_CIRCLES");
    auto g = withReligion(std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().religions.size(), 1u);
    CHECK_EQ(loaded->state().players[0].pantheon, belief("BELIEF_STONE_CIRCLES"));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(each_new_apostle_gets_a_promotion) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(5000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const Unit& a = g->state().units.back();
    REQUIRE(a.promotions.size() == 1u);
    CHECK_EQ(rules().promotions[static_cast<size_t>(a.promotions[0])].promotionClass, std::string("PROMOTION_CLASS_RELIGIOUS_APOSTLE"));
    // Orator: two more charges; Debater: +20 theological strength.
    GameState t = g->state();
    t.units.back().promotions = {rules().promotion("PROMOTION_DEBATER")};
    auto h = Game::fromScenario(rules(), t);
    const int base = rules().units[static_cast<size_t>(rules().unit("UNIT_APOSTLE"))].religiousStrength;
    CHECK(h->religiousStrength(h->state().units.back(), false) >= base + 20);
    t.units.back().promotions = {rules().promotion("PROMOTION_ORATOR")};
    auto o = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(o->unitEffectTotal(o->state().units.back(), UnitEffectKind::SpreadCharges), 2);
}

TEST(an_apostle_launches_the_inquisition) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(2000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    const ProductionItem inquisitor{ProductionKind::Unit, rules().unit("UNIT_INQUISITOR")};
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), inquisitor), -1);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const UnitId a = g->state().units.back().id;
    const int before = g->religiousStrength(*g->state().unit(a), false);
    REQUIRE(g->canLaunchInquisition(a));
    REQUIRE(g->submit(Command::launchInquisition(0, a)) == CommandError::Ok);
    CHECK(!g->state().unit(a));  // spent
    CHECK(g->state().players[0].inquisition);
    CHECK(sovtest::hasMoment(*g, 0, "MOMENT_WORLD_S_FIRST_INQUISITION"));  // 09
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), inquisitor), 75);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    CHECK(!g->canLaunchInquisition(g->state().units.back().id));  // once
    CHECK_EQ(g->religiousStrength(g->state().units.back(), false), before + 15);  // at home, after the Inquisition
}

TEST(a_guru_heals_religious_units_beside_it) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(2000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, {ProductionKind::Unit, rules().unit("UNIT_APOSTLE")})) == CommandError::Ok);
    const UnitId a = g->state().units.back().id;
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, {ProductionKind::Unit, rules().unit("UNIT_GURU")})) == CommandError::Ok);
    const UnitId guru = g->state().units.back().id;
    CHECK_EQ(g->state().unit(guru)->charges, 3);
    CHECK(!g->canHealReligious(guru));  // nobody hurt
    GameState hurt = g->state();
    hurt.unit(a)->hp = 30;
    hurt.unit(a)->pos = hurt.unit(guru)->pos;
    auto h = Game::fromScenario(rules(), std::move(hurt));
    REQUIRE(h->submit(Command::healReligious(0, guru)) == CommandError::Ok);
    CHECK_EQ(h->state().unit(a)->hp, 70);  // COMBAT_HEAL_RELIGIOUS_CHARGE
    CHECK_EQ(h->state().unit(guru)->charges, 2);
}

// Stonehenge (03; data: its two grants): the civ's next Great Prophet while it can still earn one, which
// is its one Prophet; once it cannot, an Apostle of its religion.
TEST(stonehenge_grants_the_civs_prophet_or_else_an_apostle) {
    const TypeIndex stonehenge = rules().building("BUILDING_STONEHENGE");
    const TypeIndex prophets = rules().greatPersonClass("GREAT_PERSON_CLASS_PROPHET");
    auto g = Game::fromScenario(rules(), religionState());
    const size_t before = g->state().units.size();
    g->wonderCompleted(g->state().cities[0].id, stonehenge);
    REQUIRE(g->state().units.size() == before + 1);
    const Unit& prophet = g->state().units.back();
    CHECK_EQ(prophet.type, rules().unit("UNIT_GREAT_PROPHET"));
    CHECK(prophet.greatPerson != kNone);  // a named Prophet from the pool
    CHECK_EQ(g->state().players[0].greatPeopleRecruited[at(prophets)], 1);
    // Points earn no second one.
    GameState s = g->state();
    s.players[0].greatPersonPoints[at(prophets)] = 10000;
    for (Unit& u : s.units) u.activity = Activity::Sleep;  // the Prophet waits
    auto h = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*h, 2);
    int count = 0;
    for (const Unit& u : h->state().units) count += u.owner == 0 && u.type == rules().unit("UNIT_GREAT_PROPHET") ? 1 : 0;
    CHECK_EQ(count, 1);
    // A civ with a religion gets an Apostle of it instead.
    auto k = withReligion(religionState());
    REQUIRE(k->state().players[0].religion >= 0);
    k->wonderCompleted(k->state().cities[0].id, stonehenge);
    const Unit& apostle = k->state().units.back();
    CHECK_EQ(apostle.type, rules().unit("UNIT_APOSTLE"));
    CHECK_EQ(apostle.religion, k->state().players[0].religion);
    CHECK(apostle.charges > 0);
}

TEST(st_basils_doubles_religious_tourism_and_cristo_keeps_it_whole) {
    // Religious tourism (07): the Holy City of the religion its owner founded.
    const int holy = rules().globalInt("TOURISM_FROM_HOLY_CITY");
    auto g = withReligion(religionState());
    REQUIRE(g->state().players[0].religion >= 0);
    CHECK_EQ(g->religiousTourism(g->state().cities[0]), holy);
    CHECK_EQ(g->religiousTourism(g->state().cities[1]), 0);  // not the Holy City
    CHECK_EQ(g->religiousTourism(g->state().cities[2]), 0);  // player 1 founded no religion
    auto build = [](GameState s, size_t city, const char* building) {
        s.cities[city].buildings.push_back(rules().building(building));
        std::sort(s.cities[city].buildings.begin(), s.cities[city].buildings.end());
        return s;
    };
    // St. Basil's Cathedral doubles its city's (03): in the Holy City it brings 8 more than in another city.
    auto inHoly = Game::fromScenario(rules(), build(g->state(), 0, "BUILDING_ST_BASIL_S_CATHEDRAL"));
    auto elsewhere = Game::fromScenario(rules(), build(g->state(), 1, "BUILDING_ST_BASIL_S_CATHEDRAL"));
    CHECK_EQ(inHoly->religiousTourism(inHoly->state().cities[0]), 2 * holy);
    CHECK_EQ(inHoly->tourismBase(0), elsewhere->tourismBase(0) + holy);
    // The Enlightenment halves it, except with Cristo Redentor (03: never reduced by later-era rules).
    const TypeIndex enlightenment = rules().civic("CIVIC_THE_ENLIGHTENMENT");
    auto sent = [&](bool cristo, bool enlightened) {
        GameState s = cristo ? build(g->state(), 1, "BUILDING_CRISTO_REDENTOR") : g->state();
        s.players[0].civics.done[at(enlightenment)] = enlightened ? 1 : 0;
        auto h = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*h, 2);  // player 0's next turn sends its tourism
        return h->state().players[0].tourismTo[1];
    };
    REQUIRE(sent(false, false) > 0);
    // Half of it is lost, and the cut toward a civ of another religion then halves what is left: a quarter less in all.
    CHECK_EQ(sent(false, true), sent(false, false) - holy / 4);
    CHECK_EQ(sent(true, true), sent(true, false));
}
