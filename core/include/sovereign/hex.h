// Hex grid math. Pointy-top hexes, odd-row offset storage, axial/cube math,
// map wraps east-west (cylinder) when enabled; never north-south
// (specs/civ6/01-map-and-terrain.md, Grid).
#pragma once

#include "sovereign/api.h"

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
    std::optional<Hex> normalize(Hex h) const;
    bool valid(Hex h) const { return normalize(h).has_value(); }
    int32_t index(Hex h) const { return h.y * w_ + h.x; }
    Hex at(int32_t index) const { return Hex{index % w_, index / w_}; }

    std::optional<Hex> neighbor(Hex h, Dir d) const;
    // Direction from a to an adjacent b, if adjacent.
    std::optional<Dir> directionTo(Hex a, Hex b) const;
    int distance(Hex a, Hex b) const;
    // All valid hexes with distance <= radius, in a fixed order.
    std::vector<Hex> within(Hex center, int radius) const;
    // Hexes from a to b inclusive (cube line, deterministic tie-breaking).
    std::vector<Hex> line(Hex a, Hex b) const;

private:
    // Axial position of b shifted by a multiple of the width to sit nearest a.
    Axial nearestAxial(Hex a, Hex b) const;

    int32_t w_ = 0, h_ = 0;
    bool wrap_ = false;
};

}  // namespace sov
