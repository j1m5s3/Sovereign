// Builders, tile improvements, harvesting, luxury amenities and strategic
// stockpiles (specs/civ6/01-map-and-terrain.md, 02-cities.md; data: improvements.md).
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
constexpr uint8_t kPillagedDistrictTurns = 10;
}  // namespace

namespace {
template <typename T>
bool contains(const std::vector<T>& v, T x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

int speedPercent(const GameState& s, const Rules& r) {
    return r.speeds[static_cast<size_t>(r.speed(s.setup.speed))].costPercent;
}
}  // namespace

// ------------------------------------------------------------------ queries

bool Game::resourceVisible(PlayerId player, Hex at) const {
    const Plot& p = state_.plot(at);
    return p.resource != kNone && hasUnlocked(player, rules_->resources[static_cast<size_t>(p.resource)].reveal);
}

bool Game::resourceImproved(Hex at) const {
    const Plot& p = state_.plot(at);
    if (p.resource == kNone) return false;
    if (state_.cityAt(at)) return true;  // a city center counts as improving its resource
    return p.improvement != kNone &&
           contains(rules_->improvements[static_cast<size_t>(p.improvement)].validResources, p.resource);
}

bool Game::canImproveAt(PlayerId player, Hex at, TypeIndex improvement) const {
    if (improvement < 0 || static_cast<size_t>(improvement) >= rules_->improvements.size()) return false;
    const Plot& p = state_.plot(at);
    if (p.owner != player || p.city == kNoCity || state_.cityAt(at) || state_.districtAt(at) || state_.wonderAt(at) != kNone || p.improvement == improvement)
        return false;
    const ImprovementType& im = rules_->improvements[static_cast<size_t>(improvement)];
    if (!hasUnlocked(player, im.unlock)) return false;
    // Civ unique improvements: their civ only, some on a river or at the edge of its land.
    if (im.uniqueTo != kNone && im.uniqueTo != state_.players[static_cast<size_t>(player)].civ) return false;
    if (im.needsRiver && !isRiverAdjacent(state_, at)) return false;
    if (p.park) return false;  // a National Park keeps its land as it is (07)
    if (im.minAppeal > -100 && plotAppeal(at) < im.minAppeal) return false;  // Seaside Resort: Breathtaking (07)
    if (im.coastal) {
        bool coast = false;
        for (const Hex& n : state_.grid.within(at, 1)) coast = coast || rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].shallowWater;
        if (!coast) return false;
    }
    if (im.borderOnly) {
        bool edge = false;
        for (const Hex& n : state_.grid.within(at, 1)) edge = edge || state_.plot(n).owner != player;
        if (!edge) return false;
    }
    // A visible resource only takes the improvements that work it.
    if (resourceVisible(player, at)) return contains(im.validResources, p.resource);
    if (p.feature != kNone) return contains(im.validFeatures, p.feature);
    return contains(im.validTerrains, p.terrain);
}

std::vector<TypeIndex> Game::improvementsAt(PlayerId player, Hex at) const {
    std::vector<TypeIndex> out;
    for (size_t i = 0; i < rules_->improvements.size(); ++i) {
        if (canImproveAt(player, at, static_cast<TypeIndex>(i))) out.push_back(static_cast<TypeIndex>(i));
    }
    return out;
}

bool Game::canHarvestAt(PlayerId player, Hex at) const {
    const Plot& p = state_.plot(at);
    if (p.owner != player || p.city == kNoCity || state_.cityAt(at) || p.improvement != kNone) return false;
    if (p.feature != kNone) {
        const FeatureType& f = rules_->features[static_cast<size_t>(p.feature)];
        return f.removable && !f.removeTech.none() && hasUnlocked(player, f.removeTech);
    }
    if (!resourceVisible(player, at)) return false;
    const ResourceType& r = rules_->resources[static_cast<size_t>(p.resource)];
    return !r.harvestTech.none() && hasUnlocked(player, r.harvestTech);
}

Yields Game::improvementYields(Hex at, PlayerId owner) const {
    Yields y{};
    const Plot& p = state_.plot(at);
    if (p.improvement == kNone) return y;
    const ImprovementType& im = rules_->improvements[static_cast<size_t>(p.improvement)];
    y = im.yields;
    for (const ImprovementBonus& b : im.bonuses) {
        if (hasUnlocked(owner, b.unlock)) y[static_cast<size_t>(b.yield)] += b.amount;
    }
    for (const ImprovementAdjacency& a : im.adjacency) {
        if (!a.needs.none() && !hasUnlocked(owner, a.needs)) continue;
        if (!a.obsoleteWith.none() && hasUnlocked(owner, a.obsoleteWith)) continue;
        int n = 0;
        for (const Hex& h : state_.grid.within(at, 1)) {
            if (h != at && state_.plot(h).improvement == a.improvement) ++n;
        }
        y[static_cast<size_t>(a.yield)] += a.amount * (n / a.per);
    }
    // A neighbouring unique improvement that feeds this one (Nilometer: adjacent Farms +1 Food).
    for (const Hex& h : state_.grid.within(at, 1)) {
        const TypeIndex ni = state_.plot(h).improvement;
        if (h == at || ni == kNone) continue;
        const ImprovementType& n = rules_->improvements[static_cast<size_t>(ni)];
        if (n.adjacentImprovement == p.improvement) y[static_cast<size_t>(n.adjacentYield)] += Fixed::fromInt(n.adjacentAmount);
    }
    return y;
}

Fixed Game::improvementHousing(const City& city) const {
    Fixed total;
    for (const Hex& h : state_.grid.within(city.pos, 3)) {
        const Plot& p = state_.plot(h);
        if (p.city == city.id && p.improvement != kNone) {
            total += rules_->improvements[static_cast<size_t>(p.improvement)].housing;
            // Civ ability: farms next to a river add housing (Aztec Chinampas).
            if (rules_->improvements[static_cast<size_t>(p.improvement)].id == "IMPROVEMENT_FARM" && isRiverAdjacent(state_, h))
                total += civAbility(city.owner).freshWaterFarmHousing;
        }
    }
    return total;
}

int Game::luxuryAmenities(const City& city) const {
    // Each luxury type the player has improved gives +1 amenity to up to
    // `amenityCities` cities. Sovereign reading: the largest cities get them
    // first (ties: oldest city); Civ gives them to the cities needing them most.
    const PlayerId owner = city.owner;
    std::vector<const City*> mine;
    for (const City& c : state_.cities) {
        if (c.owner == owner) mine.push_back(&c);
    }
    std::stable_sort(mine.begin(), mine.end(), [](const City* a, const City* b) { return a->population > b->population; });
    const int rank = static_cast<int>(std::find(mine.begin(), mine.end(), &city) - mine.begin());
    // Access after deals: copies traded away are lost, copies traded in count (08: Trade Deal).
    std::vector<uint8_t> have(rules_->resources.size(), 0);
    for (size_t r = 0; r < rules_->resources.size(); ++r) {
        if (rules_->resources[r].cls == ResourceClass::Luxury) have[r] = hasLuxury(owner, static_cast<TypeIndex>(r)) ? 1 : 0;
    }
    int amenities = 0;
    for (size_t r = 0; r < have.size(); ++r) {
        if (have[r] && rank < rules_->resources[r].amenityCities) ++amenities;
    }
    return amenities;
}

bool Game::hasStrategicFor(PlayerId player, TypeIndex unitType) const {
    const UnitType& u = rules_->units[static_cast<size_t>(unitType)];
    if (u.strategicResource == kNone || u.strategicCost <= 0) return true;
    const Player& p = state_.players[static_cast<size_t>(player)];
    return p.stockpile[static_cast<size_t>(u.strategicResource)] >= u.strategicCost;
}

bool Game::unitObsolete(PlayerId player, TypeIndex unitType) const {
    const UnitType& u = rules_->units[static_cast<size_t>(unitType)];
    if (!u.obsoleteWith.none() && hasUnlocked(player, u.obsoleteWith)) return true;
    // Once the upgrade can be trained, the old unit cannot (engine; unverified
    // whether the upgrade's strategic resource must be on hand).
    if (u.upgradesTo == kNone) return false;
    const UnitType& up = rules_->units[static_cast<size_t>(u.upgradesTo)];
    return !up.mustPurchase && hasUnlocked(player, up.unlock) && hasStrategicFor(player, u.upgradesTo);
}

int Game::countImprovedPlots(PlayerId player, TypeIndex improvement, bool onResourceOnly) const {
    int n = 0;
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        const Plot& p = state_.plots[i];
        if (p.owner != player || p.improvement == kNone) continue;
        if (improvement != kNone && p.improvement != improvement) continue;
        if (onResourceOnly && !resourceImproved(state_.grid.at(static_cast<int>(i)))) continue;
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------- commands

CommandError Game::validateBuilder(const Command& c) const {
    const Unit& u = *state_.unit(c.id);
    if (u.charges <= 0 || u.movesLeft <= Fixed()) {
        return c.type == CommandType::Harvest ? CommandError::CannotHarvest : CommandError::CannotImprove;
    }
    if (c.type == CommandType::Harvest) {
        return canHarvestAt(c.player, u.pos) ? CommandError::Ok : CommandError::CannotHarvest;
    }
    if (c.arg < 0 || c.arg > INT16_MAX) return CommandError::CannotImprove;
    // Military Engineers build their own improvements (Fort, Airstrip, Missile Silo); Builders the rest.
    if (static_cast<size_t>(c.arg) < rules_->improvements.size()) {
        const TypeIndex by = rules_->improvements[static_cast<size_t>(c.arg)].builtBy;
        const bool engineer = rules_->units[static_cast<size_t>(u.type)].id == "UNIT_MILITARY_ENGINEER";
        if (by != kNone ? u.type != by : engineer) return CommandError::CannotImprove;
    }
    // A Mountain Tunnel goes on a neighbouring mountain, named as the target.
    if (static_cast<size_t>(c.arg) < rules_->improvements.size() && rules_->improvements[static_cast<size_t>(c.arg)].tunnel) {
        const std::vector<Hex> sites = tunnelSites(c.player, c.id);
        return std::find(sites.begin(), sites.end(), c.target) != sites.end() ? CommandError::Ok : CommandError::CannotImprove;
    }
    return canImproveAt(c.player, u.pos, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotImprove;
}

// ---------------------------------------------------------------- formations (05: Corps and Armies)

CommandError Game::formationProblem(PlayerId player, UnitId unit, UnitId with) const {
    const Unit* u = state_.unit(unit);
    const Unit* w = state_.unit(with);
    if (!u || !w || u->owner != player || w->owner != player || u->id == w->id) return CommandError::NotYourUnit;
    const UnitType& t = rules_->units[static_cast<size_t>(u->type)];
    if (u->type != w->type || t.layer != UnitLayer::Military || (t.domain != Domain::Land && t.domain != Domain::Sea) || isLeader(*u)) return CommandError::BadUnit;
    if (u->movesLeft <= Fixed() || w->movesLeft <= Fixed() || state_.grid.distance(u->pos, w->pos) > 1) return CommandError::BadTarget;
    const Player& p = state_.players[static_cast<size_t>(player)];
    const auto has = [&](const char* civic) { const TypeIndex c = rules_->civic(civic); return c != kNone && p.civics.has(c); };
    if (u->formation == 0 && w->formation == 0) return has("CIVIC_NATIONALISM") ? CommandError::Ok : CommandError::BadUnit;
    if (u->formation == 1 && w->formation == 0) return has("CIVIC_MOBILIZATION") ? CommandError::Ok : CommandError::BadUnit;
    return CommandError::BadUnit;
}

// ---------------------------------------------------------------- pillage (05: Pillage)

bool Game::districtPillaged(Hex plot) const {
    const CityDistrict* d = state_.districtAt(plot);
    return d && d->pillagedTurns > 0;
}

CommandError Game::pillageProblem(PlayerId player, UnitId unit) const {
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    const UnitType& ut = rules_->units[static_cast<size_t>(u->type)];
    if (ut.layer != UnitLayer::Military || ut.domain != Domain::Land || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    const Plot& p = state_.plot(u->pos);
    // Only an enemy's land (barbarians are at war with everyone).
    if (p.owner == kNoPlayer || p.owner == player || !atWar(player, p.owner)) return CommandError::BadTarget;
    if (state_.cityAt(u->pos)) return CommandError::BadTarget;
    const bool improvement = p.improvement != kNone && p.pillagedTurns == 0;
    const CityDistrict* d = state_.districtAt(u->pos);
    const bool district = d && d->complete && d->pillagedTurns == 0;
    return improvement || district ? CommandError::Ok : CommandError::BadTarget;
}

CommandError Game::coastalRaidProblem(PlayerId player, UnitId unit, Hex at) const {
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    const UnitType& ut = rules_->units[static_cast<size_t>(u->type)];
    if (ut.domain != Domain::Sea || (ut.unitClass != "NAVAL_MELEE" && ut.unitClass != "NAVAL_RAIDER") || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    const auto t = state_.grid.normalize(at);
    if (!t || *t != at || state_.grid.distance(u->pos, at) != 1) return CommandError::BadTarget;
    const Plot& p = state_.plot(at);
    if (rules_->terrains[static_cast<size_t>(p.terrain)].water || p.owner == kNoPlayer || p.owner == player || !atWar(player, p.owner) || state_.cityAt(at))
        return CommandError::BadTarget;
    const bool improvement = p.improvement != kNone && p.pillagedTurns == 0;
    const CityDistrict* d = state_.districtAt(at);
    return improvement || (d && d->complete && d->pillagedTurns == 0) ? CommandError::Ok : CommandError::BadTarget;
}

CommandError Game::repairProblem(PlayerId player, UnitId builder) const {
    const Unit* u = state_.unit(builder);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (u->charges <= 0 || rules_->units[static_cast<size_t>(u->type)].buildCharges <= 0 || u->movesLeft <= Fixed()) return CommandError::CannotImprove;
    const Plot& p = state_.plot(u->pos);
    return p.owner == player && p.improvement != kNone && p.pillagedTurns > 0 ? CommandError::Ok : CommandError::CannotImprove;
}

void Game::pillage(UnitId id, std::optional<Hex> at) {
    Unit& u = *state_.unit(id);
    const Hex where = at ? *at : u.pos;
    Plot& p = state_.plot(where);
    const PlayerId victim = p.owner;
    Plunder loot;
    if (p.improvement != kNone && p.pillagedTurns == 0) {
        loot = rules_->improvements[static_cast<size_t>(p.improvement)].plunder;
        p.pillagedTurns = 255;  // until a Builder repairs it (255 world turns at the most)
    } else if (City* home = state_.city(p.city)) {
        for (CityDistrict& d : home->districts) {
            if (d.pos != where) continue;
            loot = rules_->districts[static_cast<size_t>(d.type)].plunder;
            // Sovereign reading: the city repairs a pillaged district itself in 10 turns.
            d.pillagedTurns = kPillagedDistrictTurns;
        }
    }
    Player& owner = state_.players[static_cast<size_t>(u.owner)];
    const Fixed amount = Fixed::fromInt(loot.amount);
    switch (loot.kind) {
        case PlunderKind::Gold: owner.gold += amount; break;
        case PlunderKind::Faith: owner.faith += amount; break;
        case PlunderKind::Science:
            if (owner.techs.current != kNone) owner.techs.progress[static_cast<size_t>(owner.techs.current)] += amount;
            break;
        case PlunderKind::Culture:
            if (owner.civics.current != kNone) owner.civics.progress[static_cast<size_t>(owner.civics.current)] += amount;
            break;
        case PlunderKind::Heal: u.hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), u.hp + loot.amount); break;
        case PlunderKind::None: break;
    }
    const Fixed cost = Fixed::fromInt(rules_->globalInt("PILLAGE_MOVEMENT_COST"));
    u.movesLeft = u.movesLeft > cost ? u.movesLeft - cost : Fixed();
    if (City* city = state_.city(p.city)) assignCitizens(*city);
    (void)victim;
}

TypeIndex Game::railroad() const {
    for (size_t i = 0; i < rules_->routes.size(); ++i) {
        if (rules_->routes[i].unitOnly) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

CommandError Game::railroadProblem(PlayerId player, UnitId engineer) const {
    const Unit* u = state_.unit(engineer);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    const TypeIndex rr = railroad();
    if (rr == kNone || rules_->units[static_cast<size_t>(u->type)].id != "UNIT_MILITARY_ENGINEER" || u->movesLeft <= Fixed()) return CommandError::CannotImprove;
    const RouteType& route = rules_->routes[static_cast<size_t>(rr)];
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (route.tech != kNone && !p.techs.has(route.tech)) return CommandError::CannotImprove;
    for (const auto& [res, n] : route.resourceCost) {
        if (static_cast<size_t>(res) >= p.stockpile.size() || p.stockpile[static_cast<size_t>(res)] < n) return CommandError::NotEnoughResources;
    }
    const Plot& here = state_.plot(u->pos);
    if (here.route == rr || !isLandPassable(state_, *rules_, u->pos) || (here.owner != kNoPlayer && here.owner != player && !atWar(player, here.owner)))
        return CommandError::CannotImprove;
    return CommandError::Ok;
}

std::vector<Hex> Game::tunnelSites(PlayerId player, UnitId engineer) const {
    std::vector<Hex> out;
    const Unit* u = state_.unit(engineer);
    if (!u || u->owner != player || u->charges <= 0 || u->movesLeft <= Fixed()) return out;
    for (size_t i = 0; i < rules_->improvements.size(); ++i) {
        const ImprovementType& im = rules_->improvements[i];
        if (!im.tunnel || im.builtBy != u->type) continue;
        for (const Hex& h : state_.grid.within(u->pos, 1)) {
            const Plot& p = state_.plot(h);
            if (h == u->pos || p.improvement != kNone || !rules_->terrains[static_cast<size_t>(p.terrain)].impassable) continue;
            if (p.owner != kNoPlayer && p.owner != player) continue;
            if (canImproveAt(player, h, static_cast<TypeIndex>(i)) || (p.owner == kNoPlayer && hasUnlocked(player, im.unlock))) out.push_back(h);
        }
    }
    return out;
}

void Game::applyBuilder(const Command& c) {
    Unit& u = *state_.unit(c.id);
    const bool tunnel = c.type == CommandType::BuildImprovement && static_cast<size_t>(c.arg) < rules_->improvements.size() &&
                        rules_->improvements[static_cast<size_t>(c.arg)].tunnel;
    const Hex at = tunnel ? c.target : u.pos;
    Plot& p = state_.plot(at);
    const CityId cityId = p.city;
    if (c.type == CommandType::BuildImprovement) {
        p.improvement = static_cast<TypeIndex>(c.arg);
    } else {
        // Harvest: the feature (or the bonus resource) goes, its yields go to the owning city.
        Yields gain{};
        if (p.feature != kNone) {
            gain = rules_->features[static_cast<size_t>(p.feature)].harvest;
            p.feature = kNone;
        } else {
            gain = rules_->resources[static_cast<size_t>(p.resource)].harvest;
            p.resource = kNone;
            p.resourceAmount = 0;
        }
        // Base amounts scaled by game speed and by game progress: from the base at the start to 10x with
        // the whole tech or civic tree known, whichever is further (01: Harvest; the engine's curve is
        // unverified, Sovereign reads it as linear).
        const Player& harvester = state_.players[static_cast<size_t>(c.player)];
        const auto share = [](const TreeProgress& t) {
            const int64_t done = std::count(t.done.begin(), t.done.end(), static_cast<uint8_t>(1));
            return t.done.empty() ? 0 : static_cast<int>(done * 100 / static_cast<int64_t>(t.done.size()));
        };
        const int progress = std::max(share(harvester.techs), share(harvester.civics));
        int pct = speedPercent(state_, *rules_) * (100 + 9 * progress) / 100;
        City* city = state_.city(cityId);
        // Groundbreaker: harvests in the city yield more (08: Governors).
        if (city) pct = pct * (100 + static_cast<int>(sumCityModifiers(state_, *rules_, *city, ModEffect::CityHarvestPercent).toInt())) / 100;
        if (city) {
            city->overflow += gain[static_cast<size_t>(YieldType::Production)] * pct / 100;
            city->food += gain[static_cast<size_t>(YieldType::Food)] * pct / 100;
        }
        state_.players[static_cast<size_t>(c.player)].gold += gain[static_cast<size_t>(YieldType::Gold)] * pct / 100;
    }
    u.movesLeft = Fixed();
    u.moveTarget.reset();
    if (--u.charges <= 0) {
        const UnitId gone = u.id;
        state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                          [&](const Unit& x) { return x.id == gone; }),
                           state_.units.end());
    }
    if (City* city = state_.city(cityId)) assignCitizens(*city);
}

// --------------------------------------------------------------- processing

void Game::accumulateStrategics(PlayerId pid) {
    Player& player = state_.players[static_cast<size_t>(pid)];
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        const Plot& p = state_.plots[i];
        if (p.owner != pid || p.resource == kNone) continue;
        const ResourceType& r = rules_->resources[static_cast<size_t>(p.resource)];
        if (r.accumulation <= 0) continue;
        const Hex h = state_.grid.at(static_cast<int>(i));
        if (!resourceVisible(pid, h) || !resourceImproved(h)) continue;
        const City* home = p.city == kNoCity ? nullptr : state_.city(p.city);
        const int extra = home && cityGovernorHas(*home, "GOVERNOR_PROMOTION_DEFENSE_LOGISTICS") ? 1 : 0;  // Victor
        player.stockpile[static_cast<size_t>(p.resource)] += r.accumulation + extra;
    }
    for (size_t r = 0; r < rules_->resources.size(); ++r) {
        const int cap = stockpileCap(pid, static_cast<TypeIndex>(r));
        if (cap > 0) player.stockpile[r] = std::min(player.stockpile[r], cap);
    }
}

// The [GS] stockpile cap (01): 50 per resource, +10 for each Barracks, Stable, Armory and Military Academy
// the player owns (generated stockpileCap on buildings).
int Game::stockpileCap(PlayerId pid, TypeIndex resource) const {
    const int base = rules_->resources[static_cast<size_t>(resource)].stockpileCap;
    if (base <= 0) return 0;
    int raise = 0;
    for (const City& c : state_.cities) {
        if (c.owner != pid) continue;
        for (TypeIndex b : c.buildings) raise += rules_->buildings[static_cast<size_t>(b)].stockpileCap;
    }
    return base + raise;
}

}  // namespace sov
