// The battle AI (leader doc §10, layer 3): a commander that orders one side's three squads
// every second. The trained one is a small network learned by self-play
// (battle/tools/battle_train.cpp) that runs on any CPU through its own forward pass; a
// runtime such as ONNX Runtime can replace that later behind the same interface
// (engine doc: third-party runtimes behind small interfaces). The scripted commanders
// are the baselines it is trained and tested against, and the fallback when no weights ship.
#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "sovereign/api.h"
#include "sovereign_battle/sim.h"

namespace sov::battle {

// A one-hidden-layer network: observation -> tanh hidden -> one logit per (squad, order).
struct SOV_API Policy {
    int inputs = Sim::kObservation;
    int hidden = 32;
    int outputs = kSquads * kOrders;
    std::vector<float> params;  // W1 (hidden x inputs), b1, W2 (outputs x hidden), b2

    static int paramCount(int in, int hid, int out) { return hid * in + hid + out * hid + out; }
    int paramCount() const { return paramCount(inputs, hidden, outputs); }
    bool valid() const { return inputs == Sim::kObservation && outputs == kSquads * kOrders && static_cast<int>(params.size()) == paramCount(); }
    // Small random weights (the trainer's starting point).
    void initRandom(Rng& rng, float scale = 0.1f);
    void forward(const float* observation, float* logits) const;

    // Plain text: "sovereign-commander 1", "inputs I hidden H outputs O", then the parameters.
    bool save(const std::string& path) const;
    bool load(const std::string& path, std::string* error = nullptr);
};

// Adjustments to a commander's orders against an opponent whose habits are known (leader doc
// §10, player modelling: the battle AI adapts within its styles; it never retrains mid-game).
struct Counter {
    bool holdFlanks = false;  // against a flanker: the side squads hold instead of advancing
    bool huntLeader = false;  // against a leader who fights in front: the centre hunts it
    bool pursue = false;      // against an opponent who falls back: no holding, press on
};

class SOV_API Commander {
public:
    static constexpr float kDecisionSeconds = 1.f;

    // Every squad gets the same order all battle (the scripted baselines).
    static Commander fixed(Order o);
    // The trained network. Temperature 0 always takes the best order; higher values sample
    // and play worse (difficulty levels).
    static Commander trained(std::shared_ptr<const Policy> policy, float temperature = 0.f, uint64_t seed = 1);

    bool isTrained() const { return policy_ != nullptr; }
    // Gives orders when a decision is due; call every simulation step before Sim::step.
    void update(Sim& sim, int side, float dt);
    // The orders the network would give now (no timer), after the counter.
    std::array<Order, kSquads> decide(const Sim& sim, int side);
    void setCounter(const Counter& c) { counter_ = c; }
    const Counter& counter() const { return counter_; }
    static std::array<Order, kSquads> applyCounter(const Counter& c, std::array<Order, kSquads> orders);

private:
    std::array<Order, kSquads> decideRaw(const Sim& sim, int side);
    Order fixed_ = Order::Advance;
    std::shared_ptr<const Policy> policy_;
    float temperature_ = 0.f;
    Counter counter_;
    float timer_ = 0.f;
    Rng rng_;
};

}  // namespace sov::battle
