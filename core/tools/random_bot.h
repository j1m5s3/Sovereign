// A deliberately dumb player for soak tests and the headless simulator: founds
// a city with each settler where it stands when allowed, wanders other units
// to random known plots, then ends the turn. Not the game AI (that is MVP-6).
// It only talks to the core through commands, like any real player.
#pragma once

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/rng.h"

namespace sovbot {

inline void playTurn(sov::Game& game, sov::Rng& rng) {
    using namespace sov;
    const GameState& s = game.state();
    const PlayerId me = s.currentPlayer;
    std::vector<UnitId> mine;
    for (const Unit& u : s.units) {
        if (u.owner == me) mine.push_back(u.id);
    }
    for (UnitId id : mine) {
        const Unit* u = game.state().unit(id);
        if (!u || u->moveTarget) continue;
        const UnitType& t = game.rules().units[static_cast<size_t>(u->type)];
        if (t.foundCity && game.submit(Command::foundCity(me, id)) == CommandError::Ok) continue;
        bool moved = false;
        for (int attempt = 0; attempt < 8 && !moved; ++attempt) {
            Hex to{u->pos.x + rng.range(-5, 5), u->pos.y + rng.range(-5, 5)};
            auto n = game.state().grid.normalize(to);
            if (!n) continue;
            moved = game.submit(Command::move(me, id, *n)) == CommandError::Ok;
        }
        if (!moved) game.submit(Command::setActivity(me, id, Activity::Skip));
    }
    for (UnitId id : game.unitsNeedingOrders(me)) game.submit(Command::setActivity(me, id, Activity::Skip));
    game.submit(Command::endTurn(me));
}

// Plays every player with the bot until `turns` full turns have passed.
inline void playTurns(sov::Game& game, uint64_t seed, int turns) {
    sov::Rng rng(seed);
    const int stopAt = game.state().turn + turns;
    while (game.state().turn < stopAt) playTurn(game, rng);
}

}  // namespace sovbot
