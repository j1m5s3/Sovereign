// Trains the battle commander by self-play (leader doc §10, layer 3), offline on our side.
//
//   battle_train --out data/battle_ai/commander.txt [--generations 1500] [--population 64]
//                [--episodes 64] [--threads N] [--seed 1] [--init file]
//
// Evolution strategies: each generation perturbs the network's weights in random antithetic
// pairs, plays every perturbation through the same random matchups (the candidate takes
// each side in turn) against the scripted baselines and earlier versions of itself, and
// moves the weights toward the perturbations that scored best (rank-normalised, Adam). It
// keeps the version with the best worst case against the scripted styles.
// Deterministic for a seed and thread count; no third-party code.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "sovereign_battle/arena.h"

using namespace sov::battle;

namespace {

struct Options {
    std::string out = "commander.txt";
    std::string init;
    int generations = 1500;
    int population = 64;  // antithetic pairs
    int episodes = 64;
    int threads = 0;
    uint64_t seed = 1;
    float sigma = 0.05f;
    float lr = 0.03f;
};

struct Opponent {
    Order fixed = Order::Advance;
    std::shared_ptr<const Policy> policy;  // null: the fixed order
    Commander make(uint64_t seed) const { return policy ? Commander::trained(policy, 0.f, seed) : Commander::fixed(fixed); }
};

struct Episode {
    Spec spec;
    int side = 0;  // the candidate's side
    Opponent opponent;
};

float evaluate(const std::shared_ptr<const Policy>& candidate, const std::vector<Episode>& episodes) {
    float total = 0.f;
    for (const Episode& e : episodes) {
        Commander me = Commander::trained(candidate, 0.f, e.spec.seed);
        Commander them = e.opponent.make(e.spec.seed + 1);
        const Result r = e.side == 0 ? playOut(e.spec, me, them) : playOut(e.spec, them, me);
        total += score(e.spec, r, e.side);
    }
    return total / static_cast<float>(episodes.size());
}

template <typename F>
void parallelFor(int n, int threads, F fn) {
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&] {
            for (int i = next++; i < n; i = next++) fn(i);
        });
    }
    for (std::thread& th : pool) th.join();
}

// Fixed matchups for progress reports: the candidate against one baseline, both sides.
std::vector<Episode> benchmark(Order baseline, int count, uint64_t seed) {
    Rng rng(seed);
    std::vector<Episode> out;
    for (int i = 0; i < count; ++i) {
        Episode e;
        e.spec = randomScenario(rng);
        e.side = i % 2;
        e.opponent.fixed = baseline;
        out.push_back(e);
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (!std::strcmp(argv[i], "--out")) o.out = next();
        else if (!std::strcmp(argv[i], "--init")) o.init = next();
        else if (!std::strcmp(argv[i], "--generations")) o.generations = std::atoi(next());
        else if (!std::strcmp(argv[i], "--population")) o.population = std::atoi(next());
        else if (!std::strcmp(argv[i], "--episodes")) o.episodes = std::atoi(next());
        else if (!std::strcmp(argv[i], "--threads")) o.threads = std::atoi(next());
        else if (!std::strcmp(argv[i], "--seed")) o.seed = std::strtoull(next(), nullptr, 10);
        else if (!std::strcmp(argv[i], "--sigma")) o.sigma = static_cast<float>(std::atof(next()));
        else if (!std::strcmp(argv[i], "--lr")) o.lr = static_cast<float>(std::atof(next()));
        else {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return 2;
        }
    }
    if (o.threads <= 0) o.threads = std::max(1u, std::thread::hardware_concurrency());

    Rng rng(o.seed);
    Policy theta;
    if (!o.init.empty()) {
        std::string err;
        if (!theta.load(o.init, &err)) {
            std::fprintf(stderr, "%s\n", err.c_str());
            return 1;
        }
    } else {
        theta.initRandom(rng, 0.1f);
    }
    const size_t n = theta.params.size();
    std::vector<float> m(n, 0.f), v(n, 0.f);
    std::vector<std::shared_ptr<const Policy>> pool;  // earlier versions, for self-play
    // Progress is judged on the worst of the scripted styles a player might use, so the
    // commander that ships is the one hardest to exploit.
    const Order styles[] = {Order::Advance, Order::Hold, Order::FlankLeft, Order::FlankRight, Order::HuntLeader};
    constexpr int kStyles = 5;
    std::vector<std::vector<Episode>> bench;
    for (int k = 0; k < kStyles; ++k) bench.push_back(benchmark(styles[k], 120, 9001 + static_cast<uint64_t>(k)));
    float best = -1e9f;
    const auto t0 = std::chrono::steady_clock::now();
    std::printf("training %zu parameters: %d generations, %d pairs, %d episodes, %d threads\n", n, o.generations, o.population, o.episodes,
                o.threads);

    for (int gen = 1; gen <= o.generations; ++gen) {
        // The generation's matchups, shared by every perturbation (common random numbers).
        std::vector<Episode> episodes;
        for (int e = 0; e < o.episodes; ++e) {
            Episode ep;
            ep.spec = randomScenario(rng);
            ep.side = e % 2;
            // Opponents: each scripted style, and earlier versions of itself (self-play).
            const int kind = (e / 2) % 8;
            if (kind < kStyles || pool.empty()) ep.opponent.fixed = styles[kind % kStyles];
            else ep.opponent.policy = pool[static_cast<size_t>(rng.below(static_cast<int>(pool.size())))];
            episodes.push_back(ep);
        }
        std::vector<std::vector<float>> noise(static_cast<size_t>(o.population), std::vector<float>(n));
        for (auto& eps : noise)
            for (float& x : eps) x = rng.normal();
        std::vector<float> fitness(static_cast<size_t>(o.population) * 2);
        parallelFor(o.population * 2, o.threads, [&](int k) {
            auto p = std::make_shared<Policy>(theta);
            const std::vector<float>& eps = noise[static_cast<size_t>(k / 2)];
            const float sign = k % 2 == 0 ? 1.f : -1.f;
            for (size_t j = 0; j < n; ++j) p->params[j] += sign * o.sigma * eps[j];
            fitness[static_cast<size_t>(k)] = evaluate(p, episodes);
        });
        // Rank-normalise to [-0.5, 0.5] so a few lucky battles do not dominate.
        std::vector<int> order(fitness.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return fitness[static_cast<size_t>(a)] < fitness[static_cast<size_t>(b)]; });
        std::vector<float> rank(fitness.size());
        for (size_t r = 0; r < order.size(); ++r) rank[static_cast<size_t>(order[r])] = static_cast<float>(r) / static_cast<float>(order.size() - 1) - 0.5f;
        std::vector<float> grad(n, 0.f);
        for (int pidx = 0; pidx < o.population; ++pidx) {
            const float w = rank[static_cast<size_t>(2 * pidx)] - rank[static_cast<size_t>(2 * pidx + 1)];
            const std::vector<float>& eps = noise[static_cast<size_t>(pidx)];
            for (size_t j = 0; j < n; ++j) grad[j] += w * eps[j];
        }
        const float b1 = 0.9f, b2 = 0.999f;
        const float c1 = 1.f - std::pow(b1, static_cast<float>(gen)), c2 = 1.f - std::pow(b2, static_cast<float>(gen));
        for (size_t j = 0; j < n; ++j) {
            const float g = grad[j] / (static_cast<float>(o.population) * o.sigma) - 0.005f * theta.params[j];
            m[j] = b1 * m[j] + (1.f - b1) * g;
            v[j] = b2 * v[j] + (1.f - b2) * g * g;
            theta.params[j] += o.lr * (m[j] / c1) / (std::sqrt(v[j] / c2) + 1e-8f);
        }
        if (gen % 10 == 0) {
            pool.push_back(std::make_shared<Policy>(theta));
            if (pool.size() > 8) pool.erase(pool.begin());
        }
        const float meanFit = std::accumulate(fitness.begin(), fitness.end(), 0.f) / static_cast<float>(fitness.size());
        if (gen % 10 == 0 || gen == o.generations) {
            auto cur = std::make_shared<const Policy>(theta);
            float vs[kStyles];
            parallelFor(kStyles, kStyles, [&](int k) { vs[k] = evaluate(cur, bench[static_cast<size_t>(k)]); });
            const float worst = *std::min_element(vs, vs + kStyles);
            const float both = worst + 0.1f * std::accumulate(vs, vs + kStyles, 0.f) / kStyles;
            const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::printf("gen %4d  fitness %+.3f  vs advance %+.3f hold %+.3f flank %+.3f %+.3f hunt %+.3f  worst %+.3f  (%.0f s)%s\n", gen,
                        meanFit, vs[0], vs[1], vs[2], vs[3], vs[4], worst, secs, both > best ? "  saved" : "");
            std::fflush(stdout);
            if (both > best) {
                best = both;
                if (!theta.save(o.out)) {
                    std::fprintf(stderr, "cannot write %s\n", o.out.c_str());
                    return 1;
                }
            }
        }
    }
    std::printf("best benchmark %+.3f written to %s\n", best, o.out.c_str());
    return 0;
}
