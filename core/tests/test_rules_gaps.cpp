// Rules found missing by an audit of the specs (01, 02, 03, 05): city spacing across water, occupied cities,
// Theocracy's Faith purchases, the Heavy Chariot, Observation units, the Giant Death Robot, hidden units.
#include <algorithm>

#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::claimFor;
using sovtest::flatState;
using sovtest::rules;

TEST(cities_may_stand_closer_across_water) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {5, 5}, true, 3);
    for (Plot& p : s.plots) p.continent = 0;
    s.plot({8, 5}).continent = 1;  // three plots away, on another landmass
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->canFoundCityAt(0, {8, 5}));
    CommandError why = CommandError::Ok;
    CHECK(!g->canFoundCityAt(0, {5, 8}, &why));  // three away on the same landmass
    CHECK_EQ(why, CommandError::TooCloseToCity);
}

TEST(no_city_is_founded_on_a_district_or_wonder_across_the_water) {
    // A district or wonder may stand three plots from its city on another landmass, where a city could be founded
    // (above), but the plot is taken (03).
    const auto founding = [](bool district, bool wonder) {
        GameState s = flatState(20, 12, 1);
        Game::fitPlayerToRules(s.players[0], rules());
        addCity(s, 0, {5, 5}, true, 3);
        for (Plot& p : s.plots) p.continent = 0;
        s.plot({8, 5}).continent = 1;
        claimFor(s, s.cities[0], {8, 5});
        if (district) s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {8, 5}, true});
        if (wonder) s.cities[0].wonders.push_back({rules().building("BUILDING_STONEHENGE"), {8, 5}});
        auto g = Game::fromScenario(rules(), std::move(s));
        CommandError why = CommandError::Ok;
        return g->canFoundCityAt(0, {8, 5}, &why) ? CommandError::Ok : why;
    };
    CHECK_EQ(founding(false, false), CommandError::Ok);  // in its own land, with nothing on the plot
    CHECK_EQ(founding(true, false), CommandError::CannotFoundHere);
    CHECK_EQ(founding(false, true), CommandError::CannotFoundHere);
}

TEST(occupied_cities_do_not_grow) {
    auto foodAfter = [](bool war) {
        GameState s = flatState(20, 12, 2);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        addCity(s, 0, {4, 5}, true, 3);
        const CityId taken = addCity(s, 0, {10, 5}, false, 2);
        addCity(s, 1, {16, 5}, true, 3);
        s.city(taken)->originalOwner = 1;
        s.city(taken)->population = 1;
        for (const Hex& h : s.grid.within({10, 5}, 1)) s.plot(h).owner = 0, s.plot(h).city = taken;
        s.city(taken)->worked = {s.grid.index({11, 5})};
        s.players[0].relations.resize(2);
        s.players[1].relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = war;
        auto g = Game::fromScenario(rules(), std::move(s));
        sovtest::endTurns(*g, 2);
        return g->state().city(taken)->food;
    };
    CHECK(foodAfter(true) < foodAfter(false));
}

TEST(war_weariness_amenities_stop_at_the_city_kind_floor) {
    const auto report = [](bool captured, bool war, int weariness = 0) {
        GameState s = flatState(20, 12, 2);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        addCity(s, 0, {4, 5}, true, 3);
        const CityId taken = addCity(s, 0, {10, 5}, false, 5);
        addCity(s, 1, {16, 5}, true, 3);
        City& city = *s.city(taken);
        if (captured) city.originalOwner = 1;
        city.districts.push_back({rules().district("DISTRICT_ENTERTAINMENT_COMPLEX"), {11, 5}, true});
        city.buildings.push_back(rules().building("BUILDING_ARENA"));
        std::sort(city.buildings.begin(), city.buildings.end());
        claimFor(s, city, {11, 5});
        s.players[0].relations.resize(2);
        s.players[1].relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = war;
        if (weariness > 0) s.players[0].warWeariness = {0, weariness};
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->cityReport(taken);
    };
    const int per = std::max(1, rules().globalInt("WAR_WEARINESS_POINTS_FOR_AMENITY_LOSS"));
    const int heavy = per * 20;
    const CityReport noneFounded = report(false, true);
    const CityReport noneOccupied = report(true, true);
    const CityReport noneConquered = report(true, false);
    CHECK_EQ(noneFounded.amenities, noneOccupied.amenities);  // no weariness: no loss in any case
    CHECK_EQ(noneFounded.amenities, noneConquered.amenities);
    CHECK(noneFounded.amenities > noneFounded.amenitiesNeeded);  // surplus so the floor is a stop, not a raise

    const CityReport founded = report(false, true, heavy);
    CHECK_EQ(founded.amenities, founded.amenitiesNeeded);  // founded: at its requirement

    const CityReport conquered = report(true, false, heavy);
    CHECK_EQ(conquered.amenities, conquered.amenitiesNeeded - rules().globalInt("WAR_WEARINESS_LOSS_OVER_REQ_AMENITIES_NONFOUNDED_CITY"));

    const CityReport occupied = report(true, true, heavy);
    CHECK_EQ(occupied.amenities, occupied.amenitiesNeeded - rules().globalInt("WAR_WEARINESS_LOSS_OVER_REQ_AMENITIES_AT_WAR_CITY"));

    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    addCity(s, 0, {4, 5}, true, 3);
    const CityId taken = addCity(s, 0, {10, 5}, false, 5);
    addCity(s, 1, {16, 5}, true, 3);
    s.city(taken)->originalOwner = 1;
    s.players[0].relations.resize(2);
    s.players[1].relations.resize(2);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->occupied(*g->state().city(taken)));
    CHECK(!g->occupied(g->state().cities[0]));
}

TEST(theocracy_buys_land_units_with_faith) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {5, 5}, true, 3);
    auto plain = Game::fromScenario(rules(), s);
    s.players[0].government = rules().government("GOVERNMENT_THEOCRACY");
    s.players[0].policies.assign(static_cast<size_t>(rules().governments[static_cast<size_t>(s.players[0].government)].totalSlots()), kNone);
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem warrior{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")};
    CHECK_EQ(plain->faithPurchaseCost(0, plain->state().cities[0], warrior), -1);
    // 15% under the Gold price (Theocracy's discount on Faith purchases), in fives like every price.
    CHECK_EQ(g->faithPurchaseCost(0, g->state().cities[0], warrior), g->purchaseCost(0, warrior) * 85 / 100 / 5 * 5);
}

TEST(support_units_and_open_ground) {
    GameState s = flatState(20, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const UnitId chariot = addUnit(s, "UNIT_HEAVY_CHARIOT", 0, {3, 3});
    const UnitId catapult = addUnit(s, "UNIT_CATAPULT", 0, {8, 8});
    const UnitId lone = addUnit(s, "UNIT_CATAPULT", 0, {14, 8});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {9, 8});
    auto g = Game::fromScenario(rules(), std::move(s));
    const UnitType& ct = rules().units[static_cast<size_t>(rules().unit("UNIT_HEAVY_CHARIOT"))];
    CHECK_EQ(g->maxMoves(*g->state().unit(chariot)), ct.moves + 1);  // flat grassland, no feature
    CHECK_EQ(g->unitRange(*g->state().unit(catapult)), g->unitRange(*g->state().unit(lone)) + 1);
}

// The balloon's +1 range (05: Support units) is for its own side's siege units beside it, and counts once however
// many balloons are beside one.
TEST(an_observation_balloon_helps_only_its_own_siege_units_beside_it) {
    GameState s = flatState(24, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const UnitId lone = addUnit(s, "UNIT_CATAPULT", 0, {2, 2});
    const UnitId twice = addUnit(s, "UNIT_CATAPULT", 0, {8, 4});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {7, 4});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {9, 4});
    const UnitId far = addUnit(s, "UNIT_CATAPULT", 0, {14, 4});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 0, {16, 4});  // two plots away
    const UnitId foreign = addUnit(s, "UNIT_CATAPULT", 0, {20, 8});
    addUnit(s, "UNIT_OBSERVATION_BALLOON", 1, {21, 8});  // another player's
    auto g = Game::fromScenario(rules(), std::move(s));
    const int base = g->unitRange(*g->state().unit(lone));
    CHECK_EQ(g->unitRange(*g->state().unit(twice)), base + 1);
    CHECK_EQ(g->unitRange(*g->state().unit(far)), base);
    CHECK_EQ(g->unitRange(*g->state().unit(foreign)), base);
}

TEST(a_captured_city_can_be_liberated) {
    GameState s = flatState(24, 12, 3);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    addCity(s, 0, {3, 5}, true, 3);
    const CityId freed = addCity(s, 0, {12, 5}, false, 3);
    addCity(s, 2, {20, 5}, true, 3);
    s.city(freed)->originalOwner = 2;
    s.city(freed)->capturedTurn = s.turn;
    for (Player& p : s.players) p.relations.resize(3);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->canLiberateCity(0, freed));
    REQUIRE(g->submit(Command::liberateCity(0, freed)) == CommandError::Ok);
    CHECK_EQ(g->state().city(freed)->owner, 2);
    CHECK_EQ(g->state().city(freed)->loyalty, rules().globalInt("LOYALTY_AFTER_TRANSFERRED_BY_LIBERATION"));
    CHECK_EQ(g->state().players[0].favor, rules().globalInt("FAVOR_FOR_LIBERATE_PLAYER_CITY"));
    CHECK(!g->canLiberateCity(2, freed));  // its own now
}

TEST(unhappy_cities_breed_rebels) {
    GameState s = flatState(20, 12, 2);
    s.players[1].barbarian = true;
    s.players[1].civ = kNone;
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId c = addCity(s, 0, {8, 6}, true, 4);
    s.city(c)->rebellion = 60;  // past 50 points a rising is certain (2% each)
    auto g = Game::fromScenario(rules(), std::move(s));
    const size_t units = g->state().units.size();
    sovtest::endTurns(*g, 1);
    CHECK(g->state().units.size() > units);  // the rebels
    // They carry the strongest melee arms their city's owner can field: with no techs, Warriors, never Lahore's
    // Nihang (25 strength and needing no tech, but the city-state's own; 08).
    int warriors = 0;
    for (const Unit& u : g->state().units) {
        if (u.owner != 1) continue;
        CHECK(u.type != rules().unit("UNIT_NIHANG"));
        warriors += u.type == rules().unit("UNIT_WARRIOR") ? 1 : 0;
    }
    CHECK(warriors > 0);
    CHECK_EQ(g->state().city(c)->rebellion, 0);
    CHECK(g->state().city(c)->rebellionCooldown > g->state().turn);
}

TEST(airlift_between_airports_and_paradrop) {
    GameState s = flatState(24, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const CityId a = addCity(s, 0, {4, 5}, true, 4);
    const CityId b = addCity(s, 0, {16, 5}, false, 4);
    for (const Hex& h : s.grid.within({4, 5}, 2)) s.plot(h).owner = 0, s.plot(h).city = a;
    for (const Hex& h : s.grid.within({16, 5}, 2)) s.plot(h).owner = 0, s.plot(h).city = b;
    const TypeIndex aerodrome = rules().district("DISTRICT_AERODROME");
    const TypeIndex airport = rules().building("BUILDING_AIRPORT");
    for (auto [id, pos] : {std::pair<CityId, Hex>{a, Hex{5, 5}}, {b, Hex{17, 5}}}) {
        City& c = *s.city(id);
        c.districts.push_back({aerodrome, pos, true});
        c.buildings.push_back(airport);
        std::sort(c.buildings.begin(), c.buildings.end());
    }
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_RAPID_DEPLOYMENT"))] = 1;
    s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    const UnitId soldier = addUnit(s, "UNIT_INFANTRY", 0, {5, 5});
    const UnitId commando = addUnit(s, "UNIT_SPEC_OPS", 0, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::airlift(0, soldier, {17, 5})) == CommandError::Ok);
    CHECK((g->state().unit(soldier)->pos == Hex{17, 5}));
    CHECK_EQ(g->validate(Command::paradrop(0, commando, {4, 10})), CommandError::BadTarget);  // four plots away
    REQUIRE(g->submit(Command::paradrop(0, commando, {6, 8})) == CommandError::Ok);
    CHECK((g->state().unit(commando)->pos == Hex{6, 8}));
}

TEST(submarines_hide_until_found) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    for (Plot& p : s.plots) p.terrain = rules().terrain("TERRAIN_COAST");
    s.players[1].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
    const UnitId sub = addUnit(s, "UNIT_SUBMARINE", 0, {10, 6});
    auto unseen = Game::fromScenario(rules(), s);
    CHECK(!unseen->unitVisibleTo(1, *unseen->state().unit(sub)));
    CHECK(unseen->unitVisibleTo(0, *unseen->state().unit(sub)));  // its owner
    addUnit(s, "UNIT_DESTROYER", 1, {12, 6});  // two plots away, within its sight
    auto found = Game::fromScenario(rules(), std::move(s));
    CHECK(found->unitVisibleTo(1, *found->state().unit(sub)));
}

TEST(camouflage_hides_a_unit_from_all_but_its_neighbours) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.players[1].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
    const UnitId ranger = addUnit(s, "UNIT_RANGER", 0, {10, 6});
    addUnit(s, "UNIT_WARRIOR", 1, {12, 6});  // two plots away
    auto plain = Game::fromScenario(rules(), s);
    CHECK(plain->unitVisibleTo(1, *plain->state().unit(ranger)));  // no promotion: seen as usual
    s.unit(ranger)->promotions.push_back(rules().promotion("PROMOTION_CAMOUFLAGE"));
    auto hidden = Game::fromScenario(rules(), s);
    CHECK(!hidden->unitVisibleTo(1, *hidden->state().unit(ranger)));
    CHECK(hidden->unitVisibleTo(0, *hidden->state().unit(ranger)));  // its owner
    addUnit(s, "UNIT_WARRIOR", 1, {11, 6});  // next to it
    auto found = Game::fromScenario(rules(), std::move(s));
    CHECK(found->unitVisibleTo(1, *found->state().unit(ranger)));
}
