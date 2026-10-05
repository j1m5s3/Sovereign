#include "sovereign_battle/sim.h"

#include <algorithm>
#include <limits>

namespace sov::battle {

float Rng::normal() {
    const float u1 = std::max(unit(), 1e-7f);
    const float u2 = unit();
    return std::sqrt(-2.f * std::log(u1)) * std::cos(6.2831853f * u2);
}

const char* orderName(Order o) {
    switch (o) {
        case Order::Advance: return "advance";
        case Order::Hold: return "hold";
        case Order::FlankLeft: return "flank left";
        case Order::FlankRight: return "flank right";
        case Order::FallBack: return "fall back";
        case Order::HuntLeader: return "hunt the leader";
    }
    return "?";
}

namespace {

const UnitSpec& unitOf(const Spec& s, int side) { return side == 0 ? s.attacker : s.defender; }

Vec2 startForward(int side) { return side == 0 ? Vec2(1.f, 0.f) : Vec2(-1.f, 0.f); }

Vec2 clampField(Vec2 p) {
    return {std::clamp(p.x, -Sim::kFieldX, Sim::kFieldX), std::clamp(p.y, -Sim::kFieldY, Sim::kFieldY)};
}

}  // namespace

void Sim::start(const Spec& spec) {
    spec_ = spec;
    men_.clear();
    rng_.reseed(spec.seed);
    elapsed_ = 0.f;
    finished_ = timedOut_ = false;
    for (int side = 0; side < 2; ++side) {
        leader_[side] = -1;
        leaderIsUnit_[side] = false;
        orders_[side].fill(Order::Advance);
        flanked_[side].fill(false);
        squadInitial_[side].fill(0);
    }
    for (int side = 0; side < 2; ++side) {
        const UnitSpec& u = unitOf(spec_, side);
        const Vec2 f = startForward(side), l = f.left();
        const Vec2 base = f * -1600.f;
        if (u.leaderIsUnit) {
            Soldier s;
            s.side = side;
            s.squad = 1;
            s.leader = true;
            s.hp = s.maxHp = static_cast<float>(u.hp) / 3.f;
            s.pos = base;
            s.facing = f;
            leader_[side] = static_cast<int>(men_.size());
            leaderIsUnit_[side] = true;
            men_.push_back(s);
            continue;
        }
        // Soldiers follow HP: one per 10 HP, at least one, shared out centre first.
        const int n = std::max(1, u.hp / 10);
        int sizes[kSquads] = {n / 3, n / 3, n / 3};
        const int fill[kSquads] = {1, 0, 2};
        for (int r = 0; r < n % 3; ++r) ++sizes[fill[r]];
        for (int q = 0; q < kSquads; ++q) {
            const float lateral = q == 0 ? 650.f : q == 2 ? -650.f : 0.f;
            for (int i = 0; i < sizes[q]; ++i) {
                Soldier s;
                s.side = side;
                s.squad = q;
                s.facing = f;
                const float across = (i % 2 == 0 ? 1.f : -1.f) * 100.f;
                s.pos = base + l * (lateral + across + rng_.range(-15.f, 15.f)) - f * (160.f * static_cast<float>(i / 2));
                men_.push_back(s);
                ++squadInitial_[side][q];
            }
        }
    }
    const int ls = spec_.leaderSide;
    if (ls >= 0 && ls < 2 && !leaderIsUnit_[ls]) {
        // The escorted leader rides behind its men: a hero worth three soldiers.
        Soldier s;
        s.side = ls;
        s.leader = true;
        s.hp = s.maxHp = static_cast<float>(spec_.leaderHp) / 3.f;
        s.pos = startForward(ls) * -2300.f;
        s.facing = startForward(ls);
        leader_[ls] = static_cast<int>(men_.size());
        men_.push_back(s);
    }
    for (Soldier& s : men_) s.anchor = s.pos;
    for (int side = 0; side < 2; ++side) {
        for (int q = 0; q < kSquads; ++q) {
            lastCentroid_[side][q] = centroid(side, q);
            velocity_[side][q] = Vec2();
        }
    }
    for (int side = 0; side < 2; ++side) initial_[side] = alive(side);
}

void Sim::setOrder(int side, int squad, Order o) {
    if (orders_[side][squad] == o) return;
    flanked_[side][squad] = false;
    orders_[side][squad] = o;
    for (Soldier& s : men_) {
        if (s.side == side && s.squad == squad) s.anchor = s.pos;  // a new hold stands where it is
    }
}

void Sim::setAllOrders(int side, Order o) {
    for (int q = 0; q < kSquads; ++q) setOrder(side, q, o);
}

int Sim::alive(int side) const {
    int n = 0;
    for (const Soldier& s : men_) n += s.alive && s.side == side && !s.leader;
    return n;
}

int Sim::squadAlive(int side, int squad) const {
    int n = 0;
    for (const Soldier& s : men_) n += s.alive && s.side == side && s.squad == squad;
    return n;
}

Vec2 Sim::centroid(int side, int squad) const {
    Vec2 sum;
    int n = 0;
    for (const Soldier& s : men_) {
        if (!s.alive || s.side != side) continue;
        if (squad >= 0 ? s.squad != squad : s.squad < 0) continue;
        sum += s.pos;
        ++n;
    }
    if (n == 0 && squad < 0 && leader_[side] >= 0 && men_[static_cast<size_t>(leader_[side])].alive) return men_[static_cast<size_t>(leader_[side])].pos;
    return n ? sum * (1.f / static_cast<float>(n)) : startForward(side) * -1600.f;
}

Vec2 Sim::forward(int side) const {
    const Vec2 d = (centroid(1 - side) - centroid(side)).normalized();
    return d.lengthSq() > 0.f ? d : startForward(side);
}

int Sim::nearestEnemy(int from) const {
    int best = -1;
    float bestDist = std::numeric_limits<float>::max();
    const Soldier& me = men_[static_cast<size_t>(from)];
    for (size_t i = 0; i < men_.size(); ++i) {
        const Soldier& o = men_[i];
        if (!o.alive || o.side == me.side) continue;
        const float d = (o.pos - me.pos).lengthSq();
        if (d < bestDist) {
            bestDist = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Sim::strengthOf(int i) const {
    const Soldier& s = men_[static_cast<size_t>(i)];
    if (s.leader && !leaderIsUnit_[s.side]) return spec_.leaderStrength;
    return unitOf(spec_, s.side).strength;
}

int Sim::bonusAgainst(int attacker, int defender) const {
    const Soldier& a = men_[static_cast<size_t>(attacker)];
    const Soldier& d = men_[static_cast<size_t>(defender)];
    int b = 0;
    const float facing = dot(d.facing, (a.pos - d.pos).normalized());
    if (facing < -0.5f) {
        b += kRearBonus;
    } else if (facing < 0.3f) {
        b += kFlankBonus;
    } else if (d.squad >= 0 && orders_[d.side][d.squad] == Order::Hold && dist(d.pos, d.anchor) < 200.f) {
        b -= kBracedBonus;  // a braced line, met head on
    }
    auto inAura = [&](int i) {
        const Soldier& s = men_[static_cast<size_t>(i)];
        const int l = leader_[s.side];
        return l >= 0 && l != i && men_[static_cast<size_t>(l)].alive && dist(men_[static_cast<size_t>(l)].pos, s.pos) <= kAuraRange;
    };
    if (inAura(attacker)) b += kAuraBonus;
    if (inAura(defender)) b -= kAuraBonus;
    return b;
}

void Sim::swing(int from, int to) {
    Soldier& a = men_[static_cast<size_t>(from)];
    Soldier& d = men_[static_cast<size_t>(to)];
    a.cooldown = kSwingSeconds;
    // The stronger side lands more and harder blows (05: damage scales with e^(0.04 x diff)).
    const float diff = static_cast<float>(strengthOf(from) - strengthOf(to) + bonusAgainst(from, to));
    const float scale = std::exp(0.04f * diff);
    const float hitChance = std::clamp(0.55f * scale, 0.1f, 0.95f);
    if (rng_.unit() < hitChance) {
        d.hp -= 1.8f * scale * rng_.range(0.8f, 1.2f);
        if (d.hp <= 0.f) d.alive = false;
    }
}

void Sim::moveToward(Soldier& s, Vec2 target, float speed, float dt) {
    const Vec2 to = target - s.pos;
    const float len = to.length();
    if (len < 1.f) return;
    const float stepLen = std::min(len, speed * dt);
    s.facing = to * (1.f / len);
    s.pos = clampField(s.pos + s.facing * stepLen);
}

void Sim::step(float dt, const LeaderInput* human) {
    if (finished_) return;
    elapsed_ += dt;
    // Flanking squads switch to the attack once they reach the enemy's side.
    // Their waypoint is beside and a little behind the enemy, clear of its widest man.
    Vec2 waypoint[2][kSquads];
    for (int side = 0; side < 2; ++side) {
        const Vec2 f = forward(side), c = centroid(1 - side), l = f.left();
        float reachLeft = 0.f, reachRight = 0.f;
        for (const Soldier& e : men_) {
            if (!e.alive || e.side == side) continue;
            reachLeft = std::max(reachLeft, dot(e.pos - c, l));
            reachRight = std::max(reachRight, -dot(e.pos - c, l));
        }
        for (int q = 0; q < kSquads; ++q) {
            const Order o = orders_[side][q];
            if (o != Order::FlankLeft && o != Order::FlankRight) continue;
            const float out = (o == Order::FlankLeft ? reachLeft : reachRight) + 600.f;
            waypoint[side][q] = clampField(c + l * (o == Order::FlankLeft ? out : -out) + f * 250.f);
            if (!flanked_[side][q] && squadAlive(side, q) && dist(centroid(side, q), waypoint[side][q]) < 350.f) flanked_[side][q] = true;
        }
    }
    const int humanIdx = human ? humanLeader() : -1;
    for (size_t idx = 0; idx < men_.size(); ++idx) {
        const int i = static_cast<int>(idx);
        Soldier& s = men_[idx];
        if (!s.alive) continue;
        s.cooldown = std::max(0.f, s.cooldown - dt);
        const int t = nearestEnemy(i);
        if (t < 0) continue;
        const Vec2 tp = men_[static_cast<size_t>(t)].pos;
        const float d = dist(s.pos, tp);
        auto fight = [&](int target) {
            s.facing = (men_[static_cast<size_t>(target)].pos - s.pos).normalized();
            if (s.cooldown <= 0.f) swing(i, target);
        };
        if (i == humanIdx) {
            // The human moves and strikes by hand.
            s.pos = clampField(s.pos + human->move * (kSpeed * 1.15f * dt));
            if (human->move.lengthSq() > 0.f) s.facing = human->move.normalized();
            if (human->strike && s.cooldown <= 0.f && d <= kReach * 1.2f) {
                s.facing = (tp - s.pos).normalized();
                swing(i, t);
            }
        } else if (s.squad < 0) {
            // An escorted leader stays behind its centre and fights whatever reaches it.
            if (d <= kReach) {
                fight(t);
            } else {
                const int q = squadAlive(s.side, 1) ? 1 : squadAlive(s.side, 0) ? 0 : 2;
                const Vec2 anchor = centroid(s.side, q) - forward(s.side) * 250.f;
                if (dist(anchor, s.pos) > 60.f) moveToward(s, anchor, kSpeed * 0.9f, dt);
            }
        } else {
            const Order o = orders_[s.side][s.squad];
            if (o == Order::FallBack) {
                if (d <= kReach && s.cooldown <= 0.f) swing(i, t);  // strike back while giving ground
                moveToward(s, s.pos - forward(s.side) * 200.f, kSpeed * 0.8f, dt);
            } else if (d <= kReach) {
                fight(t);
            } else if (o == Order::Hold) {
                // Stand on the anchor and meet whoever comes into the zone around it.
                if (dist(tp, s.anchor) <= kHoldZone) moveToward(s, tp, kSpeed, dt);
                else if (dist(s.pos, s.anchor) > 40.f) moveToward(s, s.anchor, kSpeed, dt);
                s.facing = (tp - s.pos).normalized();
            } else if ((o == Order::FlankLeft || o == Order::FlankRight) && !flanked_[s.side][s.squad]) {
                moveToward(s, waypoint[s.side][s.squad], kSpeed, dt);
            } else if (o == Order::HuntLeader && leader_[1 - s.side] >= 0 && men_[static_cast<size_t>(leader_[1 - s.side])].alive) {
                moveToward(s, men_[static_cast<size_t>(leader_[1 - s.side])].pos, kSpeed, dt);
            } else {
                moveToward(s, tp, kSpeed, dt);
            }
        }
    }
    // Men of one side keep a little room between them rather than piling onto one spot.
    for (size_t a = 0; a < men_.size(); ++a) {
        if (!men_[a].alive) continue;
        for (size_t b = a + 1; b < men_.size(); ++b) {
            if (!men_[b].alive || men_[b].side != men_[a].side) continue;
            const Vec2 d = men_[b].pos - men_[a].pos;
            const float l2 = d.lengthSq();
            if (l2 >= 80.f * 80.f || l2 < 1e-4f) continue;
            const float l = std::sqrt(l2);
            const Vec2 push = d * ((80.f - l) * 0.5f / l);
            men_[a].pos = clampField(men_[a].pos - push);
            men_[b].pos = clampField(men_[b].pos + push);
        }
    }
    // Squad movement, smoothed: what a commander sees of the enemy's intent.
    for (int side = 0; side < 2; ++side) {
        for (int q = 0; q < kSquads; ++q) {
            if (!squadAlive(side, q) || dt <= 0.f) continue;
            const Vec2 c = centroid(side, q);
            velocity_[side][q] = velocity_[side][q] * 0.7f + (c - lastCentroid_[side][q]) * (0.3f / dt);
            lastCentroid_[side][q] = c;
        }
    }
    // A side that falls below a quarter of its men routs, as does a lone leader that falls;
    // the time cap settles a stalemate.
    for (int side = 0; side < 2; ++side) {
        if (leaderIsUnit_[side] ? !men_[static_cast<size_t>(leader_[side])].alive : alive(side) * 4 < initial_[side]) finished_ = true;
    }
    if (elapsed_ >= spec_.timeLimit) finished_ = timedOut_ = true;
}

Result Sim::result() const {
    Result r;
    r.timedOut = timedOut_;
    auto standing = [&](int side) -> float {  // share of the side still on its feet
        if (leaderIsUnit_[side]) {
            const Soldier& l = men_[static_cast<size_t>(leader_[side])];
            return std::max(0.f, l.hp) / l.maxHp;
        }
        return initial_[side] ? static_cast<float>(alive(side)) / static_cast<float>(initial_[side]) : 0.f;
    };
    auto loss = [&](int side) { return static_cast<int>(std::lround(static_cast<float>(unitOf(spec_, side).hp) * (1.f - standing(side)))); };
    r.toAttacker = loss(0);
    r.toDefender = loss(1);
    const int ls = spec_.leaderSide;
    if (ls >= 0 && ls < 2 && !leaderIsUnit_[ls] && leader_[ls] >= 0) {
        const Soldier& l = men_[static_cast<size_t>(leader_[ls])];
        r.leaderWound = static_cast<int>(std::lround(static_cast<float>(spec_.leaderHp) * (1.f - std::max(0.f, l.hp) / l.maxHp)));
    }
    const float a = standing(0), d = standing(1);
    r.winner = a > d + 1e-4f ? 0 : d > a + 1e-4f ? 1 : -1;
    return r;
}

void Sim::observe(int side, float* out) const {
    const int enemy = 1 - side;
    int k = 0;
    auto put = [&](float v) { out[k++] = v; };
    auto frame = [&](Vec2 p) { return (side == 0 ? p : p * -1.f) * (1.f / 3000.f); };
    auto squadView = [&](int who, int q, bool full) {
        int n = 0, engaged = 0;
        float hp = 0.f;
        for (size_t i = 0; i < men_.size(); ++i) {
            const Soldier& s = men_[i];
            if (!s.alive || s.side != who || s.squad != q) continue;
            ++n;
            hp += s.hp / s.maxHp;
            const int t = nearestEnemy(static_cast<int>(i));
            engaged += t >= 0 && dist(s.pos, men_[static_cast<size_t>(t)].pos) <= kReach * 1.5f;
        }
        const int init = std::max(squadInitial_[who][q], leaderIsUnit_[who] && q == 1 ? 1 : 0);
        put(init ? static_cast<float>(n) / static_cast<float>(init) : 0.f);
        if (full) put(n ? hp / static_cast<float>(n) : 0.f);
        const Vec2 c = n ? frame(centroid(who, q)) : Vec2();
        put(c.x);
        put(c.y);
        put(n ? static_cast<float>(engaged) / static_cast<float>(n) : 0.f);
        if (full) put(flanked_[who][q] ? 1.f : 0.f);
        if (!full) {
            const Vec2 v = n ? (side == 0 ? velocity_[who][q] : velocity_[who][q] * -1.f) * (1.f / kSpeed) : Vec2();
            put(v.x);
            put(v.y);
        }
    };
    for (int q = 0; q < kSquads; ++q) squadView(side, q, true);   // 6 each
    for (int q = 0; q < kSquads; ++q) squadView(enemy, q, false); // 6 each
    for (int who : {side, enemy}) {
        const int l = leader_[who];
        if (l >= 0 && men_[static_cast<size_t>(l)].alive) {
            const Soldier& s = men_[static_cast<size_t>(l)];
            const Vec2 p = frame(s.pos);
            put(1.f);
            put(s.hp / s.maxHp);
            put(p.x);
            put(p.y);
        } else {
            for (int z = 0; z < 4; ++z) put(0.f);
        }
    }
    put(static_cast<float>(unitOf(spec_, side).strength - unitOf(spec_, enemy).strength) / 20.f);
    put(spec_.timeLimit > 0.f ? timeLeft() / spec_.timeLimit : 0.f);
    put(initial_[side] ? static_cast<float>(alive(side)) / static_cast<float>(initial_[side]) : 0.f);
    put(initial_[enemy] ? static_cast<float>(alive(enemy)) / static_cast<float>(initial_[enemy]) : 0.f);
    put(1.f);
}

}  // namespace sov::battle
