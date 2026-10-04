#include "sovereign/state.h"

#include <algorithm>

#include "sovereign/commands.h"

namespace sov {

namespace {
template <typename T, typename Id>
T* findById(std::vector<T>& v, Id id) {
    auto it = std::lower_bound(v.begin(), v.end(), id, [](const T& a, Id b) { return a.id < b; });
    return (it != v.end() && it->id == id) ? &*it : nullptr;
}
template <typename T, typename Id>
const T* findById(const std::vector<T>& v, Id id) {
    auto it = std::lower_bound(v.begin(), v.end(), id, [](const T& a, Id b) { return a.id < b; });
    return (it != v.end() && it->id == id) ? &*it : nullptr;
}
}  // namespace

Unit* GameState::unit(UnitId id) { return findById(units, id); }
const Unit* GameState::unit(UnitId id) const { return findById(units, id); }
City* GameState::city(CityId id) { return findById(cities, id); }
const City* GameState::city(CityId id) const { return findById(cities, id); }

const City* GameState::cityAt(Hex h) const {
    for (const City& c : cities) {
        if (c.pos == h) return &c;
    }
    return nullptr;
}

const Unit* GameState::unitAt(Hex h, UnitLayer layer, const Rules& rules) const {
    for (const Unit& u : units) {
        if (u.pos == h && rules.units[static_cast<size_t>(u.type)].layer == layer) return &u;
    }
    return nullptr;
}

const Unit* GameState::foreignUnitAt(Hex h, PlayerId player) const {
    for (const Unit& u : units) {
        if (u.pos == h && u.owner != player) return &u;
    }
    return nullptr;
}

const char* commandErrorName(CommandError e) {
    switch (e) {
        case CommandError::Ok: return "Ok";
        case CommandError::NotYourTurn: return "NotYourTurn";
        case CommandError::BadPlayer: return "BadPlayer";
        case CommandError::BadUnit: return "BadUnit";
        case CommandError::NotYourUnit: return "NotYourUnit";
        case CommandError::BadTarget: return "BadTarget";
        case CommandError::NoPath: return "NoPath";
        case CommandError::CannotFoundHere: return "CannotFoundHere";
        case CommandError::TooCloseToCity: return "TooCloseToCity";
        case CommandError::NotASettler: return "NotASettler";
        case CommandError::BadActivity: return "BadActivity";
        case CommandError::UnitsNeedOrders: return "UnitsNeedOrders";
        case CommandError::GameOver: return "GameOver";
    }
    return "Unknown";
}

std::string describe(const Command& c) {
    std::string s = "p" + std::to_string(c.player) + " ";
    switch (c.type) {
        case CommandType::MoveUnit:
            return s + "MoveUnit u" + std::to_string(c.unit) + " -> (" + std::to_string(c.target.x) + "," +
                   std::to_string(c.target.y) + ")";
        case CommandType::FoundCity: return s + "FoundCity u" + std::to_string(c.unit);
        case CommandType::SetActivity:
            return s + "SetActivity u" + std::to_string(c.unit) + " " + std::to_string(c.arg);
        case CommandType::EndTurn: return s + "EndTurn";
    }
    return s + "?";
}

}  // namespace sov
