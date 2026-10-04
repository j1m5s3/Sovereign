// Every change to game state is a command, whoever issues it (player, AI,
// network, live-scene result, language-model output), and every command goes
// into one log (engine doc, Core foundations: Commands). Commands are flat
// values so they serialize and travel over the network trivially.
#pragma once

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
    QueueProduction = 6,  // id = city, arg = ProductionKind, arg2 = type index: append to the queue
    Purchase = 7,         // id = city, arg = ProductionKind, arg2 = type index: buy with gold now
    BuyPlot = 8,          // id = city, target = plot to buy with gold
    LockPlot = 9,         // id = city, target = plot, arg = 1 lock a citizen on it / 0 unlock
};

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
    static Command setProduction(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::SetProduction, p, c, {}, static_cast<int32_t>(item.kind), item.type};
    }
    static Command queueProduction(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::QueueProduction, p, c, {}, static_cast<int32_t>(item.kind), item.type};
    }
    static Command purchase(PlayerId p, CityId c, ProductionItem item) {
        return {CommandType::Purchase, p, c, {}, static_cast<int32_t>(item.kind), item.type};
    }
    static Command buyPlot(PlayerId p, CityId c, Hex plot) { return {CommandType::BuyPlot, p, c, plot, 0, 0}; }
    static Command lockPlot(PlayerId p, CityId c, Hex plot, bool lock) {
        return {CommandType::LockPlot, p, c, plot, lock ? 1 : 0, 0};
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
};

const char* commandErrorName(CommandError e);
std::string describe(const Command& c);

}  // namespace sov
