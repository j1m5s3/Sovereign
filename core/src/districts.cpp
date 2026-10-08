// Specialty districts: placement, population limit, cost, adjacency
// (03-districts-buildings-wonders.md, Districts: general rules; data: districts.md).
// Encampment combat, citizen slots, great person points and pillaging arrive
// with their systems.
#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
int speedPercent(const GameState& s, const Rules& r) {
    return r.speeds[static_cast<size_t>(r.speed(s.setup.speed))].costPercent;
}

// A finished world wonder stands on the plot (one still being built does not count yet).
bool finishedWonderAt(const GameState& s, Hex h) {
    if (const City* c = s.landCity(h)) {
        for (const CityWonder& w : c->wonders) {
            if (w.pos == h && c->has(w.building)) return true;
        }
    }
    return false;
}

bool onRiver(const GameState& s, Hex h) {
    for (int d = 0; d < kNumDirs; ++d) {
        if (hasRiver(s, h, static_cast<Dir>(d))) return true;
    }
    return false;
}
}  // namespace

int Game::districtLimit(const City& city) const {
    const int per = std::max(1, rules_->globalInt(HotGlobal::DistrictPopulationRequiredPer));
    // Bi Sheng, Ada Lovelace (07): more districts in the city where they were used.
    return 1 + std::max(0, city.population - 1) / per + cityGreatPersonEffectTotal(city, GreatPersonEffectKind::DistrictCapacity);
}

int Game::districtCost(PlayerId player, TypeIndex type) const {
    const DistrictType& d = rules_->districts[static_cast<size_t>(type)];
    Fixed cost = Fixed::fromInt(d.cost * speedPercent(state_, *rules_)) / 100;
    // x (1 + 9 x the larger share of the tech or civic tree completed).
    if (d.costProgression != DistrictCostProgression::None) cost = cost * (Fixed::fromInt(1) + treeProgress(player) * 9);
    if (d.costProgression == DistrictCostProgression::NumUnderAvgPlusTech && d.costDiscountPercent > 0) {
        // Discount for a type built less than the player's own average: A types unlocked,
        // B districts completed, discounted while B >= A and B / A > this type's count.
        int unlocked = 0, completed = 0, ofType = 0;
        for (size_t i = 0; i < rules_->districts.size(); ++i) {
            const DistrictType& o = rules_->districts[i];
            if (o.needsPopulation && o.costProgression == DistrictCostProgression::NumUnderAvgPlusTech &&
                hasUnlocked(player, o.unlock))
                ++unlocked;
        }
        for (const City& c : state_.cities) {
            if (c.owner != player) continue;
            for (const CityDistrict& cd : c.districts) {
                if (!cd.complete || !rules_->districts[static_cast<size_t>(cd.type)].needsPopulation) continue;
                ++completed;
                ofType += cd.type == type ? 1 : 0;
            }
        }
        if (unlocked > 0 && completed >= unlocked && completed > ofType * unlocked)
            cost = cost * (100 - d.costDiscountPercent) / 100;
    }
    return std::max(1, static_cast<int>(cost.toInt()));
}

bool Game::districtOpenIn(const City& city, TypeIndex type) const {
    const DistrictType& d = rules_->districts[static_cast<size_t>(type)];
    if (d.cost <= 0 || !hasUnlocked(city.owner, d.unlock) || city.district(type, false)) return false;
    if (d.needsPopulation) {
        int used = 0;
        for (const CityDistrict& cd : city.districts) used += rules_->districts[static_cast<size_t>(cd.type)].needsPopulation ? 1 : 0;
        if (used >= districtLimit(city)) return false;
    }
    return true;
}

bool Game::districtUnblockedIn(const City& city, TypeIndex type) const {
    const DistrictType& d = rules_->districts[static_cast<size_t>(type)];
    for (TypeIndex other : d.exclusiveWith) {
        if (city.district(other, false)) return false;
    }
    if (d.onePerPlayer) {
        for (const City& c : state_.cities) {
            if (c.owner == city.owner && c.district(type, false)) return false;
        }
    }
    return true;
}

bool Game::canPlaceDistrict(const City& city, TypeIndex type, Hex plot, CommandError* why) const {
    return canPlaceDistrict(city, type, plot, why, true);
}

bool Game::canPlaceDistrict(const City& city, TypeIndex type, Hex plot, CommandError* why, bool cityChecks) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    if (type < 0 || static_cast<size_t>(type) >= rules_->districts.size()) return fail(CommandError::CannotBuild);
    const DistrictType& d = rules_->districts[static_cast<size_t>(type)];
    if (cityChecks && !districtOpenIn(city, type)) return fail(CommandError::CannotBuild);
    auto h = state_.grid.normalize(plot);
    if (!h || *h != plot) return fail(CommandError::BadTarget);
    // Owned by this city, within 3, open land, no visible luxury or strategic resource.
    const Plot& p = state_.plot(plot);
    if (p.city != city.id || plot == city.pos || state_.grid.distance(city.pos, plot) > 3) return fail(CommandError::BadTarget);
    // Nothing built on it yet (no city: the plot is this city's land, and not its center).
    if (state_.districtAt(plot) || state_.wonderAt(plot) != kNone || campAt(plot)) return fail(CommandError::BadTarget);
    if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].naturalWonder) return fail(CommandError::BadTarget);  // 01: natural wonders
    if (d.water) {
        // Harbor: Coast or Lake (not Ocean) next to land.
        const TerrainType& t = rules_->terrains[static_cast<size_t>(p.terrain)];
        if (!t.water || t.impassable || t.id == "TERRAIN_OCEAN") return fail(CommandError::BadTarget);
        bool nearLand = false;
        for (int dir = 0; dir < kNumDirs; ++dir) {
            auto n = state_.grid.neighbor(plot, static_cast<Dir>(dir));
            if (n && !rules_->terrains[static_cast<size_t>(state_.plot(*n).terrain)].water) nearLand = true;
        }
        if (!nearLand) return fail(CommandError::BadTarget);
    } else if (!isLandPassable(state_, *rules_, plot)) {
        return fail(CommandError::BadTarget);
    }
    if (resourceVisible(city.owner, plot) &&
        rules_->resources[static_cast<size_t>(p.resource)].cls != ResourceClass::Bonus)
        return fail(CommandError::BadTarget);
    if (d.notAdjacentToCityCenter && state_.grid.distance(city.pos, plot) == 1) return fail(CommandError::BadTarget);
    if (!d.validTerrains.empty() && std::find(d.validTerrains.begin(), d.validTerrains.end(), p.terrain) == d.validTerrains.end())
        return fail(CommandError::BadTarget);
    if (cityChecks && !districtUnblockedIn(city, type)) return fail(CommandError::CannotBuild);
    if (d.aqueduct) {
        // Next to the City Center and to a River, Lake, Oasis (fresh-water features) or Mountain.
        if (state_.grid.distance(city.pos, plot) != 1) return fail(CommandError::BadTarget);
        bool water = onRiver(state_, plot);
        for (int dir = 0; dir < kNumDirs && !water; ++dir) {
            auto n = state_.grid.neighbor(plot, static_cast<Dir>(dir));
            if (!n) continue;
            const Plot& np = state_.plot(*n);
            water = rules_->terrains[static_cast<size_t>(np.terrain)].relief == Relief::Mountain ||
                    (np.feature != kNone && rules_->features[static_cast<size_t>(np.feature)].freshWater) || isLake(state_, *rules_, *n, &lakes_);
        }
        if (!water) return fail(CommandError::BadTarget);
    }
    if (d.canal) {
        // Between two bodies of water (two water neighbours not touching each other), or between
        // water and the City Center (03: Canal [GS]).
        std::vector<Hex> water;
        for (int dir = 0; dir < kNumDirs; ++dir) {
            auto n = state_.grid.neighbor(plot, static_cast<Dir>(dir));
            if (n && rules_->terrains[static_cast<size_t>(state_.plot(*n).terrain)].water) water.push_back(*n);
        }
        bool links = !water.empty() && state_.grid.distance(city.pos, plot) == 1;
        for (size_t a = 0; a < water.size() && !links; ++a) {
            for (size_t b = a + 1; b < water.size() && !links; ++b) links = state_.grid.distance(water[a], water[b]) > 1;
        }
        if (!links) return fail(CommandError::BadTarget);
    }
    if (d.floodplainsRiver) {
        // On Floodplains along a river (Dam).
        const std::string& f = p.feature == kNone ? std::string() : rules_->features[static_cast<size_t>(p.feature)].id;
        if (f.rfind("FEATURE_FLOODPLAINS", 0) != 0 || !onRiver(state_, plot)) return fail(CommandError::BadTarget);
    }
    if (why) *why = CommandError::Ok;
    return true;
}

int Game::plotAppeal(Hex plot) const {
    int appeal = onRiver(state_, plot) || isLakeAdjacent(state_, *rules_, plot, &lakes_) ? 1 : 0;  // +1 once next to a river or lake
    const City* home = state_.city(state_.plot(plot).city);
    const uint32_t held = home ? heldWonders(home->owner, bit(W::Biosphere) | bit(W::Eiffel) | bit(W::GoldenGate)) : 0;
    const bool biosphere = (held & bit(W::Biosphere)) != 0;  // Biosphère (03): Rainforest and Marsh +1 Appeal
    for (int dir = 0; dir < kNumDirs; ++dir) {
        const std::optional<Hex> n = state_.grid.neighbor(plot, static_cast<Dir>(dir));
        if (!n) continue;
        const Plot& np = state_.plot(*n);
        appeal += rules_->terrains[static_cast<size_t>(np.terrain)].appeal;
        if (np.feature != kNone) {
            const FeatureType& f = rules_->features[static_cast<size_t>(np.feature)];
            appeal += f.appeal + (biosphere && (f.id == "FEATURE_JUNGLE" || f.id == "FEATURE_MARSH") ? 1 : 0);
        }
        if (np.improvement != kNone) appeal += np.pillagedTurns > 0 ? -1 : rules_->improvements[static_cast<size_t>(np.improvement)].appeal;
        if (const CityDistrict* d = state_.districtAt(*n)) appeal += rules_->districts[static_cast<size_t>(d->type)].appeal;
        if (state_.wonderAt(*n) != kNone) appeal += 1;
        if (campAt(*n)) appeal -= 1;
    }
    // Alvar Aalto, Charles Correa (07): appeal across the city where they were used; Eiffel Tower, Golden Gate Bridge (03) in all.
    if (home && (!home->greatPeopleHere.empty() || (held & (bit(W::Eiffel) | bit(W::GoldenGate))) != 0))
        appeal += static_cast<int>(sumCityModifiers(state_, *rules_, *home, ModEffect::CityAppeal).toInt());
    return appeal;
}

Fixed Game::districtHousing(const City& city) const {
    Fixed total;
    const bool works = cityGovernorHas(city, "GOVERNOR_PROMOTION_WATER_WORKS");  // Liang
    for (const CityDistrict& cd : city.districts) {
        if (!cd.complete) continue;
        const DistrictType& d = rules_->districts[static_cast<size_t>(cd.type)];
        total += Fixed::fromInt(d.housing);
        if (works && (d.id == "DISTRICT_NEIGHBORHOOD" || d.id == "DISTRICT_AQUEDUCT")) total += Fixed::fromInt(2);
        if (!d.appealHousing.empty()) {
            const int appeal = plotAppeal(cd.pos);
            for (const auto& [minimum, change] : d.appealHousing) {
                if (appeal >= minimum) {
                    total += Fixed::fromInt(change);
                    break;
                }
            }
        }
        if (d.aqueduct) {
            // Up to CITY_POPULATION_AQUEDUCT_MIN without fresh water, else +CITY_POPULATION_AQUEDUCT_BOOST.
            const bool fresh = hasFreshWater(state_, *rules_, city.pos, &lakes_);
            bool coastal = false;
            state_.grid.forEachWithin(city.pos, 1, [&](Hex n) {
                if (n != city.pos && rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].shallowWater) coastal = true;
            });
            if (fresh) {
                total += rules_->global("CITY_POPULATION_AQUEDUCT_BOOST");
            } else {
                const Fixed base = rules_->global(coastal ? HotGlobal::CityPopulationCoast : HotGlobal::CityPopulationNoWater);
                total += std::max(Fixed(), rules_->global("CITY_POPULATION_AQUEDUCT_MIN") - base);
            }
        }
    }
    return total;
}

int Game::districtAmenities(const City& city) const {
    int total = 0;
    const bool works = cityGovernorHas(city, "GOVERNOR_PROMOTION_WATER_WORKS");  // Liang
    for (const CityDistrict& cd : city.districts) {
        if (!cd.complete) continue;
        const DistrictType& d = rules_->districts[static_cast<size_t>(cd.type)];
        total += d.amenities;
        if (works && (d.id == "DISTRICT_CANAL" || d.id == "DISTRICT_DAM")) total += 1;
        if (d.amenityFeature != kNone) {
            const auto beside = state_.grid.within(cd.pos, 1);
            if (std::any_of(beside.begin(), beside.end(), [&](const Hex& n) { return n != cd.pos && state_.plot(n).feature == d.amenityFeature; }))
                total += d.amenityFeatureAmount;
        }
    }
    return total;
}

bool Game::cityPrevents(CityId id, bool floods) const {
    const City* c = state_.city(id);
    if (!c) return false;
    for (const CityDistrict& cd : c->districts) {
        const DistrictType& d = rules_->districts[static_cast<size_t>(cd.type)];
        if (cd.complete && (floods ? d.preventsFloods : d.preventsDrought)) return true;
    }
    return false;
}

std::vector<Hex> Game::districtPlots(CityId id, TypeIndex type) const {
    std::vector<Hex> out;
    const City* c = state_.city(id);
    if (!c) return out;
    state_.grid.forEachWithin(c->pos, 3, [&](Hex h) {
        if (canPlaceDistrict(*c, type, h, nullptr, true)) out.push_back(h);
    });
    return out;
}

bool Game::anyDistrictPlot(CityId id, TypeIndex type) const {
    const City* c = state_.city(id);
    if (!c) return false;
    // canPlaceDistrict on each plot, with what does not depend on the plot looked at once, and only the city's own
    // plots looked at.
    if (type < 0 || static_cast<size_t>(type) >= rules_->districts.size() || !districtOpenIn(*c, type) || !districtUnblockedIn(*c, type)) return false;
    bool any = false;
    state_.grid.forEachWithin(c->pos, 3, [&](Hex h) {
        any = any || (h != c->pos && state_.plot(h).city == c->id && canPlaceDistrict(*c, type, h, nullptr, false));
    });
    return any;
}

int Game::specialistSlots(const City& city, const CityDistrict& district) const {
    int slots = 0;
    for (TypeIndex b : city.buildings) {
        const BuildingType& bt = rules_->buildings[static_cast<size_t>(b)];
        if (bt.districtType == district.type) slots += bt.citizenSlots;
    }
    return slots;
}

Yields Game::specialistYield(const City& city, const CityDistrict& district) const {
    Yields y = rules_->districts[static_cast<size_t>(district.type)].specialistYields;
    for (TypeIndex b : city.buildings) {
        const BuildingType& bt = rules_->buildings[static_cast<size_t>(b)];
        if (bt.districtType != district.type) continue;
        for (size_t i = 0; i < kNumYields; ++i) y[i] += bt.specialistYields[i];
    }
    return y;
}

Yields Game::districtAdjacency(PlayerId player, TypeIndex type, Hex plot) const {
    Yields out{};
    if (districtPillaged(plot)) return out;  // 05: Pillage
    const DistrictType& d = rules_->districts[static_cast<size_t>(type)];
    const TypeIndex cityCenter = rules_->district("DISTRICT_CITY_CENTER");
    // The neighbours (within(plot, 1) less the plot), with the district and the city on each looked up at most once:
    // several rows ask after them.
    Hex nbr[kNumDirs];
    int nbrs = 0;
    state_.grid.forEachWithin(plot, 1, [&](Hex n) {
        if (n != plot && nbrs < kNumDirs) nbr[nbrs++] = n;
    });
    const CityDistrict* nearDistrict[kNumDirs] = {};
    uint8_t nearCity[kNumDirs] = {};  // 0 not looked up yet, 1 none, 2 a city
    uint8_t districtKnown = 0;        // bit k: nearDistrict[k] looked up
    const auto districtNear = [&](int k) {
        if (!(districtKnown & (1u << k))) {
            nearDistrict[k] = state_.districtAt(nbr[k]);
            districtKnown = static_cast<uint8_t>(districtKnown | (1u << k));
        }
        return nearDistrict[k];
    };
    const auto cityNear = [&](int k) {
        if (nearCity[k] == 0) nearCity[k] = static_cast<uint8_t>(state_.cityAt(nbr[k]) ? 2 : 1);
        return nearCity[k] == 2;
    };
    for (const DistrictAdjacency& a : d.adjacency) {
        int matches = 0;
        if (a.kind == DistrictAdjacencyKind::River) {
            matches = onRiver(state_, plot) ? 1 : 0;
        } else {
            for (int k = 0; k < nbrs; ++k) {
                const Hex n = nbr[k];
                const Plot& np = state_.plot(n);
                bool hit = false;
                switch (a.kind) {
                    case DistrictAdjacencyKind::Mountain:
                        hit = rules_->terrains[static_cast<size_t>(np.terrain)].relief == Relief::Mountain;
                        break;
                    case DistrictAdjacencyKind::AnyDistrict: hit = districtNear(k) || cityNear(k); break;  // city centers count
                    case DistrictAdjacencyKind::District: {
                        const CityDistrict* nd = districtNear(k);
                        hit = (nd && nd->type == a.ref) || (a.ref == cityCenter && cityNear(k));
                        break;
                    }
                    case DistrictAdjacencyKind::SeaResource:
                        hit = rules_->terrains[static_cast<size_t>(np.terrain)].water && resourceVisible(player, n);
                        break;
                    case DistrictAdjacencyKind::Feature: hit = np.feature == a.ref; break;
                    case DistrictAdjacencyKind::Improvement: hit = np.improvement == a.ref; break;
                    case DistrictAdjacencyKind::StrategicResource:
                        hit = resourceVisible(player, n) &&
                              rules_->resources[static_cast<size_t>(np.resource)].cls == ResourceClass::Strategic;
                        break;
                    case DistrictAdjacencyKind::Wonder: hit = finishedWonderAt(state_, n); break;
                    case DistrictAdjacencyKind::NaturalWonder:
                        hit = np.feature != kNone && rules_->features[static_cast<size_t>(np.feature)].naturalWonder;
                        break;
                    case DistrictAdjacencyKind::River: break;
                }
                matches += hit ? 1 : 0;
            }
        }
        out[static_cast<size_t>(a.yield)] += Fixed::fromInt(a.amount * (matches / a.tilesRequired));  // each "per 2" row floored
    }
    // Civ abilities: extra adjacency from a district or from terrain (leaders-and-art-style).
    for (const CivAdjacency& a : civAbility(player).extraAdjacency) {
        if (a.district != type) continue;
        int matches = 0;
        for (int k = 0; k < nbrs; ++k) {
            if (a.from != kNone) {
                const CityDistrict* nd = districtNear(k);
                matches += nd && nd->type == a.from ? 1 : 0;
            } else {
                matches += rules_->terrains[static_cast<size_t>(state_.plot(nbr[k]).terrain)].base == a.fromTerrainBase ? 1 : 0;
            }
        }
        out[static_cast<size_t>(a.yield)] += Fixed::fromInt(a.amount * (matches / a.per));
    }
    // Pantheons (06): a Holy Site's Faith from adjacent Tundra (Dance of the Aurora), Desert (Desert Folklore) or Rainforest (Sacred Path).
    if (d.id == "DISTRICT_HOLY_SITE") {
        const City* follower = state_.city(state_.plot(plot).city);
        if (follower) {
            const bool aurora = cityFollows(*follower, Bf::DanceOfTheAurora), folklore = cityFollows(*follower, Bf::DesertFolklore),
                       path = cityFollows(*follower, Bf::SacredPath);
            if (aurora || folklore || path) {
                int n = 0;
                for (const Hex& h : state_.grid.within(plot, 1)) {
                    if (h == plot) continue;
                    const Plot& q = state_.plot(h);
                    const TerrainType& t = rules_->terrains[static_cast<size_t>(q.terrain)];
                    const bool flatOrHills = t.relief != Relief::Mountain;
                    n += (aurora && flatOrHills && t.base == "TUNDRA") || (folklore && flatOrHills && t.base == "DESERT") ||
                                 (path && q.feature != kNone && rules_->features[static_cast<size_t>(q.feature)].id == "FEATURE_JUNGLE")
                             ? 1
                             : 0;
                }
                out[static_cast<size_t>(YieldType::Faith)] += Fixed::fromInt(n);
            }
        }
    }
    // Machu Picchu (03: Wonders): +1 per adjacent Mountain for Commercial Hubs, Industrial Zones and Theater Squares.
    const YieldType mountainYield = d.id == "DISTRICT_COMMERCIAL_HUB" ? YieldType::Gold
                                    : d.id == "DISTRICT_INDUSTRIAL_ZONE" ? YieldType::Production
                                    : d.id == "DISTRICT_THEATER_SQUARE" ? YieldType::Culture
                                                                        : YieldType::Food;
    if (mountainYield != YieldType::Food && holdsWonder(player, W::MachuPicchu)) {
        int mountains = 0;
        for (const Hex& n : state_.grid.within(plot, 1)) {
            if (n != plot && rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].relief == Relief::Mountain) ++mountains;
        }
        out[static_cast<size_t>(mountainYield)] += Fixed::fromInt(mountains);
    }
    int pct = 100 + sumDistrictAdjacencyPercent(state_, *rules_, state_.players[static_cast<size_t>(player)], type);
    // Vilnius (08: suzerain): +50% Theater Square adjacency for each level of its suzerain's best alliance (1+, 2+, 3+).
    if (d.id == "DISTRICT_THEATER_SQUARE" && suzerainBonus(player, Cs::Vilnius)) {
        int level = 0;
        for (const Player& o : state_.players) level = std::max(level, allianceLevel(player, o.id));
        pct += 50 * level;
    }
    // Reyna's Harbormaster doubles the Commercial Hub's and Harbor's adjacency in her city.
    const Plot& here = state_.plot(plot);
    const City* home = here.city == kNoCity ? nullptr : state_.city(here.city);
    if (home && home->owner == player && (d.id == "DISTRICT_COMMERCIAL_HUB" || d.id == "DISTRICT_HARBOR") && cityGovernorHas(*home, "GOVERNOR_PROMOTION_HARBORMASTER"))
        pct += 100;
    for (Fixed& y : out) y = y * std::max(0, pct) / 100;
    return out;
}

void Game::placeDistrict(City& city, TypeIndex type, Hex plot) {
    // Placing clears removable features, any improvement and a bonus resource.
    Plot& p = state_.plot(plot);
    if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].removable) p.feature = kNone;
    p.improvement = kNone;
    if (p.resource != kNone && rules_->resources[static_cast<size_t>(p.resource)].cls == ResourceClass::Bonus) {
        p.resource = kNone;
        p.resourceAmount = 0;
    }
    city.districts.push_back({type, plot, false});
    assignCitizens(city);  // the plot can no longer be worked
}

}  // namespace sov
