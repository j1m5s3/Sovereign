// Religion (06-religion.md; data: religion.md, global RELIGION_* parameters): pantheons,
// founding with a Great Prophet, beliefs, pressure and followers, religious units spreading
// and fighting theologically, and the religious victory.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {

size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }

bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }

bool beliefTaken(const GameState& s, TypeIndex belief) {
    for (const Player& p : s.players) {
        if (p.pantheon == belief) return true;
    }
    for (const FoundedReligion& r : s.religions) {
        if (std::find(r.beliefs.begin(), r.beliefs.end(), belief) != r.beliefs.end()) return true;
    }
    return false;
}

bool typeTaken(const GameState& s, TypeIndex religion) {
    return std::any_of(s.religions.begin(), s.religions.end(), [&](const FoundedReligion& r) { return r.type == religion; });
}

void fitPressure(City& c, size_t n) {
    if (c.pressure.size() < n) c.pressure.resize(n, 0);
}

}  // namespace

bool Game::beliefModelled(TypeIndex belief) const {
    const BeliefType& b = rules_->beliefs[at(belief)];
    if (b.worshipBuilding != kNone || b.grantUnit != kNone) return true;
    return std::any_of(rules_->modifiers.begin(), rules_->modifiers.end(),
                       [&](const Modifier& m) { return m.sourceKind == ModSource::Belief && m.sourceIndex == belief; });
}

int Game::maxReligions() const {
    const TypeIndex size = rules_->mapSize(state_.setup.mapSize);
    return size == kNone ? 3 : std::max(1, rules_->mapSizes[at(size)].maxReligions);
}

std::vector<TypeIndex> Game::availableBeliefs(BeliefClass cls) const {
    std::vector<TypeIndex> out;
    for (size_t i = 0; i < rules_->beliefs.size(); ++i) {
        if (rules_->beliefs[i].cls == cls && !beliefTaken(state_, static_cast<TypeIndex>(i))) out.push_back(static_cast<TypeIndex>(i));
    }
    return out;
}

bool Game::canFoundPantheon(PlayerId player, TypeIndex belief) const {
    const Player& p = state_.players[at(player)];
    if (!isMajor(p) || p.pantheon != kNone) return false;
    if (belief < 0 || at(belief) >= rules_->beliefs.size() || rules_->beliefs[at(belief)].cls != BeliefClass::Pantheon) return false;
    if (beliefTaken(state_, belief)) return false;
    return p.faith >= Fixed::fromInt(rules_->globalInt("RELIGION_PANTHEON_MIN_FAITH"));
}

bool Game::canFoundReligion(UnitId prophet, TypeIndex religion, TypeIndex founder, TypeIndex follower, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    const Unit* u = state_.unit(prophet);
    if (!u || !rules_->units[at(u->type)].foundReligion) return fail(CommandError::BadUnit);
    const Player& p = state_.players[at(u->owner)];
    if (p.religion >= 0 || static_cast<int>(state_.religions.size()) >= maxReligions()) return fail(CommandError::CannotFoundReligion);
    // On a finished Holy Site of one of the player's cities: that city becomes the Holy City.
    const CityDistrict* d = state_.districtAt(u->pos);
    const City* city = state_.plot(u->pos).city != kNoCity ? state_.city(state_.plot(u->pos).city) : nullptr;
    if (!d || !d->complete || rules_->districts[at(d->type)].id != "DISTRICT_HOLY_SITE" || !city || city->owner != u->owner)
        return fail(CommandError::CannotFoundReligion);
    if (religion < 0 || at(religion) >= rules_->religions.size() || typeTaken(state_, religion)) return fail(CommandError::CannotFoundReligion);
    auto ok = [&](TypeIndex b, BeliefClass cls) {
        return b >= 0 && at(b) < rules_->beliefs.size() && rules_->beliefs[at(b)].cls == cls && !beliefTaken(state_, b);
    };
    if (!ok(founder, BeliefClass::Founder) || !ok(follower, BeliefClass::Follower)) return fail(CommandError::CannotFoundReligion);
    if (why) *why = CommandError::Ok;
    return true;
}

bool Game::canEvangelize(UnitId apostle, TypeIndex belief) const {
    const Unit* u = state_.unit(apostle);
    if (!u || u->religion < 0 || rules_->units[at(u->type)].religiousStrength <= 0 || rules_->units[at(u->type)].spreadCharges <= 0) return false;
    const FoundedReligion& r = state_.religions[static_cast<size_t>(u->religion)];
    if (r.founder != u->owner || u->charges < rules_->units[at(u->type)].spreadCharges) return false;  // an unused Apostle
    if (rules_->units[at(u->type)].id != "UNIT_APOSTLE") return false;
    if (belief < 0 || at(belief) >= rules_->beliefs.size() || beliefTaken(state_, belief)) return false;
    const BeliefClass cls = rules_->beliefs[at(belief)].cls;
    if (cls == BeliefClass::Pantheon) return false;
    // Each class holds one belief.
    for (TypeIndex b : r.beliefs) {
        if (rules_->beliefs[at(b)].cls == cls) return false;
    }
    return true;
}

int Game::cityMajorityReligion(const City& city) const { return majorityReligion(state_, *rules_, city); }

int Game::cityFollowers(const City& city, int religion) const { return religionFollowers(state_, *rules_, city, religion); }

bool Game::canSpreadReligion(UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u || u->religion < 0 || u->charges <= 0) return false;
    const UnitType& t = rules_->units[at(u->type)];
    if (t.spreadCharges <= 0 || u->movesLeft <= Fixed()) return false;
    const CityId cid = state_.plot(u->pos).city;
    const City* c = cid != kNoCity ? state_.city(cid) : nullptr;
    if (!c) return false;
    // Inquisitors work only at home (06: Remove Heresy).
    if (t.id == "UNIT_INQUISITOR" && c->owner != u->owner) return false;
    return true;
}

int Game::faithPurchaseCost(PlayerId player, const City& city, ProductionItem item) const {
    const Player& p = state_.players[at(player)];
    const int speed = speedPercent(state_, *rules_);
    const int majority = cityMajorityReligion(city);
    if (item.kind == ProductionKind::Unit) {
        if (item.type < 0 || at(item.type) >= rules_->units.size()) return -1;
        const UnitType& u = rules_->units[at(item.type)];
        if (u.purchaseYield != "FAITH" || !hasUnlocked(player, u.unlock)) return -1;
        if (!u.needsBuilding.empty() && std::none_of(u.needsBuilding.begin(), u.needsBuilding.end(), [&](TypeIndex b) { return city.has(b); }))
            return -1;
        // Missionaries and Apostles carry the city's majority religion; Inquisitors, Gurus and
        // Warrior Monks wait for the beliefs and actions that open them.
        if (u.id != "UNIT_MISSIONARY" && u.id != "UNIT_APOSTLE") return -1;
        if (majority < 0) return -1;
        const int copies = at(item.type) < p.unitsTrained.size() ? p.unitsTrained[at(item.type)] : 0;
        int cost = (u.cost + u.costProgression * copies) * speed / 100;
        const int discount = static_cast<int>(sumPlayerModifiers(state_, *rules_, p, ModEffect::ReligiousUnitDiscountPercent).toInt());
        cost = cost * std::max(0, 100 - discount) / 100;
        return std::max(1, cost);
    }
    if (item.kind == ProductionKind::Building) {
        if (item.type < 0 || at(item.type) >= rules_->buildings.size()) return -1;
        const BuildingType& b = rules_->buildings[at(item.type)];
        // Leader ability: a district's buildings for Faith at their gold price (Golden Pilgrimage).
        if (const TypeIndex d = civAbility(player).faithPurchaseDistrict; d != kNone && b.districtType == d && !b.faithOnly && canProduce(city, item)) {
            const int gold = purchaseCost(player, item);
            if (gold > 0) return gold;
        }
        if (!b.faithOnly || city.has(item.type)) return -1;
        // A worship building needs its belief in the city's majority religion, its district and prerequisites.
        bool belief = false;
        for (size_t i = 0; i < rules_->beliefs.size(); ++i) {
            if (rules_->beliefs[i].worshipBuilding == item.type && religionHas(state_, majority, static_cast<TypeIndex>(i))) belief = true;
        }
        if (!belief || b.districtType == kNone || !city.district(b.districtType, true)) return -1;
        for (TypeIndex req : b.prereqs) {
            if (!city.has(req)) return -1;
        }
        return std::max(1, b.cost * speed / 100);
    }
    return -1;
}

int Game::religiousStrength(const Unit& unit, bool defending) const {
    int s = rules_->units[at(unit.type)].religiousStrength;
    if (s <= 0 || !defending || unit.religion < 0) return s;
    // Defending near its own Holy City, or in a city that follows its religion (06: Theological combat).
    const FoundedReligion& r = state_.religions[static_cast<size_t>(unit.religion)];
    const City* holy = state_.city(r.holyCity);
    if (holy && state_.grid.distance(holy->pos, unit.pos) <= 1) s += rules_->globalInt("COMBAT_RELIGIOUS_HOLY_CITY_TILE");
    const CityId cid = state_.plot(unit.pos).city;
    if (const City* c = cid != kNoCity ? state_.city(cid) : nullptr; c && cityMajorityReligion(*c) == unit.religion)
        s += rules_->globalInt("COMBAT_RELIGIOUS_FOLLOWING_CITY_TILE");
    return s;
}

Yields Game::founderYields(PlayerId player) const {
    Yields out{};
    const Player& p = state_.players[at(player)];
    if (p.religion < 0) return out;
    int cities = 0, followers = 0;
    std::vector<const City*> following;
    for (const City& c : state_.cities) {
        followers += cityFollowers(c, p.religion);
        if (cityMajorityReligion(c) == p.religion) {
            ++cities;
            following.push_back(&c);
        }
    }
    for (const Modifier& m : rules_->modifiers) {
        if (m.sourceKind != ModSource::Belief || !religionHas(state_, p.religion, m.sourceIndex)) continue;
        if (m.effect == ModEffect::FounderYieldPerCity) {
            out[static_cast<size_t>(m.yield)] += m.amount * cities;
        } else if (m.effect == ModEffect::FounderYieldPerFollowers) {
            out[static_cast<size_t>(m.yield)] += m.amount * (followers / m.per);
        } else if (m.effect == ModEffect::FounderYieldPerDistrict) {
            int n = 0;
            for (const City* c : following) n += c->district(m.district, true) ? 1 : 0;
            out[static_cast<size_t>(m.yield)] += m.amount * n;
        }
    }
    return out;
}

PlayerId Game::religiousVictor() const {
    for (size_t r = 0; r < state_.religions.size(); ++r) {
        const PlayerId founder = state_.religions[r].founder;
        if (!isMajor(state_.players[at(founder)])) continue;
        bool all = true;
        for (const Player& civ : state_.players) {
            if (!isMajor(civ)) continue;
            int cities = 0, converted = 0;
            for (const City& c : state_.cities) {
                if (c.owner != civ.id) continue;
                ++cities;
                converted += cityMajorityReligion(c) == static_cast<int>(r) ? 1 : 0;
            }
            if (cities > 0 && converted * 2 <= cities) all = false;  // more than half its cities
        }
        if (all) return founder;
    }
    return kNoPlayer;
}

CommandError Game::validateReligion(const Command& c) const {
    switch (c.type) {
        case CommandType::FoundPantheon:
            return canFoundPantheon(c.player, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotFoundReligion;
        case CommandType::FoundReligion: {
            const Unit* u = state_.unit(c.id);
            if (!u) return CommandError::BadUnit;
            if (u->owner != c.player) return CommandError::NotYourUnit;
            CommandError why = CommandError::Ok;
            canFoundReligion(c.id, static_cast<TypeIndex>(c.arg), static_cast<TypeIndex>(c.arg2), static_cast<TypeIndex>(c.target.x), &why);
            return why;
        }
        case CommandType::EvangelizeBelief: {
            const Unit* u = state_.unit(c.id);
            if (!u) return CommandError::BadUnit;
            if (u->owner != c.player) return CommandError::NotYourUnit;
            return canEvangelize(c.id, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotFoundReligion;
        }
        case CommandType::SpreadReligion: {
            const Unit* u = state_.unit(c.id);
            if (!u) return CommandError::BadUnit;
            if (u->owner != c.player) return CommandError::NotYourUnit;
            return canSpreadReligion(c.id) ? CommandError::Ok : CommandError::CannotSpread;
        }
        default: return CommandError::BadTarget;
    }
}

void Game::applyReligion(const Command& c) {
    Player& p = state_.players[at(c.player)];
    switch (c.type) {
        case CommandType::FoundPantheon: {
            p.pantheon = static_cast<TypeIndex>(c.arg);
            awardFirst(c.player, "MOMENT_WORLD_S_FIRST_PANTHEON", "MOMENT_PANTHEON_FOUNDED");
            p.faith -= Fixed::fromInt(rules_->globalInt("RELIGION_PANTHEON_MIN_FAITH"));
            const TypeIndex grant = rules_->beliefs[at(p.pantheon)].grantUnit;
            if (grant != kNone) {
                for (const City& city : state_.cities) {
                    if (city.owner != c.player || !city.capital) continue;
                    if (auto spot = unitSpawnPlot(city, grant)) spawnUnit(grant, c.player, *spot);
                    break;
                }
            }
            break;
        }
        case CommandType::FoundReligion: {
            const Unit* u = state_.unit(c.id);
            FoundedReligion r;
            r.type = static_cast<TypeIndex>(c.arg);
            r.founder = c.player;
            r.holyCity = state_.plot(u->pos).city;
            if (p.pantheon != kNone) r.beliefs.push_back(p.pantheon);
            r.beliefs.push_back(static_cast<TypeIndex>(c.arg2));
            r.beliefs.push_back(static_cast<TypeIndex>(c.target.x));
            state_.religions.push_back(r);
            const int index = static_cast<int>(state_.religions.size()) - 1;
            p.religion = static_cast<int16_t>(index);
            awardFirst(c.player, "MOMENT_WORLD_S_FIRST_RELIGION", "MOMENT_RELIGION_FOUNDED");
            for (City& city : state_.cities) fitPressure(city, state_.religions.size());
            // The Holy City converts at once (RELIGION_SPREAD_HOLY_CITY_PRESSURE_PER_POP).
            City& holy = *state_.city(r.holyCity);
            holy.pressure[static_cast<size_t>(index)] += rules_->globalInt("RELIGION_SPREAD_HOLY_CITY_PRESSURE_PER_POP") * std::max(1, holy.population);
            removeUnit(c.id);
            break;
        }
        case CommandType::EvangelizeBelief: {
            const Unit* u = state_.unit(c.id);
            state_.religions[static_cast<size_t>(u->religion)].beliefs.push_back(static_cast<TypeIndex>(c.arg));
            removeUnit(c.id);  // the Apostle is spent
            break;
        }
        case CommandType::SpreadReligion: {
            Unit& u = *state_.unit(c.id);
            const UnitType& t = rules_->units[at(u.type)];
            City& city = *state_.city(state_.plot(u.pos).city);
            fitPressure(city, state_.religions.size());
            const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
            const int64_t amount = static_cast<int64_t>(t.religiousStrength) * rules_->globalInt("RELIGION_SPREAD_STRENGTH_MULTIPLIER") / 100 *
                                   u.hp / std::max(1, maxHp);
            // Others lose a share of their pressure; then this religion gains (06: Spread Religion).
            for (size_t i = 0; i < city.pressure.size(); ++i) {
                if (static_cast<int>(i) != u.religion) city.pressure[i] -= city.pressure[i] * t.evictPercent / 100;
            }
            if (t.id != "UNIT_INQUISITOR") city.pressure[static_cast<size_t>(u.religion)] += static_cast<int32_t>(amount);
            if (t.id != "UNIT_INQUISITOR" && city.owner != u.owner) breakPromises(u.owner, city.owner, PromiseKind::NoConverting);  // 08 [GS]
            u.movesLeft = Fixed();
            if (--u.charges <= 0) removeUnit(c.id);
            break;
        }
        default: break;
    }
}

void Game::shiftPressure(Hex at, int range, int religion, int amount) {
    if (religion < 0) return;
    for (City& c : state_.cities) {
        if (state_.grid.distance(c.pos, at) > range) continue;
        fitPressure(c, state_.religions.size());
        int32_t& v = c.pressure[static_cast<size_t>(religion)];
        v = std::max<int32_t>(0, v + amount);
    }
}

void Game::theologicalCombat(Unit& attacker, Unit& defender) {
    Rng& rng = state_.rng.get(RngStream::Combat);
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
    const int sa = religiousStrength(attacker, false), sd = religiousStrength(defender, true);
    const int toDefender = combatDamage(sa - sd, rng.range(0, extra));
    const int toAttacker = combatDamage(sd - sa, rng.range(0, extra));
    defender.hp -= toDefender;
    attacker.hp -= toAttacker;
    attacker.movesLeft = Fixed();
    const int win = rules_->globalInt("RELIGION_SPREAD_COMBAT_VICTORY");
    const int range = rules_->globalInt("RELIGION_SPREAD_RANGE_COMBAT_VICTORY");
    const Hex where = defender.pos;
    // The winner's religion gains pressure nearby and the loser's loses it (06: Theological combat).
    auto settle = [&](Unit& loser, const Unit& winner) {
        shiftPressure(where, range, winner.religion, win);
        const Player& lp = state_.players[at(loser.owner)];
        if (sumPlayerModifiers(state_, *rules_, lp, ModEffect::NoCombatPressureLoss) <= Fixed()) shiftPressure(where, range, loser.religion, -win);
    };
    const UnitId aid = attacker.id, did = defender.id;
    const bool defenderDies = defender.hp <= 0, attackerDies = attacker.hp <= 0;
    if (defenderDies) settle(defender, attacker);
    if (attackerDies) settle(attacker, defender);
    if (defenderDies) removeUnit(did);
    if (attackerDies) removeUnit(aid);
}

void Game::processReligion() {
    if (state_.religions.empty()) return;
    const size_t n = state_.religions.size();
    const int base = rules_->globalInt("RELIGION_SPREAD_ADJACENT_PER_TURN_PRESSURE");
    const int baseRange = rules_->globalInt("RELIGION_SPREAD_ADJACENT_CITY_DISTANCE");
    const TypeIndex holySite = rules_->district("DISTRICT_HOLY_SITE");
    // Every city with a majority religion presses its neighbours within 10 tiles: x2 with a Holy
    // Site, x4 as the Holy City (06: Passive pressure). Sources are read before anything changes.
    struct Source { Hex pos; int religion; int amount; int range; };
    std::vector<Source> sources;
    for (City& c : state_.cities) {
        fitPressure(c, n);
        const int maj = cityMajorityReligion(c);
        if (maj < 0) continue;
        const Player& founder = state_.players[at(state_.religions[static_cast<size_t>(maj)].founder)];
        int amount = base * 10;  // tenths, so percentage beliefs keep their precision
        if (holySite != kNone && c.district(holySite, true)) amount *= rules_->globalInt("RELIGION_SPREAD_HOLY_SITE_PRESSURE_MULTIPLIER");
        if (state_.religions[static_cast<size_t>(maj)].holyCity == c.id) amount *= rules_->globalInt("RELIGION_SPREAD_HOLY_CITY_PRESSURE_MULTIPLIER");
        amount = amount * (100 + static_cast<int>(sumPlayerModifiers(state_, *rules_, founder, ModEffect::ReligionPressurePercent).toInt())) / 100;
        amount = amount * (100 + static_cast<int>(sumCityModifiers(state_, *rules_, c, ModEffect::CityReligionPressurePercent).toInt())) / 100;  // Bishop
        const int range = baseRange + static_cast<int>(sumPlayerModifiers(state_, *rules_, founder, ModEffect::ReligionPressureRange).toInt());
        sources.push_back({c.pos, maj, amount, range});
    }
    for (const Source& src : sources) {
        for (City& c : state_.cities) {
            if (c.pos == src.pos || state_.grid.distance(c.pos, src.pos) > src.range) continue;
            c.pressure[static_cast<size_t>(src.religion)] += src.amount / 10;
        }
    }
}

}  // namespace sov
