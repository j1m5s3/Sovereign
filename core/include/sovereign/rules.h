// Rules data: every unit, terrain, resource, civ and tunable constant is loaded
// from JSON files under data/rules/, never hard-coded (engine doc, "Rules are
// data"). Later directories override earlier ones row by row (by "id"), which
// is how mods change rules.
#pragma once

#include <array>
#include <climits>
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

// What unlocks a unit, building, resource, government or policy: a tech or a
// civic (04-tech-civics-government.md). An empty unlock is available from the start.
struct Unlock {
    bool civic = false;
    TypeIndex index = kNone;
    bool none() const { return index == kNone; }
};

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
    Unlock removeTech;  // tech a Builder needs to harvest it
    Yields harvest{};   // one-time yields from harvesting (base, Standard speed)
    std::vector<TypeIndex> validTerrains;
};

struct ResourceType {
    std::string id, name;
    ResourceClass cls = ResourceClass::Bonus;
    Yields yields{};
    Unlock reveal;  // tech that reveals it; none: always visible
    int frequency = 0;       // land placement weight
    int seaFrequency = 0;    // water placement weight
    std::vector<TypeIndex> validTerrains;
    std::vector<TypeIndex> validFeatures;
    int amenityCities = 0;   // luxury: +1 amenity to this many cities
    Yields harvest{};        // bonus: one-time yields from harvesting it
    Unlock harvestTech;      // none with empty harvest: cannot be harvested
    int accumulation = 0;    // strategic: added to the stockpile per improved source per turn
    int stockpileCap = 0;    // strategic: stockpile cap
};

// Unit promotions and abilities (05-units-and-combat.md; data: promotions.md,
// units.md "Unit abilities"). Effects carry their conditions as data.
enum class UnitEffectKind : uint8_t {
    Untracked = 0,      // not modelled yet
    Strength,           // +amount combat strength when `when` holds
    Moves,
    Range,
    Sight,
    Attacks,            // extra attacks per turn
    XpPercent,
    FlankingPercent,    // extra percent of the flanking bonus
    SupportPercent,
    MoveAfterAttack,
    AttackAfterMove,    // cancels NoAttackAfterMove
    NoAttackAfterMove,  // siege: cannot attack once it has moved this turn
    IgnoreZoc,
    ExertZoc,
    NoRiverPenalty,
    NoWoundedPenalty,
    HealAfterAction,
    IgnoreBorders,
    RangedVsDistrict,   // COMBAT_RANGED_VS_DISTRICT_STRENGTH_MODIFIER applies
    BombardVsUnit,      // COMBAT_BOMBARD_VS_UNIT_STRENGTH_MODIFIER applies
};

enum class CombatAtom : uint8_t {
    Untracked = 0,  // never holds
    Attacking,
    VsClass,        // opponent unit class
    VsDomain,
    VsDistrict,     // opponent is a city or district
    CombatType,     // melee or ranged
    TileHills,      // this unit's plot
    TileFeature,
    TileTerrain,
    OpponentFortified,
    OpponentWounded,
    DistrictTile,
    OwnTerritory,
};

struct CombatCondition {
    CombatAtom atom = CombatAtom::Untracked;
    bool negate = false;
    std::string value;      // class id for VsClass
    TypeIndex ref = kNone;  // feature or terrain
    int arg = 0;            // Domain for VsDomain; 0 melee / 1 ranged for CombatType
};

struct UnitEffect {
    UnitEffectKind kind = UnitEffectKind::Untracked;
    int amount = 0;
    std::vector<std::vector<CombatCondition>> when;  // every group needs any one condition
};

struct AbilityType {
    std::string id, name;
    std::vector<std::string> classes;  // unit classes it applies to when granted
    bool inactive = false;             // needs a grant (policy, government, building...)
    std::vector<UnitEffect> effects;
};

struct PromotionType {
    std::string id, name;
    std::string promotionClass;
    int tier = 1;
    std::vector<TypeIndex> prereqs;  // any one
    std::vector<UnitEffect> effects;
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
    int costProgression = 0;  // PREVIOUS_COPIES: extra cost per copy already trained
    int popCost = 0;          // population removed when trained (Settler)
    int minPopulation = 0;    // city population needed to train
    bool mustPurchase = false;
    std::string purchaseYield;  // "GOLD", "FAITH" or empty
    Unlock unlock;
    int era = 0;  // era index of its unlock (Ancient when it has none)
    TypeIndex strategicResource = kNone;  // resource spent to train it
    int strategicCost = 0;
    TypeIndex upgradesTo = kNone;  // can no longer be trained once this one can
    Unlock obsoleteWith;           // can no longer be trained once this is known
    int bombard = 0;
    int resourceMaintenance = 0;   // strategicResource spent per turn [GS]
    std::string promotionClass;    // empty: no promotions
    std::vector<TypeIndex> abilities;  // innate
    TypeIndex capturedAs = kNone;  // civilian captured by an enemy becomes this (kNone: destroyed)
};

// Tile improvements built by Builders (02-cities.md, 01-map-and-terrain.md; data: improvements.md).
struct ImprovementBonus {
    YieldType yield = YieldType::Food;
    Fixed amount;
    Unlock unlock;
};

struct ImprovementAdjacency {
    YieldType yield = YieldType::Food;
    Fixed amount;
    int per = 1;                     // amount per this many adjacent improvements
    TypeIndex improvement = kNone;
    Unlock needs, obsoleteWith;
};

struct ImprovementType {
    std::string id, name;
    Unlock unlock;
    Yields yields{};
    std::vector<TypeIndex> validTerrains, validFeatures, validResources;
    std::vector<ImprovementBonus> bonuses;
    std::vector<ImprovementAdjacency> adjacency;
    Fixed housing;  // per improved plot the city owns
};

struct BuildingType {
    std::string id, name;
    std::string district;   // e.g. "DISTRICT_CITY_CENTER"
    Unlock unlock;
    int cost = 0;
    int maintenance = 0;
    Yields yields{};
    Fixed housing;
    int amenities = 0;
    int outerDefenseHp = 0;
    int defense = 0;
    std::vector<TypeIndex> prereqs;  // buildings needed first
    bool needsRiver = false;
    bool purchasable = false;
    bool granted = false;  // given by the rules (Palace), never built
};

// Amenity balance bands (eras-moments-loyalty.md, Amenities).
struct HappinessLevel {
    std::string id;
    int minBalance = 0;  // INT32_MIN for the lowest band
    int growthPercent = 0;
    int yieldPercent = 0;  // non-food yields
};

// Research trees (04-tech-civics-government.md).
struct EraType {
    std::string id, name;
};

// What earns a boost. Conditions the core cannot track yet load as NotTracked
// and never fire until the system they need exists.
enum class BoostKind : uint8_t {
    None = 0,
    CoastalCity,      // own a city next to coast or lake
    Building,         // `count` of your cities have building `ref`
    OwnUnits,         // own `count` units of type `ref`
    Tech,             // tech `ref` researched
    Civic,            // civic `ref` completed
    GovernmentTier,   // a government of tier `count` or higher
    TotalPopulation,  // `count` citizens across your cities
    CityPopulation,   // one city of `count` population
    LandCombatUnits,  // `count` land military units
    Improvement,             // `count` plots with improvement `ref`
    ImprovementOnResource,   // `count` plots with improvement `ref` on a resource it improves
    ImproveResource,         // a plot with resource `ref` improved
    ImprovedTiles,           // `count` improved plots
    NotTracked,       // districts, improvements, combat, religion... (later milestones)
};

struct Boost {
    int percent = 0;  // 0: the node has no boost
    BoostKind kind = BoostKind::None;
    TypeIndex ref = kNone;
    int count = 1;
    std::string type, text;  // as in the data
};

struct TreeNode {
    std::string id, name;
    int era = 0;
    int cost = 0;  // Standard speed
    std::vector<TypeIndex> prereqs;
    Boost boost;
    bool combatAdjacency = false;  // enables flanking and support bonuses
    bool enforceBorders = false;   // closes the player's borders to units not at war
};

enum class PolicySlot : uint8_t { Military = 0, Economic, Diplomatic, Wildcard, GreatPerson };
constexpr size_t kNumGovernmentSlotTypes = 4;  // Military, Economic, Diplomatic, Wildcard

struct GovernmentType {
    std::string id, name;
    int tier = 0;
    Unlock unlock;
    std::array<int, kNumGovernmentSlotTypes> slots{};
    int totalSlots() const { return slots[0] + slots[1] + slots[2] + slots[3]; }
};

struct PolicyType {
    std::string id, name;
    PolicySlot slot = PolicySlot::Military;
    Unlock unlock;                        // none: not adoptable yet (legacy and Dark Age cards)
    std::vector<TypeIndex> obsoletedBy;   // replacement cards: once one is unlocked this card retires
    TypeIndex government = kNone;         // only under this government
};

enum class ModCollection : uint8_t { OwnerCity = 0, OwnerCityPlots, PlayerCities, PlayerCapital, PlayerCityPlots, Player };
enum class ModEffect : uint8_t {
    CityYield = 0,      // flat yield on a city
    CityYieldPercent,   // percentage on a city's yield
    PlotYield,          // flat yield on a worked plot
    CityHousing,
    CityAmenities,
    CityGrowthPercent,
    CityDefense,
    UnitProductionPercent,    // production toward matching units in a city
    PlotPurchaseCostPercent,  // gold cost of buying plots for a city
    UnitMaintenanceDiscount,  // player: gold off each unit's maintenance
    GrantAbility,             // player: matching units gain `ability`
    UnitXpPercent,            // player: combat XP bonus for units of `unitClass` (empty: all)
};
enum class ReqType : uint8_t {
    PlotHasResource = 0,
    PlotHasFeature,
    PlotHasTerrain,
    CityHasBuilding,
    CityIsCapital,
    CityMinPopulation,
    PlayerIsHuman,
};

struct Requirement {
    ReqType type = ReqType::PlayerIsHuman;
    TypeIndex ref = kNone;  // resource/feature/terrain/building index
    int value = 0;
    bool negate = false;
};

struct RequirementSet {
    bool any = false;  // false: all must hold
    std::vector<Requirement> reqs;
};

// Civ VI's modifier model (00-overview.md, Architecture recommendations):
// who it affects (collection), what it does (effect), when (requirements).
enum class ModSource : uint8_t { Building = 0, Civ, Everyone, Policy, Government };

struct Modifier {
    std::string id;
    std::string source;  // id of the building, civ, policy... that carries it ("EVERYONE": all players)
    ModSource sourceKind = ModSource::Everyone;
    TypeIndex sourceIndex = kNone;
    ModCollection collection = ModCollection::OwnerCity;
    ModEffect effect = ModEffect::CityYield;
    RequirementSet ownerReqs, subjectReqs;
    YieldType yield = YieldType::Food;
    Fixed amount;
    // UnitProductionPercent filters (empty/none/-1: any).
    std::string unitClass;
    TypeIndex unit = kNone;
    int maxEra = -1;
    TypeIndex ability = kNone;  // GrantAbility
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
    std::vector<AbilityType> abilities;
    std::vector<PromotionType> promotions;
    std::vector<UnitType> units;
    std::vector<BuildingType> buildings;
    std::vector<ImprovementType> improvements;
    std::vector<EraType> eras;
    std::vector<TreeNode> techs;
    std::vector<TreeNode> civics;
    std::vector<GovernmentType> governments;
    std::vector<PolicyType> policies;
    std::vector<HappinessLevel> happiness;  // ascending by minBalance
    std::vector<Modifier> modifiers;
    std::vector<CivType> civs;
    std::vector<MapSizeType> mapSizes;
    std::vector<GameSpeedType> speeds;
    std::vector<std::string> startingUnits;  // unit ids every major civ starts with

    TypeIndex terrain(const std::string& id) const;
    TypeIndex feature(const std::string& id) const;
    TypeIndex resource(const std::string& id) const;
    TypeIndex ability(const std::string& id) const;
    TypeIndex promotion(const std::string& id) const;
    TypeIndex unit(const std::string& id) const;
    TypeIndex building(const std::string& id) const;
    TypeIndex improvement(const std::string& id) const;
    TypeIndex era(const std::string& id) const;
    TypeIndex tech(const std::string& id) const;
    TypeIndex civic(const std::string& id) const;
    TypeIndex government(const std::string& id) const;
    TypeIndex policy(const std::string& id) const;
    // Modifiers whose source is this id, in load order.
    std::vector<const Modifier*> modifiersFrom(const std::string& source) const;
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
