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
        if (u->charges > 0) {
            // Builders improve where they stand, preferring the resource's own improvement.
            std::vector<TypeIndex> options = game.improvementsAt(me, u->pos);
            if (!options.empty() && game.submit(Command::buildImprovement(me, id, options.front())) == CommandError::Ok) continue;
        }
        bool moved = false;
        for (int attempt = 0; attempt < 8 && !moved; ++attempt) {
            Hex to{u->pos.x + rng.range(-5, 5), u->pos.y + rng.range(-5, 5)};
            auto n = game.state().grid.normalize(to);
            if (!n) continue;
            moved = game.submit(Command::move(me, id, *n)) == CommandError::Ok;
        }
        if (!moved) game.submit(Command::setActivity(me, id, Activity::Skip));
    }
    for (CityId cid : game.citiesNeedingProduction(me)) {
        std::vector<ProductionItem> items = game.buildableItems(cid);
        if (!items.empty()) game.submit(Command::setProduction(me, cid, items[rng.below(static_cast<uint32_t>(items.size()))]));
    }
    // Research: a random available tech and civic; adopt the newest-unlocked
    // government when changes are free, then fill empty policy slots.
    const Player& pl = game.state().players[static_cast<size_t>(me)];
    if (pl.techs.current == kNone) {
        std::vector<TypeIndex> techs = game.availableTechs(me);
        if (!techs.empty()) game.submit(Command::chooseResearch(me, techs[rng.below(static_cast<uint32_t>(techs.size()))]));
    }
    if (pl.civics.current == kNone) {
        std::vector<TypeIndex> civics = game.availableCivics(me);
        if (!civics.empty()) game.submit(Command::chooseCivic(me, civics[rng.below(static_cast<uint32_t>(civics.size()))]));
    }
    for (size_t g = game.rules().governments.size(); g-- > 0;) {
        const GovernmentType& gt = game.rules().governments[g];
        const int current = pl.government == kNone ? -1 : game.rules().governments[static_cast<size_t>(pl.government)].tier;
        if (gt.tier > current && game.submit(Command::changeGovernment(me, static_cast<TypeIndex>(g))) == CommandError::Ok) break;
    }
    for (size_t slot = 0; slot < pl.policies.size(); ++slot) {
        if (pl.policies[slot] != kNone) continue;
        std::vector<TypeIndex> fits;
        for (size_t k = 0; k < game.rules().policies.size(); ++k) {
            if (game.canSetPolicy(me, static_cast<int>(slot), static_cast<TypeIndex>(k))) fits.push_back(static_cast<TypeIndex>(k));
        }
        if (!fits.empty()) game.submit(Command::setPolicy(me, static_cast<int>(slot), fits[rng.below(static_cast<uint32_t>(fits.size()))]));
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
