// Trade routes and roads (07-economy-trade-great-people.md, Trade routes; 01-map-and-terrain.md,
// Routes). A Trader in one of the player's cities starts a route to a city in range; the route
// pays its origin each turn by the districts at the destination, lays roads along the way and
// ends after its length (the Trader comes home), or when war or a raider cuts it.
#include <algorithm>
#include <queue>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {

size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }

bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity; }

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
            if (bt.tradeCapacity > 0 && (bt.tradeCapacityUnless == kNone || !c.has(bt.tradeCapacityUnless))) city += bt.tradeCapacity;
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
                                                  cs && suzerainOf(destination.owner) == origin.owner, true);
    for (size_t i = 0; i < kNumYields; ++i) out[i] += extra[i];
    return out;
}

Yields Game::tradeRouteYields(const City& origin, const City& destination) const {
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
        for (const Hex& h : state_.grid.within(origin.pos, 3)) owns = owns || (paititi != kNone && state_.plot(h).feature == paititi && state_.plot(h).city == origin.id);
        if (owns) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(4);
    }
    // Policy cards (04): Caravansaries, Market Economy, Raj... by the kind of route.
    {
        const bool cs = isCityState(destination.owner);
        const bool ally = !domestic && alliance(origin.owner, destination.owner) != AllianceType::None;
        const Yields extra = tradeRouteModifierYields(state_, *rules_, state_.players[at(origin.owner)], domestic, ally, cs,
                                                      cs && suzerainOf(destination.owner) == origin.owner);
        for (size_t i = 0; i < kNumYields; ++i) out[i] += extra[i];
    }
    // Market Economy (04): international routes +1 Gold per luxury and per strategic resource at the destination.
    if (!domestic && policyIs(origin.owner, "POLICY_MARKET_ECONOMY")) {
        int goods = 0;
        for (const Hex& h : state_.grid.within(destination.pos, 3)) {
            const Plot& p = state_.plot(h);
            if (p.city != destination.id || p.resource == kNone || !resourceImproved(h)) continue;
            const ResourceClass rc = rules_->resources[at(p.resource)].cls;
            goods += rc == ResourceClass::Luxury || rc == ResourceClass::Strategic ? 1 : 0;
        }
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(goods);
    }
    // City-state suzerains (08): Venice (+1 Gold per luxury at an international destination), Hunza (+0.2 Gold
    // per plot of the way), Kumasi (+2 Culture and +1 Gold per district on routes to city-states).
    if (!domestic && suzerainBonus(origin.owner, "CITYSTATE_VENICE")) {
        int luxuries = 0;
        for (const Hex& h : state_.grid.within(destination.pos, 3)) {
            const Plot& p = state_.plot(h);
            luxuries += p.city == destination.id && p.resource != kNone && rules_->resources[at(p.resource)].cls == ResourceClass::Luxury && resourceImproved(h) ? 1 : 0;
        }
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(luxuries);
    }
    if (suzerainBonus(origin.owner, "CITYSTATE_HUNZA"))
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::ratio(state_.grid.distance(origin.pos, destination.pos), 5);
    if (isCityState(destination.owner) && suzerainBonus(origin.owner, "CITYSTATE_KUMASI")) {
        int districts = 0;
        for (const CityDistrict& d : destination.districts) districts += d.complete ? 1 : 0;
        out[static_cast<size_t>(YieldType::Culture)] += Fixed::fromInt(2 * districts);
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(districts);
    }
    // Wonders (03): Great Zimbabwe (+2 Gold per bonus resource of the origin), Torre de Belém (+2 Gold per luxury at an
    // international destination), University of Sankore (+1 Science and +1 Gold on other civs' routes to it).
    auto goods = [&](const City& city, ResourceClass cls) {
        int n = 0;
        for (const Hex& h : state_.grid.within(city.pos, 3)) {
            const Plot& p = state_.plot(h);
            n += p.city == city.id && p.resource != kNone && rules_->resources[at(p.resource)].cls == cls && resourceImproved(h) ? 1 : 0;
        }
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
        n += origin.has(rules_->building("BUILDING_TEMPLE")) ? 1 : 0;
        bool worship = false;
        for (const BeliefType& bt : rules_->beliefs) worship = worship || (bt.worshipBuilding != kNone && origin.has(bt.worshipBuilding));
        out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2 * (n + (worship ? 1 : 0)));
    }
    // Routes to a city-state bring the sender a bonus by its type (07: MINOR_CIV_*_SEND_TRADE_ROUTE_BONUS).
    if (isCityState(destination.owner)) {
        const TypeIndex cst = state_.players[at(destination.owner)].cityState;
        switch (rules_->cityStates[at(cst)].kind) {
            case CityStateKind::Scientific: out[static_cast<size_t>(YieldType::Science)] += Fixed::fromInt(1); break;
            case CityStateKind::Cultural: out[static_cast<size_t>(YieldType::Culture)] += Fixed::fromInt(1); break;
            case CityStateKind::Religious: out[static_cast<size_t>(YieldType::Faith)] += Fixed::fromInt(1); break;
            case CityStateKind::Industrial:
            case CityStateKind::Militaristic: out[static_cast<size_t>(YieldType::Production)] += Fixed::fromInt(1); break;
            case CityStateKind::Trade: out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(2); break;
        }
    }
    // Letters of Marque (09) halves route yields; Isolationism's domestic bonus is a generated modifier above.
    if (policyIs(origin.owner, "POLICY_LETTERS_OF_MARQUE")) {
        for (Fixed& y : out) y = y / 2;
    }
    // Civ ability: routes whose way crosses desert (Arabia).
    if (const int gold = civAbility(origin.owner).desertRouteGold; gold > 0) {
        bool desert = false;
        for (const Hex& h : state_.grid.line(origin.pos, destination.pos)) desert = desert || rules_->terrains[at(state_.plot(h).terrain)].base == "DESERT";
        if (desert) out[static_cast<size_t>(YieldType::Gold)] += Fixed::fromInt(gold);
    }
    return out;
}

std::vector<Hex> Game::tradePath(PlayerId player, TypeIndex traderType, const City& origin, const City& destination) const {
    const int landRange = rules_->globalInt("TRADE_ROUTE_BASE_RANGE");
    const int waterRange = rules_->globalInt("TRADE_ROUTE_WATER_RANGE_REFUEL");
    const bool sails = canEmbark(player, traderType);
    const bool ocean = canEnterOcean(player);
    // Breadth-first over plots: land, then water too once Traders may embark (07: Range).
    // Range refuels in the player's own cities and in cities holding its Trading Post (07: Trading Posts):
    // a plot is searched again when reached with more range left. Each arrival is its own entry, so the
    // way back is the walk that reached the goal.
    const int n = state_.grid.size();
    std::vector<uint8_t> refuel(static_cast<size_t>(n), 0);
    for (const City& c : state_.cities) {
        if (c.owner == player || c.hasTradingPost(player)) refuel[static_cast<size_t>(state_.grid.index(c.pos))] = 1;
    }
    auto search = [&](bool water, int range) -> std::vector<Hex> {
        struct Entry { int plot, parent, fuel; };
        std::vector<Entry> entries;
        std::vector<int> best(static_cast<size_t>(n), -1);
        std::queue<int> open;
        const int start = state_.grid.index(origin.pos), goal = state_.grid.index(destination.pos);
        entries.push_back({start, -1, range});
        best[static_cast<size_t>(start)] = range;
        open.push(0);
        int found = -1;
        while (!open.empty() && found < 0) {
            const Entry cur = entries[static_cast<size_t>(open.front())];
            const int curIndex = open.front();
            open.pop();
            if (cur.fuel <= 0) continue;
            for (int d = 0; d < kNumDirs; ++d) {
                auto nh = state_.grid.neighbor(state_.grid.at(cur.plot), static_cast<Dir>(d));
                if (!nh) continue;
                const int ni = state_.grid.index(*nh);
                const TerrainType& t = rules_->terrains[at(state_.plot(*nh).terrain)];
                const bool ok = ni == goal || (t.water ? water && !t.impassable && (t.id != "TERRAIN_OCEAN" || ocean) : isLandPassable(state_, *rules_, *nh));
                if (!ok) continue;
                const int left = refuel[static_cast<size_t>(ni)] ? range : cur.fuel - 1;
                if (left <= best[static_cast<size_t>(ni)]) continue;
                best[static_cast<size_t>(ni)] = left;
                entries.push_back({ni, curIndex, left});
                if (ni == goal) {
                    found = static_cast<int>(entries.size()) - 1;
                    break;
                }
                open.push(static_cast<int>(entries.size()) - 1);
            }
        }
        std::vector<Hex> path;
        for (int e = found; e != -1; e = entries[static_cast<size_t>(e)].parent) path.push_back(state_.grid.at(entries[static_cast<size_t>(e)].plot));
        std::reverse(path.begin(), path.end());
        return path;
    };
    std::vector<Hex> path = search(false, landRange);
    if (path.empty() && sails) path = search(true, waterRange);
    return path;
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
    std::vector<CityId> out;
    for (const City& c : state_.cities) {
        if (canStartTradeRoute(trader, c.id)) out.push_back(c.id);
    }
    return out;
}

int Game::tradeRouteLength() const {
    const int era = std::clamp(worldEra(), 0, static_cast<int>(rules_->eras.size()) - 1);
    const int turns = rules_->globalInt("TRADE_ROUTE_TURN_DURATION_BASE") + rules_->eras[static_cast<size_t>(era)].tradeRouteExtraTurns;
    return std::max(1, turns * speedPercent(state_, *rules_) / 100);
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
    r.turnsLeft = tradeRouteLength();
    // Roads along the land part of the way (TRADE_ROUTE_PLACES_ROADS), upgraded to the owner's era.
    const TypeIndex road = roadFor(c.player);
    for (const Hex& h : tradePath(c.player, u.type, origin, dest)) {
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
            for (int32_t pi : r.path) {
                const Hex h = state_.grid.at(pi);
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
            // Religion travels with the caravans (06: Trade routes carry pressure).
            const int maj = cityMajorityReligion(*origin);
            if (maj >= 0) {
                City& d = *state_.city(r.destination);
                if (d.pressure.size() < state_.religions.size()) d.pressure.resize(state_.religions.size(), 0);
                d.pressure[static_cast<size_t>(maj)] += static_cast<int32_t>(rules_->global("RELIGION_SPREAD_TRADE_ROUTE_PRESSURE_FOR_DESTINATION").round());
            }
            if (--r.turnsLeft > 0) continue;
            dedicationScore(pid, "DEDICATION_REFORM_THE_COINAGE", 1);  // 09: a route completed
            // A completed route leaves the owner a Trading Post at its destination (07).
            City& d = *state_.city(r.destination);
            if (d.tradingPosts.size() < state_.players.size()) d.tradingPosts.resize(state_.players.size(), 0);
            d.tradingPosts[at(pid)] = 1;
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
