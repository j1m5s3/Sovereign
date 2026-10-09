// Every change to game state is a command, whoever issues it (player, AI,
// network, live-scene result, language-model output), and every command goes
// into one log (engine doc, Core foundations: Commands). Commands are flat
// values so they serialize and travel over the network trivially.
#pragma once

#include "sovereign/api.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "sovereign/hex.h"
#include "sovereign/state.h"

namespace sov {

enum class CommandType : uint8_t {
    MoveUnit = 1,         // id = unit, target: start or replace a (multi-turn) move order
    FoundCity = 2,        // id = unit (a settler) founds a city where it stands
    SetActivity = 3,      // id = unit, arg = Activity
    EndTurn = 4,          // player ends their turn
    SetProduction = 5,    // id = city, arg = ProductionKind, arg2 = type index: replace the queue
                          // (a district not yet placed goes on target)
    QueueProduction = 6,  // id = city, arg = ProductionKind, arg2 = type index: append to the queue (target as above)
    Purchase = 7,         // id = city, arg = ProductionKind, arg2 = type index: buy with gold now
    BuyPlot = 8,          // id = city, target = plot to buy with gold
    LockPlot = 9,         // id = city, target = plot, arg = 1 lock a citizen on it / 0 unlock
    ChooseResearch = 10,  // id = tech
    ChooseCivic = 11,     // id = civic
    ChangeGovernment = 12,  // id = government
    SetPolicy = 13,       // id = slot of the current government, arg = policy (-1 empties the slot)
    BuildImprovement = 14,  // id = builder, arg = improvement: build it where the builder stands
    Harvest = 15,         // id = builder: harvest the feature where it stands, else its bonus resource; arg 1: the resource, leaving the feature
    DeclareWar = 16,      // arg = the player to declare war on
    MakePeace = 17,       // arg = the player to offer peace; peace comes once both have offered
    Attack = 18,          // id = unit, target = adjacent plot: melee attack (or capture a civilian)
    RangedAttack = 19,    // id = unit, target = plot in range and sight
    Promote = 20,         // id = unit, arg = promotion
    CityStrike = 21,      // id = city with walls, target = enemy unit in range: the city's ranged strike
    RazeCity = 22,        // id = city captured this turn (not an original capital): burn it down
    EquipGear = 23,       // id = leader, arg = gear (or -1 with arg2 = slot to take that item off)
    LinkEscort = 24,      // id = military unit, arg = leader or civilian on its plot to escort (-1 ends the link)
    ChooseSuccessor = 25, // arg = Succession kind; id = the unit for Succession::Unit; arg2 = promotion an heir keeps (-1: none)
    AbandonLeader = 26,   // give up the captured leader and crown a successor
    SendAssassin = 27,    // id = agent, arg = target player (-1 calls it home)
    CityStance = 28,      // id = city where the leader stands, arg = Stance
    BattleResult = 29,    // the live battle's field result: arg = damage to the defender, arg2 = to the attacker,
                          // target.x = wound to the leader (clamped by the core); any time, by the battle's human;
                          // data = the human's habits x1000 (flank, fall back, hunt leader, leader in front), optional
    AutoResolveBattle = 30,  // settle the pending battle with the normal Civ roll
    PatronizeGreatPerson = 31,  // arg = great person class, arg2 = 0 gold / 1 faith: buy its current individual now
    PassGreatPerson = 32,       // arg = great person class: decline its current individual
    ActivateGreatPerson = 33,   // id = great person unit: use it where it stands (one charge or one Great Work)
    FoundPantheon = 34,         // arg = pantheon belief
    FoundReligion = 35,         // id = Great Prophet on a Holy Site, arg = religion, arg2 = Founder belief, target.x = Follower belief
    EvangelizeBelief = 36,      // id = Apostle, arg = a Worship, Enhancer (or missing) belief for its religion
    SpreadReligion = 37,        // id = religious unit in a city's territory: spread its religion there
    StartTradeRoute = 38,       // id = Trader in one of the player's cities, arg = destination city
    SendEnvoy = 39,             // arg = a city-state player the sender has met
    ProposeDeal = 40,           // arg = the other player, data = the items (4 ints each: kind, giver, amount, resource);
                                // an AI answers at once, a human later with AnswerDeal
    AnswerDeal = 41,            // id = a waiting deal, arg = 1 accept / 0 reject (the proposer may withdraw it with 0)
    Denounce = 42,              // arg = the player to denounce
    RecordTalk = 43,            // arg = the civ spoken to, text = the conversation's summary (printable, capped)
    AppointGovernor = 44,       // arg = governor type (spends a title; it starts with its base ability)
    PromoteGovernor = 45,       // arg = governor type, arg2 = promotion (spends a title)
    AssignGovernor = 46,        // arg = governor type, id = city (own, or a city-state's for Amani); it starts establishing
    SpyMission = 47,            // id = spy agent, arg = SpyMission (None: home), arg2 = city; a new city costs travel first
    CongressVote = 48,          // id = item in session, arg = option (0 A, 1 B), arg2 = candidate index, target.x = votes bought with favor
    UpgradeUnit = 49,           // id = unit: becomes the next unit in its line for gold (05: Upgrades)
    RebaseUnit = 50,            // id = aircraft, target = a friendly air base with a free slot
    JoinEmergency = 52,         // arg = index into GameState::emergencies (running, the player eligible)
    SendDelegation = 60,        // arg = the civ, arg2 = 1 a resident embassy (else a delegation); costs Gold (08)
    AskPromise = 59,            // arg = the civ asked, arg2 = PromiseKind; costs Diplomatic Favor [GS]
    DesignatePark = 63,         // id = Naturalist: a National Park of its plot and three beside it (07)
    PerformConcert = 64,        // id = Rock Band in a foreign city's district or wonder plot: tourism toward that civ [GS]
    ContributeCharge = 66,      // id = Military Engineer on a district being built, or a Builder with the Royal Society where its city's project runs: a charge adds its share of the cost (03)
    BribeCamp = 67,             // id = a camp (Barbarian Clans mode): its units leave the player alone for a while (01)
    HireFromCamp = 68,          // id = a camp (Barbarian Clans mode): its best unit joins the player, next to the camp (01)
    InciteCamp = 69,            // id = a camp, arg = a civ (Barbarian Clans mode): the camp raids that civ for a while (01)
    BuildIndustry = 70,         // id = Builder on an improved luxury (Monopolies mode): an Industry, or a Corporation of one (07)
    LiberateCity = 71,          // id = city captured this turn: back to its original owner (02: Captured cities)
    Airlift = 72,               // id = land unit on an Aerodrome with an Airport, target = another of the player's (Rapid Deployment; 05)
    Paradrop = 73,              // id = Spec Ops in the player's territory, target = a land plot within 3 (05)
    BuyPolicyChanges = 74,      // pays Gold to change government and policies this turn (04)
    LaunchInquisition = 75,     // id = an unused Apostle of the player's religion: Inquisitors may be bought (06)
    HealReligious = 76,         // id = Guru: a heal charge restores its own and adjacent religious units (06)
    BuildRoad = 77,             // id = Military Engineer (until railroads), Legionary or Qin's Builder: a road on its plot for a charge (none for Qin's)
    SetCityFocus = 78,          // id = city, arg = CityFocus: the yield its citizens favour (02: Citizens)
    AppointBodyguard = 80,      // id = a military unit of BODYGUARD_MIN_LEVEL+ or a Great General/Admiral on the ruler's plot, or -1
                                // with arg = an appointed governor's type: the ruler's named bodyguard (leader doc §8.3)
    CreateProduct = 79,         // id = Great Merchant in a Corporation's city (Monopolies mode): a Product Great Work (07)
    LevyMilitary = 65,          // arg = a city-state it is suzerain of: its military units serve the player for LEVY_MILITARY_TURN_DURATION (08)
    ChooseDedication = 62,      // arg = Rules::dedications (09: Dedications)
    MoveGreatWork = 61,         // id = the city holding it, arg = its index there, arg2 = the city it goes to, target.x = the building (07)
    Excavate = 58,              // id = Archaeologist on an antiquity site or shipwreck: an Artifact into a free slot
    FormUnit = 57,              // id = military unit, arg = a neighbouring unit of its type: a Corps/Fleet, or an Army/Armada
    Pillage = 55,               // id = military land unit: pillages the improvement or district on its plot (enemy land)
    RepairImprovement = 56,     // id = Builder: repairs the pillaged improvement on its own plot (no charge)
    BuildRailroad = 54,         // id = Military Engineer: lays a railroad on its plot for the route's resources
    PromoteSpy = 53,            // id = spy agent with a promotion pending, arg = Rules::spyPromotions (one it lacks)
    LaunchWmd = 51,             // arg = weapon (Rules::wmds), target = blast centre; id = bomber or Nuclear Submarine,
                                // or -1 with data = {x, y} of the player's Missile Silo
};

// Casus belli (08: War types): the reason a war is declared for, each scaling the declaration's grievances.
enum class CasusBelli : int32_t { None = 0, HolyWar, Liberation, Reconquest, Protectorate, Colonial, TerritorialExpansion, Ideological, Retribution, GoldenAge, JointWar };
constexpr int kNumCasusBelli = 11;  // JointWar only through a deal (08: Joint War)

// Who takes the throne (leader doc §5): the dynasty's next heir, a level-4+ military unit, a regent
// when neither exists, an appointed governor (the command's unit is its governor type), or a Great
// General or Admiral (the command's unit).
enum class Succession : int32_t { Heir = 0, Unit = 1, Regent = 2, Governor = 3, GreatPerson = 4 };

// The leader's stance toward a city's citizens (leader doc §4).
enum class Stance : int32_t { Benevolence = 0, Fear = 1 };

struct Command {
    CommandType type = CommandType::EndTurn;
    PlayerId player = kNoPlayer;
    int32_t id = -1;  // the unit or city the command acts on
    Hex target;
    int32_t arg = 0;
    int32_t arg2 = 0;
    std::vector<int32_t> data;  // a variable payload (deal items)
    std::string text;           // free text carried with the command (capped by validation)

    Command(CommandType t = CommandType::EndTurn, PlayerId p = kNoPlayer, int32_t i = -1, Hex at = {}, int32_t a = 0, int32_t a2 = 0)
        : type(t), player(p), id(i), target(at), arg(a), arg2(a2) {}

    // overland: a land unit keeps to land rather than embarking on the way.
    static Command move(PlayerId p, UnitId u, Hex to, bool overland = false) { return {CommandType::MoveUnit, p, u, to, overland ? 1 : 0, 0}; }
    static Command foundCity(PlayerId p, UnitId u) { return {CommandType::FoundCity, p, u, {}, 0, 0}; }
    static Command setActivity(PlayerId p, UnitId u, Activity a) {
        return {CommandType::SetActivity, p, u, {}, static_cast<int32_t>(a), 0};
    }
    static Command endTurn(PlayerId p) { return {CommandType::EndTurn, p, -1, {}, 0, 0}; }
    static Command setProduction(PlayerId p, CityId c, ProductionItem item, Hex at = {}) {
        return {CommandType::SetProduction, p, c, at, item.packedKind(), item.type};
    }
    static Command queueProduction(PlayerId p, CityId c, ProductionItem item, Hex at = {}) {
        return {CommandType::QueueProduction, p, c, at, item.packedKind(), item.type};
    }
    static Command purchase(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::Purchase, p, c, {}, item.packedKind(), item.type};
    }
    static Command buyPlot(PlayerId p, CityId c, Hex plot) { return {CommandType::BuyPlot, p, c, plot, 0, 0}; }
    static Command lockPlot(PlayerId p, CityId c, Hex plot, bool lock) {
        return {CommandType::LockPlot, p, c, plot, lock ? 1 : 0, 0};
    }
    static Command setCityFocus(PlayerId p, CityId c, CityFocus focus) {
        return {CommandType::SetCityFocus, p, c, {}, static_cast<int32_t>(focus), 0};
    }
    static Command chooseResearch(PlayerId p, TypeIndex tech) { return {CommandType::ChooseResearch, p, tech, {}, 0, 0}; }
    static Command chooseCivic(PlayerId p, TypeIndex civic) { return {CommandType::ChooseCivic, p, civic, {}, 0, 0}; }
    static Command changeGovernment(PlayerId p, TypeIndex gov) {
        return {CommandType::ChangeGovernment, p, gov, {}, 0, 0};
    }
    static Command setPolicy(PlayerId p, int slot, TypeIndex policy) {
        return {CommandType::SetPolicy, p, slot, {}, policy, 0};
    }
    static Command buildImprovement(PlayerId p, UnitId u, TypeIndex improvement) {
        return {CommandType::BuildImprovement, p, u, {}, improvement, 0};
    }
    static Command harvest(PlayerId p, UnitId u, bool resource = false) { return {CommandType::Harvest, p, u, {}, resource ? 1 : 0, 0}; }
    static Command declareWar(PlayerId p, PlayerId target) { return {CommandType::DeclareWar, p, -1, {}, target, 0}; }
    // A war with a casus belli (08: War types; arg2 = CasusBelli), its grievances scaled down.
    static Command declareWarFor(PlayerId p, PlayerId target, CasusBelli why) { return {CommandType::DeclareWar, p, -1, {}, target, static_cast<int32_t>(why)}; }
    static Command makePeace(PlayerId p, PlayerId target) { return {CommandType::MakePeace, p, -1, {}, target, 0}; }
    static Command attack(PlayerId p, UnitId u, Hex at) { return {CommandType::Attack, p, u, at, 0, 0}; }
    static Command rangedAttack(PlayerId p, UnitId u, Hex at) { return {CommandType::RangedAttack, p, u, at, 0, 0}; }
    static Command promote(PlayerId p, UnitId u, TypeIndex promotion) {
        return {CommandType::Promote, p, u, {}, promotion, 0};
    }
    static Command cityStrike(PlayerId p, CityId c, Hex at) { return {CommandType::CityStrike, p, c, at, 0, 0}; }
    // The city's Encampment fires instead of its center (arg 1; 03: Defense).
    static Command encampmentStrike(PlayerId p, CityId c, Hex at) { return {CommandType::CityStrike, p, c, at, 1, 0}; }
    static Command razeCity(PlayerId p, CityId c) { return {CommandType::RazeCity, p, c, {}, 0, 0}; }
    static Command liberateCity(PlayerId p, CityId c) { return {CommandType::LiberateCity, p, c, {}, 0, 0}; }
    static Command airlift(PlayerId p, UnitId unit, Hex to) { return {CommandType::Airlift, p, unit, to, 0, 0}; }
    static Command paradrop(PlayerId p, UnitId unit, Hex to) { return {CommandType::Paradrop, p, unit, to, 0, 0}; }
    static Command buyPolicyChanges(PlayerId p) { return {CommandType::BuyPolicyChanges, p, 0, {}, 0, 0}; }
    static Command launchInquisition(PlayerId p, UnitId apostle) { return {CommandType::LaunchInquisition, p, apostle, {}, 0, 0}; }
    static Command healReligious(PlayerId p, UnitId guru) { return {CommandType::HealReligious, p, guru, {}, 0, 0}; }
    static Command equipGear(PlayerId p, UnitId leader, TypeIndex gear) {
        return {CommandType::EquipGear, p, leader, {}, gear, 0};
    }
    static Command removeGear(PlayerId p, UnitId leader, GearSlot slot) {
        return {CommandType::EquipGear, p, leader, {}, -1, static_cast<int32_t>(slot)};
    }
    static Command linkEscort(PlayerId p, UnitId escort, UnitId escorted) {
        return {CommandType::LinkEscort, p, escort, {}, escorted, 0};
    }
    static Command chooseSuccessor(PlayerId p, Succession kind, UnitId unit = kNoUnit, TypeIndex keepPromotion = kNone) {
        return {CommandType::ChooseSuccessor, p, unit, {}, static_cast<int32_t>(kind), keepPromotion};
    }
    static Command cityStance(PlayerId p, CityId c, Stance stance) {
        return {CommandType::CityStance, p, c, {}, static_cast<int32_t>(stance), 0};
    }
    static Command battleResult(PlayerId p, int toDefender, int toAttacker, int leaderWound, std::vector<int32_t> habits = {}) {
        Command c{CommandType::BattleResult, p, -1, Hex{leaderWound, 0}, toDefender, toAttacker};
        c.data = std::move(habits);
        return c;
    }
    static Command autoResolveBattle(PlayerId p) { return {CommandType::AutoResolveBattle, p, -1, {}, 0, 0}; }
    static Command sendAssassin(PlayerId p, int32_t agent, PlayerId target) {
        return {CommandType::SendAssassin, p, agent, {}, target, 0};
    }
    static Command appointBodyguard(PlayerId p, UnitId unit, TypeIndex governor = kNone) {
        return {CommandType::AppointBodyguard, p, unit, {}, governor, 0};
    }
    static Command abandonLeader(PlayerId p) { return {CommandType::AbandonLeader, p, -1, {}, 0, 0}; }
    static Command patronizeGreatPerson(PlayerId p, TypeIndex cls, bool faith) {
        return {CommandType::PatronizeGreatPerson, p, -1, {}, cls, faith ? 1 : 0};
    }
    static Command passGreatPerson(PlayerId p, TypeIndex cls) { return {CommandType::PassGreatPerson, p, -1, {}, cls, 0}; }
    static Command activateGreatPerson(PlayerId p, UnitId u) { return {CommandType::ActivateGreatPerson, p, u, {}, 0, 0}; }
    static Command foundPantheon(PlayerId p, TypeIndex belief) { return {CommandType::FoundPantheon, p, -1, {}, belief, 0}; }
    static Command foundReligion(PlayerId p, UnitId prophet, TypeIndex religion, TypeIndex founder, TypeIndex follower) {
        return {CommandType::FoundReligion, p, prophet, Hex{follower, 0}, religion, founder};
    }
    static Command evangelizeBelief(PlayerId p, UnitId apostle, TypeIndex belief) {
        return {CommandType::EvangelizeBelief, p, apostle, {}, belief, 0};
    }
    static Command spreadReligion(PlayerId p, UnitId u) { return {CommandType::SpreadReligion, p, u, {}, 0, 0}; }
    static Command sendEnvoy(PlayerId p, PlayerId cityState) { return {CommandType::SendEnvoy, p, -1, {}, cityState, 0}; }
    static Command levyMilitary(PlayerId p, PlayerId cityState) { return {CommandType::LevyMilitary, p, -1, {}, cityState, 0}; }
    static Command bribeCamp(PlayerId p, int32_t camp) { return {CommandType::BribeCamp, p, camp, {}, 0, 0}; }
    static Command hireFromCamp(PlayerId p, int32_t camp) { return {CommandType::HireFromCamp, p, camp, {}, 0, 0}; }
    static Command inciteCamp(PlayerId p, int32_t camp, PlayerId against) { return {CommandType::InciteCamp, p, camp, {}, against, 0}; }
    static Command buildIndustry(PlayerId p, UnitId builder) { return {CommandType::BuildIndustry, p, builder, {}, 0, 0}; }
    static Command createProduct(PlayerId p, UnitId merchant) { return {CommandType::CreateProduct, p, merchant, {}, 0, 0}; }
    static Command startTradeRoute(PlayerId p, UnitId trader, CityId destination) {
        return {CommandType::StartTradeRoute, p, trader, {}, destination, 0};
    }
    SOV_API static Command proposeDeal(PlayerId p, PlayerId to, const std::vector<DealItem>& items);
    static Command answerDeal(PlayerId p, int32_t deal, bool accept) { return {CommandType::AnswerDeal, p, deal, {}, accept ? 1 : 0, 0}; }
    static Command denounce(PlayerId p, PlayerId target) { return {CommandType::Denounce, p, -1, {}, target, 0}; }
    static Command appointGovernor(PlayerId p, TypeIndex governor) { return {CommandType::AppointGovernor, p, -1, {}, governor, 0}; }
    static Command promoteGovernor(PlayerId p, TypeIndex governor, TypeIndex promotion) {
        return {CommandType::PromoteGovernor, p, -1, {}, governor, promotion};
    }
    static Command assignGovernor(PlayerId p, TypeIndex governor, CityId city) { return {CommandType::AssignGovernor, p, city, {}, governor, 0}; }
    static Command spyMission(PlayerId p, int32_t spy, SpyMission mission, CityId city) {
        return {CommandType::SpyMission, p, spy, {}, static_cast<int32_t>(mission), city};
    }
    static Command upgradeUnit(PlayerId p, UnitId unit) { return {CommandType::UpgradeUnit, p, unit, {}, 0, 0}; }
    static Command rebaseUnit(PlayerId p, UnitId unit, Hex to) { return {CommandType::RebaseUnit, p, unit, to, 0, 0}; }
    static Command sendDelegation(PlayerId p, PlayerId to, bool embassy) { return {CommandType::SendDelegation, p, -1, {}, to, embassy ? 1 : 0}; }
    static Command askPromise(PlayerId p, PlayerId of, PromiseKind kind) { return {CommandType::AskPromise, p, -1, {}, of, static_cast<int32_t>(kind)}; }
    static Command moveGreatWork(PlayerId p, CityId from, int index, CityId to, TypeIndex building) {
        return {CommandType::MoveGreatWork, p, from, Hex{building, 0}, index, to};
    }
    static Command designatePark(PlayerId p, UnitId naturalist) { return {CommandType::DesignatePark, p, naturalist, {}, 0, 0}; }
    static Command performConcert(PlayerId p, UnitId band) { return {CommandType::PerformConcert, p, band, {}, 0, 0}; }
    static Command chooseDedication(PlayerId p, TypeIndex dedication) { return {CommandType::ChooseDedication, p, -1, {}, dedication, 0}; }
    static Command excavate(PlayerId p, UnitId archaeologist) { return {CommandType::Excavate, p, archaeologist, {}, 0, 0}; }
    static Command formUnit(PlayerId p, UnitId unit, UnitId with) { return {CommandType::FormUnit, p, unit, {}, with, 0}; }
    static Command pillage(PlayerId p, UnitId unit) { return {CommandType::Pillage, p, unit, {}, 0, 0}; }
    // A coastal raid (05): a naval melee unit or raider pillages the neighbouring land plot `at` (arg 1).
    static Command coastalRaid(PlayerId p, UnitId unit, Hex at) { return {CommandType::Pillage, p, unit, at, 1, 0}; }
    static Command repairImprovement(PlayerId p, UnitId builder) { return {CommandType::RepairImprovement, p, builder, {}, 0, 0}; }
    static Command buildRailroad(PlayerId p, UnitId engineer) { return {CommandType::BuildRailroad, p, engineer, {}, 0, 0}; }
    static Command buildRoad(PlayerId p, UnitId unit) { return {CommandType::BuildRoad, p, unit, {}, 0, 0}; }
    static Command contributeCharge(PlayerId p, UnitId engineer) { return {CommandType::ContributeCharge, p, engineer, {}, 0, 0}; }
    // A Mountain Tunnel is built on the neighbouring mountain `at` (BuildImprovement with a target).
    static Command buildTunnel(PlayerId p, UnitId engineer, TypeIndex tunnel, Hex at) { return {CommandType::BuildImprovement, p, engineer, at, tunnel, 0}; }
    static Command promoteSpy(PlayerId p, int32_t spy, TypeIndex promotion) { return {CommandType::PromoteSpy, p, spy, {}, promotion, 0}; }
    static Command joinEmergency(PlayerId p, int32_t emergency) { return {CommandType::JoinEmergency, p, -1, {}, emergency, 0}; }
    static Command launchWmd(PlayerId p, UnitId unit, TypeIndex weapon, Hex target) { return {CommandType::LaunchWmd, p, unit, target, weapon, 0}; }
    static Command launchWmdFromSilo(PlayerId p, Hex silo, TypeIndex weapon, Hex target) {
        Command c{CommandType::LaunchWmd, p, -1, target, weapon, 0};
        c.data = {silo.x, silo.y};
        return c;
    }
    static Command congressVote(PlayerId p, int32_t item, int option, int32_t candidate, int extraVotes = 0) {
        return {CommandType::CongressVote, p, item, Hex{extraVotes, 0}, option, candidate};
    }
    static Command recordTalk(PlayerId p, PlayerId leader, const std::string& summary) {
        Command c{CommandType::RecordTalk, p, -1, {}, leader, 0};
        c.text = summary;
        return c;
    }
    // Buy a religious unit or a worship building with Faith (target.x = 1 marks a Faith purchase).
    static Command purchaseWithFaith(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::Purchase, p, c, Hex{1, 0}, item.packedKind(), item.type};
    }
};

enum class CommandError : uint8_t {
    Ok = 0,
    NotYourTurn,
    BadPlayer,
    BadUnit,
    NotYourUnit,
    BadTarget,
    NoPath,
    CannotFoundHere,
    TooCloseToCity,
    NotASettler,
    BadActivity,
    UnitsNeedOrders,
    GameOver,
    BadCity,
    NotYourCity,
    CannotBuild,
    QueueFull,
    NotEnoughGold,
    CannotBuyPlot,
    CannotWorkPlot,
    ProductionNeeded,
    CannotResearch,
    ResearchNeeded,
    CivicNeeded,
    CannotAdoptGovernment,
    CannotSetPolicy,
    ChangesLocked,
    CannotImprove,
    CannotHarvest,
    NotEnoughResources,
    CannotDeclareWar,
    CannotMakePeace,
    CannotAttack,
    CannotPromote,
    CannotStrike,
    CannotRaze,
    CannotEquip,
    CannotEscort,
    LeaderNeeded,
    CannotSucceed,
    CannotSendAgent,
    CannotTakeStance,
    BattlePending,
    NoBattle,
    NoGreatPerson,
    NotEnoughFaith,
    CannotActivate,
    CannotFoundReligion,
    CannotSpread,
    CannotTrade,
    CannotSendEnvoy,
    CannotDeal,
    NoDeal,
    CannotDenounce,
    CannotGovern,
    CannotSpy,
    CannotVote,
    CannotUpgrade,
    CannotTreatWithClan,
    CannotGuard,
};

// The items a ProposeDeal command carries (empty when its payload is malformed).
SOV_API std::vector<DealItem> dealItems(const Command& c);

SOV_API const char* commandErrorName(CommandError e);
SOV_API std::string describe(const Command& c);

}  // namespace sov
