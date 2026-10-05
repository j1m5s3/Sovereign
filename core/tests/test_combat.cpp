// War and peace, combat strength and damage, ZOC, capture, XP, promotions,
// healing and strategic fuel (05-units-and-combat.md).
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addUnit;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex promotion(const char* id) { return rules().promotion(id); }

// Two players on flat grassland; `edit` adds units and tweaks the state.
template <typename Edit>
std::unique_ptr<Game> duel(Edit edit, bool war = true) {
    GameState s = flatState(16, 12, 2);
    edit(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    if (war) g->submit(Command::declareWar(0, 1));
    return g;
}

const Unit& unit(const Game& g, UnitId id) { return *g.state().unit(id); }
}  // namespace

TEST(combat_data_from_civ_tables) {
    const Rules& r = rules();
    const UnitType& warrior = r.units[at(r.unit("UNIT_WARRIOR"))];
    CHECK_EQ(warrior.promotionClass, std::string("PROMOTION_CLASS_MELEE"));
    REQUIRE(warrior.abilities.size() == 1u);
    CHECK_EQ(r.abilities[at(warrior.abilities[0])].id, std::string("ABILITY_ANTI_SPEAR"));
    const UnitType& catapult = r.units[at(r.unit("UNIT_CATAPULT"))];
    CHECK_EQ(catapult.bombard, 35);
    CHECK_EQ(catapult.ranged, 0);
    const UnitType& tank = r.units[at(r.unit("UNIT_TANK"))];
    CHECK_EQ(tank.resourceMaintenance, 1);
    CHECK_EQ(tank.strategicResource, r.resource("RESOURCE_OIL"));
    CHECK_EQ(r.units[at(r.unit("UNIT_SETTLER"))].capturedAs, r.unit("UNIT_SETTLER"));
    CHECK_EQ(r.units[at(r.unit("UNIT_TRADER"))].capturedAs, kNone);
    const PromotionType& commando = r.promotions[at(promotion("PROMOTION_COMMANDO"))];
    CHECK_EQ(commando.tier, 2);
    CHECK_EQ(commando.prereqs.size(), 2u);
    CHECK(r.civics[at(r.civic("CIVIC_MILITARY_TRADITION"))].combatAdjacency);
    CHECK(r.civics[at(r.civic("CIVIC_EARLY_EMPIRE"))].enforceBorders);
    int melee = 0;
    for (const PromotionType& p : r.promotions) melee += p.promotionClass == "PROMOTION_CLASS_MELEE";
    CHECK_EQ(melee, 7);
}

TEST(combat_damage_formula) {
    auto g = duel([](GameState&) {}, false);
    // 30 x e^(0.04 x diff) at the middle roll (24 + 6), +-20% at the ends.
    CHECK_EQ(g->combatDamage(0, 6), 30);
    CHECK_EQ(g->combatDamage(0, 0), 24);
    CHECK_EQ(g->combatDamage(0, 12), 36);
    CHECK_EQ(g->combatDamage(10, 6), 45);   // 44.75
    CHECK_EQ(g->combatDamage(-10, 6), 20);  // 20.11
    CHECK_EQ(g->combatDamage(25, 6), 82);   // 81.55
    CHECK_EQ(g->combatDamage(-200, 0), 1);  // COMBAT_MINIMUM_DAMAGE
}

TEST(war_and_peace_need_minimum_turns) {
    auto g = duel([](GameState&) {}, false);
    CHECK(!g->atWar(0, 1));
    CHECK_EQ(g->submit(Command::declareWar(0, 0)), CommandError::CannotDeclareWar);
    Command far = Command::declareWar(0, 1);
    far.arg = 257;  // must not wrap to player 1
    CHECK_EQ(g->submit(far), CommandError::CannotDeclareWar);
    CHECK_EQ(g->submit(Command::declareWar(0, 1)), CommandError::Ok);
    CHECK(g->atWar(0, 1) && g->atWar(1, 0));
    CHECK_EQ(g->submit(Command::declareWar(0, 1)), CommandError::CannotDeclareWar);
    CHECK_EQ(g->submit(Command::makePeace(0, 1)), CommandError::CannotMakePeace);  // DIPLOMACY_WAR_MIN_TURNS
    endTurns(*g, 2 * rules().globalInt("DIPLOMACY_WAR_MIN_TURNS"));
    CHECK_EQ(g->submit(Command::makePeace(0, 1)), CommandError::Ok);
    CHECK(g->atWar(0, 1));  // until the other side agrees
    CHECK_EQ(g->submit(Command::makePeace(0, 1)), CommandError::CannotMakePeace);
    endTurns(*g, 1);
    CHECK_EQ(g->submit(Command::makePeace(1, 0)), CommandError::Ok);
    CHECK(!g->atWar(0, 1) && !g->atWar(1, 0));
    CHECK_EQ(g->submit(Command::declareWar(1, 0)), CommandError::CannotDeclareWar);  // DIPLOMACY_PEACE_MIN_TURNS
    endTurns(*g, 2 * rules().globalInt("DIPLOMACY_PEACE_MIN_TURNS"));
    CHECK_EQ(g->submit(Command::declareWar(1, 0)), CommandError::Ok);
}

TEST(melee_attack_damages_both_and_gives_xp) {
    UnitId a = kNoUnit, d = kNoUnit;
    auto peace = duel([&](GameState& s) {
        a = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        d = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    }, false);
    CHECK_EQ(peace->submit(Command::attack(0, a, {6, 5})), CommandError::CannotAttack);

    auto g = duel([&](GameState& s) {
        a = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        d = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    });
    CombatPreview pv = g->previewAttack(a, {6, 5}, false);
    REQUIRE(pv.valid);
    CHECK_EQ(pv.attackerStrength, 20);
    CHECK_EQ(pv.defenderStrength, 20);
    CHECK_EQ(pv.damageToDefenderMin, 24);
    CHECK_EQ(pv.damageToDefenderMax, 36);
    CHECK_EQ(g->submit(Command::attack(0, a, {7, 5})), CommandError::CannotAttack);  // not adjacent
    CHECK_EQ(g->submit(Command::rangedAttack(0, a, {6, 5})), CommandError::CannotAttack);  // no ranged strength
    CHECK_EQ(g->submit(Command::attack(0, a, {6, 5})), CommandError::Ok);
    const Unit& ua = unit(*g, a);
    const Unit& ud = unit(*g, d);
    CHECK(ud.hp >= 64 && ud.hp <= 76);
    CHECK(ua.hp >= 64 && ua.hp <= 76);
    CHECK_EQ(ua.pos, (Hex{5, 5}));
    CHECK_EQ(ua.movesLeft, Fixed());
    CHECK_EQ(ua.xp, 4);  // 20/20 + 2 melee + 1 attacker
    CHECK_EQ(ud.xp, 3);
    CHECK_EQ(g->submit(Command::attack(0, a, {6, 5})), CommandError::CannotAttack);  // one attack per turn
}

TEST(strength_terrain_fortify_wounds_and_matchups) {
    const Rules& r = rules();
    UnitId w = kNoUnit, sp = kNoUnit, h = kNoUnit;
    auto g = duel([&](GameState& s) {
        s.plot({6, 5}).terrain = r.terrain("TERRAIN_GRASS_HILLS");
        s.plot({6, 5}).feature = r.feature("FEATURE_FOREST");
        w = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        sp = addUnit(s, "UNIT_SPEARMAN", 1, {6, 5});
        h = addUnit(s, "UNIT_HORSEMAN", 0, {7, 5});
    });
    const Unit& uw = unit(*g, w);
    const Unit& usp = unit(*g, sp);
    const Unit& uh = unit(*g, h);
    CHECK_EQ(g->combatStrength(uw, usp, true, false), 25);   // 20 + 5 Anti-Spear
    CHECK_EQ(g->combatStrength(usp, uw, false, false), 31);  // 25 + 3 hills + 3 woods
    CHECK_EQ(g->combatStrength(usp, uh, false, false), 41);  // + 10 Anti-Cavalry
    GameState s = g->state();
    s.unit(sp)->fortifyTurns = 5;  // capped at FORTIFY_TURN_MAX
    s.unit(sp)->hp = 55;           // round(10 - 5.5) = 5 (halves up)
    auto g2 = Game::fromScenario(r, s);
    CHECK_EQ(g2->combatStrength(unit(*g2, sp), unit(*g2, w), false, false), 31 + 6 - 5);
}

TEST(flanking_and_support_after_military_tradition) {
    const Rules& r = rules();
    UnitId a = kNoUnit, d = kNoUnit;
    auto setup = [&](bool tradition) {
        return duel([&](GameState& s) {
            Game::fitPlayerToRules(s.players[0], r);
            Game::fitPlayerToRules(s.players[1], r);
            if (tradition) {
                for (Player& p : s.players) p.civics.done[at(r.civic("CIVIC_MILITARY_TRADITION"))] = 1;
            }
            a = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
            d = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
            addUnit(s, "UNIT_WARRIOR", 0, {6, 4});  // flanks the defender
            addUnit(s, "UNIT_WARRIOR", 0, {6, 6});
            addUnit(s, "UNIT_WARRIOR", 1, {7, 5});  // supports it
        });
    };
    auto plain = setup(false);
    CHECK_EQ(plain->combatStrength(unit(*plain, a), unit(*plain, d), true, false), 20);
    auto g = setup(true);
    CHECK_EQ(g->combatStrength(unit(*g, a), unit(*g, d), true, false), 24);
    CHECK_EQ(g->combatStrength(unit(*g, d), unit(*g, a), false, false), 22);
}

TEST(ranged_attack_hits_only_the_target) {
    UnitId archer = kNoUnit, cat = kNoUnit, d = kNoUnit;
    auto g = duel([&](GameState& s) {
        archer = addUnit(s, "UNIT_ARCHER", 0, {5, 5});
        cat = addUnit(s, "UNIT_CATAPULT", 0, {5, 6});
        d = addUnit(s, "UNIT_WARRIOR", 1, {7, 5});
    });
    CHECK_EQ(g->submit(Command::attack(0, archer, {6, 5})), CommandError::CannotAttack);  // ranged units cannot melee
    CombatPreview pv = g->previewAttack(archer, {7, 5}, true);
    REQUIRE(pv.valid);
    CHECK_EQ(pv.attackerStrength, 25);
    CHECK_EQ(pv.damageToAttackerMax, 0);
    CHECK_EQ(g->combatStrength(unit(*g, cat), unit(*g, d), true, true), 35 - 17);  // bombard vs unit
    CHECK_EQ(g->submit(Command::rangedAttack(0, archer, {8, 5})), CommandError::CannotAttack);  // out of range
    CHECK_EQ(g->submit(Command::rangedAttack(0, archer, {7, 5})), CommandError::Ok);
    CHECK_EQ(unit(*g, archer).hp, 100);
    CHECK(unit(*g, d).hp < 100);
    CHECK_EQ(unit(*g, archer).xp, 20 / 25 + 1 + 1);
    // Siege cannot fire after moving.
    CHECK_EQ(g->submit(Command::move(0, cat, {6, 6})), CommandError::Ok);
    CHECK_EQ(g->submit(Command::rangedAttack(0, cat, {7, 5})), CommandError::CannotAttack);
}

TEST(kill_advances_and_captures_civilians) {
    UnitId a = kNoUnit, d = kNoUnit, b = kNoUnit, settlerA = kNoUnit, settler = kNoUnit, third = kNoUnit, trader = kNoUnit;
    auto g = duel([&](GameState& s) {
        a = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        d = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
        b = addUnit(s, "UNIT_BUILDER", 1, {6, 5});
        s.unit(d)->hp = 1;
        settlerA = addUnit(s, "UNIT_WARRIOR", 0, {5, 8});
        settler = addUnit(s, "UNIT_SETTLER", 1, {6, 8});
        third = addUnit(s, "UNIT_WARRIOR", 0, {9, 2});
        trader = addUnit(s, "UNIT_TRADER", 1, {10, 2});
    });
    CHECK_EQ(g->submit(Command::attack(0, a, {6, 5})), CommandError::Ok);
    CHECK(g->state().unit(d) == nullptr);
    CHECK_EQ(unit(*g, a).pos, (Hex{6, 5}));
    CHECK_EQ(unit(*g, a).xp, 2 * 20 / 20 + 2 + 1);  // kill bonus doubles the strength ratio
    CHECK_EQ(unit(*g, b).owner, 0);                  // the escorted Builder is captured
    CombatPreview pv = g->previewAttack(settlerA, {6, 8}, false);
    CHECK(pv.valid && pv.capture);
    CHECK_EQ(g->submit(Command::attack(0, settlerA, {6, 8})), CommandError::Ok);
    CHECK_EQ(unit(*g, settler).owner, 0);
    CHECK_EQ(unit(*g, settlerA).pos, (Hex{6, 8}));
    CHECK_EQ(g->submit(Command::attack(0, third, {10, 2})), CommandError::Ok);
    CHECK(g->state().unit(trader) == nullptr);  // not capturable: destroyed
}

TEST(zone_of_control_stops_movement) {
    UnitId w = kNoUnit, horse = kNoUnit;
    auto setup = [&](bool war) {
        return duel([&](GameState& s) {
            addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
            w = addUnit(s, "UNIT_WARRIOR", 0, {4, 5});
            horse = addUnit(s, "UNIT_HORSEMAN", 0, {4, 7});
        }, war);
    };
    auto g = setup(true);
    CHECK(g->inEnemyZoc(unit(*g, w), {5, 5}));
    CHECK(!g->inEnemyZoc(unit(*g, w), {4, 4}));
    CHECK(!g->inEnemyZoc(unit(*g, horse), {5, 5}));  // cavalry ignores ZOC
    CHECK_EQ(g->submit(Command::move(0, w, {5, 5})), CommandError::Ok);
    CHECK_EQ(unit(*g, w).movesLeft, Fixed());  // entering the ZOC ends its move
    auto p = setup(false);
    CHECK(!p->inEnemyZoc(unit(*p, w), {5, 5}));
    CHECK_EQ(p->submit(Command::move(0, w, {5, 5})), CommandError::Ok);
    CHECK_EQ(unit(*p, w).movesLeft, Fixed::fromInt(1));
}

TEST(promotion_and_healing) {
    const Rules& r = rules();
    UnitId w = kNoUnit;
    auto g = duel([&](GameState& s) {
        w = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        s.unit(w)->xp = 15;
        s.unit(w)->hp = 40;
        s.unit(w)->activity = Activity::Sleep;
    });
    std::vector<TypeIndex> options = g->availablePromotions(w);
    REQUIRE(options.size() == 2u);  // Battlecry or Tortoise
    CHECK_EQ(g->submit(Command::promote(0, w, promotion("PROMOTION_COMMANDO"))), CommandError::CannotPromote);
    CHECK_EQ(g->submit(Command::promote(0, w, promotion("PROMOTION_BATTLECRY"))), CommandError::Ok);
    CHECK_EQ(unit(*g, w).hp, 90);
    CHECK_EQ(unit(*g, w).xp, 0);
    CHECK_EQ(unit(*g, w).level(), 2);
    CHECK_EQ(unit(*g, w).movesLeft, Fixed());
    CHECK_EQ(g->xpForNextLevel(unit(*g, w)), 30);
    // Battlecry: +7 attacking melee units.
    GameState s = g->state();
    UnitId foe = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    auto g2 = Game::fromScenario(r, s);
    CHECK_EQ(g2->combatStrength(unit(*g2, w), unit(*g2, foe), true, false), 27 - 1);  // 90 HP: -1
    CHECK_EQ(g2->combatStrength(unit(*g2, w), unit(*g2, foe), false, false), 19);
    // Healing: a unit that stays put heals 10 in neutral land, 15 at home.
    endTurns(*g, 2);
    CHECK_EQ(unit(*g, w).hp, 100);
    GameState h = g->state();
    h.unit(w)->hp = 50;
    h.plot({5, 5}).owner = 0;
    auto g3 = Game::fromScenario(r, h);
    endTurns(*g3, 2);
    CHECK_EQ(unit(*g3, w).hp, 50 + r.globalInt("COMBAT_HEAL_LAND_FRIENDLY"));
}

TEST(oligarchy_and_unpaid_fuel_change_strength) {
    const Rules& r = rules();
    UnitId w = kNoUnit, foe = kNoUnit, tank = kNoUnit;
    auto g = duel([&](GameState& s) {
        Game::fitPlayerToRules(s.players[0], r);
        const TypeIndex oligarchy = r.government("GOVERNMENT_OLIGARCHY");
        s.players[0].government = oligarchy;
        s.players[0].policies.assign(static_cast<size_t>(r.governments[at(oligarchy)].totalSlots()), kNone);
        w = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        foe = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
        tank = addUnit(s, "UNIT_TANK", 0, {5, 7});
        s.unit(tank)->hp = 50;
        for (Unit& u : s.units) u.activity = Activity::Sleep;
    });
    CHECK_EQ(g->combatStrength(unit(*g, w), unit(*g, foe), true, false), 24);  // Oligarchy +4 melee
    const int full = g->combatStrength(unit(*g, tank), unit(*g, foe), true, false);
    // No Oil in the stockpile: the next turn's maintenance goes unpaid.
    endTurns(*g, 2);
    CHECK(g->state().players[0].fuelShort[at(r.resource("RESOURCE_OIL"))]);
    CHECK_EQ(g->combatStrength(unit(*g, tank), unit(*g, foe), true, false), full - 20);
    CHECK_EQ(unit(*g, tank).hp, 50);  // and the tank cannot heal
}

TEST(closed_borders_block_units_not_at_war) {
    const Rules& r = rules();
    UnitId w = kNoUnit;
    auto setup = [&](bool war) {
        return duel([&](GameState& s) {
            Game::fitPlayerToRules(s.players[1], r);
            s.players[1].civics.done[at(r.civic("CIVIC_EARLY_EMPIRE"))] = 1;
            for (int x = 7; x < 10; ++x) s.plot({x, 5}).owner = 1;
            w = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        }, war);
    };
    auto peace = setup(false);
    CHECK_EQ(peace->submit(Command::move(0, w, {7, 5})), CommandError::NoPath);
    auto war = setup(true);
    CHECK_EQ(war->submit(Command::move(0, w, {7, 5})), CommandError::Ok);
}

TEST(units_upgrade_for_gold_in_their_territory) {
    GameState s = flatState(20, 14, 1);
    const UnitId id = addUnit(s, "UNIT_SLINGER", 0, {6, 6});
    s.units.back().hp = 70;
    s.units.back().xp = 12;
    for (const Hex& h : s.grid.within({6, 6}, 2)) s.plot(h).owner = 0;
    Player& p = s.players[0];
    p.techs.resize(rules().techs.size());
    p.gold = Fixed::fromInt(100);
    auto g = Game::fromScenario(rules(), s);
    const Unit& u = *g->state().unit(id);
    CHECK_EQ(g->upgradeCost(u), 10 + (60 - 35));  // UPGRADE_BASE_COST + the production difference
    CHECK(g->upgradeProblem(id) == CommandError::CannotUpgrade);  // Archery unknown
    s.players[0].techs.done[static_cast<size_t>(rules().tech("TECH_ARCHERY"))] = 1;
    auto g2 = Game::fromScenario(rules(), s);
    REQUIRE(g2->submit(Command::upgradeUnit(0, id)) == CommandError::Ok);
    const Unit& up = *g2->state().unit(id);
    CHECK(up.type == rules().unit("UNIT_ARCHER"));
    CHECK_EQ(up.hp, 70);  // keeps its health and experience
    CHECK_EQ(up.xp, 12);
    CHECK(up.movesLeft == Fixed());  // the upgrade takes its turn
    CHECK(g2->state().players[0].gold == Fixed::fromInt(65));
    // Outside its territory it cannot.
    s.units[0].pos = {12, 6};
    auto g3 = Game::fromScenario(rules(), std::move(s));
    CHECK(g3->upgradeProblem(id) == CommandError::CannotUpgrade);
}

// ---- formations (05: Corps and Armies)

TEST(twins_form_a_corps_then_an_army) {
    GameState s = flatState(16, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const UnitId a = addUnit(s, "UNIT_INFANTRY", 0, {5, 5});
    const UnitId b = addUnit(s, "UNIT_INFANTRY", 0, {6, 5});
    const UnitId c = addUnit(s, "UNIT_INFANTRY", 0, {5, 6});
    const UnitId other = addUnit(s, "UNIT_TANK", 0, {4, 5});
    const UnitId foe = addUnit(s, "UNIT_INFANTRY", 1, {10, 5});
    auto none = Game::fromScenario(rules(), s);
    CHECK(none->formationProblem(0, a, b) == CommandError::BadUnit);  // Nationalism first
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATIONALISM"))] = 1;
    auto g = Game::fromScenario(rules(), s);
    const int single = g->combatStrength(*g->state().unit(a), *g->state().unit(foe), true, false);
    CHECK(g->formationProblem(0, a, other) == CommandError::BadUnit);  // not the same type
    REQUIRE(g->submit(Command::formUnit(0, a, b)) == CommandError::Ok);
    CHECK(g->state().unit(b) == nullptr);
    CHECK_EQ(g->state().unit(a)->formation, 1);
    CHECK_EQ(g->combatStrength(*g->state().unit(a), *g->state().unit(foe), true, false), single + 10);
    // An Army needs Mobilization (and the turn the Corps spent forming).
    GameState t = g->state();
    for (Unit& u : t.units) u.movesLeft = Fixed::fromInt(2);
    auto h = Game::fromScenario(rules(), t);
    CHECK(h->formationProblem(0, a, c) == CommandError::BadUnit);
    t.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_MOBILIZATION"))] = 1;
    auto k = Game::fromScenario(rules(), std::move(t));
    REQUIRE(k->submit(Command::formUnit(0, a, c)) == CommandError::Ok);
    CHECK_EQ(k->state().unit(a)->formation, 2);
    CHECK_EQ(k->combatStrength(*k->state().unit(a), *k->state().unit(foe), true, false), single + 17);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*k), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().unit(a)->formation, 2);
}
