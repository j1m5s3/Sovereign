#include "sovereign_battle/show.h"

#include <algorithm>

namespace sov::battle {

namespace {

// Each man's own variety (when he first swings, which way he falls) comes from his index alone: no clock and no draw
// from the sim's generator, so a replay looks the same each time it is watched.
uint32_t hashOf(int i) { return static_cast<uint32_t>(i) * 2654435761u >> 16; }

// A man's first strike comes this long (0.1 to 0.5 s) after he meets the enemy, so a line does not swing in unison.
double phaseOf(int i) { return 0.1 + 0.4 * static_cast<double>(hashOf(i) & 1023u) / 1023.0; }

constexpr float kHpDrop = 0.005f;        // hp never rises, and a landed blow takes 0.1 hp or more even at extreme odds
constexpr float kCooldownRise = 0.001f;  // the sim sets 1.6 at a swing and otherwise only lowers it
constexpr float kAhead = 0.26f;          // cos 75 degrees: a blow from this near ahead knocks a man onto his back

Cue cue(int man, CueKind kind, float delay, int other) {
    Cue c;
    c.man = man;
    c.kind = kind;
    c.delay = delay;
    c.other = other;
    return c;
}

}  // namespace

Show::Show(const ShowTiming& t) : t_(t) {}

int Show::target(int man) const {
    return man >= 0 && man < static_cast<int>(target_.size()) ? target_[static_cast<size_t>(man)] : -1;
}

void Show::strike(int man, int at, double now) {
    Man& m = mem_[static_cast<size_t>(man)];
    m.struck = now;
    m.impactAt = now + t_.impact;
    m.at = at;
}

bool Show::fallsBackward(const std::vector<Seen>& men, int man, int by) const {
    if (by < 0) return (hashOf(man) & 1u) != 0;
    // He faced whom he fought, else across the field; a blow from ahead of him knocks him back, most of the time.
    const Seen& me = men[static_cast<size_t>(man)];
    const int fought = mem_[static_cast<size_t>(man)].target;
    Vec2 face = fought >= 0 ? (men[static_cast<size_t>(fought)].pos - me.pos).normalized() : Vec2();
    if (face.lengthSq() <= 0.f) face = Vec2(me.side == 0 ? 1.f : -1.f, 0.f);
    const Vec2 from = (men[static_cast<size_t>(by)].pos - me.pos).normalized();
    return dot(face, from) >= kAhead && hashOf(man) % 4u != 0;
}

void Show::update(const std::vector<Seen>& men, double now, std::vector<Cue>& out) {
    const size_t n = men.size();
    if (n < mem_.size()) mem_.clear();  // another battle: the sim's men are only ever added to
    mem_.resize(n);
    target_.assign(n, -1);
    fresh_.assign(n, 0);
    const float engage2 = t_.engage * t_.engage;
    auto inReach = [&](size_t a, size_t b) { return (men[a].pos - men[b].pos).lengthSq() <= engage2; };
    auto distSq = [&](size_t a, size_t b) { return (men[a].pos - men[b].pos).lengthSq(); };

    // 1. Whom each living man fights: the nearest living enemy within engage.
    for (size_t i = 0; i < n; ++i) {
        if (!men[i].alive) continue;
        int best = -1;
        float bestD = 0.f;
        for (size_t j = 0; j < n; ++j) {
            if (!men[j].alive || men[j].side == men[i].side || !inReach(i, j)) continue;
            const float d = distSq(i, j);
            if (best < 0 || d < bestD) {
                best = static_cast<int>(j);
                bestD = d;
            }
        }
        target_[i] = best;
    }

    // 2. First sight: a man is remembered as he is; one already dead lies, with no fall (an online view that started
    // late). One dead before and alive again (only a scene shown an earlier frame) is forgotten, with no cue.
    for (size_t i = 0; i < n; ++i) {
        Man& m = mem_[i];
        if (m.seen && (m.alive || !men[i].alive)) continue;
        const bool first = !m.seen;
        m = Man();
        m.seen = true;
        m.hp = men[i].hp;
        m.alive = men[i].alive;
        m.cooldown = men[i].cooldown;
        fresh_[i] = 1;
        if (first && !men[i].alive) {
            Cue c = cue(static_cast<int>(i), CueKind::Lie, 0.f, -1);
            c.backward = fallsBackward(men, static_cast<int>(i), -1);
            out.push_back(c);
        }
    }
    // Whether he stood at the last frame (a man first seen: now). A man killed this frame still swung in it, and still
    // counts as a foe the man who killed him struck at.
    auto stood = [&](size_t i) { return mem_[i].alive; };

    // 3. Swings, where the frame carries the cooldowns: each man whose cooldown rose swung, at the nearest enemy who
    // stood at the last frame (as the sim's nearestEnemy). Only men dead before this frame are left out: a man can
    // swing and then be killed by a man later in the same step.
    bool carries = n > 0;
    for (const Seen& s : men) carries = carries && s.cooldown >= 0.f;
    std::vector<int> swung;
    for (size_t i = 0; i < n; ++i) {
        const Man& m = mem_[i];
        if (fresh_[i] || !m.alive || men[i].cooldown < 0.f || m.cooldown < 0.f || men[i].cooldown <= m.cooldown + kCooldownRise) continue;
        int at = -1;
        float atD = 0.f;
        for (size_t j = 0; j < n; ++j) {
            if (men[j].side == men[i].side || !stood(j)) continue;
            const float d = distSq(i, j);
            if (at < 0 || d < atD) {
                at = static_cast<int>(j);
                atD = d;
            }
        }
        strike(static_cast<int>(i), at, now);
        swung.push_back(static_cast<int>(i));
    }

    // 4. Blows that landed: each man whose hit points fell, or who died, and whose blow it was.
    std::vector<int> victims, by;
    for (size_t v = 0; v < n; ++v) {
        if (fresh_[v] || !mem_[v].alive) continue;
        if (men[v].alive && mem_[v].hp - men[v].hp <= kHpDrop) continue;
        victims.push_back(static_cast<int>(v));
        by.push_back(-1);
    }
    if (carries) {
        // The sim deals its damage at the swing, so the blow is one struck this frame. The victims are taken with the
        // fewest possible strikers first (foes in reach who struck this frame), so a man with one keeps him; a man with
        // none in reach goes last, as any striker will do for him. Each gets the first of: a foe in reach, not yet
        // credited, who struck at him; one in reach not yet credited, whose strike now counts as at him (the sim chose
        // between two nearly equal foes before the step's jostling moved them); one in reach who struck at him; any in
        // reach; and only then any man who struck, those not yet credited first.
        std::vector<uint8_t> credited(n, 0);
        std::vector<int> foes(victims.size(), 0);
        for (size_t k = 0; k < victims.size(); ++k) {
            const size_t v = static_cast<size_t>(victims[k]);
            for (const int s : swung) foes[k] += men[static_cast<size_t>(s)].side != men[v].side && inReach(static_cast<size_t>(s), v);
        }
        std::vector<size_t> order(victims.size());
        for (size_t k = 0; k < order.size(); ++k) order[k] = k;
        auto rank = [&](size_t k) { return foes[k] > 0 ? foes[k] : static_cast<int>(n) + 1; };
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return rank(a) < rank(b); });
        auto pick = [&](size_t v, bool atHim, bool spare, bool reach) {
            int best = -1;
            for (const int s : swung) {
                const size_t si = static_cast<size_t>(s);
                if (men[si].side == men[v].side) continue;
                if (atHim && mem_[si].at != static_cast<int>(v)) continue;
                if (spare && credited[si]) continue;
                if (reach && !inReach(si, v)) continue;
                if (best < 0 || distSq(si, v) < distSq(static_cast<size_t>(best), v)) best = s;
            }
            return best;
        };
        for (const size_t k : order) {
            const size_t v = static_cast<size_t>(victims[k]);
            int b = pick(v, true, true, true);
            if (b < 0) {
                b = pick(v, false, true, true);
                if (b >= 0) mem_[static_cast<size_t>(b)].at = victims[k];
            }
            if (b < 0) b = pick(v, true, false, true);
            if (b < 0) b = pick(v, false, false, true);
            if (b < 0) b = pick(v, false, true, false);
            if (b < 0) b = pick(v, false, false, false);
            if (b >= 0) credited[static_cast<size_t>(b)] = 1;
            by[k] = b;
        }
        for (const int s : swung) out.push_back(cue(s, CueKind::Strike, 0.f, mem_[static_cast<size_t>(s)].at));
    } else {
        for (size_t k = 0; k < victims.size(); ++k) {
            const size_t v = static_cast<size_t>(victims[k]);
            // A strike on its way to him whose blow is still to land, the soonest.
            int best = -1;
            for (size_t e = 0; e < n; ++e) {
                const Man& m = mem_[e];
                if (men[e].side == men[v].side || !(men[e].alive || stood(e)) || m.at != victims[k] || m.impactAt < now || !inReach(e, v)) continue;
                if (best < 0 || m.impactAt < mem_[static_cast<size_t>(best)].impactAt) best = static_cast<int>(e);
            }
            // Else an enemy in reach who fights him, else any enemy in reach, the one who struck longest ago: free, he
            // strikes now; still striking, the blow is his strike's (a strike is never restarted mid-clip). The living
            // come first, so a man killed in the interval starts a new strike only when nobody else could have struck.
            for (int pass = 0; pass < 4 && best < 0; ++pass) {
                const bool living = pass < 2;
                for (size_t e = 0; e < n; ++e) {
                    if (men[e].side == men[v].side || men[e].alive != living || !stood(e) || !inReach(e, v)) continue;
                    const int fights = men[e].alive ? target_[e] : mem_[e].target;
                    if (pass % 2 == 0 && fights != victims[k]) continue;
                    const size_t b = static_cast<size_t>(best);
                    if (best < 0 || mem_[e].struck < mem_[b].struck || (mem_[e].struck == mem_[b].struck && distSq(e, v) < distSq(b, v))) {
                        best = static_cast<int>(e);
                    }
                }
                if (best >= 0 && now - mem_[static_cast<size_t>(best)].struck >= t_.strike) {
                    strike(best, victims[k], now);
                    out.push_back(cue(best, CueKind::Strike, 0.f, victims[k]));
                }
            }
            by[k] = best;
        }
    }
    for (size_t k = 0; k < victims.size(); ++k) {
        const size_t v = static_cast<size_t>(victims[k]);
        const float delay = by[k] >= 0 ? static_cast<float>(std::max(0.0, mem_[static_cast<size_t>(by[k])].impactAt - now)) : 0.f;
        if (!men[v].alive) {
            // The killing blow gets no flinch: he falls.
            Cue c = cue(victims[k], CueKind::Fall, delay, by[k]);
            c.backward = fallsBackward(men, victims[k], by[k]);
            out.push_back(c);
        } else {
            Cue c = cue(victims[k], CueKind::Hit, delay, by[k]);
            c.weight = std::clamp(0.5f + (mem_[v].hp - men[v].hp) / 4.f, 0.5f, 1.f);
            out.push_back(c);
        }
    }

    // 5. Strikes on their own, where the frame carries no cooldowns (after the blows, so a man credited one is not
    // struck twice): a man who meets the enemy first strikes a moment later, as the sim's does (cooldown 0 on
    // arrival), then once a cadence while he is engaged. A blow that lands before such a strike's impact attaches to it
    // (step 4), so it becomes a hit rather than a miss.
    if (!carries) {
        for (size_t i = 0; i < n; ++i) {
            if (!men[i].alive || target_[i] < 0) continue;
            Man& m = mem_[i];
            if (!m.engaged) m.struck = std::max(m.struck, now - t_.cadence + phaseOf(static_cast<int>(i)));
            if (now - m.struck >= t_.cadence) {
                strike(static_cast<int>(i), target_[i], now);
                out.push_back(cue(static_cast<int>(i), CueKind::Strike, 0.f, target_[i]));
            }
        }
    }

    // 6. Remember the men as this frame shows them.
    for (size_t i = 0; i < n; ++i) {
        Man& m = mem_[i];
        m.hp = men[i].hp;
        m.alive = men[i].alive;
        m.cooldown = men[i].cooldown;
        m.engaged = target_[i] >= 0;
        m.target = target_[i];
    }
}

}  // namespace sov::battle
