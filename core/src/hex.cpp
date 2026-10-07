#include "sovereign/hex.h"

#include <algorithm>
#include <cstdlib>

namespace sov {

namespace {
int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

// Rounds n / d to the nearest integer, halves away from zero.
int64_t roundDiv(int64_t n, int64_t d) {
    return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d);
}

// Point i of the n + 1 points of the line from aa to bb (n their distance), in axial coordinates.
Axial lineAxial(Axial aa, Axial bb, int n, int i) {
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
    return Axial{static_cast<int32_t>(rx), static_cast<int32_t>(rz)};
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

std::optional<Dir> HexGrid::directionTo(Hex a, Hex b) const {
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = neighbor(a, static_cast<Dir>(d));
        if (n && *n == b) return static_cast<Dir>(d);
    }
    return std::nullopt;
}

Axial HexGrid::nearestAxial(Axial aa, Hex b) const {
    const Axial bb = toAxial(b);
    if (!wrap_) return bb;
    Axial best = bb;
    int bestDist = axialDistance(aa, bb);
    // A copy a map's width east or west is at least w - |dq| away (its q alone differs that much), so it is nearer
    // only when b itself is farther than that.
    if (bestDist <= w_ - std::abs(bb.q - aa.q)) return best;
    for (int shift : {-w_, w_}) {
        const Axial cand{bb.q + shift, bb.r};  // b moved a map's width: the same row, its q shifted by the width
        const int d = axialDistance(aa, cand);
        if (d < bestDist) {
            bestDist = d;
            best = cand;
        }
    }
    return best;
}

int HexGrid::distance(Hex a, Hex b) const {
    // The nearest of b and, on a wrapping map, its copies a map's width to the east and west (nearestAxial's choice).
    const Axial aa = toAxial(a), bb = toAxial(b);
    const int d = axialDistance(aa, bb);
    if (!wrap_ || d <= w_ - std::abs(bb.q - aa.q)) return d;  // no copy can be nearer (nearestAxial's bound)
    return std::min({d, axialDistance(aa, Axial{bb.q - w_, bb.r}), axialDistance(aa, Axial{bb.q + w_, bb.r})});
}

std::vector<Hex> HexGrid::within(Hex center, int radius) const {
    std::vector<Hex> out;
    out.reserve(static_cast<size_t>(std::min<int64_t>(3 * static_cast<int64_t>(radius) * (radius + 1) + 1, size())));
    // A row spans 2 * radius + 1 hexes, so only a map narrower than that can wrap onto itself.
    const bool mayRepeat = wrap_ && w_ <= 2 * static_cast<int64_t>(radius);
    walkWithin(center, radius, [&](Hex h, Axial) {
        if (!mayRepeat || std::find(out.begin(), out.end(), h) == out.end()) out.push_back(h);
    });
    return out;
}

std::vector<Hex> HexGrid::line(Hex a, Hex b) const {
    Axial aa = toAxial(a);
    Axial bb = nearestAxial(aa, b);
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
    return normalize(toOffset(lineAxial(aa, bb, n, i)));
}

const HexGrid::LineStep* HexGrid::lineSteps(Axial delta, int n) {
    // Built once: for each offset within kLineStepRange, line(a, b)'s inside as steps from a. lineAxial's nudges keep
    // every point off a tie, so moving both ends by whole hexes moves its points by as much: the steps from (0, 0) to
    // the offset serve every a, with a's row parity choosing the column step (odd rows sit half a hex east).
    struct Table {
        std::vector<int32_t> first;  // by offset (dq, dr): the index of its first step in steps
        std::vector<LineStep> steps;
        static size_t slot(int dq, int dr) { return static_cast<size_t>((dq + kLineStepRange) * (2 * kLineStepRange + 1) + dr + kLineStepRange); }
        Table() {
            constexpr int R = kLineStepRange;
            first.assign(slot(R, R) + 1, 0);
            for (int dq = -R; dq <= R; ++dq) {
                for (int dr = -R; dr <= R; ++dr) {
                    const Axial to{dq, dr};
                    const int len = axialDistance(Axial{}, to);
                    first[slot(dq, dr)] = static_cast<int32_t>(steps.size());
                    for (int i = 1; i < len && len <= R; ++i) {
                        const Axial s = lineAxial(Axial{}, to, len, i);
                        LineStep st{};
                        st.dx[0] = static_cast<int8_t>(s.q + floorDiv(s.r, 2));      // from an even row
                        st.dx[1] = static_cast<int8_t>(s.q + floorDiv(s.r + 1, 2));  // from an odd row
                        st.dy = static_cast<int8_t>(s.r);
                        steps.push_back(st);
                    }
                }
            }
        }
    };
    static const Table table;
    if (n > kLineStepRange) return nullptr;  // |dq| and |dr| are at most n
    return table.steps.data() + table.first[Table::slot(delta.q, delta.r)];
}

}  // namespace sov
