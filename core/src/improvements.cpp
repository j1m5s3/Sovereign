// Builders, tile improvements, harvesting, luxury amenities and strategic
// stockpiles (specs/civ6/01-map-and-terrain.md, 02-cities.md; data: improvements.md).
#include <algorithm>
#include <cstddef>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
template <typename T>
bool contains(const std::vector<T>& v, T x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

int speedPercent(const GameState& s, const Rules& r) {
    return r.speeds[static_cast<size_t>(r.speed(s.setup.speed))].costPercent;
}

constexpr int kBonusAmenityCities = 4;  // Buenos Aires' bonus resources reach as many cities as most luxuries (Sovereign reading)
}  // namespace

// ------------------------------------------------------------------ queries

bool Game::resourceVisible(PlayerId player, Hex at) const {
    const Plot& p = state_.plot(at);
    if (p.resource == kNone) return false;
    if (p.resource == oil_ && usedBy(player, Gp::JamesYoung)) return true;  // James Young (07): Oil seen early
    return hasUnlocked(player, rules_->resources[static_cast<size_t>(p.resource)].reveal);
}

bool Game::resourceImproved(Hex at) const {
    const Plot& p = state_.plot(at);
    if (p.resource == kNone) return false;
    if (p.improvement != kNone && contains(rules_->improvements[static_cast<size_t>(p.improvement)].validResources, p.resource)) return true;
    // A strategic resource still hidden when a district or wonder went over it is granted once revealed (03).
    if (rules_->resources[static_cast<size_t>(p.resource)].cls == ResourceClass::Strategic && (state_.districtAt(at) || state_.wonderAt(at) != kNone))
        return true;
    return state_.cityAt(at) != nullptr;  // a city center counts as improving its resource (looked for last: it looks through every city)
}

bool Game::canImproveAt(PlayerId player, Hex at, TypeIndex improvement, bool ownUnit) const {
    return improvement >= 0 && static_cast<size_t>(improvement) < rules_->improvements.size() && improvablePlot(player, at) &&
           improvementFits(player, at, improvement, ownUnit, resourceVisible(player, at));
}

// The player's own worked land, with no city, district or wonder on it.
bool Game::improvablePlot(PlayerId player, Hex at) const {
    const Plot& p = state_.plot(at);
    return p.owner == player && p.city != kNoCity && !state_.cityAt(at) && !state_.districtAt(at) && state_.wonderAt(at) == kNone;
}

bool Game::improvementFits(PlayerId player, Hex at, TypeIndex improvement, bool ownUnit, bool resourceSeen) const {
    const Plot& p = state_.plot(at);
    if (p.improvement == improvement || p.park) return false;  // a National Park keeps its land as it is (07)
    const ImprovementType& im = rules_->improvements[static_cast<size_t>(improvement)];
    // The plot's land, looked at first as most improvements are for other land: a visible resource only takes the
    // improvements that work it.
    if (!(resourceSeen ? contains(im.validResources, p.resource) : p.feature != kNone ? contains(im.validFeatures, p.feature) : contains(im.validTerrains, p.terrain)))
        return false;
    // Sea improvements on water, the others on land: Amber and Oil are found on both, each with an improvement of its own.
    if (im.water != rules_->terrains[static_cast<size_t>(p.terrain)].water) return false;
    if (!ownUnit && !hasUnlocked(player, im.unlock)) return false;
    // Civ unique improvements: their civ only, some on a river or at the edge of its land.
    if (im.uniqueTo != kNone && im.uniqueTo != state_.players[static_cast<size_t>(player)].civ) return false;
    if (im.needsRiver && !isRiverAdjacent(state_, at)) return false;
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
    if (im.governorPromotion != kNone) {
        const City* home = state_.plot(at).city == kNoCity ? nullptr : state_.city(state_.plot(at).city);
        if (!home || !cityGovernorHas(*home, rules_->governorPromotions[static_cast<size_t>(im.governorPromotion)].id.c_str())) return false;
    }
    // The dearer checks last. City-states' unique improvements (08): for whoever enjoys that city-state's suzerain bonus.
    if (im.cityState != kNone && !enjoysSuzerainBonus(state_, *rules_, player, im.cityState, cityStateOfType(im.cityState))) return false;
    return im.minAppeal <= -100 || plotAppeal(at) >= im.minAppeal;  // Seaside Resort: Breathtaking (07)
}

// Only the improvements that take the plot's land can fit it (improvementFits looks at the land first).
const std::vector<TypeIndex>& Game::improvementsForLand(const Plot& p, bool resourceSeen) const {
    return resourceSeen ? resourceImprovements_[static_cast<size_t>(p.resource)]
           : p.feature != kNone ? featureImprovements_[static_cast<size_t>(p.feature)]
                                : terrainImprovements_[static_cast<size_t>(p.terrain)];
}

std::vector<TypeIndex> Game::improvementsAt(PlayerId player, Hex at) const {
    std::vector<TypeIndex> out;
    if (!improvablePlot(player, at)) return out;  // once for the plot, not once per improvement
    const bool resourceSeen = resourceVisible(player, at);
    for (const TypeIndex i : improvementsForLand(state_.plot(at), resourceSeen)) {
        if (improvementFits(player, at, i, false, resourceSeen)) out.push_back(i);
    }
    return out;
}

bool Game::builderCanImprove(PlayerId player, Hex at) const {
    if (!improvablePlot(player, at)) return false;
    const bool resourceSeen = resourceVisible(player, at);
    const std::vector<TypeIndex>& candidates = improvementsForLand(state_.plot(at), resourceSeen);
    // Any one will do, so the city-states' improvements, whose suzerain check costs most, are tried last.
    for (const bool cityStates : {false, true}) {
        for (const TypeIndex i : candidates) {
            const ImprovementType& im = rules_->improvements[static_cast<size_t>(i)];
            if (im.builtBy != kNone || (im.cityState != kNone) != cityStates) continue;
            if (improvementFits(player, at, i, false, resourceSeen)) return true;
        }
    }
    return false;
}

// A harvest takes the plot's feature when it has one that can be removed, else its resource (the Bananas in a
// Rainforest, the Wheat on Floodplains, the Fish on a Reef): with `resource`, always the resource.
static bool harvestsFeature(const Rules& rules, const Plot& p, bool resource) {
    return !resource && p.feature != kNone && rules.features[static_cast<size_t>(p.feature)].removable;
}

bool Game::canHarvestAt(PlayerId player, Hex at, bool resource) const {
    const Plot& p = state_.plot(at);
    if (p.owner != player || p.city == kNoCity || state_.cityAt(at) || p.improvement != kNone) return false;
    if (harvestsFeature(*rules_, p, resource)) {
        const FeatureType& f = rules_->features[static_cast<size_t>(p.feature)];
        if (resolutionHits(ResolutionKind::DeforestationTreaty, 0, p.feature)) return false;  // World Congress: no chopping it
        return !f.removeTech.none() && hasUnlocked(player, f.removeTech);
    }
    if (p.resource == kNone || !resourceVisible(player, at)) return false;
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
    if (im.governorPromotion != kNone && p.city != kNoCity) {
        const City* home = state_.city(p.city);
        if (home && cityGovernorHas(*home, rules_->governorPromotions[static_cast<size_t>(im.governorPromotion)].id.c_str()))
            for (size_t k = 0; k < kNumYields; ++k) y[k] += im.governorYields[k];
    }
    // Its plot qualifies (08): the Moai beside the coast, or on or beside Volcanic Soil.
    for (const ImprovementTileYield& t : im.tileYields) {
        bool fits = false;
        state_.grid.forEachWithin(at, 1, [&](Hex h) {
            const Plot& q = state_.plot(h);
            fits = fits || (t.nextToCoast && rules_->terrains[static_cast<size_t>(q.terrain)].shallowWater) || (t.nearFeature != kNone && q.feature == t.nearFeature);
        });
        if (fits) y[static_cast<size_t>(t.yield)] += t.amount;
    }
    for (const ImprovementAdjacency& a : im.adjacency) {
        if (!a.needs.none() && !hasUnlocked(owner, a.needs)) continue;
        if (!a.obsoleteWith.none() && hasUnlocked(owner, a.obsoleteWith)) continue;
        int n = 0;
        state_.grid.forEachWithin(at, 1, [&](Hex h) {
            if (h == at) return;
            const Plot& q = state_.plot(h);
            if (a.improvement != kNone) n += q.improvement == a.improvement ? 1 : 0;
            else if (!a.district.empty()) {
                const CityDistrict* d = state_.districtAt(h);
                const bool center = state_.cityAt(h) != nullptr;
                n += (d && d->complete && (a.district == "ANY" || rules_->districts[static_cast<size_t>(d->type)].id == a.district)) ||
                             (center && (a.district == "ANY" || a.district == "DISTRICT_CITY_CENTER"))
                         ? 1
                         : 0;
            } else if (a.feature != kNone) n += q.feature == a.feature ? 1 : 0;
            else if (a.resourceClass >= 0) n += q.resource != kNone && static_cast<int>(rules_->resources[static_cast<size_t>(q.resource)].cls) == a.resourceClass ? 1 : 0;
            else if (a.seaResource) n += rules_->terrains[static_cast<size_t>(q.terrain)].water && resourceVisible(owner, h) ? 1 : 0;
        });
        y[static_cast<size_t>(a.yield)] += a.amount * (n / a.per);
    }
    // A neighbouring unique improvement that feeds this one (Nilometer: adjacent Farms +1 Food).
    state_.grid.forEachWithin(at, 1, [&](Hex h) {
        const TypeIndex ni = state_.plot(h).improvement;
        if (h == at || ni == kNone) return;
        const ImprovementType& n = rules_->improvements[static_cast<size_t>(ni)];
        if (n.adjacentImprovement == p.improvement) y[static_cast<size_t>(n.adjacentYield)] += Fixed::fromInt(n.adjacentAmount);
    });
    return y;
}

Fixed Game::improvementHousing(const City& city) const {
    Fixed total;
    state_.grid.forEachWithin(city.pos, 3, [&](Hex h) {
        const Plot& p = state_.plot(h);
        if (p.city == city.id && p.improvement != kNone) {
            total += rules_->improvements[static_cast<size_t>(p.improvement)].housing;
            // Civ ability: farms next to a river or lake add housing (Aztec Chinampas).
            if (rules_->improvements[static_cast<size_t>(p.improvement)].id == "IMPROVEMENT_FARM" &&
                (isRiverAdjacent(state_, h) || isLakeAdjacent(state_, *rules_, h, &lakes_)))
                total += civAbility(city.owner).freshWaterFarmHousing;
        }
    });
    return total;
}

int Game::luxuryAmenities(const City& city) const {
    ReportShare shared;
    return luxuryAmenities(city, shared);
}

int Game::luxuryAmenities(const City& city, ReportShare& shared) const {
    if (!shared.luxuryShares) shared.luxuryShares = luxuryShares(city.owner, shared);
    for (const auto& [id, amenities] : *shared.luxuryShares) {
        if (id == city.id) return amenities;
    }
    return 0;
}

std::vector<std::pair<CityId, int>> Game::luxuryShares(PlayerId player, ReportShare& shared) const {
    // Each luxury type the player has improved gives +1 amenity to up to `amenityCities` of its cities, those that need
    // them most (02: Amenities). Sovereign reading: those whose population asks the most Amenities that luxuries have
    // not given yet; of those, the larger, then the older city.
    std::vector<const City*> cities;
    for (const City& c : state_.cities) {
        if (c.owner == player) cities.push_back(&c);
    }
    const size_t n = cities.size();
    const int perAmenity = std::max(1, rules_->globalInt("CITY_POP_PER_AMENITY"));
    std::vector<int> unmet(n);
    for (size_t i = 0; i < n; ++i) unmet[i] = std::max(0, (cities[i]->population + perAmenity - 1) / perAmenity - 1);
    std::vector<std::pair<CityId, int>> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = {cities[i]->id, 0};
    std::vector<size_t> order(n);
    const auto give = [&](int reach) {
        const size_t k = std::min(n, static_cast<size_t>(std::max(0, reach)));
        for (size_t i = 0; i < n; ++i) order[i] = i;
        std::partial_sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(k), order.end(), [&](size_t a, size_t b) {
            if (unmet[a] != unmet[b]) return unmet[a] > unmet[b];
            if (cities[a]->population != cities[b]->population) return cities[a]->population > cities[b]->population;
            return a < b;
        });
        for (size_t i = 0; i < k; ++i) {
            --unmet[order[i]];
            ++out[order[i]].second;
        }
    };
    // Access after deals: copies traded away are lost, copies traded in count (08: Trade Deal).
    if (!shared.luxuries) shared.luxuries = luxuriesHeld(player);
    const std::vector<uint8_t>& have = *shared.luxuries;
    for (size_t r = 0; r < have.size(); ++r) {
        if (!have[r] || rules_->resources[r].cls != ResourceClass::Luxury) continue;
        // Luxury Policy (World Congress): option A lifts the luxury's cap (twice the cities), B bans it.
        const int32_t res = static_cast<int32_t>(r);
        if (resolutionHits(ResolutionKind::LuxuryPolicy, 1, res)) continue;
        give(rules_->resources[r].amenityCities * (resolutionHits(ResolutionKind::LuxuryPolicy, 0, res) ? 2 : 1));
    }
    // Buenos Aires (08: suzerain): each kind of improved bonus resource is an Amenity too, for as many cities as a luxury's.
    if (suzerainBonus(player, Cs::BuenosAires, shared)) {
        const std::vector<int> copies = resourceCopies(player);
        for (size_t r = 0; r < rules_->resources.size(); ++r) {
            if (rules_->resources[r].cls == ResourceClass::Bonus && copies[r] > 0) give(kBonusAmenityCities);
        }
    }
    return out;
}

int Game::strategicCostIn(const City* city, TypeIndex unitType) const {
    const UnitType& u = rules_->units[static_cast<size_t>(unitType)];
    if (u.strategicResource == kNone || u.strategicCost <= 0) return 0;
    if (city && cityGovernorHas(*city, "GOVERNOR_PROMOTION_BLACK_MARKETEER")) return (u.strategicCost * 20 + 99) / 100;
    return u.strategicCost;
}

bool Game::hasStrategicFor(PlayerId player, TypeIndex unitType, const City* city) const {
    const UnitType& u = rules_->units[static_cast<size_t>(unitType)];
    if (u.strategicResource == kNone || u.strategicCost <= 0) return true;
    const Player& p = state_.players[static_cast<size_t>(player)];
    return p.stockpile[static_cast<size_t>(u.strategicResource)] >= strategicCostIn(city, unitType);
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

Game::ImprovedPlots Game::improvedPlots(PlayerId player) const {
    ImprovedPlots n;
    n.byImprovement.assign(rules_->improvements.size(), 0);
    n.onResource.assign(rules_->improvements.size(), 0);
    n.byResource.assign(rules_->resources.size(), 0);
    // Only plots that have had an improvement can have one now; the counts do not depend on the order they are made in.
    for (const int32_t i : improvedOnce_) {
        const Plot& p = state_.plots[static_cast<size_t>(i)];
        if (p.owner != player || p.improvement == kNone) continue;
        ++n.total;
        ++n.byImprovement[static_cast<size_t>(p.improvement)];
        if (!resourceImproved(state_.grid.at(i))) continue;
        ++n.onResource[static_cast<size_t>(p.improvement)];
        ++n.byResource[static_cast<size_t>(p.resource)];
    }
    return n;
}

// ---------------------------------------------------------------- commands

CommandError Game::validateBuilder(const Command& c) const {
    const Unit& u = *state_.unit(c.id);
    const UnitType& ut = rules_->units[static_cast<size_t>(u.type)];
    if (u.charges <= 0 || u.movesLeft <= Fixed()) {
        return c.type == CommandType::Harvest ? CommandError::CannotHarvest : CommandError::CannotImprove;
    }
    if (c.type == CommandType::Harvest) {
        if (c.arg != 0 && c.arg != 1) return CommandError::CannotHarvest;
        return isBuilder(ut) && canHarvestAt(c.player, u.pos, c.arg == 1) ? CommandError::Ok : CommandError::CannotHarvest;
    }
    if (c.arg < 0 || c.arg > INT16_MAX) return CommandError::CannotImprove;
    // Military Engineers build their own improvements (Fort, Airstrip, Missile Silo), a Legionary its Fort; Builders the rest.
    bool own = false;
    if (static_cast<size_t>(c.arg) < rules_->improvements.size()) {
        const TypeIndex by = rules_->improvements[static_cast<size_t>(c.arg)].builtBy;
        own = contains(ut.builds, static_cast<TypeIndex>(c.arg));
        if (!ut.builds.empty() ? !own : by != kNone ? u.type != by : !isBuilder(ut)) return CommandError::CannotImprove;
    }
    // A Mountain Tunnel goes on a neighbouring mountain, named as the target.
    if (static_cast<size_t>(c.arg) < rules_->improvements.size() && rules_->improvements[static_cast<size_t>(c.arg)].tunnel) {
        const std::vector<Hex> sites = tunnelSites(c.player, c.id);
        return std::find(sites.begin(), sites.end(), c.target) != sites.end() ? CommandError::Ok : CommandError::CannotImprove;
    }
    return canImproveAt(c.player, u.pos, static_cast<TypeIndex>(c.arg), own) ? CommandError::Ok : CommandError::CannotImprove;
}

// ---------------------------------------------------------------- formations (05: Corps and Armies)

CommandError Game::formationProblem(PlayerId player, UnitId unit, UnitId with) const {
    // The partner looked up first: one tried with every unit in turn (as the AI does) mostly finds it is someone else's.
    const Unit* w = state_.unit(with);
    if (!w || w->owner != player || unit == with) return CommandError::NotYourUnit;
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
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
    const bool route = p.route >= 0 && !p.routePillaged;
    return improvement || district || route ? CommandError::Ok : CommandError::BadTarget;
}

CommandError Game::coastalRaidProblem(PlayerId player, UnitId unit, Hex at) const {
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    const UnitType& ut = rules_->units[static_cast<size_t>(u->type)];
    const bool raider = ut.unitClass == "NAVAL_MELEE" || ut.unitClass == "NAVAL_RAIDER" || unitHas(*u, UnitEffectKind::CoastalRaid);
    if (ut.domain != Domain::Sea || !raider || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    const auto t = state_.grid.normalize(at);
    if (!t || *t != at || state_.grid.distance(u->pos, at) != 1) return CommandError::BadTarget;
    const Plot& p = state_.plot(at);
    if (rules_->terrains[static_cast<size_t>(p.terrain)].water || p.owner == kNoPlayer || p.owner == player || !atWar(player, p.owner) || state_.cityAt(at))
        return CommandError::BadTarget;
    const bool improvement = p.improvement != kNone && p.pillagedTurns == 0;
    const CityDistrict* d = state_.districtAt(at);
    return improvement || (d && d->complete && d->pillagedTurns == 0) ? CommandError::Ok : CommandError::BadTarget;
}

CommandError Game::chargeProblem(PlayerId player, UnitId unit) const {
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (u->charges <= 0 || u->movesLeft <= Fixed()) return CommandError::CannotImprove;
    if (chargedProject(*u)) return CommandError::Ok;  // a Builder with the Royal Society
    const CityDistrict* d = state_.districtAt(u->pos);
    const City* c = state_.city(state_.plot(u->pos).city);
    if (!d || d->complete || !c || c->owner != player) return CommandError::CannotImprove;
    const DistrictType& dt = rules_->districts[static_cast<size_t>(d->type)];
    return dt.chargePercent > 0 && dt.chargeUnit == rules_->units[static_cast<size_t>(u->type)].id ? CommandError::Ok : CommandError::CannotImprove;
}

// The Royal Society (03; data: each charge completes 2% of a project): a Builder's charge speeds the project its city is
// building, from where the project runs: its district, or the City Center for a project that needs none.
std::optional<ProductionItem> Game::chargedProject(const Unit& builder) const {
    if (!isBuilder(rules_->units[static_cast<size_t>(builder.type)]) || projectChargePercent(builder.owner) <= 0) return std::nullopt;
    const City* c = state_.city(state_.plot(builder.pos).city);
    if (!c || c->owner != builder.owner || c->queue.empty() || c->queue.front().kind != ProductionKind::Project) return std::nullopt;
    const ProjectType& pj = rules_->projects[static_cast<size_t>(c->queue.front().type)];
    if (pj.district == kNone) return builder.pos == c->pos ? std::optional<ProductionItem>(c->queue.front()) : std::nullopt;
    const CityDistrict* d = state_.districtAt(builder.pos);
    return d && d->type == pj.district && d->complete ? std::optional<ProductionItem>(c->queue.front()) : std::nullopt;
}

int Game::projectChargePercent(PlayerId player) const {
    int pct = 0;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (TypeIndex b : c.buildings) pct += rules_->buildings[static_cast<size_t>(b)].projectChargePercent;
    }
    return pct;
}

CommandError Game::repairProblem(PlayerId player, UnitId builder) const {
    const Unit* u = state_.unit(builder);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (u->charges <= 0 || rules_->units[static_cast<size_t>(u->type)].buildCharges <= 0 || u->movesLeft <= Fixed()) return CommandError::CannotImprove;
    const Plot& p = state_.plot(u->pos);
    const bool improvement = p.owner == player && p.improvement != kNone && p.pillagedTurns > 0;
    // A pillaged road on the player's land or on no one's.
    const bool route = p.routePillaged && (p.owner == player || p.owner == kNoPlayer);
    return improvement || route ? CommandError::Ok : CommandError::CannotImprove;
}

void Game::pillage(UnitId id, std::optional<Hex> at) {
    Unit& u = *state_.unit(id);
    const Hex where = at ? *at : u.pos;
    Plot& p = state_.plot(where);
    const PlayerId victim = p.owner;
    Plunder loot;
    if (p.improvement != kNone && p.pillagedTurns == 0) {
        loot = rules_->improvements[static_cast<size_t>(p.improvement)].plunder;
        p.pillagedTurns = kPillagedUntilRepaired;
    } else {
        bool district = false;
        if (City* home = state_.city(p.city)) {
            for (CityDistrict& d : home->districts) {
                if (d.pos != where || !d.complete || d.pillagedTurns > 0) continue;
                loot = rules_->districts[static_cast<size_t>(d.type)].plunder;
                // Sovereign reading: the city repairs a pillaged district itself in 10 turns.
                d.pillagedTurns = kPillagedDistrictTurns;
                district = true;
            }
        }
        // With nothing else left to take, the road (no plunder).
        if (!district && p.route >= 0) p.routePillaged = true;
    }
    Player& owner = state_.players[static_cast<size_t>(u.owner)];
    // Plunder bonuses (Francis Drake, Ching Shih...) raise the yields, not the healing.
    const int bonus = loot.kind == PlunderKind::Heal ? 0 : plunderPercent(u);
    const Fixed amount = Fixed::fromInt(loot.amount * (100 + bonus) / 100);
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
    const Fixed cost = Fixed::fromInt(rules_->globalInt(unitEffectTotal(u, UnitEffectKind::CheapPillage) > 0 ? "PILLAGE_ADVANCED_MOVEMENT_COST" : "PILLAGE_MOVEMENT_COST"));
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

// A road by hand (01: Routes): a Military Engineer, until railroads replace its roads [GS], or a Legionary
// (leaders-and-art-style) spends a charge on a road of the owner's era on its plot; Qin's Builders lay one for none.
CommandError Game::roadProblem(PlayerId player, UnitId unit) const {
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    const UnitType& ut = rules_->units[static_cast<size_t>(u->type)];
    const TypeIndex road = roadFor(player);
    if ((!ut.buildsRoads && !freeRoad(player, unit)) || u->charges <= 0 || u->movesLeft <= Fixed() || road == kNone)
        return CommandError::CannotImprove;
    if (const TypeIndex rr = railroad(); rr != kNone && ut.id == "UNIT_MILITARY_ENGINEER") {
        const TypeIndex tech = rules_->routes[static_cast<size_t>(rr)].tech;
        if (tech != kNone && state_.players[static_cast<size_t>(player)].techs.has(tech)) return CommandError::CannotImprove;
    }
    const Plot& here = state_.plot(u->pos);
    if (!isLandPassable(state_, *rules_, u->pos) || (here.route >= road && !here.routePillaged) ||
        (here.owner != kNoPlayer && here.owner != player && !atWar(player, here.owner)))
        return CommandError::CannotImprove;
    return CommandError::Ok;
}

// Qin Shi Huang's Standardization (leaders-and-art-style): roads cost his Builders no charge. Builders lay no roads
// in Civ VI, so his are the only ones that do.
bool Game::freeRoad(PlayerId player, UnitId unit) const {
    const Unit* u = state_.unit(unit);
    return u && u->owner == player && isBuilder(rules_->units[static_cast<size_t>(u->type)]) && civAbility(player).builderRoads;
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
        if (const size_t i = static_cast<size_t>(state_.grid.index(at)); !improvedOnceAt_[i]) {
            improvedOnceAt_[i] = 1;
            improvedOnce_.push_back(static_cast<int32_t>(i));
        }
        // Historic moments (09): a unique improvement, a tunnel, a resort, a green improvement.
        const ImprovementType& built = rules_->improvements[static_cast<size_t>(c.arg)];
        if (built.uniqueTo != kNone) awardOnce(c.player, "MOMENT_UNIQUE_TILE_IMPROVEMENT_BUILT");
        if (built.tunnel) awardFirst(c.player, "MOMENT_FIRST_MOUNTAIN_TUNNEL_IN_WORLD", "MOMENT_FIRST_MOUNTAIN_TUNNEL", 0);
        if (built.id == "IMPROVEMENT_SEASIDE_RESORT") awardFirst(c.player, "MOMENT_WORLD_S_FIRST_SEASIDE_RESORT", "MOMENT_FIRST_SEASIDE_RESORT", 0);
        if (built.id == "IMPROVEMENT_SOLAR_FARM" || built.id == "IMPROVEMENT_WIND_FARM" || built.id == "IMPROVEMENT_OFFSHORE_WIND_FARM")
            awardFirst(c.player, "MOMENT_FIRST_GREEN_IMPROVEMENT_IN_WORLD", "MOMENT_FIRST_GREEN_IMPROVEMENT", 0);
        // Nalanda (08): a player's first Mahavihara grants a random technology.
        Player& builder = state_.players[static_cast<size_t>(c.player)];
        if (built.id == "IMPROVEMENT_MAHAVIHARA" && suzerainBonus(c.player, Cs::Nalanda) && !contains(builder.improvementGrants, p.improvement)) {
            builder.improvementGrants.push_back(p.improvement);
            GreatPersonEffect gift;
            gift.kind = GreatPersonEffectKind::RandomTechs;
            gift.count = 1;
            applyEffectAt(c.player, nullptr, at, gift);
        }
    } else {
        // Harvest: the feature (or the bonus resource) goes, its yields go to the owning city.
        Yields gain{};
        bool treaty = false;
        if (harvestsFeature(*rules_, p, c.arg == 1)) {
            gain = rules_->features[static_cast<size_t>(p.feature)].harvest;
            treaty = resolutionHits(ResolutionKind::DeforestationTreaty, 1, p.feature);  // World Congress: +100% from it
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
        if (treaty) pct *= 2;
        if (city) {
            city->overflow += gain[static_cast<size_t>(YieldType::Production)] * pct / 100;
            city->food += gain[static_cast<size_t>(YieldType::Food)] * pct / 100;
        }
        state_.players[static_cast<size_t>(c.player)].gold += gain[static_cast<size_t>(YieldType::Gold)] * pct / 100;
    }
    u.movesLeft = Fixed();
    u.moveTarget.reset();
    if (--u.charges <= 0 && rules_->units[static_cast<size_t>(u.type)].layer != UnitLayer::Military) {  // a Legionary stays, its charge spent
        const UnitId gone = u.id;
        state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                          [&](const Unit& x) { return x.id == gone; }),
                           state_.units.end());
    }
    if (City* city = state_.city(cityId)) assignCitizens(*city);
}

// Monopolies and Corporations mode (07; Sovereign values, the spec gives only the outline). After Economics a Builder
// makes an Industry on an improved luxury the player owns, one per luxury type; after Electricity it turns the
// Industry into a Corporation. An Industry gives +2 Gold on its plot and +10% Gold in its city, a Corporation +4 Gold
// and +2 Production and +20%. Owning 60% or more of a luxury's improved sources in the world (at least 2) is a
// Monopoly: +3 Gold and +2 Tourism a turn for each of those sources.
CommandError Game::industryProblem(PlayerId player, Hex at) const {
    const Plot& p = state_.plot(at);
    if (!state_.setup.monopolies || p.owner != player || p.city == kNoCity || p.resource == kNone || p.pillagedTurns > 0)
        return CommandError::CannotImprove;
    if (rules_->resources[static_cast<size_t>(p.resource)].cls != ResourceClass::Luxury || !resourceVisible(player, at) || !resourceImproved(at))
        return CommandError::CannotImprove;
    const Player& pl = state_.players[static_cast<size_t>(player)];
    if (p.industry == 0) {
        const TypeIndex economics = rules_->tech("TECH_ECONOMICS");
        if (economics == kNone || !pl.techs.has(economics)) return CommandError::CannotImprove;
        for (const Plot& o : state_.plots) {
            if (o.owner == player && o.resource == p.resource && o.industry > 0) return CommandError::CannotImprove;  // one per luxury type
        }
        return CommandError::Ok;
    }
    const TypeIndex electricity = rules_->tech("TECH_ELECTRICITY");
    return p.industry == 1 && electricity != kNone && pl.techs.has(electricity) ? CommandError::Ok : CommandError::CannotImprove;
}

void Game::applyIndustry(const Command& c) {
    Unit& u = *state_.unit(c.id);
    Plot& p = state_.plot(u.pos);
    p.industry = static_cast<uint8_t>(p.industry + 1);
    const CityId cityId = p.city;
    u.movesLeft = Fixed();
    u.moveTarget.reset();
    if (--u.charges <= 0) {
        const UnitId gone = u.id;
        state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(), [&](const Unit& x) { return x.id == gone; }), state_.units.end());
    }
    if (City* city = state_.city(cityId)) assignCitizens(*city);
}

bool Game::hasMonopoly(PlayerId player, TypeIndex luxury) const {
    if (!state_.setup.monopolies || luxury == kNone) return false;
    int mine = 0, all = 0;
    for (const int32_t i : resourcePlots_) {
        const Plot& p = state_.plots[static_cast<size_t>(i)];
        if (p.resource != luxury || p.owner == kNoPlayer || !resourceImproved(state_.grid.at(i))) continue;
        ++all;
        mine += p.owner == player ? 1 : 0;
    }
    return mine >= 2 && mine * 100 >= all * 60;
}

int Game::monopolySources(PlayerId player) const {
    if (!state_.setup.monopolies) return 0;
    std::vector<int> mine(rules_->resources.size(), 0), all(rules_->resources.size(), 0);
    for (const int32_t i : resourcePlots_) {
        const Plot& p = state_.plots[static_cast<size_t>(i)];
        if (p.resource == kNone || p.owner == kNoPlayer || rules_->resources[static_cast<size_t>(p.resource)].cls != ResourceClass::Luxury) continue;
        if (!resourceImproved(state_.grid.at(i))) continue;
        ++all[static_cast<size_t>(p.resource)];
        mine[static_cast<size_t>(p.resource)] += p.owner == player ? 1 : 0;
    }
    int n = 0;
    for (size_t r = 0; r < mine.size(); ++r) n += mine[r] >= 2 && mine[r] * 100 >= all[r] * 60 ? mine[r] : 0;
    return n;
}

// --------------------------------------------------------------- processing

void Game::accumulateStrategics(PlayerId pid) {
    Player& player = state_.players[static_cast<size_t>(pid)];
    // The suzerain also gathers its city-states' strategic resources (08: Suzerain).
    std::vector<PlayerId> holders{pid};
    for (const Player& cs : state_.players) {
        if (cs.cityState != kNone && cs.alive && isSuzerain(pid, cs.id)) holders.push_back(cs.id);
    }
    const bool corporate = governmentIs(pid, "GOVERNMENT_CORPORATE_LIBERTARIANISM");  // 04: +1 a source in its own cities
    for (const int32_t i : resourcePlots_) {
        const Plot& p = state_.plots[static_cast<size_t>(i)];
        if (p.resource == kNone || std::find(holders.begin(), holders.end(), p.owner) == holders.end()) continue;
        const ResourceType& r = rules_->resources[static_cast<size_t>(p.resource)];
        if (r.accumulation <= 0) continue;
        const Hex h = state_.grid.at(i);
        if (!resourceVisible(pid, h) || !resourceImproved(h)) continue;
        const City* home = p.city == kNoCity ? nullptr : state_.city(p.city);
        int extra = home && cityGovernorHas(*home, "GOVERNOR_PROMOTION_DEFENSE_LOGISTICS") ? 1 : 0;  // Victor
        // Drill Manuals, Equestrian Orders, Resource Management (04): +1 a source of their resources.
        static const std::pair<const char*, const char*> kCards[] = {
            {"POLICY_DRILL_MANUALS", "RESOURCE_NITER"}, {"POLICY_DRILL_MANUALS", "RESOURCE_COAL"}, {"POLICY_EQUESTRIAN_ORDERS", "RESOURCE_HORSES"},
            {"POLICY_EQUESTRIAN_ORDERS", "RESOURCE_IRON"}, {"POLICY_RESOURCE_MANAGEMENT", "RESOURCE_ALUMINUM"}, {"POLICY_RESOURCE_MANAGEMENT", "RESOURCE_OIL"}};
        for (const auto& [card, res] : kCards) extra += r.id == res && policyIs(pid, card) ? 1 : 0;
        extra += corporate && p.owner == pid ? 1 : 0;
        // Foreign Investor (08: Amani): her city-state's strategics come in twice over.
        int copies = 1;
        if (home && p.owner != pid) {
            PlayerId holder = kNoPlayer;
            const Governor* amani = establishedGovernor(*home, &holder);
            if (amani && holder == pid && governorHasPromotion(*amani, "GOVERNOR_PROMOTION_FOREIGN_INVESTOR")) copies = 2;
        }
        player.stockpile[static_cast<size_t>(p.resource)] += (r.accumulation + extra) * copies;
    }
    // Hattusa (08: suzerain): +2 a turn of each strategic resource revealed but not yet improved.
    if (suzerainBonus(pid, Cs::Hattusa)) {
        for (size_t r = 0; r < rules_->resources.size() && r < player.stockpile.size(); ++r) {
            const ResourceType& rt = rules_->resources[r];
            if (rt.cls != ResourceClass::Strategic || !hasUnlocked(pid, rt.reveal)) continue;
            bool improved = false;
            for (size_t j = 0; j < resourcePlots_.size() && !improved; ++j) {
                const int32_t i = resourcePlots_[j];
                const Plot& p = state_.plots[static_cast<size_t>(i)];
                improved = p.owner == pid && p.resource == static_cast<TypeIndex>(r) && resourceImproved(state_.grid.at(i));
            }
            if (!improved) player.stockpile[r] += 2;
        }
    }
    // Great people (07): resources a turn (Douglas MacArthur, Yi Sun-sin, John Rockefeller...).
    for (size_t r = 0; r < rules_->resources.size() && r < player.stockpile.size(); ++r)
        player.stockpile[r] += greatPersonEffectTotal(pid, GreatPersonEffectKind::ResourcePerTurn, static_cast<TypeIndex>(r));
    // Aerospace Contractors (04): +3 Aluminum a turn in each city with a Spaceport.
    if (policyIs(pid, "POLICY_AEROSPACE_CONTRACTORS")) {
        const TypeIndex aluminum = rules_->resource("RESOURCE_ALUMINUM"), spaceport = rules_->district("DISTRICT_SPACEPORT");
        for (const City& c : state_.cities) {
            if (aluminum != kNone && c.owner == pid && c.district(spaceport, true)) player.stockpile[static_cast<size_t>(aluminum)] += 3;
        }
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
