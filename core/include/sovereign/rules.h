// Rules data: every unit, terrain, resource, civ and tunable constant is loaded
// from JSON files under data/rules/, never hard-coded (engine doc, "Rules are
// data"). Later directories override earlier ones row by row (by "id"), which
// is how mods change rules.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "sovereign/fixed.h"

namespace sov {

enum class YieldType : uint8_t { Food = 0, Production, Gold, Science, Culture, Faith, Count };
constexpr size_t kNumYields = static_cast<size_t>(YieldType::Count);
using Yields = std::array<Fixed, kNumYields>;

const char* yieldName(YieldType y);
bool parseYieldName(const std::string& s, YieldType& out);

enum class Relief : uint8_t { Flat = 0, Hills, Mountain };
enum class Domain : uint8_t { Land = 0, Sea, Air };
// 1UPT layers (05-units-and-combat.md, Stacking).
enum class UnitLayer : uint8_t { Military = 0, Civilian, Support };
enum class ResourceClass : uint8_t { Bonus = 0, Luxury, Strategic };

using TypeIndex = int16_t;
constexpr TypeIndex kNone = -1;

struct TerrainType {
    std::string id, name;
    std::string base;  // climate family, e.g. "GRASSLAND"; map gen picks by (base, relief)
    Relief relief = Relief::Flat;
    Yields yields{};
    int moveCost = 1;
    int defense = 0;
    int appeal = 0;
    int sightModifier = 0;  // added to the sight of a unit standing here
    int sightThrough = 0;   // obstacle height for line of sight
    bool impassable = false;
    bool water = false;
    bool shallowWater = false;
};

struct FeatureType {
    std::string id, name;
    Yields yields{};
    int moveChange = 0;
    int defense = 0;
    int appeal = 0;
    int sightThrough = 0;
    bool impassable = false;
    bool freshWater = false;
    bool removable = false;
    std::vector<TypeIndex> validTerrains;
};

struct ResourceType {
    std::string id, name;
    ResourceClass cls = ResourceClass::Bonus;
    Yields yields{};
    std::string revealTech;  // empty: always visible
    int frequency = 0;       // land placement weight
    int seaFrequency = 0;    // water placement weight
    std::vector<TypeIndex> validTerrains;
    std::vector<TypeIndex> validFeatures;
};

struct UnitType {
    std::string id, name;
    std::string unitClass;  // e.g. "MELEE", "RECON", "CIVILIAN"
    Domain domain = Domain::Land;
    UnitLayer layer = UnitLayer::Military;
    int cost = 0;
    int maintenance = 0;
    int combat = 0;
    int ranged = 0;
    int range = 0;
    int moves = 2;
    int sight = 2;
    bool zoneOfControl = false;
    bool foundCity = false;
    int buildCharges = 0;
};

struct CivType {
    std::string id, name, leader;
    std::vector<std::string> cityNames;
};

struct MapSizeType {
    std::string id;
    int width = 0, height = 0;
    int defaultPlayers = 0;
};

struct GameSpeedType {
    std::string id;
    int costPercent = 100;
    int turns = 500;
};

class Rules {
public:
    // Loads every rules file from each directory in order; rows in later
    // directories replace rows with the same id. Returns false with a message.
    bool load(const std::vector<std::string>& dirs, std::string* error);
    // Loads from already-read file contents (filename -> text per layer).
    bool loadFromText(const std::vector<std::map<std::string, std::string>>& layers, std::string* error);

    std::vector<TerrainType> terrains;
    std::vector<FeatureType> features;
    std::vector<ResourceType> resources;
    std::vector<UnitType> units;
    std::vector<CivType> civs;
    std::vector<MapSizeType> mapSizes;
    std::vector<GameSpeedType> speeds;
    std::vector<std::string> startingUnits;  // unit ids every major civ starts with

    TypeIndex terrain(const std::string& id) const;
    TypeIndex feature(const std::string& id) const;
    TypeIndex resource(const std::string& id) const;
    TypeIndex unit(const std::string& id) const;
    TypeIndex civ(const std::string& id) const;
    TypeIndex mapSize(const std::string& id) const;
    TypeIndex speed(const std::string& id) const;
    // Terrain with this climate base and relief, or kNone.
    TypeIndex terrainFor(const std::string& base, Relief relief) const;

    // Named constants (GlobalParameters-style). Missing names are a load error
    // when required through requireGlobals().
    Fixed global(const std::string& name) const;
    int globalInt(const std::string& name) const { return static_cast<int>(global(name).toInt()); }
    bool hasGlobal(const std::string& name) const { return globals_.count(name) != 0; }

    // Checksum of the loaded rules; saves and multiplayer peers must match.
    uint64_t checksum() const { return checksum_; }

    // The names of the rules files the loader reads, in load order.
    static const std::vector<std::string>& fileNames();

private:
    std::map<std::string, Fixed> globals_;
    uint64_t checksum_ = 0;
};

uint64_t fnv1a(const void* data, size_t size, uint64_t h = 0xCBF29CE484222325ull);

}  // namespace sov
