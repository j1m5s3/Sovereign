#include "sovereign_battle/arena.h"
#include "test.h"

using namespace sov::battle;

namespace {

Spec duel(int attacker, int defender, uint32_t seed, bool loneDefender = false) {
    Spec s;
    s.attacker = {"Swordsman", 0, attacker, 100, false};
    s.defender = {"Swordsman", 1, defender, 100, loneDefender};
    s.leaderSide = 0;
    s.humanSide = -1;
    s.leaderStrength = 30;
    s.seed = seed;
    return s;
}

Result scripted(const Spec& s, Order a = Order::Advance, Order d = Order::Advance) {
    Commander ca = Commander::fixed(a), cd = Commander::fixed(d);
    return playOut(s, ca, cd);
}

// Average score for the attacker when its squads take these orders against a holding line.
float attackScore(Order left, Order centre, Order right, int seeds) {
    float total = 0.f;
    for (int seed = 1; seed <= seeds; ++seed) {
        Spec s = duel(36, 36, static_cast<uint32_t>(seed));
        s.leaderSide = -1;
        Sim sim;
        sim.start(s);
        sim.setOrder(0, 0, left);
        sim.setOrder(0, 1, centre);
        sim.setOrder(0, 2, right);
        sim.setAllOrders(1, Order::Hold);
        for (int i = 0; i < 2000 && !sim.finished(); ++i) sim.step(0.1f);
        total += score(s, sim.result(), 0);
    }
    return total / static_cast<float>(seeds);
}

}  // namespace

TEST(soldiers_follow_hp_in_three_squads) {
    Spec s = duel(30, 30, 1);
    s.defender.hp = 70;
    Sim sim;
    sim.start(s);
    CHECK_EQ(sim.started(0), 10);
    CHECK_EQ(sim.started(1), 7);
    CHECK_EQ(sim.squadAlive(1, 1), 3);  // the odd soldier goes to the centre
    CHECK_EQ(sim.squadAlive(1, 0), 2);
    CHECK(sim.leaderOf(0) >= 0);
    CHECK_EQ(sim.leaderOf(1), -1);
    float obs[Sim::kObservation];
    sim.observe(1, obs);
    CHECK_EQ(obs[Sim::kObservation - 1], 1.f);
}

TEST(numbers_decide_most_fights) {
    const Result even = scripted(duel(35, 35, 1));
    CHECK(even.toAttacker > 0 && even.toDefender > 0);
    int strongWins = 0, strongLessHurt = 0;
    for (uint32_t seed = 1; seed <= 12; ++seed) {
        const Result r = scripted(duel(50, 30, seed));
        strongWins += r.winner == 0;
        strongLessHurt += r.toAttacker < r.toDefender;
    }
    CHECK(strongWins >= 10);
    CHECK(strongLessHurt >= 10);
}

TEST(same_seed_same_battle) {
    const Result a = scripted(duel(40, 38, 7)), b = scripted(duel(40, 38, 7));
    CHECK_EQ(a.toAttacker, b.toAttacker);
    CHECK_EQ(a.toDefender, b.toDefender);
    CHECK_EQ(a.leaderWound, b.leaderWound);
}

TEST(a_lone_leader_fights_alone) {
    const Result r = scripted(duel(40, 16, 3, true));
    CHECK(r.toDefender > 0);
    CHECK(r.toDefender <= 100 && r.toAttacker <= 100);
}

TEST(flanking_beats_a_frontal_attack) {
    // Against a braced line, pinning the centre and sending the wings round the sides does
    // better than charging straight in.
    const float frontal = attackScore(Order::Advance, Order::Advance, Order::Advance, 16);
    const float flank = attackScore(Order::FlankLeft, Order::Advance, Order::FlankRight, 16);
    CHECK(flank > frontal + 0.05f);
}

TEST(falling_back_saves_men) {
    // A weaker force that gives ground loses less than one that stands and dies.
    float stand = 0.f, retreat = 0.f;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        const Spec s = duel(40, 28, seed);
        stand += static_cast<float>(scripted(s, Order::Advance, Order::Hold).toDefender);
        retreat += static_cast<float>(scripted(s, Order::Advance, Order::FallBack).toDefender);
    }
    CHECK(retreat < stand);
}

TEST(the_human_leader_moves_by_hand) {
    Spec s = duel(30, 30, 2);
    s.humanSide = 0;
    Sim sim;
    sim.start(s);
    const int l = sim.humanLeader();
    REQUIRE(l >= 0);
    const Vec2 before = sim.soldiers()[static_cast<size_t>(l)].pos;
    LeaderInput in;
    in.move = Vec2(0.f, 1.f);
    for (int i = 0; i < 10; ++i) sim.step(0.1f, &in);
    CHECK(sim.soldiers()[static_cast<size_t>(l)].pos.y > before.y + 200.f);
}

TEST(sim_measures_each_sides_habits) {
    Sim sim;
    sim.start(duel(30, 30, 3));
    sim.setOrder(0, 0, Order::FlankLeft);
    sim.setOrder(0, 1, Order::Advance);
    sim.setOrder(0, 2, Order::FlankRight);
    sim.setAllOrders(1, Order::FallBack);
    for (int i = 0; i < 100 && !sim.finished(); ++i) sim.step(0.1f);
    const Habits a = sim.habits(0), d = sim.habits(1);
    CHECK(a.flank >= 600);  // two of three squads flanking
    CHECK_EQ(a.fallBack, 0);
    CHECK(d.fallBack >= 990);
    CHECK_EQ(d.flank, 0);
    CHECK(a.leaderFront >= 0 && a.leaderFront <= 1000);
}
