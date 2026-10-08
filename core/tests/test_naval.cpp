// Ships, embarkation and the Harbor (05-units-and-combat.md, Embarkation; 03, Harbor).
#include <algorithm>

#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

void giveTech(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[static_cast<size_t>(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.techs.done[at(rules().tech(tech))] = 1;
}

// Land for x < 8, Coast for 8 <= x < 12, Ocean beyond.
GameState seaState(int players = 2) {
    GameState s = flatState(16, 12, players);
    for (int y = 0; y < 12; ++y) {
        for (int x = 8; x < 16; ++x) s.plot({x, y}).terrain = rules().terrain(x < 12 ? "TERRAIN_COAST" : "TERRAIN_OCEAN");
    }
    for (PlayerId p = 0; p < players; ++p) {
        Player& pl = s.players[static_cast<size_t>(p)];
        Game::fitPlayerToRules(pl, rules());
        pl.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));  // a mapped sea
    }
    return s;
}

const Unit& unit(const Game& g, UnitId id) { return *g.state().unit(id); }
}  // namespace

TEST(naval_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.techs[at(r.tech("TECH_SAILING"))].embarkUnit, r.unit("UNIT_BUILDER"));
    CHECK_EQ(r.techs[at(r.tech("TECH_CELESTIAL_NAVIGATION"))].embarkUnit, r.unit("UNIT_TRADER"));
    CHECK(r.techs[at(r.tech("TECH_SHIPBUILDING"))].embarkAll);
    CHECK(r.techs[at(r.tech("TECH_CARTOGRAPHY"))].ocean);
    CHECK_EQ(r.techs[at(r.tech("TECH_STEAM_POWER"))].embarkedMoves, 2);
    CHECK_EQ(r.eras[0].embarkedStrength, 10);
    CHECK_EQ(r.eras[3].embarkedStrength, 30);
    const DistrictType& harbor = r.districts[at(r.district("DISTRICT_HARBOR"))];
    CHECK(harbor.water);
    CHECK_EQ(r.buildings[at(r.building("BUILDING_LIGHTHOUSE"))].districtType, r.district("DISTRICT_HARBOR"));
}

TEST(land_units_embark_after_shipbuilding_and_reach_the_ocean_after_cartography) {
    GameState s = seaState();
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    {
        auto g = Game::fromScenario(rules(), GameState(s));
        CHECK(!g->findPath(w, {9, 5}));
        CHECK_EQ(g->submit(Command::move(0, w, {9, 5})), CommandError::NoPath);
    }
    giveTech(s, 0, "TECH_SHIPBUILDING");
    auto g = Game::fromScenario(rules(), GameState(s));
    CHECK(g->findPath(w, {9, 5}));
    CHECK(!g->findPath(w, {13, 5}));  // Ocean waits for Cartography
    // Embarking costs 2 plus the water tile, more than a Warrior's 2: it takes all the moves.
    CHECK_EQ(g->moveCost(unit(*g, w), {7, 5}, {8, 5}).value_or(Fixed()), Fixed::fromInt(3));
    REQUIRE(g->submit(Command::move(0, w, {8, 5})) == CommandError::Ok);
    CHECK_EQ(unit(*g, w).pos, (Hex{8, 5}));
    CHECK(g->isEmbarked(unit(*g, w)));
    CHECK_EQ(unit(*g, w).movesLeft, Fixed());
    CHECK_EQ(g->maxMoves(unit(*g, w)), 2);  // the embarked base
    giveTech(s, 0, "TECH_CARTOGRAPHY");
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK(g2->findPath(w, {13, 5}));
    // An overland order never embarks.
    CHECK(!g2->findPath(w, {9, 5}, true));
}

// Afloat, an overland order may still go by water (it is only kept from embarking). Once ashore it keeps dry, so an
// order that lands where only a way over the water is left ends at once, not on the next turn.
TEST(an_overland_order_that_lands_with_only_a_wet_way_left_ends_at_once) {
    // A land column (x = 6) between two strips of Coast, x = 4-5 and 7-8, from edge to edge; land beyond them.
    GameState s = flatState(12, 8, 2);
    for (int y = 0; y < 8; ++y) {
        for (int x : {4, 5, 7, 8}) s.plot({x, y}).terrain = rules().terrain("TERRAIN_COAST");
    }
    for (PlayerId p = 0; p < 2; ++p) {
        Player& pl = s.players[static_cast<size_t>(p)];
        Game::fitPlayerToRules(pl, rules());
        pl.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    }
    giveTech(s, 0, "TECH_SHIPBUILDING");
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {5, 3});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->isEmbarked(unit(*g, w)));
    REQUIRE(g->submit(Command::move(0, w, {9, 3}, true)) == CommandError::Ok);
    // Landing on the column takes all its moves; from there only the Coast beyond leads on.
    CHECK_EQ(unit(*g, w).pos.x, 6);
    CHECK(!g->isEmbarked(unit(*g, w)));
    CHECK_EQ(unit(*g, w).movesLeft, Fixed());
    CHECK(!unit(*g, w).moveTarget);
}

TEST(amphibious_units_embark_and_land_without_the_extra_cost) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SHIPBUILDING");
    const UnitId plain = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    const UnitId marine = addUnit(s, "UNIT_WARRIOR", 0, {7, 7});
    s.units.back().promotions = {rules().promotion("PROMOTION_AMPHIBIOUS")};
    auto g = Game::fromScenario(rules(), std::move(s));
    // Embarking or landing costs 2 on top of the plot entered; Amphibious waives it (05).
    CHECK_EQ(g->moveCost(unit(*g, plain), {7, 5}, {8, 5}).value_or(Fixed()), Fixed::fromInt(3));
    CHECK_EQ(g->moveCost(unit(*g, plain), {8, 5}, {7, 5}).value_or(Fixed()), Fixed::fromInt(3));
    CHECK_EQ(g->moveCost(unit(*g, marine), {7, 7}, {8, 7}).value_or(Fixed()), Fixed::fromInt(1));
    CHECK_EQ(g->moveCost(unit(*g, marine), {8, 7}, {7, 7}).value_or(Fixed()), Fixed::fromInt(1));
}

// A unit ability can waive it too (Global Army's free embark, given here to the Warrior).
TEST(an_ability_waives_the_embark_cost) {
    Rules r = rules();
    r.units[at(r.unit("UNIT_WARRIOR"))].abilities.push_back(r.ability("ABILITY_GLOBAL_ARMY"));
    GameState s = seaState();
    giveTech(s, 0, "TECH_SHIPBUILDING");
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    auto g = Game::fromScenario(r, std::move(s));
    CHECK_EQ(g->moveCost(unit(*g, w), {7, 5}, {8, 5}).value_or(Fixed()), Fixed::fromInt(1));
    CHECK_EQ(g->moveCost(unit(*g, w), {8, 5}, {7, 5}).value_or(Fixed()), Fixed::fromInt(1));
}

TEST(builders_embark_after_sailing) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SAILING");
    const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {7, 5});
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->findPath(b, {9, 5}));
    CHECK(!g->findPath(w, {9, 6}));
}

// A unit's move reach is every plot a move order for it finds a path to, overland or not, planned as the order is: for
// a linked escort on its leader's plot, as the leader's (a Builder stands in for the leader here: it embarks after
// Sailing, a Warrior not). An escort away from its leader, or linked to another player's unit, moves alone.
TEST(a_move_reach_is_where_a_move_order_finds_a_path) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SAILING");
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {7, 5});
    const UnitId escort = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    s.units.back().escorting = builder;
    const UnitId lone = addUnit(s, "UNIT_WARRIOR", 0, {6, 8});
    const UnitId apart = addUnit(s, "UNIT_WARRIOR", 0, {7, 10});
    s.units.back().escorting = builder;
    const UnitId theirs = addUnit(s, "UNIT_BUILDER", 1, {6, 2});
    const UnitId foreign = addUnit(s, "UNIT_WARRIOR", 0, {6, 2});
    s.units.back().escorting = theirs;
    auto g = Game::fromScenario(rules(), std::move(s));
    const HexGrid& grid = g->state().grid;
    const auto reaches = [&](UnitId id, Hex h, bool overland) { return g->moveReach(id, overland)[static_cast<size_t>(grid.index(h))] != 0; };
    for (bool overland : {false, true}) {
        for (const auto& [id, mover] : {std::pair{builder, builder}, std::pair{escort, builder}, std::pair{lone, lone}, std::pair{apart, apart},
                                        std::pair{foreign, foreign}}) {
            const std::vector<uint8_t> reach = g->moveReach(id, overland);
            REQUIRE(reach.size() == static_cast<size_t>(grid.size()));
            int wrong = 0;
            for (int i = 0; i < grid.size(); ++i) wrong += (reach[static_cast<size_t>(i)] != 0) != g->findPath(mover, grid.at(i), overland).has_value();
            CHECK_EQ(wrong, 0);
        }
        CHECK_EQ(reaches(escort, {9, 5}, overland), !overland);  // the pair goes to sea as the Builder does
        CHECK(!reaches(lone, {9, 8}, overland));
        CHECK(reaches(lone, {2, 8}, overland));
        CHECK(!reaches(apart, {9, 10}, overland));  // alone, the Warrior keeps to the land
        CHECK(reaches(foreign, {7, 5}, overland));  // its own units' plot, which the other player's units may not enter
    }
}

// A Builder passing one of ours on the shore embarks from that plot with what moves it has left, as from any other,
// but not with none: there it would end its move on the other one (05: Stacking). So it waits a plot short.
TEST(units_embark_past_our_units_only_with_moves_left) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SAILING");
    for (int y = 0; y < 12; ++y) {
        if (y != 6) s.plot({7, y}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    }
    const UnitId a = addUnit(s, "UNIT_BUILDER", 0, {7, 6});
    const UnitId b = addUnit(s, "UNIT_BUILDER", 0, {5, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const std::optional<std::vector<PathStep>> path = g->findPath(b, {8, 6});
    REQUIRE(path && path->size() == 4u);
    CHECK_EQ((*path)[1].turn, 0);  // (6,6)
    CHECK_EQ((*path)[2].turn, 1);  // past the other Builder, with 1 MP
    CHECK_EQ(path->back().turn, 1);
    // And it moves so: it waits beside the other Builder, then passes it and embarks with the 1 MP left.
    REQUIRE(g->submit(Command::move(0, b, {8, 6})) == CommandError::Ok);
    CHECK_EQ(unit(*g, b).pos, (Hex{6, 6}));
    REQUIRE(g->submit(Command::setActivity(0, a, Activity::Sleep)) == CommandError::Ok);
    sovtest::endTurns(*g, 2);
    CHECK_EQ(unit(*g, b).pos, (Hex{8, 6}));
}

TEST(embarked_movement_grows_with_techs) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SHIPBUILDING");
    giveTech(s, 0, "TECH_SQUARE_RIGGING");
    giveTech(s, 0, "TECH_STEAM_POWER");
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {9, 5});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->maxMoves(unit(*g, w)), 5);  // 2 + 1 + 2
}

TEST(the_great_lighthouse_speeds_ships_and_embarked_units) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SHIPBUILDING");
    addCity(s, 0, {7, 5}, true);
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {9, 5});
    const UnitId galley = addUnit(s, "UNIT_GALLEY", 0, {10, 6});
    auto g = Game::fromScenario(rules(), s);
    const int embarked = g->maxMoves(unit(*g, w)), sailing = g->maxMoves(unit(*g, galley));
    City& port = s.cities[0];
    port.buildings.push_back(rules().building("BUILDING_GREAT_LIGHTHOUSE"));
    std::sort(port.buildings.begin(), port.buildings.end());
    auto h = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(h->maxMoves(unit(*h, w)), embarked + 1);       // embarked units +1 movement (03)
    CHECK_EQ(h->maxMoves(unit(*h, galley)), sailing + 1);   // naval units +1 movement
}

TEST(ships_keep_to_the_water_and_put_into_port) {
    GameState s = seaState();
    addCity(s, 0, {7, 5}, true);
    const UnitId galley = addUnit(s, "UNIT_GALLEY", 0, {9, 5});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->findPath(galley, {11, 6}));
    CHECK(!g->findPath(galley, {12, 5}));  // no Cartography
    CHECK(!g->findPath(galley, {5, 5}));   // land
    CHECK(g->findPath(galley, {7, 5}));    // the coastal city is a port
}

TEST(coastal_cities_train_ships) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SAILING");
    const CityId port = addCity(s, 0, {7, 5}, true);
    const CityId inland = addCity(s, 0, {2, 5}, false);
    addUnit(s, "UNIT_WARRIOR", 0, {7, 5});  // the garrison holds the city plot
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem galley{ProductionKind::Unit, rules().unit("UNIT_GALLEY")};
    CHECK(g->isCoastalCity(*g->state().city(port)));
    CHECK(!g->isCoastalCity(*g->state().city(inland)));
    CHECK(g->canProduce(*g->state().city(port), galley));
    CHECK(!g->canProduce(*g->state().city(inland), galley));
    // With the city plot taken, the new ship waits on the water beside it.
    const auto spot = g->unitSpawnPlot(*g->state().city(port), galley.type);
    REQUIRE(spot.has_value());
    CHECK(rules().terrains[at(g->state().plot(*spot).terrain)].water);
}

TEST(embarked_units_defend_weakly_and_cannot_attack) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SHIPBUILDING");
    giveTech(s, 1, "TECH_SAILING");
    const UnitId swimmer = addUnit(s, "UNIT_SWORDSMAN", 0, {9, 5});
    const UnitId beach = addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
    const UnitId galley = addUnit(s, "UNIT_GALLEY", 1, {10, 5});
    const UnitId galley2 = addUnit(s, "UNIT_GALLEY", 1, {8, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    // An embarked Swordsman (35) defends with its owner's era strength: Shipbuilding is Classical.
    CHECK_EQ(g->playerEra(0), 1);
    CHECK_EQ(g->combatStrength(unit(*g, swimmer), unit(*g, galley), false, false), 15);
    CHECK_EQ(g->submit(Command::attack(0, swimmer, {10, 5})), CommandError::CannotAttack);
    // Land units do not attack ships at sea.
    CHECK_EQ(g->submit(Command::attack(0, beach, {8, 6})), CommandError::CannotAttack);
    (void)galley2;
}

TEST(ships_attack_on_the_water) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_SAILING");
    giveTech(s, 1, "TECH_SHIPBUILDING");
    const UnitId galley = addUnit(s, "UNIT_GALLEY", 0, {10, 5});
    const UnitId swimmer = addUnit(s, "UNIT_WARRIOR", 1, {9, 5});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    REQUIRE(g->submit(Command::attack(0, galley, {9, 5})) == CommandError::Ok);
    const Unit* w = g->state().unit(swimmer);
    CHECK(!w || w->hp < 100);
}

TEST(the_harbor_goes_on_the_coast_next_to_land) {
    GameState s = seaState();
    giveTech(s, 0, "TECH_CELESTIAL_NAVIGATION");
    const CityId c = addCity(s, 0, {7, 5}, true, 4);
    for (const Hex& h : s.grid.within({7, 5}, 3)) {
        s.plot(h).owner = 0;
        s.plot(h).city = c;
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex harbor = rules().district("DISTRICT_HARBOR");
    const City& city = *g->state().city(c);
    CHECK(g->canPlaceDistrict(city, harbor, {8, 5}));    // Coast next to the city
    CHECK(!g->canPlaceDistrict(city, harbor, {6, 5}));   // land
    CHECK(!g->canPlaceDistrict(city, harbor, {10, 5}));  // Coast with no land beside it
    // +2 Gold next to the City Center.
    CHECK(g->districtAdjacency(0, harbor, {8, 5})[static_cast<size_t>(YieldType::Gold)] >= Fixed::fromInt(2));
}
