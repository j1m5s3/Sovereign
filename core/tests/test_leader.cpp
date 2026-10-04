// The playable leader in classic control (specs/sovereign/leader-character-brainstorm.md
// §1, §2, §5, §8.8): start, gear, escorts, capture, barbarian safety, linked moves.
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"

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
    REQUIRE(g.submit(Command::rangedAttack(1, f.archer, {8, 5})) == CommandError::Ok);  // killed, not captured
    const Player& p = g.state().players[0];
    CHECK(!g.leaderOf(0));
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
    (void)leader;
}

TEST(a_captured_leader_holds_the_throne_until_abandoned) {
    Fall f = fallScenario();
    Game& g = *f.game;
    REQUIRE(g.submit(Command::attack(1, f.sword, {8, 5})) == CommandError::Ok);
    const Player& p = g.state().players[0];
    CHECK_EQ(p.captor, 1);
    CHECK(!p.successionPending);
    pass(g, 1);
    CHECK_EQ(g.submit(Command::chooseSuccessor(0, Succession::Heir)), CommandError::CannotSucceed);
    pass(g, 6);
    CHECK_EQ(g.state().players[0].interregnumTurns, rules().globalInt("LEADER_INTERREGNUM_TURNS"));  // frozen while held
    CHECK_EQ(g.submit(Command::abandonLeader(0)), CommandError::Ok);
    CHECK(g.state().players[0].captor == kNoPlayer && g.state().players[0].successionPending);
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
