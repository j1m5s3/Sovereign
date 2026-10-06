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

bool Game::canPlaceWonder(const City& city, TypeIndex building, Hex plot) const {
    if (building < 0 || at(building) >= rules_->buildings.size()) return false;
    const BuildingType& b = rules_->buildings[at(building)];
    if (!b.wonder) return false;
    auto h = state_.grid.normalize(plot);
    if (!h || *h != plot) return false;
    const Plot& p = state_.plot(plot);
    if (p.city != city.id || plot == city.pos || state_.grid.distance(city.pos, plot) > 3) return false;
    if (state_.cityAt(plot) || state_.districtAt(plot) || state_.wonderAt(plot) != kNone || campAt(plot)) return false;
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
        const City* c = state_.cityAt(*n);
        capital |= c && c->owner == city.owner && c->capital;
        const CityDistrict* cd = state_.districtAt(*n);
        district |= cd && cd->type == w.nextToDistrict;
        resource |= np.resource == w.nextToResource && resourceVisible(city.owner, *n);
        improvement |= np.improvement != kNone && np.improvement == w.nextToImprovement;
    }
    if ((w.nextToLand && !land) || (w.coastal && !coast) || (w.nextToMountain && !mountain) || (w.nextToCityCenter && !center) ||
        (w.nextToCapital && !capital))
        return false;
    if (w.nextToDistrict != kNone && !district) return false;
    if (w.nextToResource != kNone && !resource) return false;
    if (w.nextToImprovement != kNone && !improvement) return false;
    return true;
}

std::vector<Hex> Game::wonderPlots(CityId id, TypeIndex building) const {
    std::vector<Hex> out;
    const City* c = state_.city(id);
    if (!c) return out;
    for (const Hex& h : state_.grid.within(c->pos, 3)) {
        if (canPlaceWonder(*c, building, h)) out.push_back(h);
    }
    return out;
}

void Game::wonderCompleted(CityId id, TypeIndex building) {
    if (City* c = state_.city(id); c && building >= 0 && static_cast<size_t>(building) < rules_->buildings.size()) completeWonder(*c, building);
}

void Game::completeWonder(City& city, TypeIndex building) {
    const BuildingType& b = rules_->buildings[at(building)];
    // A wonder of an earlier era than the world's scores a little less (09).
    const int wonderEra = b.unlock.none() ? 0 : (b.unlock.civic ? rules_->civics : rules_->techs)[at(b.unlock.index)].era;
    awardMoment(city.owner, wonderEra < state_.gameEra ? "MOMENT_OLD_WORLD_WONDER_COMPLETED" : "MOMENT_WORLD_WONDER_COMPLETED");
    // One-time effects: free units here, Eurekas.
    for (const GreatPersonEffect& fx : b.wonderEffects) {
        if (fx.kind == GreatPersonEffectKind::Unit) {
            // A free Great Prophet only while religions remain to be founded.
            if (rules_->units[at(fx.ref)].foundReligion && static_cast<int>(state_.religions.size()) >= maxReligions()) continue;
            if (auto spot = unitSpawnPlot(city, fx.ref)) spawnUnit(fx.ref, city.owner, *spot);
        } else {
            applyEffectAt(city.owner, &city, city.pos, fx);
        }
    }
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
