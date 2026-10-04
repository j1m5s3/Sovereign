// Complete game state. Everything the rules read lives here and is saved;
// nothing outside it may influence a rules decision.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sovereign/fixed.h"
#include "sovereign/hex.h"
#include "sovereign/rng.h"
#include "sovereign/rules.h"

namespace sov {

using PlayerId = int8_t;
using UnitId = int32_t;
using CityId = int32_t;
constexpr PlayerId kNoPlayer = -1;
constexpr UnitId kNoUnit = -1;
constexpr CityId kNoCity = -1;

// River flags stored on the three edges a plot owns (E, SE, SW); the other
// three belong to the neighbour on that side.
enum RiverEdge : uint8_t { kRiverE = 1, kRiverSE = 2, kRiverSW = 4 };

struct Plot {
    TypeIndex terrain = 0;
    TypeIndex feature = kNone;
    TypeIndex resource = kNone;
    TypeIndex improvement = kNone;
    uint8_t resourceAmount = 0;
    uint8_t riverEdges = 0;
    PlayerId owner = kNoPlayer;
    CityId city = kNoCity;  // owning city
    int16_t continent = -1;
};

enum class Activity : uint8_t { Awake = 0, Sleep, Fortify, Skip };

struct Unit {
    UnitId id = kNoUnit;
    TypeIndex type = kNone;
    PlayerId owner = kNoPlayer;
    Hex pos;
    int hp = 100;
    Fixed movesLeft;
    Activity activity = Activity::Awake;
    std::optional<Hex> moveTarget;  // multi-turn move order
    int xp = 0;
    int charges = 0;  // build charges left (Builders)
};

enum class ProductionKind : uint8_t { Unit = 0, Building = 1 };

struct ProductionItem {
    ProductionKind kind = ProductionKind::Unit;
    TypeIndex type = kNone;
    bool operator==(const ProductionItem& o) const { return kind == o.kind && type == o.type; }
};

// Production already put into an item; kept when the player switches away.
struct ProductionProgress {
    ProductionItem item;
    Fixed amount;
};

struct City {
    CityId id = kNoCity;
    PlayerId owner = kNoPlayer;
    std::string name;
    Hex pos;
    int population = 1;
    int foundedTurn = 0;
    bool capital = false;
    Fixed food;            // growth bucket
    Fixed borderCulture;   // border growth bucket
    int plotsByCulture = 0;  // plots acquired by border growth so far
    Fixed overflow;        // production carried into the next item
    std::vector<TypeIndex> buildings;  // sorted
    std::vector<ProductionItem> queue;
    std::vector<ProductionProgress> progress;
    std::vector<int32_t> worked;  // plot indices worked by citizens (center excluded), sorted
    std::vector<int32_t> locked;  // plot indices the player pinned a citizen to, sorted

    bool has(TypeIndex building) const;
};

// A player's progress through one research tree (techs or civics). Progress
// is kept per node, so switching away loses nothing (04-tech-civics-government.md).
struct TreeProgress {
    std::vector<uint8_t> done;      // per node
    std::vector<uint8_t> boosted;   // per node: boost already earned
    std::vector<Fixed> progress;    // per node
    TypeIndex current = kNone;      // node being researched
    Fixed overflow;                 // carried into the next node

    bool has(TypeIndex node) const {
        return node >= 0 && static_cast<size_t>(node) < done.size() && done[static_cast<size_t>(node)] != 0;
    }
    void resize(size_t n) {
        done.resize(n, 0);
        boosted.resize(n, 0);
        progress.resize(n);
    }
};

// Per-player knowledge of each plot (01-map-and-terrain.md, Visibility).
enum class Visibility : uint8_t { Unrevealed = 0, Revealed = 1, Visible = 2 };

struct Player {
    PlayerId id = kNoPlayer;
    TypeIndex civ = kNone;
    bool human = false;
    bool alive = true;
    int citiesFounded = 0;  // drives city naming
    Fixed gold;
    Fixed faith;  // lifetime total until religion arrives
    TreeProgress techs, civics;
    TypeIndex government = kNone;
    std::vector<TypeIndex> policies;   // one entry per slot of the government (kNone: empty)
    std::vector<int> governmentUses;   // per government: times adopted
    int anarchyTurns = 0;
    bool freeChanges = false;          // government and policies may change this turn
    std::vector<int> unitsTrained;  // per unit type, for PREVIOUS_COPIES cost progression
    std::vector<int> stockpile;     // per resource: strategic stockpile [GS]
    std::vector<uint8_t> visibility;  // Visibility per plot index
    Hex startPos;
};

struct PlayerSetup {
    std::string civ;
    bool human = false;
};

struct GameSetup {
    uint64_t seed = 1;
    std::string mapSize = "MAPSIZE_DUEL";
    std::string speed = "GAMESPEED_STANDARD";
    bool wrapX = true;
    std::vector<PlayerSetup> players;
};

struct GameState {
    GameSetup setup;
    int turn = 1;
    PlayerId currentPlayer = 0;
    HexGrid grid;
    std::vector<Plot> plots;
    std::vector<Player> players;
    std::vector<Unit> units;    // sorted by id
    std::vector<City> cities;   // sorted by id
    UnitId nextUnitId = 1;
    CityId nextCityId = 1;
    RngSet rng;

    Plot& plot(Hex h) { return plots[static_cast<size_t>(grid.index(h))]; }
    const Plot& plot(Hex h) const { return plots[static_cast<size_t>(grid.index(h))]; }

    Unit* unit(UnitId id);
    const Unit* unit(UnitId id) const;
    City* city(CityId id);
    const City* city(CityId id) const;
    const City* cityAt(Hex h) const;
    // Unit on this plot in the given layer, if any.
    const Unit* unitAt(Hex h, UnitLayer layer, const Rules& rules) const;
    // Any unit on the plot not owned by this player.
    const Unit* foreignUnitAt(Hex h, PlayerId player) const;
};

}  // namespace sov
