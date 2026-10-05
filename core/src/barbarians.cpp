// Barbarians (01-map-and-terrain.md, Barbarians; 05-units-and-combat.md,
// Barbarian AI combat; data: barbarians-goody-huts.md, BARBARIAN_* globals).
// The barbarian player has no turn of its own and issues no commands: it acts
// inside the world turn (beginGlobalTurn), so replaying the command log
// replays it exactly. Its behaviour is deliberately simple until the real AI
// (MVP-6): camps appear out of sight, release units, and those units attack
// when the odds look good, raid nearby players once bold enough, and
// otherwise stay near their camp.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

PlayerId Game::barbarianPlayer() const {
    for (const Player& p : state_.players) {
        if (p.barbarian && !p.freeCity) return p.id;
    }
    return kNoPlayer;
}

const Camp* Game::campAt(Hex h) const {
    for (const Camp& c : state_.camps) {
        if (c.pos == h) return &c;
    }
    return nullptr;
}

void Game::linkBarbarians() {
    // The barbarians and the Free Cities are at war with everyone.
    for (const Player& b : state_.players) {
        if (!b.barbarian) continue;
        for (Player& p : state_.players) {
            if (p.id == b.id) continue;
            p.relations[static_cast<size_t>(b.id)].war = true;
            state_.players[static_cast<size_t>(b.id)].relations[static_cast<size_t>(p.id)].war = true;
        }
    }
}

void Game::noteKill(const Unit& victim, const Unit* killer) {
    // Camp boldness: +15 per kill, -10 per unit lost (BARBARIAN_BOLDNESS_PER_*).
    for (Camp& c : state_.camps) {
        if (killer && killer->camp == c.id) c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_KILL");
        if (victim.camp == c.id) c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_UNIT_LOST");
    }
}

void Game::enterPlot(Unit& unit) {
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    if (owner.barbarian || rules_->units[static_cast<size_t>(unit.type)].layer != UnitLayer::Military) return;
    auto it = std::find_if(state_.camps.begin(), state_.camps.end(), [&](const Camp& c) { return c.pos == unit.pos; });
    if (it == state_.camps.end()) return;
    // Clearing a camp pays gold; its surviving units roam on without a home.
    state_.camps.erase(it);
    state_.players[static_cast<size_t>(unit.owner)].gold += Fixed::fromInt(rules_->globalInt("BARBARIAN_CAMP_CLEAR_GOLD"));
}

void Game::processBarbarians() {
    const PlayerId bp = barbarianPlayer();
    if (bp == kNoPlayer) return;
    healAndFortify(bp);
    for (Unit& u : state_.units) {
        if (u.owner == bp) u.movesLeft = Fixed::fromInt(maxMoves(u));
    }
    refreshVisibility(bp);
    placeCamps(bp);
    for (Camp& c : state_.camps) {
        c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_TURN");
        if (--c.spawnTimer <= 0) {
            releaseUnit(c, bp);
            c.spawnTimer = rules_->barbarianTribes[static_cast<size_t>(c.tribe)].spawnTurns;
        }
    }
    std::vector<UnitId> ids;
    for (const Unit& u : state_.units) {
        if (u.owner == bp) ids.push_back(u.id);
    }
    for (UnitId id : ids) barbarianAct(id);
}

void Game::placeCamps(PlayerId bp) {
    int majors = 0;
    for (const Player& p : state_.players) majors += p.alive && !p.barbarian ? 1 : 0;
    const int target = rules_->globalInt("BARBARIAN_CAMP_MAX_PER_MAJOR_CIV") * majors;
    if (static_cast<int>(state_.camps.size()) >= target) return;
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    // A third of the target on the first world turn, then a small chance each turn.
    int toAdd = 0;
    if (state_.turn == 2) {
        toAdd = target * rules_->globalInt("BARBARIAN_CAMP_FIRST_TURN_PERCENT_OF_TARGET_TO_ADD") / 100;
    } else if (rng.chance(static_cast<uint32_t>(rules_->globalInt("BARBARIAN_CAMP_ODDS_OF_NEW_CAMP_SPAWNING")))) {
        toAdd = 1;
    }
    if (toAdd <= 0) return;

    const int cityGap = rules_->globalInt("BARBARIAN_CAMP_MINIMUM_DISTANCE_CITY");
    const int campGap = rules_->globalInt("BARBARIAN_CAMP_MINIMUM_DISTANCE_ANOTHER_CAMP");
    std::vector<Hex> spots;
    for (int i = 0; i < state_.grid.size(); ++i) {
        const Hex h = state_.grid.at(i);
        if (!isLandPassable(state_, *rules_, h) || state_.plot(h).owner != kNoPlayer) continue;
        bool ok = true;
        for (const Unit& u : state_.units) ok = ok && u.pos != h;
        for (const Player& p : state_.players) {
            if (!p.barbarian && p.alive && p.visibility[static_cast<size_t>(i)] == static_cast<uint8_t>(Visibility::Visible))
                ok = false;
        }
        for (const City& c : state_.cities) ok = ok && state_.grid.distance(c.pos, h) >= cityGap;
        for (const Camp& c : state_.camps) ok = ok && state_.grid.distance(c.pos, h) >= campGap;
        if (ok) spots.push_back(h);
    }
    for (int n = 0; n < toAdd && !spots.empty(); ++n) {
        const Hex at = spots[rng.below(static_cast<uint32_t>(spots.size()))];
        spots.erase(std::remove_if(spots.begin(), spots.end(),
                                   [&](const Hex& h) { return state_.grid.distance(h, at) < campGap; }),
                    spots.end());
        // Tribe: the first land tribe whose resource (if any) lies near the camp.
        TypeIndex tribe = kNone;
        for (size_t t = 0; t < rules_->barbarianTribes.size() && tribe == kNone; ++t) {
            const BarbarianTribe& bt = rules_->barbarianTribes[t];
            if (bt.coastal) continue;  // naval tribes wait for naval movement
            bool fits = bt.resource == kNone;
            for (const Hex& h : state_.grid.within(at, bt.resourceRange)) {
                if (fits) break;
                fits = state_.plot(h).resource == bt.resource;
            }
            if (fits) tribe = static_cast<TypeIndex>(t);
        }
        if (tribe == kNone) return;
        Camp camp;
        camp.id = state_.nextCampId++;
        camp.pos = at;
        camp.tribe = tribe;
        camp.spawnTimer = rules_->barbarianTribes[static_cast<size_t>(tribe)].spawnTurns;
        state_.camps.push_back(camp);  // ids only grow, so the vector stays sorted
        releaseUnit(state_.camps.back(), bp);
    }
}

void Game::releaseUnit(Camp& camp, PlayerId bp) {
    int count = 0;
    for (const Unit& u : state_.units) count += u.camp == camp.id ? 1 : 0;
    if (count >= rules_->globalInt("BARBARIAN_MAX_UNITS_PER_CAMP")) return;
    const BarbarianTribe& tribe = rules_->barbarianTribes[static_cast<size_t>(camp.tribe)];
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    const bool ranged = rng.chance(static_cast<uint32_t>(tribe.rangedPercent));
    // The strongest generic unit of the class that at least half the majors can build (BARBARIAN_TECH_PERCENT).
    auto best = [&](const std::string& cls) {
        TypeIndex pick = kNone;
        int majors = 0;
        for (const Player& p : state_.players) majors += p.alive && !p.barbarian ? 1 : 0;
        for (size_t i = 0; i < rules_->units.size(); ++i) {
            const UnitType& ut = rules_->units[i];
            if (ut.unitClass != cls || ut.domain != Domain::Land || ut.layer != UnitLayer::Military) continue;
            int knowing = 0;
            for (const Player& p : state_.players) {
                if (p.alive && !p.barbarian && hasUnlocked(p.id, ut.unlock)) ++knowing;
            }
            if (knowing * 100 < majors * rules_->globalInt("BARBARIAN_TECH_PERCENT")) continue;
            const int strength = ranged ? ut.ranged : ut.combat;
            const UnitType* cur = pick == kNone ? nullptr : &rules_->units[static_cast<size_t>(pick)];
            if (!cur || strength > (ranged ? cur->ranged : cur->combat)) pick = static_cast<TypeIndex>(i);
        }
        return pick;
    };
    TypeIndex type = best(ranged ? "RANGED" : tribe.unitClass);
    if (type == kNone && !ranged) type = best("MELEE");
    if (type == kNone) return;
    std::optional<Hex> spot;
    for (const Hex& h : state_.grid.within(camp.pos, 1)) {
        if (spot) break;
        if (isLandPassable(state_, *rules_, h) && !state_.unitAt(h, UnitLayer::Military, *rules_) &&
            !state_.foreignUnitAt(h, bp) && !state_.cityAt(h))
            spot = h;
    }
    if (!spot) return;
    Unit& u = spawnUnit(type, bp, *spot);
    u.camp = camp.id;
}

void Game::barbarianAct(UnitId id) {
    const Unit* u = state_.unit(id);
    if (!u) return;
    const Camp* camp = nullptr;
    for (const Camp& c : state_.camps) {
        if (c.id == u->camp) camp = &c;
    }
    // Units whose camp is gone roam at full boldness.
    const int bold = camp ? camp->boldness : 100;
    const BarbarianTribe* tribe = camp ? &rules_->barbarianTribes[static_cast<size_t>(camp->tribe)] : nullptr;
    const int raidAt = tribe ? tribe->raidBoldness : 0;
    const int cityAt = tribe ? tribe->attackBoldness : 0;
    const PlayerId me = u->owner;

    auto tryAttack = [&]() {
        const Unit* self = state_.unit(id);
        if (!self || self->movesLeft <= Fixed()) return false;
        const bool ranged = unitRange(*self) > 0;
        for (const Hex& h : state_.grid.within(self->pos, std::max(1, unitRange(*self)))) {
            CombatPreview pv = previewAttack(id, h, ranged);
            if (!pv.valid) continue;
            if (pv.city != kNoCity ? bold < cityAt : !pv.capture && pv.damageToAttackerMax > pv.damageToDefenderMax) continue;
            applyCombat(ranged ? Command::rangedAttack(me, id, h) : Command::attack(me, id, h));
            return true;
        }
        return false;
    };
    if (tryAttack()) return;

    // Pick where to go: the nearest target within 8 plots once bold enough, else home.
    u = state_.unit(id);
    std::optional<Hex> goal;
    int bestDist = 9;
    if (bold >= raidAt) {
        for (const Unit& e : state_.units) {
            const int d = state_.grid.distance(u->pos, e.pos);
            if (atWar(me, e.owner) && d < bestDist) {
                bestDist = d;
                goal = e.pos;
            }
        }
        for (const City& c : state_.cities) {
            const int d = state_.grid.distance(u->pos, c.pos);
            if (bold >= cityAt && d < bestDist) {
                bestDist = d;
                goal = c.pos;
            }
        }
    }
    if (!goal && camp && state_.grid.distance(u->pos, camp->pos) > 2) goal = camp->pos;
    if (!goal) {
        state_.unit(id)->activity = Activity::Fortify;
        return;
    }
    // Head for the free plot next to the goal (or the goal itself) closest to the unit.
    std::vector<Hex> around = state_.grid.within(*goal, 1);
    std::stable_sort(around.begin(), around.end(), [&](const Hex& a, const Hex& b) {
        return state_.grid.distance(u->pos, a) < state_.grid.distance(u->pos, b);
    });
    for (const Hex& n : around) {
        if (n == u->pos) break;  // already as close as it gets
        if (state_.unitAt(n, UnitLayer::Military, *rules_) || state_.foreignUnitAt(n, me) || state_.cityAt(n)) continue;
        if (!findPath(id, n)) continue;
        Unit* mover = state_.unit(id);
        mover->moveTarget = n;
        mover->activity = Activity::Awake;
        advanceUnit(id);
        if (Unit* after = state_.unit(id)) after->moveTarget.reset();  // re-decided every turn
        break;
    }
    tryAttack();
}

}  // namespace sov
