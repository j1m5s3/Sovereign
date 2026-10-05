#include "sovereign_battle/commander.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace sov::battle {

void Policy::initRandom(Rng& rng, float scale) {
    params.assign(static_cast<size_t>(paramCount()), 0.f);
    for (float& p : params) p = rng.normal() * scale;
}

void Policy::forward(const float* observation, float* logits) const {
    const float* w1 = params.data();
    const float* b1 = w1 + hidden * inputs;
    const float* w2 = b1 + hidden;
    const float* b2 = w2 + outputs * hidden;
    std::vector<float> h(static_cast<size_t>(hidden));
    for (int j = 0; j < hidden; ++j) {
        float a = b1[j];
        const float* row = w1 + j * inputs;
        for (int i = 0; i < inputs; ++i) a += row[i] * observation[i];
        h[static_cast<size_t>(j)] = std::tanh(a);
    }
    for (int o = 0; o < outputs; ++o) {
        float a = b2[o];
        const float* row = w2 + o * hidden;
        for (int j = 0; j < hidden; ++j) a += row[j] * h[static_cast<size_t>(j)];
        logits[o] = a;
    }
}

bool Policy::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "sovereign-commander 1\n";
    out << "inputs " << inputs << " hidden " << hidden << " outputs " << outputs << "\n";
    char buf[32];
    for (size_t i = 0; i < params.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%.7g", static_cast<double>(params[i]));
        out << buf << ((i + 1) % 8 == 0 ? '\n' : ' ');
    }
    out << "\n";
    return static_cast<bool>(out);
}

bool Policy::load(const std::string& path, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = path + ": " + why;
        return false;
    };
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail("cannot open");
    std::string magic, k1, k2, k3;
    int version = 0;
    in >> magic >> version >> k1 >> inputs >> k2 >> hidden >> k3 >> outputs;
    if (magic != "sovereign-commander" || version != 1 || k1 != "inputs" || k2 != "hidden" || k3 != "outputs") return fail("not a commander file");
    if (inputs != Sim::kObservation || outputs != kSquads * kOrders || hidden < 1 || hidden > 1024) return fail("shape does not match this build");
    params.assign(static_cast<size_t>(paramCount()), 0.f);
    for (float& p : params) {
        if (!(in >> p)) return fail("too few parameters");
    }
    return true;
}

Commander Commander::fixed(Order o) {
    Commander c;
    c.fixed_ = o;
    return c;
}

Commander Commander::trained(std::shared_ptr<const Policy> policy, float temperature, uint64_t seed) {
    Commander c;
    if (policy && policy->valid()) c.policy_ = std::move(policy);
    c.temperature_ = temperature;
    c.rng_.reseed(seed);
    return c;
}

std::array<Order, kSquads> Commander::decide(const Sim& sim, int side) {
    std::array<Order, kSquads> out;
    out.fill(fixed_);
    if (!policy_) return out;
    float obs[Sim::kObservation];
    sim.observe(side, obs);
    float logits[kSquads * kOrders];
    policy_->forward(obs, logits);
    for (int q = 0; q < kSquads; ++q) {
        const float* l = logits + q * kOrders;
        int best = static_cast<int>(std::max_element(l, l + kOrders) - l);
        if (temperature_ > 0.f) {
            // Softmax sampling: a weaker, less predictable commander.
            float p[kOrders], sum = 0.f;
            for (int o = 0; o < kOrders; ++o) sum += p[o] = std::exp((l[o] - l[best]) / temperature_);
            float r = rng_.unit() * sum;
            for (int o = 0; o < kOrders; ++o) {
                if ((r -= p[o]) <= 0.f) {
                    best = o;
                    break;
                }
            }
        }
        out[static_cast<size_t>(q)] = static_cast<Order>(best);
    }
    return out;
}

void Commander::update(Sim& sim, int side, float dt) {
    timer_ -= dt;
    if (timer_ > 0.f) return;
    timer_ += kDecisionSeconds;
    if (timer_ < 0.f) timer_ = kDecisionSeconds;
    const std::array<Order, kSquads> orders = decide(sim, side);
    for (int q = 0; q < kSquads; ++q) sim.setOrder(side, q, orders[static_cast<size_t>(q)]);
}

}  // namespace sov::battle
