// Air power (05-units-and-combat.md: air units, air combat; 03: Aerodrome). Aircraft live in
// their own layer: they never stack with or defend a plot, and only fly from a base with a free
// air slot (a City Center holds 1, an Aerodrome 2 plus 1 per Hangar or Airport). They rebase
// between bases, strike anything within their range from the base, and are intercepted on the
// way by the strongest defender covering the target: an enemy fighter on patrol (fortified)
// within its range, or an anti-air unit within one tile. Sovereign readings: the rebase range is
// twice the strike range. Aircraft Carriers carry 2 (moving them along) and an Airstrip bases 3.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::isAircraft(const Unit& unit) const { return rules_->units[at(unit.type)].domain == Domain::Air; }

namespace {
// The player's Aerodrome with an Airport on this plot (05: Airlift).
bool airportAt(const GameState& s, const Rules& r, PlayerId player, Hex h) {
    const CityDistrict* d = s.districtAt(h);
    const City* c = s.city(s.plot(h).city);
    const TypeIndex airport = r.building("BUILDING_AIRPORT");
    return d && d->complete && d->pillagedTurns == 0 && r.districts[static_cast<size_t>(d->type)].id == "DISTRICT_AERODROME" && c && c->owner == player &&
           airport != kNone && c->has(airport);
}
}  // namespace

CommandError Game::airliftProblem(UnitId id, Hex to) const {
    const Unit* u = state_.unit(id);
    if (!u) return CommandError::BadUnit;
    const UnitType& ut = rules_->units[static_cast<size_t>(u->type)];
    const TypeIndex civic = rules_->civic("CIVIC_RAPID_DEPLOYMENT");
    if (ut.domain != Domain::Land || ut.layer != UnitLayer::Military || isEmbarked(*u) || u->movesLeft < Fixed::fromInt(maxMoves(*u))) return CommandError::BadTarget;
    if (civic == kNone || !state_.players[static_cast<size_t>(u->owner)].civics.has(civic)) return CommandError::BadTarget;
    auto h = state_.grid.normalize(to);
    if (!h || *h == u->pos || !airportAt(state_, *rules_, u->owner, u->pos) || !airportAt(state_, *rules_, u->owner, *h)) return CommandError::BadTarget;
    return state_.unitAt(*h, UnitLayer::Military, *rules_) ? CommandError::BadTarget : CommandError::Ok;
}

bool Game::unitVisibleTo(PlayerId viewer, const Unit& unit) const {
    if (unit.owner == viewer) return true;
    if (visibility(viewer, unit.pos) != Visibility::Visible) return false;
    // Hidden units (05): stealthy ships by their ability; Camouflage (recon) and Twilight Veil (Warrior Monks) by promotion.
    bool hidden = false;
    for (TypeIndex a : rules_->units[static_cast<size_t>(unit.type)].abilities) {
        for (const UnitEffect& e : rules_->abilities[static_cast<size_t>(a)].effects) hidden = hidden || e.kind == UnitEffectKind::Hidden;
    }
    for (TypeIndex p : unit.promotions) {
        for (const UnitEffect& e : rules_->promotions[static_cast<size_t>(p)].effects) hidden = hidden || e.kind == UnitEffectKind::Hidden;
    }
    if (!hidden) return true;
    for (const City& c : state_.cities) {
        if (c.owner == viewer && state_.grid.distance(c.pos, unit.pos) <= 1) return true;
    }
    const TypeIndex reveal = rules_->ability("ABILITY_REVEAL_STEALTH");
    for (const Unit& o : state_.units) {
        if (o.owner != viewer) continue;
        const int d = state_.grid.distance(o.pos, unit.pos);
        if (d <= 1) return true;
        const std::vector<TypeIndex>& oa = rules_->units[static_cast<size_t>(o.type)].abilities;
        if (reveal != kNone && std::find(oa.begin(), oa.end(), reveal) != oa.end() && d <= unitSight(o)) return true;
    }
    return false;
}

CommandError Game::paradropProblem(UnitId id, Hex to) const {
    const Unit* u = state_.unit(id);
    if (!u) return CommandError::BadUnit;
    const std::vector<TypeIndex> abilities = unitAbilities(*u);
    const TypeIndex drop = rules_->ability("ABILITY_PARADROP");
    if (drop == kNone || std::find(abilities.begin(), abilities.end(), drop) == abilities.end()) return CommandError::BadTarget;
    if (state_.plot(u->pos).owner != u->owner || u->movesLeft < Fixed::fromInt(maxMoves(*u))) return CommandError::BadTarget;
    auto h = state_.grid.normalize(to);
    if (!h || *h == u->pos || state_.grid.distance(u->pos, *h) > 3 || !isLandPassable(state_, *rules_, *h) || state_.cityAt(*h)) return CommandError::BadTarget;
    if (visibility(u->owner, *h) == Visibility::Unrevealed || state_.unitAt(*h, UnitLayer::Military, *rules_) || state_.foreignUnitAt(*h, u->owner))
        return CommandError::BadTarget;
    return CommandError::Ok;
}

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
    int slots = rules_->districts[at(d->type)].airSlots + usedHere(*owner, Gp::Raskova);  // Marina Raskova (07)
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
            strength = combatStrength(d, attacker, true, false);  // the strength formula, as in every fight (05)
        } else if (t.antiAir > 0 && state_.grid.distance(d.pos, target) <= 1) {
            // Anti-air guns and AA ships cover adjacent plots; +25 in a city's territory with Air Defense Initiative (08: Victor).
            const int bonus = territoryGovernorHas(d.pos, d.owner, "GOVERNOR_PROMOTION_AIR_DEFENSE_INITIATIVE") ? 25 : 0;
            strength = t.antiAir + bonus - woundedPenalty(d);
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
