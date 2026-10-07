// Climate change and natural disasters (09-climate-disasters.md [GS]; data: climate-disasters.md).
// CO2 comes from strategic resources burned: unit maintenance (half their CO2) and power plants
// (1 fuel a turn each). Every two points of warming CO2 (Maps_XP2.CO2For1DegreeTempRise per map
// size) is one climate change point, half a degree. Climate phases I-VII arrive at cumulative
// points; they melt polar ice and raise the sea, flooding (pillaging) and then submerging
// coastal lowlands in three bands, which a Flood Barrier protects. Disasters roll each world
// turn from RANDOM_EVENT_START_TURN: each event's occurrences per game at the chosen intensity,
// spread over the turn limit and raised by warming. A disaster strikes an area, pillages or
// destroys improvements, hurts units, people and cities, and leaves fertile ground behind.
// A pillaged improvement waits for a Builder, as one pillaged in war does (05: Pillage); a
// pillaged district repairs itself (kPillagedDistrictTurns, a Sovereign reading).
#include <algorithm>
#include <optional>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
const UnitType& typeOf(const Rules& r, const Unit& u) { return r.units[at(u.type)]; }

// A stable per-plot number, so lowland bands and active volcanoes never move.
uint32_t plotHash(int32_t index, uint64_t salt) {
    uint64_t x = static_cast<uint64_t>(index) * 0x9E3779B97F4A7C15ull + salt;
    return static_cast<uint32_t>(splitmix64(x) >> 32);
}

// CO2 per resource burned (Resource_Consumption.CO2perkWh).
int64_t co2PerResource(const std::string& id) {
    if (id == "RESOURCE_COAL") return 820;
    if (id == "RESOURCE_OIL") return 490;
    if (id == "RESOURCE_URANIUM") return 48;
    return 0;
}

// Area radius of an event's hex count (1, 3, 7, 19 hexes).
int radiusOf(int hexes) { return hexes >= 19 ? 2 : hexes >= 3 ? 1 : 0; }
}  // namespace

// ------------------------------------------------------------------ queries

int Game::climateChangePoints() const {
    const TypeIndex size = rules_->mapSize(state_.setup.mapSize);
    const int64_t perDegree = size == kNone ? 2000000 : rules_->mapSizes[at(size)].co2PerDegree;
    const int64_t co2 = state_.co2 * (100 + deforestationPercent()) / 100;
    return static_cast<int>(std::min<int64_t>(1000, co2 * 2 / std::max<int64_t>(1, perDegree)));
}

namespace {
int countWoods(const GameState& s, const Rules& r) {
    const TypeIndex forest = r.feature("FEATURE_FOREST"), jungle = r.feature("FEATURE_JUNGLE");
    int n = 0;
    for (const Plot& p : s.plots) n += p.feature != kNone && (p.feature == forest || p.feature == jungle) ? 1 : 0;
    return n;
}
}  // namespace

// The share of woods lost sets a deforestation level (data: Light <=10% -1, Expected <=25% +1, Heavy <=40% +3,
// Extreme +5), and the level CO2's effect (Reduced -20%, Unchanged 0, Slight +10%, Increase +30%, Huge +50%).
// Sovereign reading: the level comes straight from the world's share lost, not a running average.
int Game::deforestationPercent() const {
    if (state_.woodsAtStart <= 0) return 0;
    const int lost = std::max(0, state_.woodsAtStart - countWoods(state_, *rules_)) * 100 / state_.woodsAtStart;
    const int level = lost <= 10 ? -1 : lost <= 25 ? 1 : lost <= 40 ? 3 : 5;
    return level <= 0 ? -20 : level <= 1 ? 0 : level <= 2 ? 10 : level <= 3 ? 30 : 50;
}

int Game::temperatureTenths() const { return climateChangePoints() * 5; }

int Game::lowlandBand(Hex h) const {
    const int32_t i = state_.grid.index(h);
    const Plot& p = state_.plot(h);
    const TerrainType& t = rules_->terrains[at(p.terrain)];
    if (t.water || t.relief != Relief::Flat || p.feature == rules_->feature("FEATURE_VOLCANO")) return 0;
    if (state_.cityAt(h) || state_.districtAt(h) || state_.wonderAt(h) != kNone) return 0;
    bool coastal = false;  // on the sea: rising water does not reach lakes (01: Lake)
    for (int d = 0; d < 6 && !coastal; ++d) {
        const auto n = state_.grid.neighbor(h, static_cast<Dir>(d));
        coastal = n && rules_->terrains[at(state_.plot(*n).terrain)].water && !isLake(state_, *rules_, *n);
    }
    if (!coastal) return 0;
    if (static_cast<int>(plotHash(i, 0x10A1A9D5u) % 100) >= rules_->globalInt("CLIMATE_CHANGE_PERCENT_COASTAL_LOWLANDS")) return 0;
    return 1 + static_cast<int>(plotHash(i, 0xBA9D5u) % 3);
}

bool Game::inDrought(Hex h) const {
    for (const GameState::Drought& d : state_.droughts) {
        if (d.turnsLeft > 0 && state_.grid.distance(d.center, h) <= d.radius) return true;
    }
    return false;
}

// ------------------------------------------------------------------ CO2

void Game::addCo2(PlayerId pid, int64_t amount) {
    if (amount <= 0) return;
    state_.co2 += amount;
    if (pid >= 0 && at(pid) < state_.players.size()) state_.players[at(pid)].co2 += amount;
}

void Game::unitCo2(PlayerId pid, size_t resource, int burned) {
    if (burned <= 0 || resource >= rules_->resources.size()) return;
    addCo2(pid, co2PerResource(rules_->resources[resource].id) * burned * rules_->globalInt("CLIMATE_CO2_PERCENT_FROM_UNITS") / 100);
}

// Power [GS] (09: Power), as the player's turn begins: each city's demand from its buildings is met
// by its own free sources (Hydroelectric Dam, renewables), then by the player's power plants within
// POWER_PLANT_RANGE of it, nearest first, burning just enough of their resource (each unit gives
// the plant's power; Coal and Oil 4, Uranium 16) and emitting its CO2.
void Game::burnPower(PlayerId pid) {
    Player& p = state_.players[at(pid)];
    // Buildings.RegionalRange; Mexico City's suzerain reaches 3 tiles farther from its Industrial Zones (08).
    const int plantRange = 6 + (suzerainBonus(pid, "CITYSTATE_MEXICO_CITY") ? 3 : 0);
    // Free power (data): Synthetic Technocracy +3 in every city; Cardiff's suzerain +2 per Lighthouse, Shipyard
    // and Seaport, while at peace with it.
    const bool technocracy = governmentIs(pid, "GOVERNMENT_SYNTHETIC_TECHNOCRACY");
    bool cardiff = false;
    for (const Player& cs : state_.players) {
        cardiff = cardiff || (cs.cityState != kNone && cs.alive && rules_->cityStates[at(cs.cityState)].id == "CITYSTATE_CARDIFF" &&
                              enjoysSuzerainBonus(state_, *rules_, pid, cs.cityState));
    }
    const TypeIndex harborBuildings[] = {rules_->building("BUILDING_LIGHTHOUSE"), rules_->building("BUILDING_SHIPYARD"), rules_->building("BUILDING_SEAPORT")};
    std::vector<City*> mine;
    for (City& c : state_.cities) {
        if (c.owner != pid) continue;
        c.powerDemand = c.powerSupply = 0;
        for (TypeIndex b : c.buildings) {
            const BuildingType& bt = rules_->buildings[at(b)];
            c.powerDemand += bt.requiredPower;
            c.powerSupply += bt.powerProvided;
        }
        if (technocracy) c.powerSupply += 3;
        if (policyIs(pid, "POLICY_AEROSPACE_CONTRACTORS") && c.district(rules_->district("DISTRICT_SPACEPORT"), true)) c.powerSupply += 3;  // 04
        c.powerDemand += 5 * c.laserStations;  // each Terrestrial Laser Station (09: Power)
        for (TypeIndex b : harborBuildings) c.powerSupply += cardiff && b != kNone && c.has(b) ? 2 : 0;
        for (const Hex& h : state_.grid.within(c.pos, 3)) {
            const Plot& pl = state_.plot(h);
            if (pl.city == c.id && pl.improvement != kNone && pl.pillagedTurns == 0) c.powerSupply += rules_->improvements[at(pl.improvement)].powerProvided;
        }
        mine.push_back(&c);
    }
    for (City* c : mine) {
        if (c->powerSupply >= c->powerDemand) continue;
        // The player's plants in reach, nearest first (ties: lower city id).
        std::vector<std::pair<int, const City*>> plants;
        for (const City* o : mine) {
            const int d = state_.grid.distance(o->pos, c->pos);
            if (d > plantRange) continue;
            for (TypeIndex b : o->buildings) {
                if (rules_->buildings[at(b)].burnsResource != kNone) plants.push_back({d, o});
            }
        }
        std::stable_sort(plants.begin(), plants.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [d, o] : plants) {
            if (c->powerSupply >= c->powerDemand) break;
            for (TypeIndex b : o->buildings) {
                const BuildingType& bt = rules_->buildings[at(b)];
                if (bt.burnsResource == kNone || bt.powerPerResource <= 0 || at(bt.burnsResource) >= p.stockpile.size()) continue;
                const int short_ = c->powerDemand - c->powerSupply;
                const int per = bt.powerPerResource + (cityGovernorHas(*o, "GOVERNOR_PROMOTION_INDUSTRIALIST") ? 1 : 0);  // Magnus
                const int units = std::min(p.stockpile[at(bt.burnsResource)], (short_ + per - 1) / per);
                if (units <= 0) continue;
                p.stockpile[at(bt.burnsResource)] -= units;
                c->powerSupply += units * per;
                awardFirst(pid, "MOMENT_FIRST_RESOURCE_CONSUMED_FOR_POWER_IN_WORLD", "MOMENT_FIRST_RESOURCE_CONSUMED_FOR_POWER", 0);  // 09
                addCo2(pid, co2PerResource(rules_->resources[at(bt.burnsResource)].id) * units);
            }
        }
    }
}

// ------------------------------------------------------------------ the world turn

void Game::processClimate() {
    if (state_.woodsAtStart < 0) state_.woodsAtStart = countWoods(state_, *rules_);
    // Storms move on, fires may spread (09: Natural disasters).
    {
        std::vector<GameState::Ongoing> running;
        running.swap(state_.ongoing);
        Rng& rng = state_.rng.get(RngStream::Gameplay);
        for (GameState::Ongoing o : running) {
            if (o.disaster < 0 || at(o.disaster) >= rules_->disasters.size() || --o.turnsLeft < 0) continue;
            const DisasterType& dt = rules_->disasters[at(o.disaster)];
            std::optional<Hex> next;
            if (dt.kind == DisasterKind::Fire) {
                int spread = 0;
                for (const DisasterDamage& dd : dt.damage) spread = dd.type == DisasterDamageType::Spread ? dd.percent : spread;
                const TypeIndex forest = rules_->feature("FEATURE_FOREST"), jungle = rules_->feature("FEATURE_JUNGLE");
                std::vector<Hex> woods;
                for (const Hex& h : state_.grid.within(o.center, 1)) {
                    const TypeIndex f = state_.plot(h).feature;
                    if (f != kNone && (f == forest || f == jungle)) woods.push_back(h);
                }
                if (woods.empty() || !rng.chance(static_cast<uint32_t>(spread))) {
                    if (!woods.empty()) state_.ongoing.push_back(o);  // still smouldering
                    continue;
                }
                next = woods[rng.below(static_cast<uint32_t>(woods.size()))];
            } else {
                next = state_.grid.neighbor(o.center, static_cast<Dir>(o.dir));
            }
            if (!next) continue;
            o.center = *next;
            strikeDisaster(o.disaster, o.center, true);
            if (o.turnsLeft > 0) state_.ongoing.push_back(o);
        }
    }
    // Droughts run down.
    for (GameState::Drought& d : state_.droughts) --d.turnsLeft;
    state_.droughts.erase(std::remove_if(state_.droughts.begin(), state_.droughts.end(),
                                         [](const GameState::Drought& d) { return d.turnsLeft <= 0; }),
                          state_.droughts.end());

    // Climate phases: each needs its own points on top of the ones before.
    const int points = climateChangePoints();
    int needed = 0;
    for (size_t k = 0; k < rules_->climatePhases.size(); ++k) {
        needed += rules_->climatePhases[k].points;
        const int phase = static_cast<int>(k) + 1;
        if (points < needed || state_.climatePhase >= phase) continue;
        state_.climatePhase = phase;
        pushEvent(EventKind::ClimatePhase, kNoPlayer, kNoPlayer, phase);
        const TypeIndex ice = rules_->feature("FEATURE_ICE");
        const TypeIndex coast = rules_->terrain("TERRAIN_COAST");
        const TypeIndex barrier = rules_->building("BUILDING_FLOOD_BARRIER");
        // Sea level: II floods 1 m lowlands, III 2 m, IV drowns 1 m, V floods 3 m, VI drowns 2 m, VII 3 m.
        static const int floods[8] = {0, 0, 1, 2, 0, 3, 0, 0};
        static const int drowns[8] = {0, 0, 0, 0, 1, 0, 2, 3};
        std::vector<CityId> reassign;
        for (int i = 0; i < state_.grid.size(); ++i) {
            const Hex h = state_.grid.at(i);
            Plot& p = state_.plot(h);
            if (ice != kNone && p.feature == ice &&
                static_cast<int>(plotHash(i, 0x1CEu) % 100) < rules_->climatePhases[k].iceLoss)
                p.feature = kNone;
            const int band = phase < 8 ? lowlandBand(h) : 0;
            if (band == 0 || (band != floods[phase] && band != drowns[phase])) continue;
            const City* owner = p.city == kNoCity ? nullptr : state_.city(p.city);
            if (owner && barrier != kNone && owner->has(barrier)) continue;
            if (band == floods[phase]) {
                if (p.improvement != kNone) p.pillagedTurns = kPillagedUntilRepaired;
                continue;
            }
            if (coast == kNone) continue;
            p.terrain = coast;
            p.feature = kNone;
            p.improvement = kNone;
            p.resource = kNone;
            p.route = -1;
            p.routePillaged = false;
            p.pillagedTurns = 0;
            p.fertility = {};
            std::vector<UnitId> lost;
            for (const Unit& u : state_.units) {
                if (u.pos == h && typeOf(*rules_, u).domain == Domain::Land && !isLeader(u)) lost.push_back(u.id);
            }
            for (UnitId id : lost) removeUnit(id);
            if (owner) reassign.push_back(owner->id);
        }
        for (CityId id : reassign) {
            City* c = state_.city(id);
            if (!c) continue;
            c->worked.clear();
            c->locked.clear();
            assignCitizens(*c);
        }
    }

    // Natural disasters.
    const int intensity = state_.setup.disasterIntensity;
    if (intensity < 0 || intensity >= kNumDisasterIntensities || state_.turn < rules_->globalInt("RANDOM_EVENT_START_TURN")) return;
    const int limit = std::max(1, turnLimit());
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    const int temp = temperatureTenths();
    for (size_t d = 0; d < rules_->disasters.size(); ++d) {
        const DisasterType& dt = rules_->disasters[d];
        const int freq = dt.frequencyTenths[at(intensity)];
        if (freq <= 0) continue;
        // A volcano natural wonder's eruption is rolled only on a map that has the wonder.
        if (dt.naturalWonder != kNone &&
            std::none_of(state_.plots.begin(), state_.plots.end(), [&](const Plot& p) { return p.feature == dt.naturalWonder; }))
            continue;
        // Occurrences per game spread over the turns, raised chancePerDegree % per degree.
        const int64_t odds = static_cast<int64_t>(freq) * (1000 + temp * dt.chancePerDegree);
        const int64_t scale = static_cast<int64_t>(limit) * 10 * 1000;
        if (static_cast<int64_t>(rng.below(static_cast<uint32_t>(std::min<int64_t>(scale, 0x7FFFFFFF)))) >= odds) continue;
        // Where it can strike.
        const DisasterIntensityType* setting = at(intensity) < rules_->disasterIntensities.size() ? &rules_->disasterIntensities[at(intensity)] : nullptr;
        std::vector<Hex> sites;
        for (int i = 0; i < state_.grid.size(); ++i) {
            const Hex h = state_.grid.at(i);
            const Plot& p = state_.plot(h);
            const TerrainType& t = rules_->terrains[at(p.terrain)];
            const std::string& feat = p.feature == kNone ? std::string() : rules_->features[at(p.feature)].id;
            bool ok = false;
            switch (dt.kind) {
                case DisasterKind::Flood: ok = feat.rfind("FEATURE_FLOODPLAINS", 0) == 0 && !cityPrevents(p.city, true); break;
                case DisasterKind::Eruption:
                    // An active volcano, or the volcano natural wonder the eruption belongs to (always active).
                    ok = dt.naturalWonder != kNone ? p.feature == dt.naturalWonder
                                                   : feat == "FEATURE_VOLCANO" && setting && static_cast<int>(plotHash(i, 0xE5u) % 100) < setting->activeVolcanoes;
                    break;
                case DisasterKind::Blizzard: ok = !t.water && (t.base == "TUNDRA" || t.base == "SNOW"); break;
                case DisasterKind::DustStorm: ok = !t.water && t.base == "DESERT"; break;
                case DisasterKind::Tornado: ok = !t.water && (t.base == "GRASSLAND" || t.base == "PLAINS"); break;
                case DisasterKind::Hurricane:
                    for (int k = 0; k < 6 && !t.water && !ok; ++k) {
                        const auto n = state_.grid.neighbor(h, static_cast<Dir>(k));
                        ok = n && rules_->terrains[at(state_.plot(*n).terrain)].water;
                    }
                    break;
                case DisasterKind::Drought: ok = !t.water && p.owner != kNoPlayer && !inDrought(h) && !cityPrevents(p.city, false); break;
                case DisasterKind::Fire: ok = feat == "FEATURE_FOREST" || feat == "FEATURE_JUNGLE"; break;
                case DisasterKind::Meteor: ok = !t.water && !t.impassable && !state_.cityAt(h); break;  // a meteor shower: any open land
                case DisasterKind::Nuclear: {
                    // The Industrial Zone of a city whose reactor is old enough (09: nuclear accidents).
                    const CityDistrict* zone = state_.districtAt(h);
                    const City* home = zone ? state_.city(p.city) : nullptr;
                    ok = home && rules_->districts[at(zone->type)].id == "DISTRICT_INDUSTRIAL_ZONE" && home->has(rules_->building("BUILDING_NUCLEAR_POWER_PLANT")) &&
                         state_.turn - home->reactorSince >= dt.minTurnAtRisk;
                    break;
                }
            }
            if (ok) sites.push_back(h);
        }
        if (sites.empty()) continue;
        strikeDisaster(static_cast<TypeIndex>(d), sites[rng.below(static_cast<uint32_t>(sites.size()))]);
    }
}

void Game::strikeDisaster(TypeIndex disaster, Hex center, bool follow) {
    const DisasterType& dt = rules_->disasters[at(disaster)];
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    const int intensity = std::clamp(state_.setup.disasterIntensity, 0, kNumDisasterIntensities - 1);
    const int extra = at(intensity) < rules_->disasterIntensities.size() ? rules_->disasterIntensities[at(intensity)].extraRange : 0;

    // The plots it strikes.
    std::vector<Hex> area;
    switch (dt.kind) {
        case DisasterKind::Flood:
            // The floodplains along the river near the center.
            for (const Hex& h : state_.grid.within(center, 2 + extra)) {
                const TypeIndex f = state_.plot(h).feature;
                if (f != kNone && rules_->features[at(f)].id.rfind("FEATURE_FLOODPLAINS", 0) == 0) area.push_back(h);
            }
            break;
        case DisasterKind::Eruption: area = state_.grid.within(center, 1 + extra); break;
        case DisasterKind::Drought: area = state_.grid.within(center, radiusOf(dt.hexes)); break;
        case DisasterKind::Nuclear:
            // The Industrial Zone, one ring more per severity (Sovereign reading; the data gives no radius).
            area = state_.grid.within(center, dt.severity);
            for (const Hex& h : area) state_.plot(h).fallout = static_cast<uint8_t>(std::max<int>(state_.plot(h).fallout, std::min(255, dt.fallout)));
            break;
        default: {
            const int radius = radiusOf(dt.hexes) + (dt.kind == DisasterKind::Fire ? 0 : extra);
            area = state_.grid.within(center, radius);
            if (dt.hexes == 3 && extra == 0 && area.size() > 3) {
                // Three plots: the center and two beside it.
                std::stable_partition(area.begin(), area.end(), [&](const Hex& h) { return h == center; });
                area.resize(3);
            }
            break;
        }
    }
    if (area.empty()) return;
    const PlayerId victim = state_.plot(center).owner;
    if (!follow) pushEvent(EventKind::Disaster, kNoPlayer, victim, static_cast<int>(disaster));
    // Storms move on for their duration; fires may spread (09: storms last 3 turns, a forest fire 9).
    const bool storm = dt.kind == DisasterKind::Blizzard || dt.kind == DisasterKind::DustStorm || dt.kind == DisasterKind::Tornado ||
                       dt.kind == DisasterKind::Hurricane;
    if (!follow && (storm || dt.kind == DisasterKind::Fire) && dt.duration > 1)
        state_.ongoing.push_back({disaster, center, static_cast<int8_t>(rng.below(kNumDirs)), dt.duration - 1});
    if (dt.kind == DisasterKind::Drought) {
        state_.droughts.push_back({center, radiusOf(dt.hexes), std::max(1, dt.duration)});
    }

    const TypeIndex volcanicSoil = rules_->feature("FEATURE_VOLCANIC_SOIL");
    const TypeIndex forest = rules_->feature("FEATURE_FOREST"), jungle = rules_->feature("FEATURE_JUNGLE");
    const TypeIndex burntForest = rules_->feature("FEATURE_BURNT_FOREST"), burntJungle = rules_->feature("FEATURE_BURNT_JUNGLE");
    // From phase IV storms and floods leave no fertility; from phase V storms and droughts may wash earlier fertility away.
    const bool weather = dt.kind != DisasterKind::Eruption && dt.kind != DisasterKind::Fire && dt.kind != DisasterKind::Meteor;
    const int removal = state_.climatePhase > 0 && at(state_.climatePhase - 1) < rules_->climatePhases.size()
                            ? rules_->climatePhases[at(state_.climatePhase - 1)].fertilityRemoval
                            : 0;
    const bool barren = weather && state_.climatePhase >= 4;
    std::vector<CityId> shrank;
    for (const Hex& h : area) {
        Plot& p = state_.plot(h);
        const TerrainType& t = rules_->terrains[at(p.terrain)];
        const City* atCity = state_.cityAt(h);
        City* city = atCity ? state_.city(atCity->id) : nullptr;
        // A Nilometer next to the plot halves flood damage there (Egypt).
        bool sheltered = false;
        if (dt.kind == DisasterKind::Flood) {
            for (const Hex& n : state_.grid.within(h, 1)) {
                const TypeIndex ni = state_.plot(n).improvement;
                sheltered = sheltered || (ni != kNone && rules_->improvements[static_cast<size_t>(ni)].halvesFloods);
            }
        }
        // The Great Bath (03): +1 Faith to its owner for each of their plots a flood reaches.
        if (dt.kind == DisasterKind::Flood && p.owner != kNoPlayer && buildingsOwned(p.owner, "BUILDING_GREAT_BATH") > 0)
            state_.players[at(p.owner)].faith += Fixed::fromInt(1);
        // Damage (none where Liang's Reinforced Materials guards the city).
        const City* guarded = p.city == kNoCity ? nullptr : state_.city(p.city);
        const bool reinforced = guarded && cityGovernorHas(*guarded, "GOVERNOR_PROMOTION_REINFORCED_MATERIALS");
        for (DisasterDamage dd : dt.damage) {
            if (reinforced) continue;
            if (sheltered) dd.percent /= 2;
            switch (dd.type) {
                case DisasterDamageType::ImprovementDestroyed:
                    if (p.improvement != kNone && rng.chance(static_cast<uint32_t>(dd.percent))) {
                        p.improvement = kNone;
                        p.pillagedTurns = 0;
                    }
                    break;
                case DisasterDamageType::ImprovementPillaged:
                    if (p.improvement != kNone && rng.chance(static_cast<uint32_t>(dd.percent))) p.pillagedTurns = kPillagedUntilRepaired;
                    break;
                case DisasterDamageType::PopulationLoss:
                    if (city && city->population > 1) {
                        const int lost = std::max(1, city->population * dd.percent / 100);
                        city->population = std::max(1, city->population - lost);
                        shrank.push_back(city->id);
                    }
                    break;
                case DisasterDamageType::CityGarrison:
                    if (city && city->hp > 0) city->hp = std::max(1, city->hp - rng.range(dd.minHp, std::max(dd.minHp, dd.maxHp)));
                    break;
                case DisasterDamageType::CityWalls:
                    if (city) city->wallHp = std::max(0, city->wallHp - rng.range(dd.minHp, std::max(dd.minHp, dd.maxHp)));
                    break;
                case DisasterDamageType::CivilianKilled:
                case DisasterDamageType::UnitDamageLand:
                case DisasterDamageType::UnitDamageNaval: {
                    std::vector<UnitId> lost;
                    for (Unit& u : state_.units) {
                        if (u.pos != h || isLeader(u)) continue;
                        const UnitType& ut = typeOf(*rules_, u);
                        const bool civilian = ut.combat == 0 && ut.domain == Domain::Land;
                        if (dd.type == DisasterDamageType::CivilianKilled) {
                            if (civilian && rng.chance(static_cast<uint32_t>(dd.percent))) lost.push_back(u.id);
                            continue;
                        }
                        const Domain want = dd.type == DisasterDamageType::UnitDamageLand ? Domain::Land : Domain::Sea;
                        if (civilian || ut.domain != want) continue;
                        if (dd.percent > 0 && !rng.chance(static_cast<uint32_t>(dd.percent))) continue;
                        u.hp -= rng.range(dd.minHp, std::max(dd.minHp, dd.maxHp));
                        if (u.hp <= 0) lost.push_back(u.id);
                    }
                    for (UnitId id : lost) removeUnit(id);
                    break;
                }
                case DisasterDamageType::DistrictPillaged:
                case DisasterDamageType::BuildingPillaged:
                case DisasterDamageType::BuildingDestroyed: {
                    // A district on the plot is pillaged (its buildings idle with it); a meltdown destroys its buildings.
                    const CityDistrict* here = state_.districtAt(h);
                    City* home = here ? state_.city(p.city) : nullptr;
                    if (!home || !rng.chance(static_cast<uint32_t>(std::min(100, dd.percent)))) break;
                    for (CityDistrict& d : home->districts) {
                        if (d.pos != h) continue;
                        if (dd.type == DisasterDamageType::BuildingDestroyed) {
                            const TypeIndex kind = d.type;
                            home->buildings.erase(std::remove_if(home->buildings.begin(), home->buildings.end(),
                                                                 [&](TypeIndex b) {
                                                                     const BuildingType& bt = rules_->buildings[at(b)];
                                                                     return !bt.wonder && bt.districtType == kind;
                                                                 }),
                                                  home->buildings.end());
                        } else if (d.complete && (dd.type == DisasterDamageType::DistrictPillaged ||
                                                  std::any_of(home->buildings.begin(), home->buildings.end(), [&](TypeIndex b) {
                                                      return rules_->buildings[at(b)].districtType == d.type;
                                                  }))) {
                            d.pillagedTurns = kPillagedDistrictTurns;
                        }
                    }
                    break;
                }
                case DisasterDamageType::Spread:
                case DisasterDamageType::Other: break;
            }
        }
        // Fertility left behind (on land only).
        if (t.water || t.impassable) continue;
        if (dt.kind == DisasterKind::Fire) {
            if (p.feature == forest && burntForest != kNone) p.feature = burntForest;
            else if (p.feature == jungle && burntJungle != kNone) p.feature = burntJungle;
        }
        if (weather && dt.kind != DisasterKind::Flood && removal > 0 && rng.chance(static_cast<uint32_t>(removal))) p.fertility = {};
        for (const DisasterFertility& f : dt.fertility) {
            if (barren) break;
            const bool soil = f.replaceFeature && f.feature == volcanicSoil && volcanicSoil != kNone;
            bool fits = f.feature == kNone || p.feature == f.feature;
            if (soil) fits = p.feature == kNone || p.feature == volcanicSoil;
            if (!fits || (h == center && dt.kind == DisasterKind::Eruption)) continue;
            if (!rng.chance(static_cast<uint32_t>(std::max(0, f.percent)))) continue;
            if (soil && !city) p.feature = volcanicSoil;
            int8_t& y = p.fertility[at(static_cast<int>(f.yield))];
            y = static_cast<int8_t>(std::min(4, y + f.amount));
        }
    }
    std::sort(shrank.begin(), shrank.end());
    shrank.erase(std::unique(shrank.begin(), shrank.end()), shrank.end());
    for (CityId id : shrank) {
        City* c = state_.city(id);
        if (!c) continue;
        c->worked.clear();
        c->locked.clear();
        assignCitizens(*c);
        requestAid(c->owner);  // 08: Aid Request (opens only when no competition runs)
    }
}

}  // namespace sov
