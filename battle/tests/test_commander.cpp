#include <cstdio>
#include <memory>

#include "sovereign_battle/arena.h"
#include "test.h"

using namespace sov::battle;

namespace {

std::shared_ptr<const Policy> shipped() {
    auto p = std::make_shared<Policy>();
    std::string err;
    if (!p->load(SOVEREIGN_BATTLE_AI, &err)) {
        std::printf("  %s\n", err.c_str());
        return nullptr;
    }
    return p;
}

struct Record {
    float score = 0.f;  // mean, for the trained side
    int wins = 0, losses = 0, games = 0;
};

// The trained commander against a scripted one over random matchups, on both sides.
Record versus(const std::shared_ptr<const Policy>& policy, Order baseline, int games, uint64_t seed, int maxGap = 15) {
    Rng rng(seed);
    Record r;
    for (int g = 0; g < games; ++g) {
        const Spec s = randomScenario(rng, maxGap);
        const int side = g % 2;
        Commander me = Commander::trained(policy, 0.f, s.seed);
        Commander them = Commander::fixed(baseline);
        const Result res = side == 0 ? playOut(s, me, them) : playOut(s, them, me);
        r.score += score(s, res, side);
        r.wins += res.winner == side;
        r.losses += res.winner == 1 - side;
        ++r.games;
    }
    r.score /= static_cast<float>(games);
    return r;
}

}  // namespace

TEST(policy_round_trips_through_its_file) {
    Rng rng(5);
    Policy p;
    p.initRandom(rng);
    REQUIRE(p.valid());
    const std::string path = "battle_policy_roundtrip.txt";
    REQUIRE(p.save(path));
    Policy q;
    std::string err;
    REQUIRE(q.load(path, &err));
    std::remove(path.c_str());
    float obs[Sim::kObservation];
    for (int i = 0; i < Sim::kObservation; ++i) obs[i] = rng.range(-1.f, 1.f);
    float a[kSquads * kOrders], b[kSquads * kOrders];
    p.forward(obs, a);
    q.forward(obs, b);
    for (int i = 0; i < kSquads * kOrders; ++i) CHECK(std::fabs(a[i] - b[i]) < 1e-4f);
    Policy bad;
    CHECK(!bad.load("no_such_commander.txt", &err));
}

TEST(a_commander_without_weights_falls_back_to_the_baseline) {
    Commander c = Commander::trained(nullptr);
    CHECK(!c.isTrained());
    Sim sim;
    Spec s;
    sim.start(s);
    for (Order o : c.decide(sim, 0)) CHECK(o == Order::Advance);
}

TEST(the_trained_commander_ships) {
    const auto p = shipped();
    REQUIRE(p != nullptr);
    CHECK(p->valid());
}

TEST(the_trained_commander_is_harder_to_exploit_than_any_script) {
    // Against every scripted style a player might use, the trained commander's worst result
    // beats the worst result of every scripted commander: no single plan beats it badly.
    const auto p = shipped();
    REQUIRE(p != nullptr);
    const Order styles[] = {Order::Advance, Order::Hold, Order::FlankLeft, Order::FlankRight, Order::HuntLeader};
    float trainedWorst = 1e9f;
    for (Order style : styles) {
        const Record r = versus(p, style, 120, 31 + static_cast<uint64_t>(style));
        std::printf("  trained vs %-16s score %+.3f, won %d lost %d\n", orderName(style), static_cast<double>(r.score), r.wins, r.losses);
        trainedWorst = std::min(trainedWorst, r.score);
    }
    for (Order script : styles) {
        float worst = 1e9f;
        for (Order style : styles) {
            Rng rng(31 + static_cast<uint64_t>(style));
            float total = 0.f;
            for (int g = 0; g < 120; ++g) {
                const Spec s = randomScenario(rng);
                const int side = g % 2;
                Commander me = Commander::fixed(script), them = Commander::fixed(style);
                total += score(s, side == 0 ? playOut(s, me, them) : playOut(s, them, me), side);
            }
            worst = std::min(worst, total / 120.f);
        }
        std::printf("  scripted %-16s worst %+.3f (trained worst %+.3f)\n", orderName(script), static_cast<double>(worst), static_cast<double>(trainedWorst));
        CHECK(trainedWorst > worst);
    }
}

TEST(the_trained_commander_punishes_a_passive_enemy) {
    const auto p = shipped();
    REQUIRE(p != nullptr);
    const Record r = versus(p, Order::Hold, 120, 12);
    std::printf("  vs hold: score %+.3f, won %d lost %d\n", static_cast<double>(r.score), r.wins, r.losses);
    CHECK(r.score > 0.1f);
    CHECK(r.wins > r.losses);
}

TEST(the_trained_commander_does_not_flip_clear_mismatches) {
    // Civ math stays the backbone: against a much stronger force the trained commander can
    // lose less, but it should rarely win.
    const auto p = shipped();
    REQUIRE(p != nullptr);
    Rng rng(21);
    int upsets = 0, games = 0;
    for (int g = 0; g < 60; ++g) {
        Spec s = randomScenario(rng, 0);
        const int side = g % 2;
        UnitSpec& mine = side == 0 ? s.attacker : s.defender;
        UnitSpec& theirs = side == 0 ? s.defender : s.attacker;
        if (mine.leaderIsUnit || theirs.leaderIsUnit) continue;
        mine.hp = theirs.hp = 100;
        theirs.strength = mine.strength + 14;
        s.leaderSide = -1;
        Commander me = Commander::trained(p, 0.f, s.seed);
        Commander them = Commander::fixed(Order::Advance);
        const Result res = side == 0 ? playOut(s, me, them) : playOut(s, them, me);
        upsets += res.winner == side;
        ++games;
    }
    std::printf("  upsets: %d of %d\n", upsets, games);
    CHECK(upsets * 5 <= games);
}
