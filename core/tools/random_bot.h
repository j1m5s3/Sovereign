// A deliberately dumb player for soak tests and the headless simulator: founds
// a city with each settler where it stands when allowed, declares random wars,
// attacks at even odds (cities too), strikes with walled cities, sometimes
// razes what it captures, places districts where adjacency is best, goes for nearby enemies and barbarian camps, wanders
// other units to random known plots, then ends the turn. Not the game AI (that is MVP-6).
// It only talks to the core through commands, like any real player.
#pragma once

#include <algorithm>
#include <optional>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/rng.h"

namespace sovbot {

inline void playTurn(sov::Game& game, sov::Rng& rng) {
    using namespace sov;
    if (game.gameOver()) return;
    if (game.battlePending()) game.submit(Command::autoResolveBattle(game.state().currentPlayer));
    const GameState& s = game.state();
    const PlayerId me = s.currentPlayer;
    // Idle assassins go after a random civ.
    for (const Agent& a : std::vector<Agent>(s.agents)) {
        if (a.owner != me || a.target != kNoPlayer) continue;
        game.submit(Command::sendAssassin(me, a.id, static_cast<PlayerId>(rng.below(static_cast<uint32_t>(s.players.size())))));
    }
    // A fallen or captured leader is replaced by the first successor allowed.
    if (s.players[static_cast<size_t>(me)].captor != kNoPlayer) game.submit(Command::abandonLeader(me));
    for (Succession k : {Succession::Heir, Succession::Unit, Succession::Regent}) {
        if (!s.players[static_cast<size_t>(me)].successionPending) break;
        const std::vector<UnitId> units = game.successorUnits(me);
        game.submit(Command::chooseSuccessor(me, k, units.empty() ? kNoUnit : units.front()));
    }
    // War: now and then pick a fight with another player; offer peace once allowed.
    if (s.turn > 20 && rng.chance(3)) {
        PlayerId target = static_cast<PlayerId>(rng.below(static_cast<uint32_t>(s.players.size())));
        if (game.canDeclareWar(me, target)) game.submit(Command::declareWar(me, target));
    }
    for (const Player& other : s.players) {
        if (game.canMakePeace(me, other.id) && rng.chance(10)) game.submit(Command::makePeace(me, other.id));
    }
    // Walled cities shoot at the first enemy in reach; fresh conquests are sometimes burned.
    std::vector<CityId> cities;
    for (const City& c : s.cities) {
        if (c.owner == me) cities.push_back(c.id);
    }
    for (CityId cid : cities) {
        if (game.canRazeCity(me, cid) && rng.chance(25)) {
            game.submit(Command::razeCity(me, cid));
            continue;
        }
        const City* c = game.state().city(cid);
        for (const Hex& h : game.state().grid.within(c->pos, 2)) {
            if (game.canCityStrike(cid, h) && game.submit(Command::cityStrike(me, cid, h)) == CommandError::Ok) break;
        }
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
        // The leader now and then re-equips in a city or takes the escort on its plot.
        if (game.isLeader(*u)) {
            bool equipped = false;
            for (size_t g = 0; g < game.rules().gear.size() && !equipped; ++g) {
                if (rng.chance(4) && game.canEquip(id, static_cast<TypeIndex>(g)))
                    equipped = game.submit(Command::equipGear(me, id, static_cast<TypeIndex>(g))) == CommandError::Ok;
            }
            if (equipped) continue;
            // Now and then it takes a stance in the city it stands in.
            if (const City* here = game.state().cityAt(u->pos); here && here->owner == me && rng.chance(10)) {
                const Stance st = rng.chance(50) ? Stance::Fear : Stance::Benevolence;
                if (game.canTakeStance(me, here->id, st)) game.submit(Command::cityStance(me, here->id, st));
            }
            const Unit* guard = game.state().unitAt(u->pos, UnitLayer::Military, game.rules());
            if (guard && guard->owner == me && guard->escorting != id && rng.chance(30))
                game.submit(Command::linkEscort(me, guard->id, id));
            u = game.state().unit(id);
        }
        // Attack an enemy in reach when the odds look even or better.
        bool fought = false;
        for (const Hex& h : game.state().grid.within(u->pos, std::max(1, game.unitRange(*u)))) {
            const bool ranged = game.unitRange(*u) > 0;
            CombatPreview pv = game.previewAttack(id, h, ranged);
            if (!pv.valid) continue;
            if (!pv.capture && !pv.captureCity && pv.damageToAttackerMax > pv.damageToDefenderMax) continue;
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
        // Military units head for the nearest enemy unit or city they see, or a camp they know of.
        if (t.layer == UnitLayer::Military && rng.chance(50)) {
            std::optional<Hex> best;
            auto consider = [&](Hex h) {
                if (!best || game.state().grid.distance(u->pos, h) < game.state().grid.distance(u->pos, *best)) best = h;
            };
            for (const Unit& e : game.state().units) {
                if (game.atWar(me, e.owner) && game.visibility(me, e.pos) == Visibility::Visible) consider(e.pos);
            }
            for (const City& c : game.state().cities) {
                if (game.atWar(me, c.owner) && game.visibility(me, c.pos) == Visibility::Visible) consider(c.pos);
            }
            for (const Camp& c : game.state().camps) {
                if (game.visibility(me, c.pos) != Visibility::Unrevealed) consider(c.pos);
            }
            if (best) {
                // A free camp is entered (clearing it); anything else is approached.
                if (game.campAt(*best) && game.submit(Command::move(me, id, *best)) == CommandError::Ok) moved = true;
                for (const Hex& n : game.state().grid.within(*best, 1)) {
                    if (moved) break;
                    if (n != *best) moved = game.submit(Command::move(me, id, n)) == CommandError::Ok;
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
        if (items.empty()) continue;
        const ProductionItem pick = items[rng.below(static_cast<uint32_t>(items.size()))];
        Hex at{};
        if (pick.kind == ProductionKind::District && !districtInWork(*game.state().city(cid), game.rules(), pick.type)) {
            // New districts go where their adjacency is best (ties: first plot).
            Fixed best = Fixed::fromInt(-1);
            for (const Hex& h : game.districtPlots(cid, pick.type)) {
                Fixed total;
                for (const Fixed& y : game.districtAdjacency(me, pick.type, h)) total += y;
                if (total > best) {
                    best = total;
                    at = h;
                }
            }
        }
        game.submit(Command::setProduction(me, cid, pick, at));
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
    while (game.state().turn < stopAt && !game.gameOver()) playTurn(game, rng);
}

}  // namespace sovbot
