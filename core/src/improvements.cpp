// Builders, tile improvements, harvesting, luxury amenities and strategic
// stockpiles (specs/civ6/01-map-and-terrain.md, 02-cities.md; data: improvements.md).
#include <algorithm>

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
    return canImproveAt(c.player, u.pos, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotImprove;
}

void Game::applyBuilder(const Command& c) {
    Unit& u = *state_.unit(c.id);
    const Hex at = u.pos;
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
        // Base amounts scaled by game speed; Civ also scales them with tree
        // progress (unverified, see 01-map-and-terrain.md).
        int pct = speedPercent(state_, *rules_);
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
        if (resourceVisible(pid, h) && resourceImproved(h)) player.stockpile[static_cast<size_t>(p.resource)] += r.accumulation;
    }
    for (size_t r = 0; r < rules_->resources.size(); ++r) {
        const int cap = rules_->resources[r].stockpileCap;
        if (cap > 0) player.stockpile[r] = std::min(player.stockpile[r], cap);
    }
}

}  // namespace sov
