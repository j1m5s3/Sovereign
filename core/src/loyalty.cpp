// Loyalty and the Free Cities [R&F] (specs/civ6/02-cities.md, Loyalty): citizen pressure,
// per-turn change (ages, governors, religion, amenities...), revolt at 0 and flipping back.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
// Pressure a city exerts on a plot: population x (cutoff - distance), within the cutoff.
Fixed pressureFrom(const GameState& s, const Rules& r, const City& from, Hex at) {
    const int cutoff = r.globalInt("CITIZEN_IDENTITY_PRESSURE_RADIUS_CUTOFF");
    const int d = s.grid.distance(from.pos, at);
    if (d >= cutoff) return Fixed();
    // Each civ's citizens press by its age (02: Loyalty): x1.5 in a Golden or Heroic Age, x0.5 in a Dark Age.
    const Age age = s.players[static_cast<size_t>(from.owner)].age;
    const Fixed factor = age == Age::Golden || age == Age::Heroic ? Fixed::ratio(3, 2) : age == Age::Dark ? Fixed::ratio(1, 2) : Fixed::fromInt(1);
    Fixed p = Fixed::fromInt(from.population * (cutoff - d)) * factor;
    if (from.capital) p += Fixed::fromInt(from.population * (cutoff - d));  // capitals count twice (age factor 1)
    return p;
}
}  // namespace

PlayerId Game::freeCityPlayer() const {
    for (const Player& p : state_.players) {
        if (p.freeCity) return p.id;
    }
    return kNoPlayer;
}

PlayerId Game::ensureFreeCityPlayer() {
    if (const PlayerId f = freeCityPlayer(); f != kNoPlayer) return f;
    // Created on the first revolt so games without one keep their player list.
    Player f;
    f.id = static_cast<PlayerId>(state_.players.size());
    f.barbarian = true;
    f.freeCity = true;
    fitPlayerToRules(f, *rules_);
    f.visibility.assign(static_cast<size_t>(state_.grid.size()), 0);
    state_.players.push_back(std::move(f));
    for (Player& p : state_.players) p.relations.resize(state_.players.size());
    linkBarbarians();
    refreshVisibility(state_.players.back().id);
    return state_.players.back().id;
}

Fixed Game::loyaltyPressure(const City& city) const {
    Fixed domestic, foreign;
    for (const City& o : state_.cities) {
        // A Cultural alliance (08): no loyalty pressure between the allies.
        if (o.owner != city.owner && alliance(o.owner, city.owner) == AllianceType::Cultural) continue;
        // City-states and Free Cities press on no one else (Civ VI Loyalty guide: they have no impact).
        if (o.owner != city.owner && (isCityState(o.owner) || state_.players[static_cast<size_t>(o.owner)].freeCity)) continue;
        const Fixed p = pressureFrom(state_, *rules_, o, city.pos);
        if (o.owner == city.owner) domestic += p;
        else foreign += p;
    }
    const Fixed lower = std::min(domestic, foreign) + Fixed::ratio(1, 2);
    const Fixed net = (domestic - foreign) * 10 / lower;
    const Fixed cap = Fixed::fromInt(rules_->globalInt("LOYALTY_PER_TURN_FROM_NEARBY_CITIZEN_PRESSURE_MAX_LOYALTY"));
    return std::clamp(net, -cap, cap);
}

Fixed Game::loyaltyPerTurn(CityId id) const {
    const City* c = state_.city(id);
    if (!c) return Fixed();
    Fixed change = loyaltyPressure(*c);
    // A Feared ruler's cities rarely flip: pressure losses are halved; a Beloved one gains loyalty (§8.1).
    if (change < Fixed() && feared(c->owner)) change = change / 2;
    if (beloved(c->owner)) change += Fixed::fromInt(rules_->globalInt("REPUTATION_BELOVED_LOYALTY"));
    if (state_.players[static_cast<size_t>(c->owner)].freeCity) {
        return change + Fixed::fromInt(rules_->globalInt("IDENTITY_PER_TURN_FROM_FREE_CITIES"));
    }
    // City-states hold to themselves (IDENTITY_PER_TURN_FROM_CITY_STATES).
    if (isCityState(c->owner)) return change + Fixed::fromInt(rules_->globalInt("IDENTITY_PER_TURN_FROM_CITY_STATES"));
    // Religion: a majority religion the owner founded steadies the city; one another civ founded unsettles it.
    if (const int maj = cityMajorityReligion(*c); maj >= 0) {
        const PlayerId founder = state_.religions[static_cast<size_t>(maj)].founder;
        if (founder == c->owner) change += Fixed::fromInt(rules_->globalInt("IDENTITY_PER_TURN_FROM_RELIGION_MATCHING_FOUNDED"));
        else if (founder != kNoPlayer) change += Fixed::fromInt(rules_->globalInt("IDENTITY_PER_TURN_FROM_RELIGION_MISMATCHING_FOUNDED"));
    }
    const CityReport rep = cityReport(id);
    if (!rules_->happiness.empty()) change += Fixed::fromInt(rules_->happiness[static_cast<size_t>(rep.happiness)].loyaltyPerTurn);
    if (rep.yields[static_cast<size_t>(YieldType::Food)] < rep.foodConsumption)
        change += Fixed::fromInt(rules_->globalInt("IDENTITY_PER_TURN_FROM_STARVATION"));
    change += sumCityModifiers(state_, *rules_, *c, ModEffect::CityLoyalty);
    // Communications Office (04): +1 a turn per title of the city's own established governor.
    if (policyIs(c->owner, "POLICY_COMMUNICATIONS_OFFICE")) {
        PlayerId holder = kNoPlayer;
        if (const Governor* g = establishedGovernor(*c, &holder); g && holder == c->owner) change += Fixed::fromInt(static_cast<int>(g->promotions.size()));
    }
    // Migration Treaty (World Congress) on the owner: -5 (A) or +5 (B) loyalty per turn.
    if (const PassedResolution* mt = passed(ResolutionKind::MigrationTreaty); mt && mt->target == c->owner)
        change += Fixed::fromInt(mt->option == 0 ? -5 : 5);
    // An heir's trait steadies every city (Divine Right, Shogunate Law, The Consolidator).
    change += Fixed::fromInt(civAbility(c->owner).cityLoyalty);
    // Leader ability: conquered cities settle down faster (King of Kings).
    if (c->originalOwner != kNoPlayer && c->originalOwner != c->owner) change += Fixed::fromInt(civAbility(c->owner).capturedCityLoyalty);
    // An established governor of the owner steadies the city (08: Governors, IdentityPressure).
    PlayerId govOwner = kNoPlayer;
    if (const Governor* g = establishedGovernor(*c, &govOwner); g && govOwner == c->owner)
        change += Fixed::fromInt(rules_->governors[static_cast<size_t>(g->type)].loyalty + civAbility(c->owner).governorLoyalty);
    // Wonders (03): the Colosseum steadies the owner's cities within 6 tiles; the Statue of Liberty keeps them loyal.
    if (nearOwnWonder(*c, wonderType(W::Colosseum), 6)) change += Fixed::fromInt(2);
    if (change < Fixed() && nearOwnWonder(*c, wonderType(W::Liberty), 6)) change = Fixed();
    return change;
}

const LoyaltyLevel* Game::loyaltyLevel(const City& city) const {
    const LoyaltyLevel* level = nullptr;
    for (size_t i = 0; i < rules_->loyaltyLevels.size(); ++i) {
        const LoyaltyLevel& l = rules_->loyaltyLevels[i];
        // Under Fear the city does not count as in Unrest (leader doc §4).
        const bool floor = fearActive(city) && i == 1;
        if (city.loyalty >= l.minLoyalty || floor) level = &l;
    }
    return level;
}

void Game::transferCity(CityId id, PlayerId to, int loyalty) {
    City& c = *state_.city(id);
    const PlayerId from = c.owner;
    // The units in the city go with it; a leader goes home instead (or is held if it has none).
    std::vector<UnitId> there;
    for (const Unit& u : state_.units) {
        if (u.pos == c.pos && u.owner == from) there.push_back(u.id);
    }
    c.owner = to;
    c.loyalty = loyalty;
    c.queue.clear();
    c.progress.clear();
    c.overflow = Fixed();
    c.locked.clear();
    const bool wasCapital = c.capital;
    if (wasCapital) {
        c.capital = false;
        c.buildings.erase(std::remove_if(c.buildings.begin(), c.buildings.end(),
                                         [&](TypeIndex b) { return rules_->buildings[static_cast<size_t>(b)].granted; }),
                          c.buildings.end());
    }
    for (Plot& p : state_.plots) {
        if (p.city == id) p.owner = to;
    }
    City* home = nullptr;
    if (wasCapital) {
        for (City& o : state_.cities) {
            if (o.owner == from && (!home || o.foundedTurn < home->foundedTurn)) home = &o;
        }
        if (home) {
            home->capital = true;
            for (size_t b = 0; b < rules_->buildings.size(); ++b) {
                if (!rules_->buildings[b].granted) continue;
                auto it = std::lower_bound(home->buildings.begin(), home->buildings.end(), static_cast<TypeIndex>(b));
                if (it == home->buildings.end() || *it != static_cast<TypeIndex>(b)) home->buildings.insert(it, static_cast<TypeIndex>(b));
            }
            assignCitizens(*home);
        }
    } else {
        for (City& o : state_.cities) {
            if (o.owner == from && o.capital) home = &o;
        }
    }
    for (UnitId uid : there) {
        Unit* u = state_.unit(uid);
        if (!u) continue;
        if (isLeader(*u)) {
            if (home) {
                if (Unit* e = escortMut(*u)) e->escorting = kNoUnit;
                u->pos = home->pos;
                u->moveTarget.reset();
            } else {
                leaderLost(uid, to, true);
            }
            continue;
        }
        u->owner = to;
        u->moveTarget.reset();
        u->escorting = kNoUnit;
        u->activity = Activity::Fortify;
    }
    groundAircraft(*state_.city(id));  // aircraft left at its Aerodrome cannot stay in a foreign base
    assignCitizens(*state_.city(id));
    refreshVisibility(from);
    refreshVisibility(to);
    checkElimination(from);
}

void Game::processLoyalty(PlayerId pid) {
    const int maximum = rules_->globalInt("LOYALTY_MAXIMUM");
    std::vector<CityId> ids;
    for (const City& c : state_.cities) {
        if (c.owner == pid) ids.push_back(c.id);
    }
    // Changes are worked out first so a revolt does not shift its neighbours' numbers this turn.
    std::vector<int> changes;
    for (CityId id : ids) changes.push_back(static_cast<int>(loyaltyPerTurn(id).round()));
    for (size_t i = 0; i < ids.size(); ++i) {
        City* c = state_.city(ids[i]);
        if (!c || c->owner != pid) continue;
        c->loyalty = std::clamp(c->loyalty + changes[i], 0, maximum);
        if (c->loyalty == 0) {
            transferCity(ids[i], ensureFreeCityPlayer(), rules_->globalInt("LOYALTY_START") / 2);
            continue;
        }
        // Unhappiness (02: Amenities): rebellion points by the city's mood, each a REBELLION_CHANCE_PER_POINT% chance a
        // turn of rebels rising beside it, then REBELLION_COOLDOWN_TURNS of quiet.
        if (!rules_->happiness.empty()) {
            const CityReport rep = cityReport(c->id);
            c->rebellion = std::max(0, c->rebellion + rules_->happiness[static_cast<size_t>(rep.happiness)].rebellionPoints);
            if (c->rebellion > 0 && state_.turn >= c->rebellionCooldown) {
                const int chance = static_cast<int>(rules_->global("REBELLION_CHANCE_PER_POINT").toInt()) * c->rebellion;
                if (static_cast<int>(state_.rng.get(RngStream::Gameplay).below(100)) < chance) {
                    c->rebellion = 0;
                    c->rebellionCooldown = state_.turn + rules_->globalInt("REBELLION_COOLDOWN_TURNS");
                    rebellion(*c);
                    c = state_.city(ids[i]);
                    if (!c || c->owner != pid) continue;
                }
            }
        }
        // Too much iron fist: a Feared ruler's city in Unrest, once Fear has worn off, may rebel (§8.2).
        const bool unrest = !rules_->loyaltyLevels.empty() && c->loyalty < rules_->loyaltyLevels[1].minLoyalty;
        if (unrest && feared(pid) && !fearActive(*c) &&
            static_cast<int>(state_.rng.get(RngStream::Gameplay).below(100)) < rules_->globalInt("REBELLION_PERCENT"))
            rebellion(*c);
    }
}

void Game::processFreeCities() {
    const PlayerId fc = freeCityPlayer();
    if (fc == kNoPlayer) return;
    const int maximum = rules_->globalInt("LOYALTY_MAXIMUM");
    std::vector<CityId> ids;
    for (const City& c : state_.cities) {
        if (c.owner == fc) ids.push_back(c.id);
    }
    for (CityId id : ids) {
        City& c = *state_.city(id);
        c.loyalty = std::clamp(c.loyalty + static_cast<int>(loyaltyPerTurn(id).round()), 0, maximum);
        if (c.loyalty > 0) continue;
        // It joins the civ whose citizens press on it hardest (ties: the lowest player id).
        PlayerId best = kNoPlayer;
        Fixed bestPressure;
        for (const Player& p : state_.players) {
            if (!p.alive || p.barbarian || p.cityState != kNone) continue;  // only a major civ takes it in
            Fixed pressure;
            for (const City& o : state_.cities) {
                if (o.owner == p.id) pressure += pressureFrom(state_, *rules_, o, c.pos);
            }
            if (pressure > bestPressure) {
                bestPressure = pressure;
                best = p.id;
            }
        }
        if (best != kNoPlayer) {
            const bool free = state_.players[static_cast<size_t>(c.owner)].freeCity;
            transferCity(id, best, rules_->globalInt("LOYALTY_AFTER_TRANSFERRED_BY_CULTURAL_IDENTITY"));
            if (free) awardMoment(best, "MOMENT_FREE_CITY_JOINS");  // 09
        }
        else c.loyalty = 1;
    }
}

}  // namespace sov
