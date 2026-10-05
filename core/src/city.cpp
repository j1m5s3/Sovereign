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

const CityDistrict* City::district(TypeIndex type, bool completeOnly) const {
    for (const CityDistrict& d : districts) {
        if (d.type == type && (d.complete || !completeOnly)) return &d;
    }
    return nullptr;
}

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
        // A resource yields nothing until the owner has the tech that reveals it.
        if (hasUnlocked(city.owner, res.reveal)) {
            for (size_t i = 0; i < kNumYields; ++i) y[i] += res.yields[i];
        }
    }
    if (at == city.pos) {
        y[idx(YieldType::Food)] = std::max(y[idx(YieldType::Food)], rules_->global("YIELD_FOOD_CITY_TERRAIN_REPLACE"));
        y[idx(YieldType::Production)] =
            std::max(y[idx(YieldType::Production)], rules_->global("YIELD_PRODUCTION_CITY_TERRAIN_REPLACE"));
    }
    // Civ unique buildings: a yield on adjacent improvements of a type next to its district (Mill Town).
    if (p.improvement != kNone && p.pillagedTurns == 0) {
        for (TypeIndex bi : city.buildings) {
            const BuildingType& b = rules_->buildings[static_cast<size_t>(bi)];
            if (b.adjacentImprovement != p.improvement) continue;
            const CityDistrict* d = b.districtType == kNone ? nullptr : city.district(b.districtType, true);
            const Hex home = d ? d->pos : city.pos;
            if (state_.grid.distance(home, at) == 1) y[idx(b.adjacentYield)] += Fixed::fromInt(b.adjacentAmount);
        }
    }
    // Civ ability: worked mountains (Inca).
    if (rules_->terrains[static_cast<size_t>(p.terrain)].relief == Relief::Mountain && at != city.pos)
        y[idx(YieldType::Production)] += Fixed::fromInt(civAbility(city.owner).mountainProduction);
    // Ground a disaster left fertile; a drought takes 1 Food (09: Climate and Disasters).
    for (size_t i = 0; i < kNumYields; ++i) y[i] += Fixed::fromInt(p.fertility[i]);
    if (!state_.droughts.empty() && inDrought(at)) y[idx(YieldType::Food)] = std::max(Fixed(), y[idx(YieldType::Food)] - rules_->global("DROUGHT_FOOD_LOSS_PER_TILE"));
    if (p.improvement != kNone && p.pillagedTurns == 0 && at != city.pos) {
        const Yields imp = improvementYields(at, city.owner);
        for (size_t i = 0; i < kNumYields; ++i) y[i] += imp[i];
    }
    for (size_t i = 0; i < kNumYields; ++i) {
        y[i] += sumPlotModifiers(state_, *rules_, city, at, static_cast<YieldType>(i));
    }
    if (p.improvement != kNone && p.pillagedTurns == 0 && rules_->improvements[static_cast<size_t>(p.improvement)].powerProvided > 0 &&
        cityGovernorHas(city, "GOVERNOR_PROMOTION_RENEWABLE_SUBSIDIZER"))
        y[idx(YieldType::Gold)] += Fixed::fromInt(2);  // Reyna
    if (p.fallout > 0 && at != city.pos) return Yields{};  // contaminated ground cannot be worked
    return y;
}

std::vector<Hex> Game::workablePlots(const City& city) const {
    std::vector<Hex> out;
    for (const Hex& h : state_.grid.within(city.pos, 3)) {
        if (h == city.pos) continue;
        const Plot& p = state_.plot(h);
        if (p.city != city.id || state_.districtAt(h) || state_.wonderAt(h) != kNone) continue;  // district and wonder plots are not worked
        const TerrainType& t = rules_->terrains[static_cast<size_t>(p.terrain)];
        // Mountains can be worked by a civ whose ability allows it (Inca).
        if (t.impassable && !(t.relief == Relief::Mountain && civAbility(city.owner).mountainProduction > 0)) continue;
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
    // Envoys to city-states pay in the capital and per building (08).
    {
        const Yields ey = envoyYields(*c);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += ey[i];
    }
    // Trade routes from this city pay by the districts at their destinations (07).
    for (const TradeRoute& tr : state_.tradeRoutes) {
        if (tr.origin != c->id) continue;
        if (const City* dest = state_.city(tr.destination)) {
            const Yields ty = tradeRouteYields(*c, *dest);
            for (size_t i = 0; i < kNumYields; ++i) raw[i] += ty[i];
        }
    }
    // Great Works in the city's slots, and great people whose effects improve its buildings.
    for (const GreatWork& w : c->greatWorks) {
        const GreatWorkType& gw = rules_->greatWorkTypes[static_cast<size_t>(w.type)];
        raw[idx(gw.yield)] += Fixed::fromInt(gw.amount);
    }
    for (TypeIndex person : owner.greatPeopleActivated) {
        for (const GreatPersonEffect& fx : rules_->greatPeople[static_cast<size_t>(person)].effects) {
            if (fx.kind == GreatPersonEffectKind::BuildingYield && c->has(fx.ref)) raw[idx(fx.yield)] += Fixed::fromInt(fx.amount);
        }
    }
    // Finished districts add their adjacency yields to the city.
    for (const CityDistrict& d : c->districts) {
        if (!d.complete) continue;
        Yields adj = districtAdjacency(c->owner, d.type, d.pos);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += adj[i];
    }
    // Every citizen adds a little culture and science (CULTURE/SCIENCE_PERCENTAGE_YIELD_PER_POP).
    raw[idx(YieldType::Culture)] += Fixed::ratio(rules_->globalInt("CULTURE_PERCENTAGE_YIELD_PER_POP"), 100) * c->population;
    raw[idx(YieldType::Science)] += Fixed::ratio(rules_->globalInt("SCIENCE_PERCENTAGE_YIELD_PER_POP"), 100) * c->population;
    int districtsDone = 0;
    for (const CityDistrict& d : c->districts) districtsDone += d.complete ? 1 : 0;
    for (size_t i = 0; i < kNumYields; ++i) {
        const YieldType y = static_cast<YieldType>(i);
        raw[i] += sumCityModifiers(state_, *rules_, *c, ModEffect::CityYield, y);
        raw[i] += sumCityModifiers(state_, *rules_, *c, ModEffect::CityYieldPerPop, y) * c->population;  // Tax Collector, Researcher
        raw[i] += sumCityModifiers(state_, *rules_, *c, ModEffect::CityYieldPerDistrict, y) * districtsDone;  // Bishop
    }
    if (const Unit* here = leaderOf(c->owner); here && here->pos == c->pos)
        raw[idx(YieldType::Production)] += Fixed::fromInt(unitEffectTotal(*here, UnitEffectKind::CityProduction));

    // Housing from water access, then buildings and modifiers.
    bool fresh = isRiverAdjacent(state_, c->pos), coastal = false;
    for (const Hex& n : state_.grid.within(c->pos, 1)) {
        const Plot& p = state_.plot(n);
        if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].freshWater) fresh = true;
        if (n != c->pos && rules_->terrains[static_cast<size_t>(p.terrain)].shallowWater) coastal = true;
    }
    const char* water = fresh ? "CITY_POPULATION_RIVER_LAKE" : coastal ? "CITY_POPULATION_COAST" : "CITY_POPULATION_NO_WATER";
    rep.housing += rules_->global(water);
    rep.housing += improvementHousing(*c);
    rep.housing += districtHousing(*c);
    if (const int mh = civAbility(c->owner).mountainCityHousing; mh > 0) {
        bool mountain = false;
        for (const Hex& n : state_.grid.within(c->pos, 1)) mountain = mountain || rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].relief == Relief::Mountain;
        if (mountain) rep.housing += Fixed::fromInt(mh);
    }
    rep.housing += sumCityModifiers(state_, *rules_, *c, ModEffect::CityHousing);

    // Amenities: bankruptcy costs 1 per 10 gold below zero (00-overview.md, Turn processing order).
    rep.amenities += static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityAmenities).toInt());
    rep.amenities += luxuryAmenities(*c);
    rep.amenities += districtAmenities(*c);
    if (c->powerDemand > 0 && c->powerSupply >= c->powerDemand) {
        for (TypeIndex bi : c->buildings) rep.amenities += rules_->buildings[static_cast<size_t>(bi)].poweredAmenities;
    }
    {
        const CivAbility& ab = civAbility(c->owner);
        for (TypeIndex b : c->buildings) {
            const BuildingType& bt = rules_->buildings[static_cast<size_t>(b)];
            rep.amenities += bt.wonder ? ab.amenityPerWonder : 0;
            for (const auto& [district, n] : ab.districtBuildingAmenities) rep.amenities += bt.districtType == district && district != kNone ? n : 0;
        }
        PlayerId holder = kNoPlayer;
        if (ab.governorAmenity > 0 && establishedGovernor(*c, &holder) && holder == c->owner) rep.amenities += ab.governorAmenity;
        const int religion = state_.players[static_cast<size_t>(c->owner)].religion;
        const int majority = cityMajorityReligion(*c);
        if (ab.foreignReligionAmenity > 0 && majority >= 0 && majority != religion) rep.amenities += ab.foreignReligionAmenity;
        if (c->capital && ab.capitalAmenityPerKills > 0)
            rep.amenities += std::min(ab.capitalAmenityMax, state_.players[static_cast<size_t>(c->owner)].killsThisEra / ab.capitalAmenityPerKills);
    }
    for (const Hex& h : state_.grid.within(c->pos, 3)) {
        const Plot& ip = state_.plot(h);
        if (ip.city == c->id && ip.improvement != kNone && ip.pillagedTurns == 0) rep.amenities += rules_->improvements[static_cast<size_t>(ip.improvement)].amenities;
    }
    // The leader's Builder-King promotions work in the city it stands in (leader doc §3).
    const Unit* leader = leaderOf(c->owner);
    if (leader && leader->pos == c->pos) rep.amenities += unitEffectTotal(*leader, UnitEffectKind::CityAmenities);
    // Citizen stances and reputation (leader doc §4, §8.1).
    if (state_.turn < c->benevolenceUntil) rep.amenities += rules_->globalInt("STANCE_BENEVOLENCE_AMENITIES");
    if (state_.turn >= c->fearUntil && state_.turn < c->fearAfterUntil) rep.amenities -= rules_->globalInt("STANCE_FEAR_AFTER_AMENITIES");
    if (beloved(c->owner)) rep.amenities += rules_->globalInt("REPUTATION_BELOVED_AMENITIES");
    if (feared(c->owner)) rep.amenities -= rules_->globalInt("REPUTATION_FEARED_AMENITIES");
    if (owner.gold < Fixed()) rep.amenities -= static_cast<int>((-owner.gold).ceil() + 9) / 10;
    rep.amenities -= warWearinessAmenities(c->owner);  // 08: War weariness
    const int perAmenity = std::max(1, rules_->globalInt("CITY_POP_PER_AMENITY"));
    rep.amenitiesNeeded = std::max(0, (c->population + perAmenity - 1) / perAmenity - 1);
    const int balance = rep.amenities - rep.amenitiesNeeded;
    rep.happiness = 0;
    for (size_t i = 0; i < rules_->happiness.size(); ++i) {
        if (balance >= rules_->happiness[i].minBalance) rep.happiness = static_cast<int>(i);
    }
    // Under Fear the city does not count as in Unrest or Revolt (§4).
    if (fearActive(*c)) {
        for (size_t i = 0; i < rules_->happiness.size(); ++i) {
            if (rules_->happiness[i].id == "HAPPINESS_UNHAPPY") rep.happiness = std::max(rep.happiness, static_cast<int>(i));
        }
    }
    const int moodYield = rules_->happiness.empty() ? 0 : rules_->happiness[static_cast<size_t>(rep.happiness)].yieldPercent;
    const LoyaltyLevel* loyal = loyaltyLevel(*c);
    const int loyaltyYield = loyal ? loyal->yieldPercent : 0;  // Wavering -25% ... Unrest -100% [R&F]

    for (size_t i = 0; i < kNumYields; ++i) {
        int pct = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityYieldPercent,
                                                          static_cast<YieldType>(i)).toInt());
        // A city short of power loses production, up to POWER_MAX_PRODUCTION_MODIFIER_PENALTY (09: Power).
        if (i == idx(YieldType::Production) && c->powerDemand > c->powerSupply)
            pct += rules_->globalInt("POWER_MAX_PRODUCTION_MODIFIER_PENALTY") * (c->powerDemand - c->powerSupply) / c->powerDemand;
        // Leader ability: while at peace with every major civ (Edo Peace).
        if (civAbility(c->owner).peaceYieldPercent[i] > Fixed()) {
            bool peace = true;
            for (const Player& o : state_.players) peace = peace && !(o.id != c->owner && isMajorCiv(o.id) && atWar(c->owner, o.id));
            if (peace) pct += static_cast<int>(civAbility(c->owner).peaceYieldPercent[i].toInt());
        }
        // Difficulty: AI cities at Immortal and Deity (00-overview: Difficulty levels).
        if (difficultyAi(c->owner)) {
            const bool sciCulFaith = i == idx(YieldType::Science) || i == idx(YieldType::Culture) || i == idx(YieldType::Faith);
            const bool prodGold = i == idx(YieldType::Production) || i == idx(YieldType::Gold);
            pct += sciCulFaith ? difficulty().aiYieldPercent : prodGold ? difficulty().aiProductionGoldPercent : 0;
        }
        if (i != idx(YieldType::Food)) pct += moodYield;
        pct += loyaltyYield;
        rep.yields[i] = raw[i] * std::max(0, pct) / 100;
    }
    // Civ unique buildings: gold per trade route from the city (Forum), food per mountain beside it (Qullqa).
    for (TypeIndex bi : c->buildings) {
        const BuildingType& b = rules_->buildings[static_cast<size_t>(bi)];
        if (b.goldPerTradeRoute > 0) {
            int routes = 0;
            for (const TradeRoute& tr : state_.tradeRoutes) routes += tr.origin == c->id ? 1 : 0;
            rep.yields[idx(YieldType::Gold)] += Fixed::fromInt(b.goldPerTradeRoute * routes);
        }
        if (b.foodPerAdjacentMountain > 0) {
            int mountains = 0;
            for (const Hex& n : state_.grid.within(c->pos, 1)) {
                mountains += rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].relief == Relief::Mountain ? 1 : 0;
            }
            rep.yields[idx(YieldType::Food)] += Fixed::fromInt(b.foodPerAdjacentMountain * std::min(2, mountains));
        }
    }
    // Governors: Magnus's Industrialist (+2 Production per power plant), Reyna's Renewable Subsidizer (+2 Gold from a Hydroelectric Dam).
    for (TypeIndex bi : c->buildings) {
        const BuildingType& b = rules_->buildings[static_cast<size_t>(bi)];
        if (b.burnsResource != kNone && cityGovernorHas(*c, "GOVERNOR_PROMOTION_INDUSTRIALIST")) rep.yields[idx(YieldType::Production)] += Fixed::fromInt(2);
        if (b.powerProvided > 0 && cityGovernorHas(*c, "GOVERNOR_PROMOTION_RENEWABLE_SUBSIDIZER")) rep.yields[idx(YieldType::Gold)] += Fixed::fromInt(2);
    }
    // Power [GS] (09: Power): fully powered buildings give their bonus.
    if (c->powerDemand > 0 && c->powerSupply >= c->powerDemand) {
        for (TypeIndex bi : c->buildings) {
            const BuildingType& b = rules_->buildings[static_cast<size_t>(bi)];
            if (b.requiredPower > 0) for (size_t i = 0; i < kNumYields; ++i) rep.yields[i] += b.poweredYields[i];
        }
    }
    // Civ abilities (leaders-and-art-style): culture per suzerainty and yields per governor title in
    // the capital; gold from an established governor.
    {
        const CivAbility& ab = civAbility(c->owner);
        if (c->capital && ab.culturePerSuzerainty > 0) {
            int suzerain = 0;
            for (const Player& cs : state_.players) suzerain += cs.cityState != kNone && cs.alive && suzerainOf(cs.id) == c->owner ? 1 : 0;
            rep.yields[idx(YieldType::Culture)] += Fixed::fromInt(ab.culturePerSuzerainty * suzerain);
        }
        if (c->capital) {
            const int titles = governorTitles(c->owner);
            for (size_t i = 0; i < kNumYields; ++i) rep.yields[i] += ab.capitalYieldsPerGovernorTitle[i] * titles + ab.capitalYields[i];
        }
        PlayerId holder = kNoPlayer;
        if (ab.governorGold > 0 && establishedGovernor(*c, &holder) && holder == c->owner)
            rep.yields[idx(YieldType::Gold)] += Fixed::fromInt(ab.governorGold);
    }
    // Leader abilities on the city's buildings and wonders (Carolingian Renaissance, Builder of Monuments).
    {
        const CivAbility& ab = civAbility(c->owner);
        for (TypeIndex bi : c->buildings) {
            const BuildingType& b = rules_->buildings[static_cast<size_t>(bi)];
            for (const auto& [district, y] : ab.districtBuildingYields) {
                if (b.districtType == district && district != kNone) {
                    for (size_t i = 0; i < kNumYields; ++i) rep.yields[i] += y[i];
                }
            }
            if (b.wonder) rep.yields[idx(YieldType::Culture)] += Fixed::fromInt(ab.wonderCulture);
        }
    }
    // A district project turns part of the city's production into a yield while it runs (03: Projects).
    if (!c->queue.empty() && c->queue.front().kind == ProductionKind::Project) {
        const ProjectType& pj = rules_->projects[static_cast<size_t>(c->queue.front().type)];
        if (pj.converts) rep.yields[idx(pj.conversionYield)] += rep.yields[idx(YieldType::Production)] * pj.conversionPercent / 100;
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
    } else if (item.kind == ProductionKind::District) {
        return districtCost(player, item.type);  // already scaled by game speed
    } else if (item.kind == ProductionKind::Project) {
        // GAME_PROGRESS: x (1 + param/100 x the larger share of the tech or civic tree completed).
        const ProjectType& pj = rules_->projects[static_cast<size_t>(item.type)];
        Fixed cost = Fixed::fromInt(pj.cost);
        if (pj.costProgression == DistrictCostProgression::GameProgress) {
            const Player& p = state_.players[static_cast<size_t>(player)];
            auto share = [](const TreeProgress& t) {
                const int64_t done = std::count(t.done.begin(), t.done.end(), static_cast<uint8_t>(1));
                return t.done.empty() ? Fixed() : Fixed::ratio(done, static_cast<int64_t>(t.done.size()));
            };
            cost = cost * (Fixed::fromInt(1) + std::max(share(p.techs), share(p.civics)) * pj.costProgressionParam / 100);
        }
        return std::max(1, static_cast<int>(cost.toInt()) * speedPercent(state_, *rules_) / 100);
    } else {
        base = rules_->buildings[static_cast<size_t>(item.type)].cost;
    }
    return std::max(1, base * speedPercent(state_, *rules_) / 100);
}

int Game::purchaseCost(PlayerId player, ProductionItem item) const {
    if (item.kind == ProductionKind::District || item.kind == ProductionKind::Project) return -1;  // built, never bought
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
    cost = cost * speedPercent(state_, *rules_) / 100;
    const int pct = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::PlotPurchaseCostPercent).toInt());
    return cost * std::max(0, pct) / 100;
}

bool Game::canProduce(const City& c, ProductionItem item, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    if (item.kind == ProductionKind::Unit) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->units.size()) return fail(CommandError::CannotBuild);
        const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
        // Ships need a city on the coast or a lake; aircraft a free air slot in the city or its Aerodrome.
        if ((u.domain == Domain::Air && !freeAirBase(c)) || (u.domain == Domain::Sea && !isCoastalCity(c)) || u.mustPurchase || !u.trainable || u.cost <= 0 ||
            !hasUnlocked(c.owner, u.unlock) || unitObsolete(c.owner, item.type))
            return fail(CommandError::CannotBuild);
        if (u.needsDistrict != kNone && !c.district(u.needsDistrict, true)) return fail(CommandError::CannotBuild);
        // Civ uniques: only their civ trains them, and for it they replace their base unit.
        const TypeIndex civ = state_.players[static_cast<size_t>(c.owner)].civ;
        if (u.uniqueTo != kNone && u.uniqueTo != civ) return fail(CommandError::CannotBuild);
        if (rules_->uniqueUnitFor(civ, item.type) != kNone) return fail(CommandError::CannotBuild);
        if (u.agent && !u.spy && agentsOf(c.owner) >= agentCapacity(c.owner)) return fail(CommandError::CannotBuild);
        if (u.spy && spiesOf(c.owner) >= spyCapacity(c.owner)) return fail(CommandError::CannotBuild);
        if (!u.needsBuilding.empty() &&
            std::none_of(u.needsBuilding.begin(), u.needsBuilding.end(), [&](TypeIndex b) { return cityHasBuilding(c, *rules_, b); }))
            return fail(CommandError::CannotBuild);
    } else if (item.kind == ProductionKind::Building) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->buildings.size()) return fail(CommandError::CannotBuild);
        const BuildingType& b = rules_->buildings[static_cast<size_t>(item.type)];
        if (b.granted || b.faithOnly || c.has(item.type) || !hasUnlocked(c.owner, b.unlock)) return fail(CommandError::CannotBuild);
        // Civ uniques: only their civ builds them, and for it they replace their base building.
        const TypeIndex civ = state_.players[static_cast<size_t>(c.owner)].civ;
        if (b.uniqueTo != kNone && b.uniqueTo != civ) return fail(CommandError::CannotBuild);
        for (const BuildingType& u : rules_->buildings) {
            if (u.uniqueTo == civ && civ != kNone && u.replaces == item.type) return fail(CommandError::CannotBuild);
        }
        if (b.replaces != kNone && c.has(b.replaces)) return fail(CommandError::CannotBuild);
        if (b.wonder) {
            // Once in the world, on a plot of its own (03: Wonders).
            if (wonderBuilt(item.type)) return fail(CommandError::CannotBuild);
            const bool sited = std::any_of(c.wonders.begin(), c.wonders.end(), [&](const CityWonder& w) { return w.building == item.type; });
            if (!sited && wonderPlots(c.id, item.type).empty()) return fail(CommandError::CannotBuild);
            if (why) *why = CommandError::Ok;
            return true;
        }
        // Buildings outside the City Center need their finished district.
        if (b.district != "DISTRICT_CITY_CENTER" && (b.districtType == kNone || !c.district(b.districtType, true)))
            return fail(CommandError::CannotBuild);
        for (TypeIndex req : b.prereqs) {
            if (!cityHasBuilding(c, *rules_, req)) return fail(CommandError::CannotBuild);
        }
        if (b.needsRiver && !isRiverAdjacent(state_, c.pos)) return fail(CommandError::CannotBuild);
    } else if (item.kind == ProductionKind::District) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->districts.size()) return fail(CommandError::CannotBuild);
        const DistrictType& d = rules_->districts[static_cast<size_t>(item.type)];
        if (d.cost <= 0 || !hasUnlocked(c.owner, d.unlock)) return fail(CommandError::CannotBuild);
        const CityDistrict* placed = c.district(item.type, false);
        if (placed && placed->complete) return fail(CommandError::CannotBuild);
        if (!placed && d.needsPopulation) {
            // A new district needs room under the population limit.
            int used = 0;
            for (const CityDistrict& cd : c.districts) used += rules_->districts[static_cast<size_t>(cd.type)].needsPopulation ? 1 : 0;
            if (used >= districtLimit(c)) return fail(CommandError::CannotBuild);
        }
    } else if (item.kind == ProductionKind::Project) {
        if (item.type < 0 || static_cast<size_t>(item.type) >= rules_->projects.size()) return fail(CommandError::CannotBuild);
        const ProjectType& pj = rules_->projects[static_cast<size_t>(item.type)];
        const Player& p = state_.players[static_cast<size_t>(c.owner)];
        const int done = static_cast<size_t>(item.type) < p.projectsDone.size() ? p.projectsDone[static_cast<size_t>(item.type)] : 0;
        if (!pj.modelled || !hasUnlocked(c.owner, pj.unlock) || (pj.maxPerPlayer > 0 && done >= pj.maxPerPlayer)) return fail(CommandError::CannotBuild);
        if (!pj.districtId.empty() && (pj.district == kNone || !c.district(pj.district, true))) return fail(CommandError::CannotBuild);
        if (pj.prerequisite != kNone && (static_cast<size_t>(pj.prerequisite) >= p.projectsDone.size() || p.projectsDone[static_cast<size_t>(pj.prerequisite)] == 0))
            return fail(CommandError::CannotBuild);
        if (pj.resource != kNone && p.stockpile[static_cast<size_t>(pj.resource)] < pj.resourceAmount) return fail(CommandError::NotEnoughResources);
        // Repair Outer Defenses: only with walls that are down.
        for (const ProjectEffect& e : pj.effects) {
            if (e.kind == ProjectEffectKind::RepairWalls && c.wallHp >= cityMaxWallHp(c)) return fail(CommandError::CannotBuild);
        }
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
        if (canProduce(*c, it) && hasStrategicFor(c->owner, it.type)) out.push_back(it);
    }
    for (size_t i = 0; i < rules_->buildings.size(); ++i) {
        ProductionItem it{ProductionKind::Building, static_cast<TypeIndex>(i)};
        if (canProduce(*c, it) && std::find(c->queue.begin(), c->queue.end(), it) == c->queue.end()) out.push_back(it);
    }
    // Districts already placed here, or with a plot to go on.
    for (size_t i = 0; i < rules_->districts.size(); ++i) {
        ProductionItem it{ProductionKind::District, static_cast<TypeIndex>(i)};
        if (!canProduce(*c, it) || std::find(c->queue.begin(), c->queue.end(), it) != c->queue.end()) continue;
        if (c->district(it.type, false) || !districtPlots(id, it.type).empty()) out.push_back(it);
    }
    for (size_t i = 0; i < rules_->projects.size(); ++i) {
        ProductionItem it{ProductionKind::Project, static_cast<TypeIndex>(i)};
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
    const Player& p = state_.players[static_cast<size_t>(player)];
    Fixed net;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        if (p.anarchyTurns == 0) net += cityReport(c.id).yields[idx(YieldType::Gold)];  // anarchy: no gold
        for (TypeIndex b : c.buildings) net -= Fixed::fromInt(rules_->buildings[static_cast<size_t>(b)].maintenance);
        for (const CityDistrict& d : c.districts) {
            if (d.complete) net -= Fixed::fromInt(rules_->districts[static_cast<size_t>(d.type)].maintenance);
        }
    }
    const Fixed discount = sumPlayerModifiers(state_, *rules_, p, ModEffect::UnitMaintenanceDiscount);
    for (const Unit& u : state_.units) {
        if (u.owner != player) continue;
        const Fixed m = Fixed::fromInt(rules_->units[static_cast<size_t>(u.type)].maintenance) - discount;
        if (m > Fixed()) net -= m;
    }
    net -= Fixed::fromInt(leaderUpkeep(player));  // the leader's mount (leader doc §8.8)
    for (size_t i = 0; i < p.wmds.size() && i < rules_->wmds.size(); ++i) net -= Fixed::fromInt(p.wmds[i] * rules_->wmds[i].maintenance);
    if (p.anarchyTurns == 0) net += founderYields(player)[idx(YieldType::Gold)];  // Tithe and the like (06)
    // Gold promised by deals (08: Trade Deal).
    for (const Agreement& a : state_.agreements) {
        if (a.kind != DealItemKind::GoldPerTurn || a.until < state_.turn) continue;
        if (a.from == player) net -= Fixed::fromInt(a.amount);
        if (a.to == player) net += Fixed::fromInt(a.amount);
    }
    return net;
}

std::optional<Hex> Game::unitSpawnPlot(const City& c, TypeIndex unitType) const {
    const UnitType& ut = rules_->units[static_cast<size_t>(unitType)];
    if (ut.domain == Domain::Air) return freeAirBase(c);
    const UnitLayer layer = ut.layer;
    for (const Hex& h : state_.grid.within(c.pos, 1)) {  // the center comes first
        if (ut.domain == Domain::Sea) {
            // A new ship waits in the port, or on the water next to it.
            const TerrainType& t = rules_->terrains[static_cast<size_t>(state_.plot(h).terrain)];
            if (h != c.pos && (!t.water || t.impassable || (t.id == "TERRAIN_OCEAN" && !canEnterOcean(c.owner)))) continue;
        } else if (!isLandPassable(state_, *rules_, h)) {
            continue;
        }
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
            if (c.arg < 0 || c.arg > 3 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (!canProduce(*city, item, &why)) return why;
            if (item.kind == ProductionKind::District && !city->district(item.type, false) &&
                !canPlaceDistrict(*city, item.type, c.target, &why))
                return why;
            if (item.kind == ProductionKind::Unit && !hasStrategicFor(c.player, item.type)) return CommandError::NotEnoughResources;
            if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].wonder &&
                std::none_of(city->wonders.begin(), city->wonders.end(), [&](const CityWonder& w) { return w.building == item.type; }) &&
                !canPlaceWonder(*city, item.type, c.target))
                return CommandError::BadTarget;
            return CommandError::Ok;
        case CommandType::QueueProduction:
            if (c.arg < 0 || c.arg > 3 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (!canProduce(*city, item, &why)) return why;
            if (item.kind == ProductionKind::District && !city->district(item.type, false) &&
                !canPlaceDistrict(*city, item.type, c.target, &why))
                return why;
            if (item.kind == ProductionKind::Unit && !hasStrategicFor(c.player, item.type)) return CommandError::NotEnoughResources;
            if (item.kind != ProductionKind::Unit &&
                std::find(city->queue.begin(), city->queue.end(), item) != city->queue.end())
                return CommandError::CannotBuild;
            if (static_cast<int>(city->queue.size()) >= rules_->globalInt("CITY_PRODUCTION_QUEUE_MAX"))
                return CommandError::QueueFull;
            if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].wonder &&
                std::none_of(city->wonders.begin(), city->wonders.end(), [&](const CityWonder& w) { return w.building == item.type; }) &&
                !canPlaceWonder(*city, item.type, c.target))
                return CommandError::BadTarget;
            return CommandError::Ok;
        case CommandType::Purchase: {
            if (c.arg < 0 || c.arg > 3 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (c.target.x == 1) {
                // Religious units and worship buildings, bought with Faith (06).
                const int faith = faithPurchaseCost(c.player, *city, item);
                if (faith < 0) return CommandError::CannotBuild;
                if (item.kind == ProductionKind::Unit && !unitSpawnPlot(*city, item.type)) return CommandError::CannotBuild;
                if (state_.players[static_cast<size_t>(c.player)].faith < Fixed::fromInt(faith)) return CommandError::NotEnoughFaith;
                return CommandError::Ok;
            }
            if (!canProduce(*city, item, &why)) return why;
            int cost = purchaseCost(c.player, item);
            if (cost < 0) return CommandError::CannotBuild;
            if (item.kind == ProductionKind::Unit) {
                const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
                if (city->population < u.minPopulation || !unitSpawnPlot(*city, item.type)) return CommandError::CannotBuild;
                if (!hasStrategicFor(c.player, item.type)) return CommandError::NotEnoughResources;
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
        case CommandType::QueueProduction:
            if (item.kind == ProductionKind::District && !city.district(item.type, false))
                placeDistrict(city, item.type, c.target);
            if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].wonder &&
                std::none_of(city.wonders.begin(), city.wonders.end(), [&](const CityWonder& w) { return w.building == item.type; })) {
                // The wonder's plot is reserved; its improvement and removable feature go.
                Plot& wp = state_.plot(c.target);
                wp.improvement = kNone;
                if (wp.feature != kNone && rules_->features[static_cast<size_t>(wp.feature)].removable) wp.feature = kNone;
                city.wonders.push_back({item.type, c.target});
                assignCitizens(city);
            }
            if (c.type == CommandType::SetProduction) city.queue.assign(1, item);
            else city.queue.push_back(item);
            break;
        case CommandType::Purchase:
            if (c.target.x == 1) {
                p.faith -= Fixed::fromInt(faithPurchaseCost(c.player, city, item));
                if (item.kind == ProductionKind::Building) {
                    city.buildings.push_back(item.type);
                    std::sort(city.buildings.begin(), city.buildings.end());
                } else {
                    const int religion = cityMajorityReligion(city);
                    if (p.unitsTrained.size() < rules_->units.size()) p.unitsTrained.resize(rules_->units.size(), 0);
                    ++p.unitsTrained[static_cast<size_t>(item.type)];
                    Unit& u = spawnUnit(item.type, c.player, *unitSpawnPlot(city, item.type));
                    u.religion = static_cast<int16_t>(religion);
                    u.charges = rules_->units[static_cast<size_t>(item.type)].spreadCharges;
                }
                break;
            }
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
    if (item.kind == ProductionKind::Building && cityGovernorHas(city, "GOVERNOR_PROMOTION_CITADEL_OF_GOD"))
        state_.players[static_cast<size_t>(city.owner)].faith += Fixed::fromInt(productionCost(city.owner, item) / 4);  // Moksha
    if (item.kind == ProductionKind::Unit) {
        const UnitType& u = rules_->units[static_cast<size_t>(item.type)];
        if (city.population < u.minPopulation) return false;
        // Training waits while the strategic resource is short.
        if (!hasStrategicFor(city.owner, item.type)) return false;
        if (u.agent) {
            // Assassins become off-map agents, within the capacity (leader doc §6).
            if (u.spy ? spiesOf(city.owner) >= spyCapacity(city.owner) : agentsOf(city.owner) >= agentCapacity(city.owner)) return false;
            Agent a;
            a.id = state_.nextAgentId++;
            a.owner = city.owner;
            a.spy = u.spy;
            state_.agents.push_back(a);
            Player& owner = state_.players[static_cast<size_t>(city.owner)];
            if (owner.unitsTrained.size() < rules_->units.size()) owner.unitsTrained.resize(rules_->units.size(), 0);
            ++owner.unitsTrained[static_cast<size_t>(item.type)];
            return true;
        }
        auto spot = unitSpawnPlot(city, item.type);
        if (!spot) return false;
        Player& p = state_.players[static_cast<size_t>(city.owner)];
        if (u.strategicResource != kNone) p.stockpile[static_cast<size_t>(u.strategicResource)] -= u.strategicCost;
        if (p.unitsTrained.size() < rules_->units.size()) p.unitsTrained.resize(rules_->units.size(), 0);
        ++p.unitsTrained[static_cast<size_t>(item.type)];
        // Provision: settlers trained under Magnus cost no population (08: Governors).
        if (sumCityModifiers(state_, *rules_, city, ModEffect::SettlerNoPopCost) <= Fixed()) city.population -= u.popCost;
        Unit& made = spawnUnit(item.type, city.owner, *spot);
        for (TypeIndex bi : city.buildings) {
            const int pct = rules_->buildings[static_cast<size_t>(bi)].trainedXpPercent;
            if (pct > 0 && !u.promotionClass.empty()) made.xp = std::min(xpForNextLevel(made), made.xp + xpForNextLevel(made) * pct / 100);
        }
        if (!u.promotionClass.empty() && cityGovernorHas(city, "GOVERNOR_PROMOTION_EMBRASURE")) made.xp = std::max(made.xp, xpForNextLevel(made));  // Victor's Embrasure
        if (made.charges > 0) made.charges += static_cast<int>(sumCityModifiers(state_, *rules_, city, ModEffect::BuilderExtraCharges).toInt()) +
                                              (u.buildCharges > 0 && !u.foundCity ? civAbility(city.owner).extraBuilderCharges : 0);
        assignCitizens(city);
        refreshVisibility(city.owner);
    } else if (item.kind == ProductionKind::District) {
        for (CityDistrict& d : city.districts) {
            if (d.type == item.type) d.complete = true;
        }
    } else if (item.kind == ProductionKind::Project) {
        completeProject(city, item.type);
    } else {
        auto it = std::lower_bound(city.buildings.begin(), city.buildings.end(), item.type);
        if (it == city.buildings.end() || *it != item.type) {
            city.buildings.insert(it, item.type);
            city.wallHp += rules_->buildings[static_cast<size_t>(item.type)].outerDefenseHp;  // new walls stand at full HP
            state_.players[static_cast<size_t>(city.owner)].envoyTokens += rules_->buildings[static_cast<size_t>(item.type)].envoysOnBuild;
            if (rules_->buildings[static_cast<size_t>(item.type)].wonder) completeWonder(city, item.type);
        }
    }
    return true;
}

void Game::completeProject(City& city, TypeIndex project) {
    const ProjectType& pj = rules_->projects[static_cast<size_t>(project)];
    Player& p = state_.players[static_cast<size_t>(city.owner)];
    if (p.projectsDone.size() < rules_->projects.size()) p.projectsDone.resize(rules_->projects.size(), 0);
    ++p.projectsDone[static_cast<size_t>(project)];
    if (pj.resource != kNone) p.stockpile[static_cast<size_t>(pj.resource)] = std::max(0, p.stockpile[static_cast<size_t>(pj.resource)] - pj.resourceAmount);
    for (const auto& [cls, points] : pj.greatPersonPoints) {
        if (static_cast<size_t>(cls) < p.greatPersonPoints.size()) p.greatPersonPoints[static_cast<size_t>(cls)] += points;
    }
    for (const ProjectEffect& e : pj.effects) {
        switch (e.kind) {
            case ProjectEffectKind::RepairWalls: city.wallHp = cityMaxWallHp(city); break;
            case ProjectEffectKind::Loyalty: city.loyalty = std::min(rules_->globalInt("LOYALTY_MAXIMUM"), city.loyalty + e.amount); break;
            case ProjectEffectKind::Favor: p.favor += e.amount; break;
            case ProjectEffectKind::RemoveCo2: {
                const int64_t removed = std::min<int64_t>(state_.co2, e.amount);
                state_.co2 -= removed;
                p.co2 = std::max<int64_t>(0, p.co2 - removed);
                break;
            }
            case ProjectEffectKind::RevealMap:
                for (uint8_t& v : p.visibility) v = std::max(v, static_cast<uint8_t>(Visibility::Revealed));
                break;
            case ProjectEffectKind::CultureFromScience: p.civics.overflow += sciencePerTurn(city.owner) * e.amount; break;
            case ProjectEffectKind::ExpeditionSpeed: break;  // the space race (Science victory) reads projectsDone
            case ProjectEffectKind::Wmd:
                if (p.wmds.size() < rules_->wmds.size()) p.wmds.resize(rules_->wmds.size(), 0);
                if (e.weapon != kNone) p.wmds[static_cast<size_t>(e.weapon)] += e.amount;
                break;
        }
    }
}

int Game::expeditionSpeed(PlayerId player) const {
    // The expedition itself gives 1 light-year a turn; each laser station adds its own (repeatable).
    const Player& p = state_.players[static_cast<size_t>(player)];
    int speed = 0;
    bool launched = false;
    for (size_t i = 0; i < rules_->projects.size() && i < p.projectsDone.size(); ++i) {
        const ProjectType& pj = rules_->projects[i];
        for (const ProjectEffect& e : pj.effects) {
            if (e.kind != ProjectEffectKind::ExpeditionSpeed || p.projectsDone[i] == 0) continue;
            speed += e.amount * p.projectsDone[i];
            launched |= pj.maxPerPlayer == 1;  // the expedition itself (the stations are repeatable)
        }
    }
    return launched ? speed : 0;
}

void Game::processSpaceRace() {
    for (Player& p : state_.players) {
        if (!p.alive || p.barbarian) continue;
        p.lightYears += expeditionSpeed(p.id);
    }
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

    // Steps 2-5 of the turn order: yields, gold and maintenance, research.
    std::vector<CityReport> reports;
    for (CityId id : ids) reports.push_back(cityReport(id));
    player.gold += goldPerTurn(pid);
    Fixed science, culture;
    if (player.anarchyTurns == 0) {  // anarchy: no gold, science, culture or faith
        for (const CityReport& r : reports) {
            science += r.yields[idx(YieldType::Science)];
            culture += r.yields[idx(YieldType::Culture)];
            player.faith += r.yields[idx(YieldType::Faith)];
        }
        // A founder's beliefs pay for its religion's spread (06: Founder beliefs).
        const Yields fy = founderYields(pid);
        science += fy[idx(YieldType::Science)];
        culture += fy[idx(YieldType::Culture)];
        player.faith += fy[idx(YieldType::Faith)];
        science += allianceShare(pid, YieldType::Science);
        culture += allianceShare(pid, YieldType::Culture);
    }
    player.lifetimeCulture += culture;  // domestic tourists (07: Tourism)
    processResearch(pid, science, culture);
    accumulateStrategics(pid);
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
            // Migration Treaty (World Congress) on its target: +20% (A) or -20% (B) growth.
            if (const PassedResolution* mt = passed(ResolutionKind::MigrationTreaty); mt && mt->target == city.owner) pct += mt->option == 0 ? 20 : -20;
            surplus = surplus * std::max(0, pct) / 100;
            if (const LoyaltyLevel* loyal = loyaltyLevel(city)) surplus = surplus * loyal->growthPercent / 100;
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
            // A civ's first city of each size tier (09: historic moments).
            static const std::pair<int, const char*> tiers[] = {{10, "BUSTLING"}, {15, "LARGE"}, {20, "ENORMOUS"}, {25, "GIGANTIC"}};
            for (const auto& [size, name] : tiers) {
                if (city.population != size) continue;
                const std::string world = std::string("MOMENT_WORLD_S_FIRST_") + name + "_CITY";
                const std::string own = std::string("MOMENT_FIRST_") + name + "_CITY";
                awardFirst(pid, world.c_str(), own.c_str());
            }
        } else if (city.food < Fixed()) {
            city.food = Fixed();
            if (city.population > 1) {
                --city.population;
                assignCitizens(city);
            }
        }

        // Production (02-cities.md, Production).
        Fixed prod = rep.yields[idx(YieldType::Production)];
        while (!city.queue.empty() && !canProduce(city, city.queue.front())) city.queue.erase(city.queue.begin());
        if (city.queue.empty()) {
            city.overflow += prod;
        } else {
            const ProductionItem item = city.queue.front();
            if (item.kind == ProductionKind::Unit) {
                // Policies such as Agoge speed production toward some units; a leader's domain (Sea Dogs).
                const int pct = 100 + static_cast<int>(sumUnitProductionPercent(state_, *rules_, city, item.type).toInt()) +
                                civAbility(pid).domainProductionPercent[static_cast<size_t>(rules_->units[static_cast<size_t>(item.type)].domain)];
                prod = prod * std::max(0, pct) / 100;
            } else if (item.kind == ProductionKind::Building && !rules_->buildings[static_cast<size_t>(item.type)].wonder) {
                // Leader abilities: City Center buildings (City of Marble), walls (Standardization).
                const BuildingType& b = rules_->buildings[static_cast<size_t>(item.type)];
                const CivAbility& ab = civAbility(pid);
                int pct = 100;
                if (b.district == "DISTRICT_CITY_CENTER") pct += ab.cityCenterBuildingProductionPercent;
                if (b.outerDefenseHp > 0) pct += ab.wallProductionPercent;
                prod = prod * pct / 100;
            } else if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].wonder) {
                // Civ ability: faster wonders of some eras (France).
                const CivAbility& ab = civAbility(pid);
                const Unlock& u = rules_->buildings[static_cast<size_t>(item.type)].unlock;
                const int era = u.none() ? 0 : (u.civic ? rules_->civics : rules_->techs)[static_cast<size_t>(u.index)].era;
                if (ab.wonderProductionPercent > 0 && era >= ab.wonderEraMin && era <= ab.wonderEraMax) prod = prod * (100 + ab.wonderProductionPercent) / 100;
            } else if (item.kind == ProductionKind::District) {
                // Zoning Commissioner (08: Governors); Urban Development Treaty A (World Congress).
                int pct = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, city, ModEffect::CityDistrictProductionPercent).toInt());
                // Civ ability: districts go faster in cities next to a mountain (Inca).
                if (civAbility(pid).mountainDistrictProductionPercent > 0) {
                    bool mountain = false;
                    for (const Hex& n : state_.grid.within(city.pos, 1)) mountain = mountain || rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].relief == Relief::Mountain;
                    if (mountain) pct += civAbility(pid).mountainDistrictProductionPercent;
                }
                if (const PassedResolution* ud = passed(ResolutionKind::UrbanDevelopment); ud && ud->option == 0 && ud->target == item.type) pct += 100;
                prod = prod * std::max(0, pct) / 100;
            } else if (item.kind == ProductionKind::Project) {
                // Governors: Victor's Arms Race Proponent (nuclear projects), Pingala's Space Initiative (space race).
                const ProjectType& pj = rules_->projects[static_cast<size_t>(item.type)];
                const bool nuclear = pj.id == "PROJECT_MANHATTAN_PROJECT" || pj.id == "PROJECT_OPERATION_IVY" ||
                                     std::any_of(pj.effects.begin(), pj.effects.end(), [](const ProjectEffect& e) { return e.kind == ProjectEffectKind::Wmd; });
                int pct = 100;
                if (nuclear && cityGovernorHas(city, "GOVERNOR_PROMOTION_ARMS_RACE_PROPONENT")) pct += 30;
                if (pj.spaceRace && cityGovernorHas(city, "GOVERNOR_PROMOTION_SPACE_INITIATIVE")) pct += 30;
                prod = prod * pct / 100;
            }
            prod += Fixed::fromInt(envoyProduction(city, item));  // Industrial and Militaristic city-states (08)
            prod += city.overflow;
            city.overflow = Fixed();
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
        // Land Acquisition: a faster border expansion rate (08: Governors).
        const int faster = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, c3, ModEffect::CityBorderGrowthPercent).toInt());
        const Fixed cost = Fixed::fromInt(borderGrowthCost(c3.plotsByCulture)) * 100 / std::max(1, faster);
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
