// Hex grid math. Pointy-top hexes, odd-row offset storage, axial/cube math,
// map wraps east-west (cylinder) when enabled; never north-south
// (specs/civ6/01-map-and-terrain.md, Grid).
#pragma once

#include "sovereign/api.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

namespace sov {

// Civ's DirectionTypes order.
enum class Dir : uint8_t { NE = 0, E, SE, SW, W, NW };
constexpr int kNumDirs = 6;
constexpr Dir opposite(Dir d) { return static_cast<Dir>((static_cast<int>(d) + 3) % 6); }

struct Axial {
    int32_t q = 0, r = 0;
    bool operator==(const Axial& o) const { return q == o.q && r == o.r; }
};

// Offset (column, row) position on the map. Row 0 is the north edge.
struct Hex {
    int32_t x = 0, y = 0;
    bool operator==(const Hex& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Hex& o) const { return !(*this == o); }
};

// floorDiv(a, 2) as one shift (every compiler this builds with shifts a negative arithmetically, which floors), where
// the division's rounding fix takes a branch on the row's parity.
constexpr int32_t floorHalf(int32_t a) { return a >> 1; }
static_assert(floorHalf(-3) == -2 && floorHalf(-4) == -2 && floorHalf(-1) == -1 && floorHalf(3) == 1, "a right shift floors");

// odd-r: odd rows are shifted half a hex to the east. Inline, as hex distances are taken everywhere.
inline Axial toAxial(Hex h) { return Axial{h.x - floorHalf(h.y), h.y}; }
inline Hex toOffset(Axial a) { return Hex{a.q + floorHalf(a.r), a.r}; }
inline int axialDistance(Axial a, Axial b) {
    const int dq = a.q - b.q, dr = a.r - b.r;
    return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
}

// A step in each direction (NE, E, SE, SW, W, NW) in offset coordinates. The column step depends on the row's
// parity, odd rows sitting half a hex east: the axial steps (1,-1) (1,0) (0,1) (-1,1) (-1,0) (0,-1) carried
// through toAxial and toOffset.
inline constexpr int8_t kHexStepX[2][kNumDirs] = {{0, 1, 0, -1, -1, -1}, {1, 1, 1, 0, -1, 0}};
inline constexpr int8_t kHexStepY[kNumDirs] = {-1, 0, 1, 1, 0, -1};

class SOV_API HexGrid {
public:
    HexGrid() = default;
    HexGrid(int32_t width, int32_t height, bool wrapX) : w_(width), h_(height), wrap_(wrapX) {}

    int32_t width() const { return w_; }
    int32_t height() const { return h_; }
    bool wrapX() const { return wrap_; }
    int32_t size() const { return w_ * h_; }

    // Normalises x for wrapping; nullopt when off the map.
    std::optional<Hex> normalize(Hex h) const {
        if (h.y < 0 || h.y >= h_) return std::nullopt;
        if (h.x < 0 || h.x >= w_) {
            if (!wrap_) return std::nullopt;
            h.x = static_cast<int32_t>(((h.x % w_) + w_) % w_);
        }
        return h;
    }
    bool valid(Hex h) const { return normalize(h).has_value(); }
    int32_t index(Hex h) const { return h.y * w_ + h.x; }
    Hex at(int32_t index) const { return Hex{index % w_, index / w_}; }

    std::optional<Hex> neighbor(Hex h, Dir d) const {
        const int i = static_cast<int>(d);
        return normalize(Hex{h.x + kHexStepX[h.y & 1][i], h.y + kHexStepY[i]});
    }
    // Direction from a to an adjacent b, if adjacent.
    std::optional<Dir> directionTo(Hex a, Hex b) const;
    int distance(Hex a, Hex b) const {
        // The nearest of b and, on a wrapping map, its copies a map's width to the east and west (nearestAxial's choice).
        const Axial aa = toAxial(a), bb = toAxial(b);
        const int d = axialDistance(aa, bb);
        if (!wrap_ || d <= w_ - std::abs(bb.q - aa.q)) return d;  // no copy can be nearer (nearestAxial's bound)
        return std::min({d, axialDistance(aa, Axial{bb.q - w_, bb.r}), axialDistance(aa, Axial{bb.q + w_, bb.r})});
    }
    // All valid hexes with distance <= radius, in a fixed order.
    std::vector<Hex> within(Hex center, int radius) const;
    // The same hexes nearest first: the center, then each ring in within()'s order.
    std::vector<Hex> nearestFirst(Hex center, int radius) const;
    // Calls fn(hex) for each hex of within(center, radius), in the same order, without building the list.
    template <typename Fn>
    void forEachWithin(Hex center, int radius, Fn&& fn) const {
        if (wrap_ && w_ <= 2 * static_cast<int64_t>(radius)) {  // a row wraps onto itself: within() drops the repeats
            for (const Hex& h : within(center, radius)) fn(h);
            return;
        }
        walkWithin(center, radius, [&](Hex h, Axial) { fn(h); });
    }
    // The same, calling fn(hex, step) with the axial step from center to the copy of the hex that line(center, hex)
    // runs to: the hex itself, or on a wrapping map the copy a map's width east or west nearest the center.
    template <typename Fn>
    void forEachWithinStep(Hex center, int radius, Fn&& fn) const {
        if (wrap_ && w_ <= 2 * static_cast<int64_t>(radius)) {
            const Axial c = toAxial(center);
            for (const Hex& h : within(center, radius)) {
                const Axial b = nearestAxial(c, h);
                fn(h, Axial{b.q - c.q, b.r - c.r});
            }
            return;
        }
        // Each step of the walk is at most radius long, under half the map's width: the nearest copy is the one walked to.
        walkWithin(center, radius, fn);
    }
    // Hexes from a to b inclusive (cube line, deterministic tie-breaking).
    std::vector<Hex> line(Hex a, Hex b) const;
    // The hexes of line(a, b) strictly between a and b, in order and without building the list: calls fn(hex) for
    // each until it returns false, and then returns false (true when it ran to the end).
    template <typename Fn>
    bool between(Hex a, Hex b, Fn&& fn) const {
        const Axial aa = toAxial(a), bb = nearestAxial(aa, b);
        return betweenStep(a, Axial{bb.q - aa.q, bb.r - aa.r}, fn);
    }
    // between(a, b) given the axial step from a to the copy of b the line runs to (as forEachWithinStep gives it).
    template <typename Fn>
    bool betweenStep(Hex a, Axial step, Fn&& fn) const {
        const int n = axialDistance(Axial{}, step);
        if (n <= 1) return true;  // the same hex or a neighbour: nothing between
        if (const LineStep* s = lineSteps(step, n)) {  // a short line: its steps from a
            const int parity = a.y & 1;
            for (const LineStep* const end = s + (n - 1); s != end; ++s) {
                const std::optional<Hex> h = normalize(Hex{a.x + s->dx[parity], a.y + s->dy});
                if (h && !fn(*h)) return false;
            }
            return true;
        }
        const Axial aa = toAxial(a), bb{aa.q + step.q, aa.r + step.r};
        for (int i = 1; i < n; ++i) {
            const std::optional<Hex> h = linePoint(aa, bb, n, i);
            if (h && !fn(*h)) return false;
        }
        return true;
    }

private:
    // The hexes within radius of center row by row, north to south and west to east, to fn(hex, step) with the axial
    // step from center walked to reach it: on a map narrow enough for a row to wrap onto itself, some more than once.
    template <typename Fn>
    void walkWithin(Hex center, int radius, Fn&& fn) const {
        const Axial c = toAxial(center);
        for (int dr = -radius; dr <= radius; ++dr) {
            const int32_t y = c.r + dr;
            if (y < 0 || y >= h_) continue;  // off the top or bottom of the map
            const int qMin = std::max(-radius, -dr - radius);
            const int qMax = std::min(radius, -dr + radius);
            // Along a row the axial q and the offset column rise together, one plot a step.
            const int32_t first = toOffset(Axial{c.q + qMin, y}).x;
            for (int32_t x = first; x <= first + (qMax - qMin); ++x) {
                Hex h{x, y};
                if (x < 0 || x >= w_) {
                    if (!wrap_) continue;
                    h.x = static_cast<int32_t>(((x % w_) + w_) % w_);
                }
                fn(h, Axial{qMin + (x - first), dr});
            }
        }
    }
    // Axial position of b shifted by a multiple of the width to sit nearest aa.
    Axial nearestAxial(Axial aa, Hex b) const;
    // Point i of the n + 1 points of the line from aa to bb (n their distance); nullopt when off the map.
    std::optional<Hex> linePoint(Axial aa, Axial bb, int n, int i) const;
    // A point of a line as a step from its start: the column step from a start on an even row and on an odd row
    // (odd rows sit half a hex east), and the row step.
    struct LineStep {
        int8_t dx[2];
        int8_t dy;
    };
    static constexpr int kLineStepRange = 8;
    // The n - 1 points strictly inside a line of length n to an end delta away, as steps from its start, the same
    // points linePoint gives; null when n is over kLineStepRange.
    static const LineStep* lineSteps(Axial delta, int n);

    int32_t w_ = 0, h_ = 0;
    bool wrap_ = false;
};

}  // namespace sov
