// Rules data: every unit, terrain, resource, civ and tunable constant is loaded
// from JSON files under data/rules/, never hard-coded (engine doc, "Rules are
// data"). Later directories override earlier ones row by row (by "id"), which
// is how mods change rules.
#pragma once

#include "sovereign/api.h"

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

SOV_API const char* yieldName(YieldType y);
SOV_API bool parseYieldName(const std::string& s, YieldType& out);

enum class Relief : uint8_t { Flat = 0, Hills, Mountain };
enum class Domain : uint8_t { Land = 0, Sea, Air };
// 1UPT layers (05-units-and-combat.md, Stacking). The leader has its own layer so it can
// share a plot with one military escort and one civilian (leader doc §1).
enum class UnitLayer : uint8_t { Military = 0, Civilian, Support, Leader, Air };  // Air: aircraft, based, never stacked
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
    WallFullDamage,     // support: adjacent friendly melee deal full damage to walls (Battering Ram)
    BypassWalls,        // support: adjacent friendly melee hit the city past its walls (Siege Tower)
    // The leader's SOVEREIGN promotions (leader doc §3).
    AuraStrength,       // +amount to the presence aura
    AssassinDefense,    // +amount to the leader's defence against assassins
    CityProduction,     // +amount production in the city the leader stands in
    CityAmenities,      // +amount amenities in the city the leader stands in
    // Civ uniques (leaders-and-art-style: Civ abilities, uniques and dynasties).
    HealOnKill,         // +amount HP when it destroys a unit
    MeleeAndRanged,     // a ranged unit that may also attack in melee
    CaptureAsBuilder,   // a land unit it destroys joins its owner as a Builder
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
    AdjacentSameUnit,   // a friendly unit of the same type stands next to it
    OpponentTileBase,   // the opponent stands on terrain of this climate (value: e.g. "DESERT")
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
    std::string branch;  // SOVEREIGN promotions: only one branch's tier-2 promotion per reign
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
    int antiAir = 0;  // strength against aircraft striking an adjacent plot (05: air combat)
    int airSlots = 0; // aircraft it carries (Aircraft Carrier)
    bool deliversWmd = false;  // bombers and the Nuclear Submarine (05: Nuclear weapons)
    bool wmdImmune = false;    // the Giant Death Robot
    int moves = 2;
    int sight = 2;
    bool zoneOfControl = false;
    bool foundCity = false;
    int buildCharges = 0;
    int costProgression = 0;  // PREVIOUS_COPIES: extra cost per copy already trained
    int popCost = 0;          // population removed when trained (Settler)
    int minPopulation = 0;    // city population needed to train
    bool mustPurchase = false;
    bool trainable = true;  // false: never in a city queue (Great People, spies)
    std::vector<TypeIndex> needsBuilding;  // the city must have one of these (empty: none)
    std::string purchaseYield;  // "GOLD", "FAITH" or empty
    // Religious units (06): theological strength, spread charges, the share of other
    // religions a spread removes, heal charges; the Great Prophet founds a religion.
    int religiousStrength = 0;
    int spreadCharges = 0;
    int evictPercent = 0;
    int healCharges = 0;
    bool foundReligion = false;
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
    // A civ's unique unit: only that civ trains it, and for that civ it replaces `replaces`.
    TypeIndex uniqueTo = kNone;
    TypeIndex replaces = kNone;
    std::string uniqueToId;            // (loading only)
    TypeIndex capturedAs = kNone;  // civilian captured by an enemy becomes this (kNone: destroyed)
    bool agent = false;            // training it creates an off-map agent (assassins), not a map unit
    bool spy = false;              // the agent is a spy (within the spy capacity civics grant)
    TypeIndex needsDistrict = kNone;  // the training city must have this district finished
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
    int appeal = 0;  // to neighbouring plots (01: Appeal)
    // Civ unique improvements (leaders-and-art-style).
    TypeIndex uniqueTo = kNone;
    std::string uniqueToId;  // (loading only)
    int amenities = 0;       // to its city
    int defense = 0;         // combat strength for units defending on it
    int sight = 0;           // extra sight for units on it
    bool borderOnly = false; // only on plots at the edge of the owner's territory
    bool needsRiver = false;
    bool halvesFloods = false;  // flood damage on adjacent plots halved
    TypeIndex adjacentImprovement = kNone;  // gives `adjacentYield` to adjacent improvements of this type
    YieldType adjacentYield = YieldType::Food;
    int adjacentAmount = 0;
    std::string adjacentImprovementId;  // (loading only)
    int powerProvided = 0;   // free power to its city (renewables, 09: Power)
    TypeIndex builtBy = kNone;  // only this unit builds it (Military Engineer); kNone: Builders
    std::string builtById;      // (loading only)
    int airSlots = 0;           // aircraft it bases (Airstrip)
};

// One-time effects of great people and wonders (07: Great People; 03: Wonders).
enum class GreatPersonEffectKind : uint8_t {
    Yield = 0,          // one-time gold, faith, science or culture
    Production,         // one-time production toward the city's current item
    Boost,              // a Eureka or Inspiration (or the whole node if already boosted)
    RandomBoost,        // `count` random boosts of a tree between two eras
    PromotionXp,        // a military unit here gains enough XP for a promotion
    Building,           // the building, free, in this city
    Unit,               // a free unit here
    BuildingYield,      // permanent: + yield from a building in all the player's cities
    GreatPersonPoints,  // points toward every class
};

struct GreatPersonEffect {
    GreatPersonEffectKind kind = GreatPersonEffectKind::Yield;
    YieldType yield = YieldType::Gold;
    int amount = 0;
    bool scaled = false;      // scales with game speed
    bool civic = false;       // Boost / RandomBoost: the civic tree
    bool orComplete = false;  // Boost: completes the node if it is already boosted
    TypeIndex ref = kNone;    // tech, civic, building or unit
    int count = 0;
    int minEra = 0, maxEra = 0;
};

// Where a wonder may stand (03: Wonders; data: wonders.md, Placement).
struct WonderPlacement {
    std::vector<TypeIndex> terrains;  // the plot is one of these (empty: any land)
    bool mountain = false;            // ...or a mountain
    std::vector<TypeIndex> features;  // ...or carries one of these
    std::vector<TypeIndex> needsFeature;  // the plot must carry one of these
    bool river = false, coastal = false, lake = false, notLake = false;
    bool nextToLand = false, nextToCapital = false, nextToMountain = false, nextToCityCenter = false;
    TypeIndex nextToDistrict = kNone, nextToResource = kNone, nextToImprovement = kNone;
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
    int airSlots = 0;  // aircraft its district can base (Hangar, Airport)
    // A civ's unique building (leaders-and-art-style): only that civ builds it; for it, it replaces `replaces`.
    TypeIndex uniqueTo = kNone, replaces = kNone;
    std::string uniqueToId, replacesId;  // (loading only)
    // Unique effects: a yield on adjacent improvements of a type (next to its district), gold per
    // trade route from the city, envoys when built, XP for units trained in the city (% of the first
    // promotion), food per mountain next to the city (at most 2).
    TypeIndex adjacentImprovement = kNone;
    YieldType adjacentYield = YieldType::Production;
    int adjacentAmount = 0;
    std::string adjacentImprovementId;   // (loading only)
    int goldPerTradeRoute = 0, envoysOnBuild = 0, trainedXpPercent = 0, foodPerAdjacentMountain = 0;
    // Power [GS] (09: Power).
    int requiredPower = 0;             // power it needs to work fully
    Yields poweredYields{};            // extra yields while its city is fully powered
    int poweredAmenities = 0;
    TypeIndex burnsResource = kNone;   // a power plant: burns this, `powerPerResource` power each
    int powerPerResource = 0;
    int powerProvided = 0;             // free power to its city (Hydroelectric Dam)
    int defense = 0;
    std::vector<TypeIndex> prereqs;  // buildings needed first
    bool needsRiver = false;
    bool purchasable = false;
    bool granted = false;  // given by the rules (Palace), never built
    bool faithOnly = false;  // a worship building: bought with Faith by a religion holding its belief
    TypeIndex districtType = kNone;  // Rules::districts; kNone while its district is not modelled
    bool meleeCannotDamageWalls = false;
    bool wallsCannotBeBypassed = false;
    std::vector<std::pair<TypeIndex, int>> greatPersonPoints;  // (great person class, points per turn)
    std::vector<std::pair<std::string, int>> greatWorkSlots;   // (slot type, count): "WRITING", "ART", ...
    int tradeCapacity = 0;                 // + trade route capacity
    TypeIndex tradeCapacityUnless = kNone; // ...unless the city has this building (Lighthouse: a Market)
    // World wonders (03: Wonders): built once in the world, on a plot of their own.
    bool wonder = false;
    WonderPlacement placement;
    std::vector<GreatPersonEffect> wonderEffects;  // one-time effects on completion
    std::string text;                              // the full effect text, for players
};

// Religion (06-religion.md; data: religion.md).
enum class BeliefClass : uint8_t { Pantheon = 0, Follower, Worship, Founder, Enhancer };
constexpr int kNumBeliefClasses = 5;

struct BeliefType {
    std::string id, name, text;
    BeliefClass cls = BeliefClass::Pantheon;
    TypeIndex worshipBuilding = kNone;  // Worship: the building it unlocks
    TypeIndex grantUnit = kNone;        // Pantheon: a unit given in the capital when chosen
};

struct ReligionType {
    std::string id, name;
};

// Great people (07-economy-trade-great-people.md, Great People; data: great-people.md).
struct GreatPersonClass {
    std::string id, name;
    TypeIndex unit = kNone;      // the unit a recruited great person is
    TypeIndex district = kNone;  // the district that earns its points
    int maxPerPlayer = 0;        // 0: no limit (Prophets: 1)
};

struct GreatPersonAura {
    Domain domain = Domain::Land;
    int strength = 0;
    int moves = 0;
    int range = 0;
    std::vector<int> eras;  // unit eras it helps
};

struct GreatPersonType {
    std::string id, name;
    TypeIndex cls = kNone;
    int era = 0;
    int charges = 0;
    // Activation requirements; ones the core cannot check yet are ignored.
    bool ownedTile = false;
    TypeIndex district = kNone;  // must stand on a finished district of this type (City Center: the city plot)
    bool noMilitaryUnit = false;
    int unitDomain = -1;         // a military unit of this Domain must share the plot
    TypeIndex missingBuilding = kNone;
    std::vector<GreatPersonEffect> effects;
    std::vector<std::string> untrackedEffects;  // effects of systems not built yet (shown, not applied)
    TypeIndex greatWorkType = kNone;
    int greatWorkCount = 0;
    bool hasAura = false;
    GreatPersonAura aura;
};

struct GreatWorkType {
    std::string id;
    YieldType yield = YieldType::Culture;
    int amount = 0;
    int tourism = 0;
    std::vector<std::string> slots;  // slot types that accept it
};

// Barbarian tribes (barbarians-goody-huts.md): which units a camp releases and how bold it is.
struct BarbarianTribe {
    std::string id;
    bool coastal = false;
    TypeIndex resource = kNone;  // tribe chosen when this resource is within resourceRange
    int resourceRange = 0;
    int rangedPercent = 0;
    int spawnTurns = 0;
    int raidBoldness = 0;
    int attackBoldness = 0;
    std::string unitClass;  // class of the melee units its camps release
};

// One adjacency row of a district (03-districts-buildings-wonders.md, Adjacency bonuses).
enum class DistrictAdjacencyKind : uint8_t { Mountain = 0, River, AnyDistrict, District, Feature, Improvement, StrategicResource, SeaResource };
struct DistrictAdjacency {
    YieldType yield = YieldType::Food;
    int amount = 0;
    int tilesRequired = 1;  // "per 2" rows: amount per this many matching neighbours, floored
    DistrictAdjacencyKind kind = DistrictAdjacencyKind::Mountain;
    TypeIndex ref = kNone;  // district, feature or improvement
};

enum class DistrictCostProgression : uint8_t { None = 0, NumUnderAvgPlusTech, GameProgress };

// Districts (districts.md). The City Center is the city itself; the others are placed.
struct DistrictType {
    std::string id, name;
    int hp = 0;
    int attackRange = 0;
    Unlock unlock;
    int cost = 0;  // base production cost at Standard speed
    DistrictCostProgression costProgression = DistrictCostProgression::None;
    int costDiscountPercent = 0;
    bool needsPopulation = false;  // counts toward the population limit
    int maintenance = 0;
    bool notAdjacentToCityCenter = false;
    bool water = false;  // placed on Coast or Lake next to land (Harbor)
    std::vector<DistrictAdjacency> adjacency;
    std::vector<std::pair<TypeIndex, int>> greatPersonPoints;  // (great person class, points per turn)
    Yields tradeDomestic{}, tradeInternational{};  // to a route's origin when this district is at its destination (07)
    int housing = 0, amenities = 0;  // to its city once complete
    int appeal = 0;                  // to neighbouring plots (01: Appeal)
    // Housing added by the district plot's appeal: (minimum appeal, change), highest first (Neighborhood, Preserve).
    std::vector<std::pair<int, int>> appealHousing;
    bool aqueduct = false;          // next to the City Center and a River, Lake, Oasis or Mountain; housing to 6 or +2
    bool onePerPlayer = false;      // Government Plaza, Diplomatic Quarter
    bool floodplainsRiver = false;  // on Floodplains along a river (Dam)
    bool preventsDrought = false, preventsFloods = false;  // for its city's plots [GS]
    std::vector<TypeIndex> exclusiveWith;  // not in a city that has one of these
    std::vector<TypeIndex> validTerrains;  // only on these terrains (empty: any; Spaceport: flat land)
    int airSlots = 0;                      // aircraft based here (City Center 1, Aerodrome 2)
    bool canal = false;                    // between two bodies of water (or water and the City Center); ships sail through
    std::vector<std::string> exclusiveIds;  // (loading only)
};

// Amenity balance bands (eras-moments-loyalty.md, Amenities).
struct HappinessLevel {
    std::string id;
    int minBalance = 0;  // INT32_MIN for the lowest band
    int growthPercent = 0;
    int yieldPercent = 0;  // non-food yields
    int loyaltyPerTurn = 0;  // [R&F]
};

// Loyalty levels [R&F] (02-cities.md, Loyalty): yield and growth scaling by loyalty.
struct LoyaltyLevel {
    std::string id;
    int minLoyalty = 0;
    int yieldPercent = 0;   // added to every yield (-100 .. 0)
    int growthPercent = 100;
};

// Research trees (04-tech-civics-government.md).
struct EraType {
    std::string id, name;
    int embarkedStrength = 10;     // defence of an embarked unit whose owner is in this era (05: Embarkation)
    int greatPersonBaseCost = 0;   // great person points for this era's first great person (07)
    int tradeRouteExtraTurns = 0;  // added to a trade route's minimum length in this world era [GS]
    int minTurns = 0, maxTurns = 0;  // how long the world stays in this era (0: no limit) [R&F]
    int eraScoreShift = 0;           // shift to both age thresholds when this era is scored [GS]
    int grievanceDecay = 0;          // grievances that fade each turn while the world is in this era [GS]
};

// Historic moments (09: Era score and Ages; data: eras-moments-loyalty.md).
struct MomentType {
    std::string id, name;
    int eraScore = 0;
    int obsoleteEra = -1;  // stops counting once the world reaches this era (-1: never)
};

// Roads (01: Routes): movement cost along them, and whether they bridge rivers.
struct RouteType {
    std::string id, name;
    Fixed moveCost = Fixed::fromInt(1);
    bool bridges = false;
    int era = 0;  // the era whose roads these are
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
    bool embarkAll = false;        // every land unit may embark (Shipbuilding)
    TypeIndex embarkUnit = kNone;  // one unit type may embark (Builders after Sailing, Traders after Celestial Navigation)
    bool ocean = false;            // units may enter Ocean (Cartography)
    int embarkedMoves = 0;         // + movement while embarked
    bool tradeCapacity = false;    // +1 trade route capacity (Foreign Trade)
    int envoys = 0;                // envoys granted on completion (civics)
    int spies = 0;                 // spy capacity it grants (08: Espionage)
};

enum class PolicySlot : uint8_t { Military = 0, Economic, Diplomatic, Wildcard, GreatPerson };
constexpr size_t kNumGovernmentSlotTypes = 4;  // Military, Economic, Diplomatic, Wildcard

struct GovernmentType {
    std::string id, name;
    int tier = 0;
    Unlock unlock;
    std::array<int, kNumGovernmentSlotTypes> slots{};
    int totalSlots() const { return slots[0] + slots[1] + slots[2] + slots[3]; }
    int influencePerTurn = 0;    // influence points toward envoys (08: City-States)
    int influenceThreshold = 0;  // points per batch of envoys
    int envoysPerThreshold = 0;
    int favor = 0;               // Diplomatic Favor per turn [GS]
};

// City-states (08: City-States; data: city-states.md).
enum class CityStateKind : uint8_t { Scientific = 0, Cultural, Religious, Trade, Industrial, Militaristic };
struct CityStateType {
    std::string id, name, suzerainText;
    CityStateKind kind = CityStateKind::Scientific;
};
// What a city-state of a kind gives each player with enough envoys there, while at peace.
enum class EnvoyToward : uint8_t { Units = 0, Buildings, Districts };
struct EnvoyBonus {
    CityStateKind kind = CityStateKind::Scientific;
    int envoys = 1;
    YieldType yield = YieldType::Food;
    int amount = 0;             // + yield (in the capital, or per `building`)
    bool capital = false;
    TypeIndex building = kNone;
    int production = 0;         // + production toward `toward` items (in the capital, or in cities with one of `buildings`)
    EnvoyToward toward = EnvoyToward::Units;
    std::vector<TypeIndex> buildings;
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
    UnitStrength,             // player: +amount combat strength for units of `unitClass` (`vsBarbarians`: only against them)
    DistrictAdjacencyPercent, // player: +amount % adjacency yield for `district`
    CityLoyalty,              // +amount loyalty per turn in a city [R&F]
    // Religion (06), player effects of founder and enhancer beliefs:
    FounderYieldPerCity,        // + `yield` per city (any owner) following the player's religion
    FounderYieldPerFollowers,   // + `yield` per `per` followers of the player's religion
    FounderYieldPerDistrict,    // + `yield` per `district` in cities following the player's religion
    ReligionPressureRange,      // + tiles of passive pressure range
    ReligionPressurePercent,    // + % passive pressure
    ReligiousUnitDiscountPercent,  // % off Faith purchases of religious units
    UnitStrengthNearFollowingCity, // + strength for combat units near cities following the religion (`foreign`: only theirs)
    ReligiousUnitsIgnoreTerrain,   // flag: religious units pay 1 per plot
    NoCombatPressureLoss,          // flag: theological defeats cost no pressure
    ReligionColonizes,             // flag: new cities start following the religion
    // City effects (governor promotions, 08: Governors):
    CityYieldPerPop,               // + `yield` per citizen
    CityYieldPerDistrict,          // + `yield` per completed district
    CityGreatPersonPercent,        // + % great person points from the city
    CityHarvestPercent,            // + % yield from harvests and chops on its plots
    CityBorderGrowthPercent,       // + % border expansion rate
    CityDistrictProductionPercent, // + % production toward districts
    CityReligionPressurePercent,   // + % religious pressure the city exerts
    SettlerNoPopCost,              // flag: settlers trained here cost no population
    BuilderExtraCharges,           // + build charges for builders trained here
    WarWearinessPercent,           // player: + % war weariness gained (Propaganda -25, Fascism +20)
};
enum class ReqType : uint8_t {
    PlotHasResource = 0,
    PlotHasFeature,
    PlotHasTerrain,
    CityHasBuilding,
    CityIsCapital,
    CityMinPopulation,
    PlayerIsHuman,
    PlotHasImprovement,  // ref kNone: any improvement (PlotHasFeature likewise: any feature)
    PlotNextToRiver,
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
enum class ModSource : uint8_t { Building = 0, Civ, Everyone, Policy, Government, Belief, Governor };

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
    bool vsBarbarians = false;  // UnitStrength
    int per = 1;                // FounderYieldPerFollowers: followers per point
    bool foreign = false;       // UnitStrengthNearFollowingCity: foreign cities only (Crusade)
    TypeIndex district = kNone;  // DistrictAdjacencyPercent
};

// City projects (03-districts-buildings-wonders.md, Projects; data: projects.md).
enum class ProjectEffectKind : uint8_t { RepairWalls = 0, Loyalty, Favor, RemoveCo2, RevealMap, CultureFromScience, ExpeditionSpeed, Wmd };
struct ProjectEffect {
    ProjectEffectKind kind = ProjectEffectKind::Loyalty;
    int amount = 0;
    TypeIndex weapon = kNone;  // Wmd: Rules::wmds
};
// A weapon of mass destruction (05: Nuclear weapons; data: units.md, WMDs).
struct WmdType {
    std::string id, name;
    int blastRadius = 1;
    int falloutTurns = 10;
    int icbmRange = 12;    // from a Missile Silo or a Nuclear Submarine
    int maintenance = 0;   // gold per turn for each one held
};
struct ProjectType {
    std::string id, name;
    std::string districtId;            // runs in this district ("": any city)
    TypeIndex district = kNone;        // kNone with a districtId: a district the core lacks (not available)
    Unlock unlock;
    int cost = 0;
    DistrictCostProgression costProgression = DistrictCostProgression::None;
    int costProgressionParam = 0;      // GAME_PROGRESS: cost x (1 + param/100 x tree progress)
    int maxPerPlayer = 0;              // 0: repeatable
    bool spaceRace = false;
    TypeIndex prerequisite = kNone;    // a project the player must have completed
    bool converts = false;
    YieldType conversionYield = YieldType::Gold;
    int conversionPercent = 0;         // share of the city's production added as that yield while it runs
    std::vector<std::pair<TypeIndex, int>> greatPersonPoints;  // on completion
    TypeIndex resource = kNone;
    int resourceAmount = 0;            // strategic resource spent on completion
    std::vector<ProjectEffect> effects;
    bool modelled = false;             // the core carries all of it (others are not offered)
};

// A difficulty level (00-overview.md, Difficulty levels; Sovereign: skill first, AI bonuses only at the top two).
struct DifficultyType {
    std::string id, name;
    int aiSkill = 3;
    int aiYieldPercent = 0;           // Science, Culture, Faith in AI cities
    int aiProductionGoldPercent = 0;  // Production, Gold in AI cities
    int aiCombat = 0, aiXpPercent = 0, aiFreeBoosts = 0;  // free Eurekas and Inspirations as each era begins
    int aiExtraWarriors = 0, aiExtraBuilders = 0, aiExtraSettlers = 0;
    int humanCombat = 0, humanXpPercent = 0, humanCampGoldPercent = 0;
};

// Natural disasters and climate (09: Climate and Disasters [GS]; data: climate-disasters.md).
enum class DisasterKind : uint8_t { Flood = 0, Eruption, Blizzard, DustStorm, Tornado, Hurricane, Drought, Fire };
enum class DisasterDamageType : uint8_t {
    ImprovementDestroyed = 0, ImprovementPillaged, PopulationLoss, CivilianKilled, UnitDamageLand, UnitDamageNaval, CityGarrison, CityWalls, Other,
};
struct DisasterDamage {
    DisasterDamageType type = DisasterDamageType::Other;
    int percent = 0, minHp = 0, maxHp = 0;
};
struct DisasterFertility {
    YieldType yield = YieldType::Food;
    TypeIndex feature = kNone;  // kNone: any affected land plot
    int percent = 0, amount = 0;
    bool replaceFeature = false;
};
constexpr int kNumDisasterIntensities = 5;  // Minimal, Light, Moderate, Heavy, Hyperreal
struct DisasterType {
    std::string id, name;
    DisasterKind kind = DisasterKind::Flood;
    int severity = 0, hexes = 0, duration = 0, chancePerDegree = 0;
    std::array<int, kNumDisasterIntensities> frequencyTenths{};  // expected occurrences per game, x10
    std::vector<DisasterDamage> damage;
    std::vector<DisasterFertility> fertility;
};
struct ClimatePhaseType {
    std::string id, name;
    int points = 0;  // climate change points this phase adds (one point: 0.5 degrees)
    int iceLoss = 0, fertilityRemoval = 0;
};
struct DisasterIntensityType {
    std::string id, name;
    int activeVolcanoes = 70, extraRange = 0;
};

// World Congress resolutions (08: Diplomatic Favor and World Congress [GS]; data:
// world-congress-emergencies.md). The core carries the effects of the kinds listed; the others
// are never put to a vote.
enum class ResolutionKind : uint8_t {
    Unsupported = 0, DiplomaticVictory, TradePolicy, Patronage, MigrationTreaty, PublicRelations, MilitaryAdvisory, UrbanDevelopment,
};
enum class ResolutionTarget : uint8_t { Player = 0, GreatPersonClass, District, PromotionClass, Other };
struct ResolutionType {
    std::string id, name, optionA, optionB;
    ResolutionKind kind = ResolutionKind::Unsupported;
    ResolutionTarget target = ResolutionTarget::Other;
    int minEra = -1, maxEra = -1;  // world eras it may be proposed in (-1: no bound)
};

// A spy operation (08: Espionage; data: diplomacy-espionage.md, Spy operations). Success is
// a 3d6 roll at or above base - 2, less the spy's level and bonuses, plus the defence.
struct SpyOperationType {
    std::string id, name;
    int turns = 8;
    int base = 0;              // 0: no roll (passive operations)
    int levelChange = 1, enemyChange = 3, enemyLevelChange = 1;
    TypeIndex district = kNone;  // the district the target city needs
    bool needsDistrict = false;  // a district is named (kNone then: one the core cannot place yet)
};

// A spy promotion (08: Espionage; data: promotions.md, Espionage): one is chosen per level gained.
struct SpyPromotionType {
    std::string id, name;
    std::vector<int> levels;     // per Rules::spyOperations: extra levels on that operation
    std::vector<int> faster;     // per Rules::spyOperations: % fewer turns
    int allLevels = 0;           // extra levels on every operation
    int escape = 0;              // easier escape (3d6 need lowered)
    int travelFaster = 0;        // % faster to establish in a new city
    int counterspyLevels = 0;    // extra levels when counterspying
};

// Governors (08: Governors [R&F]; data: governors.md). A governor's promotions form a tree:
// one is its base ability; each other needs one of its `prerequisites`.
struct GovernorPromotionType {
    std::string id, name, effects;  // effects: the rules text (what the core carries is in modifiers.json)
    TypeIndex governor = kNone;
    int tier = 0, column = 0;
    bool base = false;
    std::vector<TypeIndex> prerequisites;
};
struct GovernorType {
    std::string id, name, title;
    int establishPercent = 100;  // speed of establishing (Victor 150: faster)
    int loyalty = 8;             // loyalty per turn in its city once established
    bool cityStates = false;     // may serve in a city-state (Amani)
    std::vector<TypeIndex> promotions;  // GovernorPromotionType indices; the base ability first
};

// A leader's agenda: what the AI version likes and dislikes (leaders-and-art-style.md,
// Leader details; scored in diplomacy.cpp).
enum class Agenda : uint8_t {
    None = 0, QueenOfTheSeas, DefenderOfTheFaith, PaxRomana, SpartanPride, TolerantConqueror, Magnanimous,
    FirstEmperor, ClosedCountry, EternalName, PatronOfTrade, HonourableWar, SapaInca,
};

// A civ's own ability (leaders-and-art-style: Civ abilities, uniques and dynasties). Plot yields go
// through civ-sourced modifiers; these are the effects the modifier model does not carry.
struct CivAdjacency {
    TypeIndex district = kNone;      // the district that gains
    TypeIndex from = kNone;          // per adjacent district of this type, or
    std::string fromTerrainBase;     // per `per` adjacent plots of this terrain climate
    int per = 1;
    YieldType yield = YieldType::Production;
    int amount = 0;
};
struct CivAbility {
    std::string name;
    std::vector<CivAdjacency> extraAdjacency;
    int wonderProductionPercent = 0, wonderEraMin = 0, wonderEraMax = 0;  // toward wonders of these eras
    int amenityPerWonder = 0;           // in the wonder's city
    int foundPopulation = 0;            // new cities start larger
    TypeIndex foundBuilding = kNone;    // and with this building
    int culturePerSuzerainty = 0;       // in the capital
    int governorLoyalty = 0, governorGold = 0;  // in cities with an established governor
    TypeIndex extraGovernorTitleCivic = kNone;  // +1 governor title with this civic
    int desertRouteGold = 0;            // trade routes whose way crosses desert
    Yields capitalYieldsPerGovernorTitle{};
    Fixed freshWaterFarmHousing;        // per farm next to a river
    int mountainDistrictProductionPercent = 0;  // in cities next to a mountain
    int mountainProduction = 0;         // mountains can be worked for this much production
    // Leader abilities (Leader details) use the same struct; these fields are theirs so far.
    std::array<int, 3> domainProductionPercent{};  // toward units of a Domain (Land, Sea, Air)
    std::vector<std::pair<TypeIndex, int>> greatPersonPercent;  // +% points of a great person class
    int intercontinentalRouteGold = 0;  // international routes to another landmass
    Yields internationalRouteYields{};
    std::vector<std::pair<TypeIndex, Yields>> districtBuildingYields;  // per building in that district
    std::vector<std::pair<TypeIndex, int>> districtBuildingAmenities;
    struct NearLeader { std::string unitClass; int amount = 0, range = 0; };
    std::vector<NearLeader> strengthNearLeader;
    int cityCenterBuildingProductionPercent = 0, wallProductionPercent = 0;
    int governorAmenity = 0;            // in cities with an established governor
    std::vector<TypeIndex> grantAbilities;  // to its units of the ability's classes
    int capturedCityLoyalty = 0;        // per turn in cities another civ founded
    int foreignReligionAmenity = 0;     // in its cities following another religion
    int nearFollowingCityStrength = 0, nearFollowingCityRange = 0;
    int extraBuilderCharges = 0;
    Yields peaceYieldPercent{};         // while at peace with every major civ
    int wonderCulture = 0;              // per wonder in the city
    TypeIndex faithPurchaseDistrict = kNone;  // that district's buildings can be bought with Faith
    int killFaithPercent = 0;           // Faith per kill: % of the victim's strength
    int capitalAmenityPerKills = 0, capitalAmenityMax = 0;  // +1 in the capital per that many kills this era
    int mountainCityHousing = 0;        // in cities next to a mountain
    // Heir traits (leaders-and-art-style: Dynasties) so far.
    Yields capitalYields{};
    int cityLoyalty = 0;                // per turn in every city
    int unitXpPercent = 0;              // combat XP for all its units
    std::vector<std::pair<std::string, int>> classStrength;  // + strength for a unit class
};

struct CivType {
    std::string id, name, leader;
    CivAbility ability;         // the civ's own (Civ abilities, uniques and dynasties)
    CivAbility leaderAbility;   // its launch leader's (Leader details)
    CivAbility combined;        // both, as the rules apply them
    std::vector<std::string> cityNames;
    Agenda agenda = Agenda::None;
    std::string agendaId, agendaName, agendaText;
    std::string leaning, voice;  // the leader's leaning and speaking voice (diplomacy personas)
};

// The leader's loadout (leader doc §2, §8.8; data in leader.json). Weapons set melee
// strength (and ranged strength and range); armor adds defence when defending; mounts
// add movement and cost twice the upkeep of the matching mounted unit.
enum class GearSlot : uint8_t { Weapon = 0, Armor, Mount };
constexpr int kNumGearSlots = 3;

struct GearType {
    std::string id, name;
    GearSlot slot = GearSlot::Weapon;
    Unlock unlock;            // none: available from the start
    int combat = 0;           // melee strength (weapons)
    int ranged = 0, range = 0;  // ranged weapons
    int defense = 0;          // added when the leader defends (armor)
    int moves = 0;            // movement change (mounts add, heavy armor may subtract)
    TypeIndex strategicResource = kNone;
    int strategicCost = 0;    // spent once when equipped
    int goldCost = 0;         // at Standard speed
    TypeIndex upkeepAs = kNone;  // mounts: the unit whose maintenance the leader pays twice
};

// A civ's hand-made line of rulers (leaders-and-art-style.md, Dynasties): the starting
// leader, then its heirs in order.
struct Dynasty {
    std::string id;
    TypeIndex civ = kNone;
    std::vector<std::string> names;
    std::vector<CivAbility> traits;    // per name (0, the starting leader: none): the heir's personal trait
    std::vector<CivAbility> combined;  // the civ's abilities with that heir's trait
};

struct MapSizeType {
    std::string id;
    int width = 0, height = 0;
    int defaultPlayers = 0;
    int maxReligions = 0;  // religions that can be founded (Great Prophets) on this size
    int defaultCityStates = 0;
    int64_t co2PerDegree = 2000000;  // CO2 for each degree of warming (09: Climate [GS])
};

struct GameSpeedType {
    std::string id;
    int costPercent = 100;
    int turns = 500;
};

class SOV_API Rules {
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
    std::vector<DistrictType> districts;
    std::vector<BarbarianTribe> barbarianTribes;
    std::vector<ImprovementType> improvements;
    std::vector<EraType> eras;
    std::vector<TreeNode> techs;
    std::vector<TreeNode> civics;
    std::vector<GovernmentType> governments;
    std::vector<PolicyType> policies;
    std::vector<HappinessLevel> happiness;  // ascending by minBalance
    std::vector<LoyaltyLevel> loyaltyLevels;  // ascending by minLoyalty
    std::vector<Modifier> modifiers;
    std::vector<CivType> civs;
    std::vector<MapSizeType> mapSizes;
    std::vector<GameSpeedType> speeds;
    std::vector<std::string> startingUnits;  // unit ids every major civ starts with
    std::vector<DifficultyType> difficulties;  // Settler .. Deity (setup.json)
    std::vector<ProjectType> projects;
    std::vector<WmdType> wmds;
    std::vector<GearType> gear;
    std::vector<Dynasty> dynasties;
    std::vector<GreatPersonClass> greatPersonClasses;
    std::vector<GreatPersonType> greatPeople;  // every individual, by class then era (07: Great People)
    std::vector<GreatWorkType> greatWorkTypes;
    std::vector<BeliefType> beliefs;
    std::vector<RouteType> routes;  // by era, Ancient first
    std::vector<CityStateType> cityStates;
    std::vector<GovernorType> governors;
    std::vector<SpyOperationType> spyOperations;
    std::vector<SpyPromotionType> spyPromotions;
    std::vector<ResolutionType> resolutions;
    std::vector<DisasterType> disasters;
    std::vector<ClimatePhaseType> climatePhases;
    std::vector<DisasterIntensityType> disasterIntensities;
    std::vector<std::string> promotionClasses;  // the unit promotion classes in use (Military Advisory targets)
    std::vector<GovernorPromotionType> governorPromotions;
    std::vector<std::pair<TypeIndex, int>> governorTitleCivics;  // civic, titles it grants
    std::vector<MomentType> moments;
    std::vector<EnvoyBonus> envoyBonuses;
    std::vector<ReligionType> religions;
    TypeIndex leaderUnit = kNone;  // the unit every major civ's leader is (layer Leader)

    TypeIndex terrain(const std::string& id) const;
    TypeIndex feature(const std::string& id) const;
    TypeIndex resource(const std::string& id) const;
    TypeIndex ability(const std::string& id) const;
    TypeIndex promotion(const std::string& id) const;
    TypeIndex unit(const std::string& id) const;
    TypeIndex building(const std::string& id) const;
    TypeIndex district(const std::string& id) const;
    TypeIndex improvement(const std::string& id) const;
    TypeIndex era(const std::string& id) const;
    TypeIndex tech(const std::string& id) const;
    TypeIndex civic(const std::string& id) const;
    TypeIndex government(const std::string& id) const;
    TypeIndex policy(const std::string& id) const;
    // Modifiers whose source is this id, in load order.
    std::vector<const Modifier*> modifiersFrom(const std::string& source) const;
    TypeIndex civ(const std::string& id) const;
    TypeIndex gearType(const std::string& id) const;
    TypeIndex greatPersonClass(const std::string& id) const;
    TypeIndex greatPerson(const std::string& id) const;
    TypeIndex greatWorkType(const std::string& id) const;
    TypeIndex belief(const std::string& id) const;
    TypeIndex religion(const std::string& id) const;
    TypeIndex moment(const std::string& id) const;
    TypeIndex governor(const std::string& id) const;
    TypeIndex spyOperation(const std::string& id) const;
    TypeIndex resolution(const std::string& id) const;
    TypeIndex project(const std::string& id) const;
    TypeIndex wmd(const std::string& id) const;
    TypeIndex governorPromotion(const std::string& id) const;
    // The civ's dynasty, or null when it has none.
    const Dynasty* dynastyOf(TypeIndex civ) const;
    TypeIndex mapSize(const std::string& id) const;
    // The civ's unique unit replacing `base` (kNone: none, `base` itself stays).
    TypeIndex uniqueUnitFor(TypeIndex civ, TypeIndex base) const;
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

SOV_API uint64_t fnv1a(const void* data, size_t size, uint64_t h = 0xCBF29CE484222325ull);

}  // namespace sov
