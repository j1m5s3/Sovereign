#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sovereign_battle/arena.h"
#include "sovereign_battle/commander.h"
#include "sovereign_battle/show.h"
#include "test.h"

using namespace sov::battle;

namespace {

Spec duel(int attacker, int defender, uint32_t seed, bool loneDefender = false) {
    Spec s;
    s.attacker = {"Swordsman", 0, attacker, 100, false};
    s.defender = {"Swordsman", 1, defender, 100, loneDefender};
    s.leaderSide = loneDefender ? -1 : 0;
    s.humanSide = -1;
    s.leaderStrength = 30;
    s.seed = seed;
    return s;
}

// How a human moves the ruler by hand (Spec::humanSide): not at all, chasing the nearest foe and striking, giving ground
// from foes near him while striking, or wandering at random and striking now and then.
enum class Keys { None, Chase, GiveGround, Wander };

// A battle and the orders each side keeps all through it.
struct Fight {
    Spec spec;
    Order attack = Order::Advance, defend = Order::Advance;
    Keys keys = Keys::None;
};

Fight byHand(int strength, int side, uint32_t seed, Keys keys, Order attack, Order defend) {
    Fight f{duel(strength, 36, seed), attack, defend, keys};
    f.spec.leaderSide = side;
    f.spec.humanSide = side;
    f.spec.leaderStrength = 40 + static_cast<int>(seed % 20u);
    return f;
}

// Even and lopsided lines, a line that holds, one that falls back striking as it goes (men who strike walking away),
// flankers, and a lone leader whom many men strike at once (two attackers share a victim); rulers moved and struck by
// hand (who strike as they move, at up to 1.2 reach); then random matchups like the game's, enough that some man swings
// and is killed by a man later in the same step.
std::vector<Fight> fights() {
    std::vector<Fight> f;
    for (uint32_t seed = 1; seed <= 4; ++seed) f.push_back({duel(36, 36, seed), Order::Advance, Order::Advance});
    f.push_back({duel(50, 30, 5), Order::Advance, Order::Advance});
    f.push_back({duel(30, 50, 6), Order::Advance, Order::Advance});
    f.push_back({duel(36, 36, 7), Order::Advance, Order::Hold});
    f.push_back({duel(40, 28, 8), Order::Advance, Order::FallBack});
    f.push_back({duel(36, 36, 9), Order::FlankLeft, Order::Advance});
    f.push_back({duel(40, 16, 10, true), Order::Advance, Order::Advance});
    f.push_back({duel(45, 20, 11, true), Order::HuntLeader, Order::Advance});
    for (uint32_t k = 0; k < 6; ++k) {
        const Keys keys = static_cast<Keys>(1 + k % 3);
        f.push_back(byHand(36 + 4 * static_cast<int>(k % 3), static_cast<int>(k % 2), 20 + k, keys, k < 3 ? Order::Advance : Order::Hold,
                           k < 3 ? Order::Advance : Order::FallBack));
    }
    Rng rng(12);
    for (int k = 0; k < 40; ++k) f.push_back({randomScenario(rng), static_cast<Order>(rng.below(kOrders)), static_cast<Order>(rng.below(kOrders))});
    return f;
}

// The human's hand on the keys: a wandering ruler keeps his course until he picks another.
struct Hand {
    Rng rng;
    Vec2 course;
    explicit Hand(uint32_t seed) : rng(seed) {}
};

// A step of a fight: both commanders, then the sim, with the human's keys for the ruler when he is moved by hand.
void stepFight(Sim& sim, Commander& a, Commander& d, const Fight& f, float dt, Hand& hand) {
    a.update(sim, 0, dt);
    d.update(sim, 1, dt);
    const int l = sim.humanLeader();
    if (f.keys == Keys::None || l < 0) {
        sim.step(dt);
        return;
    }
    const std::vector<Soldier>& men = sim.soldiers();
    const Soldier& me = men[static_cast<size_t>(l)];
    Vec2 to;
    for (const Soldier& e : men) {
        if (e.alive && e.side != me.side && (to.lengthSq() <= 0.f || (e.pos - me.pos).lengthSq() < to.lengthSq())) to = e.pos - me.pos;
    }
    LeaderInput in;
    if (f.keys == Keys::Chase) {
        in.move = to.length() > 120.f ? to.normalized() : Vec2();
        in.strike = true;
    } else if (f.keys == Keys::GiveGround) {
        in.move = to.length() < 200.f ? (to * -1.f).normalized() : to.normalized();
        in.strike = true;
    } else {
        // A new course twice a second on average: toward the foe, any way, or standing.
        if (hand.rng.unit() < dt * 2.f) {
            const float turn = hand.rng.range(0.f, 6.2831853f);
            const float pick = hand.rng.unit();
            hand.course = pick < 0.4f ? to.normalized() : pick < 0.8f ? Vec2(std::cos(turn), std::sin(turn)) : Vec2();
        }
        in.move = hand.course;
        in.strike = hand.rng.unit() < 0.3f;
    }
    sim.step(dt, &in);
}

// The men as the battle's own machine sees them: every swing's cooldown carried.
std::vector<Seen> carried(const Sim& sim) {
    std::vector<Seen> out;
    for (const Soldier& s : sim.soldiers()) {
        Seen m;
        m.pos = s.pos;
        m.hp = s.hp;
        m.cooldown = s.cooldown;
        m.side = s.side;
        m.alive = s.alive;
        out.push_back(m);
    }
    return out;
}

// As a snapshot carries them (FSovBattleSnapshot::Encode): positions and hit points in hundredths, no cooldown.
float hundredths(float v) { return std::floor(v * 100.f + 0.5f) / 100.f; }
std::vector<Seen> snapshot(const Sim& sim) {
    std::vector<Seen> out = carried(sim);
    for (Seen& m : out) {
        m.pos = Vec2(hundredths(m.pos.x), hundredths(m.pos.y));
        m.hp = hundredths(m.hp);
        m.cooldown = -1.f;
    }
    return out;
}

// Counts the slips and keeps the first, so a broken rule reports once rather than for every frame.
struct Slips {
    int count = 0;
    std::string first;
    void expect(bool ok, const std::string& what) {
        if (ok) return;
        if (count++ == 0) first = what;
    }
};

std::string at(const char* what, int man, double t) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s (man %d at %.3f s)", what, man, t);
    return buf;
}

}  // namespace

TEST(show_follows_every_swing) {
    // Where the battle runs, the scene sees every swing: each is one strike at a foe in reach, nobody strikes twice
    // within a swing's cooldown, every hit and fall is the blow of a man who struck that same frame and waits exactly
    // for it to land, and each man who dies falls once, on the frame he dies, and is shown nothing after.
    const ShowTiming timing;
    const float dt = 1.f / 60.f;
    Slips slips;
    int strikes = 0, hits = 0, deaths = 0, sharedVictims = 0, swungAndDied = 0, olderStrikeAtVictim = 0, rulerStrikes = 0;
    for (const Fight& f : fights()) {
        Sim sim;
        sim.start(f.spec);
        Commander a = Commander::fixed(f.attack), d = Commander::fixed(f.defend);
        Hand keys(f.spec.seed);
        Show show;
        std::vector<Cue> cues;
        show.update(carried(sim), sim.elapsed(), cues);
        slips.expect(cues.empty(), "nothing to show before the fight");
        const size_t n = sim.soldiers().size();
        std::vector<double> lastStrike(n, -1e9), lastHitBy(n, -1e9);
        std::vector<int> lastStriker(n, -1), lastStrikeAt(n, -1), falls(n, 0);
        for (int step = 0; step < 20000 && !sim.finished(); ++step) {
            const std::vector<Soldier> before = sim.soldiers();
            stepFight(sim, a, d, f, dt, keys);
            const double t = sim.elapsed();
            cues.clear();
            show.update(carried(sim), t, cues);
            const std::vector<Soldier>& men = sim.soldiers();
            std::vector<int> struck(n, 0), hit(n, 0), fell(n, 0);
            for (const Cue& c : cues) {
                const size_t i = static_cast<size_t>(c.man);
                slips.expect(before[i].alive, at("nothing shown to a man dead before this frame", c.man, t));
                if (c.kind == CueKind::Strike) {
                    ++struck[i];
                    ++strikes;
                    slips.expect(men[i].cooldown > before[i].cooldown, at("a strike is a swing", c.man, t));
                    // Within engage at the step's end: a man in reach as he swings (a ruler moved by hand, 1.2 reach)
                    // may have moved on a little in the step.
                    slips.expect(c.other >= 0 && men[static_cast<size_t>(c.other)].side != men[i].side
                                     && dist(men[static_cast<size_t>(c.other)].pos, men[i].pos) <= timing.engage,
                                 at("a strike is at a foe in reach", c.man, t));
                    slips.expect(t - lastStrike[i] >= Sim::kSwingSeconds - dt - 1e-4, at("no man strikes twice within his cooldown", c.man, t));
                    lastStrike[i] = t;
                    lastStrikeAt[i] = c.other;
                    rulerStrikes += static_cast<int>(i) == sim.humanLeader();
                    swungAndDied += !men[i].alive;
                } else if (c.kind == CueKind::Hit || c.kind == CueKind::Fall) {
                    const bool ok = c.other >= 0 && lastStrike[static_cast<size_t>(c.other)] == t;
                    slips.expect(ok, at("a blow is struck that same frame", c.man, t));
                    slips.expect(c.delay == timing.impact, at("and waits for it to land", c.man, t));
                    if (ok && lastHitBy[i] > t - timing.strike && lastStriker[i] != c.other) ++sharedVictims;
                    lastHitBy[i] = t;
                    lastStriker[i] = c.other;
                    if (c.kind == CueKind::Hit) {
                        ++hit[i];
                        ++hits;
                        slips.expect(men[i].alive && men[i].hp < before[i].hp, at("a hit is a man hurt", c.man, t));
                    } else {
                        ++fell[i];
                        ++falls[i];
                        ++deaths;
                        slips.expect(!men[i].alive, at("a fall is a man killed", c.man, t));
                    }
                } else {
                    slips.expect(false, at("no man is seen dead first in a battle shown from its start", c.man, t));
                }
            }
            for (size_t i = 0; i < n; ++i) {
                if (!before[i].alive) continue;
                const bool swung = men[i].cooldown > before[i].cooldown;
                slips.expect(struck[i] == (swung ? 1 : 0), at("each swing strikes once", static_cast<int>(i), t));
                slips.expect(fell[i] == (men[i].alive ? 0 : 1), at("each death falls once, on its frame", static_cast<int>(i), t));
                slips.expect(hit[i] == (men[i].alive && men[i].hp < before[i].hp ? 1 : 0), at("each wound flinches once", static_cast<int>(i), t));
            }
            // A victim of this frame's blow with an older strike at him still on its way: the blow is still this frame's.
            for (const Cue& c : cues) {
                if (c.kind == CueKind::Strike) continue;
                for (size_t e = 0; e < n; ++e) {
                    olderStrikeAtVictim += e != static_cast<size_t>(c.other) && lastStrike[e] < t && lastStrike[e] > t - timing.impact
                        && lastStrikeAt[e] == c.man;
                }
            }
        }
        for (size_t i = 0; i < n; ++i) {
            slips.expect(falls[i] == (sim.soldiers()[i].alive ? 0 : 1), at("every man killed fell once", static_cast<int>(i), sim.elapsed()));
        }
    }
    std::printf("  exact: %d strikes (%d by a ruler moved by hand), %d hits, %d falls; %d blows on a man another hit within a "
                "second, %d with an older strike at him landing, %d men struck and died in one step\n",
                strikes, rulerStrikes, hits, deaths, sharedVictims, olderStrikeAtVictim, swungAndDied);
    if (slips.count) std::printf("  %d slips, the first: %s\n", slips.count, slips.first.c_str());
    CHECK_EQ(slips.count, 0);
    CHECK(strikes > 0 && hits > 0 && deaths > 0);
    CHECK(sharedVictims > 0);        // two attackers share a victim
    CHECK(olderStrikeAtVictim > 0);  // and the older strike does not take this frame's blow
    CHECK(swungAndDied > 0);         // a man swings and is killed in the same step
    CHECK(rulerStrikes > 0);         // the ruler's own strikes
}

TEST(snapshots_tell_the_same_story) {
    // A replay or the other player's view: snapshots every 0.1 s, rounded as Encode does and with no cooldown, shown
    // at uneven frame times (alternating 1/30 and 1/90 s, the frame floor(t x 10) as the replay plays it).
    const ShowTiming timing;
    Slips slips;
    int strikes = 0, credited = 0, uncredited = 0, deaths = 0, engagedSpells = 0;
    for (const Fight& f : fights()) {
        Sim sim;
        sim.start(f.spec);
        Commander a = Commander::fixed(f.attack), d = Commander::fixed(f.defend);
        Hand keys(f.spec.seed);
        std::vector<std::vector<Seen>> frames{snapshot(sim)};
        const float dt = 1.f / 60.f;
        for (int step = 1; step <= 20000 && !sim.finished(); ++step) {
            stepFight(sim, a, d, f, dt, keys);
            if (step % 6 == 0 || sim.finished()) frames.push_back(snapshot(sim));
        }
        const size_t n = frames.front().size();
        // The snapshot that first shows each man dead.
        std::vector<int> diedIn(n, -1);
        for (size_t k = 0; k < frames.size(); ++k) {
            for (size_t i = 0; i < n; ++i) {
                if (diedIn[i] < 0 && !frames[k][i].alive) diedIn[i] = static_cast<int>(k);
            }
        }
        Show show;
        std::vector<Cue> cues;
        std::vector<double> lastStrike(n, -1e9), engagedSince(n, -1.0);
        std::vector<int> falls(n, 0), strikesSince(n, 0);
        int shownBefore = -1;
        double t = 0.0;
        for (int tick = 0; static_cast<int>(t * 10.0) < static_cast<int>(frames.size()) + 20; ++tick) {
            const int k = std::min(static_cast<int>(t * 10.0), static_cast<int>(frames.size()) - 1);
            const std::vector<Seen>& men = frames[static_cast<size_t>(k)];
            cues.clear();
            show.update(men, t, cues);
            for (const Cue& c : cues) {
                const size_t i = static_cast<size_t>(c.man);
                if (c.kind == CueKind::Strike) {
                    ++strikes;
                    ++strikesSince[i];
                    slips.expect(t - lastStrike[i] >= timing.strike - 1e-9, at("no man strikes twice within a second", c.man, t));
                    slips.expect(c.other >= 0 && men[static_cast<size_t>(c.other)].side != men[i].side, at("a strike is at a foe", c.man, t));
                    lastStrike[i] = t;
                    continue;
                }
                slips.expect(c.kind != CueKind::Lie, at("no man is seen dead first", c.man, t));
                if (c.kind == CueKind::Fall) {
                    ++falls[i];
                    ++deaths;
                    // The first update that shows the snapshot he is dead in, waiting no longer than a blow.
                    slips.expect(k == diedIn[i] && shownBefore < diedIn[i], at("a fall comes with the snapshot that shows it", c.man, t));
                    slips.expect(c.delay >= 0.f && c.delay <= timing.impact, at("a fall waits at most for a blow", c.man, t));
                }
                uncredited += c.other < 0;
                if (c.other >= 0) {
                    ++credited;
                    const size_t e = static_cast<size_t>(c.other);
                    slips.expect(men[e].side != men[i].side && dist(men[e].pos, men[i].pos) <= timing.engage,
                                 at("the blow is a foe's in reach", c.man, t));
                    slips.expect(t - lastStrike[e] <= timing.strike, at("whose strike still plays", c.man, t));
                    const double want = std::max(0.0, lastStrike[e] + timing.impact - t);
                    slips.expect(std::fabs(c.delay - want) < 1e-5, at("and lands at its impact", c.man, t));
                }
            }
            // A man engaged without a break for 3.2 s (two cadences, and a frame's slack) strikes at least twice.
            for (size_t i = 0; i < n; ++i) {
                if (men[i].alive && show.target(static_cast<int>(i)) >= 0) {
                    if (engagedSince[i] < 0.0) {
                        engagedSince[i] = t;
                        strikesSince[i] = 0;
                    }
                    if (t - engagedSince[i] >= 2.0 * timing.cadence + 0.05) {
                        ++engagedSpells;
                        slips.expect(strikesSince[i] >= 2, at("a man engaged for 3.2 s strikes at least twice", static_cast<int>(i), t));
                        engagedSince[i] = t;
                        strikesSince[i] = 0;
                    }
                } else {
                    engagedSince[i] = -1.0;
                }
            }
            shownBefore = std::max(shownBefore, k);
            t += tick % 2 == 0 ? 1.0 / 30.0 : 1.0 / 90.0;
        }
        for (size_t i = 0; i < n; ++i) slips.expect(falls[i] == (diedIn[i] >= 0 ? 1 : 0), at("every man killed falls once", static_cast<int>(i), t));
    }
    std::printf("  snapshots: %d strikes, %d blows credited and %d not, %d falls, %d spells of 3.2 s engaged\n", strikes, credited,
                uncredited, deaths, engagedSpells);
    if (slips.count) std::printf("  %d slips, the first: %s\n", slips.count, slips.first.c_str());
    CHECK_EQ(slips.count, 0);
    CHECK(strikes > 0 && credited > 0 && deaths > 0 && engagedSpells > 0);
    // A man hurt nearly always has a foe in reach, even a tenth of a second late; a blow from a man who has since moved
    // beyond engage (a ruler moved by hand, say) goes uncredited, landing at once. At most 1 in 500 (seen: 1 in 2,200).
    CHECK(uncredited * 500 <= credited + uncredited);
}

TEST(foes_who_kill_each_other_both_fall) {
    // Two foes in reach, then both dead in the next frame (each killed the other between the two): each falls once, by
    // the other's blow, struck then (neither had a strike on its way) and landing at its impact. From snapshots and where
    // the battle runs (both swung).
    const ShowTiming timing;
    for (const bool carries : {false, true}) {
        Show show;
        std::vector<Seen> men(2);
        men[1].pos = Vec2(120.f, 0.f);
        men[1].side = 1;
        for (Seen& m : men) {
            m.hp = 0.3f;
            m.cooldown = carries ? 0.5f : -1.f;
        }
        std::vector<Cue> cues;
        show.update(men, 1.0, cues);
        REQUIRE(cues.empty());
        for (Seen& m : men) {
            m.hp = -0.2f;
            m.alive = false;
            m.cooldown = carries ? Sim::kSwingSeconds : -1.f;
        }
        show.update(men, 1.1, cues);
        int falls[2] = {0, 0}, strikes[2] = {0, 0};
        for (const Cue& c : cues) {
            const int other = 1 - c.man;
            if (c.kind == CueKind::Strike) {
                strikes[c.man] += c.other == other;
            } else {
                CHECK(c.kind == CueKind::Fall && c.other == other && c.delay == timing.impact);
                ++falls[c.man];
            }
        }
        CHECK(falls[0] == 1 && falls[1] == 1 && strikes[0] == 1 && strikes[1] == 1);
        CHECK_EQ(static_cast<int>(cues.size()), 4);
        cues.clear();
        show.update(men, 1.2, cues);
        CHECK(cues.empty());
    }
}

TEST(a_blow_goes_to_the_strike_on_its_way) {
    // From snapshots: A (0) starts a strike at B (1) on his cadence, and B's hit points fall before its blow lands while
    // C (2), free and fighting B too, is in reach. The blow is A's, landing at his strike's impact, and C does not strike
    // for it.
    const ShowTiming timing;
    Show show;
    std::vector<Seen> men(3);
    men[0].pos = Vec2(-120.f, 0.f);
    men[1].side = 1;
    men[2].pos = Vec2(0.f, 1000.f);
    for (Seen& m : men) m.hp = 5.f;
    std::vector<Cue> cues;
    double t = 1.0, struck = -1.0;
    for (; t < 4.0 && struck < 0.0; t += 0.05) {
        cues.clear();
        show.update(men, t, cues);
        for (const Cue& c : cues) {
            if (c.man == 0 && c.kind == CueKind::Strike && c.other == 1) struck = t;
        }
    }
    REQUIRE(struck >= 0.0);
    men[2].pos = Vec2(0.f, 150.f);
    men[1].hp = 4.f;
    const double now = struck + 0.2;
    cues.clear();
    show.update(men, now, cues);
    int hits = 0, byC = 0;
    for (const Cue& c : cues) {
        if (c.man == 1 && c.kind == CueKind::Hit) {
            ++hits;
            CHECK_EQ(c.other, 0);
            CHECK(std::fabs(c.delay - (struck + timing.impact - now)) < 1e-5);
        }
        byC += c.man == 2;
    }
    CHECK_EQ(hits, 1);
    CHECK_EQ(byC, 0);
    CHECK_EQ(show.target(2), 1);  // C fights B
}

TEST(show_leaves_the_fight_alone) {
    // Two battles from one seed and one spec, one watched every step: the same to the bit.
    for (const Fight& f : {Fight{duel(36, 36, 3), Order::Advance, Order::FallBack}, Fight{duel(40, 16, 4, true)}}) {
        Sim plain, watched;
        plain.start(f.spec);
        watched.start(f.spec);
        Commander pa = Commander::fixed(f.attack), pd = Commander::fixed(f.defend);
        Commander wa = Commander::fixed(f.attack), wd = Commander::fixed(f.defend);
        Show show;
        std::vector<Cue> cues;
        int targets = 0;
        for (int step = 0; step < 20000 && !(plain.finished() && watched.finished()); ++step) {
            pa.update(plain, 0, 1.f / 60.f);
            pd.update(plain, 1, 1.f / 60.f);
            plain.step(1.f / 60.f);
            wa.update(watched, 0, 1.f / 60.f);
            wd.update(watched, 1, 1.f / 60.f);
            watched.step(1.f / 60.f);
            show.update(carried(watched), watched.elapsed(), cues);
            for (size_t i = 0; i < watched.soldiers().size(); ++i) targets += show.target(static_cast<int>(i)) >= 0;
        }
        CHECK(cues.size() > 0 && targets > 0);
        REQUIRE(plain.soldiers().size() == watched.soldiers().size());
        bool same = plain.elapsed() == watched.elapsed();
        for (size_t i = 0; i < plain.soldiers().size(); ++i) {
            const Soldier& p = plain.soldiers()[i];
            const Soldier& w = watched.soldiers()[i];
            const float pf[] = {p.pos.x, p.pos.y, p.facing.x, p.facing.y, p.hp, p.cooldown};
            const float wf[] = {w.pos.x, w.pos.y, w.facing.x, w.facing.y, w.hp, w.cooldown};
            same = same && std::memcmp(pf, wf, sizeof pf) == 0 && p.alive == w.alive;
        }
        CHECK(same);
        const Result rp = plain.result(), rw = watched.result();
        CHECK(rp.toAttacker == rw.toAttacker && rp.toDefender == rw.toDefender && rp.leaderWound == rw.leaderWound && rp.winner == rw.winner
              && rp.timedOut == rw.timedOut);
    }
}

TEST(a_man_first_seen_dead_lies) {
    // An online view that starts late: a man dead the first time he is seen lies already, with no fall, then or after.
    Show show;
    std::vector<Seen> men(2);
    men[0].pos = Vec2(0.f, 0.f);
    men[0].hp = 5.f;
    men[1].pos = Vec2(100.f, 0.f);
    men[1].side = 1;
    men[1].hp = -0.5f;
    men[1].alive = false;
    std::vector<Cue> cues;
    show.update(men, 1.0, cues);
    REQUIRE(cues.size() == 1);
    CHECK(cues[0].man == 1 && cues[0].kind == CueKind::Lie && cues[0].delay == 0.f);
    cues.clear();
    show.update(men, 1.1, cues);
    CHECK(cues.empty());
    CHECK_EQ(show.target(0), -1);  // the dead are fought by nobody
}

TEST(a_view_that_starts_mid_fight) {
    // The other player's view, started mid-fight: at first sight the men already dead lie, with no fall, and nothing
    // else is shown (the wounded are not hit again, nobody strikes at once); after that each man killed falls once.
    const Fight f{duel(36, 36, 2), Order::Advance, Order::Advance};
    Sim sim;
    sim.start(f.spec);
    Commander a = Commander::fixed(f.attack), d = Commander::fixed(f.defend);
    Hand keys(f.spec.seed);
    const float dt = 1.f / 60.f;
    const std::vector<Seen> start = snapshot(sim);
    auto deadNow = [&sim]() {
        int k = 0;
        for (const Soldier& s : sim.soldiers()) k += !s.alive;
        return k;
    };
    for (int step = 0; step < 20000 && !sim.finished() && deadNow() < 6; ++step) stepFight(sim, a, d, f, dt, keys);
    REQUIRE(!sim.finished());
    const std::vector<Seen> first = snapshot(sim);
    const size_t n = first.size();
    int dead = 0, wounded = 0;
    for (size_t i = 0; i < n; ++i) {
        dead += !first[i].alive;
        wounded += first[i].alive && first[i].hp < start[i].hp;
    }
    CHECK(dead > 0 && wounded > 0);
    Show show;
    std::vector<Cue> cues;
    double t = 30.0;
    show.update(first, t, cues);
    int lies = 0, other = 0;
    for (const Cue& c : cues) {
        if (c.kind == CueKind::Lie && !first[static_cast<size_t>(c.man)].alive) ++lies;
        else ++other;
    }
    CHECK_EQ(lies, dead);
    CHECK_EQ(other, 0);
    std::vector<int> falls(n, 0), lieAgain(n, 0);
    for (int step = 1; step <= 20000 && !sim.finished(); ++step) {
        stepFight(sim, a, d, f, dt, keys);
        if (step % 6 != 0 && !sim.finished()) continue;
        t += 0.1;
        cues.clear();
        show.update(snapshot(sim), t, cues);
        for (const Cue& c : cues) {
            falls[static_cast<size_t>(c.man)] += c.kind == CueKind::Fall;
            lieAgain[static_cast<size_t>(c.man)] += c.kind == CueKind::Lie;
        }
    }
    int fellLater = 0;
    bool once = true;
    for (size_t i = 0; i < n; ++i) {
        const int want = first[i].alive && !sim.soldiers()[i].alive ? 1 : 0;
        fellLater += want;
        once = once && falls[i] == want && lieAgain[i] == 0;
    }
    CHECK(fellLater > 0);
    CHECK(once);
}

TEST(show_forgets_and_starts_over) {
    // A scene shown an earlier frame: a man dead and then alive again is forgotten, with nothing shown, and is hit
    // afresh. A smaller battle after a bigger one starts over: a man dead in it lies, not falls.
    Show show;
    std::vector<Seen> men(2);
    men[0].pos = Vec2(0.f, 0.f);
    men[0].hp = 5.f;
    men[1].pos = Vec2(100.f, 0.f);
    men[1].side = 1;
    men[1].hp = 5.f;
    std::vector<Cue> cues;
    auto shown = [&cues](int man, CueKind kind) {
        int k = 0;
        for (const Cue& c : cues) k += c.man == man && c.kind == kind;
        return k;
    };
    show.update(men, 1.0, cues);
    CHECK(cues.empty());
    men[1].hp = -0.5f;
    men[1].alive = false;
    cues.clear();
    show.update(men, 1.1, cues);
    CHECK_EQ(shown(1, CueKind::Fall), 1);
    men[1].hp = 5.f;
    men[1].alive = true;
    cues.clear();
    show.update(men, 1.2, cues);
    CHECK(cues.empty());
    men[1].hp = 4.f;
    cues.clear();
    show.update(men, 1.3, cues);
    CHECK_EQ(shown(1, CueKind::Hit), 1);
    CHECK_EQ(shown(1, CueKind::Fall) + shown(1, CueKind::Lie), 0);
    std::vector<Seen> next(1);
    next[0].hp = -1.f;
    next[0].alive = false;
    cues.clear();
    show.update(next, 2.0, cues);
    REQUIRE(cues.size() == 1);
    CHECK(cues[0].man == 0 && cues[0].kind == CueKind::Lie);
}
