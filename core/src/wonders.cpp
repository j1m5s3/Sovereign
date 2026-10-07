// World wonders (03-districts-buildings-wonders.md, Wonders; data: wonders.md): built once in
// the world, each on a plot of its own chosen when building starts; a rival's completion takes
// it off every other list and refunds half the production put into it.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::wonderBuilt(TypeIndex building) const {
    return std::any_of(state_.cities.begin(), state_.cities.end(), [&](const City& c) { return c.has(building); });
}

bool Game::canPlaceWonder(const City& city, TypeIndex building, Hex plot) const { return canPlaceWonder(city, building, plot, nullptr); }

bool Game::canPlaceWonder(const City& city, TypeIndex building, Hex plot, BuiltNear* built) const {
    if (building < 0 || at(building) >= rules_->buildings.size()) return false;
    const BuildingType& b = rules_->buildings[at(building)];
    if (!b.wonder) return false;
    auto h = state_.grid.normalize(plot);
    if (!h || *h != plot) return false;
    const Plot& p = state_.plot(plot);
    if (p.city != city.id || plot == city.pos || state_.grid.distance(city.pos, plot) > 3) return false;
    if (resourceVisible(city.owner, plot)) return false;  // not on resources
    if (p.feature != kNone && rules_->features[at(p.feature)].naturalWonder) return false;  // nor on natural wonders
    const WonderPlacement& w = b.placement;
    const TerrainType& t = rules_->terrains[at(p.terrain)];
    const bool water = std::any_of(w.terrains.begin(), w.terrains.end(), [&](TypeIndex x) { return rules_->terrains[at(x)].water; });
    if (t.water != water) return false;
    if (!water && !isLandPassable(state_, *rules_, plot) && !(w.mountain && t.relief == Relief::Mountain)) return false;
    if (!w.terrains.empty() || w.mountain || !w.features.empty()) {
        const bool ok = std::find(w.terrains.begin(), w.terrains.end(), p.terrain) != w.terrains.end() ||
                        (w.mountain && t.relief == Relief::Mountain) ||
                        (p.feature != kNone && std::find(w.features.begin(), w.features.end(), p.feature) != w.features.end());
        if (!ok) return false;
    }
    if (!w.needsFeature.empty() && std::find(w.needsFeature.begin(), w.needsFeature.end(), p.feature) == w.needsFeature.end()) return false;
    if (w.river && !isRiverAdjacent(state_, plot)) return false;
    if ((w.lake || w.notLake) && isLake(state_, *rules_, plot, &lakes_) != w.lake) return false;  // Huey Teocalli on a lake; harbour wonders on the sea
    // Nothing built on it yet: looked for once the plot's own land has passed, as these look through every city.
    if ((built ? builtOn(*built, plot) : state_.cityAt(plot) || state_.districtAt(plot) || state_.wonderAt(plot) != kNone) || campAt(plot)) return false;
    bool land = false, coast = false, capital = false, mountain = false, center = false, district = false, resource = false, improvement = false;
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = state_.grid.neighbor(plot, static_cast<Dir>(d));
        if (!n) continue;
        const Plot& np = state_.plot(*n);
        const TerrainType& nt = rules_->terrains[at(np.terrain)];
        land |= !nt.water;
        coast |= nt.water;
        mountain |= nt.relief == Relief::Mountain;
        center |= *n == city.pos;
        // Cities and districts are looked for only by the wonders that need a capital or a district beside them.
        const City* c = w.nextToCapital ? state_.cityAt(*n) : nullptr;
        capital |= c && c->owner == city.owner && c->capital;
        const CityDistrict* cd = w.nextToDistrict != kNone ? state_.districtAt(*n) : nullptr;
        district |= cd && cd->type == w.nextToDistrict;
        resource |= np.resource == w.nextToResource && resourceVisible(city.owner, *n);
        improvement |= np.improvement != kNone && np.improvement == w.nextToImprovement;
    }
    if ((w.nextToLand && !land) || (w.coastal && !coast) || (w.nextToMountain && !mountain) || (w.nextToCityCenter && !center) ||
        (w.nextToCapital && !capital))
        return false;
    if (w.nextToDistrict != kNone && !district) return false;
    // A building the city needs first, any one of them (03: BuildingPrereqs); a civ's unique building counts as
    // the one it replaces (the Aztec Calmecac as the Library the Great Library needs).
    const std::vector<TypeIndex>& any = rules_->buildings[at(building)].prereqsAny;
    if (!any.empty() && std::none_of(any.begin(), any.end(), [&](TypeIndex pre) { return cityHasBuilding(city, *rules_, pre); })) return false;
    if (w.nextToResource != kNone && !resource) return false;
    if (w.nextToImprovement != kNone && !improvement) return false;
    return true;
}

std::vector<Hex> Game::wonderPlots(CityId id, TypeIndex building) const {
    std::vector<Hex> out;
    const City* c = state_.city(id);
    if (!c) return out;
    BuiltNear built(c->pos);
    state_.grid.forEachWithin(c->pos, 3, [&](Hex h) {
        if (canPlaceWonder(*c, building, h, &built)) out.push_back(h);
    });
    return out;
}

bool Game::anyWonderPlot(CityId id, TypeIndex building) const {
    const City* c = state_.city(id);
    if (!c) return false;
    // canPlaceWonder on each plot, with what does not depend on the plot looked at once: the buildings the city needs
    // first, and that the plot is the city's own.
    if (building >= 0 && at(building) < rules_->buildings.size()) {
        const std::vector<TypeIndex>& needs = rules_->buildings[at(building)].prereqsAny;
        if (!needs.empty() && std::none_of(needs.begin(), needs.end(), [&](TypeIndex pre) { return cityHasBuilding(*c, *rules_, pre); })) return false;
    }
    BuiltNear built(c->pos);
    bool any = false;
    state_.grid.forEachWithin(c->pos, 3, [&](Hex h) { any = any || (state_.plot(h).city == c->id && canPlaceWonder(*c, building, h, &built)); });
    return any;
}

bool Game::holdsWonder(PlayerId player, W w) const {
    const TypeIndex b = wonderType(w);
    if (b == kNone) return false;
    return std::any_of(state_.cities.begin(), state_.cities.end(), [&](const City& c) { return c.owner == player && c.has(b); });
}

uint32_t Game::heldWonders(PlayerId player, uint32_t which) const {
    uint32_t held = 0;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (size_t i = 0; i < static_cast<size_t>(W::Count); ++i) {
            if ((which >> i & 1u) != 0 && wonders_[i] != kNone && c.has(wonders_[i])) held |= 1u << i;
        }
    }
    return held;
}

void Game::grantTorreBuildings(PlayerId player) {
    // Each of the player's cities off the capital's continent gets the cheapest building it could build in each of its districts.
    int home = -1;
    for (const City& c : state_.cities) {
        if (c.owner == player && c.capital) home = state_.plot(c.pos).continent;
    }
    for (City& c : state_.cities) {
        if (c.owner != player || state_.plot(c.pos).continent == home) continue;
        for (const CityDistrict& d : c.districts) {
            if (!d.complete) continue;
            const std::string& district = rules_->districts[at(d.type)].id;
            TypeIndex best = kNone;
            for (size_t b = 0; b < rules_->buildings.size(); ++b) {
                const BuildingType& bt = rules_->buildings[b];
                if (bt.wonder || bt.district != district || !canProduce(c, {ProductionKind::Building, static_cast<TypeIndex>(b)})) continue;
                if (best == kNone || bt.cost < rules_->buildings[at(best)].cost) best = static_cast<TypeIndex>(b);
            }
            if (best != kNone) c.buildings.insert(std::lower_bound(c.buildings.begin(), c.buildings.end(), best), best);
        }
    }
}

bool Game::nearOwnWonder(const City& city, TypeIndex b, int range) const {
    if (b == kNone) return false;
    for (const City& o : state_.cities) {
        if (o.owner != city.owner || !o.has(b)) continue;
        Hex site = o.pos;
        for (const CityWonder& cw : o.wonders) {
            if (cw.building == b) site = cw.pos;
        }
        if (state_.grid.distance(site, city.pos) <= range) return true;
    }
    return false;
}

int Game::kilwaPercent(const City& city, CityStateKind kind) const {
    const TypeIndex kilwa = wonderType(W::Kilwa);
    if (!holdsWonder(city.owner, W::Kilwa)) return 0;
    int n = 0;
    for (const Player& p : state_.players) {
        if (p.cityState != kNone && p.alive && rules_->cityStates[at(p.cityState)].kind == kind && isSuzerain(city.owner, p.id)) ++n;
    }
    return (city.has(kilwa) && n >= 1 ? 15 : 0) + (n >= 2 ? 15 : 0);
}

// The Golden Gate Bridge (03; data: roads within 1 tile): its plot becomes a land bridge carrying the owner's
// road, and the land beside it gets that road too.
void Game::bridgeRoads(const City& city, TypeIndex building) {
    const TypeIndex road = roadFor(city.owner);
    if (road == kNone) return;
    for (const CityWonder& w : city.wonders) {
        if (w.building != building) continue;
        for (const Hex& h : state_.grid.within(w.pos, 1)) {
            Plot& p = state_.plot(h);
            if (h != w.pos && !isLandPassable(state_, *rules_, h)) continue;
            if (p.route < road) {
                p.route = static_cast<int8_t>(road);
                p.routePillaged = false;
            }
        }
    }
}

void Game::wonderCompleted(CityId id, TypeIndex building) {
    City* c = state_.city(id);
    if (!c || building < 0 || static_cast<size_t>(building) >= rules_->buildings.size()) return;
    if (!c->has(building)) {
        c->buildings.push_back(building);
        std::sort(c->buildings.begin(), c->buildings.end());
    }
    completeWonder(*c, building);
}

void Game::completeWonder(City& city, TypeIndex building) {
    const BuildingType& b = rules_->buildings[at(building)];
    // A wonder of an earlier era than the world's scores a little less (09).
    const int wonderEra = b.unlock.none() ? 0 : (b.unlock.civic ? rules_->civics : rules_->techs)[at(b.unlock.index)].era;
    awardMoment(city.owner, wonderEra < state_.gameEra ? "MOMENT_OLD_WORLD_WONDER_COMPLETED" : "MOMENT_WORLD_WONDER_COMPLETED");
    // One-time effects: free units here, Eurekas.
    for (const GreatPersonEffect& fx : b.wonderEffects) {
        if (fx.kind == GreatPersonEffectKind::Unit) {
            if (rules_->units[at(fx.ref)].foundReligion) {
                wonderProphet(city, fx.ref);  // Stonehenge: the civ's Prophet, or an Apostle
                continue;
            }
            if (auto spot = unitSpawnPlot(city, fx.ref)) spawnUnit(fx.ref, city.owner, *spot);
        } else {
            applyEffectAt(city.owner, &city, city.pos, fx);
        }
    }
    if (building == wonderType(W::Torre)) grantTorreBuildings(city.owner);
    if (building == wonderType(W::GoldenGate)) bridgeRoads(city, building);
    syncPolicySlots(city.owner);  // Alhambra, Forbidden City, Potala Palace, Big Ben
    // Apadana: +2 envoys for each wonder completed in its city, itself included.
    if (city.has(rules_->building("BUILDING_APADANA")) && !policyIs(city.owner, "POLICY_ROGUE_STATE"))
        state_.players[at(city.owner)].envoyTokens += 2;
    // Everyone else building it loses it, keeping half of what went in (R&F).
    for (City& other : state_.cities) {
        if (other.id == city.id) continue;
        const ProductionItem item{ProductionKind::Building, building};
        other.queue.erase(std::remove(other.queue.begin(), other.queue.end(), item), other.queue.end());
        for (size_t i = 0; i < other.progress.size(); ++i) {
            if (other.progress[i].item == item) {
                other.overflow += other.progress[i].amount / 2;
                other.progress.erase(other.progress.begin() + static_cast<long>(i));
                break;
            }
        }
        other.wonders.erase(std::remove_if(other.wonders.begin(), other.wonders.end(), [&](const CityWonder& w) { return w.building == building; }),
                            other.wonders.end());
    }
}

}  // namespace sov
