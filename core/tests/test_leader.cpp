// The playable leader in classic control (specs/sovereign/leader-character-brainstorm.md
// §1, §2, §5, §8.8): start, gear, escorts, capture, barbarian safety, linked moves.
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex gear(const char* id) { return rules().gearType(id); }
Hex hx(int x, int y) { return Hex{x, y}; }

UnitId addLeader(GameState& s, PlayerId owner, Hex pos) {
    const UnitId id = addUnit(s, "UNIT_SOVEREIGN", owner, pos);
    Unit& u = s.units.back();
    u.gear = {gear("GEAR_CLUB"), gear("GEAR_HIDE"), kNone};
    return id;
}

template <typename Edit>
std::unique_ptr<Game> duel(Edit edit, bool war = true) {
    GameState s = flatState(16, 12, 2);
    edit(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    if (war) g->submit(Command::declareWar(0, 1));
    return g;
}

// Ends `n` turns, skipping units that still want orders.
void pass(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        const PlayerId me = g.state().currentPlayer;
        for (UnitId id : g.unitsNeedingOrders(me)) g.submit(Command::setActivity(me, id, Activity::Skip));
        sovtest::endTurns(g, 1);
    }
}

void giveTech(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[static_cast<size_t>(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.techs.done[at(rules().tech(tech))] = 1;
}
}  // namespace

TEST(leader_rules_data) {
    const Rules& r = rules();
    REQUIRE(r.leaderUnit != kNone);
    const UnitType& t = r.units[at(r.leaderUnit)];
    CHECK(t.layer == UnitLayer::Leader);
    CHECK(!t.trainable);
    CHECK_EQ(t.maintenance, 0);
    REQUIRE(gear("GEAR_SWORD") != kNone && gear("GEAR_HORSE") != kNone);
    CHECK(r.gear[at(gear("GEAR_SWORD"))].strategicResource == r.resource("RESOURCE_IRON"));
    CHECK(r.gear[at(gear("GEAR_HORSE"))].upkeepAs == r.unit("UNIT_HORSEMAN"));
    // Fully geared at Iron Working: Sword + Iron mail = the Swordsman (balance rule, §2).
    CHECK_EQ(r.gear[at(gear("GEAR_SWORD"))].combat + r.gear[at(gear("GEAR_IRON_MAIL"))].defense,
             r.units[at(r.unit("UNIT_SWORDSMAN"))].combat);
    const Dynasty* rome = r.dynastyOf(r.civ("CIVILIZATION_ROME"));
    REQUIRE(rome);
    CHECK_EQ(rome->names.size(), 3u);
    CHECK_EQ(rome->names[1], std::string("Tiberius"));
}

TEST(leader_starts_on_the_settler_tile) {
    std::string err;
    auto g = Game::create(rules(), sovtest::duelSetup(42), &err);
    REQUIRE(g);
    for (const Player& p : g->state().players) {
        if (p.barbarian) continue;
        const Unit* l = g->leaderOf(p.id);
        REQUIRE(l);
        CHECK(l->pos == p.startPos);
        CHECK_EQ(l->gear[0], gear("GEAR_CLUB"));
        CHECK_EQ(l->gear[1], gear("GEAR_HIDE"));
        CHECK_EQ(l->gear[2], kNone);
        const Unit* settler = g->state().unitAt(p.startPos, UnitLayer::Civilian, rules());
        CHECK(settler && rules().units[at(settler->type)].foundCity);
    }
    // It cannot be trained.
    for (const City& c : g->state().cities) {
        for (const ProductionItem& i : g->buildableItems(c.id)) CHECK(i.type != rules().leaderUnit || i.kind != ProductionKind::Unit);
    }
}

TEST(leader_strength_comes_from_gear) {
    UnitId leader = 0, enemy = 0;
    auto g = duel([&](GameState& s) {
        leader = addLeader(s, 0, {5, 5});
        enemy = addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
    });
    const Unit& l = *g->state().unit(leader);
    const Unit& e = *g->state().unit(enemy);
    CHECK_EQ(g->combatStrength(l, e, true, false), 16);   // Club
    CHECK_EQ(g->combatStrength(l, e, false, false), 19);  // Club + Hide when defending
    CHECK_EQ(g->maxMoves(l), 2);
    CHECK_EQ(g->unitRange(l), 0);
}

TEST(equip_gear_needs_city_tech_and_gold) {
    UnitId leader = 0;
    CityId city = kNoCity;
    auto g = duel(
        [&](GameState& s) {
            city = addCity(s, 0, {5, 5}, true);
            leader = addLeader(s, 0, {8, 5});
            giveTech(s, 0, "TECH_BRONZE_WORKING");
            giveTech(s, 0, "TECH_HORSEBACK_RIDING");
            s.players[0].gold = Fixed::fromInt(500);
        },
        false);
    const TypeIndex spear = gear("GEAR_SPEAR");
    CHECK_EQ(g->submit(Command::equipGear(0, leader, spear)), CommandError::CannotEquip);  // not in a city
    REQUIRE(g->submit(Command::move(0, leader, {5, 5})) == CommandError::Ok);
    pass(*g, 2);
    REQUIRE(g->state().unit(leader)->pos == hx(5, 5));
    CHECK_EQ(g->submit(Command::equipGear(0, leader, gear("GEAR_SWORD"))), CommandError::CannotEquip);  // no Iron Working
    const Fixed before = g->state().players[0].gold;
    CHECK_EQ(g->submit(Command::equipGear(0, leader, spear)), CommandError::Ok);
    CHECK_EQ(g->state().unit(leader)->gear[0], spear);
    CHECK_EQ(g->state().players[0].gold, before - Fixed::fromInt(g->gearCost(spear)));
    CHECK(g->state().unit(leader)->movesLeft == Fixed());  // it took the turn
    CHECK_EQ(g->submit(Command::equipGear(0, leader, gear("GEAR_BRONZE_SCALE"))), CommandError::CannotEquip);  // no moves

    // A horse needs Horses in the stockpile, adds 2 moves and costs twice a Horseman's upkeep.
    pass(*g, 2);
    const TypeIndex horse = gear("GEAR_HORSE");
    CHECK_EQ(g->submit(Command::equipGear(0, leader, horse)), CommandError::NotEnoughResources);
    GameState s2 = g->state();
    s2.players[0].stockpile[at(rules().resource("RESOURCE_HORSES"))] = 20;
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    const Fixed upkeepBefore = g2->goldPerTurn(0);
    CHECK_EQ(g2->submit(Command::equipGear(0, leader, horse)), CommandError::Ok);
    CHECK_EQ(g2->state().players[0].stockpile[at(rules().resource("RESOURCE_HORSES"))], 10);
    CHECK_EQ(g2->maxMoves(*g2->state().unit(leader)), 4);
    CHECK_EQ(g2->leaderUpkeep(0), 2 * rules().units[at(rules().unit("UNIT_HORSEMAN"))].maintenance);
    CHECK_EQ(g2->goldPerTurn(0), upkeepBefore - Fixed::fromInt(g2->leaderUpkeep(0)));
    (void)city;
}

TEST(escort_defends_before_the_leader) {
    UnitId leader = 0, escort = 0, enemy = 0;
    auto g = duel([&](GameState& s) {
        leader = addLeader(s, 0, {5, 5});
        escort = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        s.units.back().hp = 1;  // the escort falls to the first blow
        enemy = addUnit(s, "UNIT_SWORDSMAN", 1, {6, 5});
    });
    CHECK_EQ(g->defenderAt({5, 5})->id, escort);
    pass(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    CombatPreview pv = g->previewAttack(enemy, {5, 5}, false);
    CHECK(pv.valid && pv.defender == escort);
    REQUIRE(g->submit(Command::attack(1, enemy, {5, 5})) == CommandError::Ok);
    CHECK(!g->state().unit(escort));
    // The leader still holds the plot: the victor stays put and nothing is captured.
    REQUIRE(g->state().unit(leader));
    CHECK(g->state().unit(leader)->owner == 0);
    CHECK(g->state().unit(enemy)->pos == hx(6, 5));
    CHECK_EQ(g->defenderAt({5, 5})->id, leader);
}

TEST(unescorted_leader_is_captured_by_melee) {
    UnitId leader = 0, builder = 0, enemy = 0;
    auto g = duel([&](GameState& s) {
        leader = addLeader(s, 0, {5, 5});
        s.units.back().hp = 1;
        builder = addUnit(s, "UNIT_BUILDER", 0, {5, 5});
        enemy = addUnit(s, "UNIT_SWORDSMAN", 1, {6, 5});
    });
    pass(*g, 1);
    CombatPreview pv = g->previewAttack(enemy, {5, 5}, false);
    CHECK(pv.valid && pv.defender == leader && !pv.capture);
    REQUIRE(g->submit(Command::attack(1, enemy, {5, 5})) == CommandError::Ok);
    CHECK(!g->leaderOf(0));
    CHECK(g->state().unit(enemy)->pos == hx(5, 5));  // the victor advances
    CHECK(g->state().unit(builder) && g->state().unit(builder)->owner == 1);  // and takes the civilian
}

TEST(leader_can_be_shot_and_struck) {
    UnitId leader = 0, archer = 0;
    auto g = duel([&](GameState& s) {
        leader = addLeader(s, 0, {5, 5});
        s.units.back().hp = 1;
        archer = addUnit(s, "UNIT_ARCHER", 1, {7, 5});
    });
    pass(*g, 1);
    CHECK(g->previewAttack(archer, {5, 5}, true).valid);
    REQUIRE(g->submit(Command::rangedAttack(1, archer, {5, 5})) == CommandError::Ok);
    CHECK(!g->leaderOf(0));
}

TEST(barbarians_never_kill_the_leader) {
    UnitId leader = 0, barb = 0;
    auto g = duel(
        [&](GameState& s) {
            addCity(s, 0, {2, 2}, true);
            leader = addLeader(s, 0, {8, 6});
            s.units.back().hp = 5;
            Player b;
            b.id = 2;
            b.barbarian = true;
            s.players.push_back(b);
            barb = addUnit(s, "UNIT_SWORDSMAN", 2, {9, 6});
        },
        false);
    REQUIRE(g->atWar(0, 2));
    // The badly hurt leader attacks and loses: it lives, and goes home to the capital.
    REQUIRE(g->submit(Command::attack(0, leader, {9, 6})) == CommandError::Ok);
    const Unit* l = g->state().unit(leader);
    REQUIRE(l);
    CHECK(l->hp >= 1);
    CHECK(l->pos == hx(2, 2));
    CHECK(g->state().unit(barb));
}

TEST(linked_escort_moves_with_the_leader) {
    UnitId leader = 0, escort = 0;
    auto g = duel(
        [&](GameState& s) {
            leader = addLeader(s, 0, {5, 5});
            escort = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
        },
        false);
    CHECK_EQ(g->submit(Command::linkEscort(0, leader, leader)), CommandError::CannotEscort);  // not military
    CHECK_EQ(g->submit(Command::linkEscort(0, escort, leader)), CommandError::Ok);
    CHECK_EQ(g->escortOf(*g->state().unit(leader))->id, escort);
    auto waiting = g->unitsNeedingOrders(0);
    CHECK(std::find(waiting.begin(), waiting.end(), escort) == waiting.end());
    REQUIRE(g->submit(Command::move(0, leader, {6, 5})) == CommandError::Ok);
    CHECK(g->state().unit(leader)->pos == hx(6, 5));
    CHECK(g->state().unit(escort)->pos == hx(6, 5));
    // An order to the escort moves the pair.
    REQUIRE(g->submit(Command::move(0, escort, {7, 5})) == CommandError::Ok);
    CHECK(g->state().unit(leader)->pos == hx(7, 5));
    CHECK(g->state().unit(escort)->pos == hx(7, 5));
    CHECK_EQ(g->submit(Command::linkEscort(0, escort, -1)), CommandError::Ok);
    CHECK(!g->escortOf(*g->state().unit(leader)));
}

// A unit may pass another player's units at peace (05: Stacking), a lone leader too; with its escort it keeps out of
// them, as the pair would share a plot with them.
TEST(a_linked_pair_keeps_out_of_other_players_units) {
    UnitId leader = 0, escort = 0;
    auto g = duel(
        [&](GameState& s) {
            // A wall of mountains at x = 6 with one gap, (6,5), where the other player's Warrior stands.
            for (int y = 0; y < 12; ++y) {
                if (y != 5) s.plot({6, y}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
            }
            addUnit(s, "UNIT_WARRIOR", 1, {6, 5});
            leader = addLeader(s, 0, {5, 5});
            escort = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
            s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        },
        false);
    REQUIRE(g->submit(Command::linkEscort(0, escort, leader)) == CommandError::Ok);
    CHECK(!g->findPath(leader, {7, 5}).has_value());
    REQUIRE(g->submit(Command::linkEscort(0, escort, -1)) == CommandError::Ok);
    CHECK(g->findPath(leader, {7, 5}).has_value());
}

// A military unit can be linked to a civilian on its plot, as to the leader (05: Formations): the pair moves together
// and keeps out of other players' units.
TEST(a_civilian_moves_with_its_linked_escort) {
    UnitId settler = 0, escort = 0, other = 0, ram = 0;
    auto g = duel(
        [&](GameState& s) {
            for (int y = 0; y < 12; ++y) {
                if (y != 5 && y != 7) s.plot({8, y}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
            }
            addUnit(s, "UNIT_WARRIOR", 1, {8, 5});  // the other player's Warrior holds the gap at (8,5)
            settler = addUnit(s, "UNIT_SETTLER", 0, {5, 5});
            escort = addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
            ram = addUnit(s, "UNIT_BATTERING_RAM", 0, {5, 5});
            other = addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
            s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        },
        false);
    CHECK_EQ(g->submit(Command::linkEscort(0, settler, escort)), CommandError::CannotEscort);  // a civilian escorts no one
    CHECK_EQ(g->submit(Command::linkEscort(0, escort, ram)), CommandError::CannotEscort);      // nor is a support unit escorted
    CHECK_EQ(g->submit(Command::linkEscort(0, other, settler)), CommandError::CannotEscort);   // not on its plot
    CHECK_EQ(g->submit(Command::linkEscort(0, escort, settler)), CommandError::Ok);
    CHECK_EQ(g->escortOf(*g->state().unit(settler))->id, escort);
    auto waiting = g->unitsNeedingOrders(0);
    CHECK(std::find(waiting.begin(), waiting.end(), escort) == waiting.end());
    REQUIRE(g->submit(Command::move(0, settler, {6, 5})) == CommandError::Ok);
    CHECK(g->state().unit(settler)->pos == hx(6, 5));
    CHECK(g->state().unit(escort)->pos == hx(6, 5));
    REQUIRE(g->submit(Command::move(0, escort, {7, 5})) == CommandError::Ok);  // an order to the escort moves the pair
    CHECK(g->state().unit(settler)->pos == hx(7, 5));
    CHECK(g->state().unit(escort)->pos == hx(7, 5));
    // Through the gap at (8,5) only alone; linked, the path goes round by the gap at (8,7).
    const std::optional<std::vector<PathStep>> linked = g->findPath(settler, {9, 5});
    REQUIRE(linked.has_value());
    CHECK(std::none_of(linked->begin(), linked->end(), [](const PathStep& p) { return p.pos == hx(8, 5); }));
    REQUIRE(g->submit(Command::linkEscort(0, escort, -1)) == CommandError::Ok);
    CHECK(!g->escortOf(*g->state().unit(settler)));
    const std::optional<std::vector<PathStep>> alone = g->findPath(settler, {9, 5});
    REQUIRE(alone.has_value());
    CHECK(std::any_of(alone->begin(), alone->end(), [](const PathStep& p) { return p.pos == hx(8, 5); }));
}

TEST(city_capture_takes_the_leader) {
    UnitId leader = 0, enemy = 0;
    CityId city = kNoCity;
    auto g = duel([&](GameState& s) {
        addCity(s, 0, {2, 2}, true);
        city = addCity(s, 0, {5, 5}, false);
        s.cities.back().hp = 1;  // the next blow takes it
        leader = addLeader(s, 0, {5, 5});
        enemy = addUnit(s, "UNIT_SWORDSMAN", 1, {6, 5});
    });
    pass(*g, 1);
    REQUIRE(g->submit(Command::attack(1, enemy, {5, 5})) == CommandError::Ok);
    CHECK_EQ(g->state().city(city)->owner, 1);
    CHECK(!g->leaderOf(0));
}

TEST(leader_games_replay_and_save) {
    std::string err;
    sov::GameSetup setup = sovtest::duelSetup(11);
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    const UnitId leader = g->leaderOf(0)->id;
    const Unit* guard = nullptr;
    for (const Unit& u : g->state().units) {
        if (u.owner == 0 && rules().units[at(u.type)].layer == UnitLayer::Military) guard = &u;
    }
    REQUIRE(guard);
    if (guard->pos == g->leaderOf(0)->pos) REQUIRE(g->submit(Command::linkEscort(0, guard->id, leader)) == CommandError::Ok);
    pass(*g, 3);
    auto replayed = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(replayed);
    CHECK_EQ(replayed->stateHash(), g->stateHash());
}

// ---- succession, captivity and regicide (leader doc §5)

namespace {
// Player 0 (England) with a capital, a government with a slotted card, and its leader alone
// at (8,5) at 1 HP; player 1 has an Archer and a Swordsman next to it.
struct Fall {
    std::unique_ptr<Game> game;
    UnitId leader = 0, archer = 0, sword = 0;
    CityId capital = kNoCity;
};

Fall fallScenario(bool regicide = false, int dynastyNext = 1) {
    Fall f;
    f.game = duel([&](GameState& s) {
        s.setup.regicide = regicide;
        f.capital = addCity(s, 0, {2, 2}, true);
        addCity(s, 1, {13, 9}, true);
        f.leader = addLeader(s, 0, {8, 5});
        s.units.back().hp = 1;
        s.units.back().gear[0] = gear("GEAR_SPEAR");
        f.archer = addUnit(s, "UNIT_ARCHER", 1, {10, 5});
        f.sword = addUnit(s, "UNIT_SWORDSMAN", 1, {9, 5});
        Player& p = s.players[0];
        Game::fitPlayerToRules(p, rules());
        p.dynastyNext = dynastyNext;
        p.government = rules().government("GOVERNMENT_CHIEFDOM");
        p.policies.assign(static_cast<size_t>(rules().governments[at(p.government)].totalSlots()), kNone);
        p.policies[0] = rules().policy("POLICY_DISCIPLINE");
    });
    pass(*f.game, 1);  // player 1's turn
    return f;
}
}  // namespace

TEST(fallen_leader_starts_an_interregnum_and_a_succession) {
    Fall f = fallScenario();
    Game& g = *f.game;
    CHECK_EQ(g.state().players[0].leaderName, std::string("Elizabeth I"));
    const int before = g.grievances(0, 1);
    REQUIRE(g.submit(Command::rangedAttack(1, f.archer, {8, 5})) == CommandError::Ok);  // killed, not captured
    const Player& p = g.state().players[0];
    CHECK(!g.leaderOf(0));
    CHECK_EQ(g.grievances(0, 1), before + rules().globalInt("LEADER_KILLED_GRIEVANCES"));  // the killer is known (§5)
    CHECK(p.successionPending);
    CHECK_EQ(p.interregnumTurns, rules().globalInt("LEADER_INTERREGNUM_TURNS"));
    CHECK(std::all_of(p.policies.begin(), p.policies.end(), [](TypeIndex x) { return x == kNone; }));
    pass(g, 1);
    REQUIRE(g.state().currentPlayer == 0);
    for (UnitId id : g.unitsNeedingOrders(0)) g.submit(Command::setActivity(0, id, Activity::Skip));
    CHECK_EQ(g.submit(Command::endTurn(0)), CommandError::LeaderNeeded);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Regent)), CommandError::CannotSucceed);  // an heir exists
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Heir)), CommandError::Ok);
    const Unit* heir = g.leaderOf(0);
    REQUIRE(heir);
    CHECK(heir->pos == g.state().city(f.capital)->pos);
    CHECK_EQ(heir->gear[0], gear("GEAR_SPEAR"));  // the loadout passes on
    CHECK_EQ(g.state().players[0].leaderName, std::string("James I"));
    CHECK_EQ(g.state().players[0].dynastyNext, 2);
    // Policies stay locked until the interregnum runs out, then a free change opens.
    CHECK_EQ(g.submit(Command::setPolicy(0, 0, rules().policy("POLICY_DISCIPLINE"))), CommandError::ChangesLocked);
    for (int i = 0; i < rules().globalInt("LEADER_INTERREGNUM_TURNS"); ++i) pass(g, 2);
    CHECK_EQ(g.state().players[0].interregnumTurns, 0);
    CHECK(g.state().players[0].freeChanges);
}

TEST(successors_after_the_dynasty_runs_out) {
    Fall f = fallScenario(false, 3);  // England's two heirs are spent
    Game& g = *f.game;
    REQUIRE(g.submit(Command::rangedAttack(1, f.archer, {8, 5})) == CommandError::Ok);
    pass(g, 1);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Heir)), CommandError::CannotSucceed);
    CHECK(g.successorUnits(0).empty());
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Regent)), CommandError::Ok);
    CHECK_EQ(g.state().players[0].leaderName, std::string("England Regent"));
}

TEST(a_veteran_unit_can_take_the_throne) {
    UnitId veteran = 0, leader = 0, archer = 0;
    auto g = duel([&](GameState& s) {
        addCity(s, 0, {2, 2}, true);
        leader = addLeader(s, 0, {8, 5});
        s.units.back().hp = 1;
        veteran = addUnit(s, "UNIT_WARRIOR", 0, {3, 3});
        s.units.back().promotions = {rules().promotion("PROMOTION_BATTLECRY"), rules().promotion("PROMOTION_TORTOISE"),
                                     rules().promotion("PROMOTION_COMMANDO")};
        archer = addUnit(s, "UNIT_ARCHER", 1, {10, 5});
        Game::fitPlayerToRules(s.players[0], rules());
        s.players[0].dynastyNext = 3;
    });
    pass(*g, 1);
    REQUIRE(g->submit(Command::rangedAttack(1, archer, {8, 5})) == CommandError::Ok);
    pass(*g, 1);
    REQUIRE(g->successorUnits(0).size() == 1u);
    CHECK_EQ(g->submit(Command::chooseSuccessor(0, Succession::Regent)), CommandError::CannotSucceed);
    CHECK_EQ(g->submit(Command::chooseSuccessor(0, Succession::Unit, veteran)), CommandError::Ok);
    CHECK(!g->state().unit(veteran));
    REQUIRE(g->leaderOf(0));
    CHECK_EQ(g->state().players[0].leaderName, std::string("England Warlord"));
    CHECK(g->leaderOf(0)->promotions == std::vector<TypeIndex>{rules().promotion("PROMOTION_SOVEREIGN_WEAPON_MASTER")});  // combat-heavy (§5)
    (void)leader;
}


TEST(a_captured_leader_holds_the_throne_until_abandoned) {
    Fall f = fallScenario();
    Game& g = *f.game;
    const int capitalLoyalty = g.state().city(f.capital)->loyalty, eraScore = g.state().players[0].eraScore;
    REQUIRE(g.submit(Command::attack(1, f.sword, {8, 5})) == CommandError::Ok);
    const Player& p = g.state().players[0];
    CHECK_EQ(p.captor, 1);
    CHECK_EQ(p.capturedTurn, g.state().turn);
    CHECK(!p.successionPending);
    CHECK_EQ(g.state().city(f.capital)->loyalty, capitalLoyalty);  // a capture costs nothing while the ransom is open
    CHECK_EQ(p.eraScore, eraScore);
    pass(g, 1);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Heir)), CommandError::CannotSucceed);
    pass(g, 6);
    CHECK_EQ(g.state().players[0].interregnumTurns, rules().globalInt("LEADER_INTERREGNUM_TURNS"));  // frozen while held
    const int held = g.state().city(f.capital)->loyalty, era = g.state().players[0].eraScore;
    CHECK_EQ(g.submit(Command::abandonLeader(0)), CommandError::Ok);
    CHECK(g.state().players[0].captor == kNoPlayer && g.state().players[0].successionPending);
    // Abandoning the captive costs more loyalty than a killed leader (§5).
    REQUIRE(rules().globalInt("LEADER_ABANDON_LOYALTY") > rules().globalInt("LEADER_LOSS_LOYALTY"));
    CHECK_EQ(g.state().city(f.capital)->loyalty, std::max(0, held - rules().globalInt("LEADER_ABANDON_LOYALTY")));
    CHECK_EQ(g.state().players[0].eraScore, era - std::min(era, rules().globalInt("LEADER_LOSS_ERA_SCORE")));
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Heir)), CommandError::Ok);
    CHECK(g.leaderOf(0));
}

TEST(regicide_eliminates_the_player) {
    Fall f = fallScenario(true);
    Game& g = *f.game;
    REQUIRE(g.submit(Command::attack(1, f.sword, {8, 5})) == CommandError::Ok);
    CHECK(!g.state().players[0].alive);
    CHECK_EQ(g.state().city(f.capital)->owner, 1);  // the captor takes the cities
    for (const Unit& u : g.state().units) CHECK(u.owner != 0);
    CHECK(g.gameOver());  // the last major standing wins (Domination: it holds every capital)
}

TEST(ai_crowns_a_successor) {
    Fall f = fallScenario();
    Game& g = *f.game;
    REQUIRE(g.submit(Command::rangedAttack(1, f.archer, {8, 5})) == CommandError::Ok);
    pass(g, 1);
    REQUIRE(g.state().currentPlayer == 0);
    ai::playTurn(g);
    CHECK(g.leaderOf(0));
    CHECK(!g.state().players[0].successionPending);
    CHECK(g.state().currentPlayer != 0);  // and the turn ended
}

// ---- assassins, levelling and the aura (leader doc §1, §3, §6)

namespace {
TypeIndex promo(const char* id) { return rules().promotion(id); }

void addEncampment(GameState& s, CityId city) {
    City& c = *std::find_if(s.cities.begin(), s.cities.end(), [&](const City& x) { return x.id == city; });
    c.districts.push_back({rules().district("DISTRICT_ENCAMPMENT"), Hex{c.pos.x + 1, c.pos.y + 1}, true});
}

void giveCivic(GameState& s, PlayerId p, const char* civic) {
    Player& pl = s.players[static_cast<size_t>(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.civics.done[at(rules().civic(civic))] = 1;
}

Agent addAgent(GameState& s, PlayerId owner, PlayerId target, int level) {
    Agent a;
    a.id = s.nextAgentId++;
    a.owner = owner;
    a.target = target;
    a.level = level;
    s.agents.push_back(a);
    return a;
}
}  // namespace

TEST(assassins_need_an_encampment_the_civic_and_capacity) {
    CityId plain = kNoCity, camp = kNoCity;
    auto g = duel(
        [&](GameState& s) {
            plain = addCity(s, 0, {2, 2}, true);
            camp = addCity(s, 0, {8, 6}, false);
            addEncampment(s, camp);
            giveCivic(s, 0, "CIVIC_POLITICAL_PHILOSOPHY");
        },
        false);
    const ProductionItem assassin{ProductionKind::Unit, rules().unit("UNIT_ASSASSIN")};
    CHECK(!g->canProduce(*g->state().city(plain), assassin));
    CHECK(g->canProduce(*g->state().city(camp), assassin));
    CHECK_EQ(g->agentCapacity(0), 1);
    GameState s = g->state();
    addAgent(s, 0, kNoPlayer, 1);
    auto full = Game::fromScenario(rules(), std::move(s));
    CHECK(!full->canProduce(*full->state().city(camp), assassin));  // capacity reached
}

TEST(sending_assassins) {
    int32_t id = 0;
    auto g = duel([&](GameState& s) { id = addAgent(s, 0, kNoPlayer, 1).id; }, false);
    CHECK_EQ(g->submit(Command::sendAssassin(0, id, 0)), CommandError::CannotSendAgent);  // not yourself
    CHECK_EQ(g->submit(Command::sendAssassin(0, id + 5, 1)), CommandError::CannotSendAgent);
    CHECK_EQ(g->submit(Command::sendAssassin(0, id, 1)), CommandError::Ok);
    CHECK_EQ(g->agent(id)->target, 1);
    CHECK_EQ(g->agent(id)->travel, rules().globalInt("ASSASSIN_TRAVEL_TURNS"));
}

TEST(guarded_leader_in_a_city_gives_no_opening) {
    UnitId leader = 0;
    int32_t id = 0;
    auto g = duel(
        [&](GameState& s) {
            addCity(s, 0, {5, 5}, true);
            leader = addLeader(s, 0, {5, 5});
            addUnit(s, "UNIT_WARRIOR", 0, {6, 5});
            id = addAgent(s, 1, 0, 4).id;
        },
        false);
    CHECK(!g->leaderExposed(*g->state().unit(leader)));
    pass(*g, 10);
    CHECK(g->leaderOf(0) && g->leaderOf(0)->hp == 100);
    CHECK(g->agent(id) && g->agent(id)->target == 0);  // still waiting
    CHECK(g->state().events.empty());
}

TEST(assassins_strike_exposed_leaders) {
    // Over many seeds a Recruit against a lone, hurt leader sometimes strikes and sometimes
    // fails; every attempt leaves a matching event.
    int hits = 0, misses = 0;
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        UnitId leader = 0;
        int32_t id = 0;
        auto g = duel(
            [&](GameState& s) {
                s.rng.seed(seed);
                addCity(s, 0, {2, 2}, true);
                leader = addLeader(s, 0, {9, 6});
                s.units.back().hp = 40;
                id = addAgent(s, 1, 0, 1).id;
            },
            false);
        const int odds = g->assassinSuccessPercent(*g->agent(id), *g->state().unit(leader));
        CHECK(odds > 50);  // assassins usually outmatch a lone leader (§6)
        pass(*g, 2);       // one world turn
        REQUIRE(g->state().events.size() == 1u);
        const GameEvent& e = g->state().events.back();
        CHECK(e.actor == 1 && e.target == 0);
        // A hit or a captured assassin names the sender (§6); a killed leader adds the killer's grievance (§5).
        const int sent = rules().globalInt("ASSASSIN_SENDER_GRIEVANCES");
        const int killed = rules().globalInt("LEADER_KILLED_GRIEVANCES");
        if (e.kind == EventKind::AssassinKilledLeader) CHECK(g->grievances(0, 1) > killed && g->grievances(0, 1) <= sent + killed);
        else if (e.kind == EventKind::AssassinKilled) CHECK_EQ(g->grievances(0, 1), 0);
        else CHECK(g->grievances(0, 1) > 0 && g->grievances(0, 1) <= sent);
        if (e.kind == EventKind::AssassinKilledLeader) {
            ++hits;
            CHECK(!g->leaderOf(0) && g->state().players[0].successionPending);
            CHECK_EQ(g->agent(id)->level, 2);  // it comes home a level higher
            CHECK_EQ(g->agent(id)->target, kNoPlayer);
        } else if (e.kind == EventKind::AssassinWoundedLeader) {
            ++hits;
            CHECK(g->leaderOf(0)->hp < 40);
        } else {
            ++misses;
            CHECK(!g->agent(id));  // dead or captured
        }
    }
    CHECK(hits > 0);
    CHECK(misses > 0);
}

TEST(guards_lower_assassin_odds) {
    UnitId lone = 0, guarded = 0;
    int32_t id = 0;
    auto g = duel(
        [&](GameState& s) {
            lone = addLeader(s, 0, {3, 3});
            guarded = addLeader(s, 1, {10, 8});
            addUnit(s, "UNIT_WARRIOR", 1, {10, 8});
            addUnit(s, "UNIT_WARRIOR", 1, {11, 8});
            id = addAgent(s, 0, 1, 1).id;
        },
        false);
    const Agent& a = *g->agent(id);
    CHECK(g->leaderDefenseVsAssassin(*g->state().unit(guarded)) >= g->leaderDefenseVsAssassin(*g->state().unit(lone)) + 20);
    CHECK(g->assassinSuccessPercent(a, *g->state().unit(guarded)) < g->assassinSuccessPercent(a, *g->state().unit(lone)));
}

TEST(leader_promotions_one_branch_per_reign) {
    UnitId leader = 0;
    auto g = duel(
        [&](GameState& s) {
            leader = addLeader(s, 0, {5, 5});
            s.units.back().xp = 100;
            s.units.back().promotions = {promo("PROMOTION_SOVEREIGN_WEAPON_MASTER"), promo("PROMOTION_SOVEREIGN_MARSHAL"),
                                         promo("PROMOTION_SOVEREIGN_WARY")};
        },
        false);
    // Tier 1 of another branch is fine; its tier 2 is not once Warlord is finished.
    CHECK(g->canPromote(leader, promo("PROMOTION_SOVEREIGN_OVERSEER")));
    CHECK(!g->canPromote(leader, promo("PROMOTION_SOVEREIGN_SPYMASTER")));
    CHECK(!g->canPromote(leader, promo("PROMOTION_BATTLECRY")));  // not its class
    CHECK_EQ(g->unitEffectTotal(*g->state().unit(leader), UnitEffectKind::AssassinDefense), 15);
}

TEST(presence_aura_and_builder_king) {
    UnitId leader = 0, nearUnit = 0, farUnit = 0, enemy = 0;
    CityId city = kNoCity;
    auto g = duel([&](GameState& s) {
        city = addCity(s, 0, {4, 4}, true);
        leader = addLeader(s, 0, {4, 4});
        nearUnit = addUnit(s, "UNIT_WARRIOR", 0, {6, 4});
        farUnit = addUnit(s, "UNIT_WARRIOR", 0, {12, 4});
        enemy = addUnit(s, "UNIT_WARRIOR", 1, {13, 4});
    });
    const Unit& e = *g->state().unit(enemy);
    const int aura = rules().globalInt("LEADER_AURA_STRENGTH");
    CHECK_EQ(g->combatStrength(*g->state().unit(nearUnit), e, true, false),
             g->combatStrength(*g->state().unit(farUnit), e, true, false) + aura);
    const Fixed before = g->cityReport(city).yields[static_cast<size_t>(YieldType::Production)];
    GameState s = g->state();
    s.unit(leader)->promotions = {promo("PROMOTION_SOVEREIGN_OVERSEER")};
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g2->cityReport(city).yields[static_cast<size_t>(YieldType::Production)], before + Fixed::fromInt(2));
}

TEST(presence_aura_does_not_stack_with_a_great_general) {
    // Leader doc section 1: the higher of the leader's aura and a Great General's applies.
    UnitId both = 0, leaderOnly = 0, generalOnly = 0, neither = 0, enemy = 0;
    auto g = duel([&](GameState& s) {
        addLeader(s, 0, {4, 4});
        const GreatPersonType& h = rules().greatPeople[at(rules().greatPerson("GREAT_PERSON_HANNIBAL_BARCA"))];
        addUnit(s, rules().units[at(rules().greatPersonClasses[at(h.cls)].unit)].id.c_str(), 0, {4, 8});
        s.units.back().greatPerson = rules().greatPerson("GREAT_PERSON_HANNIBAL_BARCA");
        s.units.back().charges = h.charges;
        both = addUnit(s, "UNIT_SWORDSMAN", 0, {4, 6});
        leaderOnly = addUnit(s, "UNIT_SWORDSMAN", 0, {3, 3});
        generalOnly = addUnit(s, "UNIT_SWORDSMAN", 0, {4, 9});
        neither = addUnit(s, "UNIT_SWORDSMAN", 0, {12, 4});
        enemy = addUnit(s, "UNIT_WARRIOR", 1, {13, 4});
    });
    const Unit& e = *g->state().unit(enemy);
    auto strength = [&](UnitId id) { return g->combatStrength(*g->state().unit(id), e, true, false); };
    const int leaderAura = rules().globalInt("LEADER_AURA_STRENGTH");
    REQUIRE(g->greatPersonAuraStrength(*g->state().unit(both)) == 5);
    REQUIRE(leaderAura < 5);
    CHECK_EQ(strength(leaderOnly), strength(neither) + leaderAura);
    CHECK_EQ(strength(generalOnly), strength(neither) + 5);
    CHECK_EQ(strength(both), strength(neither) + 5);
    // A Marshal's aura (3 + 2) ties the general's; with more it would be the leader's that applies.
    GameState s = g->state();
    for (Unit& u : s.units)
        if (u.type == rules().unit("UNIT_SOVEREIGN"))
            u.promotions = {rules().promotion("PROMOTION_SOVEREIGN_WEAPON_MASTER"), rules().promotion("PROMOTION_SOVEREIGN_MARSHAL")};
    auto g2 = Game::fromScenario(rules(), std::move(s));
    const Unit& e2 = *g2->state().unit(enemy);
    CHECK_EQ(g2->combatStrength(*g2->state().unit(both), e2, true, false),
             g2->combatStrength(*g2->state().unit(neither), e2, true, false) + std::max(leaderAura + 2, 5));
}

TEST(fear_angers_coreligionists_and_the_old_owners_allies) {
    // Leader doc section 4: Fear brings a small grievance from civs that share the ruler's religion or are
    // allied with the city's original owner.
    CityId c = kNoCity;
    GameState s = flatState(16, 12, 4);
    c = addCity(s, 0, {5, 5}, false);
    s.cities.back().originalOwner = 2;
    addCity(s, 0, {10, 8}, true);
    addLeader(s, 0, {5, 5});
    addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
    s.players[0].religion = 0;
    s.players[3].religion = 0;  // a coreligionist
    for (PlayerId a : {1, 2}) {  // 1 and 2 are allies
        const Relation unused{};
        s.players[at(a)].relations.assign(4, unused);
        Relation& ally = s.players[at(a)].relations[at(3 - a)];
        ally.alliance = AllianceType::Research;
        ally.allianceUntil = 100;
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->alliance(1, 2) == AllianceType::Research);
    REQUIRE(g->submit(Command::cityStance(0, c, Stance::Fear)) == CommandError::Ok);
    const int amount = rules().globalInt("STANCE_FEAR_GRIEVANCES");
    REQUIRE(amount > 0);
    CHECK_EQ(g->grievances(1, 0), amount);  // allied with the original owner
    CHECK_EQ(g->grievances(2, 0), 0);       // the original owner itself is not its own ally
    CHECK_EQ(g->grievances(3, 0), amount);  // shares the ruler's religion
}

TEST(the_leader_gains_xp_from_the_civs_deeds) {
    // Leader doc section 3: founding cities, city-state quests and historic moments.
    UnitId leader = 0, settler = 0;
    auto g = duel(
        [&](GameState& s) {
            addCity(s, 0, {3, 3}, true);
            leader = addLeader(s, 0, {9, 6});
            settler = addUnit(s, "UNIT_SETTLER", 0, {9, 8});
        },
        false);
    auto xp = [&] { return g->state().unit(leader)->xp; };
    REQUIRE(xp() == 0);
    const int scoreAtStart = g->state().players[0].eraScore;
    REQUIRE(g->submit(Command::foundCity(0, settler)) == CommandError::Ok);
    const int founded = xp();
    CHECK_EQ(founded, rules().globalInt("LEADER_XP_FOUND_CITY") +  // and any historic moment the city made
                          (g->state().players[0].eraScore - scoreAtStart) * rules().globalInt("LEADER_XP_PER_ERA_SCORE"));
    const int scoreBefore = g->state().players[0].eraScore;
    REQUIRE(g->completeItem(g->stateMutForTests().cities[0], {ProductionKind::Building, rules().building("BUILDING_PYRAMIDS")}));
    const int moment = g->state().players[0].eraScore - scoreBefore;
    REQUIRE(moment > 0);  // a wonder is a historic moment
    CHECK_EQ(xp(), founded + moment * rules().globalInt("LEADER_XP_PER_ERA_SCORE"));
    // A quest from a city-state (none here, so a quest list with one entry is made up for it).
    GameState s = g->state();
    s.quests.push_back({1, 0, QuestKind::TrainUnit, rules().unit("UNIT_WARRIOR")});
    auto g2 = Game::fromScenario(rules(), std::move(s));
    const int start = g2->state().unit(leader)->xp;
    g2->questDone(0, QuestKind::TrainUnit, rules().unit("UNIT_WARRIOR"));
    CHECK_EQ(g2->state().unit(leader)->xp, start + rules().globalInt("LEADER_XP_QUEST"));
}

TEST(presence_aura_steadies_the_city_it_stands_in) {
    // The city whose land the leader stands on gains loyalty; Statesman promotions add to it (§1, §3).
    CityId home = kNoCity, other = kNoCity, theirs = kNoCity;
    auto at = [&](Hex pos, std::vector<TypeIndex> promos) {
        return duel(
            [&](GameState& s) {
                home = addCity(s, 0, {3, 4}, true);
                other = addCity(s, 0, {9, 4}, false);
                theirs = addCity(s, 1, {13, 9}, true);
                addLeader(s, 0, pos);
                s.units.back().promotions = promos;
            },
            false);
    };
    const auto away = at({6, 8}, {});
    const auto near = at({4, 4}, {});  // home's land beside the center
    const int aura = rules().globalInt("LEADER_AURA_LOYALTY");
    REQUIRE(aura > 0);
    CHECK_EQ(near->loyaltyPerTurn(home), away->loyaltyPerTurn(home) + Fixed::fromInt(aura));
    CHECK_EQ(near->loyaltyPerTurn(other), away->loyaltyPerTurn(other));
    CHECK_EQ(near->loyaltyPerTurn(theirs), away->loyaltyPerTurn(theirs));
    const auto wary = at({9, 4}, {promo("PROMOTION_SOVEREIGN_WARY")});  // in the other city now
    CHECK_EQ(wary->loyaltyPerTurn(other), away->loyaltyPerTurn(other) + Fixed::fromInt(aura + 2));
    CHECK_EQ(wary->loyaltyPerTurn(home), away->loyaltyPerTurn(home));
    const auto spymaster = at({4, 4}, {promo("PROMOTION_SOVEREIGN_WARY"), promo("PROMOTION_SOVEREIGN_SPYMASTER")});
    CHECK_EQ(spymaster->loyaltyPerTurn(home), away->loyaltyPerTurn(home) + Fixed::fromInt(aura + 4));
    const auto builder = at({4, 4}, {promo("PROMOTION_SOVEREIGN_OVERSEER")});  // not a Statesman promotion
    CHECK_EQ(builder->loyaltyPerTurn(home), near->loyaltyPerTurn(home));
}

TEST(a_slain_leader_shakes_every_city_and_the_era) {
    // §5: a killed leader costs every city loyalty at once and the empire era score; the enemy's cities are untouched.
    for (const int score : {9, 1}) {
        UnitId archer = 0;
        CityId home = kNoCity, shaky = kNoCity, theirs = kNoCity;
        auto g = duel([&](GameState& s) {
            home = addCity(s, 0, {2, 2}, true);
            shaky = addCity(s, 0, {2, 8}, false);
            s.cities.back().loyalty = 4;
            theirs = addCity(s, 1, {13, 9}, true);
            s.cities.back().loyalty = 90;
            addLeader(s, 0, {8, 5});
            s.units.back().hp = 1;
            archer = addUnit(s, "UNIT_ARCHER", 1, {10, 5});
            Player& p = s.players[0];
            p.eraScore = score;
            p.eraScoreTotal = score + 40;
        });
        pass(*g, 1);
        const int homeLoyalty = g->state().city(home)->loyalty, theirLoyalty = g->state().city(theirs)->loyalty;
        const int era = g->state().players[0].eraScore, total = g->state().players[0].eraScoreTotal;
        REQUIRE(g->submit(Command::rangedAttack(1, archer, {8, 5})) == CommandError::Ok);
        REQUIRE(!g->leaderOf(0));
        const int drop = rules().globalInt("LEADER_LOSS_LOYALTY"), lost = std::min(era, rules().globalInt("LEADER_LOSS_ERA_SCORE"));
        REQUIRE(g->state().city(shaky)->loyalty < drop && lost > 0);
        CHECK_EQ(g->state().city(home)->loyalty, homeLoyalty - drop);
        CHECK_EQ(g->state().city(shaky)->loyalty, 0);  // never below 0
        CHECK_EQ(g->state().city(theirs)->loyalty, theirLoyalty);
        CHECK_EQ(g->state().players[0].eraScore, era - lost);
        CHECK_EQ(g->state().players[0].eraScoreTotal, total - lost);
    }
}

TEST(an_heir_keeps_one_promotion) {
    UnitId leader = 0, archer = 0;
    auto g = duel([&](GameState& s) {
        addCity(s, 0, {2, 2}, true);
        leader = addLeader(s, 0, {8, 5});
        s.units.back().hp = 1;
        s.units.back().promotions = {promo("PROMOTION_SOVEREIGN_WARY")};
        archer = addUnit(s, "UNIT_ARCHER", 1, {10, 5});
    });
    pass(*g, 1);
    REQUIRE(g->submit(Command::rangedAttack(1, archer, {8, 5})) == CommandError::Ok);
    pass(*g, 1);
    CHECK_EQ(g->submit(Command::chooseSuccessor(0, Succession::Heir, kNoUnit, promo("PROMOTION_SOVEREIGN_MARSHAL"))),
             CommandError::CannotSucceed);  // it never had that one
    CHECK_EQ(g->submit(Command::chooseSuccessor(0, Succession::Heir, kNoUnit, promo("PROMOTION_SOVEREIGN_WARY"))), CommandError::Ok);
    REQUIRE(g->leaderOf(0));
    CHECK(g->leaderOf(0)->promotions == std::vector<TypeIndex>{promo("PROMOTION_SOVEREIGN_WARY")});
    (void)leader;
}

namespace {
// Player 1 holds player 0's ruler; the throne stands empty, its loadout and promotions put by (§5).
std::unique_ptr<Game> heldRuler(int gold, int capturedTurn, bool humanOwner = false) {
    return duel(
        [&](GameState& s) {
            addCity(s, 0, {2, 2}, true);
            addCity(s, 1, {13, 9}, true);
            s.turn = 20;
            Player& p = s.players[0];
            Game::fitPlayerToRules(p, rules());
            p.captor = 1;
            p.capturedTurn = capturedTurn;
            p.savedGear = {gear("GEAR_SPEAR"), gear("GEAR_HIDE"), kNone};
            p.savedPromotions = {promo("PROMOTION_SOVEREIGN_WARY")};
            p.interregnumTurns = rules().globalInt("LEADER_INTERREGNUM_TURNS");
            p.gold = Fixed::fromInt(gold);
            p.human = humanOwner;
            for (Player& x : s.players) x.met.assign(s.players.size(), 1);
        },
        false);
}
}  // namespace

TEST(a_captured_ruler_is_ransomed_home) {
    auto g = heldRuler(500, 17);
    REQUIRE(!g->leaderOf(0));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].captor, 1);
    CHECK_EQ(loaded->state().players[0].capturedTurn, 17);
    // Only the captor offers it, and only to the captive's own civ.
    const DealItem ruler{DealItemKind::Ruler, 1, 0, kNone};
    const std::vector<DealItem> offer = g->offerableItems(1, 0);
    CHECK(std::any_of(offer.begin(), offer.end(), [](const DealItem& i) { return i.kind == DealItemKind::Ruler && i.amount == 0; }));
    const std::vector<DealItem> back = g->offerableItems(0, 1);
    CHECK(std::none_of(back.begin(), back.end(), [](const DealItem& i) { return i.kind == DealItemKind::Ruler; }));
    CHECK(g->dealProblem({0, 0, 1, 0, {{DealItemKind::Ruler, 0, 1, kNone}}}) != CommandError::Ok);
    CHECK(g->dealProblem({0, 0, 1, 0, {{DealItemKind::Ruler, 1, 1, kNone}}}) != CommandError::Ok);
    CHECK(g->dealProblem({0, 0, 1, 0, {ruler, ruler}}) != CommandError::Ok);
    CHECK(describeDealItem(rules(), g->state(), ruler).find("frees " + g->state().players[0].leaderName) != std::string::npos);
    // Bought back for gold: home in the capital with its loadout and promotions; the interregnum ends next turn.
    const std::vector<DealItem> terms = {ruler, {DealItemKind::Gold, 0, 300, kNone}};
    CHECK(g->dealValue(0, {0, 0, 1, 0, terms}) > 0);
    CHECK(!g->wouldAccept(1, {0, 0, 1, 0, {ruler, {DealItemKind::Gold, 0, 50, kNone}}}));  // too cheap for the captor
    REQUIRE(g->wouldAccept(1, {0, 0, 1, 0, terms}));
    REQUIRE(g->submit(Command::proposeDeal(0, 1, terms)) == CommandError::Ok);
    const Unit* l = g->leaderOf(0);
    REQUIRE(l);
    CHECK(l->pos == hx(2, 2));
    CHECK_EQ(l->gear[0], gear("GEAR_SPEAR"));
    CHECK(l->promotions == std::vector<TypeIndex>{promo("PROMOTION_SOVEREIGN_WARY")});
    const Player& p = g->state().players[0];
    CHECK_EQ(p.captor, kNoPlayer);
    CHECK(!p.successionPending);
    CHECK_EQ(p.interregnumTurns, 1);
    CHECK_EQ(p.gold, Fixed::fromInt(200));
    CHECK_EQ(p.leaderName, std::string("Elizabeth I"));  // the same ruler, not an heir
}

TEST(the_ai_ransoms_its_ruler_or_gives_it_up) {
    // Waiting, it buys its ruler back at the lowest price the captor takes.
    auto g = heldRuler(1000, 19);
    const Fixed captorGold = g->state().players[1].gold;
    ai::playTurn(*g);
    REQUIRE(g->leaderOf(0));
    CHECK_EQ(g->state().players[0].captor, kNoPlayer);
    const int price = 100 + 50 * 2;  // a level 2 ruler
    const Fixed gained = g->state().players[1].gold - captorGold;  // the ransom, and whatever else the turn brought
    CHECK(gained >= Fixed::fromInt(price) && gained < Fixed::fromInt(2 * price));
    CHECK_EQ(g->state().players[0].leaderName, std::string("Elizabeth I"));
    // Without the gold it waits, then gives the captive up after kRansomPatience (10) turns and crowns the heir.
    auto poor = heldRuler(0, 11);
    ai::playTurn(*poor);
    CHECK_EQ(poor->state().players[0].captor, 1);
    CHECK(!poor->leaderOf(0));
    auto spent = heldRuler(0, 10);
    ai::playTurn(*spent);
    CHECK_EQ(spent->state().players[0].captor, kNoPlayer);
    REQUIRE(spent->leaderOf(0));
    CHECK_EQ(spent->state().players[0].leaderName, std::string("James I"));
}

TEST(the_ai_gives_up_a_ruler_no_one_will_ransom) {
    // A captor that is no major civ (here a barbarian player) takes no ransom: the captive is given up at once.
    GameState s = flatState(16, 12, 3);
    addCity(s, 0, {2, 2}, true);
    addCity(s, 1, {13, 9}, true);
    s.players[2].barbarian = true;
    Player& p = s.players[0];
    Game::fitPlayerToRules(p, rules());
    p.captor = 2;
    p.capturedTurn = s.turn;
    p.gold = Fixed::fromInt(1000);
    p.interregnumTurns = rules().globalInt("LEADER_INTERREGNUM_TURNS");
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    CHECK_EQ(g->state().players[0].captor, kNoPlayer);
    CHECK(g->leaderOf(0));
}

TEST(the_ai_offers_a_human_its_ruler_back) {
    auto g = heldRuler(1000, 19, true);
    pass(*g, 1);  // the human ends its turn; the captor plays
    REQUIRE(g->state().currentPlayer == 1);
    ai::playTurn(*g);
    const auto offered = std::find_if(g->state().deals.begin(), g->state().deals.end(), [](const Deal& d) {
        return d.from == 1 && d.to == 0 && std::any_of(d.items.begin(), d.items.end(), [](const DealItem& i) { return i.kind == DealItemKind::Ruler && i.amount == 0; });
    });
    REQUIRE(offered != g->state().deals.end());
    const auto gold = std::find_if(offered->items.begin(), offered->items.end(), [](const DealItem& i) { return i.kind == DealItemKind::Gold; });
    REQUIRE(gold != offered->items.end());
    CHECK_EQ(gold->from, 0);
    CHECK_EQ(gold->amount, 2 * (100 + 50 * 2));
}

TEST(leader_goals_put_the_most_pressing_first) {
    CityId calm = kNoCity, restless = kNoCity;
    UnitId mine = kNoUnit;
    auto g = duel([&](GameState& s) {
        calm = addCity(s, 0, hx(3, 5), true);
        restless = addCity(s, 0, hx(10, 9), false);
        s.cities.back().loyalty = 20;  // below Unrest
        mine = addLeader(s, 0, hx(5, 5));
        addLeader(s, 1, hx(7, 5));
    }, false);
    // A city near revolt, then the rival leader two plots away; no promotion is near at 0 XP.
    std::vector<LeaderGoal> goals = g->leaderGoals(0);
    REQUIRE(goals.size() >= 2u);
    CHECK(goals[0].kind == LeaderGoalKind::CityUnrest);
    CHECK_EQ(goals[0].id, restless);
    CHECK_EQ(goals[0].value, 20);
    CHECK(std::none_of(goals.begin(), goals.end(), [&](const LeaderGoal& x) { return x.kind == LeaderGoalKind::CityUnrest && x.id == calm; }));
    CHECK(goals[1].kind == LeaderGoalKind::RivalLeaderNear);
    CHECK_EQ(goals[1].id, 1);
    CHECK_EQ(goals[1].value, 2);
    CHECK(goals.back().kind == LeaderGoalKind::RivalLeaderNear);

    // An assassin in place goes first, and XP close to the next level shows last.
    GameState s = g->state();
    addAgent(s, 1, 0, 1);
    for (Unit& u : s.units)
        if (u.id == mine) u.xp = g->xpForNextLevel(u) - 3;
    auto h = Game::fromScenario(rules(), std::move(s));
    goals = h->leaderGoals(0);
    REQUIRE(goals.size() >= 4u);
    CHECK(goals.front().kind == LeaderGoalKind::AssassinNear);
    CHECK_EQ(goals.front().value, 1);  // the leader stands outside a city: exposed
    CHECK(goals.back().kind == LeaderGoalKind::PromotionSoon);
    CHECK_EQ(goals.back().value, 3);
    // An assassin still travelling is not reported, and nobody else's leader has goals here.
    GameState t = h->state();
    t.agents.back().travel = 2;
    auto k = Game::fromScenario(rules(), std::move(t));
    goals = k->leaderGoals(0);
    CHECK(std::none_of(goals.begin(), goals.end(), [](const LeaderGoal& x) { return x.kind == LeaderGoalKind::AssassinNear; }));
}
TEST(chronicle_records_wars_rulers_and_successions) {
    UnitId enemy = 0;
    auto g = duel([&](GameState& s) {
        addCity(s, 0, hx(2, 2), true);
        addLeader(s, 0, hx(5, 5));
        s.units.back().hp = 1;
        enemy = addUnit(s, "UNIT_SWORDSMAN", 1, hx(6, 5));
    }, false);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);  // a surprise war
    pass(*g, 1);
    REQUIRE(g->submit(Command::attack(1, enemy, hx(5, 5))) == CommandError::Ok);
    REQUIRE(!g->leaderOf(0));
    pass(*g, 1);
    if (g->state().players[0].captor != kNoPlayer) REQUIRE(g->submit(Command::abandonLeader(0)) == CommandError::Ok);
    REQUIRE(g->submit(Command::chooseSuccessor(0, Succession::Heir)) == CommandError::Ok);
    const std::string me = rules().civs[0].name, them = rules().civs[1].name;
    const std::vector<std::string> lines = g->chronicleLines(0);
    const auto has = [&](const std::string& what) {
        return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(what) != std::string::npos; });
    };
    CHECK(has("Turn 1: " + me + " declared a surprise war on " + them + "."));
    CHECK(has(them + " captured the ruler of " + me) || has(them + " slew the ruler of " + me));
    CHECK(has(rules().dynastyOf(g->state().players[0].civ)->names[1] + " took the throne of " + me));
    // The other side's chronicle has the war too; it lives in the save.
    const std::vector<std::string> theirs = g->chronicleLines(1);
    CHECK(!theirs.empty() && theirs[0].find("surprise war") != std::string::npos);
    std::string err;
    auto back = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(back);
    CHECK(back->chronicleLines(0) == lines);
    // The Hall of Sovereigns record names the new ruler and the reign's state.
    const std::string hall = g->hallEntry(0);
    CHECK(hall.find(rules().dynastyOf(g->state().players[0].civ)->names[1] + " of " + me) == 0);
    CHECK(hall.find("the reign goes on") != std::string::npos);
    CHECK(Game::chronicleWorthy(EventKind::LeaderLost) && !Game::chronicleWorthy(EventKind::DealProposed));
    // Taking a ruler in battle is an achievement for the captor.
    const std::vector<std::string> theirs1 = g->achievementsEarned(1), mine = g->achievementsEarned(0);
    CHECK(std::find(theirs1.begin(), theirs1.end(), "ACH_KINGTAKER") != theirs1.end());
    CHECK(std::find(mine.begin(), mine.end(), "ACH_KINGTAKER") == mine.end());
}
TEST(achievements_follow_how_a_game_ended) {
    const Rules& r = rules();
    REQUIRE(!r.achievements.empty());
    for (const AchievementType& a : r.achievements) {
        CHECK(a.unlock.empty() || std::any_of(r.cosmetics.begin(), r.cosmetics.end(), [&](const CosmeticType& c) { return c.id == a.unlock; }));
    }
    GameState s = sovtest::flatState(16, 12, 2);
    s.setup.speed = "GAMESPEED_SHORT_REIGN";
    s.winner = 0;
    s.victory = Victory::Science;
    auto g = Game::fromScenario(r, std::move(s));
    const std::vector<std::string> won = g->achievementsEarned(0);
    for (const char* id : {"ACH_CROWNED_IN_GLORY", "ACH_STARGAZER", "ACH_SWIFT_REIGN"}) CHECK(std::find(won.begin(), won.end(), id) != won.end());
    CHECK(std::find(won.begin(), won.end(), "ACH_PATRON") == won.end());
    CHECK(g->achievementsEarned(1).empty());
}

namespace {
// A leader killed with no heir left; player 0 also holds Hannibal Barca and has appointed Victor and Magnus.
struct Pool {
    std::unique_ptr<Game> game;
    UnitId general = 0, veteran = 0;
    CityId seat = kNoCity;
};
Pool poolScenario() {
    Pool f;
    UnitId archer = 0;
    f.game = duel([&](GameState& s) {
        addCity(s, 0, {2, 2}, true);
        f.seat = addCity(s, 0, {6, 9}, false);
        addLeader(s, 0, {8, 5});
        s.units.back().hp = 1;
        const GreatPersonType& h = rules().greatPeople[at(rules().greatPerson("GREAT_PERSON_HANNIBAL_BARCA"))];
        f.general = addUnit(s, rules().units[at(rules().greatPersonClasses[at(h.cls)].unit)].id.c_str(), 0, {3, 3});
        s.units.back().greatPerson = rules().greatPerson("GREAT_PERSON_HANNIBAL_BARCA");
        addUnit(s, rules().units[at(rules().greatPersonClasses[at(h.cls)].unit)].id.c_str(), 0, {4, 3});  // no great person in it
        f.veteran = addUnit(s, "UNIT_WARRIOR", 0, {3, 4});
        s.units.back().promotions = {rules().promotion("PROMOTION_BATTLECRY"), rules().promotion("PROMOTION_TORTOISE"),
                                     rules().promotion("PROMOTION_COMMANDO")};
        archer = addUnit(s, "UNIT_ARCHER", 1, {10, 5});
        Player& p = s.players[0];
        Game::fitPlayerToRules(p, rules());
        p.dynastyNext = 3;
        Governor victor;
        victor.type = rules().governor("GOVERNOR_VICTOR");
        victor.city = f.seat;
        Governor magnus;
        magnus.type = rules().governor("GOVERNOR_MAGNUS");
        p.governors = {victor, magnus};
        p.governorTitlesSpent = 2;
    });
    pass(*f.game, 1);
    REQUIRE(f.game->submit(Command::rangedAttack(1, archer, {8, 5})) == CommandError::Ok);
    pass(*f.game, 1);
    return f;
}
}  // namespace

TEST(a_great_general_can_take_the_throne) {
    Pool f = poolScenario();
    Game& g = *f.game;
    CHECK(g.successorGreatPeople(0) == std::vector<UnitId>{f.general});  // not the one with no great person in it
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::GreatPerson, f.veteran)), CommandError::CannotSucceed);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::GreatPerson, f.general)), CommandError::Ok);
    CHECK(!g.state().unit(f.general));  // the Great Person is spent
    REQUIRE(g.leaderOf(0));
    CHECK_EQ(g.leaderOf(0)->level(), 3);  // a higher start, with Marshal's stronger aura
    CHECK(g.leaderOf(0)->promotions ==
          (std::vector<TypeIndex>{promo("PROMOTION_SOVEREIGN_WEAPON_MASTER"), promo("PROMOTION_SOVEREIGN_MARSHAL")}));
    const std::vector<std::string> lines = g.chronicleLines(0);
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.find("A great commander took the throne") != std::string::npos; }));
}

TEST(a_governor_can_take_the_throne) {
    Pool f = poolScenario();
    Game& g = *f.game;
    const TypeIndex victor = rules().governor("GOVERNOR_VICTOR"), magnus = rules().governor("GOVERNOR_MAGNUS");
    CHECK(g.successorGovernors(0) == (std::vector<TypeIndex>{victor, magnus}));
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Governor, rules().governor("GOVERNOR_AMANI"))), CommandError::CannotSucceed);
    // Magnus seeds Builder-King; Victor, below, Warlord.
    GameState copy = g.state();
    auto g2 = Game::fromScenario(rules(), std::move(copy));
    CHECK_EQ(g2->submit(Command::chooseSuccessor(0, Succession::Governor, magnus)), CommandError::Ok);
    CHECK(g2->leaderOf(0)->promotions == std::vector<TypeIndex>{promo("PROMOTION_SOVEREIGN_OVERSEER")});
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Governor, victor)), CommandError::Ok);
    REQUIRE(g.leaderOf(0));
    CHECK(g.leaderOf(0)->promotions == std::vector<TypeIndex>{promo("PROMOTION_SOVEREIGN_WEAPON_MASTER")});
    CHECK(g.leaderOf(0)->pos == hx(2, 2));  // crowned in the capital
    CHECK_EQ(g.state().players[0].leaderName, std::string("Victor"));
    CHECK(!g.governor(0, victor));  // gone from his post, and the title with him
    CHECK_EQ(g.governorTitlesLeft(0), g.governorTitles(0) - 2);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Governor, magnus)), CommandError::CannotSucceed);  // crowned already
    const std::vector<std::string> lines = g.chronicleLines(0);
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.find("A governor took the throne") != std::string::npos; }));
}

TEST(an_ai_crowns_a_great_general_before_a_veteran) {
    Pool f = poolScenario();
    Game& g = *f.game;
    REQUIRE(g.state().currentPlayer == 0);
    ai::playTurn(g);
    REQUIRE(g.leaderOf(0));
    CHECK(!g.state().unit(f.general));
    CHECK(g.state().unit(f.veteran));
    CHECK(g.governor(0, rules().governor("GOVERNOR_VICTOR")));  // never a governor
}

// ---- body doubles (leader doc §8.6)

TEST(body_doubles_are_trained_kept_and_paid_for) {
    CityId city = kNoCity;
    auto g = duel(
        [&](GameState& s) {
            city = addCity(s, 0, {5, 5}, true, 4);
            addLeader(s, 0, {5, 5});
        },
        false);
    const ProductionItem item{ProductionKind::Unit, rules().unit("UNIT_BODY_DOUBLE")};
    REQUIRE(item.type != kNone);
    CHECK(!g->canProduce(*g->state().city(city), item));  // needs Diplomatic Service
    GameState s = g->state();
    giveCivic(s, 0, "CIVIC_DIPLOMATIC_SERVICE");
    auto g2 = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g2->canProduce(*g2->state().city(city), item));
    const Fixed gold = g2->goldPerTurn(0);
    const size_t units = g2->state().units.size();
    REQUIRE(g2->completeItem(g2->stateMutForTests().cities[0], item));
    CHECK_EQ(g2->state().players[0].bodyDoubles, 1);
    CHECK_EQ(g2->state().units.size(), units);  // kept off the map
    CHECK_EQ(g2->goldPerTurn(0), gold - Fixed::fromInt(rules().units[at(item.type)].maintenance));
    CHECK(!g2->canProduce(*g2->state().city(city), item));  // BODY_DOUBLE_MAX is 1
    std::string err;
    auto back = loadGame(rules(), saveGame(*g2), &err);
    REQUIRE(back);
    CHECK_EQ(back->state().players[0].bodyDoubles, 1);
}

TEST(an_assassin_can_strike_a_body_double) {
    // Over many seeds, a strike that would have hit a lone, hurt leader sometimes falls on the double.
    int doubles = 0, others = 0;
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        UnitId leader = 0;
        auto g = duel(
            [&](GameState& s) {
                s.rng.seed(seed);
                addCity(s, 0, {2, 2}, true);
                leader = addLeader(s, 0, {9, 6});
                s.units.back().hp = 40;
                addAgent(s, 1, 0, 1);
                s.players[0].bodyDoubles = 1;
            },
            false);
        pass(*g, 2);
        REQUIRE(g->state().events.size() == 1u);
        const GameEvent& e = g->state().events.back();
        if (e.kind == EventKind::AssassinKilledDouble) {
            ++doubles;
            CHECK_EQ(g->state().players[0].bodyDoubles, 0);
            CHECK(g->leaderOf(0) && g->state().unit(leader)->hp >= 40);  // the ruler is unhurt (it may heal)
            CHECK(g->grievances(0, 1) > 0);                               // and knows the sender
        } else {
            ++others;
            CHECK_EQ(g->state().players[0].bodyDoubles, 1);
        }
    }
    CHECK(doubles > 0);
    CHECK(others > 0);
}

TEST(an_ai_keeps_a_body_double_once_assassins_come) {
    for (const bool assassinCame : {false, true}) {
        CityId city = kNoCity;
        auto g = duel(
            [&](GameState& s) {
                city = addCity(s, 0, {5, 5}, true, 1);  // too small for a Settler
                City& c = s.cities.back();
                c.queue.clear();
                c.buildings.push_back(rules().building("BUILDING_MONUMENT"));
                std::sort(c.buildings.begin(), c.buildings.end());
                addLeader(s, 0, {5, 5});
                addUnit(s, "UNIT_WARRIOR", 0, {5, 5});
                addUnit(s, "UNIT_WARRIOR", 0, {5, 6});  // army enough for one city
                addUnit(s, "UNIT_BUILDER", 0, {4, 5});
                giveCivic(s, 0, "CIVIC_DIPLOMATIC_SERVICE");
                OpinionMemory m;
                m.about = 1;
                m.kind = MemoryKind::Assassin;
                m.amount = -15;
                m.duration = 60;
                if (assassinCame) s.players[0].memories.push_back(m);
                s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
            },
            false);
        ai::playTurn(*g);
        const City& c = *g->state().city(city);
        REQUIRE(!c.queue.empty());
        CHECK_EQ(c.queue.front() == (ProductionItem{ProductionKind::Unit, rules().unit("UNIT_BODY_DOUBLE")}), assassinCame);
    }
}
