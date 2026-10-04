#include "sovereign/rng.h"

namespace sov {

uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void Rng::seed(uint64_t s) {
    for (auto& word : s_) word = splitmix64(s);
}

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint64_t Rng::next() {
    const uint64_t result = rotl(s_[1] * 5, 7) * 9;
    const uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
}

uint32_t Rng::below(uint32_t bound) {
    // Rejection sampling keeps the result unbiased and portable.
    const uint32_t threshold = (0u - bound) % bound;
    for (;;) {
        uint32_t r = static_cast<uint32_t>(next() >> 32);
        if (r >= threshold) return r % bound;
    }
}

int32_t Rng::range(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    return lo + static_cast<int32_t>(below(static_cast<uint32_t>(hi - lo) + 1));
}

void RngSet::seed(uint64_t gameSeed) {
    for (size_t i = 0; i < streams_.size(); ++i) {
        uint64_t x = gameSeed ^ (0xA0761D6478BD642Full * (i + 1));
        streams_[i].seed(splitmix64(x));
    }
}

}  // namespace sov
