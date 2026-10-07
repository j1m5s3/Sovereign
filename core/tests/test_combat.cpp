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

// Path planning ends the turn on each of the six plots around an enemy at war: a Warrior two plots away plans the
// step next to it with no moves left, on whichever side it comes from.
TEST(zone_of_control_ends_planned_moves_on_every_side) {
    const Hex enemy{8, 6};
    std::vector<UnitId> ours;
    auto g = duel([&](GameState& s) {
        addUnit(s, "UNIT_WARRIOR", 1, enemy);
        for (int d = 0; d < kNumDirs; ++d) {
            const Hex side = *s.grid.neighbor(enemy, static_cast<Dir>(d));
            ours.push_back(addUnit(s, "UNIT_WARRIOR", 0, *s.grid.neighbor(side, static_cast<Dir>(d))));
        }
    });
    for (int d = 0; d < kNumDirs; ++d) {
        const Hex side = *g->state().grid.neighbor(enemy, static_cast<Dir>(d));
        const auto path = g->findPath(ours[static_cast<size_t>(d)], side);
        REQUIRE(path && path->size() == 2u);
        CHECK_EQ(path->back().turn, 0);
        CHECK_EQ(path->back().movesLeft, Fixed());
    }
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

// An ability a unit already has counts once when a modifier grants it too (Oligarchy's +4, made the Warrior's own here).
TEST(a_granted_ability_the_unit_already_has_counts_once) {
    Rules r = rules();
    r.units[at(r.unit("UNIT_WARRIOR"))].abilities.push_back(r.ability("ABILITY_OLIGARCHY_MELEE_BUFF"));
    const auto strength = [&](bool oligarchy) {
        GameState s = flatState(16, 12, 2);
        if (oligarchy) {
            Game::fitPlayerToRules(s.players[0], r);
            const TypeIndex government = r.government("GOVERNMENT_OLIGARCHY");
            s.players[0].government = government;
            s.players[0].policies.assign(static_cast<size_t>(r.governments[at(government)].totalSlots()), kNone);
        }
        const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        const UnitId foe = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
        auto g = Game::fromScenario(r, std::move(s));
        return g->combatStrength(unit(*g, w), unit(*g, foe), true, false);
    };
    CHECK_EQ(strength(false), 24);  // 20 and its own +4
    CHECK_EQ(strength(true), 24);
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

// Another player's units and cities keep a unit out, and so do a standing Encampment it is at war with, closed
// borders unless it is already inside them, and Music Censorship for a foreign Rock Band (04, 05).
TEST(what_keeps_a_unit_out_of_a_plot) {
    const Rules& r = rules();
    enum class Case { Peace, War, CampLost, Censorship, ClosedBorders };
    UnitId w = kNoUnit, band = kNoUnit;
    auto setup = [&](Case c) {
        return duel([&](GameState& s) {
            Game::fitPlayerToRules(s.players[1], r);
            sovtest::addCity(s, 1, {10, 5}, true);  // its land: (9, 5), (11, 5), (10, 4), (11, 4), (10, 6), (11, 6)
            s.cities[0].districts.push_back({r.district("DISTRICT_ENCAMPMENT"), {11, 5}, true});  // with no one in it
            if (c == Case::CampLost) s.plot({11, 5}).city = kNoCity;  // the city no longer holds the plot
            if (c == Case::ClosedBorders) s.players[1].civics.done[at(r.civic("CIVIC_EARLY_EMPIRE"))] = 1;
            if (c == Case::Censorship) {
                Player& p = s.players[1];
                p.government = r.government("GOVERNMENT_CHIEFDOM");
                p.policies.assign(static_cast<size_t>(r.governments[at(p.government)].totalSlots()), kNone);
                p.policies[0] = r.policy("POLICY_MUSIC_CENSORSHIP");
                s.plot({8, 4}).owner = 1;  // the band's own plot too
            }
            w = addUnit(s, "UNIT_WARRIOR", 0, {8, 5});
            band = addUnit(s, "UNIT_ROCK_BAND", 0, {8, 4});
        }, c == Case::War || c == Case::CampLost);
    };
    auto peace = setup(Case::Peace);
    CHECK(!peace->moveCost(unit(*peace, w), {9, 5}, {10, 5}));                // their city
    CHECK(peace->moveCost(unit(*peace, w), {12, 5}, {11, 5}).has_value());    // their Encampment, at peace
    CHECK(peace->moveCost(unit(*peace, band), {8, 5}, {9, 5}).has_value());   // their land
    // A path search keeps out alike (the city is in sight, so only the city stops it).
    REQUIRE(peace->visibility(0, {10, 5}) != Visibility::Unrevealed);
    CHECK(!peace->findPath(w, {10, 5}).has_value());
    CHECK(peace->findPath(w, {9, 5}).has_value());
    auto war = setup(Case::War);
    CHECK(!war->moveCost(unit(*war, w), {12, 5}, {11, 5}));                    // a standing enemy Encampment
    CHECK(!war->moveCost(unit(*war, w), {9, 5}, {10, 5}));
    auto lost = setup(Case::CampLost);
    CHECK(lost->moveCost(unit(*lost, w), {12, 5}, {11, 5}).has_value());      // no Encampment of the city's there now
    auto censored = setup(Case::Censorship);
    CHECK(!censored->moveCost(unit(*censored, band), {8, 5}, {9, 5}));         // no foreign Rock Band enters
    CHECK(!censored->moveCost(unit(*censored, band), {9, 5}, {10, 4}));        // nor moves on inside
    CHECK(censored->moveCost(unit(*censored, w), {8, 5}, {9, 5}).has_value()); // other units do
    REQUIRE(censored->visibility(0, {9, 5}) != Visibility::Unrevealed);
    CHECK(!censored->findPath(band, {9, 5}).has_value());
    CHECK(censored->findPath(w, {9, 5}).has_value());
    const auto stay = censored->findPath(band, {8, 4});  // a band already inside may stay where it is
    REQUIRE(stay.has_value());
    CHECK_EQ(stay->size(), 1u);
    auto closed = setup(Case::ClosedBorders);
    CHECK(!closed->moveCost(unit(*closed, w), {8, 5}, {9, 5}));                 // closed borders
    CHECK(closed->moveCost(unit(*closed, w), {9, 5}, {10, 4}).has_value());     // a unit already inside moves on
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
    CHECK(g->formationProblem(0, a, foe) == CommandError::NotYourUnit);  // nor someone else's
    CHECK(g->formationProblem(1, foe, a) == CommandError::NotYourUnit);
    CHECK(g->formationProblem(1, a, foe) == CommandError::NotYourUnit);
    CHECK(g->formationProblem(1, a, b) == CommandError::NotYourUnit);
    CHECK(g->formationProblem(0, a, a) == CommandError::NotYourUnit);  // nor itself
    CHECK(g->formationProblem(0, a, 999) == CommandError::NotYourUnit);  // nor one that is gone
    CHECK(g->formationProblem(0, 999, a) == CommandError::NotYourUnit);
    REQUIRE(g->submit(Command::formUnit(0, a, b)) == CommandError::Ok);
    CHECK(g->state().unit(b) == nullptr);
    CHECK_EQ(g->state().unit(a)->formation, 1);
    CHECK(sovtest::hasMoment(*g, 0, "MOMENT_WORLD_S_FIRST_CORPS"));  // 09
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

// ---- Corps and Armies trained whole (05: Corps and Armies)

TEST(a_military_academy_trains_corps_and_armies_whole) {
    GameState s = sovtest::flatState(16, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId city = sovtest::addCity(s, 0, {4, 5}, true, 8);
    const TypeIndex spear = rules().unit("UNIT_SPEARMAN");
    s.players[0].techs.done[static_cast<size_t>(rules().tech("TECH_BRONZE_WORKING"))] = 1;
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATIONALISM"))] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem single{ProductionKind::Unit, spear};
    const ProductionItem corps{ProductionKind::Unit, spear, 1};
    const ProductionItem army{ProductionKind::Unit, spear, 2};
    REQUIRE(g->canProduce(*g->state().city(city), single));
    CHECK(!g->canProduce(*g->state().city(city), corps));  // no Military Academy
    GameState t = g->state();
    t.cities[0].buildings.push_back(rules().building("BUILDING_MILITARY_ACADEMY"));
    std::sort(t.cities[0].buildings.begin(), t.cities[0].buildings.end());
    auto h = Game::fromScenario(rules(), std::move(t));
    const City& c = *h->state().city(city);
    CHECK(h->canProduce(c, corps));
    CHECK(!h->canProduce(c, army));  // Armies need Mobilization
    CHECK_EQ(h->productionCost(0, corps), h->productionCost(0, single) * 3 / 2);
    const auto items = h->buildableItems(city);
    CHECK(std::find(items.begin(), items.end(), corps) != items.end());
    // Bought outright, it arrives as a Corps; the command carries the formation.
    GameState u = h->state();
    u.players[0].gold = Fixed::fromInt(10000);
    auto k = Game::fromScenario(rules(), std::move(u));
    const size_t before = k->state().units.size();
    REQUIRE(k->submit(Command::purchase(0, city, corps)) == CommandError::Ok);
    REQUIRE(k->state().units.size() == before + 1);
    CHECK_EQ(k->state().units.back().type, spear);
    CHECK_EQ(k->state().units.back().formation, 1);
    // A Corps in the queue survives a save.
    REQUIRE(k->submit(Command::setProduction(0, city, corps)) == CommandError::Ok);
    std::string err;
    auto back = loadGame(rules(), saveGame(*k), &err);
    REQUIRE(back);
    CHECK(back->state().city(city)->queue.front() == corps);
}

// Fleets and Armadas, the naval Corps and Armies, need a Seaport where land units need a Military Academy.
TEST(a_seaport_trains_fleets_and_armadas_whole) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    sovtest::addCity(s, 0, {4, 5}, true, 8);
    for (const char* c : {"CIVIC_NATIONALISM", "CIVIC_MOBILIZATION"}) s.players[0].civics.done[at(rules().civic(c))] = 1;
    const TypeIndex galley = rules().unit("UNIT_GALLEY"), spear = rules().unit("UNIT_SPEARMAN");
    const auto trains = [&](const char* building, TypeIndex unit, int formation) {
        GameState t = s;
        t.cities[0].buildings.push_back(rules().building(building));
        std::sort(t.cities[0].buildings.begin(), t.cities[0].buildings.end());
        auto g = Game::fromScenario(rules(), std::move(t));
        return g->canTrainFormation(g->state().cities[0], unit, formation);
    };
    CHECK(trains("BUILDING_SEAPORT", galley, 1));
    CHECK(trains("BUILDING_SEAPORT", galley, 2));
    CHECK(!trains("BUILDING_MILITARY_ACADEMY", galley, 1));
    CHECK(!trains("BUILDING_SEAPORT", spear, 1));
    CHECK(trains("BUILDING_MILITARY_ACADEMY", spear, 2));
}

TEST(units_trained_in_a_city_keep_its_buildings_combat_xp) {
    // 03: each Encampment, Harbor and Aerodrome building gives units of its classes trained in its city
    // +25% combat XP for good (the Airport +50%); a pillaged district's buildings give none.
    const auto trained = [](std::vector<const char*> buildings, const char* unitId, bool pillaged = false) {
        GameState s = sovtest::flatState(16, 12, 2);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        s.players[0].stockpile[at(rules().resource("RESOURCE_HORSES"))] = 50;
        s.players[0].stockpile[at(rules().resource("RESOURCE_OIL"))] = 10;
        sovtest::addCity(s, 0, {4, 5}, true, 8);
        for (const char* b : buildings) s.cities[0].buildings.push_back(rules().building(b));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        s.cities[0].districts.push_back({rules().district("DISTRICT_ENCAMPMENT"), {6, 5}, true});
        if (pillaged) s.cities[0].districts.back().pillagedTurns = kPillagedDistrictTurns;
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->completeItem(g->stateMutForTests().cities[0], ProductionItem{ProductionKind::Unit, rules().unit(unitId)}));
        const Unit& made = g->state().units.back();
        CHECK_EQ(made.type, rules().unit(unitId));
        CHECK_EQ(made.xp, 0);  // a faster pace, not a head start
        return static_cast<int>(made.xpBonus);
    };
    CHECK_EQ(trained({}, "UNIT_SPEARMAN"), 0);
    CHECK_EQ(trained({"BUILDING_BARRACKS"}, "UNIT_SPEARMAN"), 25);
    CHECK_EQ(trained({"BUILDING_BARRACKS"}, "UNIT_SLINGER"), 25);
    CHECK_EQ(trained({"BUILDING_BARRACKS"}, "UNIT_HORSEMAN"), 0);  // cavalry and siege learn at the Stable
    CHECK_EQ(trained({"BUILDING_STABLE"}, "UNIT_HORSEMAN"), 25);
    CHECK_EQ(trained({"BUILDING_STABLE"}, "UNIT_CATAPULT"), 25);
    CHECK_EQ(trained({"BUILDING_STABLE"}, "UNIT_SPEARMAN"), 0);
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_ARMORY", "BUILDING_MILITARY_ACADEMY"}, "UNIT_SPEARMAN"), 75);
    CHECK_EQ(trained({"BUILDING_STABLE", "BUILDING_ARMORY", "BUILDING_MILITARY_ACADEMY"}, "UNIT_HORSEMAN"), 75);
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_ARMORY"}, "UNIT_BUILDER"), 0);  // civilians earn no XP
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_ARMORY"}, "UNIT_SPEARMAN", true), 0);
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_LIGHTHOUSE"}, "UNIT_SPEARMAN"), 25);
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_CALMECAC"}, "UNIT_SPEARMAN"), 50);  // the Aztecs' Calmecac adds its own 25%
    CHECK_EQ(trained({"BUILDING_CALMECAC"}, "UNIT_BUILDER"), 0);
    // Ships wait in the port: the Lighthouse, Shipyard and Seaport each add 25%, and the pillaged Encampment is not theirs.
    CHECK_EQ(trained({"BUILDING_LIGHTHOUSE"}, "UNIT_GALLEY"), 25);
    CHECK_EQ(trained({"BUILDING_BARRACKS", "BUILDING_LIGHTHOUSE", "BUILDING_SHIPYARD", "BUILDING_SEAPORT"}, "UNIT_GALLEY", true), 75);
    // Aircraft: the Hangar 25%, the Airport 50%.
    CHECK_EQ(trained({"BUILDING_HANGAR"}, "UNIT_BIPLANE"), 25);
    CHECK_EQ(trained({"BUILDING_HANGAR", "BUILDING_AIRPORT"}, "UNIT_BIPLANE"), 75);
    CHECK_EQ(trained({"BUILDING_HANGAR", "BUILDING_AIRPORT"}, "UNIT_SPEARMAN"), 0);
    // A unit bought with Faith (Theocracy, the Grand Master's Chapel) is trained in the city too.
    GameState s = sovtest::flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    const CityId city = sovtest::addCity(s, 0, {4, 5}, true, 8);
    s.cities[0].buildings.push_back(rules().building("BUILDING_BARRACKS"));
    s.players[0].techs.done[at(rules().tech("TECH_BRONZE_WORKING"))] = 1;
    s.players[0].government = rules().government("GOVERNMENT_THEOCRACY");
    s.players[0].policies.assign(static_cast<size_t>(rules().governments[at(s.players[0].government)].totalSlots()), kNone);
    s.players[0].faith = Fixed::fromInt(5000);
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::purchaseWithFaith(0, city, ProductionItem{ProductionKind::Unit, rules().unit("UNIT_SPEARMAN")})) == CommandError::Ok);
    CHECK_EQ(g->state().units.back().type, rules().unit("UNIT_SPEARMAN"));
    CHECK_EQ(g->state().units.back().xpBonus, 25);
}

TEST(recon_naval_and_melee_promotions_take_effect) {
    UnitId scout = kNoUnit, plain = kNoUnit, raider = kNoUnit, victim = kNoUnit, carrier = kNoUnit;
    auto g = duel([&](GameState& s) {
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        s.plot({5, 5}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
        s.plot({5, 6}).feature = rules().feature("FEATURE_FOREST");
        scout = addUnit(s, "UNIT_SCOUT", 0, {4, 5});
        s.units.back().promotions = {promotion("PROMOTION_ALPINE"), promotion("PROMOTION_RANGER")};
        plain = addUnit(s, "UNIT_SCOUT", 0, {4, 6});
        for (int y = 0; y < 12; ++y) s.plot({12, y}).terrain = s.plot({13, y}).terrain = rules().terrain("TERRAIN_COAST");
        raider = addUnit(s, "UNIT_PRIVATEER", 0, {12, 5});
        s.units.back().promotions = {promotion("PROMOTION_BOARDING")};
        victim = addUnit(s, "UNIT_GALLEY", 1, {13, 5});
        s.units.back().hp = 1;
        carrier = addUnit(s, "UNIT_AIRCRAFT_CARRIER", 0, {12, 9});
        s.units.back().promotions = {promotion("PROMOTION_FLIGHT_DECK")};
    });
    // Alpine and Ranger: hills and woods cost a single move.
    CHECK(*g->moveCost(unit(*g, scout), {4, 5}, {5, 5}) == Fixed::fromInt(1));
    CHECK(*g->moveCost(unit(*g, plain), {4, 5}, {5, 5}) > Fixed::fromInt(1));
    CHECK(*g->moveCost(unit(*g, scout), {4, 6}, {5, 6}) == Fixed::fromInt(1));
    CHECK(*g->moveCost(unit(*g, plain), {4, 6}, {5, 6}) > Fixed::fromInt(1));
    // Flight Deck: one more aircraft aboard.
    CHECK_EQ(g->airSlots(0, {12, 9}), rules().units[at(rules().unit("UNIT_AIRCRAFT_CARRIER"))].airSlots + 1);
    // Boarding: Gold from a ship it sinks.
    const Fixed gold = g->state().players[0].gold;
    const CommandError boarded = g->submit(Command::rangedAttack(0, raider, {13, 5}));
    REQUIRE(boarded == CommandError::Ok);
    REQUIRE(g->state().unit(victim) == nullptr);
    CHECK(g->state().players[0].gold > gold);
}

TEST(helicopters_ignore_terrain_costs) {
    UnitId heli = kNoUnit, foot = kNoUnit;
    auto g = duel([&](GameState& s) {
        s.plot({5, 5}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
        s.plot({5, 5}).feature = rules().feature("FEATURE_FOREST");
        heli = addUnit(s, "UNIT_HELICOPTER", 0, {4, 5});
        foot = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
    }, false);
    CHECK_EQ(g->moveCost(unit(*g, foot), {4, 5}, {5, 5}).value_or(Fixed()), Fixed::fromInt(3));  // hills and woods
    CHECK_EQ(g->moveCost(unit(*g, heli), {4, 5}, {5, 5}).value_or(Fixed()), Fixed::fromInt(1));
}
