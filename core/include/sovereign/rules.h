// Rules data: every unit, terrain, resource, civ and tunable constant is loaded
// from JSON files under data/rules/, never hard-coded (engine doc, "Rules are
// data"). Later directories override earlier ones row by row (by "id"), which
// is how mods change rules.
#pragma once

#include "sovereign/api.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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

// What pillaging gives the unit's owner (05: Pillage; data: Districts.PlunderType/Amount).
enum class PlunderKind : uint8_t { None = 0, Gold, Faith, Science, Culture, Heal };
struct Plunder {
    PlunderKind kind = PlunderKind::None;
    int amount = 0;
};

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
    // Natural wonders (01): placed by the map script over `tiles` plots; they give their own yields,
    // `adjacentYields` to neighbouring plots, and may double neighbours' terrain yields.
    bool naturalWonder = false;
    int tiles = 1;
    Yields adjacentYields{};
    bool doublesAdjacentTerrain = false;
    bool noCity = false;  // no city is founded on it: a natural wonder or an Oasis (02: Founding)
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
    AuraLoyalty,        // +amount loyalty per turn to the city whose land the leader stands on
    StancePower,        // +amount % to a stance's outcome: Benevolence lasts longer, Fear gives more loyalty (Statesman)
    LeadsEscort,        // the leader's linked escort fights as a formation amount steps larger (Marshal)
    // Civ uniques (leaders-and-art-style: Civ abilities, uniques and dynasties).
    HealOnKill,         // +amount HP when it destroys a unit
    MeleeAndRanged,     // a ranged unit that may also attack in melee
    CaptureAsBuilder,   // a land unit it destroys joins its owner as a Builder
    // Rock Band promotions [GS] (07: Rock Bands).
    BandLevel,          // +amount levels for a concert at `at`
    BandBurst,          // +amount tourism from a concert at `at`
    PlunderPercent,     // +amount% to what it gains by pillaging or plundering (07: Plunder)
    CheapPillage,       // pillaging costs PILLAGE_ADVANCED_MOVEMENT_COST (05: Pillage)
    // Movement, sight, healing and kills (05: promotions):
    IgnoreHills,        // hills cost 1 movement
    IgnoreForest,       // woods cost no extra movement
    IgnoreTerrain,      // every land plot costs 1 movement
    FreeEmbark,         // embarking and disembarking cost no extra movement
    SeesThroughFeatures,  // woods and rainforest do not block its sight
    CoastalRaid,        // may raid the coast like a naval raider
    HealNeutral,        // + HP healing in neutral territory
    HealEnemy,          // + HP healing in enemy territory
    AirSlots,           // + aircraft it can carry
    KillYield,          // `amount`% of a killed unit's strength as yield `at` (GOLD, FAITH, CULTURE)
    // Apostle promotions (06: one random promotion for each new Apostle):
    SpreadCharges,      // + spread charges (Orator)
    EvictPercent,       // + % of other religions removed when it spreads (Proselytizer)
    ForeignSpreadPercent,  // + % spread strength in other civs' cities (Translator)
    WonderCharges,      // + charges the first time it stands next to a natural wonder (Pilgrim)
    ConvertGold,        // + Gold the first time it turns a city to its religion (Indulgence Vendor)
    HeathenConversion,  // spreading turns the barbarians next to it (Heathen Conversion)
    Martyr,             // a Relic if it dies in theological combat
    HealAura,           // + healing for the owner's units next to it (Chaplain)
    OpenGroundMoves,    // + movement when its turn starts on flat ground with no feature (Heavy Chariot)
    ObservedRange,      // + range while a friendly Observation unit stands next to it (Observation Balloon, Drone)
    FightEmbarked,      // may attack while embarked (Giant Death Robot)
    Hidden,             // seen only from next to it or by units that see hidden ones (Stealth, Camouflage, Twilight Veil)
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
    OpponentMinEra,     // the opponent unit is of this era or later (ref: Rules::eras)
    InFormation,        // this unit is a Corps or Army
    TileFort,           // this unit's plot has an improvement with a defense bonus (a Fort)
    CoastalTile,        // this unit's plot is land next to water
    HomeContinent,      // this unit is on its owner's capital's landmass
    OpponentMinor,      // the opponent belongs to a city-state
    OpponentFreeCity,   // the opponent belongs to the Free Cities
    NearOwnTerritory,   // this unit is in or next to its owner's territory
    NextToFriendlyClass, // a unit of the owner's of class `value` stands next to it (a Drone)
    TileFlat,           // this unit's plot is flat land (no hills, mountain or water)
    TileRoad,           // this land unit's plot has a road that is not pillaged
    NextToMountain,     // a mountain stands next to this unit's plot
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
    std::string at;  // BandLevel/BandBurst: a district or improvement id, or WONDER, NATIONAL_PARK, NATURAL_WONDER
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
    std::vector<std::string> tags;  // further class tags abilities may name (Lahore's Nihang: LAHORE_NIHANG)
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
    int excavations = 0;       // antiquity sites it may dig (the Archaeologist; 07: Archaeology)
    bool wmdImmune = false;    // the Giant Death Robot
    int moves = 2;
    int sight = 2;
    bool zoneOfControl = false;
    bool foundCity = false;
    int buildCharges = 0;
    bool buildsRoads = false;       // spends a charge on a road (01: Routes; Military Engineers, the Legionary)
    std::vector<TypeIndex> builds;  // its charges build only these, unlocked by the unit itself (the Legionary's Fort)
    int costProgression = 0;  // PREVIOUS_COPIES: extra cost per copy already trained
    int gameProgressPercent = 0;  // GAME_PROGRESS: cost x (1 + this/100 x tree progress) (the Trader, 400)
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
    bool bodyDouble = false;       // trained into the player's body doubles, off the map (leader doc §8.6)
    TypeIndex needsDistrict = kNone;  // the training city must have this district finished
    // A city-state's unit (08: Lahore's Nihang): never trained; whoever enjoys the city-state's suzerain bonus buys it.
    TypeIndex cityState = kNone;
    std::string cityStateId;  // (loading only)
};

// A Builder (01, 02): its charges improve and harvest tiles, and the Builder bonuses (Pyramids, Serfdom...) are its.
inline bool isBuilder(const UnitType& t) { return t.buildCharges > 0 && t.layer == UnitLayer::Civilian; }

// Whether an ability for these class tags reaches the unit: one of them is its class or one of its tags.
inline bool hasClassIn(const UnitType& t, const std::vector<std::string>& classes) {
    if (std::find(classes.begin(), classes.end(), t.unitClass) != classes.end()) return true;
    for (const std::string& tag : t.tags) {
        if (std::find(classes.begin(), classes.end(), tag) != classes.end()) return true;
    }
    return false;
}

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
    // ...or what else it counts next to it: a district (`district`: an id, or ANY), a feature, a resource class, or
    // any resource the owner can see on water (the Fishery's sea resources, as a Harbor counts them).
    std::string district;
    TypeIndex feature = kNone;
    int resourceClass = -1;          // ResourceClass, -1: none
    bool seaResource = false;
    Unlock needs, obsoleteWith;
};

// A yield an improvement adds to its own plot when the plot qualifies (08: the Moai beside the coast, or on or
// beside Volcanic Soil).
struct ImprovementTileYield {
    YieldType yield = YieldType::Food;
    Fixed amount;
    bool nextToCoast = false;       // a Coast plot beside it
    TypeIndex nearFeature = kNone;  // this feature on the plot or beside it
};

// A yield an improvement gives each plot of its owner beside it (08: the Nazca Line).
struct ImprovementNeighbourYield {
    YieldType yield = YieldType::Food;
    Fixed amount;
    Unlock needs;
    bool resource = false;      // the plot has a resource its owner can see
    TypeIndex terrain = kNone;  // the plot is of this terrain
    bool notHills = false;      // the plot is not hills
};

struct ImprovementType {
    std::string id, name;
    Unlock unlock;
    Yields yields{};
    std::vector<TypeIndex> validTerrains, validFeatures, validResources;
    std::vector<ImprovementBonus> bonuses;
    std::vector<ImprovementAdjacency> adjacency;
    std::vector<ImprovementTileYield> tileYields;
    std::vector<ImprovementNeighbourYield> neighbourYields;
    Fixed housing;  // per improved plot the city owns
    int appeal = 0;  // to neighbouring plots (01: Appeal)
    // Tourism (07): equal to the plot's appeal, Culture, Faith... once `tourismAfter` is known (none: at once).
    std::string tourismSource;
    int tourismPercent = 0;
    Unlock tourismAfter;
    int minAppeal = -100;  // the plot's appeal it needs (Seaside Resort 4)
    std::optional<YieldType> appealYield;  // as much of this yield as the plot's Appeal (the Seaside Resort's Gold, 03)
    std::vector<std::pair<TypeIndex, Unlock>> terrainUnlocks;  // terrains it needs more for (a Farm on Hills: Civil Engineering, 03)
    bool coastal = false;  // on the coast only
    bool water = false;    // works the sea (Fishing Boats, Offshore Oil Rig ...): on water only, and the others on land only
    // Civ unique improvements (leaders-and-art-style).
    TypeIndex uniqueTo = kNone;
    std::string uniqueToId;  // (loading only)
    // City-states' unique improvements (08): for players enjoying that city-state's suzerain bonus.
    TypeIndex cityState = kNone;
    std::string cityStateId;  // (loading only)
    // Improvements a governor opens (08: Liang's Fishery and City Park): built only where the city's governor
    // holds the promotion, which adds `governorYields` there.
    TypeIndex governorPromotion = kNone;
    std::string governorPromotionId;  // (loading only)
    Yields governorYields{};
    int waterAmenity = 0;    // to its city when beside the coast, a lake or a river (City Park)
    int amenities = 0;       // to its city
    // Amenities to its city while within `nearWonderRange` tiles of this wonder (03: the Temple of Artemis's Camps,
    // Pastures and Plantations).
    TypeIndex nearWonder = kNone;
    std::string nearWonderId;  // (loading only)
    int nearWonderRange = 0, nearWonderAmenities = 0;
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
    bool tunnel = false;        // Mountain Tunnel: its mountain becomes passable (built from a neighbouring plot)
    Plunder plunder;            // what pillaging it gives
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
    Ability,            // permanent: an ability for the player's units of its classes
    // One-time (07: Great People):
    Envoys,             // `amount` envoys to send
    EnvoysHere,         // `amount` envoys at the city-state it stands in
    GovernorTitles,     // `amount` governor titles
    Relic,              // `amount` relics into free slots
    RandomTechs,        // `count` random available techs, completed
    Formation,          // the military unit here becomes a Corps (1) or Army (2)
    UnitsInDistricts,   // a `ref` unit in each of this city's districts
    NavalMeleeUnit,     // the player's best naval melee unit, here
    ScienceAdjacent,    // `amount` Science per adjacent plot of `what` (MOUNTAIN or a feature id)
    SciencePerArtifact, // `amount` Science per artifact in this city
    ScienceNearWonder,  // `amount` Science when next to a natural wonder
    WonderProduction,   // `amount` toward a wonder of [minEra, maxEra] built here, else `count`
    UnitXp,             // the military unit here gains +`amount`% combat XP for good
    ConvertBarbarians,  // barbarian units next to it join the player
    Suzerain,           // the player becomes suzerain of the city-state it stands in, others' envoys removed
    LuxuryHere,         // `amount` lasting copies of the luxury it stands on (Magellan, Colaeus)
    // Lasting, read from greatPeopleActivated (or the city's greatPeopleHere):
    TradeRoutes,        // + `amount` trade route capacity
    ResourcePerTurn,    // + `amount` of resource `ref` a turn
    DistrictCapacity,   // + `amount` districts in the city where it was used
    Ocean,              // the player's ships may enter Ocean
    ArtifactTourism,    // artifacts give `amount`% of their tourism
    // One-time, from world wonders (World Wonders):
    RandomCivics,       // `count` random available civics, completed
    DiplomaticVp,       // + `amount` diplomatic victory points
    Population,         // + `amount` population in each of the player's cities
    PromoteAll,         // every military unit of the player gains enough XP for a promotion
    TreasuryPercent,    // + `amount`% of the player's gold
    // Lasting, on the district it was used on (03: regional buildings; Tesla, Paxton):
    RegionalRange,      // the district's regional buildings reach `amount` tiles farther
    RegionalYield,      // + `amount` `yield` to each city the district's regional buildings reach
    RegionalAmenity,    // + `amount` Amenity to each city the district's regional buildings reach
    WonderPurchase,     // the rest of the wonder built on this plot, at 2 Gold per Production, up to half the treasury
    AbsorbCityState,    // the city-state whose land this is joins the player; + `amount` Loyalty a turn in this city
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
    std::string what;  // ScienceAdjacent: MOUNTAIN or a feature id
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
    bool canal = false;  // links water as a Canal district does, and ships sail through once built (the Panama Canal, 03)
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
    TypeIndex trainedAbility = kNone;  // units of its classes trained in its city keep its combat XP (the Barracks, 03)
    int citizenSlots = 0;              // specialist slots it opens in its district (02)
    Yields specialistYields{};         // extra yields for each specialist in its district
    // Power [GS] (09: Power).
    int requiredPower = 0;             // power it needs to work fully
    Yields poweredYields{};            // extra yields while its city is fully powered
    int poweredAmenities = 0;
    TypeIndex burnsResource = kNone;   // a power plant: burns this, `powerPerResource` power each
    int powerPerResource = 0;
    int powerProvided = 0;             // free power to its city (Hydroelectric Dam)
    int projectChargePercent = 0;      // each Builder charge completes this share of a project (Royal Society, 03)
    int plazaTier = 0;                 // a Government Plaza building's tier, needing a government of that tier (03); 0 for others
    // Yields its district's unimproved neighbours gain while their Appeal is within the bounds (the Preserve's Grove and
    // Sanctuary, 03).
    struct AppealYield {
        YieldType yield = YieldType::Food;
        int amount = 0, minAppeal = 0, maxAppeal = 0;
    };
    std::vector<AppealYield> appealYields;
    int defense = 0;
    std::vector<TypeIndex> prereqs;  // buildings needed first, any one of them (BuildingPrereqs: the Armory needs a Barracks or a Stable)
    std::vector<TypeIndex> prereqsAny;  // a wonder's: any one of these in the city (03: the Great Library needs a Library)
    std::vector<TypeIndex> exclusiveWith;  // never in a city with one of these (the Barracks and the Stable, 03)
    bool needsRiver = false;
    bool purchasable = false;
    bool granted = false;  // given by the rules (Palace), never built
    bool faithOnly = false;  // a worship building: bought with Faith by a religion holding its belief
    TypeIndex districtType = kNone;  // Rules::districts; kNone while its district is not modelled
    bool meleeCannotDamageWalls = false;
    bool preventsFloods = false;  // no floods in its city's plots [GS] (the Great Bath)
    bool wallsCannotBeBypassed = false;
    std::vector<std::pair<TypeIndex, int>> greatPersonPoints;  // (great person class, points per turn)
    std::vector<std::pair<std::string, int>> greatWorkSlots;   // (slot type, count): "WRITING", "ART", ...
    int stockpileCap = 0;  // raises its owner's strategic resource stockpile caps (01: [GS] stockpile model)
    // Theming (07: Theming bonuses): with every slot full and the works matching, their yields and
    // tourism gain these percents.
    struct Theming {
        bool uniquePerson = false, sameObject = false, uniqueCivs = false, sameEra = false;
        int yieldPercent = 0, tourismPercent = 0;
    };
    std::optional<Theming> theming;
    int tradeCapacity = 0;                 // + trade route capacity
    int spreadCharges = 0;                 // + spread charges for the owner's religious units (Hagia Sophia)
    int regionalRange = 0;                 // Factory, Zoo, Stadium, Aquarium, Colosseum...: its yields and Amenities reach the owner's cities this near (03)
    int modifierCount = 0;                 // modifiers it carries (counted at load, for the AI's valuation)
    int policySlots[4] = {0, 0, 0, 0};     // + policy slots by PolicySlot (Alhambra, Forbidden City, Potala Palace, Big Ben)
    TypeIndex tradeCapacityUnless = kNone; // ...unless the city has this building (Lighthouse: a Market)
    // World wonders (03: Wonders): built once in the world, on a plot of their own.
    bool wonder = false;
    WonderPlacement placement;
    std::vector<GreatPersonEffect> wonderEffects;  // one-time effects on completion (lasting kinds: while it stands)
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
    bool luxuryHere = false;     // a luxury the player can see must lie on the plot (Magellan, Colaeus)
    bool barbarianBeside = false;     // a barbarian unit beside it (Boudica)
    bool standardFormation = false;   // the military unit here is not a Corps or Army yet (El Cid, Napoleon)
    bool relicSlot = false;           // one of the player's cities has a free Relic slot (Jeanne d'Arc)
    bool enemyTerritory = false;      // on the land of a civ at war with the player (Tupac Amaru)
    bool cityStateTerritory = false;  // on a city-state's land (Matthew Perry, Zhou Daguan)
    bool suzerainTerritory = false;   // on the land of a city-state the player is suzerain of (Stamford Raffles)
    bool nonHostileTerritory = false; // not on the land of anyone at war with the player
    bool incompleteWonder = false;    // on the plot of a wonder its city is building (the Great Engineers)
    bool spaceRaceProject = false;    // its city is building a space race project (Korolev, Sagan)
    bool mountainBeside = false;      // a Mountain beside it (Galileo)
    bool naturalWonderNear = false;   // a natural wonder on or beside it (Darwin)
    TypeIndex featureNear = kNone;    // this feature on or beside it (Janaki Ammal: Rainforest)
    TypeIndex cityGreatWork = kNone;  // its city holds a Great Work of this type (Mary Leakey: an Artifact)
    std::vector<GreatPersonEffect> effects;
    std::vector<std::string> untrackedEffects;  // effects of systems not built yet (shown, not applied)
    bool hasModifiers = false;                   // lasting effects as modifiers (Rules::modifiers, source = its id)
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
enum class DistrictAdjacencyKind : uint8_t {
    Mountain = 0, River, AnyDistrict, District, Feature, Improvement, StrategicResource, SeaResource,
    Wonder,        // a finished world wonder
    NaturalWonder  // any natural wonder's plot
};
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
    TypeIndex amenityFeature = kNone;  // ...and amenityFeatureAmount more beside this feature (an Aqueduct by a Geothermal Fissure [GS])
    int amenityFeatureAmount = 0;
    int envoysNextToCityCenter = 0;  // envoys once built beside its City Center (Diplomatic Quarter [GS])
    int spyDefenseLevels = 0;        // enemy spies work this many levels lower against it and the districts beside it
    int loyalty = 0;                 // loyalty per turn in its city while it stands (the Government Plaza +8 [R&F], 02)
    int appeal = 0;                  // to neighbouring plots (01: Appeal)
    // Housing added by the district plot's appeal: (minimum appeal, change), highest first (Neighborhood, Preserve).
    std::vector<std::pair<int, int>> appealHousing;
    bool aqueduct = false;          // next to the City Center and a River, Lake, Oasis or Mountain; housing to 6 or +2
    bool onePerPlayer = false;      // Government Plaza, Diplomatic Quarter
    bool repeatable = false;        // a city may hold several (Neighborhood, Canal: no OnePerCity flag, 03)
    bool floodplainsRiver = false;  // on Floodplains along a river (Dam)
    bool preventsDrought = false, preventsFloods = false;  // for its city's plots [GS]
    std::vector<TypeIndex> exclusiveWith;  // not in a city that has one of these
    std::vector<TypeIndex> validTerrains;  // only on these terrains (empty: any; Spaceport: flat land)
    int airSlots = 0;                      // aircraft based here (City Center 1, Aerodrome 2)
    bool canal = false;                    // between two bodies of water (or water and the City Center); ships sail through
    std::string chargeUnit;                // a unit that may spend a charge on it while it is built (Military Engineer)
    int chargePercent = 0;                 // ...for this share of its cost
    std::vector<std::string> exclusiveIds;  // (loading only)
    Plunder plunder;  // what pillaging it gives (05: Pillage)
    Yields specialistYields{};  // each specialist working in it (02: Citizens and specialists)
};

// Amenity balance bands (eras-moments-loyalty.md, Amenities).
struct HappinessLevel {
    std::string id;
    int minBalance = 0;  // INT32_MIN for the lowest band
    int growthPercent = 0;
    int yieldPercent = 0;  // non-food yields
    int loyaltyPerTurn = 0;  // [R&F]
    int rebellionPoints = 0;  // added each turn (negative: removed) toward rebels rising (02: Amenities)
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

// A game begun in a later era (game-setup.md, Advanced start eras; player-retention §5): the techs and civics of
// earlier eras, this gold and faith and these units for every major civ, and cities founded with this population
// and the City Center buildings of this era and earlier ones.
struct EraStartType {
    TypeIndex era = kNone;
    int gold = 0, faith = 0;
    int capitalPopulation = 1, otherPopulation = 1;
    std::vector<std::pair<TypeIndex, int>> units;  // unit type, how many
    std::vector<TypeIndex> buildings;              // this era's own (earlier eras' rows add theirs)
};

// Achievements and the cosmetics they unlock (player-retention §7), checked when a game ends for a player.
// Cosmetic only: a cosmetic never changes the rules, and is not part of the game state.
enum class AchievementKind : uint8_t {
    Victory = 0,  // win (a `victory` kind, or any), in a game of `speed` when one is named
    RulersTaken,  // capture or slay `value` rival rulers in battle in one game
    LeaderLevel,  // the ruler reaches level `value`
    Wonders,      // hold `value` world wonders at the end
};
struct AchievementType {
    std::string id, name, text;
    AchievementKind kind = AchievementKind::Victory;
    int value = 1;
    std::string victory;  // "SCIENCE", "CULTURE", ... (empty: any)
    std::string speed;    // a game speed id (empty: any)
    std::string unlock;   // the CosmeticType it unlocks (empty: none)
};
struct CosmeticType {
    std::string id, name;
    std::array<int, 3> color{{255, 255, 255}};  // the ruler's figure tint, 0..255
};
// Historic moments (09: Era score and Ages; data: eras-moments-loyalty.md).
struct MomentType {
    std::string id, name;
    int eraScore = 0;
    int obsoleteEra = -1;  // stops counting once the world reaches this era (-1: never)
    int eraMin = -1, eraMax = -1;  // counts only while the world's era is in this window (-1: open)
};

// A Rock Band concert's outcome [GS] (07: Rock Bands).
struct RockBandResult {
    std::string id, name;
    int albumSales = 0, tourismBomb = 0, probability = 0;
    bool dies = false, gainsLevel = false, extraPromotion = false;
};

// A dedication [R&F] (09: Dedications): chosen at a new era within its era window.
struct DedicationType {
    std::string id, name;
    int eraMin = 0, eraMax = -1;  // Rules::eras (-1: no end)
};

// Roads (01: Routes): movement cost along them, and whether they bridge rivers.
struct RouteType {
    std::string id, name;
    Fixed moveCost = Fixed::fromInt(1);
    bool bridges = false;
    int era = 0;  // the era whose roads these are
    // The railroad [GS]: laid by Military Engineers only, after `tech`, paying `resourceCost` per plot.
    bool unitOnly = false;
    TypeIndex tech = kNone;
    std::vector<std::pair<TypeIndex, int>> resourceCost;
    std::vector<std::pair<std::string, int>> resourceCostIds;  // (loading only)
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
    District,                // `count` of your cities have a finished district `ref`
    SpecialtyDistricts,      // finished specialty districts (those counting toward the population limit) of `count` types
    TradeRoutes,             // `count` trade routes running
    MetCivs,                 // met `count` other major civs
    MetCityStates,           // met `count` city-states
    Pantheon,                // founded a pantheon
    Religion,                // founded a religion
    FollowingCities,         // `count` cities in the world follow the religion you founded
    Alliance,                // an alliance of level `count` or higher
    GreatPeople,             // earned `count` great people
    Corps,                   // `count` Corps or Fleets
    Armies,                  // `count` Armies or Armadas
    DistrictAppeal,          // a finished district `ref` on a plot of appeal `count` or more
    ThemedBuildings,         // `count` buildings with themed Great Works
    BuildingNextToMountain,  // building `ref` in a city whose district for it stands next to a Mountain
    Wonders,                 // `count` world wonders finished in your cities
    WonderFromEra,           // a world wonder of era `count` or later finished in one of your cities
    UnitAndImprovement,      // a unit `ref` (or its unique) and a plot with improvement `improvement` (on `resource`, if set)
    AirBaseAbroad,           // an Aerodrome or Airstrip off your capital's continent
    Continents,              // land of `count` continents revealed
    BarbarianKills,          // `count` barbarian units killed
    // Earned by an event as it happens (Game::eventBoost), never read from the state:
    KillWith,                // a unit `ref` (or its unique) kills a unit
    KillUnit,                // a unit `ref` (or its unique) killed
    ClearCamp,               // a barbarian camp cleared
    WarDeclaredOn,           // the target of a declaration of war
    CasusBelliWar,           // war declared with a casus belli
    Artifact,                // an artifact extracted
    NationalPark,            // a National Park designated
    NaturalWonder,           // a natural wonder discovered
    NotTracked,              // a condition the rules core cannot read
};

struct Boost {
    int percent = 0;  // 0: the node has no boost
    BoostKind kind = BoostKind::None;
    TypeIndex ref = kNone;
    TypeIndex improvement = kNone, resource = kNone;  // UnitAndImprovement
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
    int victoryPoints = 0;         // Diplomatic Victory points on completion (08: Diplomatic Victory)
    int navalMoves = 0;            // + movement for naval units (Mathematics)
    int urbanDefenseHp = 0;        // every city's outer defense is at least this (Steel's urban defenses)
    int writingTourismPercent = 0; // Writing Great Works' tourism scaled to this percent (Printing: 200)
    int tourismPercent = 0;        // + tourism percent (Computers, Environmentalism)
    std::vector<std::pair<TypeIndex, int>> buildingTourism;  // tourism from each city with the building (Conservation: walls, Arena)
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
    // A Dark Age card [R&F] (09: Ages): only in a Dark Age, while the world era is in [minEra, maxEra].
    bool darkAge = false;
    int minEra = 0, maxEra = 0;
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
    // Policy cards (04: Policies; generated in policies.json):
    TradeRouteYield,               // player: + `yield` on the player's trade routes of `scope` (ALL, DOMESTIC, INTERNATIONAL, ALLY, CITY_STATE, SUZERAIN)
    ItemProductionPercent,         // city: + % production toward `scope` (WONDERS in [minEra, maxEra], BUILDING, DISTRICT, DISTRICT_BUILDINGS, SPACE_RACE)
    GreatPersonPoints,             // player: + points a turn toward `gpClass`
    CityGreatPersonPoints,         // city: + points a turn toward `gpClass`
    FavorPerTurn,                  // player: + Diplomatic Favor a turn
    CityFavorPerTurn,              // city: + Diplomatic Favor a turn
    InfluencePerTurn,              // player: + influence points a turn toward envoys
    // Great people's lasting activation effects (07; generated in greatpeople.json):
    RouteTourismPercent,           // player: + % tourism toward civs it runs a trade route to
    DistrictTourism,               // player: + tourism from each of its completed `district`
    CityAppeal,                    // city: + appeal on its plots
    CityTourism,                   // city: + tourism (Shopping Mall, Ferris Wheel)
    EmbarkedMoves,                 // player: + movement for its embarked units (Great Lighthouse)
    // Governments (04; generated in governments.json):
    PurchaseDiscountPercent,       // player: % off what `yield` (GOLD or FAITH) buys (Theocracy, Democracy)
};
enum class ReqType : uint8_t {
    PlotHasResource = 0,
    PlotHasFeature,
    PlotHasTerrain,
    CityHasBuilding,
    CityIsCapital,
    CityHasDistrict,     // a completed district of `ref`
    CityHasGarrison,     // a military unit of the owner in the city center
    CityHasGovernor,     // an established governor with at least `value` titles (0: any)
    CityMinSpecialtyDistricts,  // at least `value` completed districts that count toward the population limit
    CityOnCapitalContinent,     // on the same landmass as the owner's capital
    CityCaptured,               // founded by another civ
    CityHasImprovedResource,    // a plot of the city holds resource `ref` under a working improvement
    CityMinTerrainTiles,        // at least `value` of the city's plots on terrain `ref` or its hills (Amundsen-Scott)
    PlotHasResourceClass,       // the plot holds a resource of class `value` (ResourceClass)
    CityDistrictNextToRiver,    // the city's completed district `ref` lies next to a river (River Goddess)
    PlayerAtPeace,              // the owner is at war with no major civ
    WorldMinEra,                // the world era is at least `value`
    CityMinPopulation,
    PlayerIsHuman,
    PlotHasImprovement,  // ref kNone: any improvement (PlotHasFeature likewise: any feature)
    PlotNextToRiver,
    PlotIsLake,      // 01: Lake (Huey Teocalli, Mausoleum)
    PlotNextToLake,  // a lake plot beside it (Aztec Chinampas)
    CityFullLoyalty,  // the city's loyalty is at LOYALTY_MAXIMUM (the Monument [R&F])
    CityIsCoastal,    // the city center lies beside Coast or a lake (the Lighthouse's Housing)
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
enum class ModSource : uint8_t { Building = 0, Civ, Everyone, Policy, Government, Belief, Governor, GreatPerson, CityState };

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
    int minEra = -1, maxEra = -1;
    TypeIndex ability = kNone;  // GrantAbility
    bool vsBarbarians = false;  // UnitStrength
    int per = 1;                // FounderYieldPerFollowers: followers per point
    bool foreign = false;       // UnitStrengthNearFollowingCity: foreign cities only (Crusade)
    TypeIndex district = kNone;  // DistrictAdjacencyPercent, ItemProductionPercent
    TypeIndex building = kNone;  // ItemProductionPercent
    TypeIndex gpClass = kNone;   // GreatPersonPoints, CityGreatPersonPoints
    std::string scope;           // TradeRouteYield, ItemProductionPercent
    bool military = false;       // UnitProductionPercent: military units only
    bool toDestination = false;  // TradeRouteYield: paid to the destination city instead of the origin
};

// City projects (03-districts-buildings-wonders.md, Projects; data: projects.md).
enum class ProjectEffectKind : uint8_t { RepairWalls = 0, Loyalty, Favor, RemoveCo2, RevealMap, CultureFromScience, ExpeditionSpeed, Wmd, Aid, Competition, Decommission, Festival, Recommission, Convert };
struct ProjectEffect {
    ProjectEffectKind kind = ProjectEffectKind::Loyalty;
    int amount = 0;
    TypeIndex weapon = kNone;  // Wmd: Rules::wmds; Decommission: the power plant (Rules::buildings);
                               // Competition: the CompetitionKind it scores
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
enum class DisasterKind : uint8_t { Flood = 0, Eruption, Blizzard, DustStorm, Tornado, Hurricane, Drought, Fire, Nuclear, Meteor };
enum class DisasterDamageType : uint8_t {
    ImprovementDestroyed = 0, ImprovementPillaged, PopulationLoss, CivilianKilled, UnitDamageLand, UnitDamageNaval, CityGarrison, CityWalls,
    DistrictPillaged, BuildingPillaged, BuildingDestroyed, Spread,
    FarmDestroyed, FarmPillaged,  // the data's specific improvement (droughts): Farms (09)
    Other,
};
struct DisasterDamage {
    DisasterDamageType type = DisasterDamageType::Other;
    int percent = 0, minHp = 0, maxHp = 0;
    int lowlandPercent = 0;  // the chance instead on a coastal lowland plot, when set (09: hurricanes)
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
    int minTurnAtRisk = 0, fallout = 0;  // Nuclear: a reactor's age before it is at risk; fallout turns
    int spacing = 0;                     // plots it keeps from a running storm (storms) or drought (droughts)
    TypeIndex naturalWonder = kNone;     // Eruption: only this volcano natural wonder erupts (Mount Vesuvius...)
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
    LuxuryPolicy, WorldReligion, HeritageOrganization, WorldIdeology, BorderControl, PublicWorks, GlobalEnergy, Sovereignty,
    DeforestationTreaty, EspionagePact, MercenaryCompanies, ArmsControl,
};
enum class ResolutionTarget : uint8_t {
    Player = 0, GreatPersonClass, District, PromotionClass, Resource, Religion, GreatWorkObject, Government, Project, Building,
    CityStateKind, Feature, SpyOperation, Yield, Other,
};
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

// A tribal village's reward (01: Tribal Villages; data: barbarians-goody-huts).
enum class GoodyKind : uint8_t { Relic = 0, Inspiration, Eureka, GovernorTitle, Envoy, Favor, Faith, Gold, Xp, Heal, Strategic, Tech, Population, Unit };
struct GoodyType {
    std::string id, category;
    int weight = 0;
    GoodyKind kind = GoodyKind::Gold;
    int amount = 0;
    TypeIndex unit = kNone;  // Unit: what appears in the nearest city
    std::string unitClass;   // Unit, when `unit` is kNone: the player's best unit of this class (Game::bestUnitOfClass)
    int minTurn = 0;
    bool needsCity = false;
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
    std::string text;  // what it does, in words (the setup screen; rules ignore it)
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
    Fixed freshWaterFarmHousing;        // per farm next to a river or lake
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
    bool builderRoads = false;          // its Builders lay roads by hand for no charge
    bool floodSafeDistricts = false;    // floods do not pillage its districts or buildings (Gift of the Nile)
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
    std::string uniquesText;     // its unique unit and building or improvement, in words (the setup screen)
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
    int scienceVictoryPercent = 100;  // of SCIENCE_VICTORY_POINTS_REQUIRED (Short Reign scales its victories)
};

// Named constants read on hot paths (Rules::global(HotGlobal)), each the one of the same name in capitals.
enum class HotGlobal : uint8_t {
    CitizenIdentityPressureRadiusCutoff = 0,
    CityFoodConsumptionPerPopulation,
    CityMinRange,
    CityPopulationCoast,
    CityPopulationNoWater,
    CityPopulationRiverLake,
    CitySightRange,
    CombatMaxNumAttacks,
    CulturePercentageYieldPerPop,
    DiplomaticVictoryPointsRequired,
    DistrictPopulationRequiredPer,
    InfluenceTokensMinimumForSuzerain,
    MovementEmbarkCost,
    MovementRiverCost,
    ReligionSpreadAtheismPressurePerPop,
    ReputationThreshold,
    SciencePercentageYieldPerPop,
    ScienceVictoryPointsRequired,
    TradingPostGoldInForeignCity,
    TradingPostGoldInOwnCity,
    WarWearinessPointsForAmenityLoss,
    YieldFoodCityTerrainReplace,
    YieldProductionCityTerrainReplace,
    Count
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
    std::vector<TypeIndex> navalMoveTechs;  // techs with navalMoves, found at load (maxMoves runs often)
    std::vector<TypeIndex> urbanDefenseTechs;  // techs with urbanDefenseHp
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
    std::vector<EraStartType> eraStarts;     // by era; Ancient has none
    std::vector<AchievementType> achievements;
    std::vector<CosmeticType> cosmetics;
    const EraStartType* eraStart(int era) const;
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
    std::vector<GoodyType> goodies;
    std::vector<ResolutionType> resolutions;
    std::vector<DisasterType> disasters;
    std::vector<ClimatePhaseType> climatePhases;
    std::vector<DisasterIntensityType> disasterIntensities;
    std::vector<std::string> promotionClasses;  // the unit promotion classes in use (Military Advisory targets)
    std::vector<GovernorPromotionType> governorPromotions;
    std::vector<std::pair<TypeIndex, int>> governorTitleCivics;  // civic, titles it grants
    std::vector<MomentType> moments;
    std::vector<DedicationType> dedications;
    std::vector<RockBandResult> rockBandResults;
    std::vector<EnvoyBonus> envoyBonuses;
    std::vector<ReligionType> religions;
    TypeIndex leaderUnit = kNone;  // the unit every major civ's leader is (layer Leader)

    TypeIndex terrain(const std::string& id) const;
    TypeIndex feature(const std::string& id) const;
    TypeIndex resource(const std::string& id) const;
    TypeIndex ability(const std::string& id) const;
    TypeIndex promotion(const std::string& id) const;
    TypeIndex leaningPromotion(TypeIndex civ) const;  // tier 1 of the SOVEREIGN branch its leader leans to, or kNone
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
    // Indices into `modifiers` with this effect, in load order: the player-collection ones, and the city and plot ones.
    // Built when the rules load; call indexModifiers() again after changing `modifiers`.
    const std::vector<uint32_t>& playerModifiers(ModEffect effect) const;
    const std::vector<uint32_t>& cityModifiers(ModEffect effect) const;
    // The same city modifiers split by whether a policy or a government brings them: those neither brings, by effect;
    // each policy's, of every effect (null for an index the rules lack); and each government's by effect (null for an
    // index the rules lack or an effect it has none of). A pass over a city's modifiers looks at a policy's only when
    // the city's owner has slotted it, and at a government's only while the owner has adopted it. Built by
    // indexModifiers().
    const std::vector<uint32_t>& cityModifiersBesidePolicies(ModEffect effect) const;
    const std::vector<uint32_t>* policyCityModifiers(TypeIndex policy) const;
    const std::vector<uint32_t>* governmentCityModifiers(TypeIndex government, ModEffect effect) const {
        if (government < 0 || static_cast<size_t>(government) >= governmentCityMods_.size()) return nullptr;
        const std::vector<std::vector<uint32_t>>& byEffect = governmentCityMods_[static_cast<size_t>(government)];
        const size_t e = static_cast<size_t>(effect);
        return e < byEffect.size() && !byEffect[e].empty() ? &byEffect[e] : nullptr;
    }
    // The city modifiers of plot yields, each listed once: under the improvement, resource, feature or terrain (in
    // that order of preference) its subject requirements all need, or as unkeyed when they need none of those. Only a
    // plot's own lists and the unkeyed one can hold modifiers that apply to it. Built by indexModifiers().
    struct PlotModifiers {
        std::vector<uint32_t> unkeyed;
        std::vector<std::vector<uint32_t>> byImprovement, byResource, byFeature, byTerrain;
    };
    const PlotModifiers& plotYieldModifiers() const { return plotYieldMods_; }
    void indexModifiers();
    TypeIndex civ(const std::string& id) const;
    TypeIndex gearType(const std::string& id) const;
    TypeIndex greatPersonClass(const std::string& id) const;
    TypeIndex greatPerson(const std::string& id) const;
    TypeIndex cityState(const std::string& id) const;
    TypeIndex greatWorkType(const std::string& id) const;
    TypeIndex belief(const std::string& id) const;
    TypeIndex religion(const std::string& id) const;
    TypeIndex moment(const std::string& id) const;
    TypeIndex governor(const std::string& id) const;
    TypeIndex dedication(const std::string& id) const;
    TypeIndex spyOperation(const std::string& id) const;
    TypeIndex resolution(const std::string& id) const;
    TypeIndex project(const std::string& id) const;
    TypeIndex wmd(const std::string& id) const;
    TypeIndex governorPromotion(const std::string& id) const;
    // The civ's dynasty, or null when it has none.
    const Dynasty* dynastyOf(TypeIndex civ) const;
    TypeIndex mapSize(const std::string& id) const;
    // The civ's unique unit or building replacing `base` (kNone: none, `base` itself stays).
    TypeIndex uniqueUnitFor(TypeIndex civ, TypeIndex base) const;
    TypeIndex uniqueBuildingFor(TypeIndex civ, TypeIndex base) const;
    // The civ uniques replacing this building or unit (those whose `replaces` it is), in index order; null for an
    // index the rules lack. Built when the rules load; call indexUniques() again after changing a `replaces`.
    const std::vector<TypeIndex>* buildingsReplacing(TypeIndex building) const { return replacing(buildingsReplacing_, buildings.size(), building); }
    const std::vector<TypeIndex>* unitsReplacing(TypeIndex unit) const { return replacing(unitsReplacing_, units.size(), unit); }
    void indexUniques();
    TypeIndex speed(const std::string& id) const;
    // Terrain with this climate base and relief, or kNone.
    TypeIndex terrainFor(const std::string& base, Relief relief) const;

    // Named constants (GlobalParameters-style). Missing names are a load error
    // when required through requireGlobals().
    Fixed global(std::string_view name) const;
    int globalInt(std::string_view name) const { return static_cast<int>(global(name).toInt()); }
    bool hasGlobal(std::string_view name) const { return findGlobal(name) != nullptr; }
    // The same for a constant read on a hot path, found by its name once at load rather than at every call.
    Fixed global(HotGlobal g) const { return hotGlobals_[static_cast<size_t>(g)]; }
    int globalInt(HotGlobal g) const { return static_cast<int>(global(g).toInt()); }
    // Its name, as global(name) takes it.
    static std::string_view hotGlobalName(HotGlobal g);

    // Checksum of the loaded rules; saves and multiplayer peers must match.
    uint64_t checksum() const { return checksum_; }

    // The names of the rules files the loader reads, in load order.
    static const std::vector<std::string>& fileNames();

private:
    // The named constants, found through a table of their names' hashes: a lookup hashes the name once and compares
    // one name, where a tree of names compares strings at every level.
    struct Global {
        uint64_t hash;
        std::string name;
        Fixed value;
    };
    std::vector<Global> globals_;
    std::vector<uint32_t> globalSlots_;  // open addressing, a power of two in size and at most half full: 1 + an index in globals_, 0 empty
    const Global* findGlobal(std::string_view name) const;
    std::array<Fixed, static_cast<size_t>(HotGlobal::Count)> hotGlobals_{};  // by HotGlobal: global(hotGlobalName(g))
    uint64_t checksum_ = 0;
    std::vector<std::vector<uint32_t>> playerModsByEffect_, cityModsByEffect_;  // by ModEffect (indexModifiers)
    std::vector<std::vector<uint32_t>> cityModsBesidePolicies_;                  // by ModEffect (indexModifiers)
    std::vector<std::vector<uint32_t>> policyCityMods_;                          // by policy (indexModifiers)
    std::vector<std::vector<std::vector<uint32_t>>> governmentCityMods_;         // by government, then ModEffect (indexModifiers)
    PlotModifiers plotYieldMods_;
    std::vector<std::vector<TypeIndex>> buildingsReplacing_, unitsReplacing_;  // by building and unit (indexUniques)
    static const std::vector<TypeIndex>* replacing(const std::vector<std::vector<TypeIndex>>& by, size_t count, TypeIndex i) {
        return by.size() == count && i >= 0 && static_cast<size_t>(i) < count ? &by[static_cast<size_t>(i)] : nullptr;
    }
};

SOV_API uint64_t fnv1a(const void* data, size_t size, uint64_t h = 0xCBF29CE484222325ull);

}  // namespace sov
