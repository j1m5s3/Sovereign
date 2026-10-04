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
    MoveUnit = 1,     // unit, target: start or replace a (multi-turn) move order
    FoundCity = 2,    // unit (a settler) founds a city where it stands
    SetActivity = 3,  // unit, arg = Activity
    EndTurn = 4,      // player ends their turn
};

struct Command {
    CommandType type = CommandType::EndTurn;
    PlayerId player = kNoPlayer;
    UnitId unit = kNoUnit;
    Hex target;
    int32_t arg = 0;

    static Command move(PlayerId p, UnitId u, Hex to) { return {CommandType::MoveUnit, p, u, to, 0}; }
    static Command foundCity(PlayerId p, UnitId u) { return {CommandType::FoundCity, p, u, {}, 0}; }
    static Command setActivity(PlayerId p, UnitId u, Activity a) {
        return {CommandType::SetActivity, p, u, {}, static_cast<int32_t>(a)};
    }
    static Command endTurn(PlayerId p) { return {CommandType::EndTurn, p, kNoUnit, {}, 0}; }
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
};

const char* commandErrorName(CommandError e);
std::string describe(const Command& c);

}  // namespace sov
