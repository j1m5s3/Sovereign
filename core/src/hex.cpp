#include "sovereign/hex.h"

#include <algorithm>
#include <cstdlib>

namespace sov {

namespace {
constexpr std::array<Axial, 6> kAxialDirs = {{
    {1, -1},  // NE
    {1, 0},   // E
    {0, 1},   // SE
    {-1, 1},  // SW
    {-1, 0},  // W
    {0, -1},  // NW
}};

int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// Rounds n / d to the nearest integer, halves away from zero.
int64_t roundDiv(int64_t n, int64_t d) {
    return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d);
}
}  // namespace

Axial toAxial(Hex h) {
    // odd-r: odd rows are shifted half a hex to the east.
    return Axial{h.x - static_cast<int32_t>(floorDiv(h.y, 2)), h.y};
}

Hex toOffset(Axial a) {
    return Hex{a.q + static_cast<int32_t>(floorDiv(a.r, 2)), a.r};
}

int axialDistance(Axial a, Axial b) {
    int dq = a.q - b.q, dr = a.r - b.r;
    return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

std::optional<Hex> HexGrid::normalize(Hex h) const {
    if (h.y < 0 || h.y >= h_) return std::nullopt;
    if (h.x < 0 || h.x >= w_) {
        if (!wrap_) return std::nullopt;
        h.x = static_cast<int32_t>(((h.x % w_) + w_) % w_);
    }
    return h;
}

std::optional<Hex> HexGrid::neighbor(Hex h, Dir d) const {
    Axial a = toAxial(h);
    const Axial& o = kAxialDirs[static_cast<int>(d)];
    return normalize(toOffset(Axial{a.q + o.q, a.r + o.r}));
}

std::optional<Dir> HexGrid::directionTo(Hex a, Hex b) const {
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = neighbor(a, static_cast<Dir>(d));
        if (n && *n == b) return static_cast<Dir>(d);
    }
    return std::nullopt;
}

Axial HexGrid::nearestAxial(Hex a, Hex b) const {
    Axial aa = toAxial(a);
    Axial best = toAxial(b);
    if (!wrap_) return best;
    int bestDist = axialDistance(aa, best);
    for (int shift : {-w_, w_}) {
        Axial cand = toAxial(Hex{b.x + shift, b.y});
        int d = axialDistance(aa, cand);
        if (d < bestDist) {
            bestDist = d;
            best = cand;
        }
    }
    return best;
}

int HexGrid::distance(Hex a, Hex b) const {
    return axialDistance(toAxial(a), nearestAxial(a, b));
}

std::vector<Hex> HexGrid::within(Hex center, int radius) const {
    std::vector<Hex> out;
    out.reserve(static_cast<size_t>(std::min<int64_t>(3 * static_cast<int64_t>(radius) * (radius + 1) + 1, size())));
    // A row spans 2 * radius + 1 hexes, so only a map narrower than that can wrap onto itself.
    const bool mayRepeat = wrap_ && w_ <= 2 * static_cast<int64_t>(radius);
    Axial c = toAxial(center);
    for (int dr = -radius; dr <= radius; ++dr) {
        int qMin = std::max(-radius, -dr - radius);
        int qMax = std::min(radius, -dr + radius);
        for (int dq = qMin; dq <= qMax; ++dq) {
            auto h = normalize(toOffset(Axial{c.q + dq, c.r + dr}));
            if (!h) continue;
            if (mayRepeat && std::find(out.begin(), out.end(), *h) != out.end()) continue;
            out.push_back(*h);
        }
    }
    return out;
}

std::vector<Hex> HexGrid::line(Hex a, Hex b) const {
    Axial aa = toAxial(a);
    Axial bb = nearestAxial(a, b);
    int n = axialDistance(aa, bb);
    std::vector<Hex> out;
    out.reserve(static_cast<size_t>(n) + 1);
    if (n == 0) {
        out.push_back(a);
        return out;
    }
    for (int i = 0; i <= n; ++i) {
        if (auto h = linePoint(aa, bb, n, i)) out.push_back(*h);
    }
    return out;
}

std::optional<Hex> HexGrid::linePoint(Axial aa, Axial bb, int n, int i) const {
    // Cube lerp in integers scaled by n * kNudgeScale, with a small nudge so
    // points on hex edges fall consistently to one side.
    constexpr int64_t kNudgeScale = 1000000;
    const int64_t S = static_cast<int64_t>(n) * kNudgeScale;
    const int64_t ax = aa.q, az = aa.r, ay = -aa.q - aa.r;
    const int64_t bx = bb.q, bz = bb.r, by = -bb.q - bb.r;
    int64_t x = (ax * n + (bx - ax) * i) * kNudgeScale + 1;
    int64_t y = (ay * n + (by - ay) * i) * kNudgeScale + 2;
    int64_t z = (az * n + (bz - az) * i) * kNudgeScale - 3;
    int64_t rx = roundDiv(x, S), ry = roundDiv(y, S), rz = roundDiv(z, S);
    int64_t dx = std::llabs(rx * S - x), dy = std::llabs(ry * S - y), dz = std::llabs(rz * S - z);
    if (dx > dy && dx > dz) rx = -ry - rz;
    else if (dy > dz) ry = -rx - rz;
    else rz = -rx - ry;
    return normalize(toOffset(Axial{static_cast<int32_t>(rx), static_cast<int32_t>(rz)}));
}

}  // namespace sov
