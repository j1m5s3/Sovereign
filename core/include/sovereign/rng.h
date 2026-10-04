// Deterministic random numbers. The core owns every random draw; each purpose
// gets its own stream so a cosmetic draw never shifts a combat roll (engine
// doc, Core foundations: RNG). Never use <random> distributions here: their
// output is implementation-defined and differs between MSVC and GCC.
#pragma once

#include "sovereign/api.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace sov {

enum class RngStream : uint8_t {
    MapGen = 0,
    Gameplay,  // goody huts, spawns, disasters
    Combat,
    AI,
    Visual,    // cosmetic only; never read by rules
    Count
};

// xoshiro256** seeded through splitmix64.
class SOV_API Rng {
public:
    Rng() { seed(0); }
    explicit Rng(uint64_t s) { seed(s); }
    void seed(uint64_t s);

    uint64_t next();
    // Uniform integer in [0, bound). bound must be > 0.
    uint32_t below(uint32_t bound);
    // Uniform integer in [lo, hi] inclusive.
    int32_t range(int32_t lo, int32_t hi);
    // True with probability percent / 100.
    bool chance(uint32_t percent) { return below(100) < percent; }

    const std::array<uint64_t, 4>& state() const { return s_; }
    void setState(const std::array<uint64_t, 4>& s) { s_ = s; }

private:
    std::array<uint64_t, 4> s_{};
};

class SOV_API RngSet {
public:
    void seed(uint64_t gameSeed);
    Rng& get(RngStream stream) { return streams_[static_cast<size_t>(stream)]; }
    const Rng& get(RngStream stream) const { return streams_[static_cast<size_t>(stream)]; }

private:
    std::array<Rng, static_cast<size_t>(RngStream::Count)> streams_;
};

SOV_API uint64_t splitmix64(uint64_t& x);

}  // namespace sov
