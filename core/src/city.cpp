// City rules (specs/civ6/02-cities.md): yields, citizens, growth, housing,
// amenities, border growth, production, purchases, gold and maintenance.
#include <algorithm>
#include <tuple>

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
    // Natural wonders next door (01): their adjacent yields, or the terrain's yields again (Torres del Paine).
    if (!rules_->features.empty()) {
        for (const Hex& n : state_.grid.within(at, 1)) {
            const Plot& np = state_.plot(n);
            if (n == at || np.feature == kNone || np.feature == p.feature) continue;
            const FeatureType& nw = rules_->features[static_cast<size_t>(np.feature)];
            if (!nw.naturalWonder) continue;
            for (size_t i = 0; i < kNumYields; ++i) y[i] += nw.adjacentYields[i];
            if (nw.doublesAdjacentTerrain) {
                const Yields& ty = rules_->terrains[static_cast<size_t>(p.terrain)].yields;
                for (size_t i = 0; i < kNumYields; ++i) y[i] += ty[i];
            }
        }
    }
    if (p.improvement != kNone && p.pillagedTurns == 0 && rules_->improvements[static_cast<size_t>(p.improvement)].powerProvided > 0 &&
        cityGovernorHas(city, "GOVERNOR_PROMOTION_RENEWABLE_SUBSIDIZER"))
        y[idx(YieldType::Gold)] += Fixed::fromInt(2);  // Reyna
    if (p.fallout > 0 && at != city.pos) return Yields{};  // contaminated ground cannot be worked
    // Earth Goddess (06): +1 Faith on plots of Appeal 4 or more.
    if (cityFollows(city, Bf::EarthGoddess) && plotAppeal(at) >= 4) y[idx(YieldType::Faith)] += Fixed::fromInt(1);
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
        // A pillaged district's buildings stand idle (05: Pillage).
        if (bt.districtType != kNone) {
            const CityDistrict* home = c->district(bt.districtType, true);
            if (home && home->pillagedTurns > 0) continue;
        }
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += bt.yields[i];
        rep.housing += bt.housing;
        rep.amenities += bt.amenities;
        // Free Market, Grand Opera, Rationalism, Simultaneum (04): +50% of the district's yield from its buildings
        // with an adjacency of 4 or more for it, and +50% more in a city of 15 or more.
        static const std::array<std::tuple<const char*, const char*, YieldType>, 4> kCards = {{
            {"POLICY_FREE_MARKET", "DISTRICT_COMMERCIAL_HUB", YieldType::Gold}, {"POLICY_GRAND_OPERA", "DISTRICT_THEATER", YieldType::Culture},
            {"POLICY_RATIONALISM", "DISTRICT_CAMPUS", YieldType::Science}, {"POLICY_SIMULTANEUM", "DISTRICT_HOLY_SITE", YieldType::Faith}}};
        for (const auto& [card, district, yield] : kCards) {
            if (bt.district != district || !policyIs(c->owner, card)) continue;
            const CityDistrict* home = c->district(bt.districtType, true);
            int pct = c->population >= 15 ? 50 : 0;
            if (home && districtAdjacency(c->owner, home->type, home->pos)[idx(yield)] >= Fixed::fromInt(4)) pct += 50;
            raw[idx(yield)] += bt.yields[idx(yield)] * pct / 100;
        }
    }
    // Public Transport (04): a Neighborhood yields +1 Gold; +3 Food and +1 Production on ground of
    // appeal 2 or more, and +1 Food and +1 Production more at appeal 4 or more.
    if (policyIs(c->owner, "POLICY_PUBLIC_TRANSPORT")) {
        for (const CityDistrict& d : c->districts) {
            if (!d.complete || d.pillagedTurns > 0 || rules_->districts[static_cast<size_t>(d.type)].id != "DISTRICT_NEIGHBORHOOD") continue;
            const int appeal = plotAppeal(d.pos);
            raw[idx(YieldType::Gold)] += Fixed::fromInt(1);
            if (appeal >= 2) {
                raw[idx(YieldType::Food)] += Fixed::fromInt(3);
                raw[idx(YieldType::Production)] += Fixed::fromInt(1);
            }
            if (appeal >= 4) {
                raw[idx(YieldType::Food)] += Fixed::fromInt(1);
                raw[idx(YieldType::Production)] += Fixed::fromInt(1);
            }
        }
    }
    // Specialists (02): each earns its district's specialist yield plus its buildings' extras.
    for (const CityDistrict& d : c->districts) {
        if (d.specialists == 0) continue;
        const Yields y = specialistYield(*c, d);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += y[i] * d.specialists;
    }
    // Envoys to city-states pay in the capital and per building (08).
    {
        const Yields ey = envoyYields(*c);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += ey[i];
    }
    // Trade routes from this city pay by the districts at their destinations (07); routes to it may
    // pay it too (allies, Wisselbanken).
    for (const TradeRoute& tr : state_.tradeRoutes) {
        if (tr.origin == c->id) {
            if (const City* dest = state_.city(tr.destination)) {
                const Yields ty = tradeRouteYields(*c, *dest);
                for (size_t i = 0; i < kNumYields; ++i) raw[i] += ty[i];
            }
        } else if (tr.destination == c->id) {
            if (const City* from = state_.city(tr.origin)) {
                const Yields ty = tradeRouteDestinationYields(*from, *c);
                for (size_t i = 0; i < kNumYields; ++i) raw[i] += ty[i];
            }
        }
    }
    // Great Works in the city's slots, and great people whose effects improve its buildings.
    const bool reliquaries = !c->greatWorks.empty() && cityFollows(*c, Bf::Reliquaries);
    for (const GreatWork& w : c->greatWorks) {
        const GreatWorkType& gw = rules_->greatWorkTypes[static_cast<size_t>(w.type)];
        const int pct = themed(*c, w.building) ? 100 + rules_->buildings[static_cast<size_t>(w.building)].theming->yieldPercent : 100;  // 07: Theming
        raw[idx(gw.yield)] += Fixed::fromInt(gw.amount * pct / 100 * (gw.id == "RELIC" && reliquaries ? 3 : 1));  // Reliquaries (06)
        // Anshan (08: suzerain): +2 Science from writing, +1 from artifacts and relics.
        if (suzerainBonus(c->owner, "CITYSTATE_ANSHAN"))
            raw[idx(YieldType::Science)] += Fixed::fromInt(gw.id == "WRITING" ? 2 : (gw.id == "ARTIFACT" || gw.id == "RELIC") ? 1 : 0);
    }
    for (TypeIndex person : owner.greatPeopleActivated) {
        for (const GreatPersonEffect& fx : rules_->greatPeople[static_cast<size_t>(person)].effects) {
            if (fx.kind == GreatPersonEffectKind::BuildingYield && c->has(fx.ref)) raw[idx(fx.yield)] += Fixed::fromInt(fx.amount);
        }
    }
    // Finished districts add their adjacency yields to the city.
    const bool inquiry = goldenDedication(c->owner, "DEDICATION_FREE_INQUIRY"), steam = goldenDedication(c->owner, "DEDICATION_HEARTBEAT_OF_STEAM"),
               pen = goldenDedication(c->owner, "DEDICATION_PEN_BRUSH_AND_VOICE");
    for (const CityDistrict& d : c->districts) {
        if (!d.complete) continue;
        Yields adj = districtAdjacency(c->owner, d.type, d.pos);
        for (size_t i = 0; i < kNumYields; ++i) raw[i] += adj[i];
        // Dedications in a Golden Age (09): Commercial Hubs and Harbors add their Gold adjacency as Science (Free
        // Inquiry); Campuses their Science adjacency as Production (Heartbeat of Steam); +1 Culture per district (Pen).
        const std::string& kind = rules_->districts[static_cast<size_t>(d.type)].id;
        if (inquiry && (kind == "DISTRICT_COMMERCIAL_HUB" || kind == "DISTRICT_HARBOR")) raw[idx(YieldType::Science)] += adj[idx(YieldType::Gold)];
        if (steam && kind == "DISTRICT_CAMPUS") raw[idx(YieldType::Production)] += adj[idx(YieldType::Science)];
        if (pen && kind != "DISTRICT_CITY_CENTER") raw[idx(YieldType::Culture)] += Fixed::fromInt(1);
        // Work Ethic (06): the Holy Site's Faith adjacency as Production too.
        if (kind == "DISTRICT_HOLY_SITE" && cityFollows(*c, Bf::WorkEthic)) raw[idx(YieldType::Production)] += adj[idx(YieldType::Faith)];
    }
    // Divine Inspiration (06): +4 Faith per wonder in the city.
    if (cityFollows(*c, Bf::DivineInspiration)) {
        for (TypeIndex b : c->buildings) raw[idx(YieldType::Faith)] += Fixed::fromInt(rules_->buildings[static_cast<size_t>(b)].wonder ? 4 : 0);
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
    // University of Sankore (03: Wonders): +2 Science for each other civ's trade route to this city.
    if (c->has(wonderType(W::Sankore))) {
        for (const TradeRoute& tr : state_.tradeRoutes) {
            if (tr.destination == c->id && tr.owner != c->owner) raw[idx(YieldType::Science)] += Fixed::fromInt(2);
        }
    }
    // Johannesburg (08: suzerain): +1 Production per kind of improved resource here, +1 more after Industrialization.
    if (suzerainBonus(c->owner, "CITYSTATE_JOHANNESBURG")) {
        std::vector<TypeIndex> kinds;
        for (const Hex& h : state_.grid.within(c->pos, 3)) {
            const Plot& p = state_.plot(h);
            if (p.city == c->id && p.resource != kNone && resourceImproved(h) && std::find(kinds.begin(), kinds.end(), p.resource) == kinds.end()) kinds.push_back(p.resource);
        }
        const TypeIndex industry = rules_->tech("TECH_INDUSTRIALIZATION");
        const bool industrial = industry != kNone && owner.techs.has(industry);
        raw[idx(YieldType::Production)] += Fixed::fromInt(static_cast<int>(kinds.size()) * (industrial ? 2 : 1));
    }
    // Singapore (08: suzerain): +2 Production per major civ we trade with.
    if (suzerainBonus(c->owner, "CITYSTATE_SINGAPORE")) {
        std::vector<PlayerId> partners;
        for (const TradeRoute& tr : state_.tradeRoutes) {
            const City* a = state_.city(tr.origin);
            const City* b = state_.city(tr.destination);
            if (!a || !b) continue;
            const PlayerId other = a->owner == c->owner ? b->owner : b->owner == c->owner ? a->owner : kNoPlayer;
            if (other != kNoPlayer && other != c->owner && isMajorCiv(other) && std::find(partners.begin(), partners.end(), other) == partners.end()) partners.push_back(other);
        }
        raw[idx(YieldType::Production)] += Fixed::fromInt(2 * static_cast<int>(partners.size()));
    }

    // Housing from water access, then buildings and modifiers.
    bool fresh = isRiverAdjacent(state_, c->pos), coastal = false;
    for (const Hex& n : state_.grid.within(c->pos, 1)) {
        const Plot& p = state_.plot(n);
        if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].freshWater) fresh = true;
        if (n != c->pos && rules_->terrains[static_cast<size_t>(p.terrain)].shallowWater) coastal = true;
    }
    if (suzerainBonus(c->owner, "CITYSTATE_MOHENJO_DARO")) fresh = true;  // Mohenjo-Daro (08: suzerain): every city as if on a river
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
    rep.amenities += parkAmenities(*c);  // 07: National Parks
    // Natural wonders (01): the city owning Pamukkale gains an amenity per natural wonder in its land.
    {
        const TypeIndex pamukkale = rules_->feature("FEATURE_PAMUKKALE");
        bool owns = false;
        std::vector<TypeIndex> wonders;
        for (const Hex& h : state_.grid.within(c->pos, 3)) {
            const Plot& pl = state_.plot(h);
            if (pl.city != c->id || pl.feature == kNone || !rules_->features[static_cast<size_t>(pl.feature)].naturalWonder) continue;
            owns = owns || pl.feature == pamukkale;
            if (std::find(wonders.begin(), wonders.end(), pl.feature) == wonders.end()) wonders.push_back(pl.feature);
        }
        if (owns) rep.amenities += static_cast<int>(wonders.size());
    }
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

    const bool kilwa = holdsWonder(c->owner, W::Kilwa);
    for (size_t i = 0; i < kNumYields; ++i) {
        int pct = 100 + static_cast<int>(sumCityModifiers(state_, *rules_, *c, ModEffect::CityYieldPercent,
                                                          static_cast<YieldType>(i)).toInt());
        // Kilwa Kisiwani (03: Wonders): Science, Culture, Faith or Gold by suzerainties of the matching kind.
        if (kilwa) {
            static const std::pair<YieldType, CityStateKind> kKilwa[] = {{YieldType::Science, CityStateKind::Scientific}, {YieldType::Culture, CityStateKind::Cultural},
                                                                         {YieldType::Faith, CityStateKind::Religious}, {YieldType::Gold, CityStateKind::Trade}};
            for (const auto& [y, k] : kKilwa) pct += i == idx(y) ? kilwaPercent(*c, k) : 0;
        }
        // Antananarivo (08: suzerain): +2% Culture per great person earned.
        if (i == idx(YieldType::Culture) && suzerainBonus(c->owner, "CITYSTATE_ANTANANARIVO")) {
            int earned = 0;
            for (int n : owner.greatPeopleRecruited) earned += n;
            pct += 2 * earned;
        }
        // Collective Activism, International Space Agency (04): +5% Culture or Science per suzerainty.
        if ((i == idx(YieldType::Culture) && policyIs(c->owner, "POLICY_COLLECTIVE_ACTIVISM")) ||
            (i == idx(YieldType::Science) && policyIs(c->owner, "POLICY_INTERNATIONAL_SPACE_AGENCY")))
            pct += 5 * suzeraintiesOf(c->owner);
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
        // A Religious alliance at level 3 (08): +1 Faith per follower of the ally's religion here.
        for (const Player& ally : state_.players) {
            if (alliance(c->owner, ally.id) != AllianceType::Religious || allianceLevel(c->owner, ally.id) < 3 || ally.religion < 0) continue;
            rep.yields[idx(YieldType::Faith)] += Fixed::fromInt(cityFollowers(*c, ally.religion));
        }
        // Raj (04): +2 Gold, Faith, Science and Culture in the capital per suzerainty.
        if (c->capital && policyIs(c->owner, "POLICY_RAJ")) {
            const int n = suzeraintiesOf(c->owner);
            for (YieldType y : {YieldType::Gold, YieldType::Faith, YieldType::Science, YieldType::Culture}) rep.yields[idx(y)] += Fixed::fromInt(2 * n);
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
        // Trained as a Corps or an Army (05): UNIT_CORPS_COST_MODIFIER / UNIT_ARMY_COST_MODIFIER.
        if (item.formation > 0)
            base = static_cast<int>((Fixed::fromInt(base) * rules_->global(item.formation == 1 ? "UNIT_CORPS_COST_MODIFIER" : "UNIT_ARMY_COST_MODIFIER")).toInt());
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
    if (item.kind == ProductionKind::Unit && goldenDedication(player, "DEDICATION_MONUMENTALITY")) {
        const std::string& id = rules_->units[static_cast<size_t>(item.type)].id;
        if (id == "UNIT_BUILDER" || id == "UNIT_SETTLER") cost = cost * 70 / 100;  // 09: Monumentality
    }
    // Valletta (08: suzerain): walls at half price.
    if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].outerDefenseHp > 0 && suzerainBonus(player, "CITYSTATE_VALLETTA"))
        cost /= 2;
    // Flower Power (09): units cost twice as much to buy, Rock Bands excepted.
    if (item.kind == ProductionKind::Unit && policyIs(player, "POLICY_FLOWER_POWER") && rules_->units[static_cast<size_t>(item.type)].id != "UNIT_ROCK_BAND")
        cost *= 2;
    return cost / 5 * 5;
}

// District purchase (08: Governors, Contractor and Divine Architect). Civ VI otherwise never sells
// districts (02: Purchasing); the price is the usual 4x production cost, rounded down to a multiple of 5,
// for Faith as for Gold (Sovereign reading). The district must already be placed.
int Game::districtPurchaseCost(const City& city, TypeIndex district, bool faith) const {
    if (district < 0 || static_cast<size_t>(district) >= rules_->districts.size()) return -1;
    if (!cityGovernorHas(city, faith ? "GOVERNOR_PROMOTION_DIVINE_ARCHITECT" : "GOVERNOR_PROMOTION_CONTRACTOR")) return -1;
    const CityDistrict* d = city.district(district, false);
    if (!d || d->complete) return -1;
    const ProductionItem item{ProductionKind::District, district};
    int cost = productionCost(city.owner, item) * rules_->globalInt("GOLD_PURCHASE_MULTIPLIER") * std::max(1, rules_->globalInt("GOLD_PURCHASE_ENGINE_FACTOR"));
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

// Corps and Armies trained whole (05: Corps and Armies): land units in a city with a Military Academy,
// ships in one with a Seaport, once the civ has Nationalism (Corps, Fleets) or Mobilization (Armies, Armadas).
bool Game::canTrainFormation(const City& c, TypeIndex unit, int formation) const {
    const UnitType& u = rules_->units[static_cast<size_t>(unit)];
    if (formation < 1 || formation > 2 || u.layer != UnitLayer::Military || (u.domain != Domain::Land && u.domain != Domain::Sea) || u.agent) return false;
    if (u.combat <= 0 && u.ranged <= 0) return false;
    const TypeIndex civic = rules_->civic(formation == 1 ? "CIVIC_NATIONALISM" : "CIVIC_MOBILIZATION");
    if (civic == kNone || !state_.players[static_cast<size_t>(c.owner)].civics.has(civic)) return false;
    const TypeIndex school = rules_->building(u.domain == Domain::Land ? "BUILDING_MILITARY_ACADEMY" : "BUILDING_SEAPORT");
    return school != kNone && cityHasBuilding(c, *rules_, school);
}

bool Game::canProduce(const City& c, ProductionItem item, CommandError* why, bool purchase) const {
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
        if (item.formation > 0 && !canTrainFormation(c, item.type, item.formation)) return fail(CommandError::CannotBuild);
        // Civ uniques: only their civ trains them, and for it they replace their base unit.
        const TypeIndex civ = state_.players[static_cast<size_t>(c.owner)].civ;
        if (u.uniqueTo != kNone && u.uniqueTo != civ) return fail(CommandError::CannotBuild);
        if (rules_->uniqueUnitFor(civ, item.type) != kNone) return fail(CommandError::CannotBuild);
        if (u.agent && !u.spy && agentsOf(c.owner) >= agentCapacity(c.owner)) return fail(CommandError::CannotBuild);
        // Dark Age cards (09): Isolationism trains no Settlers; under Flower Power units are only bought.
        if (u.foundCity && policyIs(c.owner, "POLICY_ISOLATIONISM")) return fail(CommandError::CannotBuild);
        if (!purchase && policyIs(c.owner, "POLICY_FLOWER_POWER")) return fail(CommandError::CannotBuild);
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
        // Repair Outer Defenses: only with walls that are down. Send Aid: only while another civ asks for aid.
        for (const ProjectEffect& e : pj.effects) {
            if (e.kind == ProjectEffectKind::RepairWalls && c.wallHp >= cityMaxWallHp(c)) return fail(CommandError::CannotBuild);
            if (e.kind == ProjectEffectKind::Competition) {
                bool running = false;
                for (const Competition& cp : state_.competitions) running = running || (!cp.settled && static_cast<TypeIndex>(cp.kind) == e.weapon);
                if (!running) return fail(CommandError::CannotBuild);
            }
            if (e.kind == ProjectEffectKind::Decommission && (e.weapon == kNone || !c.has(e.weapon))) return fail(CommandError::CannotBuild);
            if (e.kind == ProjectEffectKind::Recommission && !c.has(rules_->building("BUILDING_NUCLEAR_POWER_PLANT"))) return fail(CommandError::CannotBuild);
            if (e.kind == ProjectEffectKind::Convert) {
                // A city with another kind of power plant converts it (09: Power).
                bool other = false;
                for (TypeIndex b : c.buildings) other = other || (b != e.weapon && rules_->buildings[static_cast<size_t>(b)].burnsResource != kNone);
                if (e.weapon == kNone || c.has(e.weapon) || !other) return fail(CommandError::CannotBuild);
            }
            if (e.kind == ProjectEffectKind::Aid) {
                const Competition* aid = runningAidRequest();
                if (!aid || aid->beneficiary == c.owner || atWar(c.owner, aid->beneficiary)) return fail(CommandError::CannotBuild);
            }
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
        if (!canProduce(*c, it) || !hasStrategicFor(c->owner, it.type)) continue;
        out.push_back(it);
        // The same unit trained as a Corps or an Army where the city can (05: Corps and Armies).
        for (uint8_t f = 1; f <= 2; ++f) {
            const ProductionItem whole{ProductionKind::Unit, it.type, f};
            if (canProduce(*c, whole)) out.push_back(whole);
        }
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
    // Merchant Confederation (04): +1 Gold per envoy placed.
    if (p.anarchyTurns == 0 && policyIs(player, "POLICY_MERCHANT_CONFEDERATION")) {
        for (int32_t e : p.envoys) net += Fixed::fromInt(e);
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
    const ProductionItem item{static_cast<ProductionKind>(c.arg & 15), static_cast<TypeIndex>(c.arg2), static_cast<uint8_t>((c.arg >> 4) & 15)};
    CommandError why = CommandError::Ok;
    switch (c.type) {
        case CommandType::SetProduction:
            if (c.arg < 0 || (c.arg & 15) > 3 || (c.arg >> 4) > 2 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
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
            if (c.arg < 0 || (c.arg & 15) > 3 || (c.arg >> 4) > 2 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
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
            if (c.arg < 0 || (c.arg & 15) > 3 || (c.arg >> 4) > 2 || c.arg2 < INT16_MIN || c.arg2 > INT16_MAX) return CommandError::CannotBuild;
            if (item.kind == ProductionKind::District) {
                const bool faith = c.target.x == 1;
                const int cost = districtPurchaseCost(*city, item.type, faith);
                if (cost < 0) return CommandError::CannotBuild;
                const Player& buyer = state_.players[static_cast<size_t>(c.player)];
                if ((faith ? buyer.faith : buyer.gold) < Fixed::fromInt(cost)) return faith ? CommandError::NotEnoughFaith : CommandError::NotEnoughGold;
                return CommandError::Ok;
            }
            if (c.target.x == 1) {
                // Religious units and worship buildings, bought with Faith (06).
                const int faith = faithPurchaseCost(c.player, *city, item);
                if (faith < 0) return CommandError::CannotBuild;
                if (item.kind == ProductionKind::Unit && !unitSpawnPlot(*city, item.type)) return CommandError::CannotBuild;
                if (state_.players[static_cast<size_t>(c.player)].faith < Fixed::fromInt(faith)) return CommandError::NotEnoughFaith;
                return CommandError::Ok;
            }
            if (!canProduce(*city, item, &why, true)) return why;
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
    const ProductionItem item{static_cast<ProductionKind>(c.arg & 15), static_cast<TypeIndex>(c.arg2), static_cast<uint8_t>((c.arg >> 4) & 15)};
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
            if (item.kind == ProductionKind::District) {
                const bool faith = c.target.x == 1;
                (faith ? p.faith : p.gold) -= Fixed::fromInt(districtPurchaseCost(city, item.type, faith));
                completeItem(city, item);
                City& bought = *state_.city(c.id);
                bought.queue.erase(std::remove(bought.queue.begin(), bought.queue.end(), item), bought.queue.end());
                for (size_t i = 0; i < bought.progress.size(); ++i) {
                    if (bought.progress[i].item == item) {
                        bought.overflow += bought.progress[i].amount;
                        bought.progress.erase(bought.progress.begin() + static_cast<long>(i));
                        break;
                    }
                }
                break;
            }
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
                    const UnitType& bought = rules_->units[static_cast<size_t>(item.type)];
                    // Only religious units carry the city's religion (Naturalists and Rock Bands do not).
                    u.religion = static_cast<int16_t>(bought.religiousStrength > 0 || bought.spreadCharges > 0 ? religion : -1);
                    if (bought.id == "UNIT_ROCK_BAND") grantBandPromotion(u);  // every band starts with one (07)
                    u.charges = rules_->units[static_cast<size_t>(item.type)].spreadCharges +
                                (goldenDedication(c.player, "DEDICATION_EXODUS_OF_THE_EVANGELISTS") ? 2 : 0);  // 09: Exodus of the Evangelists
                    if (bought.id == "UNIT_APOSTLE") grantApostlePromotion(u);  // each new Apostle gets one (06)
                    if (bought.spreadCharges > 0 && city.has(rules_->building("BUILDING_MOSQUE"))) ++u.charges;  // Mosque (03)
                    if (bought.spreadCharges > 0) {
                        for (const City& o : state_.cities) {
                            if (o.owner != c.player) continue;
                            for (TypeIndex b : o.buildings) u.charges += rules_->buildings[static_cast<size_t>(b)].spreadCharges;  // Hagia Sophia
                        }
                    }
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

    // Candidates: each workable plot, and each specialist slot in the city's districts (index -1 - district).
    struct Cand { int32_t index; Fixed score; };
    std::vector<Cand> cands;
    for (const Hex& h : plots) {
        int32_t pi = state_.grid.index(h);
        if (std::binary_search(city.locked.begin(), city.locked.end(), pi)) continue;
        cands.push_back({pi, citizenScore(plotYields(h, city))});
    }
    for (size_t k = 0; k < city.districts.size(); ++k) {
        CityDistrict& d = city.districts[k];
        d.specialists = 0;
        if (!d.complete || d.pillagedTurns > 0) continue;
        const Fixed score = citizenScore(specialistYield(city, d));
        for (int slot = 0; slot < specialistSlots(city, d); ++slot) cands.push_back({-1 - static_cast<int32_t>(k), score});
    }
    std::stable_sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.score != b.score ? a.score > b.score : a.index > b.index;  // ties: plots (indices >= 0) before slots
    });
    city.worked = city.locked;
    int citizens = static_cast<int>(city.locked.size());
    for (const Cand& c : cands) {
        if (citizens >= city.population) break;
        if (c.index >= 0) city.worked.push_back(c.index);
        else ++city.districts[static_cast<size_t>(-1 - c.index)].specialists;
        ++citizens;
    }
    std::sort(city.worked.begin(), city.worked.end());
}

bool Game::completeItem(City& city, ProductionItem item) {
    // Dedications (09): era score for science, culture, Industrial Zone and Aerodrome buildings, and districts.
    if (item.kind == ProductionKind::Building) {
        const std::string& d = rules_->buildings[static_cast<size_t>(item.type)].district;
        if (d == "DISTRICT_CAMPUS") dedicationScore(city.owner, "DEDICATION_FREE_INQUIRY", 1);
        if (d == "DISTRICT_THEATER") dedicationScore(city.owner, "DEDICATION_PEN_BRUSH_AND_VOICE", 1);
        if (d == "DISTRICT_INDUSTRIAL_ZONE") dedicationScore(city.owner, "DEDICATION_HEARTBEAT_OF_STEAM", 1);
        if (d == "DISTRICT_AERODROME") dedicationScore(city.owner, "DEDICATION_SKY_AND_STARS", 1);
    }
    if (item.kind == ProductionKind::District) dedicationScore(city.owner, "DEDICATION_MONUMENTALITY", 1);
    // Ayutthaya (08: suzerain): Culture of a tenth of a building's cost when it is done.
    if (item.kind == ProductionKind::Building && suzerainBonus(city.owner, "CITYSTATE_AYUTTHAYA"))
        processResearch(city.owner, Fixed(), Fixed::fromInt(productionCost(city.owner, item) / 10));
    // Public Transport (04): a new Neighborhood brings 100 Gold.
    if (item.kind == ProductionKind::District && rules_->districts[static_cast<size_t>(item.type)].id == "DISTRICT_NEIGHBORHOOD" &&
        policyIs(city.owner, "POLICY_PUBLIC_TRANSPORT"))
        state_.players[static_cast<size_t>(city.owner)].gold += Fixed::fromInt(100);
    // A new reactor starts its age (09: nuclear accidents).
    if (item.kind == ProductionKind::Building && rules_->buildings[static_cast<size_t>(item.type)].id == "BUILDING_NUCLEAR_POWER_PLANT") city.reactorSince = state_.turn;
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
            if (a.spy && buildingsOwned(city.owner, "BUILDING_INTELLIGENCE_AGENCY") > 0) a.level = 2;  // trained a level up (08)
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
        made.formation = item.formation;
        for (TypeIndex bi : city.buildings) {
            const int pct = rules_->buildings[static_cast<size_t>(bi)].trainedXpPercent;
            if (pct > 0 && !u.promotionClass.empty()) made.xp = std::min(xpForNextLevel(made), made.xp + xpForNextLevel(made) * pct / 100);
        }
        if (!u.promotionClass.empty() && cityGovernorHas(city, "GOVERNOR_PROMOTION_EMBRASURE")) made.xp = std::max(made.xp, xpForNextLevel(made));  // Victor's Embrasure
        // A Military alliance at level 3 (08): units trained have a promotion's XP.
        if (!u.promotionClass.empty() && u.layer == UnitLayer::Military && bestAllianceLevel(city.owner, AllianceType::Military) >= 3)
            made.xp = std::max(made.xp, xpForNextLevel(made));
        questDone(city.owner, QuestKind::TrainUnit, item.type);  // 08: Quests
        if (made.charges > 0) made.charges += static_cast<int>(sumCityModifiers(state_, *rules_, city, ModEffect::BuilderExtraCharges).toInt()) +
                                              (u.buildCharges > 0 && !u.foundCity ? civAbility(city.owner).extraBuilderCharges : 0);
        // Venetian Arsenal (03: Wonders): a second naval melee, ranged or carrier unit (`made` is not used after this).
        if ((u.unitClass == "NAVAL_MELEE" || u.unitClass == "NAVAL_RANGED" || u.unitClass == "NAVAL_CARRIER") &&
            buildingsOwned(city.owner, "BUILDING_VENETIAN_ARSENAL") > 0) {
            if (auto again = unitSpawnPlot(city, item.type)) spawnUnit(item.type, city.owner, *again);
        }
        assignCitizens(city);
        refreshVisibility(city.owner);
    } else if (item.kind == ProductionKind::District) {
        for (CityDistrict& d : city.districts) {
            if (d.type != item.type) continue;
            d.complete = true;
            // Warrior Monks (06): a new Holy Site of the religion's founder claims the unowned plots around it.
            if (rules_->districts[static_cast<size_t>(d.type)].id == "DISTRICT_HOLY_SITE" && playerHasBelief(city.owner, Bf::WarriorMonks)) {
                for (const Hex& h : state_.grid.within(d.pos, 1)) {
                    Plot& q = state_.plot(h);
                    if (q.owner != kNoPlayer) continue;
                    q.owner = city.owner;
                    q.city = city.id;
                }
            }
        }
        questDone(city.owner, QuestKind::BuildDistrict, item.type);  // 08: Quests
    } else if (item.kind == ProductionKind::Project) {
        completeProject(city, item.type);
    } else {
        auto it = std::lower_bound(city.buildings.begin(), city.buildings.end(), item.type);
        if (it == city.buildings.end() || *it != item.type) {
            city.buildings.insert(it, item.type);
            city.wallHp += rules_->buildings[static_cast<size_t>(item.type)].outerDefenseHp;  // new walls stand at full HP
            if (!policyIs(city.owner, "POLICY_ROGUE_STATE"))  // Rogue State: no envoys (09)
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
    if (pj.spaceRace) competitionScore(city.owner, CompetitionKind::SpaceStation, 30);  // space station score project
    if (pj.id == "PROJECT_BUILD_TERRESTRIAL_LASER_STATION") ++city.laserStations;  // 09: +5 power demand each
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
            case ProjectEffectKind::Aid:
                if (const Competition* aid = runningAidRequest(); aid && aid->beneficiary != kNoPlayer) {
                    state_.players[static_cast<size_t>(aid->beneficiary)].gold += Fixed::fromInt(e.amount);
                    competitionScore(city.owner, aid->kind, e.amount);
                }
                break;
            case ProjectEffectKind::Recommission: city.reactorSince = state_.turn; break;
            case ProjectEffectKind::Convert:
                city.buildings.erase(std::remove_if(city.buildings.begin(), city.buildings.end(),
                                                    [&](TypeIndex b) { return rules_->buildings[static_cast<size_t>(b)].burnsResource != kNone; }),
                                     city.buildings.end());
                city.buildings.insert(std::lower_bound(city.buildings.begin(), city.buildings.end(), e.weapon), e.weapon);
                if (rules_->buildings[static_cast<size_t>(e.weapon)].id == "BUILDING_NUCLEAR_POWER_PLANT") city.reactorSince = state_.turn;
                break;
            case ProjectEffectKind::Competition: competitionScore(city.owner, static_cast<CompetitionKind>(e.weapon), e.amount); break;
            case ProjectEffectKind::Decommission:
                // The plant goes, and with it the city's burning of its fuel (09: Climate).
                city.buildings.erase(std::remove(city.buildings.begin(), city.buildings.end(), e.weapon), city.buildings.end());
                break;
            case ProjectEffectKind::Festival: {
                // Court Festival: Culture and tourism for each luxury copy beyond the first held.
                int surplus = 0;
                for (size_t r = 0; r < rules_->resources.size(); ++r) {
                    if (rules_->resources[r].cls == ResourceClass::Luxury) surplus += std::max(0, luxuryCopies(city.owner, static_cast<TypeIndex>(r)) - 1);
                }
                p.civics.overflow += Fixed::fromInt(e.amount * surplus);
                if (p.tourismTo.size() < state_.players.size()) p.tourismTo.resize(state_.players.size(), 0);
                for (const Player& o : state_.players) {
                    if (o.id != city.owner && isMajorCiv(o.id)) p.tourismTo[static_cast<size_t>(o.id)] += e.amount * surplus;
                }
                break;
            }
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
                                civAbility(pid).domainProductionPercent[static_cast<size_t>(rules_->units[static_cast<size_t>(item.type)].domain)] +
                                (item.formation > 0 ? 25 : 0) +  // the Military Academy or Seaport that trains it (05)
                                (goldenDedication(pid, "DEDICATION_TO_ARMS") && rules_->units[static_cast<size_t>(item.type)].layer == UnitLayer::Military ? 15 : 0) +  // 09
                                (rules_->units[static_cast<size_t>(item.type)].layer == UnitLayer::Military && militaryAllianceAtWar(pid) ? 15 : 0);  // 08
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
                if (goldenDedication(pid, "DEDICATION_HEARTBEAT_OF_STEAM") && era >= rules_->era("ERA_INDUSTRIAL")) prod = prod * 110 / 100;  // 09
                // Natural wonders (01): +50% toward a wonder built beside Ik-Kil.
                for (const CityWonder& w : city.wonders) {
                    if (w.building == item.type && nextToNaturalWonder(w.pos, "FEATURE_IK_KIL")) prod = prod * 150 / 100;
                }
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
                if (governmentIs(pid, "GOVERNMENT_SYNTHETIC_TECHNOCRACY")) pct += 30;  // 04: Synthetic Technocracy
                // Dark Age cards (09): Rogue State (nuclear projects), Automated Workforce (all projects).
                if (nuclear && policyIs(pid, "POLICY_ROGUE_STATE")) pct += 50;
                if (policyIs(pid, "POLICY_AUTOMATED_WORKFORCE")) pct += 20;
                prod = prod * pct / 100;
            }
            // Kilwa Kisiwani (03: Wonders): units by Militaristic suzerainties, buildings and districts by Industrial ones.
            if (item.kind != ProductionKind::Project) {
                if (const int kp = kilwaPercent(city, item.kind == ProductionKind::Unit ? CityStateKind::Militaristic : CityStateKind::Industrial); kp > 0)
                    prod = prod * (100 + kp) / 100;
            }
            // Policy cards (04): wonders by era, walls, districts and their buildings, space race projects.
            if (item.kind != ProductionKind::Unit)
                prod = prod * std::max<int64_t>(0, 100 + sumItemProductionPercent(state_, *rules_, city, item).toInt()) / 100;
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
