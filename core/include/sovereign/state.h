// Complete game state. Everything the rules read lives here and is saved;
// nothing outside it may influence a rules decision.
#pragma once

#include "sovereign/api.h"

#include <array>
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
    int8_t route = -1;  // Rules::routes: the road on this plot (-1: none)
    bool routePillaged = false;  // pillaged (05: Pillage): moves as if it had no road until repaired
    uint8_t pillagedTurns = 0;  // the improvement yields nothing until repaired (a disaster pillaged it)
    std::array<int8_t, kNumYields> fertility{};  // yields a disaster left behind (09: Climate and Disasters)
    uint8_t fallout = 0;  // turns of nuclear contamination left (05: Nuclear weapons): not worked, units take damage
    bool village = false; // a tribal village (01: Tribal Villages), consumed by the first unit of a civ to enter
    uint8_t antiquity = 0; // 1 an antiquity site, 2 a shipwreck (07: Archaeology)
    bool park = false;     // part of a National Park (07: National Parks)
};

enum class Activity : uint8_t { Awake = 0, Sleep, Fortify, Skip };
enum class Age : uint8_t { Normal = 0, Golden, Dark, Heroic };

struct Unit {
    UnitId id = kNoUnit;
    TypeIndex type = kNone;
    PlayerId owner = kNoPlayer;
    Hex pos;
    int hp = 100;
    Fixed movesLeft;
    Activity activity = Activity::Awake;
    std::optional<Hex> moveTarget;  // multi-turn move order
    TypeIndex greatPerson = kNone;  // the individual a great person unit is (07)
    int16_t religion = -1;          // a religious unit's religion (GameState::religions index)
    bool moveOverland = false;      // the order keeps a land unit on land (no embarking)
    int xp = 0;
    int charges = 0;  // build charges left (Builders)
    std::vector<TypeIndex> promotions;  // in the order taken; level = 1 + count
    int fortifyTurns = 0;  // turns spent fortified (05: +3 per turn, max 2)
    int attacks = 0;       // attacks made this turn
    bool moved = false;    // moved this turn (no healing; siege cannot fire)
    bool attacked = false; // attacked this turn (no healing)
    int32_t camp = 0;      // barbarian camp id that spawned it (0: none)
    // The leader's loadout per GearSlot (kNone: empty); unused by other units.
    std::array<TypeIndex, kNumGearSlots> gear{{kNone, kNone, kNone}};
    UnitId escorting = kNoUnit;  // military unit linked to this leader; moves with it while they share a plot
    uint8_t formation = 0;       // 0 single, 1 Corps/Fleet, 2 Army/Armada (05: Formations)
    // Permanent abilities from natural wonders (01): bit 0 Everest (hills cost as flat ground), bit 1
    // Fountain of Youth (+10 HP healing a turn), bit 2 Bermuda Triangle (+1 movement).
    uint8_t wonderAbilities = 0;

    int level() const { return 1 + static_cast<int>(promotions.size()); }
};

enum class ProductionKind : uint8_t { Unit = 0, Building = 1, District = 2, Project = 3 };

struct ProductionItem {
    ProductionKind kind = ProductionKind::Unit;
    TypeIndex type = kNone;
    uint8_t formation = 0;  // a unit trained as a Corps/Fleet (1) or an Army/Armada (2) (05: Corps and Armies)
    bool operator==(const ProductionItem& o) const { return kind == o.kind && type == o.type && formation == o.formation; }
    // Commands carry the kind and the formation together in one argument.
    int32_t packedKind() const { return static_cast<int32_t>(kind) | (static_cast<int32_t>(formation) << 4); }
};

// Production already put into an item; kept when the player switches away.
struct ProductionProgress {
    ProductionItem item;
    Fixed amount;
};

// A specialty district placed by a city (03-districts-buildings-wonders.md). Its plot is
// reserved from placement on; it works once production completes it.
struct CityDistrict {
    TypeIndex type = kNone;  // Rules::districts
    Hex pos;
    bool complete = false;
    uint8_t pillagedTurns = 0;  // pillaged (05: Pillage): no adjacency, its buildings idle, until repaired
    uint8_t specialists = 0;    // citizens working here as specialists (02: Citizens and specialists)
    int16_t damage = 0;         // Encampment (05: City combat): hit points lost
    int16_t wallDamage = 0;     // Encampment: outer defences lost
};

// Sovereign reading: a city repairs a pillaged district itself in this many turns.
constexpr uint8_t kPillagedDistrictTurns = 10;

// ---- diplomacy (08: Diplomatic actions; leader doc §10, language-model diplomacy)
// What one side of a deal gives. Friendship and Peace bind both sides; `from` is either.
enum class DealItemKind : uint8_t { Gold = 0, GoldPerTurn, Resource, OpenBorders, Friendship, Peace, Alliance, GreatWork, Captive, JointWar };
constexpr int kNumDealItemKinds = 10;
// Alliance types [R&F] (08: Alliance); a DealItemKind::Alliance item carries one as its amount.
enum class AllianceType : int8_t { None = -1, Research = 0, Military, Economic, Cultural, Religious };
constexpr int kNumAllianceTypes = 5;
struct DealItem {
    DealItemKind kind = DealItemKind::Gold;
    PlayerId from = kNoPlayer;
    int32_t amount = 0;         // gold, gold per turn, or strategic copies per turn; GreatWork: the city holding it; Captive: the spy's id; JointWar: the target
    TypeIndex resource = kNone; // Resource: a luxury (access) or strategic resource; GreatWork: its index in that city
};
// A deal one player put to another; it waits here only while a human must answer.
struct Deal {
    int32_t id = 0;
    PlayerId from = kNoPlayer, to = kNoPlayer;
    int32_t turn = 0;
    std::vector<DealItem> items;
};
// A running term of an accepted deal: gold or a resource each turn until `until`.
struct Agreement {
    DealItemKind kind = DealItemKind::GoldPerTurn;  // GoldPerTurn or Resource
    PlayerId from = kNoPlayer, to = kNoPlayer;
    int32_t amount = 0;
    TypeIndex resource = kNone;
    int32_t until = 0;  // last turn it runs
};
// Something a civ remembers about another; its weight fades to nothing over `duration` turns.
enum class MemoryKind : uint8_t {
    DeclaredWar = 0, SurpriseWar, Denounced, MadePeace, Gift, Deal, BrokeDeal, CapturedCity, Assassin, PlunderedTrader, Warmonger, SpyCaught,
    UsedWmd,
};
struct OpinionMemory {
    PlayerId about = kNoPlayer;
    MemoryKind kind = MemoryKind::Deal;
    int16_t amount = 0;
    int16_t duration = 30;
    int32_t turn = 0;
};
// Why one civ feels as it does about another (shown to the player; fed to the dialogue layer).
enum class OpinionReasonKind : uint8_t {
    AtWar = 0, DeclaredWar, SurpriseWar, DenouncedUs, WeDenounced, Friends, OpenBorders, SameReligion, ConvertingUs,
    TradeRoutes, MadePeace, Gifts, Deals, BrokeDeal, CapturedCity, Assassin, PlunderedTrader, Warmonger, Agenda, SpyCaught, Grievances,
    UsedWmd,
};
struct OpinionReason {
    OpinionReasonKind kind = OpinionReasonKind::Agenda;
    int value = 0;
};
// A conversation's summary, recorded by the speaking player's machine (leader doc §10, Sync)
// so every machine shares the leader's memory of past talks.
struct TalkRecord {
    int32_t turn = 0;
    PlayerId speaker = kNoPlayer, leader = kNoPlayer;  // who spoke, and the civ spoken to
    std::string text;
};
constexpr size_t kMaxTalkText = 400;  // characters kept per summary
constexpr int kTalksKept = 6;         // summaries kept per pair

// Relationship states (08: Meeting and relationship states; Allied waits for alliances).
enum class Relationship : uint8_t { AtWar = 0, Denounced, Unfriendly, Neutral, Friendly, DeclaredFriend };

// An appointed governor (08: Governors [R&F]).
struct Governor {
    TypeIndex type = kNone;              // Rules::governors
    CityId city = kNoCity;               // where it serves (a city-state's city for Amani); kNoCity: unassigned
    int establishTurns = 0;              // turns until its abilities work there (0: established)
    std::vector<TypeIndex> promotions;   // Rules::governorPromotions it holds (the base ability first)
};

// The World Congress (08: Diplomatic Favor and World Congress [GS]). A session puts two
// resolutions to the vote; each voter picks option A or B and a target, with one free vote
// and more bought with favor. The winners hold until the next session.
struct CongressVote {
    PlayerId player = kNoPlayer;
    uint8_t option = 0;   // 0: A, 1: B
    int32_t target = 0;   // index into the item's candidates
    int32_t votes = 1;
};
struct CongressItem {
    TypeIndex resolution = kNone;
    std::vector<int32_t> candidates;  // players, great person classes, districts or promotion classes
    std::vector<CongressVote> votes;
};
struct PassedResolution {
    TypeIndex resolution = kNone;
    uint8_t option = 0;
    int32_t target = 0;   // the chosen candidate itself (a player id, a type index...)
};

// A trade route (07: Trade routes): the Trader travels it until it ends, then returns home.
struct TradeRoute {
    int32_t id = 0;
    PlayerId owner = kNoPlayer;
    CityId origin = kNoCity, destination = kNoCity;
    TypeIndex traderType = kNone;
    std::vector<int32_t> path;  // plot indices from origin to destination
    int turnsLeft = 0;
};

// A founded religion (06: Founding a religion).
struct FoundedReligion {
    TypeIndex type = kNone;     // Rules::religions
    PlayerId founder = kNoPlayer;
    CityId holyCity = kNoCity;
    std::vector<TypeIndex> beliefs;  // the founder's pantheon, then Founder, Follower, Worship, Enhancer
};

// A hostile emergency [R&F/GS] (08: Emergencies): civs join against the target to undo what it did.
enum class EmergencyKind : uint8_t { Military = 0, CityState, Religious, Nuclear, Betrayal };
constexpr int kNumEmergencyKinds = 5;
struct Emergency {
    EmergencyKind kind = EmergencyKind::Military;
    PlayerId target = kNoPlayer;
    CityId city = kNoCity;          // Military/CityState: the city taken; Religious: the Holy City
    int32_t religion = -1;          // Religious: the Holy City's own religion (GameState::religions)
    int32_t endTurn = 0;            // the goal must be met by then
    std::vector<uint8_t> members;   // per player: joined
    uint8_t outcome = 0;            // 0 running, 1 members succeeded, 2 failed (target rewarded)
};

// A promise one civ made another (08: Ask Promise [GS]): kept for 30 turns, or broken by doing the deed.
enum class PromiseKind : uint8_t { NoSettling = 0, NoConverting, NoSpying, NoDigging };
constexpr int kNumPromiseKinds = 4;
struct Promise {
    PlayerId by = kNoPlayer, to = kNoPlayer;  // who promised, who asked
    PromiseKind kind = PromiseKind::NoSettling;
    int32_t until = 0;
    int32_t brokenOn = 0;  // turn it was broken (0: kept); a War of Retribution is open for 30 turns after
};

// A city-state's quest for one major civ (08: Quests; data: diplomacy-espionage, City-state quests):
// fulfilled, it puts an envoy in that city-state.
enum class QuestKind : uint8_t { Convert = 0, TradeRoute, ClearCamp, TrainUnit, BuildDistrict, Eureka, Inspiration, GreatPerson };
constexpr int kNumQuestKinds = 8;
struct Quest {
    PlayerId cityState = kNoPlayer, major = kNoPlayer;
    QuestKind kind = QuestKind::TrainUnit;
    int32_t arg = -1;   // the unit, district, tech, civic, great person class or camp id it names
};

// A scored competition [GS] (08: Scored Competitions; data: world-congress-emergencies), called at a
// World Congress session; every major civ takes part.
enum class CompetitionKind : uint8_t { WorldsFair = 0, WorldGames, NobelLiterature, NobelPeace, NobelPhysics, ClimateAccords, SpaceStation, AidRequest, MilitaryAidRequest };
constexpr int kNumCompetitionKinds = 9;
struct Competition {
    CompetitionKind kind = CompetitionKind::WorldsFair;
    int32_t endTurn = 0;
    std::vector<int32_t> scores;    // per player
    std::vector<int64_t> baseline;  // per player: favor (Peace) or CO2 (Climate Accords) when it began
    bool settled = false;
    PlayerId beneficiary = kNoPlayer;  // Aid Request: the civ struck by the disaster
};

// A wonder's plot: reserved when its production starts, its own tile once built.
struct CityWonder {
    TypeIndex building = kNone;
    Hex pos;
};

// A Great Work in one of a city's building slots.
struct GreatWork {
    TypeIndex type = kNone;      // Rules::greatWorkTypes
    TypeIndex building = kNone;  // the building whose slot holds it
    TypeIndex creator = kNone;   // Rules::greatPeople
    int8_t era = -1;             // an Artifact's era (Rules::eras) and civilization (Rules::civs) (07: Theming)
    TypeIndex civ = kNone;
};

struct SOV_API City {
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
    int hp = 0;              // city center hit points (0: not yet set; full on founding)
    int wallHp = 0;          // outer defence hit points left
    int lastAttackedTurn = -100;
    bool struck = false;     // made its ranged strike this turn
    bool encampmentStruck = false;  // its Encampment made its strike this turn (03: Defense)
    PlayerId originalOwner = kNoPlayer;
    bool originalCapital = false;  // founded as its owner's capital (cannot be razed)
    int capturedTurn = -1;         // turn it last changed hands (raze is allowed that turn)
    std::vector<CityDistrict> districts;  // in placement order
    int loyalty = 100;             // 0..LOYALTY_MAXIMUM [R&F]; at 0 the city revolts to the Free Cities
    // The leader's citizen stances (leader doc §4): turns until which each effect lasts.
    int stanceTurn = -100;         // turn of the last stance (cooldown)
    int benevolenceUntil = 0;      // +amenities while turn < this
    int fearUntil = 0;             // order imposed while turn < this
    int fearAfterUntil = 0;        // resentment (-amenity, assassin openings) while turn < this
    std::vector<GreatWork> greatWorks;
    std::vector<TypeIndex> greatPeopleHere;  // individuals used here whose lasting effects apply to this city (07)
    int32_t reactorSince = 0;  // the turn its Nuclear Power Plant was built or recommissioned (09: nuclear accidents)
    int laserStations = 0;     // Terrestrial Laser Stations built here: 5 Power of demand each (09: Power)  // in the city's buildings' slots (07: Great Works)
    std::vector<int32_t> pressure;      // per founded religion (06: Spread mechanics)
    std::vector<CityWonder> wonders;    // wonder plots, reserved when building starts (03: Wonders)
    int powerDemand = 0, powerSupply = 0;  // this turn's power [GS] (09: Power), set as its owner's turn begins

    bool has(TypeIndex building) const;
    // The city's district of this type, if placed (and, with completeOnly, finished).
    const CityDistrict* district(TypeIndex type, bool completeOnly) const;
};

// The city has the building, or a civ unique that replaces it (leaders-and-art-style).
SOV_API bool cityHasBuilding(const City& city, const Rules& rules, TypeIndex building);

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

// One player's standing towards another (diplomacy-espionage.md; war and peace only for now).
struct Relation {
    bool war = false;
    int32_t since = 0;          // turn the current war or peace began (0: never at war)
    bool peaceOffered = false;  // this player offers peace; peace comes when both sides offer
    int32_t denouncedOn = 0;      // turn this player denounced that one (0: not denouncing)
    int32_t friendsUntil = 0;     // a declaration of friendship runs through this turn (both sides)
    int32_t openBordersUntil = 0; // this player opens its borders to that one through this turn
    int32_t lastProposal = 0;     // turn this player last put a deal to that one (AI pacing)
    AllianceType alliance = AllianceType::None;  // an alliance [R&F] running through allianceUntil (both sides)
    int32_t allianceUntil = 0;
    int32_t alliancePoints = 0;   // toward levels 2 and 3 (internal units, ALLIANCE_POINTS_MULTIPLIER a turn)
    uint8_t delegation = 0;       // this player keeps 1 a delegation, 2 a resident embassy, with that one (08)
};

struct Player {
    PlayerId id = kNoPlayer;
    TypeIndex civ = kNone;
    bool human = false;
    bool alive = true;
    bool barbarian = false;   // the barbarian player: at war with all, plays in the world turn
    bool freeCity = false;    // the Free Cities (also flagged barbarian: not a major, at war with all, takes no turns)
    int reputation = 0;       // -100 Feared .. +100 Beloved (leader doc §8.1)
    int strongestUnit = 0;    // highest melee strength of any unit it has had (city defence)
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
    std::vector<int> greatPersonPoints;      // per great person class (07)
    std::vector<int> projectsDone;           // per Rules::projects: times completed (03: Projects)
    std::vector<int> greatPeopleRecruited;   // per class
    std::vector<TypeIndex> greatPeoplePassed;     // individuals this player declined
    std::vector<TypeIndex> greatPeopleActivated;  // individuals it has used (their lasting effects apply)
    TypeIndex pantheon = kNone;   // Rules::beliefs (06: Pantheon)
    TypeIndex cityState = kNone;  // Rules::cityStates: a city-state (one city, no expansion; 08)
    std::vector<int> envoys;      // per player: envoys this player sent to that city-state
    int envoyTokens = 0;          // envoys waiting to be sent
    int killsThisEra = 0;         // enemy units destroyed since the world era began (Flower Wars)
    int influence = 0;            // points toward the next envoys
    PlayerId firstMetBy = kNoPlayer;  // a city-state: the first major civ to meet it (gets an envoy)
    bool hadSuzerain = false;         // a city-state: someone has been its suzerain
    // Ages (09: Era score and Ages [R&F]).
    int eraScore = 0;               // this era's score so far
    Age age = Age::Normal;          // the age set when the world entered this era
    std::vector<TypeIndex> dedications;  // Rules::dedications chosen for this era (09: Dedications)
    int dedicationsPending = 0;          // still to choose this era
    int pastGoldenAges = 0, pastDarkAges = 0;
    std::vector<int8_t> momentEras; // per moment: 1 + the world era it was last earned in (0: never)
    std::vector<uint8_t> met;       // per player: met (seen one of its cities or units)
    // Tourism and the culture victory (07: Tourism and Culture Victory).
    Fixed lifetimeCulture;
    std::vector<int32_t> tourismTo;  // per player: lifetime tourism toward that civ
    int16_t religion = -1;        // the religion it founded (GameState::religions index)
    std::vector<uint8_t> fuelShort; // per resource: unit maintenance went unpaid this turn [GS]
    std::vector<Relation> relations;  // per player
    std::vector<OpinionMemory> memories;  // what this player remembers of others (diplomacy)
    std::vector<Governor> governors;      // appointed governors (08)
    std::vector<int32_t> grievances;      // per player: grievances this player holds against it [GS]
    int favor = 0;                        // Diplomatic Favor [GS]
    int64_t co2 = 0;                      // CO2 it has emitted [GS]
    int diplomaticVictoryPoints = 0;      // [GS]
    int lightYears = 0;                   // the exoplanet expedition's distance travelled (09: Science victory)
    std::vector<int32_t> wmds;            // devices held, by Rules::wmds (05: Nuclear weapons)
    std::vector<int32_t> warWeariness;    // per other player: war weariness points against it (08: War weariness)
    int wmdsLaunched = 0;
    int governorTitlesSpent = 0;          // titles used on appointments and promotions
    // Deeds every civ hears of (agendas weigh them).
    int warsDeclared = 0, surpriseWars = 0, citiesCaptured = 0, citiesRazed = 0, tradersPlundered = 0, assassinsSent = 0;
    std::vector<uint8_t> visibility;  // Visibility per plot index
    Hex startPos;
    // The throne (leader doc §5). An interregnum empties policy slots for a few turns after
    // the leader falls; it counts down only while someone sits on the throne.
    std::string leaderName;
    int dynastyNext = 1;            // next heir in the civ's dynasty (0 is the starting leader)
    int rulingHeir = 0;             // the dynasty member on the throne (0: the starting leader; -1: not of the dynasty)
    bool successionPending = false; // the leader died or was abandoned: a successor must be chosen
    int interregnumTurns = 0;
    PlayerId captor = kNoPlayer;    // holds this player's captured leader
    std::array<TypeIndex, kNumGearSlots> savedGear{{kNone, kNone, kNone}};  // the fallen leader's loadout
    std::vector<TypeIndex> savedPromotions;  // the fallen leader's; an heir keeps one
};

// How a major civ plays (leader doc §10, AI layer 2: player modelling), built each world turn
// from public facts. Shares and indexes are x1000 and fade toward recent play; counts only grow.
enum class ProfileClass : uint8_t { Melee = 0, Ranged, AntiCavalry, LightCavalry, HeavyCavalry, Siege, Naval, Other };
constexpr size_t kNumProfileClasses = 8;
SOV_API ProfileClass profileClassOf(const std::string& promotionClass);
struct PlayerProfile {
    int32_t turnsObserved = 0;
    std::array<int32_t, kNumProfileClasses> army{};  // share of its military strength by class
    int32_t militarism = 0;     // strength per city against the world's (1000 = average)
    int32_t expansion = 0;      // cities against the average major (1000 = average)
    int32_t science = 0, culture = 0, faith = 0;  // shares of its science + culture + faith
    int32_t aggression = 0;     // wars declared (spikes) and army camped near others' cities in peace
    int32_t leaderOutside = 0;  // share of turns its leader spends outside its cities
    int32_t leaderExposed = 0;  // share of turns its leader is open to assassins (§6)
    int32_t warsDeclared = 0, surpriseWars = 0;  // counts
    int32_t citiesHeld = 0;     // cities it holds that another major founded
    // Live battles it fought by hand (each moves these a quarter of the way): shares of its squads'
    // time flanking, falling back and hunting the enemy leader, and of the battle its leader fought in front.
    int32_t battles = 0;
    int32_t battleFlank = 0, battleFallBack = 0, battleHunt = 0, battleLeaderFront = 0;
};

struct PlayerSetup {
    std::string civ;
    bool human = false;
    // A human's profile carried from earlier games (leader doc §10: it persists between games).
    bool hasProfile = false;
    PlayerProfile profile;
};

struct GameSetup {
    uint64_t seed = 1;
    std::string mapSize = "MAPSIZE_DUEL";
    std::string speed = "GAMESPEED_STANDARD";
    bool wrapX = true;
    std::vector<PlayerSetup> players;
    bool barbarians = true;
    bool tribalVillages = true;    // 01: Tribal Villages
    // Victories (09-civs-eras-victory-climate.md, Victory conditions). With Domination
    // off, the last major civ standing wins instead (VICTORY_DEFAULT).
    bool dominationVictory = true;
    bool scoreVictory = true;
    bool religiousVictory = true;  // 06: Religious victory
    bool cultureVictory = true;    // 07: Tourism and Culture Victory
    bool diplomaticVictory = true; // 08: Diplomatic Victory [GS]
    bool scienceVictory = true;    // 09: Science victory [GS] (the exoplanet expedition)
    int disasterIntensity = 2;     // 0 Minimal .. 4 Hyperreal; -1: no natural disasters (09 [GS])
    int difficulty = 3;            // Rules::difficulties: 0 Settler .. 3 Prince .. 7 Deity
    int cityStates = -1;           // city-states to place (-1: the map size's default)
    int turnLimit = 0;  // last turn played before Score decides; 0: the game speed's calendar
    // Melee involving a human's leader stack can be fought as a live battle (leader doc §9);
    // off in headless games, on in the Unreal front end.
    bool liveBattles = false;
    bool regicide = false;  // optional mode: losing the leader eliminates you (leader doc §5)
};

enum class Victory : uint8_t { None = 0, Domination, Score, LastStanding, Religious, Culture, Diplomatic, Science };

// An off-map agent (leader doc §6): an assassin sent after another civ's leader. It travels
// for a few turns, then strikes when the target leader is exposed.
// What a spy is doing (08: Espionage). Counterspy and Listening Post go on until changed;
// Gain Sources and the offensive operations end after their turns.
enum class SpyMission : uint8_t {
    None = 0, Counterspy, ListeningPost, GainSources, SiphonFunds, StealTechBoost, SabotageProduction, NeutralizeGovernor, FomentUnrest,
    GreatWorkHeist, RecruitPartisans, BreachDam, DisruptRocketry, FabricateScandal,
};
constexpr int kNumSpyMissions = 14;

struct Agent {
    int32_t id = 0;
    PlayerId owner = kNoPlayer;
    int level = 1;               // Recruit 1, Agent 2, Secret Agent 3, Master 4
    PlayerId target = kNoPlayer; // assassins: the ruler hunted (kNoPlayer: idle at home)
    int travel = 0;              // turns until it is in place
    // Spies (08: Espionage).
    bool spy = false;
    CityId city = kNoCity;       // the city it works in (kNoCity: at home)
    SpyMission mission = SpyMission::None;
    int missionTurns = 0;        // turns left on an operation that ends
    CityId sourcesCity = kNoCity;  // Gain Sources: +2 levels on operations here until sourcesUntil
    int32_t sourcesUntil = 0;
    std::vector<TypeIndex> promotions;  // Rules::spyPromotions it holds
    int promotionsPending = 0;          // levels gained and not yet spent on a promotion
};

// A spy caught at work, held by the civ that caught it until it is traded back (08: Outcomes; Captive deal item).
struct CapturedSpy {
    Agent spy;
    PlayerId captor = kNoPlayer;
};

// A melee waiting for its live battle (leader doc §9, battle result contract). The core
// has worked out the expected Civ result; a BattleResult command (clamped to the band)
// or AutoResolveBattle settles it. Nothing else may happen meanwhile.
struct PendingBattle {
    bool active = false;
    UnitId attacker = kNoUnit, defender = kNoUnit;  // defender: kNoUnit when the target is a city
    CityId city = kNoCity;                          // a city assault (§9: a city holding the leader)
    Hex target;
    PlayerId liveFor = kNoPlayer;   // the human whose leader is in the fight
    UnitId leader = kNoUnit;        // that leader
    int expectedToDefender = 0, expectedToAttacker = 0;  // HP damage at the middle roll
};

// Things that happened that players should hear about (UI and AI read them; rules do not).
enum class EventKind : uint8_t {
    AssassinKilledLeader = 1, AssassinWoundedLeader, AssassinKilled, AssassinCaptured, Rebellion, GreatPersonRecruited,
    HistoricMoment,  // value: the moment
    NewAge,          // value: the Age the player entered with the new era
    DealProposed,    // actor proposed to target; value: the deal id (in GameState::deals)
    DealAccepted,    // actor's deal was accepted by target
    DealRejected,    // actor's deal was turned down by target
    Denounced,       // actor denounced target
    FriendshipDeclared,
    WarDeclared,     // value: 1 for a surprise war
    PeaceMade,
    DealBroken,      // actor could not pay what it owed target
    SpyOperation,    // actor's spy succeeded against target; value: the SpyMission (a detected or a known one)
    SpyCaught,       // target caught actor's spy; value: 1 when it escaped
    CongressSession, // the World Congress opens a session
    ResolutionPassed,  // value: resolution; target: the candidate it applies to when that is a player
    Disaster,        // value: disaster type; target: the owner of the plot it struck (kNoPlayer: unowned)
    ClimatePhase,    // value: the phase the world entered
    GoodyHut,        // actor entered a tribal village; value: the reward (Rules::goodies)
};
struct GameEvent {
    int32_t turn = 0;
    EventKind kind = EventKind::AssassinKilled;
    PlayerId actor = kNoPlayer;   // who sent the assassin
    PlayerId target = kNoPlayer;  // whose leader was the target
    int32_t value = 0;            // damage dealt, when any
};

// A barbarian camp (01-map-and-terrain.md, Barbarians; barbarians-goody-huts.md).
struct Camp {
    int32_t id = 0;
    Hex pos;
    int boldness = 0;
    int spawnTimer = 0;  // turns until it releases its next unit
    TypeIndex tribe = kNone;  // Rules::barbarianTribes
    // Scouting (01: Barbarians): a new camp sends out a Scout; once it has seen a city and come home the
    // camp is alerted and raids. Camps made outside placeCamps (scenarios, tests) start alerted.
    bool alerted = true;
    bool scoutSaw = false;  // its scout has seen a city and is on its way home
};

struct SOV_API GameState {
    GameSetup setup;
    int turn = 1;
    PlayerId currentPlayer = 0;
    HexGrid grid;
    std::vector<Plot> plots;
    std::vector<Player> players;
    std::vector<Unit> units;    // sorted by id
    std::vector<City> cities;   // sorted by id
    std::vector<Camp> camps;    // sorted by id
    std::vector<Agent> agents;  // sorted by id
    std::vector<CapturedSpy> capturedSpies;  // in the order they were caught
    std::vector<uint8_t> greatPeopleClaimed;  // per individual: recruited by someone
    std::vector<FoundedReligion> religions;   // in founding order
    std::vector<TradeRoute> tradeRoutes;      // sorted by id
    int gameEra = 0;                          // the world era (09: Global era transitions)
    int gameEraStart = 1;                     // turn it began
    std::vector<int8_t> worldMoments;         // per moment: 1 + the era it was claimed for (world's firsts)
    int majorsAtStart = 0;                    // tourism divisor (07: Visiting tourists)
    int32_t nextTradeRouteId = 1;
    std::vector<Deal> deals;            // proposals waiting for a human's answer
    std::vector<Agreement> agreements;  // running deal terms
    std::vector<Emergency> emergencies; // hostile emergencies, running and settled (08: Emergencies)
    std::vector<Competition> competitions;  // scored competitions, running and settled (08 [GS])
    std::vector<Quest> quests;              // open city-state quests, one per city-state and major (08)
    std::vector<int32_t> battleSites;       // plots fought over before ARCHAEOLOGY_MAX_ERA (07: Archaeology)
    std::vector<int32_t> battleHistory;     // per battle site: era * 4096 + the attacker's civ + 1 (its Artifact's history)
    std::vector<Promise> promises;          // promises made, kept and broken (08 [GS])
    bool antiquityPlaced = false;           // the sites have appeared (once a civ has Natural History)
    int32_t nextDealId = 1;
    std::vector<TalkRecord> talks;      // conversation summaries, oldest first
    int64_t co2 = 0;                    // CO2 in the atmosphere from every civ [GS]
    int climatePhase = 0;               // 0: none yet, 1..7 (09: Climate Phases)
    struct Drought {
        Hex center;
        int radius = 1;
        int turnsLeft = 0;
    };
    std::vector<Drought> droughts;      // -1 Food on their plots while they last
    std::vector<PlayerProfile> profiles;  // per player (majors filled; leader doc §10 player modelling)
    int32_t nextCongressTurn = 0;       // when the World Congress next meets (0: not convened yet)
    int32_t lastSpecialSession = 0;     // the turn the last special session (an emergency or an aid request) was called
    int32_t congressOpenedTurn = 0;     // the turn the session in progress opened (0: none in session)
    std::vector<CongressItem> congress; // the resolutions in session
    std::vector<PassedResolution> passedResolutions;  // in force until the next session
    std::vector<GameEvent> events;  // most recent last, capped
    PendingBattle pendingBattle;
    UnitId nextUnitId = 1;
    int32_t nextCampId = 1;
    int32_t nextAgentId = 1;
    CityId nextCityId = 1;
    RngSet rng;
    PlayerId winner = kNoPlayer;  // set once the game is won; every command is then refused
    Victory victory = Victory::None;

    Plot& plot(Hex h) { return plots[static_cast<size_t>(grid.index(h))]; }
    const Plot& plot(Hex h) const { return plots[static_cast<size_t>(grid.index(h))]; }

    Unit* unit(UnitId id);
    const Unit* unit(UnitId id) const;
    City* city(CityId id);
    const City* city(CityId id) const;
    const City* cityAt(Hex h) const;
    // The district placed on this plot, if any (city centers are not districts here).
    const CityDistrict* districtAt(Hex h) const;
    // The wonder built or reserved on this plot (kNone: none).
    TypeIndex wonderAt(Hex h) const;
    // Unit on this plot in the given layer, if any.
    const Unit* unitAt(Hex h, UnitLayer layer, const Rules& rules) const;
    // Any unit on the plot not owned by this player.
    const Unit* foreignUnitAt(Hex h, PlayerId player) const;
};

}  // namespace sov
