#include "sovereign_battle/arena.h"

#include <algorithm>

namespace sov::battle {

Result playOut(const Spec& spec, Commander& side0, Commander& side1, float dt) {
    Sim sim;
    sim.start(spec);
    const int maxSteps = static_cast<int>(spec.timeLimit / dt) + 2;
    for (int i = 0; i < maxSteps && !sim.finished(); ++i) {
        side0.update(sim, 0, dt);
        side1.update(sim, 1, dt);
        sim.step(dt);
    }
    return sim.result();
}

Spec randomScenario(Rng& rng, int maxStrengthGap) {
    Spec s;
    const int base = 20 + rng.below(41);  // Warrior to Musketman-ish
    const int gap = std::clamp(static_cast<int>(rng.normal() * 6.f), -maxStrengthGap, maxStrengthGap);
    s.attacker.strength = base + gap / 2;
    s.defender.strength = base - (gap - gap / 2);
    s.attacker.hp = 40 + 10 * rng.below(7);
    s.defender.hp = 40 + 10 * rng.below(7);
    const float roll = rng.unit();
    s.leaderSide = roll < 0.45f ? 0 : roll < 0.9f ? 1 : -1;
    s.humanSide = -1;
    if (s.leaderSide >= 0) {
        const UnitSpec& u = s.leaderSide == 0 ? s.attacker : s.defender;
        s.leaderStrength = u.strength + rng.below(11) - 3;
        s.leaderHp = 60 + 10 * rng.below(5);
        if (rng.unit() < 0.08f) {
            // An unescorted leader caught alone.
            UnitSpec& lone = s.leaderSide == 0 ? s.attacker : s.defender;
            lone.leaderIsUnit = true;
            lone.hp = s.leaderHp;
            s.leaderSide = -1;
        }
    }
    s.seed = static_cast<uint32_t>(rng.next());
    s.timeLimit = 180.f;
    return s;
}

float score(const Spec& spec, const Result& r, int side) {
    const float lossA = static_cast<float>(r.toAttacker) / static_cast<float>(std::max(1, spec.attacker.hp));
    const float lossD = static_cast<float>(r.toDefender) / static_cast<float>(std::max(1, spec.defender.hp));
    float v = side == 0 ? lossD - lossA : lossA - lossD;
    if (spec.leaderSide >= 0 && r.leaderWound >= spec.leaderHp) v += spec.leaderSide == side ? -0.5f : 0.5f;
    return v;
}

}  // namespace sov::battle
