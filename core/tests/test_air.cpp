// Air power (05-units-and-combat.md: air units, air combat; 03: Aerodrome).
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
ProductionItem unit(const char* id) { return {ProductionKind::Unit, rules().unit(id)}; }

// Player 0 owns a city at (4,6) with an Aerodrome at (5,7); player 1 a city at (14,6); at war.
GameState skies() {
    GameState s = flatState(24, 14, 2);
    addCity(s, 0, {4, 6}, true, 8);
    addCity(s, 1, {14, 6}, true, 6);
    s.cities[0].districts.push_back({rules().district("DISTRICT_AERODROME"), {5, 7}, true});
    s.plot({5, 7}).owner = 0;
    s.plot({5, 7}).city = s.cities[0].id;
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        for (const char* t : {"TECH_FLIGHT", "TECH_ADVANCED_FLIGHT", "TECH_ADVANCED_BALLISTICS"}) p.techs.done[at(rules().tech(t))] = 1;
        p.stockpile[at(rules().resource("RESOURCE_OIL"))] = 10;
        p.stockpile[at(rules().resource("RESOURCE_ALUMINUM"))] = 10;
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
        p.relations.resize(2);
    }
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    return s;
}
}  // namespace

TEST(air_units_need_a_free_air_slot) {
    GameState s = skies();
    auto g = Game::fromScenario(rules(), s);
    const City& c = g->state().cities[0];
    CHECK_EQ(g->airSlots(0, c.pos), 1);       // the City Center
    CHECK_EQ(g->airSlots(0, {5, 7}), 2);      // the Aerodrome
    CHECK_EQ(g->airSlots(1, {5, 7}), 0);      // not theirs
    CHECK(g->canProduce(c, unit("UNIT_FIGHTER")));  // (Advanced Flight makes the Biplane obsolete)
    REQUIRE(g->freeAirBase(c).has_value());
    CHECK(*g->freeAirBase(c) == c.pos);       // the center fills first
    // Three aircraft fill every slot: no fourth.
    for (int i = 0; i < 2; ++i) addUnit(s, "UNIT_BIPLANE", 0, {5, 7});
    addUnit(s, "UNIT_BIPLANE", 0, {4, 6});
    auto full = Game::fromScenario(rules(), std::move(s));
    CHECK(!full->canProduce(full->state().cities[0], unit("UNIT_FIGHTER")));
    // Aircraft are not a garrison: the city's military layer stays empty.
    CHECK(full->state().unitAt({4, 6}, UnitLayer::Military, rules()) == nullptr);
}

TEST(aircraft_rebase_instead_of_moving) {
    GameState s = skies();
    const UnitId plane = addUnit(s, "UNIT_BIPLANE", 0, {4, 6});
    addCity(s, 0, {4, 11}, false, 3);  // 5 away: within twice the Biplane's range of 4
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->submit(Command::move(0, plane, {6, 6})) == CommandError::BadTarget);
    CHECK(g->rebaseProblem(plane, {14, 6}) != CommandError::Ok);  // an enemy city
    REQUIRE(g->submit(Command::rebaseUnit(0, plane, {4, 11})) == CommandError::Ok);
    CHECK(g->state().unit(plane)->pos == (Hex{4, 11}));
    CHECK(g->state().unit(plane)->movesLeft == Fixed());
}

TEST(air_strikes_and_interception) {
    GameState s = skies();
    const UnitId plane = addUnit(s, "UNIT_BIPLANE", 0, {5, 7});  // range 4
    const UnitId target = addUnit(s, "UNIT_INFANTRY", 1, {8, 7});
    addUnit(s, "UNIT_SCOUT", 0, {7, 8});  // someone has to see the target
    auto g = Game::fromScenario(rules(), s);
    // No line of sight needed; the target takes the damage, the aircraft none.
    REQUIRE(g->submit(Command::rangedAttack(0, plane, {8, 7})) == CommandError::Ok);
    REQUIRE(g->state().unit(target) != nullptr);
    CHECK(g->state().unit(target)->hp < 100);
    CHECK_EQ(g->state().unit(plane)->hp, 100);
    // An anti-air gun beside the target shoots at the aircraft first.
    addUnit(s, "UNIT_ANTI_AIR_GUN", 1, {9, 7});
    auto g2 = Game::fromScenario(rules(), s);
    CHECK(g2->interception(*g2->state().unit(plane), {8, 7}).first > 0);
    REQUIRE(g2->submit(Command::rangedAttack(0, plane, {8, 7})) == CommandError::Ok);
    const Unit* hurt = g2->state().unit(plane);
    CHECK(!hurt || hurt->hp < 100);
    // A fighter on patrol covers its range; an idle one does not.
    GameState s3 = s;
    s3.units.erase(std::remove_if(s3.units.begin(), s3.units.end(), [](const Unit& u) { return u.type == rules().unit("UNIT_ANTI_AIR_GUN"); }), s3.units.end());
    const UnitId guard = addUnit(s3, "UNIT_BIPLANE", 1, {11, 7});
    auto g3 = Game::fromScenario(rules(), s3);
    CHECK_EQ(g3->interception(*g3->state().unit(plane), {8, 7}).first, 0);
    s3.unit(guard)->activity = Activity::Fortify;
    auto g4 = Game::fromScenario(rules(), std::move(s3));
    CHECK(g4->interception(*g4->state().unit(plane), {8, 7}).first > 0);
}

TEST(aircraft_are_lost_with_their_base) {
    GameState s = skies();
    const UnitId plane = addUnit(s, "UNIT_BIPLANE", 0, {5, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    City& c = g->stateMutForTests().cities[0];
    c.owner = 1;  // as if taken
    g->groundAircraft(c);
    CHECK(g->state().unit(plane) == nullptr);
}

TEST(carriers_carry_aircraft_to_sea) {
    GameState s = skies();
    for (int y = 0; y < 14; ++y) {
        for (int x = 6; x < 12; ++x) s.plot({x, y}).terrain = rules().terrain("TERRAIN_COAST");
    }
    s.plot({5, 7}).terrain = rules().terrain("TERRAIN_GRASS");
    const UnitId carrier = addUnit(s, "UNIT_AIRCRAFT_CARRIER", 0, {7, 3});
    const UnitId a = addUnit(s, "UNIT_FIGHTER", 0, {4, 6});
    const UnitId b = addUnit(s, "UNIT_FIGHTER", 0, {5, 7});
    addUnit(s, "UNIT_FIGHTER", 0, {5, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->airSlots(0, {7, 3}), 2);
    CHECK_EQ(g->airSlots(1, {7, 3}), 0);
    REQUIRE(g->submit(Command::rebaseUnit(0, a, {7, 3})) == CommandError::Ok);
    REQUIRE(g->submit(Command::rebaseUnit(0, b, {7, 3})) == CommandError::Ok);
    CHECK(g->rebaseProblem(g->state().units.back().id, {7, 3}) != CommandError::Ok);  // full
    // The carrier sails off with both aboard.
    REQUIRE(g->submit(Command::move(0, carrier, {8, 3})) == CommandError::Ok);
    CHECK(g->state().unit(carrier)->pos == (Hex{8, 3}));
    CHECK(g->state().unit(a)->pos == (Hex{8, 3}));
    CHECK(g->state().unit(b)->pos == (Hex{8, 3}));
    // Sunk, it takes them down with it.
    GameState after = g->state();
    after.units.erase(std::remove_if(after.units.begin(), after.units.end(), [&](const Unit& u) { return u.id == carrier; }), after.units.end());
    auto sunk = Game::fromScenario(rules(), std::move(after));
    sunk->checkAirBases();  // (runs after every command)
    CHECK(sunk->state().unit(a) == nullptr);
    CHECK(sunk->state().unit(b) == nullptr);
}

TEST(military_engineers_build_airstrips_and_forts) {
    GameState s = skies();
    const UnitId eng = addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {3, 5});
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {3, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex airstrip = rules().improvement("IMPROVEMENT_AIRSTRIP");
    CHECK(g->submit(Command::buildImprovement(0, builder, airstrip)) == CommandError::CannotImprove);
    CHECK(g->submit(Command::buildImprovement(0, eng, rules().improvement("IMPROVEMENT_FARM"))) == CommandError::CannotImprove);
    REQUIRE(g->submit(Command::buildImprovement(0, eng, airstrip)) == CommandError::Ok);
    CHECK_EQ(g->airSlots(0, {3, 5}), 3);
    CHECK_EQ(g->airSlots(1, {3, 5}), 0);
    CHECK_EQ(rules().improvements[at(rules().improvement("IMPROVEMENT_FORT"))].defense, 4);
}
