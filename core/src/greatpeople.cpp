// Great people and Great Works (07-economy-trade-great-people.md, Great People and Great
// Works and Culture; data: great-people.md). Every class offers one individual to everyone at
// a time; players earn points from districts and buildings, recruit when their points reach
// the cost (or buy with gold or faith), and use the unit where its requirements hold.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {

size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }

bool claimed(const GameState& s, TypeIndex person) {
    return at(person) < s.greatPeopleClaimed.size() && s.greatPeopleClaimed[at(person)];
}

bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }

}  // namespace

int Game::worldEra() const {
    std::vector<int> eras;
    for (const Player& p : state_.players) {
        if (isMajor(p)) eras.push_back(playerEra(p.id));
    }
    if (eras.empty()) return 0;
    std::sort(eras.begin(), eras.end(), std::greater<int>());
    return eras[(eras.size() + 1) / 2 - 1];  // reached by at least half of them
}

TypeIndex Game::currentGreatPerson(TypeIndex cls) const { return currentGreatPerson(cls, worldEra()); }

TypeIndex Game::currentGreatPerson(TypeIndex cls, int world) const {
    // Great Prophets stop once every religion the map allows is founded (06: Founding a religion).
    if (rules_->units[at(rules_->greatPersonClasses[at(cls)].unit)].foundReligion &&
        static_cast<int>(state_.religions.size()) >= maxReligions())
        return kNone;
    TypeIndex best = kNone;
    for (size_t i = 0; i < rules_->greatPeople.size(); ++i) {
        const GreatPersonType& g = rules_->greatPeople[i];
        if (g.cls != cls || g.era < world || claimed(state_, static_cast<TypeIndex>(i))) continue;
        if (best == kNone || g.era < rules_->greatPeople[at(best)].era) best = static_cast<TypeIndex>(i);
    }
    return best;
}

int Game::greatPersonCost(TypeIndex person) const { return greatPersonCost(person, worldEra()); }

int Game::greatPersonCost(TypeIndex person, int world) const {
    const GreatPersonType& g = rules_->greatPeople[at(person)];
    const int ahead = std::max(0, g.era - world);
    const int base = rules_->eras[at(static_cast<TypeIndex>(g.era))].greatPersonBaseCost * (100 + 30 * ahead) / 100;
    return std::max(1, base * speedPercent(state_, *rules_) / 100);
}

int Game::patronageCost(PlayerId player, TypeIndex cls, bool faith) const {
    if (cls < 0 || at(cls) >= rules_->greatPersonClasses.size()) return -1;
    const Player& p = state_.players[at(player)];
    const int world = worldEra();
    const TypeIndex person = currentGreatPerson(cls, world);
    if (person == kNone) return -1;
    const GreatPersonClass& c = rules_->greatPersonClasses[at(cls)];
    if (c.maxPerPlayer > 0 && at(cls) < p.greatPeopleRecruited.size() && p.greatPeopleRecruited[at(cls)] >= c.maxPerPlayer) return -1;
    const int have = at(cls) < p.greatPersonPoints.size() ? p.greatPersonPoints[at(cls)] : 0;
    const int missing = std::max(0, greatPersonCost(person, world) - have);
    // The fixed part does not scale with game speed (07: Patronage).
    // The Oracle (03: Wonders): patronage with Faith 25% cheaper.
    if (faith) return (150 + 10 * missing) * (buildingsOwned(player, "BUILDING_ORACLE") > 0 ? 75 : 100) / 100;
    return 200 + 15 * missing;
}

int Game::usedHere(const City& city, Gp g) const {
    const TypeIndex person = greatPeople_[static_cast<size_t>(g)];
    return person == kNone ? 0 : static_cast<int>(std::count(city.greatPeopleHere.begin(), city.greatPeopleHere.end(), person));
}

bool Game::usedBy(PlayerId player, Gp g) const {
    const TypeIndex person = greatPeople_[static_cast<size_t>(g)];
    const std::vector<TypeIndex>& used = state_.players[at(player)].greatPeopleActivated;
    return person != kNone && std::find(used.begin(), used.end(), person) != used.end();
}

bool Game::codedGreatPerson(TypeIndex person) const {
    return person != kNone && std::find(std::begin(greatPeople_), std::end(greatPeople_), person) != std::end(greatPeople_);
}

int Game::greatPersonEffectTotal(PlayerId player, GreatPersonEffectKind kind, TypeIndex ref) const {
    int total = 0;
    for (TypeIndex person : state_.players[at(player)].greatPeopleActivated) {
        for (const GreatPersonEffect& fx : rules_->greatPeople[at(person)].effects) {
            if (fx.kind == kind && (ref == kNone || fx.ref == ref)) total += std::max(1, fx.amount);
        }
    }
    // A wonder's lasting effects count while its owner holds it (Jebel Barkal's Iron).
    if (kind == GreatPersonEffectKind::ResourcePerTurn) {
        for (const City& c : state_.cities) {
            if (c.owner != player) continue;
            for (TypeIndex b : c.buildings) {
                for (const GreatPersonEffect& fx : rules_->buildings[at(b)].wonderEffects) {
                    if (fx.kind == kind && (ref == kNone || fx.ref == ref)) total += std::max(1, fx.amount);
                }
            }
        }
    }
    return total;
}

int Game::cityGreatPersonEffectTotal(const City& city, GreatPersonEffectKind kind) const {
    int total = 0;
    for (TypeIndex person : city.greatPeopleHere) {
        for (const GreatPersonEffect& fx : rules_->greatPeople[at(person)].effects) {
            if (fx.kind == kind) total += std::max(1, fx.amount);
        }
    }
    return total;
}

// Tesla and Paxton (07; 03: regional buildings): what the great people used on this city's district of that type add
// to its regional buildings: tiles of reach, or a yield or Amenities for each city they reach.
int Game::regionalBonus(const City& city, TypeIndex district, GreatPersonEffectKind kind, YieldType yield) const {
    int total = 0;
    for (TypeIndex person : city.greatPeopleHere) {
        const GreatPersonType& gp = rules_->greatPeople[at(person)];
        if (district == kNone || gp.district != district) continue;
        for (const GreatPersonEffect& fx : gp.effects) {
            if (fx.kind == kind && (kind != GreatPersonEffectKind::RegionalYield || fx.yield == yield)) total += fx.amount;
        }
    }
    return total;
}

int Game::greatPersonPointsPerTurn(PlayerId player, TypeIndex cls) const {
    int total = 0;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        int city = 0;
        // A Cultural alliance at level 2 (08): +1 from each district in a city with a route to the ally.
        bool toAlly = false;
        for (const TradeRoute& r : state_.tradeRoutes) {
            const City* dest = state_.city(r.destination);
            toAlly = toAlly || (r.origin == c.id && dest && alliance(player, dest->owner) == AllianceType::Cultural && allianceLevel(player, dest->owner) >= 2);
        }
        // The Oracle (03: Wonders): +2 from each district of its city that earns points.
        const int oracle = c.has(wonderType(W::Oracle)) ? 2 : 0;
        // A pillaged district and its buildings earn none (03).
        for (const CityDistrict& d : c.districts) {
            if (!d.complete || d.pillagedTurns > 0) continue;
            for (const auto& [k, v] : rules_->districts[at(d.type)].greatPersonPoints) city += k == cls ? v + (toAlly ? 1 : 0) + oracle : 0;
        }
        for (TypeIndex b : c.buildings) {
            if (buildingIdle(c, *rules_, b)) continue;
            for (const auto& [k, v] : rules_->buildings[at(b)].greatPersonPoints) city += k == cls ? v : 0;
        }
        city += static_cast<int>(sumCityGreatPersonPoints(state_, *rules_, c, cls).toInt());  // policy cards (04)
        // Grants: more points from the city (08: Governors).
        total += city * (100 + static_cast<int>(sumCityModifiers(state_, *rules_, c, ModEffect::CityGreatPersonPercent).toInt())) / 100;
    }
    total += static_cast<int>(sumPlayerGreatPersonPoints(state_, *rules_, state_.players[at(player)], cls).toInt());  // policy cards (04)
    // Leader ability: more points of a class (Sea Dogs: Great Admirals).
    for (const auto& [k, pct] : civAbility(player).greatPersonPercent) total = k == cls ? total * (100 + pct) / 100 : total;
    // Patronage (World Congress): double (A) or no (B) points for its class.
    if (const PassedResolution* pat = passed(ResolutionKind::Patronage); pat && pat->target == cls) total = pat->option == 0 ? total * 2 : 0;
    return total;
}

int Game::extraPalaceSlots(const City& city, TypeIndex building) const {
    return rules_->buildings[at(building)].id == "BUILDING_BANK" && usedBy(city.owner, Gp::Medici) ? 2 : 0;
}

int Game::greatWorkSlots(const City& city, const std::string& slot) const {
    int n = 0;
    for (TypeIndex b : city.buildings) {
        for (const auto& [s, count] : rules_->buildings[at(b)].greatWorkSlots) n += s == slot ? count : 0;
        if (slot == "PALACE") n += extraPalaceSlots(city, b);
    }
    return n;
}

TypeIndex Game::freeGreatWorkSlot(const City& city, TypeIndex workType) const {
    const GreatWorkType& w = rules_->greatWorkTypes[at(workType)];
    const bool palace = std::find(w.slots.begin(), w.slots.end(), "PALACE") != w.slots.end();
    for (TypeIndex b : city.buildings) {
        int free = palace ? extraPalaceSlots(city, b) : 0;
        for (const auto& [s, count] : rules_->buildings[at(b)].greatWorkSlots) {
            if (std::find(w.slots.begin(), w.slots.end(), s) != w.slots.end()) free += count;
        }
        for (const GreatWork& g : city.greatWorks) free -= g.building == b ? 1 : 0;
        if (free > 0) return b;
    }
    return kNone;
}

namespace {
// The aura of the strongest Great General or Admiral near a unit (05: Great Generals and Admirals).
const GreatPersonAura* bestAura(const GameState& s, const Rules& r, const Unit& unit) {
    const UnitType& ut = r.units[at(unit.type)];
    if (ut.layer != UnitLayer::Military) return nullptr;
    const GreatPersonAura* best = nullptr;
    for (const Unit& gp : s.units) {
        if (gp.owner != unit.owner || gp.greatPerson == kNone) continue;
        const GreatPersonType& g = r.greatPeople[at(gp.greatPerson)];
        if (!g.hasAura || g.aura.domain != ut.domain || s.grid.distance(gp.pos, unit.pos) > g.aura.range) continue;
        if (std::find(g.aura.eras.begin(), g.aura.eras.end(), ut.era) == g.aura.eras.end()) continue;
        if (!best || g.aura.strength > best->strength) best = &g.aura;
    }
    return best;
}
}  // namespace

int Game::greatPersonAuraStrength(const Unit& unit) const {
    const GreatPersonAura* a = bestAura(state_, *rules_, unit);
    return a ? a->strength : 0;
}

int Game::greatPersonAuraMoves(const Unit& unit) const {
    const GreatPersonAura* a = bestAura(state_, *rules_, unit);
    return a ? a->moves : 0;
}

void Game::processGreatPeople(PlayerId pid) {
    Player& p = state_.players[at(pid)];
    if (!isMajor(p) || rules_->greatPersonClasses.empty()) return;
    fitPlayerToRules(p, *rules_);
    int earned = 0;
    for (size_t c = 0; c < rules_->greatPersonClasses.size(); ++c) {
        int points = greatPersonPointsPerTurn(pid, static_cast<TypeIndex>(c));
        if (rules_->greatPersonClasses[c].id == "GREAT_PERSON_CLASS_PROPHET" && goldenDedication(pid, "DEDICATION_EXODUS_OF_THE_EVANGELISTS")) points += 4;  // 09
        p.greatPersonPoints[c] += points;
        if (rules_->greatPersonClasses[c].id != "GREAT_PERSON_CLASS_PROPHET") earned += points;
    }
    competitionScore(pid, CompetitionKind::WorldsFair, earned);  // great person points of the eight secular classes
    int world = worldEra();  // worked out again after each recruit
    for (size_t c = 0; c < rules_->greatPersonClasses.size(); ++c) {
        const TypeIndex person = currentGreatPerson(static_cast<TypeIndex>(c), world);
        if (person == kNone) continue;
        if (std::find(p.greatPeoplePassed.begin(), p.greatPeoplePassed.end(), person) != p.greatPeoplePassed.end()) continue;
        const GreatPersonClass& cls = rules_->greatPersonClasses[c];
        if (cls.maxPerPlayer > 0 && p.greatPeopleRecruited[c] >= cls.maxPerPlayer) continue;
        const int cost = greatPersonCost(person, world);
        if (p.greatPersonPoints[c] < cost) continue;
        p.greatPersonPoints[c] -= cost;
        recruitGreatPerson(pid, person);
        awardMoment(pid, rules_->greatPeople[at(person)].era < state_.gameEra ? "MOMENT_OLD_GREAT_PERSON_RECRUITED" : "MOMENT_GREAT_PERSON_RECRUITED");
        world = worldEra();
    }
}

void Game::recruitGreatPerson(PlayerId pid, TypeIndex person, const City* in) {
    const GreatPersonType& g = rules_->greatPeople[at(person)];
    const GreatPersonClass& cls = rules_->greatPersonClasses[at(g.cls)];
    Player& p = state_.players[at(pid)];
    // The great person appears in the capital (or any city when the capital is full), or in the city given.
    std::optional<Hex> spot = in ? unitSpawnPlot(*in, cls.unit) : std::nullopt;
    for (int pass = 0; pass < 2 && !spot; ++pass) {
        for (const City& c : state_.cities) {
            if (c.owner != pid || (pass == 0 && !c.capital)) continue;
            spot = unitSpawnPlot(c, cls.unit);
            if (spot) break;
        }
    }
    if (state_.greatPeopleClaimed.size() < rules_->greatPeople.size()) state_.greatPeopleClaimed.resize(rules_->greatPeople.size(), 0);
    state_.greatPeopleClaimed[at(person)] = 1;
    ++p.greatPeopleRecruited[at(g.cls)];
    questDone(pid, QuestKind::GreatPerson, g.cls);  // 08: Quests
    // The Nobel prizes count great people of their classes (08 [GS]).
    if (cls.id == "GREAT_PERSON_CLASS_WRITER" || cls.id == "GREAT_PERSON_CLASS_ARTIST" || cls.id == "GREAT_PERSON_CLASS_MUSICIAN")
        competitionScore(pid, CompetitionKind::NobelLiterature, 1);
    if (cls.id == "GREAT_PERSON_CLASS_SCIENTIST" || cls.id == "GREAT_PERSON_CLASS_ENGINEER" || cls.id == "GREAT_PERSON_CLASS_MERCHANT")
        competitionScore(pid, CompetitionKind::NobelPhysics, 1);
    if (cls.id == "GREAT_PERSON_CLASS_SCIENTIST") greatLibraryEurekas(pid);
    pushEvent(EventKind::GreatPersonRecruited, pid, kNoPlayer, person);
    dedicationScore(pid, "DEDICATION_SKY_AND_STARS", 1);  // 09: a great person earned
    if (!spot) return;  // no city to appear in: the great person is lost
    Unit& u = spawnUnit(cls.unit, pid, *spot);
    u.greatPerson = person;
    u.charges = g.greatWorkCount > 0 ? g.greatWorkCount : g.charges;
    // Mausoleum at Halicarnassus (03): Great Engineers have a charge more.
    if (cls.id == "GREAT_PERSON_CLASS_ENGINEER" && g.greatWorkCount == 0 && buildingsOwned(pid, "BUILDING_MAUSOLEUM_AT_HALICARNASSUS") > 0) ++u.charges;
}

// Stonehenge (03; data: its two grants): the civ's next Great Prophet, its one, while it can still earn one
// (Prophets remain, it has founded no religion and has not had its Prophet); otherwise an Apostle of its
// religion, or of the city's.
void Game::wonderProphet(City& city, TypeIndex prophet) {
    Player& p = state_.players[at(city.owner)];
    fitPlayerToRules(p, *rules_);
    TypeIndex cls = kNone;
    for (size_t c = 0; c < rules_->greatPersonClasses.size() && cls == kNone; ++c) {
        if (rules_->greatPersonClasses[c].unit == prophet) cls = static_cast<TypeIndex>(c);
    }
    const TypeIndex person = cls != kNone ? currentGreatPerson(cls) : kNone;
    if (person != kNone && p.religion < 0) {
        const int most = rules_->greatPersonClasses[at(cls)].maxPerPlayer;
        if (most <= 0 || p.greatPeopleRecruited[at(cls)] < most) {
            recruitGreatPerson(city.owner, person, &city);
            return;
        }
    }
    const TypeIndex apostle = rules_->unit("UNIT_APOSTLE");
    const int religion = p.religion >= 0 ? p.religion : cityMajorityReligion(city);
    if (apostle == kNone || religion < 0) return;
    const std::optional<Hex> spot = unitSpawnPlot(city, apostle);
    if (!spot) return;
    Unit& u = spawnUnit(apostle, city.owner, *spot);
    u.religion = static_cast<int16_t>(religion);
    u.charges = rules_->units[at(apostle)].spreadCharges;
    grantApostlePromotion(u);  // each new Apostle gets one (06)
}

// The Great Library (03): whenever another civ recruits a Great Scientist, a random Eureka toward a tech
// it has neither researched nor boosted.
void Game::greatLibraryEurekas(PlayerId recruiter) {
    for (Player& o : state_.players) {
        if (o.id == recruiter || !isMajor(o) || buildingsOwned(o.id, "BUILDING_GREAT_LIBRARY") == 0) continue;
        std::vector<TypeIndex> open;
        for (size_t t = 0; t < rules_->techs.size(); ++t) {
            if (!o.techs.done[t] && !o.techs.boosted[t]) open.push_back(static_cast<TypeIndex>(t));
        }
        if (open.empty()) continue;
        const TypeIndex t = open[state_.rng.get(RngStream::Gameplay).below(static_cast<uint32_t>(open.size()))];
        const int pct = rules_->techs[at(t)].boost.percent > 0 ? rules_->techs[at(t)].boost.percent : 40;
        o.techs.boosted[at(t)] = 1;
        o.techs.progress[at(t)] += Fixed::fromInt(techCost(t)) * pct / 100;
    }
}

TypeIndex Game::unfinishedWonderAt(const City& city, Hex plot) const {
    for (const CityWonder& w : city.wonders) {
        if (w.pos == plot && !city.has(w.building)) return w.building;
    }
    return kNone;
}

bool Game::canActivateGreatPerson(UnitId id, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    const Unit* u = state_.unit(id);
    if (!u) return fail(CommandError::BadUnit);
    if (u->greatPerson == kNone || u->charges <= 0) return fail(CommandError::CannotActivate);
    const GreatPersonType& g = rules_->greatPeople[at(u->greatPerson)];
    const Plot& plot = state_.plot(u->pos);
    const City* city = plot.city != kNoCity ? state_.city(plot.city) : nullptr;
    if (city && city->owner != u->owner) city = nullptr;
    if (g.greatWorkCount > 0) {
        // A Great Work goes into a free slot of the city whose land the great person stands on.
        if (!city || freeGreatWorkSlot(*city, g.greatWorkType) == kNone) return fail(CommandError::CannotActivate);
        if (why) *why = CommandError::Ok;
        return true;
    }
    if (g.effects.empty() && !g.hasModifiers && !codedGreatPerson(u->greatPerson)) return fail(CommandError::CannotActivate);  // its effects need systems not built yet
    // Crassus: on an unowned plot beside the player's land, which he claims.
    if (u->greatPerson == greatPeople_[static_cast<size_t>(Gp::Crassus)]) {
        bool beside = false;
        for (const Hex& n : state_.grid.within(u->pos, 1)) beside = beside || (state_.plot(n).owner == u->owner && state_.plot(n).city != kNoCity);
        if (plot.owner != kNoPlayer || !beside) return fail(CommandError::CannotActivate);
    }
    if (g.ownedTile && plot.owner != u->owner) return fail(CommandError::CannotActivate);
    if (g.district != kNone) {
        const bool center = rules_->districts[at(g.district)].id == "DISTRICT_CITY_CENTER";
        const CityDistrict* d = state_.districtAt(u->pos);
        const bool ok = center ? (state_.cityAt(u->pos) && state_.cityAt(u->pos)->owner == u->owner)
                               : (city && d && d->type == g.district && d->complete);
        if (!ok) return fail(CommandError::CannotActivate);
    }
    if (g.noMilitaryUnit && state_.unitAt(u->pos, UnitLayer::Military, *rules_)) return fail(CommandError::CannotActivate);
    if (g.unitDomain >= 0) {
        const Unit* m = state_.unitAt(u->pos, UnitLayer::Military, *rules_);
        if (!m || m->owner != u->owner || static_cast<int>(rules_->units[at(m->type)].domain) != g.unitDomain || (g.standardFormation && m->formation != 0))
            return fail(CommandError::CannotActivate);
    }
    if (g.missingBuilding != kNone && (!city || city->has(g.missingBuilding))) return fail(CommandError::CannotActivate);
    // Magellan, Colaeus (07): on a luxury the player can see.
    if (g.luxuryHere && (plot.resource == kNone || rules_->resources[at(plot.resource)].cls != ResourceClass::Luxury || !resourceVisible(u->owner, u->pos)))
        return fail(CommandError::CannotActivate);
    // Whose land it is (Tupac Amaru in an enemy's, Perry and Zhou Daguan in a city-state's, not a foe's).
    if (g.enemyTerritory && (plot.owner == kNoPlayer || !atWar(u->owner, plot.owner))) return fail(CommandError::CannotActivate);
    if (g.cityStateTerritory && (plot.owner == kNoPlayer || !isCityState(plot.owner))) return fail(CommandError::CannotActivate);
    if (g.suzerainTerritory && (plot.owner == kNoPlayer || !isCityState(plot.owner) || !isSuzerain(u->owner, plot.owner) || atWar(u->owner, plot.owner)))
        return fail(CommandError::CannotActivate);
    if (g.nonHostileTerritory && plot.owner != kNoPlayer && atWar(u->owner, plot.owner)) return fail(CommandError::CannotActivate);
    // What lies beside it: a barbarian (Boudica), a Mountain (Galileo), a natural wonder (Darwin), Rainforest (Janaki Ammal).
    if (g.barbarianBeside || g.mountainBeside || g.naturalWonderNear || g.featureNear != kNone) {
        bool barbarian = false, mountain = false, wonder = false, feature = false;
        for (const Hex& h : state_.grid.within(u->pos, 1)) {
            const Plot& q = state_.plot(h);
            mountain = mountain || (h != u->pos && rules_->terrains[at(q.terrain)].relief == Relief::Mountain);
            wonder = wonder || (q.feature != kNone && rules_->features[at(q.feature)].naturalWonder);
            feature = feature || (g.featureNear != kNone && q.feature == g.featureNear);
        }
        for (const Unit& o : state_.units) barbarian = barbarian || (state_.players[at(o.owner)].barbarian && state_.grid.distance(o.pos, u->pos) <= 1);
        if ((g.barbarianBeside && !barbarian) || (g.mountainBeside && !mountain) || (g.naturalWonderNear && !wonder) || (g.featureNear != kNone && !feature))
            return fail(CommandError::CannotActivate);
    }
    // The Great Engineers: on the plot of a wonder its city is building. Korolev, Sagan: its city is building a space race project.
    if (g.incompleteWonder && (!city || unfinishedWonderAt(*city, u->pos) == kNone)) return fail(CommandError::CannotActivate);
    if (g.spaceRaceProject && (!city || city->queue.empty() || city->queue.front().kind != ProductionKind::Project ||
                               !rules_->projects[at(city->queue.front().type)].spaceRace))
        return fail(CommandError::CannotActivate);
    // Mary Leakey: its city holds an Artifact. Jeanne d'Arc: some city of the player has room for a Relic.
    if (g.cityGreatWork != kNone &&
        (!city || std::none_of(city->greatWorks.begin(), city->greatWorks.end(), [&](const GreatWork& w) { return w.type == g.cityGreatWork; })))
        return fail(CommandError::CannotActivate);
    if (g.relicSlot) {
        const TypeIndex relic = rules_->greatWorkType("RELIC");
        if (relic == kNone || std::none_of(state_.cities.begin(), state_.cities.end(), [&](const City& c) { return c.owner == u->owner && freeGreatWorkSlot(c, relic) != kNone; }))
            return fail(CommandError::CannotActivate);
    }
    // Effects that need a city (buildings, production) need one here.
    for (const GreatPersonEffect& fx : g.effects) {
        if ((fx.kind == GreatPersonEffectKind::Building || fx.kind == GreatPersonEffectKind::Production) && !city)
            return fail(CommandError::CannotActivate);
    }
    if (why) *why = CommandError::Ok;
    return true;
}

CommandError Game::validateGreatPeople(const Command& c) const {
    if (c.type == CommandType::ActivateGreatPerson) {
        const Unit* u = state_.unit(c.id);
        if (!u) return CommandError::BadUnit;
        if (u->owner != c.player) return CommandError::NotYourUnit;
        CommandError why = CommandError::Ok;
        canActivateGreatPerson(c.id, &why);
        return why;
    }
    if (c.arg < 0 || static_cast<size_t>(c.arg) >= rules_->greatPersonClasses.size()) return CommandError::NoGreatPerson;
    const TypeIndex cls = static_cast<TypeIndex>(c.arg);
    if (currentGreatPerson(cls) == kNone) return CommandError::NoGreatPerson;
    if (c.type == CommandType::PassGreatPerson) return CommandError::Ok;
    const int cost = patronageCost(c.player, cls, c.arg2 == 1);
    if (cost < 0) return CommandError::NoGreatPerson;
    const Player& p = state_.players[at(c.player)];
    if (c.arg2 == 1 ? p.faith < Fixed::fromInt(cost) : p.gold < Fixed::fromInt(cost))
        return c.arg2 == 1 ? CommandError::NotEnoughFaith : CommandError::NotEnoughGold;
    return CommandError::Ok;
}

void Game::applyGreatPeople(const Command& c) {
    Player& p = state_.players[at(c.player)];
    fitPlayerToRules(p, *rules_);
    if (c.type == CommandType::PassGreatPerson) {
        p.greatPeoplePassed.push_back(currentGreatPerson(static_cast<TypeIndex>(c.arg)));
        return;
    }
    if (c.type == CommandType::PatronizeGreatPerson) {
        const TypeIndex cls = static_cast<TypeIndex>(c.arg);
        const TypeIndex person = currentGreatPerson(cls);
        const int cost = patronageCost(c.player, cls, c.arg2 == 1);
        if (c.arg2 == 1) p.faith -= Fixed::fromInt(cost);
        else p.gold -= Fixed::fromInt(cost);
        // Points already earned count toward the price; what is left over stays.
        p.greatPersonPoints[at(cls)] = std::max(0, p.greatPersonPoints[at(cls)] - greatPersonCost(person));
        recruitGreatPerson(c.player, person);
        awardMoment(c.player, c.arg2 == 1 ? "MOMENT_GREAT_PERSON_LURED_BY_FAITH" : "MOMENT_GREAT_PERSON_LURED_BY_GOLD");
        return;
    }
    // Activation: a Great Work, or the individual's effects; the last charge spends the unit.
    Unit* u = state_.unit(c.id);
    const GreatPersonType& g = rules_->greatPeople[at(u->greatPerson)];
    if (g.greatWorkCount > 0) {
        City& city = *state_.city(state_.plot(u->pos).city);
        city.greatWorks.push_back({g.greatWorkType, freeGreatWorkSlot(city, g.greatWorkType), u->greatPerson});
    } else {
        for (const GreatPersonEffect& fx : g.effects) {
            applyGreatPersonEffect(*u, fx);
            u = state_.unit(c.id);  // a granted unit may move the unit list
        }
        // Effects in code: Crassus claims the plot for the neighbouring city; Hildegard gives Science equal to the Holy Site's Faith adjacency.
        if (u->greatPerson == greatPeople_[static_cast<size_t>(Gp::Crassus)]) {
            for (const Hex& n : state_.grid.within(u->pos, 1)) {
                const Plot& q = state_.plot(n);
                if (q.owner != c.player || q.city == kNoCity) continue;
                state_.plot(u->pos).owner = c.player;
                state_.plot(u->pos).city = q.city;
                break;
            }
        }
        if (u->greatPerson == greatPeople_[static_cast<size_t>(Gp::Hildegard)]) {
            if (const CityDistrict* d = state_.districtAt(u->pos))
                processResearch(c.player, districtAdjacency(c.player, d->type, d->pos)[static_cast<size_t>(YieldType::Faith)], Fixed());
        }
        // Its lasting effects: the player's, and the city's where it was used (07).
        p.greatPeopleActivated.push_back(u->greatPerson);
        if (City* here = state_.city(state_.plot(u->pos).city); here && here->owner == c.player) here->greatPeopleHere.push_back(u->greatPerson);
    }
    // Vatican City (08: suzerain): 400 pressure of the player's religion on the cities within 10 tiles.
    if (suzerainBonus(c.player, Cs::VaticanCity)) shiftPressure(u->pos, 10, civReligion(c.player), 400);
    if (--u->charges <= 0) removeUnit(c.id);
    refreshVisibility(c.player);
}

void Game::applyGreatPersonEffect(Unit& unit, const GreatPersonEffect& fx) {
    const PlayerId pid = unit.owner;
    const Hex here = unit.pos;
    City* city = state_.plot(here).city != kNoCity ? state_.city(state_.plot(here).city) : nullptr;
    if (city && city->owner != pid) city = nullptr;
    applyEffectAt(pid, city, here, fx);
}

void Game::applyEffectAt(PlayerId pid, City* city, Hex here, const GreatPersonEffect& fx) {
    Player& p = state_.players[at(pid)];
    const int speed = speedPercent(state_, *rules_);
    switch (fx.kind) {
        case GreatPersonEffectKind::Yield: {
            const Fixed amount = Fixed::fromInt(fx.scaled ? fx.amount * speed / 100 : fx.amount);
            if (fx.yield == YieldType::Gold) p.gold += amount;
            else if (fx.yield == YieldType::Faith) p.faith += amount;
            else if (fx.yield == YieldType::Science) processResearch(pid, amount, Fixed());
            else if (fx.yield == YieldType::Culture) processResearch(pid, Fixed(), amount);
            break;
        }
        case GreatPersonEffectKind::Production: {
            // Into the wonder being built on this plot (the Great Engineers), else what the city builds now.
            if (!city) break;
            const TypeIndex wonder = unfinishedWonderAt(*city, here);
            if (wonder == kNone && city->queue.empty()) break;
            const ProductionItem item = wonder != kNone ? ProductionItem{ProductionKind::Building, wonder, 0} : city->queue.front();
            bool found = false;
            for (ProductionProgress& pr : city->progress) {
                if (pr.item.kind == item.kind && pr.item.type == item.type) {
                    pr.amount += Fixed::fromInt(fx.amount * speed / 100);
                    found = true;
                }
            }
            if (!found) city->progress.push_back({item, Fixed::fromInt(fx.amount * speed / 100)});
            break;
        }
        case GreatPersonEffectKind::Boost:
        case GreatPersonEffectKind::RandomBoost: {
            const bool civic = fx.civic;
            TreeProgress& tree = civic ? p.civics : p.techs;
            const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
            auto boost = [&](TypeIndex node, bool orComplete) {
                const int cost = civic ? civicCost(node) : techCost(node);
                if (tree.done[at(node)]) return;
                if (tree.boosted[at(node)]) {
                    if (orComplete) {
                        tree.progress[at(node)] = Fixed::fromInt(cost);
                        completeNode(pid, civic, node);
                    }
                    return;
                }
                tree.boosted[at(node)] = 1;
                const int pct = nodes[at(node)].boost.percent > 0 ? nodes[at(node)].boost.percent : 40;
                tree.progress[at(node)] += Fixed::fromInt(cost) * pct / 100;
            };
            if (fx.kind == GreatPersonEffectKind::Boost) {
                boost(fx.ref, fx.orComplete);
                break;
            }
            std::vector<TypeIndex> pool;
            for (size_t i = 0; i < nodes.size(); ++i) {
                if (nodes[i].era >= fx.minEra && nodes[i].era <= fx.maxEra && !tree.done[i] && !tree.boosted[i])
                    pool.push_back(static_cast<TypeIndex>(i));
            }
            Rng& rng = state_.rng.get(RngStream::Gameplay);
            for (int k = 0; k < fx.count && !pool.empty(); ++k) {
                const size_t pick = rng.below(static_cast<uint32_t>(pool.size()));
                boost(pool[pick], false);
                pool.erase(pool.begin() + static_cast<long>(pick));
            }
            break;
        }
        case GreatPersonEffectKind::PromotionXp: {
            for (Unit& m : state_.units) {
                if (m.pos == here && m.owner == pid && rules_->units[at(m.type)].layer == UnitLayer::Military) {
                    m.xp = std::max(m.xp, xpForNextLevel(m));
                    break;
                }
            }
            break;
        }
        case GreatPersonEffectKind::Building: {
            if (!city || city->has(fx.ref)) break;
            city->buildings.push_back(fx.ref);
            std::sort(city->buildings.begin(), city->buildings.end());
            break;
        }
        case GreatPersonEffectKind::Unit: {
            std::optional<Hex> spot;
            if (city) spot = unitSpawnPlot(*city, fx.ref);
            if (!spot && !state_.unitAt(here, rules_->units[at(fx.ref)].layer, *rules_)) spot = here;
            if (spot) spawnUnit(fx.ref, pid, *spot);
            break;
        }
        case GreatPersonEffectKind::BuildingYield:
        case GreatPersonEffectKind::Ability:
        case GreatPersonEffectKind::TradeRoutes:
        case GreatPersonEffectKind::ResourcePerTurn:
        case GreatPersonEffectKind::DistrictCapacity:
        case GreatPersonEffectKind::Ocean:
        case GreatPersonEffectKind::ArtifactTourism:
        case GreatPersonEffectKind::RegionalRange:
        case GreatPersonEffectKind::RegionalYield:
        case GreatPersonEffectKind::RegionalAmenity: break;  // lasting: read from greatPeopleActivated or greatPeopleHere
        case GreatPersonEffectKind::Envoys:
            if (!policyIs(pid, "POLICY_ROGUE_STATE")) p.envoyTokens += fx.amount;  // Rogue State: no envoys (09)
            break;
        case GreatPersonEffectKind::EnvoysHere: {
            const PlayerId cs = state_.plot(here).owner;
            if (cs == kNoPlayer || !isCityState(cs) || policyIs(pid, "POLICY_ROGUE_STATE")) break;
            if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
            p.envoys[at(cs)] += fx.amount;
            break;
        }
        case GreatPersonEffectKind::GovernorTitles: p.governorTitlesSpent -= fx.amount; break;
        case GreatPersonEffectKind::LuxuryHere: {
            const TypeIndex lux = state_.plot(here).resource;
            if (lux == kNone || rules_->resources[at(lux)].cls != ResourceClass::Luxury) break;
            for (int k = 0; k < std::max(1, fx.amount); ++k) p.luxuryGrants.push_back(lux);
            break;
        }
        case GreatPersonEffectKind::RandomCivics: {
            Rng& rng = state_.rng.get(RngStream::Gameplay);
            for (int k = 0; k < fx.count; ++k) {
                const std::vector<TypeIndex> open = availableCivics(pid);
                if (open.empty()) break;
                const TypeIndex t = open[rng.below(static_cast<uint32_t>(open.size()))];
                p.civics.progress[at(t)] = Fixed::fromInt(civicCost(t));
                completeNode(pid, true, t);
            }
            break;
        }
        case GreatPersonEffectKind::DiplomaticVp: p.diplomaticVictoryPoints += fx.amount; break;
        case GreatPersonEffectKind::Population:
            for (City& c : state_.cities) {
                if (c.owner == pid) c.population += fx.amount;
            }
            break;
        case GreatPersonEffectKind::PromoteAll:
            for (Unit& m : state_.units) {
                if (m.owner == pid && rules_->units[at(m.type)].layer == UnitLayer::Military) m.xp = std::max(m.xp, xpForNextLevel(m));
            }
            break;
        case GreatPersonEffectKind::TreasuryPercent:
            if (p.gold > Fixed()) p.gold += p.gold * fx.amount / 100;
            break;
        case GreatPersonEffectKind::Relic: {
            const TypeIndex relic = rules_->greatWorkType("RELIC");
            for (int k = 0; k < fx.amount && relic != kNone; ++k) {
                for (City& c : state_.cities) {
                    if (c.owner != pid) continue;
                    const TypeIndex slot = freeGreatWorkSlot(c, relic);
                    if (slot == kNone) continue;
                    GreatWork w;
                    w.type = relic;
                    w.building = slot;
                    c.greatWorks.push_back(w);
                    break;
                }
            }
            break;
        }
        case GreatPersonEffectKind::RandomTechs: {
            Rng& rng = state_.rng.get(RngStream::Gameplay);
            for (int k = 0; k < fx.count; ++k) {
                const std::vector<TypeIndex> open = availableTechs(pid);
                if (open.empty()) break;
                const TypeIndex t = open[rng.below(static_cast<uint32_t>(open.size()))];
                p.techs.progress[at(t)] = Fixed::fromInt(techCost(t));
                completeNode(pid, false, t);
            }
            break;
        }
        case GreatPersonEffectKind::Formation: {
            Unit* m = nullptr;
            for (Unit& o : state_.units) {
                if (o.pos == here && o.owner == pid && rules_->units[at(o.type)].layer == UnitLayer::Military) m = &o;
            }
            if (m) m->formation = static_cast<uint8_t>(std::max<int>(m->formation, fx.amount));
            break;
        }
        case GreatPersonEffectKind::UnitsInDistricts: {
            // Tupac Amaru (07): in each finished district of the city whose land he stands on, an enemy's.
            City* target = state_.plot(here).city != kNoCity ? state_.city(state_.plot(here).city) : nullptr;
            if (!target) break;
            std::vector<Hex> spots;
            for (const CityDistrict& d : target->districts) {
                if (d.complete) spots.push_back(d.pos);
            }
            for (const Hex& h : spots) {
                if (!state_.unitAt(h, rules_->units[at(fx.ref)].layer, *rules_)) spawnUnit(fx.ref, pid, h);
            }
            break;
        }
        case GreatPersonEffectKind::NavalMeleeUnit: {
            // The most advanced naval melee unit the player can train (its civ's unique one where it has it).
            const TypeIndex best = bestUnitOfClass(pid, "NAVAL_MELEE");
            if (best == kNone) break;
            std::optional<Hex> spot;
            if (city) spot = unitSpawnPlot(*city, best);
            if (spot) spawnUnit(best, pid, *spot);
            break;
        }
        case GreatPersonEffectKind::ScienceAdjacent: {
            int n = 0;
            for (const Hex& h : state_.grid.within(here, 1)) {
                if (h == here) continue;
                const Plot& pl = state_.plot(h);
                if (fx.what == "MOUNTAIN") n += rules_->terrains[at(pl.terrain)].relief == Relief::Mountain ? 1 : 0;
                else n += pl.feature != kNone && rules_->features[at(pl.feature)].id == fx.what ? 1 : 0;
            }
            processResearch(pid, Fixed::fromInt(fx.amount * speed / 100 * n), Fixed());
            break;
        }
        case GreatPersonEffectKind::SciencePerArtifact: {
            if (!city) break;
            const TypeIndex artifact = rules_->greatWorkType("ARTIFACT");
            const int n = static_cast<int>(std::count_if(city->greatWorks.begin(), city->greatWorks.end(), [&](const GreatWork& w) { return w.type == artifact; }));
            processResearch(pid, Fixed::fromInt(fx.amount * speed / 100 * n), Fixed());
            break;
        }
        case GreatPersonEffectKind::ScienceNearWonder: {
            bool near = false;
            for (const Hex& h : state_.grid.within(here, 1)) {
                const Plot& pl = state_.plot(h);
                near = near || (pl.feature != kNone && rules_->features[at(pl.feature)].naturalWonder);
            }
            if (near) processResearch(pid, Fixed::fromInt(fx.amount * speed / 100), Fixed());
            break;
        }
        case GreatPersonEffectKind::WonderProduction: {
            // Imhotep (07): into the wonder being built on this plot, else the wonder the city builds now.
            if (!city) break;
            const TypeIndex wonder = unfinishedWonderAt(*city, here);
            if (wonder == kNone && city->queue.empty()) break;
            const ProductionItem item = wonder != kNone ? ProductionItem{ProductionKind::Building, wonder, 0} : city->queue.front();
            if (item.kind != ProductionKind::Building || !rules_->buildings[at(item.type)].wonder) break;
            const BuildingType& b = rules_->buildings[at(item.type)];
            const int era = b.unlock.none() ? 0 : (b.unlock.civic ? rules_->civics : rules_->techs)[at(b.unlock.index)].era;
            const int amount = (era >= fx.minEra && era <= fx.maxEra ? fx.amount : fx.count) * speed / 100;
            bool found = false;
            for (ProductionProgress& pr : city->progress) {
                if (pr.item == item) {
                    pr.amount += Fixed::fromInt(amount);
                    found = true;
                }
            }
            if (!found) city->progress.push_back({item, Fixed::fromInt(amount)});
            break;
        }
        case GreatPersonEffectKind::WonderPurchase: {
            // Shah Jahan (07; Civilopedia: "Grants Production towards wonder construction, capped at half your current
            // treasury, then reduces your Gold twice the amount of purchased Production"): the rest of the wonder here.
            if (!city) break;
            const TypeIndex wonder = unfinishedWonderAt(*city, here);
            if (wonder == kNone) break;
            const ProductionItem item{ProductionKind::Building, wonder, 0};
            Fixed done;
            for (const ProductionProgress& pr : city->progress) done = pr.item == item ? pr.amount : done;
            const Fixed rest = Fixed::fromInt(productionCost(pid, item, city)) - done;
            const Fixed bought = std::min(rest, Fixed::fromInt(p.gold.toInt() / 2));  // none when in debt
            if (bought <= Fixed()) break;
            p.gold -= bought * 2;
            bool found = false;
            for (ProductionProgress& pr : city->progress) {
                if (pr.item == item) {
                    pr.amount += bought;
                    found = true;
                }
            }
            if (!found) city->progress.push_back({item, bought});
            break;
        }
        case GreatPersonEffectKind::AbsorbCityState: {
            // Stamford Raffles (07; Civilopedia: "Absorbs this City-state into your empire"): its cities become the
            // player's as they stand, with their units; the Loyalty is read from the city's greatPeopleHere.
            const PlayerId cs = state_.plot(here).owner;
            if (cs == kNoPlayer || !isCityState(cs)) break;
            std::vector<CityId> ids;
            for (const City& c : state_.cities) {
                if (c.owner == cs) ids.push_back(c.id);
            }
            for (CityId id : ids) transferCity(id, pid, state_.city(id)->loyalty);
            break;
        }
        case GreatPersonEffectKind::UnitXp: {
            for (Unit& o : state_.units) {
                if (o.pos == here && o.owner == pid && rules_->units[at(o.type)].layer == UnitLayer::Military) {
                    o.xpBonus = static_cast<int16_t>(o.xpBonus + fx.amount);
                    break;
                }
            }
            break;
        }
        case GreatPersonEffectKind::ConvertBarbarians: {
            for (Unit& o : state_.units) {
                if (o.owner != pid && state_.players[at(o.owner)].barbarian && state_.grid.distance(o.pos, here) <= 1) o.owner = pid;
            }
            refreshVisibility(pid);
            break;
        }
        case GreatPersonEffectKind::Suzerain: {
            const PlayerId cs = state_.plot(here).owner;
            if (cs == kNoPlayer || !isCityState(cs)) break;
            for (Player& o : state_.players) {
                if (o.id != pid && at(cs) < o.envoys.size()) o.envoys[at(cs)] = 0;
            }
            if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
            p.envoys[at(cs)] = std::max(p.envoys[at(cs)], rules_->globalInt(HotGlobal::InfluenceTokensMinimumForSuzerain));
            break;
        }
        case GreatPersonEffectKind::GreatPersonPoints: {
            for (int& pts : p.greatPersonPoints) pts += fx.amount * speed / 100;
            break;
        }
    }
}

}  // namespace sov
