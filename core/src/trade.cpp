// Trade routes and roads (07-economy-trade-great-people.md, Trade routes; 01-map-and-terrain.md,
// Routes). A Trader in one of the player's cities starts a route to a city in range; the route
// pays its origin each turn by the districts at the destination, lays roads along the way and
// ends after its length (the Trader comes home), or when war or a raider cuts it.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {

size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }

// The city a Trader starts from: the one it stands in, or (when another civilian holds the
// city plot) the one whose land it stands on next to the center. Sovereign convenience.
const City* originOf(const GameState& s, const Unit& u) {
    if (const City* c = s.cityAt(u.pos); c && c->owner == u.owner) return c;
    const CityId id = s.plot(u.pos).city;
    const City* c = id != kNoCity ? s.city(id) : nullptr;
    return c && c->owner == u.owner && s.grid.distance(c->pos, u.pos) <= 1 ? c : nullptr;
}

}  // namespace

int Game::tradeRouteCapacity(PlayerId player) const {
    const Player& p = state_.players[at(player)];
    int cap = 0;
    for (size_t i = 0; i < rules_->civics.size(); ++i) {
        if (rules_->civics[i].tradeCapacity && i < p.civics.done.size() && p.civics.done[i]) ++cap;
    }
    for (size_t i = 0; i < rules_->techs.size(); ++i) {
        if (rules_->techs[i].tradeCapacity && i < p.techs.done.size() && p.techs.done[i]) ++cap;
    }
    // Markets (or Lighthouses in cities without one): at most +1 per city in this ruleset.
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        int city = 0;
        for (TypeIndex b : c.buildings) {
            const BuildingType& bt = rules_->buildings[at(b)];
            if (bt.tradeCapacity > 0 && !buildingIdle(c, *rules_, b) && (bt.tradeCapacityUnless == kNone || !cityHasBuilding(c, *rules_, bt.tradeCapacityUnless))) city += bt.tradeCapacity;
        }
        cap += city;
    }
    cap += greatPersonEffectTotal(player, GreatPersonEffectKind::TradeRoutes);  // Zheng He, Marco Polo... (07)
    // Trade Policy (World Congress): its target gains a route (A) or loses them all (B).
    if (const PassedResolution* tp = passed(ResolutionKind::TradePolicy); tp && tp->target == player)
        cap = tp->option == 0 ? cap + 1 : 0;
    return cap;
}

const City* Game::tradeOrigin(UnitId trader) const {
    const Unit* u = state_.unit(trader);
    return u ? originOf(state_, *u) : nullptr;
}

int Game::tradeRoutesOf(PlayerId player) const {
    return static_cast<int>(std::count_if(state_.tradeRoutes.begin(), state_.tradeRoutes.end(), [&](const TradeRoute& r) { return r.owner == player; }));
}

Yields Game::tradeRouteDestinationYields(const City& origin, const City& destination) const {
    Yields out{};
    if (origin.owner == destination.owner) return out;
    // Alliances at level 1 (08): the destination's share of an ally's route.
    static const std::pair<AllianceType, YieldType> kShare[] = {{AllianceType::Research, YieldType::Science}, {AllianceType::Economic, YieldType::Gold},
                                                                {AllianceType::Cultural, YieldType::Culture}, {AllianceType::Religious, YieldType::Faith}};
    const AllianceType type = alliance(origin.owner, destination.owner);
    for (const auto& [a, y] : kShare) {
        if (type == a) out[static_cast<size_t>(y)] += Fixed::fromInt(a == AllianceType::Economic ? 2 : 1);
    }
    // Policy cards (04): Wisselbanken, Democratic Legacy.
    const bool cs = isCityState(destination.owner);
    const Yields extra = tradeRouteModifierYields(state_, *rules_, state_.players[at(origin.owner)], false, type != AllianceType::None, cs,
                                                  cs && isSuzerain(origin.owner, destination.owner), true);
    for (size_t i = 0; i < kNumYields; ++i) out[i] += extra[i];
    return out;
}

Yields Game::tradeRouteYields(const City& origin, const City& destination, const std::vector<int32_t>* way) const {
    Yields out{};
    const bool domestic = origin.owner == destination.owner;
    auto add = [&](TypeIndex district) {
        const DistrictType& d = rules_->districts[at(district)];
        const Yields& y = domestic ? d.tradeDomestic : d.tradeInternational;
        for (size_t i = 0; i < kNumYields; ++i) out[i] += y[i];
    };
    const TypeIndex center = rules_->district("DISTRICT_CITY_CENTER");
    if (center != kNone) add(center);
    for (const CityDistrict& d : destination.districts) {
        if (d.complete) add(d.type);
    }
    // Leader abilities: international routes (Golden Pilgrimage), and to another landmass (Sea Dogs).
    if (!domestic) {
        const CivAbility& ab = civAbility(origin.owner);
        for (size_t i = 0; i < kNumYields; ++i) out[i] += ab.internationalRouteYields[i];
        if (state_.plot(origin.pos).continent != state_.plot(destination.pos).continent) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(ab.intercontinentalRouteGold);
        // Reform the Coinage (Golden Age, 09): +3 Gold per specialty district at the destination.
        if (goldenDedication(origin.owner, "DEDICATION_REFORM_THE_COINAGE")) {
            for (const CityDistrict& d : destination.districts) {
                if (d.complete && rules_->districts[static_cast<size_t>(d.type)].id != "DISTRICT_CITY_CENTER") out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(3);
            }
        }
    }
    // Alliances [R&F], level 1: routes to an ally of the type carry its yield (08: alliance levels).
    if (!domestic) {
        static const std::pair<AllianceType, YieldType> kRouteYield[] = {{AllianceType::Research, YieldType::Science}, {AllianceType::Economic, YieldType::Gold},
                                                                         {AllianceType::Cultural, YieldType::Culture}, {AllianceType::Religious, YieldType::Faith}};
        const AllianceType type = alliance(origin.owner, destination.owner);
        for (const auto& [t, y] : kRouteYield) {
            if (type == t) out[static_cast<size_t>(y)] += Fixed::fromInt(t == AllianceType::Economic ? 4 : 2);
        }
    }
    // Natural wonders (01): +4 Gold on international routes from the city owning Païtiti.
    if (!domestic) {
        const TypeIndex paititi = rules_->feature("FEATURE_PAITITI");
        bool owns = false;
        if (paititi != kNone)
            state_.grid.forEachWithin(origin.pos, 3, [&](Hex h) { owns = owns || (state_.plot(h).feature == paititi && state_.plot(h).city == origin.id); });
        if (owns) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(4);
    }
    // Policy cards (04): Caravansaries, Market Economy, Raj... by the kind of route.
    {
        const bool cs = isCityState(destination.owner);
        const bool ally = !domestic && alliance(origin.owner, destination.owner) != AllianceType::None;
        const Yields extra = tradeRouteModifierYields(state_, *rules_, state_.players[at(origin.owner)], domestic, ally, cs,
                                                      cs && isSuzerain(origin.owner, destination.owner));
        for (size_t i = 0; i < kNumYields; ++i) out[i] += extra[i];
    }
    // Market Economy (04): international routes +1 Gold per luxury and per strategic resource at the destination.
    if (!domestic && policyIs(origin.owner, "POLICY_MARKET_ECONOMY")) {
        int goods = 0;
        state_.grid.forEachWithin(destination.pos, 3, [&](Hex h) {
            const Plot& p = state_.plot(h);
            if (p.city != destination.id || p.resource == kNone || !resourceImproved(h)) return;
            const ResourceClass rc = rules_->resources[at(p.resource)].cls;
            goods += rc == ResourceClass::Luxury || rc == ResourceClass::Strategic ? 1 : 0;
        });
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(goods);
    }
    // City-state suzerains (08): Venice (+1 Gold per luxury at an international destination), Hunza (+0.2 Gold
    // per plot of the way), Kumasi (+2 Culture and +1 Gold per district on routes to city-states).
    if (!domestic && suzerainBonus(origin.owner, Cs::Venice)) {
        int luxuries = 0;
        state_.grid.forEachWithin(destination.pos, 3, [&](Hex h) {
            const Plot& p = state_.plot(h);
            luxuries += p.city == destination.id && p.resource != kNone && rules_->resources[at(p.resource)].cls == ResourceClass::Luxury && resourceImproved(h) ? 1 : 0;
        });
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(luxuries);
    }
    // Samarkand (08): international routes +1 Gold per Trading Dome of the origin.
    if (!domestic && suzerainBonus(origin.owner, Cs::Samarkand)) {
        const TypeIndex dome = rules_->improvement("IMPROVEMENT_TRADING_DOME");
        int domes = 0;
        state_.grid.forEachWithin(origin.pos, 3, [&](Hex h) {
            const Plot& p = state_.plot(h);
            domes += dome != kNone && p.city == origin.id && p.improvement == dome && p.pillagedTurns == 0 ? 1 : 0;
        });
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(domes);
    }
    // Chinguetti (08): +1 Faith per follower of the player's founded (or majority) religion in the origin city.
    if (suzerainBonus(origin.owner, Cs::Chinguetti))
        out[static_cast<size_t>(YieldType::Faith)] += Fixed::fromInt(cityFollowers(origin, civReligion(origin.owner)));
    if (suzerainBonus(origin.owner, Cs::Hunza))
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::ratio(state_.grid.distance(origin.pos, destination.pos), 5);
    if (isCityState(destination.owner) && suzerainBonus(origin.owner, Cs::Kumasi)) {
        int districts = 0;
        for (const CityDistrict& d : destination.districts) districts += d.complete ? 1 : 0;
        out[static_cast<size_t>(YieldType::Culture)] += Fixed::fromInt(2 * districts);
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(districts);
    }
    // Wonders (03): Great Zimbabwe (+2 Gold per bonus resource of the origin), Torre de Belém (+2 Gold per luxury at an
    // international destination), University of Sankore (+1 Science and +1 Gold on other civs' routes to it).
    auto goods = [&](const City& city, ResourceClass cls) {
        int n = 0;
        state_.grid.forEachWithin(city.pos, 3, [&](Hex h) {
            const Plot& p = state_.plot(h);
            n += p.city == city.id && p.resource != kNone && rules_->resources[at(p.resource)].cls == cls && resourceImproved(h) ? 1 : 0;
        });
        return n;
    };
    if (origin.has(wonderType(W::Zimbabwe))) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * goods(origin, ResourceClass::Bonus));
    if (!domestic && origin.has(wonderType(W::Torre)))
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * goods(destination, ResourceClass::Luxury));
    if (!domestic && destination.has(wonderType(W::Sankore))) {
        out[static_cast<size_t>(YieldType::Science)] += Fixed::fromInt(1);
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(1);
    }
    // Great people (07): routes to a city where Zheng He, Zhang Qian or Marco Polo were used (+2 Gold for other civs), routes to
    // city-states from where Ibn Fadlan was (+2 Faith), domestic routes from where Raja Todar Mal was (+0.5 Gold per specialty
    // district at the destination), and every route of John Rockefeller's player (+2 Gold per strategic resource at the destination).
    if (!destination.greatPeopleHere.empty() && !domestic)
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * (usedHere(destination, Gp::ZhengHe) + usedHere(destination, Gp::ZhangQian) + usedHere(destination, Gp::MarcoPolo)));
    if (!origin.greatPeopleHere.empty()) {
        if (isCityState(destination.owner)) out[static_cast<size_t>(YieldType::Faith)] += Fixed::fromInt(2 * usedHere(origin, Gp::IbnFadlan));
        if (domestic && usedHere(origin, Gp::RajaTodarMal) > 0) {
            int specialty = 0;
            for (const CityDistrict& d : destination.districts) specialty += d.complete && rules_->districts[at(d.type)].needsPopulation ? 1 : 0;
            out[static_cast<size_t>(YieldType::Gold)] += Fixed::ratio(specialty, 2);
        }
    }
    if (usedBy(origin.owner, Gp::Rockefeller)) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * goods(destination, ResourceClass::Strategic));
    // Religious Community (06): international routes +2 Gold each for the origin's Holy Site, Shrine, Temple and worship building.
    if (!domestic && cityFollows(origin, Bf::ReligiousCommunity)) {
        int n = origin.district(rules_->district("DISTRICT_HOLY_SITE"), true) ? 1 : 0;
        n += origin.has(rules_->building("BUILDING_SHRINE")) ? 1 : 0;
        n += cityHasBuilding(origin, *rules_, rules_->building("BUILDING_TEMPLE")) ? 1 : 0;  // Mali's Sahel Mosque too
        bool worship = false;
        for (const BeliefType& bt : rules_->beliefs) worship = worship || (bt.worshipBuilding != kNone && origin.has(bt.worshipBuilding));
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * (n + (worship ? 1 : 0)));
    }
    // Routes to a city-state bring the sender a bonus by its type (07: MINOR_CIV_*_SEND_TRADE_ROUTE_BONUS).
    if (isCityState(destination.owner)) {
        const TypeIndex cst = state_.players[at(destination.owner)].cityState;
        const CityStateKind kind = rules_->cityStates[at(cst)].kind;
        const int n = resolutionHits(ResolutionKind::Sovereignty, 0, static_cast<int32_t>(kind)) ? 2 : 1;  // Sovereignty A (World Congress)
        switch (kind) {
            case CityStateKind::Scientific: out[static_cast<size_t>(YieldType::Science)] += Fixed::fromInt(n); break;
            case CityStateKind::Cultural: out[static_cast<size_t>(YieldType::Culture)] += Fixed::fromInt(n); break;
            case CityStateKind::Religious: out[static_cast<size_t>(YieldType::Faith)] += Fixed::fromInt(n); break;
            case CityStateKind::Industrial:
            case CityStateKind::Militaristic: out[static_cast<size_t>(YieldType::Production)] += Fixed::fromInt(n); break;
            case CityStateKind::Trade: out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * n); break;
        }
    }
    // Letters of Marque (09) halves route yields; Isolationism's domestic bonus is a generated modifier above.
    if (policyIs(origin.owner, "POLICY_LETTERS_OF_MARQUE")) {
        for (Fixed& y : out) y = y / 2;
    }
    // Civ ability: routes whose way crosses desert (Arabia).
    if (const int gold = civAbility(origin.owner).desertRouteGold; gold > 0) {
        auto isDesert = [&](Hex h) { return rules_->terrains[at(state_.plot(h).terrain)].base == "DESERT"; };
        bool desert = false;
        if (way) {
            for (int32_t i : *way) desert = desert || isDesert(state_.grid.at(i));
        } else {
            for (const Hex& h : state_.grid.line(origin.pos, destination.pos)) desert = desert || isDesert(h);
        }
        if (desert) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(gold);
    }
    return out;
}

std::vector<std::vector<Hex>> Game::tradeWays(PlayerId player, TypeIndex traderType, const City& origin, const std::vector<const City*>& destinations) const {
    const int landRange = rules_->globalInt("TRADE_ROUTE_BASE_RANGE");
    const int waterRange = rules_->globalInt("TRADE_ROUTE_WATER_RANGE_REFUEL");
    const bool sails = canEmbark(player, traderType);
    const bool ocean = canEnterOcean(player);
    // Breadth-first over plots: land, then water too once Traders may embark (07: Range).
    // Range is 15 plots over land and 30 over water (07: Range): range is kept in units of landRange * waterRange,
    // a land plot spending waterRange of them and a water plot landRange, so a mixed way spends each leg's share.
    const int full = landRange * waterRange;
    // Range refuels in the player's own cities and in cities holding its Trading Post (07: Trading Posts):
    // a plot is searched again when reached with more range left. Each arrival is its own entry, so the
    // way back is the walk that reached the destination.
    // A destination is reached from the first plot searched beside it, whatever it stands on, and is otherwise
    // searched as any plot: so one search finds each destination's way as a search for it alone, ending there, would.
    const int n = state_.grid.size();
    std::vector<uint8_t> refuel(static_cast<size_t>(n), 0);
    for (const City& c : state_.cities) {
        if (c.owner == player || c.hasTradingPost(player)) refuel[static_cast<size_t>(state_.grid.index(c.pos))] = 1;
    }
    const int start = state_.grid.index(origin.pos);
    std::vector<int> goal(static_cast<size_t>(n), -1);  // the destination on each plot (none on the start: it is never reached)
    size_t open = 0;                                     // destinations not reached yet
    for (size_t k = 0; k < destinations.size(); ++k) {
        const int plot = state_.grid.index(destinations[k]->pos);
        if (plot == start) continue;
        goal[static_cast<size_t>(plot)] = static_cast<int>(k);
        ++open;
    }
    std::vector<std::vector<Hex>> ways(destinations.size());
    auto search = [&](bool water) {
        struct Entry { int plot, parent, fuel; };
        std::vector<Entry> entries;  // searched from in turn
        std::vector<int> best(static_cast<size_t>(n), -1);
        std::vector<int8_t> passable(static_cast<size_t>(n), -1);  // 0 no, 1 land, 2 water; worked out when first looked at
        entries.push_back({start, -1, full});
        best[static_cast<size_t>(start)] = full;
        for (size_t e = 0; e < entries.size() && open > 0; ++e) {
            const Entry cur = entries[e];
            if (cur.fuel <= 0) continue;
            const Hex here = state_.grid.at(cur.plot);
            for (int d = 0; d < kNumDirs; ++d) {
                auto nh = state_.grid.neighbor(here, static_cast<Dir>(d));
                if (!nh) continue;
                const size_t ni = static_cast<size_t>(state_.grid.index(*nh));
                if (const int k = goal[ni]; k >= 0 && ways[static_cast<size_t>(k)].empty()) {
                    std::vector<Hex>& way = ways[static_cast<size_t>(k)];
                    for (int w = static_cast<int>(e); w != -1; w = entries[static_cast<size_t>(w)].parent)
                        way.push_back(state_.grid.at(entries[static_cast<size_t>(w)].plot));
                    std::reverse(way.begin(), way.end());
                    way.push_back(*nh);
                    --open;
                }
                if (passable[ni] < 0) {
                    const TerrainType& t = rules_->terrains[at(state_.plot(*nh).terrain)];
                    const bool bridge = bridgeAt(*nh);
                    const bool ok = bridge || (t.water ? water && !t.impassable && (t.id != "TERRAIN_OCEAN" || ocean) : isLandPassable(state_, *rules_, *nh));
                    passable[ni] = static_cast<int8_t>(!ok ? 0 : t.water && !bridge ? 2 : 1);
                }
                if (!passable[ni]) continue;
                const int left = refuel[ni] ? full : cur.fuel - (passable[ni] == 2 ? landRange : waterRange);
                if (left < 0) continue;
                if (left <= best[ni]) continue;
                best[ni] = left;
                entries.push_back({static_cast<int>(ni), static_cast<int>(e), left});
            }
        }
    };
    search(false);
    if (open > 0 && sails) search(true);
    return ways;
}

std::vector<Hex> Game::tradePath(PlayerId player, TypeIndex traderType, const City& origin, const City& destination) const {
    return tradeWays(player, traderType, origin, {&destination}).front();
}

bool Game::canStartTradeRoute(UnitId traderId, CityId destinationId) const {
    const Unit* u = state_.unit(traderId);
    if (!u || rules_->units[at(u->type)].id != "UNIT_TRADER" || u->movesLeft <= Fixed()) return false;
    const City* origin = originOf(state_, *u);
    const City* dest = state_.city(destinationId);
    if (!origin || !dest || dest->id == origin->id) return false;
    const Player& them = state_.players[at(dest->owner)];
    // Routes go to major civs and city-states (07).
    if (!them.alive || them.barbarian || them.freeCity || atWar(u->owner, dest->owner)) return false;
    if (tradeRoutesOf(u->owner) >= tradeRouteCapacity(u->owner)) return false;
    if (visibility(u->owner, dest->pos) == Visibility::Unrevealed) return false;
    return !tradePath(u->owner, u->type, *origin, *dest).empty();
}

std::vector<CityId> Game::tradeDestinations(UnitId trader) const {
    // canStartTradeRoute for each city, with the checks on the Trader made once and one search for the ways to all.
    std::vector<CityId> out;
    const Unit* u = state_.unit(trader);
    if (!u || rules_->units[at(u->type)].id != "UNIT_TRADER" || u->movesLeft <= Fixed()) return out;
    const City* origin = originOf(state_, *u);
    if (!origin || tradeRoutesOf(u->owner) >= tradeRouteCapacity(u->owner)) return out;
    std::vector<const City*> cities;  // those the other checks allow
    for (const City& c : state_.cities) {
        const Player& them = state_.players[at(c.owner)];
        if (c.id == origin->id || !them.alive || them.barbarian || them.freeCity || atWar(u->owner, c.owner)) continue;
        if (visibility(u->owner, c.pos) != Visibility::Unrevealed) cities.push_back(&c);
    }
    const std::vector<std::vector<Hex>> ways = tradeWays(u->owner, u->type, *origin, cities);
    for (size_t k = 0; k < cities.size(); ++k) {
        if (!ways[k].empty()) out.push_back(cities[k]->id);
    }
    return out;
}

int Game::tradeRouteLength() const {
    const int era = std::clamp(worldEra(), 0, static_cast<int>(rules_->eras.size()) - 1);
    const int turns = rules_->globalInt("TRADE_ROUTE_TURN_DURATION_BASE") + rules_->eras[static_cast<size_t>(era)].tradeRouteExtraTurns;
    return std::max(1, turns * speedPercent(state_, *rules_) / 100);
}

// The Trader shuttles between the two cities, a round trip twice the way's length in plots, and the route ends when it
// is back home once the minimum has passed: the fewest whole round trips that reach the minimum (07: Duration;
// Sovereign reading of "exceeds" as "reaches", so a round trip that divides the minimum ends with it).
int Game::tradeRouteDuration(int steps) const {
    const int least = tradeRouteLength();
    const int trip = 2 * std::max(1, steps);
    return (least + trip - 1) / trip * trip;
}

TypeIndex Game::roadFor(PlayerId player) const {
    const int era = playerEra(player);
    TypeIndex best = kNone;
    for (size_t i = 0; i < rules_->routes.size(); ++i) {
        if (rules_->routes[i].era <= era && !rules_->routes[i].unitOnly) best = static_cast<TypeIndex>(i);  // the railroad is laid by hand
    }
    return best;
}

void Game::applyTradeRoute(const Command& c) {
    const Unit& u = *state_.unit(c.id);
    const City& origin = *originOf(state_, u);
    const City& dest = *state_.city(static_cast<CityId>(c.arg));
    TradeRoute r;
    r.id = state_.nextTradeRouteId++;
    r.owner = c.player;
    r.origin = origin.id;
    r.destination = dest.id;
    r.traderType = u.type;
    const std::vector<Hex> way = tradePath(c.player, u.type, origin, dest);
    r.turnsLeft = tradeRouteDuration(static_cast<int>(way.size()) - 1);  // the way runs from the origin's plot
    // Roads along the land part of the way (TRADE_ROUTE_PLACES_ROADS), upgraded to the owner's era.
    const TypeIndex road = roadFor(c.player);
    for (const Hex& h : way) {
        r.path.push_back(state_.grid.index(h));
        Plot& p = state_.plot(h);
        if (road != kNone && rules_->globalInt("TRADE_ROUTE_PLACES_ROADS") > 0 && !rules_->terrains[at(p.terrain)].water && p.route < road) {
            p.route = static_cast<int8_t>(road);
            p.routePillaged = false;
        }
    }
    state_.tradeRoutes.push_back(std::move(r));
    removeUnit(c.id);  // the Trader is on the road
}

void Game::processTrade(PlayerId pid) {
    std::vector<int32_t> ended;
    for (TradeRoute& r : state_.tradeRoutes) {
        if (r.owner != pid) continue;
        const City* origin = state_.city(r.origin);
        const City* dest = state_.city(r.destination);
        bool home = true;  // whether the Trader comes back
        bool cut = !origin || !dest || origin->owner != pid || atWar(pid, dest->owner);
        // A raider at war with the owner on the road plunders it (07: Plunder), unless the owner is in a
        // Golden Age with Reform the Coinage (09).
        if (!cut && !goldenDedication(pid, "DEDICATION_REFORM_THE_COINAGE")) {
            const bool safeAtSea = suzerainBonus(pid, Cs::Mogadishu);  // Mogadishu (08): no plunder on water
            for (int32_t pi : r.path) {
                const Hex h = state_.grid.at(pi);
                if (safeAtSea && rules_->terrains[at(state_.plot(h).terrain)].water) continue;
                const Unit* m = state_.unitAt(h, UnitLayer::Military, *rules_);
                if (m && atWar(pid, m->owner) && !state_.cityAt(h)) {
                    state_.players[at(m->owner)].gold += Fixed::fromInt(
                        rules_->globalInt("TRADE_ROUTE_PLUNDER_GOLD") * (100 + plunderPercent(*m)) / 100);
                    if (isMajorCiv(m->owner)) {
                        ++state_.players[at(m->owner)].tradersPlundered;
                        remember(pid, m->owner, MemoryKind::PlunderedTrader, -6, 30);
                    }
                    cut = true;
                    home = false;
                    break;
                }
            }
        }
        if (!cut) {
            // Religion travels with the caravans both ways (06: Trade routes carry pressure): the origin's into the
            // destination at RELIGION_SPREAD_TRADE_ROUTE_PRESSURE_FOR_DESTINATION times the base passive pressure, and
            // the destination's into the origin at _FOR_ORIGIN. A fraction is counted over the route's turns (half a
            // point a turn is a point every other turn). Both majorities are read before either city gains.
            City& start = *state_.city(r.origin);
            City& end = *state_.city(r.destination);
            const int fromOrigin = cityMajorityReligion(start), fromDestination = cityMajorityReligion(end);
            const int base = rules_->globalInt("RELIGION_SPREAD_ADJACENT_PER_TURN_PRESSURE");
            const auto carry = [&](City& to, int religion, const char* factor) {
                if (religion < 0) return;
                const Fixed per = rules_->global(factor) * base;
                const int64_t amount = (per * r.turnsLeft).floor() - (per * (r.turnsLeft - 1)).floor();
                if (to.pressure.size() < state_.religions.size()) to.pressure.resize(state_.religions.size(), 0);
                to.pressure[static_cast<size_t>(religion)] += static_cast<int32_t>(amount);
            };
            carry(end, fromOrigin, "RELIGION_SPREAD_TRADE_ROUTE_PRESSURE_FOR_DESTINATION");
            carry(start, fromDestination, "RELIGION_SPREAD_TRADE_ROUTE_PRESSURE_FOR_ORIGIN");
            if (--r.turnsLeft > 0) continue;
            dedicationScore(pid, "DEDICATION_REFORM_THE_COINAGE", 1);  // 09: a route completed
            // A completed route leaves the owner a Trading Post at its destination (07).
            City& d = *state_.city(r.destination);
            if (d.tradingPosts.size() < state_.players.size()) d.tradingPosts.resize(state_.players.size(), 0);
            // Historic moments (09): a post in a new civilization; posts in every civilization.
            const auto postsWith = [&](PlayerId civ) {
                for (const City& o : state_.cities) {
                    if (o.owner == civ && o.hasTradingPost(pid)) return true;
                }
                return false;
            };
            const bool newCiv = d.owner != pid && isMajorCiv(d.owner) && !postsWith(d.owner);
            d.tradingPosts[at(pid)] = 1;
            if (newCiv) {
                awardMoment(pid, "MOMENT_TRADING_POST_ESTABLISHED_IN_NEW_CIVILIZATION");
                bool all = true;
                for (const Player& x : state_.players) all = all && (x.id == pid || !isMajorCiv(x.id) || postsWith(x.id));
                if (all) awardFirst(pid, "MOMENT_FIRST_TRADING_POSTS_IN_ALL_CIVILIZATIONS", "MOMENT_TRADING_POSTS_IN_ALL_CIVILIZATIONS", 0);
            }
        }
        ended.push_back(r.id);
        if (home && origin && origin->owner == pid) {
            if (auto spot = unitSpawnPlot(*origin, r.traderType)) spawnUnit(r.traderType, pid, *spot);
        }
    }
    state_.tradeRoutes.erase(std::remove_if(state_.tradeRoutes.begin(), state_.tradeRoutes.end(),
                                            [&](const TradeRoute& r) { return std::find(ended.begin(), ended.end(), r.id) != ended.end(); }),
                             state_.tradeRoutes.end());
}

}  // namespace sov
