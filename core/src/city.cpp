// City rules (specs/civ6/02-cities.md): yields, citizens, growth, housing,
// amenities, border growth, production, purchases, gold and maintenance.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
constexpr size_t idx(YieldType y) { return static_cast<size_t>(y); }

bool insertSorted(std::vector<int32_t>& v, int32_t x) {
    auto it = std::lower_bound(v.begin(), v.end(), x);
    if (it != v.end() && *it == x) return false;
    v.insert(it, x);
    return true;
}

bool eraseSorted(std::vector<int32_t>& v, int32_t x) {
    auto it = std::lower_bound(v.begin(), v.end(), x);
    if (it == v.end() || *it != x) return false;
    v.erase(it);
    return true;
}

// Weighting for the default (balanced) citizen focus.
Fixed citizenScore(const Yields& y) {
    return y[idx(YieldType::Food)] * 4 + y[idx(YieldType::Production)] * 3 + y[idx(YieldType::Gold)] * 2 +
           y[idx(YieldType::Science)] * 2 + y[idx(YieldType::Culture)] * 2 + y[idx(YieldType::Faith)];
}

int speedPercent(const GameState& s, const Rules& r) {
    return r.speeds[static_cast<size_t>(r.speed(s.setup.speed))].costPercent;
}
}  // namespace

bool City::has(TypeIndex building) const {
    return std::binary_search(buildings.begin(), buildings.end(), building);
}

// ------------------------------------------------------------------ queries

Yields Game::plotYields(Hex at, const City& city) const {
    const Plot& p = state_.plot(at);
    Yields y = rules_->terrains[static_cast<size_t>(p.terrain)].yields;
    if (p.feature != kNone) {
        const Yields& f = rules_->features[static_cast<size_t>(p.feature)].yields;
        for (size_t i = 0; i < kNumYields; ++i) y[i] += f[i];
    }
    if (p.resource != kNone) {
        const ResourceType& res = rules_->resources[static_cast<size_t>(p.resource)];
        // Resources revealed by a tech stay hidden until research exists (MVP-3).
        if (res.revealTech.empty()) {
            for (size_t i = 0; i < kNumYields; ++i) y[i] += res.yields[i];
        }
    }
    if (at == city.pos) {
        y[idx(YieldType::Food)] = std::max(y[idx(YieldType::Food)], rules_->global("YIELD_FOOD_CITY_TERRAIN_REPLACE"));
        y[idx(YieldType::Production)] =
            std::max(y[idx(YieldType::Production)], rules_->global("YIELD_PRODUCTION_CITY_TERRAIN_REPLACE"));
    }
    for (size_t i = 0; i < kNumYields; ++i) {
        y[i] += sumPlotModifiers(state_, *rules_, city, at, static_cast<YieldType>(i));
    }
    return y;
}

std::vector<Hex> Game::workablePlots(const City& city) const {
    std::vector<Hex> out;
    for (const Hex& h : state_.grid.within(city.pos, 3)) {
        if (h == city.pos) continue;
        const Plot& p = state_.plot(h);
        if (p.city != city.id) continue;
        if (rules_->terrains[static_cast<size_t>(p.terrain)].impassable) continue;
        if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].impassable) continue;
        out.push_back(h);
    }
    return out;
}

CityReport Game::cityReport(CityId id) const {
    CityReport rep;
    const City* c = state_.city(id);
    if (!c) return rep;
    const Player& owner = state_.players[static_cast<size_t>(c->owner)];
    Yields raw = plotYields(c->pos, *c);
    for (int32_t pi : c->worked) {
        Yields y = plotYields(state_.grid.at(pi), *c);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += y[i];
    }
    for (TypeIndex b : c->buildings) {
        const BuildingType& bt = rules_->buildings[static_cast<size_t>(b)];
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += bt.yields[i];
        rep.housing += bt.housing;
        rep.amenities += bt.amenities;
    }
    // Every citizen adds a little culture and science (CULTURE/SCIENCE_PERCENTAGE_YIELD_PER_POP).
    raw[idx(YieldType::Culture)] += Fixed::ratio(rules_->globalInt("CULTURE_PERCENTAGE_YIELD_PER_POP"), 100) * c->population;
    raw[idx(YieldType::Science)] += Fixed::ratio(rules_->globalInt("SCIENCE_PERCENTAGE_YIELD_PER_POP"), 100) * c->population;
    for (size_t i = 0; i < kNumYields; ++i) {
        raw[i] += sumCityModifiers(state_, *rules_, *c, ModEffect::CityYield, static_cast<YieldType>(i));
    }

    // Housing from water access, then buildings and modifiers.
    bool fresh = isRiverAdjacent(state_, c->pos), coastal = false;
    for (const Hex& n : state_.grid.within(c->pos, 1)) {
        const Plot& p = state_.plot(n);
        if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].freshWater) fresh = true;
        if (n != c->pos && rules_->terrains[static_cast<size_t>(p.terrain)].shallowWater) coastal = true;
    }
    const char* water = fresh ? "CITY_POPULATION_RIVER_LAKE" : coastal ? "CITY_POPULATION_COAST" : "CITY_POPULATION_NO_WATER";
    rep.housing += rules_->global(water);
    rep.housing += sumCityModifiers(state_, *rules_, *c, ModEffect::CityHousing);

    // Amenities: bankruptcy costs 1 per 10 gold below zero (00-overview.md, Turn processing order).
    rep.amenities += static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityAmenities).toInt());
    if (owner.gold < Fixed()) rep.amenities -= static_cast<int>((-owner.gold).ceil() + 9) / 10;
    const int perAmenity = std::max(1, rules_->globalInt("CITY_POP_PER_AMENITY"));
    rep.amenitiesNeeded = std::max(0, (c->population + perAmenity - 1) / perAmenity - 1);
    const int balance = rep.amenities - rep.amenitiesNeeded;
    rep.happiness = 0;
    for (size_t i = 0; i < rules_->happiness.size(); ++i) {
        if (balance >= rules_->happiness[i].minBalance) rep.happiness = static_cast<int>(i);
    }
    const int moodYield = rules_->happiness.empty() ? 0 : rules_->happiness[static_cast<size_t>(rep.happiness)].yieldPercent;

    for (size_t i = 0; i < kNumYields; ++i) {
        int pct = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityYieldPercent,
                                                          static_cast<YieldType>(i)).toInt());
        if (i != idx(YieldType::Food)) pct += moodYield;
        rep.yields[i] = raw[i] * std::max(0, pct) / 100;
    }
    rep.foodConsumption = rules_->global("CITY_FOOD_CONSUMPTION_PER_POPULATION") * c->population;
    rep.defense = static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityDefense).toInt());
    return rep;
}

int Game::growthThreshold(int population) const {
    // 15 + 8n + n^1.5 with n = population - 1, rounded down, scaled by speed.
    const int n = std::max(0, population - 1);
    Fixed t = rules_->global("CITY_GROWTH_THRESHOLD") + rules_->global("CITY_GROWTH_MULTIPLIER") * n +
              Fixed::pow(Fixed::fromInt(n), rules_->global("CITY_GROWTH_EXPONENT"));
    return static_cast<int>(t.floor() * speedPercent(state_, *rules_) / 100);
}

int Game::borderGrowthCost(int plotsAcquired) const {
    // 10 + (6n)^1.3 culture, scaled by speed.
    Fixed t = rules_->global("CULTURE_COST_FIRST_PLOT") +
              Fixed::pow(rules_->global("CULTURE_COST_LATER_PLOT_MULTIPLIER") * plotsAcquired,
                         rules_->global("CULTURE_COST_LATER_PLOT_EXPONENT"));
    return static_cast<int>(t.floor() * speedPercent(state_, *rules_) / 100);
}

int Game::productionCost(PlayerId player, ProductionItem item) const {
    int base = 0;
    if (item.kind == ProductionKind::Unit) {
        const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
        const Player& p = state_.players[static_cast<size_t>(player)];
        int copies = static_cast<size_t>(item.type) < p.unitsTrained.size() ? p.unitsTrained[static_cast<size_t>(item.type)] : 0;
        base = u.cost + u.costProgression * copies;
    } else {
        base = rules_->buildings[static_cast<size_t>(item.type)].cost;
    }
    return std::max(1, base * speedPercent(state_, *rules_) / 100);
}

int Game::purchaseCost(PlayerId player, ProductionItem item) const {
    if (item.kind == ProductionKind::Unit) {
        if (rules_->units[static_cast<size_t>(item.type)].purchaseYield != "GOLD") return -1;
    } else if (!rules_->buildings[static_cast<size_t>(item.type)].purchasable) {
        return -1;
    }
    int cost = productionCost(player, item) * rules_->globalInt("GOLD_PURCHASE_MULTIPLIER") *
               std::max(1, rules_->globalInt("GOLD_PURCHASE_ENGINE_FACTOR"));
    return cost / 5 * 5;
}

int Game::plotPurchaseCost(CityId id, Hex at) const {
    const City* c = state_.city(id);
    auto h = state_.grid.normalize(at);
    if (!c || !h) return -1;
    const int dist = state_.grid.distance(c->pos, *h);
    if (dist < 1 || dist > rules_->globalInt("CITY_MAX_BUY_PLOT_RANGE")) return -1;
    if (state_.plot(*h).owner != kNoPlayer) return -1;
    bool adjacent = false;
    for (const Hex& n : state_.grid.within(*h, 1)) adjacent = adjacent || state_.plot(n).city == id;
    if (!adjacent) return -1;
    // 50 gold two plots out, 75 three out (02-cities.md, Tile purchase); rises
    // with research share once research exists.
    int cost = rules_->globalInt("PLOT_BUY_BASE_COST") * std::max(2, dist) / 2;
    return cost * speedPercent(state_, *rules_) / 100;
}

bool Game::canProduce(const City& c, ProductionItem item, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    if (item.kind == ProductionKind::Unit) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->units.size()) return fail(CommandError::CannotBuild);
        const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
        // Naval units need coastal cities (later); research gates unlocks (MVP-3).
        if (u.domain != Domain::Land || u.mustPurchase || u.cost <= 0 || !u.unlock.empty())
            return fail(CommandError::CannotBuild);
    } else if (item.kind == ProductionKind::Building) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->buildings.size()) return fail(CommandError::CannotBuild);
        const BuildingType& b = rules_->buildings[static_cast<size_t>(item.type)];
        if (b.granted || c.has(item.type) || !b.unlock.empty() || b.district != "DISTRICT_CITY_CENTER")
            return fail(CommandError::CannotBuild);
        for (TypeIndex req : b.prereqs) {
            if (!c.has(req)) return fail(CommandError::CannotBuild);
        }
        if (b.needsRiver && !isRiverAdjacent(state_, c.pos)) return fail(CommandError::CannotBuild);
    } else {
        return fail(CommandError::CannotBuild);
    }
    if (why) *why = CommandError::Ok;
    return true;
}

std::vector<ProductionItem> Game::buildableItems(CityId id) const {
    std::vector<ProductionItem> out;
    const City* c = state_.city(id);
    if (!c) return out;
    for (size_t i = 0; i < rules_->units.size(); ++i) {
        ProductionItem it{ProductionKind::Unit, static_cast<TypeIndex>(i)};
        if (canProduce(*c, it)) out.push_back(it);
    }
    for (size_t i = 0; i < rules_->buildings.size(); ++i) {
        ProductionItem it{ProductionKind::Building, static_cast<TypeIndex>(i)};
        if (canProduce(*c, it) && std::find(c->queue.begin(), c->queue.end(), it) == c->queue.end()) out.push_back(it);
    }
    return out;
}

std::vector<CityId> Game::citiesNeedingProduction(PlayerId player) const {
    std::vector<CityId> out;
    for (const City& c : state_.cities) {
        if (c.owner == player && c.queue.empty()) out.push_back(c.id);
    }
    return out;
}

Fixed Game::goldPerTurn(PlayerId player) const {
    Fixed net;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        net += cityReport(c.id).yields[idx(YieldType::Gold)];
        for (TypeIndex b : c.buildings) net -= Fixed::fromInt(rules_->buildings[static_cast<size_t>(b)].maintenance);
    }
    for (const Unit& u : state_.units) {
        if (u.owner == player) net -= Fixed::fromInt(rules_->units[static_cast<size_t>(u.type)].maintenance);
    }
    return net;
}

std::optional<Hex> Game::unitSpawnPlot(const City& c, TypeIndex unitType) const {
    const UnitLayer layer = rules_->units[static_cast<size_t>(unitType)].layer;
    for (const Hex& h : state_.grid.within(c.pos, 1)) {  // the center comes first
        if (!isLandPassable(state_, *rules_, h)) continue;
        if (state_.foreignUnitAt(h, c.owner) || state_.unitAt(h, layer, *rules_)) continue;
        const City* other = state_.cityAt(h);
        if (other && other->owner != c.owner) continue;
        return h;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- commands

CommandError Game::validateCity(const Command& c) const {
    const City* city = state_.city(c.id);
    if (!city) return CommandError::BadCity;
    if (city->owner != c.player) return CommandError::NotYourCity;
    const ProductionItem item{static_cast<ProductionKind>(c.arg), static_cast<TypeIndex>(c.arg2)};
    CommandError why = CommandError::Ok;
    switch (c.type) {
        case CommandType::SetProduction:
            if (c.arg < 0 || c.arg > 1 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            canProduce(*city, item, &why);
            return why;
        case CommandType::QueueProduction:
            if (c.arg < 0 || c.arg > 1 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (!canProduce(*city, item, &why)) return why;
            if (item.kind == ProductionKind::Building &&
                std::find(city->queue.begin(), city->queue.end(), item) != city->queue.end())
                return CommandError::CannotBuild;
            if (static_cast<int>(city->queue.size()) >= rules_->globalInt("CITY_PRODUCTION_QUEUE_MAX"))
                return CommandError::QueueFull;
            return CommandError::Ok;
        case CommandType::Purchase: {
            if (c.arg < 0 || c.arg > 1 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (!canProduce(*city, item, &why)) return why;
            int cost = purchaseCost(c.player, item);
            if (cost < 0) return CommandError::CannotBuild;
            if (item.kind == ProductionKind::Unit) {
                const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
                if (city->population < u.minPopulation || !unitSpawnPlot(*city, item.type)) return CommandError::CannotBuild;
            }
            if (state_.players[static_cast<size_t>(c.player)].gold < Fixed::fromInt(cost)) return CommandError::NotEnoughGold;
            return CommandError::Ok;
        }
        case CommandType::BuyPlot: {
            int cost = plotPurchaseCost(c.id, c.target);
            if (cost < 0) return CommandError::CannotBuyPlot;
            if (state_.players[static_cast<size_t>(c.player)].gold < Fixed::fromInt(cost)) return CommandError::NotEnoughGold;
            return CommandError::Ok;
        }
        case CommandType::LockPlot: {
            auto h = state_.grid.normalize(c.target);
            if (!h) return CommandError::CannotWorkPlot;
            const int32_t pi = state_.grid.index(*h);
            if (c.arg == 0) {
                return std::binary_search(city->locked.begin(), city->locked.end(), pi) ? CommandError::Ok
                                                                                        : CommandError::CannotWorkPlot;
            }
            std::vector<Hex> ok = workablePlots(*city);
            if (std::find(ok.begin(), ok.end(), *h) == ok.end()) return CommandError::CannotWorkPlot;
            if (std::binary_search(city->locked.begin(), city->locked.end(), pi)) return CommandError::CannotWorkPlot;
            if (static_cast<int>(city->locked.size()) >= city->population) return CommandError::CannotWorkPlot;
            return CommandError::Ok;
        }
        default: break;
    }
    return CommandError::BadTarget;
}

void Game::applyCity(const Command& c) {
    City& city = *state_.city(c.id);
    const ProductionItem item{static_cast<ProductionKind>(c.arg), static_cast<TypeIndex>(c.arg2)};
    Player& p = state_.players[static_cast<size_t>(c.player)];
    switch (c.type) {
        case CommandType::SetProduction:
            city.queue.assign(1, item);
            break;
        case CommandType::QueueProduction:
            city.queue.push_back(item);
            break;
        case CommandType::Purchase:
            p.gold -= Fixed::fromInt(purchaseCost(c.player, item));
            completeItem(city, item);
            // A bought building leaves the queue; what was put into it carries over.
            if (item.kind == ProductionKind::Building) {
                City& bought = *state_.city(c.id);
                bought.queue.erase(std::remove(bought.queue.begin(), bought.queue.end(), item), bought.queue.end());
                for (size_t i = 0; i < bought.progress.size(); ++i) {
                    if (bought.progress[i].item == item) {
                        bought.overflow += bought.progress[i].amount;
                        bought.progress.erase(bought.progress.begin() + static_cast<long>(i));
                        break;
                    }
                }
            }
            break;
        case CommandType::BuyPlot: {
            p.gold -= Fixed::fromInt(plotPurchaseCost(c.id, c.target));
            Plot& plot = state_.plot(*state_.grid.normalize(c.target));
            plot.owner = city.owner;
            plot.city = city.id;
            assignCitizens(city);
            refreshVisibility(city.owner);
            break;
        }
        case CommandType::LockPlot: {
            const int32_t pi = state_.grid.index(*state_.grid.normalize(c.target));
            if (c.arg) insertSorted(city.locked, pi);
            else eraseSorted(city.locked, pi);
            assignCitizens(city);
            break;
        }
        default: break;
    }
}

// --------------------------------------------------------------- processing

void Game::assignCitizens(City& city) {
    std::vector<Hex> plots = workablePlots(city);
    // Drop locks on plots the city can no longer work.
    std::vector<int32_t> keep;
    for (int32_t pi : city.locked) {
        if (std::find(plots.begin(), plots.end(), state_.grid.at(pi)) != plots.end()) keep.push_back(pi);
    }
    city.locked = keep;
    while (static_cast<int>(city.locked.size()) > city.population) city.locked.pop_back();

    struct Cand { int32_t index; Fixed score; };
    std::vector<Cand> cands;
    for (const Hex& h : plots) {
        int32_t pi = state_.grid.index(h);
        if (std::binary_search(city.locked.begin(), city.locked.end(), pi)) continue;
        cands.push_back({pi, citizenScore(plotYields(h, city))});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.score != b.score ? a.score > b.score : a.index < b.index;
    });
    city.worked = city.locked;
    for (const Cand& c : cands) {
        if (static_cast<int>(city.worked.size()) >= city.population) break;
        city.worked.push_back(c.index);
    }
    std::sort(city.worked.begin(), city.worked.end());
}

bool Game::completeItem(City& city, ProductionItem item) {
    if (item.kind == ProductionKind::Unit) {
        const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
        if (city.population < u.minPopulation) return false;
        auto spot = unitSpawnPlot(city, item.type);
        if (!spot) return false;
        Player& p = state_.players[static_cast<size_t>(city.owner)];
        if (p.unitsTrained.size() < rules_->units.size()) p.unitsTrained.resize(rules_->units.size(), 0);
        ++p.unitsTrained[static_cast<size_t>(item.type)];
        city.population -= u.popCost;
        spawnUnit(item.type, city.owner, *spot);
        assignCitizens(city);
        refreshVisibility(city.owner);
    } else {
        auto it = std::lower_bound(city.buildings.begin(), city.buildings.end(), item.type);
        if (it == city.buildings.end() || *it != item.type) city.buildings.insert(it, item.type);
    }
    return true;
}

bool Game::growBorders(City& city) {
    const int maxDist = rules_->globalInt("PLOT_INFLUENCE_MAX_ACQUIRE_DISTANCE");
    std::optional<Hex> best;
    int64_t bestScore = 0;
    for (const Hex& h : state_.grid.within(city.pos, maxDist)) {
        const Plot& p = state_.plot(h);
        if (p.owner != kNoPlayer) continue;
        bool touches = false;
        for (const Hex& n : state_.grid.within(h, 1)) touches = touches || state_.plot(n).city == city.id;
        if (!touches) continue;
        // Lower is better (PLOT_INFLUENCE_*; 02-cities.md, Border growth).
        int64_t score = int64_t(rules_->globalInt("PLOT_INFLUENCE_RING_COST")) * state_.grid.distance(city.pos, h);
        const TerrainType& t = rules_->terrains[static_cast<size_t>(p.terrain)];
        if (t.water) score += rules_->globalInt("PLOT_INFLUENCE_WATER_COST");
        if (p.resource != kNone) score += rules_->globalInt("PLOT_INFLUENCE_RESOURCE_COST");
        Yields y = plotYields(h, city);
        Fixed total;
        for (const Fixed& v : y) total += v;
        score += total.toInt() * rules_->globalInt("PLOT_INFLUENCE_YIELD_POINT_COST");
        if (!best || score < bestScore) {
            best = h;
            bestScore = score;
        }
    }
    if (!best) return false;
    Plot& p = state_.plot(*best);
    p.owner = city.owner;
    p.city = city.id;
    return true;
}

void Game::processCities(PlayerId pid) {
    Player& player = state_.players[static_cast<size_t>(pid)];
    std::vector<CityId> ids;
    for (const City& c : state_.cities) {
        if (c.owner == pid) ids.push_back(c.id);
    }
    if (ids.empty()) return;

    // Steps 2-5 of the turn order: yields, gold and maintenance, research totals.
    std::vector<CityReport> reports;
    for (CityId id : ids) reports.push_back(cityReport(id));
    player.gold += goldPerTurn(pid);
    for (const CityReport& r : reports) {
        player.science += r.yields[idx(YieldType::Science)];
        player.culture += r.yields[idx(YieldType::Culture)];
        player.faith += r.yields[idx(YieldType::Faith)];
    }
    if (player.gold <= rules_->global("GOLD_NEGATIVE_BALANCE_DISBAND_UNIT_LINE")) {
        // Disband the costliest unit (ties: newest) while the treasury is underwater.
        const Unit* worst = nullptr;
        for (const Unit& u : state_.units) {
            if (u.owner != pid) continue;
            int m = rules_->units[static_cast<size_t>(u.type)].maintenance;
            if (m <= 0) continue;
            if (!worst || m >= rules_->units[static_cast<size_t>(worst->type)].maintenance) worst = &u;
        }
        if (worst) {
            UnitId gone = worst->id;
            state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                              [&](const Unit& u) { return u.id == gone; }),
                               state_.units.end());
        }
    }

    // Step 7: growth, production, borders.
    for (size_t k = 0; k < ids.size(); ++k) {
        City& city = *state_.city(ids[k]);
        const CityReport& rep = reports[k];

        // Growth (02-cities.md, Population and food; Housing).
        Fixed surplus = rep.yields[idx(YieldType::Food)] - rep.foodConsumption;
        if (surplus > Fixed()) {
            const HappinessLevel* mood = rules_->happiness.empty() ? nullptr : &rules_->happiness[static_cast<size_t>(rep.happiness)];
            int pct = 100 + (mood ? mood->growthPercent : 0) +
                      static_cast<int>(sumCityModifiers(state_, *rules_, city, ModEffect::CityGrowthPercent).toInt());
            surplus = surplus * std::max(0, pct) / 100;
            const Fixed room = rep.housing - Fixed::fromInt(city.population);
            if (room >= Fixed::fromInt(rules_->globalInt("CITY_HOUSING_LEFT_50PCT_GROWTH") + 1)) {
                // full growth
            } else if (room >= Fixed::fromInt(rules_->globalInt("CITY_HOUSING_LEFT_50PCT_GROWTH"))) {
                surplus = surplus / 2;
            } else if (room >= Fixed::fromInt(rules_->globalInt("CITY_HOUSING_LEFT_ZERO_GROWTH"))) {
                surplus = surplus / 4;
            } else {
                surplus = Fixed();
            }
        }
        city.food += surplus;
        if (city.food >= Fixed::fromInt(growthThreshold(city.population))) {
            ++city.population;
            city.food = Fixed();
            assignCitizens(city);
        } else if (city.food < Fixed()) {
            city.food = Fixed();
            if (city.population > 1) {
                --city.population;
                assignCitizens(city);
            }
        }

        // Production (02-cities.md, Production).
        Fixed prod = rep.yields[idx(YieldType::Production)] + city.overflow;
        city.overflow = Fixed();
        while (!city.queue.empty() && !canProduce(city, city.queue.front())) city.queue.erase(city.queue.begin());
        if (city.queue.empty()) {
            city.overflow = prod;
        } else {
            const ProductionItem item = city.queue.front();
            auto it = std::find_if(city.progress.begin(), city.progress.end(),
                                   [&](const ProductionProgress& pp) { return pp.item == item; });
            if (it == city.progress.end()) {
                city.progress.push_back({item, Fixed()});
                it = city.progress.end() - 1;
            }
            it->amount += prod;
            const Fixed cost = Fixed::fromInt(productionCost(pid, item));
            if (it->amount >= cost && completeItem(city, item)) {
                City& c2 = *state_.city(ids[k]);  // completeItem may spawn units, never cities
                auto it2 = std::find_if(c2.progress.begin(), c2.progress.end(),
                                        [&](const ProductionProgress& pp) { return pp.item == item; });
                c2.overflow = it2->amount - cost;
                c2.progress.erase(it2);
                c2.queue.erase(c2.queue.begin());
            }
        }

        // Border growth (02-cities.md, Border growth by culture).
        City& c3 = *state_.city(ids[k]);
        c3.borderCulture += rep.yields[idx(YieldType::Culture)];
        const Fixed cost = Fixed::fromInt(borderGrowthCost(c3.plotsByCulture));
        if (c3.borderCulture >= cost) {
            if (growBorders(c3)) {
                c3.borderCulture -= cost;
                ++c3.plotsByCulture;
                assignCitizens(c3);
            }
        }
    }
    refreshVisibility(pid);
}

}  // namespace sov
