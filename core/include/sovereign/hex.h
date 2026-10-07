// Hex grid math. Pointy-top hexes, odd-row offset storage, axial/cube math,
// map wraps east-west (cylinder) when enabled; never north-south
// (specs/civ6/01-map-and-terrain.md, Grid).
#pragma once

#include "sovereign/api.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

SOV_API Axial toAxial(Hex h);
SOV_API Hex toOffset(Axial a);
SOV_API int axialDistance(Axial a, Axial b);

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

    std::optional<Hex> neighbor(Hex h, Dir d) const;
    // Direction from a to an adjacent b, if adjacent.
    std::optional<Dir> directionTo(Hex a, Hex b) const;
    int distance(Hex a, Hex b) const;
    // All valid hexes with distance <= radius, in a fixed order.
    std::vector<Hex> within(Hex center, int radius) const;
    // Calls fn(hex) for each hex of within(center, radius), in the same order, without building the list.
    template <typename Fn>
    void forEachWithin(Hex center, int radius, Fn&& fn) const {
        if (wrap_ && w_ <= 2 * static_cast<int64_t>(radius)) {  // a row wraps onto itself: within() drops the repeats
            for (const Hex& h : within(center, radius)) fn(h);
            return;
        }
        walkWithin(center, radius, fn);
    }
    // Hexes from a to b inclusive (cube line, deterministic tie-breaking).
    std::vector<Hex> line(Hex a, Hex b) const;
    // The hexes of line(a, b) strictly between a and b, in order and without building the list: calls fn(hex) for
    // each until it returns false, and then returns false (true when it ran to the end).
    template <typename Fn>
    bool between(Hex a, Hex b, Fn&& fn) const {
        const Axial aa = toAxial(a), bb = nearestAxial(aa, b);
        const int n = axialDistance(aa, bb);
        if (const LineStep* step = lineSteps(Axial{bb.q - aa.q, bb.r - aa.r}, n)) {  // a short line: its steps from a
            const int parity = a.y & 1;
            for (const LineStep* const end = step + (n > 1 ? n - 1 : 0); step != end; ++step) {
                const std::optional<Hex> h = normalize(Hex{a.x + step->dx[parity], a.y + step->dy});
                if (h && !fn(*h)) return false;
            }
            return true;
        }
        for (int i = 1; i < n; ++i) {
            const std::optional<Hex> h = linePoint(aa, bb, n, i);
            if (h && !fn(*h)) return false;
        }
        return true;
    }

private:
    // The hexes within radius of center row by row, north to south and west to east, to fn: on a map narrow enough
    // for a row to wrap onto itself, some more than once.
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
                fn(h);
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
