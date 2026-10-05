// Nuclear weapons (05-units-and-combat.md: Nuclear weapons; data: units.md, WMDs). The Manhattan
// Project unlocks building Nuclear Devices, Operation Ivy Thermonuclear Devices (city projects);
// each device held costs its maintenance. A bomber delivers one within its strike range, a Nuclear
// Submarine or the player's Missile Silo within the device's ICBM range. The blast destroys every
// unit in its radius (the Giant Death Robot resists; Sovereign: a leader survives, badly wounded),
// strips city walls, pillages improvements and kills a citizen of a city for each of its plots it
// covers (Sovereign's reading of "citizens working affected tiles"). The ground stays contaminated
// for the fallout turns: not worked, and units on it take PLOT_CONTAMINATION_DAMAGE_BASE a turn.
// A launch only against civs at war with the launcher; victims and onlookers remember it.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
constexpr int kVictimGrievance = 300;  // Sovereign: a nuclear strike outweighs a surprise war
}  // namespace

int Game::wmdsHeld(PlayerId player) const {
    int n = 0;
    for (int32_t k : state_.players[at(player)].wmds) n += k;
    return n;
}

std::vector<Hex> Game::wmdBlast(Hex target, TypeIndex weapon) const {
    return state_.grid.within(target, rules_->wmds[at(weapon)].blastRadius);
}

CommandError Game::wmdProblem(const Command& c) const {
    const Player& p = state_.players[at(c.player)];
    if (c.arg < 0 || at(c.arg) >= rules_->wmds.size() || at(c.arg) >= p.wmds.size() || p.wmds[at(c.arg)] <= 0) return CommandError::NotEnoughResources;
    const auto t = state_.grid.normalize(c.target);
    if (!t || *t != c.target) return CommandError::BadTarget;
    const WmdType& w = rules_->wmds[at(c.arg)];
    if (c.id >= 0) {
        const Unit* u = state_.unit(c.id);
        if (!u || u->owner != c.player) return CommandError::NotYourUnit;
        if (!rules_->units[at(u->type)].deliversWmd || u->movesLeft <= Fixed()) return CommandError::BadUnit;
        const int range = isAircraft(*u) ? unitRange(*u) : w.icbmRange;
        if (state_.grid.distance(u->pos, c.target) > range) return CommandError::BadTarget;
    } else {
        if (c.data.size() != 2) return CommandError::BadUnit;
        const Hex silo{c.data[0], c.data[1]};
        const auto s = state_.grid.normalize(silo);
        if (!s || *s != silo) return CommandError::BadUnit;
        const Plot& sp = state_.plot(silo);
        if (sp.improvement == kNone || rules_->improvements[at(sp.improvement)].id != "IMPROVEMENT_MISSILE_SILO" || sp.owner != c.player ||
            sp.pillagedTurns > 0)
            return CommandError::BadUnit;
        if (state_.grid.distance(silo, c.target) > w.icbmRange) return CommandError::BadTarget;
    }
    // Nothing in the blast may belong to a civ at peace with the launcher.
    const auto peaceful = [&](PlayerId o) { return o != kNoPlayer && o != c.player && !atWar(c.player, o); };
    for (const Hex& h : wmdBlast(c.target, static_cast<TypeIndex>(c.arg))) {
        if (peaceful(state_.plot(h).owner)) return CommandError::BadTarget;
        for (const Unit& u : state_.units) {
            if (u.pos == h && peaceful(u.owner)) return CommandError::BadTarget;
        }
    }
    return CommandError::Ok;
}

void Game::launchWmd(const Command& c) {
    Player& p = state_.players[at(c.player)];
    const WmdType& w = rules_->wmds[at(c.arg)];
    --p.wmds[at(c.arg)];
    ++p.wmdsLaunched;
    if (Unit* u = c.id >= 0 ? state_.unit(c.id) : nullptr) {
        u->movesLeft = Fixed();
        u->moved = true;
        u->activity = Activity::Awake;
    }
    std::vector<PlayerId> victims;
    const auto hit = [&](PlayerId o) {
        if (o != kNoPlayer && o != c.player && std::find(victims.begin(), victims.end(), o) == victims.end()) victims.push_back(o);
    };
    const std::vector<Hex> blast = wmdBlast(c.target, static_cast<TypeIndex>(c.arg));
    std::vector<UnitId> lost;
    for (const Hex& h : blast) {
        for (Unit& u : state_.units) {
            if (u.pos != h) continue;
            hit(u.owner);
            if (rules_->units[at(u.type)].wmdImmune) continue;
            if (isLeader(u)) u.hp = std::min(u.hp, 10);
            else lost.push_back(u.id);
        }
        Plot& pl = state_.plot(h);
        hit(pl.owner);
        pl.fallout = static_cast<uint8_t>(std::min(255, w.falloutTurns));
        if (pl.improvement != kNone) pl.pillagedTurns = static_cast<uint8_t>(std::max<int>(pl.pillagedTurns, std::min(255, w.falloutTurns)));
    }
    for (UnitId id : lost) removeUnit(id);
    for (City& city : state_.cities) {
        int covered = 0;
        for (const Hex& h : blast) {
            covered += state_.plot(h).city == city.id ? 1 : 0;
            if (h == city.pos) {
                city.wallHp = 0;
                city.hp = 1;
            }
        }
        if (covered > 0) city.population = std::max(1, city.population - covered);
    }
    for (PlayerId v : victims) {
        if (state_.players[at(v)].barbarian) continue;
        remember(v, c.player, MemoryKind::UsedWmd, -40, 100);
        addGrievance(v, c.player, kVictimGrievance);
    }
    for (const Player& o : state_.players) {
        if (o.id == c.player || o.barbarian || std::find(victims.begin(), victims.end(), o.id) != victims.end() || !hasMet(o.id, c.player)) continue;
        remember(o.id, c.player, MemoryKind::UsedWmd, -15, 80);
    }
    refreshVisibility(c.player);
    for (PlayerId v : victims) refreshVisibility(v);
}

void Game::processFallout() {
    const int damage = rules_->globalInt("PLOT_CONTAMINATION_DAMAGE_BASE");
    std::vector<UnitId> lost;
    for (Unit& u : state_.units) {
        if (state_.plot(u.pos).fallout == 0 || rules_->units[at(u.type)].wmdImmune || isAircraft(u)) continue;
        u.hp -= damage;
        if (isLeader(u)) u.hp = std::max(1, u.hp);
        else if (u.hp <= 0) lost.push_back(u.id);
    }
    for (UnitId id : lost) removeUnit(id);
    for (Plot& pl : state_.plots) {
        if (pl.fallout > 0) --pl.fallout;
    }
}

}  // namespace sov
