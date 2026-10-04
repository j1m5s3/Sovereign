// The rules core's front door. UI, AI, network and tests all change the game
// the same way: submit(Command). Queries never change state.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sovereign/commands.h"
#include "sovereign/rules.h"
#include "sovereign/state.h"

namespace sov {

struct PathStep {
    Hex pos;
    int turn = 0;       // 0 = reached this turn
    Fixed movesLeft;    // after entering this plot
};

class Game {
public:
    // Builds a new game: map, start positions, starting units, first turn.
    static std::unique_ptr<Game> create(const Rules& rules, const GameSetup& setup, std::string* error);
    // Rebuilds a game from its setup and command log. Every command must apply.
    static std::unique_ptr<Game> replay(const Rules& rules, const GameSetup& setup,
                                        const std::vector<Command>& log, std::string* error);
    // Starts play from a hand-made state (scenario maps and tests): computes
    // every player's visibility and begins player 0's turn.
    static std::unique_ptr<Game> fromScenario(const Rules& rules, GameState state);
    // Wraps an already-loaded state (see serialize.h).
    Game(const Rules& rules, GameState state, std::vector<Command> log);

    CommandError validate(const Command& c) const;
    // Validates, applies and logs. Nothing changes when it returns an error.
    CommandError submit(const Command& c);

    const Rules& rules() const { return *rules_; }
    const GameState& state() const { return state_; }
    const std::vector<Command>& log() const { return log_; }
    // Hash of the full state (not the log); compared between peers each turn.
    uint64_t stateHash() const;

    // Path for a unit from where it stands, planned on its owner's knowledge.
    std::optional<std::vector<PathStep>> findPath(UnitId unit, Hex target) const;
    // Movement points needed for this unit to enter `to` from adjacent `from`;
    // nullopt when it cannot enter.
    std::optional<Fixed> moveCost(const Unit& unit, Hex from, Hex to) const;
    // Units that block ending the turn (05/00: "Units need orders").
    std::vector<UnitId> unitsNeedingOrders(PlayerId player) const;
    bool canFoundCityAt(PlayerId player, Hex at, CommandError* why = nullptr) const;
    Visibility visibility(PlayerId player, Hex h) const;

private:
    void apply(const Command& c);
    void applyMove(const Command& c);
    void applyFoundCity(const Command& c);
    void applyEndTurn(const Command& c);
    // Moves the unit along its move order as far as its moves allow.
    void advanceUnit(UnitId id);
    void beginPlayerTurn(PlayerId p);
    void beginGlobalTurn();
    void refreshVisibility(PlayerId p);
    Unit& spawnUnit(TypeIndex type, PlayerId owner, Hex pos);

    const Rules* rules_;
    GameState state_;
    std::vector<Command> log_;
};

}  // namespace sov
