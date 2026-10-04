// Every change to game state is a command, whoever issues it (player, AI,
// network, live-scene result, language-model output), and every command goes
// into one log (engine doc, Core foundations: Commands). Commands are flat
// values so they serialize and travel over the network trivially.
#pragma once

#include "sovereign/api.h"

#include <cstdint>
#include <string>

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
    Harvest = 15,         // id = builder: harvest the feature or bonus resource where it stands
    DeclareWar = 16,      // arg = the player to declare war on
    MakePeace = 17,       // arg = the player to offer peace; peace comes once both have offered
    Attack = 18,          // id = unit, target = adjacent plot: melee attack (or capture a civilian)
    RangedAttack = 19,    // id = unit, target = plot in range and sight
    Promote = 20,         // id = unit, arg = promotion
    CityStrike = 21,      // id = city with walls, target = enemy unit in range: the city's ranged strike
    RazeCity = 22,        // id = city captured this turn (not an original capital): burn it down
    EquipGear = 23,       // id = leader, arg = gear (or -1 with arg2 = slot to take that item off)
    LinkEscort = 24,      // id = military unit, arg = leader on its plot to escort (-1 ends the link)
    ChooseSuccessor = 25, // arg = Succession kind; id = the unit for Succession::Unit
    AbandonLeader = 26,   // give up the captured leader and crown a successor
};

// Who takes the throne (leader doc §5): the dynasty's next heir, a level-4+ military unit,
// or a regent when neither exists (a stand-in until governors and Great Generals exist).
enum class Succession : int32_t { Heir = 0, Unit = 1, Regent = 2 };

struct Command {
    CommandType type = CommandType::EndTurn;
    PlayerId player = kNoPlayer;
    int32_t id = -1;  // the unit or city the command acts on
    Hex target;
    int32_t arg = 0;
    int32_t arg2 = 0;

    static Command move(PlayerId p, UnitId u, Hex to) { return {CommandType::MoveUnit, p, u, to, 0, 0}; }
    static Command foundCity(PlayerId p, UnitId u) { return {CommandType::FoundCity, p, u, {}, 0, 0}; }
    static Command setActivity(PlayerId p, UnitId u, Activity a) {
        return {CommandType::SetActivity, p, u, {}, static_cast<int32_t>(a), 0};
    }
    static Command endTurn(PlayerId p) { return {CommandType::EndTurn, p, -1, {}, 0, 0}; }
    static Command setProduction(PlayerId p, CityId c, ProductionItem item, Hex at = {}) {
        return {CommandType::SetProduction, p, c, at, static_cast<int32_t>(item.kind), item.type};
    }
    static Command queueProduction(PlayerId p, CityId c, ProductionItem item, Hex at = {}) {
        return {CommandType::QueueProduction, p, c, at, static_cast<int32_t>(item.kind), item.type};
    }
    static Command purchase(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::Purchase, p, c, {}, static_cast<int32_t>(item.kind), item.type};
    }
    static Command buyPlot(PlayerId p, CityId c, Hex plot) { return {CommandType::BuyPlot, p, c, plot, 0, 0}; }
    static Command lockPlot(PlayerId p, CityId c, Hex plot, bool lock) {
        return {CommandType::LockPlot, p, c, plot, lock ? 1 : 0, 0};
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
    static Command harvest(PlayerId p, UnitId u) { return {CommandType::Harvest, p, u, {}, 0, 0}; }
    static Command declareWar(PlayerId p, PlayerId target) { return {CommandType::DeclareWar, p, -1, {}, target, 0}; }
    static Command makePeace(PlayerId p, PlayerId target) { return {CommandType::MakePeace, p, -1, {}, target, 0}; }
    static Command attack(PlayerId p, UnitId u, Hex at) { return {CommandType::Attack, p, u, at, 0, 0}; }
    static Command rangedAttack(PlayerId p, UnitId u, Hex at) { return {CommandType::RangedAttack, p, u, at, 0, 0}; }
    static Command promote(PlayerId p, UnitId u, TypeIndex promotion) {
        return {CommandType::Promote, p, u, {}, promotion, 0};
    }
    static Command cityStrike(PlayerId p, CityId c, Hex at) { return {CommandType::CityStrike, p, c, at, 0, 0}; }
    static Command razeCity(PlayerId p, CityId c) { return {CommandType::RazeCity, p, c, {}, 0, 0}; }
    static Command equipGear(PlayerId p, UnitId leader, TypeIndex gear) {
        return {CommandType::EquipGear, p, leader, {}, gear, 0};
    }
    static Command removeGear(PlayerId p, UnitId leader, GearSlot slot) {
        return {CommandType::EquipGear, p, leader, {}, -1, static_cast<int32_t>(slot)};
    }
    static Command linkEscort(PlayerId p, UnitId escort, UnitId leader) {
        return {CommandType::LinkEscort, p, escort, {}, leader, 0};
    }
    static Command chooseSuccessor(PlayerId p, Succession kind, UnitId unit = kNoUnit) {
        return {CommandType::ChooseSuccessor, p, unit, {}, static_cast<int32_t>(kind), 0};
    }
    static Command abandonLeader(PlayerId p) { return {CommandType::AbandonLeader, p, -1, {}, 0, 0}; }
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
};

SOV_API const char* commandErrorName(CommandError e);
SOV_API std::string describe(const Command& c);

}  // namespace sov
