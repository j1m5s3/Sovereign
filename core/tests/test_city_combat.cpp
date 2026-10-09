// City combat (02-cities.md, City combat; 05-units-and-combat.md, Walls) and
// barbarians (01-map-and-terrain.md, Barbarians).
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
const Hex kCity{8, 5};

void addBuilding(GameState& s, CityId id, const char* building) {
    City& c = *s.city(id);
    c.buildings.push_back(rules().building(building));
    std::sort(c.buildings.begin(), c.buildings.end());
    c.wallHp += rules().buildings[at(rules().building(building))].outerDefenseHp;
}

// Player 1 holds a capital at (8,5); player 0 attacks it. `edit` sets the scene.
template <typename Edit>
std::unique_ptr<Game> siege(Edit edit, bool war = true, int players = 2) {
    GameState s = flatState(16, 12, players);
    addCity(s, 1, kCity, true, 4);
    edit(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    if (war) g->submit(Command::declareWar(0, 1));
    return g;
}

// Like siege, with player 2 as the barbarians.
template <typename Edit>
std::unique_ptr<Game> withBarbarians(Edit edit) {
    GameState s = flatState(16, 12, 3);
    s.players[2].barbarian = true;
    s.players[2].civ = kNone;
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}

const City& cityAt(const Game& g, Hex h) { return *g.state().cityAt(h); }
const Unit& unit(const Game& g, UnitId id) { return *g.state().unit(id); }

// A neighbour of the city other than `not`.
Hex besideCity(const GameState& s, Hex notHere) {
    for (const Hex& h : s.grid.within(kCity, 1)) {
        if (h != kCity && h != notHere) return h;
    }
    return kCity;
}
}  // namespace

TEST(city_strength_hp_and_walls) {
    UnitId warrior = kNoUnit;
    auto g = siege([&](GameState& s) { warrior = addUnit(s, "UNIT_WARRIOR", 1, {12, 10}); }, false);
    const City& c = cityAt(*g, kCity);
    CHECK_EQ(c.hp, 200);  // DISTRICT_CITY_CENTER
    CHECK_EQ(c.originalOwner, 1);
    CHECK(c.originalCapital);
    CHECK_EQ(g->cityMaxWallHp(c), 0);
    // Strongest unit (Warrior 20) - 10, + 3 Palace.
    CHECK_EQ(g->cityStrength(c), 13);

    auto walled = siege([&](GameState& s) {
        addUnit(s, "UNIT_WARRIOR", 1, {12, 10});
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
    }, false);
    CHECK_EQ(walled->cityStrength(cityAt(*walled, kCity)), 16);  // + 3 per wall level
    CHECK_EQ(walled->cityMaxWallHp(cityAt(*walled, kCity)), 100);

    // A garrison stronger than the derived value sets the base.
    auto garrisoned = siege([&](GameState& s) { addUnit(s, "UNIT_SWORDSMAN", 1, kCity); }, false);
    CHECK_EQ(garrisoned->cityStrength(cityAt(*garrisoned, kCity)), 38);

    // Damage: -1 per 10% of HP lost.
    auto hurt = siege([&](GameState& s) {
        addUnit(s, "UNIT_WARRIOR", 1, {12, 10});
        s.city(1)->hp = 100;
    }, false);
    CHECK_EQ(hurt->cityStrength(cityAt(*hurt, kCity)), 8);
}

TEST(walls_take_hits_first_by_attack_type) {
    UnitId warrior = kNoUnit, archer = kNoUnit, catapult = kNoUnit;
    auto g = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        archer = addUnit(s, "UNIT_ARCHER", 0, {6, 5});
        catapult = addUnit(s, "UNIT_CATAPULT", 0, {10, 5});
    });
    CombatPreview melee = g->previewAttack(warrior, kCity, false);
    REQUIRE(melee.valid);
    CHECK(melee.hitsWalls && melee.city == 1);
    const int diff = melee.attackerStrength - melee.defenderStrength;
    CHECK_EQ(melee.damageToDefenderMax, (g->combatDamage(diff, 12) * 15 + 50) / 100);  // melee 15%
    CHECK_EQ(g->submit(Command::attack(0, warrior, kCity)), CommandError::Ok);
    const City& c = cityAt(*g, kCity);
    CHECK_EQ(c.hp, 200);
    CHECK(100 - c.wallHp >= melee.damageToDefenderMin && 100 - c.wallHp <= melee.damageToDefenderMax);
    CHECK(unit(*g, warrior).hp < 100);  // the city hits back
    CHECK_EQ(unit(*g, warrior).pos, (Hex{7, 5}));
    CHECK_EQ(unit(*g, warrior).xp, rules().globalInt("EXPERIENCE_UNIT_VS_DISTRICT_NOT_CITY_CAPTURED"));
    CHECK_EQ(c.lastAttackedTurn, g->state().turn);

    CombatPreview ranged = g->previewAttack(archer, kCity, true);
    REQUIRE(ranged.valid);
    const int rdiff = ranged.attackerStrength - ranged.defenderStrength;
    CHECK_EQ(ranged.damageToDefenderMin, (g->combatDamage(rdiff, 0) * 50 + 50) / 100);  // ranged 50%
    CHECK_EQ(ranged.damageToAttackerMax, 0);
    // Ranged units fight districts at -17.
    CHECK_EQ(ranged.attackerStrength, 25 - 17);

    CombatPreview bombard = g->previewAttack(catapult, kCity, true);
    REQUIRE(bombard.valid);
    CHECK_EQ(bombard.damageToDefenderMin, g->combatDamage(bombard.attackerStrength - bombard.defenderStrength, 0));
    CHECK_EQ(bombard.attackerStrength, 35);  // siege keeps full strength against cities
}

TEST(medieval_walls_rams_and_siege_towers) {
    UnitId warrior = kNoUnit;
    auto stone = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        addBuilding(s, 1, "BUILDING_MEDIEVAL_WALLS");
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
    });
    CombatPreview pv = stone->previewAttack(warrior, kCity, false);
    CHECK(pv.hitsWalls);
    CHECK_EQ(pv.damageToDefenderMax, 0);  // melee cannot damage Medieval Walls

    auto rammed = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        addUnit(s, "UNIT_BATTERING_RAM", 0, besideCity(s, {7, 5}));
    });
    pv = rammed->previewAttack(warrior, kCity, false);
    CHECK(pv.hitsWalls);
    CHECK_EQ(pv.damageToDefenderMin, rammed->combatDamage(pv.attackerStrength - pv.defenderStrength, 0));

    auto towered = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        addBuilding(s, 1, "BUILDING_MEDIEVAL_WALLS");
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        addUnit(s, "UNIT_SIEGE_TOWER", 0, besideCity(s, {7, 5}));
    });
    pv = towered->previewAttack(warrior, kCity, false);
    CHECK(!pv.hitsWalls);  // straight at the city
    CHECK_EQ(towered->submit(Command::attack(0, warrior, kCity)), CommandError::Ok);
    CHECK(cityAt(*towered, kCity).hp < 200);
    CHECK_EQ(cityAt(*towered, kCity).wallHp, 200);
}

// Akkad (08: suzerain): its suzerain's melee and anti-cavalry units hit walls in full, as beside a Battering Ram.
TEST(akkads_suzerain_hits_walls_in_full) {
    const auto full = [](const char* cityState, const char* attacker) {
        UnitId u = kNoUnit;
        auto g = siege([&](GameState& s) {
            addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
            u = addUnit(s, attacker, 0, {7, 5});
            s.players[2].civ = kNone;
            s.players[2].cityState = rules().cityState(cityState);
            addCity(s, 2, {2, 10}, true);
            for (Player& p : s.players) {
                Game::fitPlayerToRules(p, rules());
                p.envoys.assign(s.players.size(), 0);
            }
            s.players[0].envoys[2] = 3;
        }, true, 3);
        const CombatPreview pv = g->previewAttack(u, kCity, false);
        REQUIRE(pv.hitsWalls);
        return pv.damageToDefenderMin == g->combatDamage(pv.attackerStrength - pv.defenderStrength, 0);
    };
    CHECK(full("CITYSTATE_AKKAD", "UNIT_WARRIOR"));
    CHECK(full("CITYSTATE_AKKAD", "UNIT_SPEARMAN"));
    CHECK(!full("CITYSTATE_AKKAD", "UNIT_HORSEMAN"));  // light cavalry: the usual 15%
    CHECK(!full("CITYSTATE_NAZCA", "UNIT_WARRIOR"));
}

TEST(melee_takes_a_beaten_city) {
    UnitId warrior = kNoUnit;
    CityId second = kNoCity;
    auto g = siege([&](GameState& s) {
        s.city(1)->hp = 1;
        second = addCity(s, 1, {8, 10}, false, 2);
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        addCity(s, 2, {14, 2}, true);  // a third civ, so taking one capital is not Domination
    }, true, 3);
    CHECK_EQ(g->submit(Command::attack(0, warrior, kCity)), CommandError::Ok);
    const City& c = cityAt(*g, kCity);
    CHECK_EQ(c.owner, 0);
    CHECK_EQ(c.population, 3);  // 25% lost
    CHECK_EQ(c.hp, 100);        // CITY_CAPTURED_DAMAGE_PERCENTAGE
    CHECK_EQ(c.wallHp, 0);
    CHECK(!c.capital && !c.has(rules().building("BUILDING_PALACE")));
    CHECK_EQ(c.capturedTurn, g->state().turn);
    CHECK(unit(*g, warrior).pos == kCity);
    CHECK_EQ(unit(*g, warrior).xp, rules().globalInt("EXPERIENCE_CITY_CAPTURED"));
    for (const Hex& h : g->state().grid.within(kCity, 1)) CHECK_EQ(g->state().plot(h).owner, 0);
    // The Palace moves to the loser's remaining city; the original capital cannot be razed.
    const City& rest = *g->state().city(second);
    CHECK(rest.capital && rest.has(rules().building("BUILDING_PALACE")));
    CHECK(g->state().players[1].alive);
    CHECK_EQ(g->submit(Command::razeCity(0, 1)), CommandError::CannotRaze);
}

TEST(ranged_units_cannot_take_a_city) {
    UnitId archer = kNoUnit, warrior = kNoUnit;
    auto g = siege([&](GameState& s) {
        s.city(1)->hp = 1;
        archer = addUnit(s, "UNIT_ARCHER", 0, {6, 5});
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {9, 5});
    });
    CHECK_EQ(g->submit(Command::rangedAttack(0, archer, kCity)), CommandError::Ok);
    CHECK_EQ(cityAt(*g, kCity).hp, 0);
    CHECK_EQ(cityAt(*g, kCity).owner, 1);
    // A city at 0 HP falls to the next melee unit without a fight.
    CombatPreview pv = g->previewAttack(warrior, kCity, false);
    CHECK(pv.valid && pv.captureCity);
    CHECK_EQ(g->submit(Command::attack(0, warrior, kCity)), CommandError::Ok);
    CHECK_EQ(cityAt(*g, kCity).owner, 0);
    CHECK_EQ(unit(*g, warrior).hp, 100);
}

// Ships take coastal cities too (02: City combat): a Galley beats a city beside the water and sails into it.
TEST(a_ship_takes_a_beaten_coastal_city) {
    UnitId galley = kNoUnit;
    auto g = siege([&](GameState& s) {
        s.city(1)->hp = 1;
        for (int y = 0; y < 12; ++y) s.plot({9, y}).terrain = rules().terrain("TERRAIN_COAST");
        galley = addUnit(s, "UNIT_GALLEY", 0, {9, 5});
        addCity(s, 1, {3, 10}, false);
        addCity(s, 2, {14, 2}, true);  // a third civ, so taking one capital is not Domination
    }, true, 3);
    CHECK_EQ(g->submit(Command::attack(0, galley, kCity)), CommandError::Ok);
    CHECK_EQ(cityAt(*g, kCity).owner, 0);
    CHECK(unit(*g, galley).pos == kCity);
}

// A pillaged district adds nothing to its city's strength (02: City combat).
TEST(a_pillaged_district_adds_no_city_strength) {
    auto g = siege([&](GameState& s) {
        addUnit(s, "UNIT_WARRIOR", 1, {12, 10});
        CityDistrict camp;
        camp.type = rules().district("DISTRICT_ENCAMPMENT");
        camp.pos = {10, 5};
        camp.complete = true;
        s.city(1)->districts.push_back(camp);
        sovtest::claimFor(s, *s.city(1), camp.pos);
    }, false);
    CHECK_EQ(g->cityStrength(cityAt(*g, kCity)), 13 + 2);  // the Encampment's +2
    GameState s = g->state();
    s.city(1)->districts[0].pillagedTurns = 1;
    auto pillaged = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(pillaged->cityStrength(cityAt(*pillaged, kCity)), 13);
}

TEST(captured_city_can_be_razed_that_turn) {
    UnitId warrior = kNoUnit;
    auto g = siege([&](GameState& s) {
        addCity(s, 1, {3, 10}, false);  // a later city, not the capital
        s.city(2)->hp = 1;
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {2, 10});
    });
    CHECK_EQ(g->submit(Command::razeCity(0, 2)), CommandError::NotYourCity);
    CHECK_EQ(g->submit(Command::attack(0, warrior, {3, 10})), CommandError::Ok);
    CHECK_EQ(g->state().city(2)->owner, 0);
    CHECK(g->canRazeCity(0, 2));
    CHECK_EQ(g->submit(Command::razeCity(0, 2)), CommandError::Ok);
    CHECK(g->state().city(2) == nullptr);
    for (const Hex& h : g->state().grid.within({3, 10}, 1)) {
        CHECK_EQ(g->state().plot(h).owner, kNoPlayer);
        CHECK_EQ(g->state().plot(h).city, kNoCity);
    }
}

TEST(losing_the_last_city_eliminates_a_player) {
    UnitId warrior = kNoUnit, theirs = kNoUnit;
    auto g = siege([&](GameState& s) {
        s.city(1)->hp = 1;
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        theirs = addUnit(s, "UNIT_WARRIOR", 1, {14, 10});
        addCity(s, 2, {14, 2}, true);  // a third civ keeps the game going (no Domination yet)
    }, true, 3);
    CHECK_EQ(g->submit(Command::attack(0, warrior, kCity)), CommandError::Ok);
    CHECK(!g->state().players[1].alive);
    CHECK(g->state().unit(theirs) == nullptr);
    g->submit(Command::setProduction(0, 1, {ProductionKind::Building, rules().building("BUILDING_MONUMENT")}));
    // Turns now skip the eliminated player.
    endTurns(*g, 2);
    CHECK_EQ(g->state().currentPlayer, 0);
    CHECK_EQ(g->state().turn, 2);
}

TEST(walled_city_strikes_once_per_turn) {
    UnitId enemy = kNoUnit;
    GameState s = flatState(16, 12, 2);
    const CityId mine = addCity(s, 0, {4, 5}, true);
    const CityId bare = addCity(s, 0, {4, 9}, false);
    addBuilding(s, mine, "BUILDING_ANCIENT_WALLS");
    enemy = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    s.units.back().activity = Activity::Sleep;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::cityStrike(0, mine, {6, 5})), CommandError::CannotStrike);  // not at war
    g->submit(Command::declareWar(0, 1));
    CHECK_EQ(g->submit(Command::cityStrike(0, bare, {6, 5})), CommandError::CannotStrike);  // no walls
    CHECK_EQ(g->submit(Command::cityStrike(0, mine, {7, 5})), CommandError::CannotStrike);  // nothing there
    CHECK_EQ(g->submit(Command::cityStrike(1, mine, {6, 5})), CommandError::NotYourTurn);
    CHECK_EQ(g->submit(Command::cityStrike(0, mine, {6, 5})), CommandError::Ok);
    const int hp = unit(*g, enemy).hp;
    CHECK(hp < 100);
    CHECK_EQ(g->submit(Command::cityStrike(0, mine, {6, 5})), CommandError::CannotStrike);
    endTurns(*g, 2);
    CHECK_EQ(g->submit(Command::cityStrike(0, mine, {6, 5})), CommandError::Ok);
}

TEST(cities_heal_and_walls_wait_for_the_repair_project) {
    GameState s = flatState(16, 12, 2);
    const CityId id = addCity(s, 0, {4, 5}, true);
    addBuilding(s, id, "BUILDING_ANCIENT_WALLS");
    s.city(id)->hp = 150;
    s.city(id)->wallHp = 50;
    s.city(id)->lastAttackedTurn = 1;
    CityDistrict camp;
    camp.type = rules().district("DISTRICT_ENCAMPMENT");
    camp.pos = {6, 5};
    camp.complete = true;
    camp.wallDamage = 30;
    s.city(id)->districts.push_back(camp);
    sovtest::claimFor(s, *s.city(id), camp.pos);
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem repair{ProductionKind::Project, rules().project("PROJECT_REPAIR_OUTER_DEFENSES")};
    endTurns(*g, 2);  // turn 2
    CHECK_EQ(g->state().city(id)->hp, 170);
    CHECK(!g->canProduce(*g->state().city(id), repair));  // within COMBAT_HEAL_OUTER_DEFENSES_COOLDOWN (3) turns of the attack
    endTurns(*g, 4);  // turn 4: three turns on
    CHECK(!g->canProduce(*g->state().city(id), repair));
    endTurns(*g, 2);  // turn 5
    CHECK_EQ(g->state().city(id)->hp, 200);
    CHECK_EQ(g->state().city(id)->wallHp, 50);  // walls never mend on their own
    CHECK_EQ(g->state().city(id)->districts[0].wallDamage, 30);
    REQUIRE(g->canProduce(*g->state().city(id), repair));
    REQUIRE(g->submit(Command::setProduction(0, id, repair)) == CommandError::Ok);
    endTurns(*g, 2);
    CHECK_EQ(g->state().city(id)->wallHp, 100);  // the project mends the city's walls and its Encampment's
    CHECK_EQ(g->state().city(id)->districts[0].wallDamage, 0);
    CHECK(!g->canProduce(*g->state().city(id), repair));  // offered only while walls are down
    // The Encampment's walls alone down: offered too.
    GameState s3 = g->state();
    s3.city(id)->districts[0].wallDamage = 10;
    auto g3 = Game::fromScenario(rules(), std::move(s3));
    CHECK(g3->canProduce(*g3->state().city(id), repair));

    // Surrounded by enemies: no healing.
    GameState s2 = flatState(16, 12, 2);
    const CityId besieged = addCity(s2, 0, {4, 5}, true);
    s2.city(besieged)->hp = 150;
    for (const Hex& h : s2.grid.within({4, 5}, 1)) {
        if (h == Hex{4, 5}) continue;
        addUnit(s2, "UNIT_WARRIOR", 1, h);
        s2.units.back().activity = Activity::Sleep;
    }
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    g2->submit(Command::declareWar(0, 1));
    CHECK(g2->cityUnderSiege(*g2->state().city(besieged)));
    endTurns(*g2, 2);
    CHECK_EQ(g2->state().city(besieged)->hp, 150);
}

TEST(discipline_and_the_barbarian_xp_cap) {
    UnitId mine = kNoUnit, veteran = kNoUnit, barb = kNoUnit, barb2 = kNoUnit, rival = kNoUnit;
    auto g = withBarbarians([&](GameState& s) {
        mine = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        barb = addUnit(s, "UNIT_WARRIOR", 2, {6, 5});
        rival = addUnit(s, "UNIT_WARRIOR", 1, {5, 8});
        veteran = addUnit(s, "UNIT_WARRIOR", 0, {10, 5});
        s.units.back().promotions.push_back(rules().promotion("PROMOTION_BATTLECRY"));
        barb2 = addUnit(s, "UNIT_WARRIOR", 2, {11, 5});
        Player& p = s.players[0];
        p.government = rules().government("GOVERNMENT_CHIEFDOM");
        p.policies = {rules().policy("POLICY_DISCIPLINE"), kNone};
    });
    CHECK(g->atWar(0, 2) && g->atWar(2, 1));
    CHECK_EQ(g->submit(Command::declareWar(0, 2)), CommandError::CannotDeclareWar);
    CHECK_EQ(g->submit(Command::makePeace(0, 2)), CommandError::CannotMakePeace);
    // Discipline: +5 against barbarians only.
    CHECK_EQ(g->combatStrength(unit(*g, mine), unit(*g, barb), true, false), 25);
    CHECK_EQ(g->combatStrength(unit(*g, mine), unit(*g, rival), true, false), 20);
    CHECK_EQ(g->submit(Command::attack(0, mine, {6, 5})), CommandError::Ok);
    CHECK(unit(*g, mine).xp > 0);
    if (g->state().unit(barb)) CHECK_EQ(unit(*g, barb).xp, 0);  // barbarians never gain XP
    // A level-2 unit learns nothing more from barbarians (EXPERIENCE_MAX_BARB_LEVEL).
    CHECK_EQ(g->submit(Command::attack(0, veteran, {11, 5})), CommandError::Ok);
    if (g->state().unit(veteran)) CHECK_EQ(unit(*g, veteran).xp, 0);
    (void)barb2;
}

TEST(entering_a_camp_clears_it_for_gold) {
    UnitId warrior = kNoUnit;
    auto g = withBarbarians([&](GameState& s) {
        warrior = addUnit(s, "UNIT_WARRIOR", 0, {7, 5});
        Camp c;
        c.id = s.nextCampId++;
        c.pos = {8, 5};
        c.tribe = static_cast<TypeIndex>(rules().barbarianTribes.size() - 1);
        c.spawnTimer = 99;
        s.camps.push_back(c);
    });
    CHECK(!g->canFoundCityAt(0, {8, 5}));
    const Fixed gold = g->state().players[0].gold;
    CHECK_EQ(g->submit(Command::move(0, warrior, {8, 5})), CommandError::Ok);
    CHECK(g->state().camps.empty());
    CHECK_EQ(g->state().players[0].gold, gold + Fixed::fromInt(rules().globalInt("BARBARIAN_CAMP_CLEAR_GOLD")));
}

TEST(barbarians_attack_but_never_take_cities) {
    UnitId barb = kNoUnit;
    CityId city = kNoCity;
    auto g = withBarbarians([&](GameState& s) {
        city = addCity(s, 0, {4, 5}, true);
        s.city(city)->hp = 1;
        barb = addUnit(s, "UNIT_WARRIOR", 2, {5, 5});  // no camp: roams at full boldness
    });
    endTurns(*g, 2);  // the world turn: the barbarian strikes the city
    const City& c = *g->state().city(city);
    CHECK_EQ(c.owner, 0);
    CHECK_EQ(c.lastAttackedTurn, 2);
    CHECK_EQ(c.hp, rules().globalInt("COMBAT_HEAL_CITY_GARRISON"));  // fell to 0, then healed
    REQUIRE(g->state().unit(barb));
    CHECK_EQ(unit(*g, barb).pos, (Hex{5, 5}));
    CHECK(unit(*g, barb).hp < 100);
}

TEST(barbarians_capture_settlers_as_builders) {
    UnitId settler = kNoUnit, barb = kNoUnit;
    auto g = withBarbarians([&](GameState& s) {
        settler = addUnit(s, "UNIT_SETTLER", 0, {5, 5});
        s.units.back().activity = Activity::Sleep;
        barb = addUnit(s, "UNIT_WARRIOR", 2, {6, 5});
    });
    endTurns(*g, 2);
    REQUIRE(g->state().unit(settler));
    CHECK_EQ(unit(*g, settler).owner, 2);
    CHECK_EQ(unit(*g, settler).type, rules().unit("UNIT_BUILDER"));
    CHECK_EQ(unit(*g, barb).pos, (Hex{5, 5}));
    CHECK(!g->state().players[0].alive);  // no city and no units left
}

TEST(barbarian_camps_appear_out_of_sight_and_release_units) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(11);
    setup.mapSize = "MAPSIZE_SMALL";
    setup.cityStates = 0;
    setup.players = {{"CIVILIZATION_ROME", true}, {"CIVILIZATION_EGYPT", false}, {"CIVILIZATION_CHINA", false},
                     {"CIVILIZATION_INCA", false}};
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    const PlayerId bp = g->barbarianPlayer();
    CHECK_EQ(bp, 4);
    CHECK(g->state().camps.empty());
    auto play = [&](int turns) {
        for (int i = 0; i < turns; ++i) {
            const PlayerId me = g->state().currentPlayer;
            for (UnitId id : g->unitsNeedingOrders(me)) g->submit(Command::setActivity(me, id, Activity::Sleep));
            endTurns(*g, 1);
        }
    };
    play(4);  // the first world turn adds a third of the target (3 per major)
    const auto& camps = g->state().camps;
    CHECK_EQ(camps.size(), 3u);  // 12 x 33%
    for (const Camp& c : camps) {
        CHECK(g->state().plot(c.pos).owner == kNoPlayer);
        for (PlayerId p = 0; p < 4; ++p) CHECK(g->visibility(p, c.pos) != Visibility::Visible);
        int released = 0;
        for (const Unit& u : g->state().units) {
            if (u.camp != c.id) continue;
            ++released;
            CHECK_EQ(u.owner, bp);
            // Each new camp first sends out a Scout (01: Barbarians), which ranges up to 10 plots.
            CHECK(g->rules().units[static_cast<size_t>(u.type)].id == "UNIT_SCOUT");
            CHECK(g->state().grid.distance(u.pos, c.pos) <= 11);
        }
        CHECK_EQ(released, 1);
        CHECK(!c.alerted);
    }
    for (size_t i = 0; i < camps.size(); ++i) {
        for (size_t j = i + 1; j < camps.size(); ++j)
            CHECK(g->state().grid.distance(camps[i].pos, camps[j].pos) >=
                  rules().globalInt("BARBARIAN_CAMP_MINIMUM_DISTANCE_ANOTHER_CAMP"));
    }
    // Barbarians play inside the world turn, so the command log still replays exactly.
    play(80);
    auto again = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(again);
    CHECK_EQ(again->stateHash(), g->stateHash());
}

// ---- the Encampment (03: Defense)

TEST(an_encampment_strikes_and_holds_ground) {
    UnitId foe = kNoUnit;
    auto g = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        CityDistrict camp;
        camp.type = rules().district("DISTRICT_ENCAMPMENT");
        camp.pos = {10, 5};
        camp.complete = true;
        s.cities[0].districts.push_back(camp);
        s.plot({10, 5}).owner = 1;
        s.plot({10, 5}).city = s.cities[0].id;
        foe = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {12, 5});  // two from the Encampment, four from the city
        for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
    });
    REQUIRE(g->submit(Command::setActivity(0, foe, Activity::Skip)) == CommandError::Ok);
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    const CityId cid = g->state().cities[0].id;
    CHECK(!g->canCityStrike(cid, {12, 5}));
    REQUIRE(g->canEncampmentStrike(cid, {12, 5}));
    REQUIRE(g->submit(Command::encampmentStrike(1, cid, {12, 5})) == CommandError::Ok);
    CHECK(g->state().unit(foe)->hp < 100);
    CHECK(!g->canEncampmentStrike(cid, {12, 5}));  // once a turn
    // Zone of control next to it, which path planning reads too, and +2 city strength.
    CHECK(g->inEnemyZoc(*g->state().unit(foe), {11, 5}));
    const std::optional<std::vector<PathStep>> path = g->findPath(foe, {11, 5});
    REQUIRE(path && path->size() == 2u);
    CHECK_EQ(path->back().movesLeft, Fixed());
    GameState s = g->state();
    s.cities[0].districts.clear();
    auto bare = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->cityStrength(g->state().cities[0]), bare->cityStrength(bare->state().cities[0]) + 2);
}

// A planned path ends its move beside an Encampment only as its zone of control has it: a finished one, of a civ at
// war with the mover (03: Defense).
TEST(an_encampment_stops_planned_moves_only_finished_and_at_war) {
    for (int c = 0; c < 4; ++c) {  // at war; unfinished; at peace; the Encampment's own civ's unit
        UnitId mover = kNoUnit;
        auto g = siege([&](GameState& s) {
            CityDistrict camp;
            camp.type = rules().district("DISTRICT_ENCAMPMENT");
            camp.pos = {10, 5};
            camp.complete = c != 1;
            s.cities[0].districts.push_back(camp);
            s.plot({10, 5}).owner = 1;
            s.plot({10, 5}).city = s.cities[0].id;
            mover = sovtest::addUnit(s, "UNIT_WARRIOR", c == 3 ? 1 : 0, {12, 5});
            for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
        }, c != 2);
        const std::optional<std::vector<PathStep>> path = g->findPath(mover, {11, 5});
        REQUIRE(path && path->size() == 2u);
        CHECK_EQ(path->back().movesLeft, c == 0 ? Fixed() : Fixed::fromInt(1));
    }
}

TEST(a_coastal_camp_puts_ships_to_sea) {
    auto g = withBarbarians([](GameState& s) {
        for (int y = 0; y < 12; ++y) s.plot({12, y}).terrain = rules().terrain("TERRAIN_COAST");
        for (Player& p : s.players) {
            Game::fitPlayerToRules(p, rules());
            p.techs.done[at(rules().tech("TECH_SAILING"))] = 1;
        }
        TypeIndex naval = kNone;
        for (size_t t = 0; t < rules().barbarianTribes.size(); ++t) {
            if (rules().barbarianTribes[t].coastal) naval = static_cast<TypeIndex>(t);
        }
        Camp camp;
        camp.id = s.nextCampId++;
        camp.pos = {11, 5};
        camp.tribe = naval;
        camp.spawnTimer = 1;
        s.camps.push_back(camp);
    });
    REQUIRE(g->state().camps.size() == 1u);
    sovtest::endTurns(*g, 2);  // both majors end their turns: the world turn follows
    const Unit* ship = nullptr;
    for (const Unit& u : g->state().units) {
        if (u.camp == g->state().camps[0].id) ship = &u;
    }
    REQUIRE(ship);
    CHECK(g->rules().units[at(ship->type)].domain == Domain::Sea);
    CHECK(g->rules().terrains[at(g->state().plot(ship->pos).terrain)].shallowWater);
}

TEST(a_camp_raids_only_once_its_scout_brings_word_of_a_city) {
    UnitId scout = kNoUnit, raider = kNoUnit;
    auto g = withBarbarians([&](GameState& s) {
        addCity(s, 0, {3, 5}, true, 3);
        Camp camp;
        camp.id = s.nextCampId++;
        camp.pos = {11, 5};
        camp.tribe = 0;
        camp.spawnTimer = 99;
        camp.alerted = false;
        camp.boldness = 500;  // bold enough for anything, once alerted
        s.camps.push_back(camp);
        scout = addUnit(s, "UNIT_SCOUT", 2, {5, 5});  // within sight of the city
        s.units.back().camp = camp.id;
        raider = addUnit(s, "UNIT_WARRIOR", 2, {10, 5});
        s.units.back().camp = camp.id;
    });
    REQUIRE(!g->state().camps[0].alerted);
    sovtest::endTurns(*g, 2);  // the world turn: the Scout sees the city and heads home
    CHECK(g->state().camps[0].scoutSaw || g->state().camps[0].alerted);
    CHECK(g->state().grid.distance(g->state().unit(raider)->pos, {10, 5}) <= 1);  // no raid yet
    for (int i = 0; i < 6 && !g->state().camps[0].alerted; ++i) sovtest::endTurns(*g, 2);
    CHECK(g->state().camps[0].alerted);
    REQUIRE(g->state().unit(scout));
    CHECK(g->state().grid.distance(g->state().unit(scout)->pos, {11, 5}) <= 1);
}

TEST(an_encampment_is_fought_and_falls) {
    UnitId sword = kNoUnit, archer = kNoUnit, guard = kNoUnit;
    auto setup = [&](GameState& s) {
        CityDistrict camp;
        camp.type = rules().district("DISTRICT_ENCAMPMENT");
        camp.pos = {10, 5};
        camp.complete = true;
        s.cities[0].districts.push_back(camp);
        s.plot({10, 5}).owner = 1;
        s.plot({10, 5}).city = s.cities[0].id;
        guard = addUnit(s, "UNIT_WARRIOR", 1, {10, 5});  // standing in it: the Encampment fights, not the guard
        sword = addUnit(s, "UNIT_SWORDSMAN", 0, {11, 5});
        archer = addUnit(s, "UNIT_ARCHER", 0, {12, 5});
        s.players[1].strongestUnit = 45;  // a city strength of 35: it takes more than one exchange
        for (Unit& u : s.units) u.activity = Activity::Sleep;
        for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
    };
    auto g = siege(setup);
    const CityId cid = g->state().cities[0].id;
    REQUIRE(g->encampmentTargetAt({10, 5}) != nullptr);
    CHECK_EQ(g->encampmentHp(g->state().cities[0]), 100);
    CHECK(!g->moveCost(*g->state().unit(sword), {11, 5}, {10, 5}));  // it is not walked into
    const CombatPreview p = g->previewAttack(sword, {10, 5}, false);
    REQUIRE(p.valid);
    CHECK(p.encampment);
    CHECK_EQ(p.city, cid);
    const int cityHp = g->state().cities[0].hp;
    REQUIRE(g->submit(Command::rangedAttack(0, archer, {10, 5})) == CommandError::Ok);
    REQUIRE(g->submit(Command::attack(0, sword, {10, 5})) == CommandError::Ok);
    CHECK(g->encampmentHp(g->state().cities[0]) < 100);
    REQUIRE(g->encampmentHp(g->state().cities[0]) > 0);
    CHECK_EQ(g->state().cities[0].hp, cityHp);       // the city itself is untouched
    CHECK_EQ(g->state().unit(guard)->hp, 100);        // and so is its guard
    CHECK(g->state().unit(sword)->hp < 100);          // melee pays for it
    CHECK((g->state().unit(sword)->pos == Hex{11, 5}));  // and stays outside
    // It heals on its owner's turn, and survives a save.
    const int hurt = g->state().cities[0].districts[0].damage;
    endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    CHECK(g->state().cities[0].districts[0].damage < hurt);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().cities[0].districts[0].damage, g->state().cities[0].districts[0].damage);
    // At 0 HP it falls: pillaged, no longer a target or an obstacle, back at full strength once repaired.
    GameState s = g->state();
    s.cities[0].districts[0].damage = 95;
    s.currentPlayer = 0;
    for (Unit& u : s.units) {
        u.movesLeft = Fixed::fromInt(2);
        u.attacks = 0;
        u.activity = Activity::Sleep;
    }
    auto h = Game::fromScenario(rules(), std::move(s));
    REQUIRE(h->submit(Command::rangedAttack(0, archer, {10, 5})) == CommandError::Ok);
    CHECK(h->state().cities[0].districts[0].pillagedTurns > 0);
    CHECK(h->encampmentTargetAt({10, 5}) == nullptr);
    CHECK(!h->canEncampmentStrike(cid, {11, 5}));
    REQUIRE(h->submit(Command::setActivity(0, archer, Activity::Sleep)) == CommandError::Ok);
    endTurns(*h, 2 * (kPillagedDistrictTurns + 1));  // its owner repairs it over its own turns
    CHECK_EQ(h->state().cities[0].districts[0].pillagedTurns, 0);
    CHECK_EQ(h->encampmentHp(h->state().cities[0]), 100);
}

TEST(an_encampment_shares_its_citys_walls) {
    UnitId archer = kNoUnit;
    auto g = siege([&](GameState& s) {
        addBuilding(s, 1, "BUILDING_ANCIENT_WALLS");
        CityDistrict camp;
        camp.type = rules().district("DISTRICT_ENCAMPMENT");
        camp.pos = {10, 5};
        camp.complete = true;
        s.cities[0].districts.push_back(camp);
        s.plot({10, 5}).owner = 1;
        s.plot({10, 5}).city = s.cities[0].id;
        archer = addUnit(s, "UNIT_ARCHER", 0, {12, 5});
        for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
    });
    const City& c = g->state().cities[0];
    REQUIRE(g->encampmentWallHp(c) > 0);
    const int walls = g->encampmentWallHp(c);
    REQUIRE(g->previewAttack(archer, {10, 5}, true).hitsWalls);
    REQUIRE(g->submit(Command::rangedAttack(0, archer, {10, 5})) == CommandError::Ok);
    CHECK(g->encampmentWallHp(g->state().cities[0]) < walls);
    CHECK_EQ(g->encampmentHp(g->state().cities[0]), 100);
    CHECK_EQ(g->state().cities[0].wallHp, g->cityMaxWallHp(g->state().cities[0]));  // the city's own walls are untouched
}
