// A deliberately dumb player for soak tests and the headless simulator: founds
// a city with each settler where it stands when allowed, declares random wars,
// attacks at even odds, wanders other units to random known plots, then ends
// the turn. Not the game AI (that is MVP-6).
// It only talks to the core through commands, like any real player.
#pragma once

#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/rng.h"

namespace sovbot {

inline void playTurn(sov::Game& game, sov::Rng& rng) {
    using namespace sov;
    const GameState& s = game.state();
    const PlayerId me = s.currentPlayer;
    // War: now and then pick a fight with another player; offer peace once allowed.
    if (s.turn > 20 && rng.chance(3)) {
        PlayerId target = static_cast<PlayerId>(rng.below(static_cast<uint32_t>(s.players.size())));
        if (game.canDeclareWar(me, target)) game.submit(Command::declareWar(me, target));
    }
    for (const Player& other : s.players) {
        if (game.canMakePeace(me, other.id) && rng.chance(10)) game.submit(Command::makePeace(me, other.id));
    }
    std::vector<UnitId> mine;
    for (const Unit& u : s.units) {
        if (u.owner == me) mine.push_back(u.id);
    }
    for (UnitId id : mine) {
        const Unit* u = game.state().unit(id);
        if (!u || u->moveTarget) continue;
        const UnitType& t = game.rules().units[static_cast<size_t>(u->type)];
        if (t.foundCity && game.submit(Command::foundCity(me, id)) == CommandError::Ok) continue;
        std::vector<TypeIndex> promos = game.availablePromotions(id);
        if (!promos.empty()) {
            game.submit(Command::promote(me, id, promos[rng.below(static_cast<uint32_t>(promos.size()))]));
            continue;
        }
        // Attack an enemy in reach when the odds look even or better.
        bool fought = false;
        for (const Hex& h : game.state().grid.within(u->pos, std::max(1, game.unitRange(*u)))) {
            const bool ranged = game.unitRange(*u) > 0;
            CombatPreview pv = game.previewAttack(id, h, ranged);
            if (!pv.valid) continue;
            if (!pv.capture && pv.damageToAttackerMax > pv.damageToDefenderMax) continue;
            fought = game.submit(ranged ? Command::rangedAttack(me, id, h) : Command::attack(me, id, h)) == CommandError::Ok;
            if (fought) break;
        }
        if (fought) continue;
        u = game.state().unit(id);
        if (!u) continue;
        if (u->charges > 0) {
            // Builders improve where they stand, preferring the resource's own improvement.
            std::vector<TypeIndex> options = game.improvementsAt(me, u->pos);
            if (!options.empty() && game.submit(Command::buildImprovement(me, id, options.front())) == CommandError::Ok) continue;
        }
        bool moved = false;
        // At war, military units head for the nearest enemy unit they know of.
        if (t.layer == UnitLayer::Military && rng.chance(50)) {
            const Unit* best = nullptr;
            for (const Unit& e : game.state().units) {
                if (!game.atWar(me, e.owner) || game.visibility(me, e.pos) != Visibility::Visible) continue;
                if (!best || game.state().grid.distance(u->pos, e.pos) < game.state().grid.distance(u->pos, best->pos)) best = &e;
            }
            if (best) {
                for (const Hex& n : game.state().grid.within(best->pos, 1)) {
                    if (moved) break;
                    if (n != best->pos) moved = game.submit(Command::move(me, id, n)) == CommandError::Ok;
                }
            }
        }
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
