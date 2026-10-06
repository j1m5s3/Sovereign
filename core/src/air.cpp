// Air power (05-units-and-combat.md: air units, air combat; 03: Aerodrome). Aircraft live in
// their own layer: they never stack with or defend a plot, and only fly from a base with a free
// air slot (a City Center holds 1, an Aerodrome 2 plus 1 per Hangar or Airport). They rebase
// between bases, strike anything within their range from the base, and are intercepted on the
// way by the strongest defender covering the target: an enemy fighter on patrol (fortified)
// within its range, or an anti-air unit within one tile. Sovereign readings: the rebase range is
// twice the strike range. Aircraft Carriers carry 2 (moving them along) and an Airstrip bases 3.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::isAircraft(const Unit& unit) const { return rules_->units[at(unit.type)].domain == Domain::Air; }

int Game::airSlots(PlayerId player, Hex base) const {
    // Carriers on the plot and an Airstrip in the player's land add to any city or Aerodrome slots.
    int carried = 0;
    for (const Unit& u : state_.units) {
        // Flight Deck, Hangar Deck, Folding Wings (05): +1 each.
        if (u.pos == base && u.owner == player && rules_->units[at(u.type)].airSlots > 0) carried += rules_->units[at(u.type)].airSlots + unitEffectTotal(u, UnitEffectKind::AirSlots);
    }
    const Plot& here = state_.plot(base);
    if (here.improvement != kNone && here.owner == player && here.pillagedTurns == 0) carried += rules_->improvements[at(here.improvement)].airSlots;
    return carried + baseAirSlots(player, base);
}

int Game::baseAirSlots(PlayerId player, Hex base) const {
    const City* center = state_.cityAt(base);
    if (center && center->owner == player) {
        const TypeIndex cc = rules_->district("DISTRICT_CITY_CENTER");
        return cc == kNone ? 1 : rules_->districts[at(cc)].airSlots;
    }
    const CityDistrict* d = state_.districtAt(base);
    if (!d || !d->complete || rules_->districts[at(d->type)].airSlots <= 0) return 0;
    const Plot& p = state_.plot(base);
    const City* owner = p.city == kNoCity ? nullptr : state_.city(p.city);
    if (!owner || owner->owner != player) return 0;
    int slots = rules_->districts[at(d->type)].airSlots;
    for (TypeIndex b : owner->buildings) {
        const BuildingType& bt = rules_->buildings[at(b)];
        if (bt.districtType == d->type) slots += bt.airSlots;
    }
    return slots;
}

int Game::aircraftAt(Hex base) const {
    int n = 0;
    for (const Unit& u : state_.units) n += u.pos == base && isAircraft(u) ? 1 : 0;
    return n;
}

std::optional<Hex> Game::freeAirBase(const City& city) const {
    if (aircraftAt(city.pos) < airSlots(city.owner, city.pos)) return city.pos;
    for (const CityDistrict& d : city.districts) {
        if (d.complete && aircraftAt(d.pos) < airSlots(city.owner, d.pos)) return d.pos;
    }
    return std::nullopt;
}

int Game::rebaseRange(const Unit& unit) const { return 2 * std::max(1, unitRange(unit)); }

CommandError Game::rebaseProblem(UnitId id, Hex to) const {
    const Unit* u = state_.unit(id);
    if (!u || !isAircraft(*u)) return CommandError::BadUnit;
    auto t = state_.grid.normalize(to);
    if (!t || *t != to || to == u->pos || u->movesLeft <= Fixed()) return CommandError::BadTarget;
    if (state_.grid.distance(u->pos, to) > rebaseRange(*u)) return CommandError::BadTarget;
    if (aircraftAt(to) >= airSlots(u->owner, to)) return CommandError::BadTarget;
    return CommandError::Ok;
}

// The strongest interception covering a strike on `target` by `attacker`: (strength, unit or -1).
std::pair<int, UnitId> Game::interception(const Unit& attacker, Hex target) const {
    int best = 0;
    UnitId by = kNoUnit;
    for (const Unit& d : state_.units) {
        if (!atWar(attacker.owner, d.owner)) continue;
        const UnitType& t = rules_->units[at(d.type)];
        int strength = 0;
        if (isAircraft(d)) {
            // A fighter on patrol (fortified at its base) covers its strike range.
            if (t.ranged <= 0 || d.activity != Activity::Fortify || state_.grid.distance(d.pos, target) > unitRange(d)) continue;
            strength = t.combat * d.hp / 100;
        } else if (t.antiAir > 0 && state_.grid.distance(d.pos, target) <= 1) {
            strength = t.antiAir * d.hp / 100;  // anti-air guns and AA ships cover adjacent plots
        }
        if (strength > best) {
            best = strength;
            by = d.id;
        }
    }
    return {best, by};
}

// Aircraft whose base can no longer hold them (a sunk carrier, a pillaged Airstrip) are lost.
void Game::checkAirBases() {
    std::vector<UnitId> lost;
    for (const Unit& u : state_.units) {
        if (!isAircraft(u)) continue;
        int before = 0;  // aircraft ahead of it on the same plot (by id) keep the slots first
        for (const Unit& o : state_.units) before += o.pos == u.pos && o.id < u.id && isAircraft(o) ? 1 : 0;
        if (before >= airSlots(u.owner, u.pos)) lost.push_back(u.id);
    }
    for (UnitId id : lost) removeUnit(id);
}

// Aircraft taken with a city or its Aerodrome are lost.
void Game::groundAircraft(const City& city) {
    std::vector<UnitId> lost;
    for (const Unit& u : state_.units) {
        if (!isAircraft(u) || u.owner == city.owner) continue;
        bool here = u.pos == city.pos;
        for (const CityDistrict& d : city.districts) here = here || u.pos == d.pos;
        if (here) lost.push_back(u.id);
    }
    for (UnitId id : lost) removeUnit(id);
}

}  // namespace sov
