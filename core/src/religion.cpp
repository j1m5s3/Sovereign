// Religion (06-religion.md; data: religion.md, global RELIGION_* parameters): pantheons,
// founding with a Great Prophet, beliefs, pressure and followers, religious units spreading
// and fighting theologically, and the religious victory.
#include <algorithm>
#include <iterator>

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
    if (std::find(std::begin(beliefs_), std::end(beliefs_), belief) != std::end(beliefs_)) return true;  // effects in code
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

bool Game::beliefInPlay(Bf which) const {
    const TypeIndex b = beliefs_[static_cast<size_t>(which)];
    if (b == kNone) return false;
    for (const Player& p : state_.players) {
        if (p.pantheon == b) return true;
    }
    for (size_t i = 0; i < state_.religions.size(); ++i) {
        if (religionHas(state_, static_cast<int>(i), b)) return true;
    }
    return false;
}

bool Game::cityFollows(const City& city, Bf which) const {
    const TypeIndex b = beliefs_[static_cast<size_t>(which)];
    if (b == kNone) return false;
    // Cheap checks first: this runs for every plot's yields (Earth Goddess).
    const bool pantheon = state_.players[at(city.owner)].pantheon == b;
    bool religion = false;
    for (size_t i = 0; i < state_.religions.size() && !religion; ++i) religion = religionHas(state_, static_cast<int>(i), b);
    if (!pantheon && !religion) return false;
    const int maj = cityMajorityReligion(city);
    if (maj >= 0) return religionHas(state_, maj, b);
    return pantheon;
}

bool Game::playerHasBelief(PlayerId player, Bf which) const {
    const TypeIndex b = beliefs_[static_cast<size_t>(which)];
    const Player& p = state_.players[at(player)];
    return b != kNone && (p.pantheon == b || (p.religion >= 0 && religionHas(state_, p.religion, b)));
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

bool Game::canLaunchInquisition(UnitId apostle) const {
    const Unit* u = state_.unit(apostle);
    if (!u || u->religion < 0 || rules_->units[at(u->type)].id != "UNIT_APOSTLE") return false;
    const Player& p = state_.players[at(u->owner)];
    return !p.inquisition && p.religion == u->religion && u->charges >= rules_->units[at(u->type)].spreadCharges;
}

bool Game::canHealReligious(UnitId guru) const {
    const Unit* u = state_.unit(guru);
    if (!u || rules_->units[at(u->type)].healCharges <= 0 || u->charges <= 0 || u->movesLeft <= Fixed()) return false;
    const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    for (const Unit& o : state_.units) {
        if (o.owner == u->owner && o.hp < maxHp && rules_->units[at(o.type)].religiousStrength > 0 && state_.grid.distance(o.pos, u->pos) <= 1) return true;
    }
    return false;
}

int Game::faithPurchaseCost(PlayerId player, const City& city, ProductionItem item) const {
    const Player& p = state_.players[at(player)];
    const int speed = speedPercent(state_, *rules_);
    const int majority = cityMajorityReligion(city);
    if (item.kind == ProductionKind::Unit) {
        if (item.type < 0 || at(item.type) >= rules_->units.size()) return -1;
        const UnitType& u = rules_->units[at(item.type)];
        // Grand Master's Chapel (03): land combat units for Faith, at their Gold price.
        static const char* const kChapel[] = {"RECON", "MELEE", "RANGED", "SIEGE", "HEAVY_CAVALRY", "LIGHT_CAVALRY", "RANGED_CAVALRY", "ANTI_CAVALRY"};
        if (u.purchaseYield == "GOLD" && u.domain == Domain::Land && buildingsOwned(player, "BUILDING_GRAND_MASTER_S_CHAPEL") > 0 &&
            std::any_of(std::begin(kChapel), std::end(kChapel), [&](const char* cls) { return u.unitClass == cls; }) && canProduce(city, item, nullptr, true))
            return purchaseCost(player, item, &city, YieldType::Faith);
        // Theocracy (02: Faith purchase; 04): land combat units for Faith, 15% under their Gold price.
        if (u.purchaseYield == "GOLD" && u.domain == Domain::Land && governmentIs(player, "GOVERNMENT_THEOCRACY") &&
            std::any_of(std::begin(kChapel), std::end(kChapel), [&](const char* cls) { return u.unitClass == cls; }) && canProduce(city, item, nullptr, true))
            return std::max(1, purchaseCost(player, item, &city, YieldType::Faith) * 85 / 100);
        if (u.purchaseYield != "FAITH" || !hasUnlocked(player, u.unlock)) return -1;
        // A civ's unique building counts as the one it replaces (Mali's Sahel Mosque as the Temple an Apostle needs).
        if (!u.needsBuilding.empty() &&
            std::none_of(u.needsBuilding.begin(), u.needsBuilding.end(), [&](TypeIndex b) { return cityHasBuilding(city, *rules_, b); }))
            return -1;
        // Missionaries and Apostles carry the city's majority religion; Inquisitors, Gurus and
        // Warrior Monks wait for the beliefs and actions that open them.
        // Naturalists and Rock Bands are bought with Faith whatever the city follows (07).
        const bool secular = u.id == "UNIT_NATURALIST" || u.id == "UNIT_ROCK_BAND";
        // Warrior Monks (06): bought where the city follows a religion with the belief.
        const bool monk = u.id == "UNIT_WARRIOR_MONK" && cityFollows(city, Bf::WarriorMonks);
        // Gurus wherever the city follows a religion; Inquisitors after Launch Inquisition, in cities of the player's own (06).
        const bool inquisitor = u.id == "UNIT_INQUISITOR" && p.inquisition && majority >= 0 && majority == p.religion;
        if (!secular && !monk && !inquisitor && u.id != "UNIT_MISSIONARY" && u.id != "UNIT_APOSTLE" && u.id != "UNIT_GURU") return -1;
        if (!secular && majority < 0) return -1;
        const int copies = at(item.type) < p.unitsTrained.size() ? p.unitsTrained[at(item.type)] : 0;
        int cost = (u.cost + u.costProgression * copies) * speed / 100;
        const int discount = static_cast<int>(sumPlayerModifiers(state_, *rules_, p, ModEffect::ReligiousUnitDiscountPercent).toInt());
        cost = cost * std::max(0, 100 - discount) / 100;
        if (u.id == "UNIT_GURU" && buildingsOwned(player, "BUILDING_MEENAKSHI_TEMPLE") > 0) cost = cost * 70 / 100;  // Meenakshi Temple (03)
        cost = cost * mercenaryPercent(player, item.type, YieldType::Faith) / 100;  // Mercenary Companies (World Congress): Warrior Monks
        return std::max(1, cost);
    }
    if (item.kind == ProductionKind::Building) {
        if (item.type < 0 || at(item.type) >= rules_->buildings.size()) return -1;
        const BuildingType& b = rules_->buildings[at(item.type)];
        // Valletta (08: suzerain): City Center and Encampment buildings for Faith, at their Gold price.
        if ((b.district == "DISTRICT_CITY_CENTER" || b.district == "DISTRICT_ENCAMPMENT") && !b.wonder && suzerainBonus(player, "CITYSTATE_VALLETTA") &&
            canProduce(city, item, nullptr, true))
            return purchaseCost(player, item);
        // Jesuit Education (06): Campus and Theater Square buildings for Faith, at their Gold price.
        if ((b.district == "DISTRICT_CAMPUS" || b.district == "DISTRICT_THEATER_SQUARE") && !b.wonder && cityFollows(city, Bf::JesuitEducation) &&
            canProduce(city, item, nullptr, true))
            return purchaseCost(player, item);
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
        if (!b.prereqs.empty() &&
            std::none_of(b.prereqs.begin(), b.prereqs.end(), [&](TypeIndex req) { return cityHasBuilding(city, *rules_, req); }))
            return -1;
        return std::max(1, b.cost * speed / 100);
    }
    return -1;
}

int Game::civReligion(PlayerId player) const {
    const Player& p = state_.players[at(player)];
    if (p.religion >= 0) return p.religion;
    for (const City& c : state_.cities) {
        if (c.owner == player && c.capital) return cityMajorityReligion(c);
    }
    return -1;
}

int Game::religiousStrength(const Unit& unit, bool defending) const {
    int s = rules_->units[at(unit.type)].religiousStrength;
    if (s <= 0) return s;
    // Abilities and promotions that strengthen religious units: always (Debater, Religious Orders), or in
    // their own territory (Inquisitor; the Inquisition card).
    const bool home = state_.plot(unit.pos).owner == unit.owner;
    auto add = [&](const UnitEffect& e) {
        if (e.kind != UnitEffectKind::Strength) return;
        const bool territory = e.when.size() == 1 && e.when[0].size() == 1 && e.when[0][0].atom == CombatAtom::OwnTerritory && !e.when[0][0].negate;
        if (e.when.empty() || (territory && home)) s += e.amount;
    };
    for (TypeIndex a : unitAbilities(unit)) {
        for (const UnitEffect& e : rules_->abilities[at(a)].effects) add(e);
    }
    for (TypeIndex pr : unit.promotions) {
        for (const UnitEffect& e : rules_->promotions[at(pr)].effects) add(e);
    }
    if (bestAllianceLevel(unit.owner, AllianceType::Religious) >= 2) s += 10;  // a Religious alliance at level 2 (08)
    if (home && state_.players[at(unit.owner)].inquisition) s += 15;  // the Inquisition ability (06)
    // Grand Inquisitor (08: Moksha): +10 in the territory of his city (Sovereign reading of "units bought here").
    if (territoryGovernorHas(unit.pos, unit.owner, "GOVERNOR_PROMOTION_GRAND_INQUISITOR")) s += 10;
    if (unit.religion >= 0 && resolutionHits(ResolutionKind::WorldReligion, 0, unit.religion)) s += 10;  // World Religion (World Congress)
    if (!defending || unit.religion < 0) return s;
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
    // Sacred Places (06): +2 Faith, Culture, Science and Gold for each following city with a world wonder.
    if (religionHas(state_, p.religion, beliefs_[static_cast<size_t>(Bf::SacredPlaces)])) {
        int withWonder = 0;
        for (const City* c : following) {
            withWonder += std::any_of(c->buildings.begin(), c->buildings.end(), [&](TypeIndex b) { return rules_->buildings[at(b)].wonder; }) ? 1 : 0;
        }
        for (YieldType y : {YieldType::Faith, YieldType::Culture, YieldType::Science, YieldType::Gold}) out[static_cast<size_t>(y)] += Fixed::fromInt(2 * withWonder);
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
        case CommandType::LaunchInquisition:
        case CommandType::HealReligious: {
            const Unit* u = state_.unit(c.id);
            if (!u) return CommandError::BadUnit;
            if (u->owner != c.player) return CommandError::NotYourUnit;
            const bool ok = c.type == CommandType::LaunchInquisition ? canLaunchInquisition(c.id) : canHealReligious(c.id);
            return ok ? CommandError::Ok : CommandError::BadTarget;
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
            {
                // Every class of belief held (founder, follower, worship, enhancer): a moment (09).
                bool classes[kNumBeliefClasses] = {};
                for (TypeIndex b : state_.religions[static_cast<size_t>(u->religion)].beliefs) classes[static_cast<int>(rules_->beliefs[at(b)].cls)] = true;
                bool all = true;
                for (int k = static_cast<int>(BeliefClass::Follower); k < kNumBeliefClasses; ++k) all = all && classes[k];
                if (all) awardFirst(c.player, "MOMENT_WORLD_S_FIRST_RELIGION_TO_ADOPT_ALL_BELIEFS", "MOMENT_RELIGION_ADOPTS_ALL_BELIEFS", 0);
            }
            removeUnit(c.id);  // the Apostle is spent
            break;
        }
        case CommandType::LaunchInquisition:
            state_.players[at(c.player)].inquisition = true;
            awardFirst(c.player, "MOMENT_WORLD_S_FIRST_INQUISITION", "MOMENT_INQUISITION_BEGINS", 0);
            removeUnit(c.id);  // the Apostle is spent
            break;
        case CommandType::HealReligious: {
            Unit& guru = *state_.unit(c.id);
            const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
            for (Unit& o : state_.units) {
                if (o.owner == guru.owner && rules_->units[at(o.type)].religiousStrength > 0 && state_.grid.distance(o.pos, guru.pos) <= 1)
                    o.hp = std::min(maxHp, o.hp + rules_->globalInt("COMBAT_HEAL_RELIGIOUS_CHARGE"));
            }
            guru.movesLeft = Fixed();
            if (--guru.charges <= 0) removeUnit(c.id);
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
            // Proselytizer removes more; Translator presses three times as hard in other civs' cities.
            const int evict = std::min(100, t.evictPercent + unitEffectTotal(u, UnitEffectKind::EvictPercent));
            for (size_t i = 0; i < city.pressure.size(); ++i) {
                if (static_cast<int>(i) != u.religion) city.pressure[i] -= city.pressure[i] * evict / 100;
            }
            const int64_t pressed = city.owner != u.owner ? amount * (100 + unitEffectTotal(u, UnitEffectKind::ForeignSpreadPercent)) / 100 : amount;
            const int before = cityMajorityReligion(city);
            if (t.id != "UNIT_INQUISITOR") city.pressure[static_cast<size_t>(u.religion)] += static_cast<int32_t>(pressed);
            if (before != u.religion && cityMajorityReligion(city) == u.religion) {
                dedicationScore(u.owner, "DEDICATION_EXODUS_OF_THE_EVANGELISTS", 2);  // 09
                // A rival's Holy City, or a city of a civ at war with it, turned (09: Historic moments).
                for (size_t ri = 0; ri < state_.religions.size(); ++ri) {
                    if (static_cast<int>(ri) != u.religion && state_.religions[ri].holyCity == city.id && state_.religions[ri].founder != u.owner)
                        awardMoment(u.owner, "MOMENT_RIVAL_HOLY_CITY_CONVERTED");
                }
                if (city.owner != u.owner && atWar(u.owner, city.owner)) awardMoment(u.owner, "MOMENT_ENEMY_CITY_ADOPTS_OUR_RELIGION");
                // Fez (08: suzerain): the first time this player's religious units convert the city, 20 Science per citizen.
                std::vector<CityId>& converted = state_.players[at(u.owner)].convertedCities;
                if (std::find(converted.begin(), converted.end(), city.id) == converted.end()) {
                    converted.push_back(city.id);
                    if (suzerainBonus(u.owner, "CITYSTATE_FEZ"))
                        processResearch(u.owner, Fixed::fromInt(20 * city.population * speedPercent(state_, *rules_) / 100), Fixed());
                }
                // Indulgence Vendor: Gold the first time it turns a city (bit 0x80 of wonderAbilities marks it spent).
                if (const int gold = unitEffectTotal(u, UnitEffectKind::ConvertGold); gold > 0 && !(u.wonderAbilities & 0x80)) {
                    state_.players[at(u.owner)].gold += Fixed::fromInt(gold);
                    u.wonderAbilities |= 0x80;
                }
            }
            // Heathen Conversion: the barbarians next to it join its owner.
            if (unitHas(u, UnitEffectKind::HeathenConversion)) {
                for (Unit& o : state_.units) {
                    if (state_.players[at(o.owner)].barbarian && state_.grid.distance(o.pos, u.pos) == 1) o.owner = u.owner;
                }
            }
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
    // Martyr (06): a Relic in a free slot of its owner's if it falls.
    for (Unit* fallen : {defenderDies ? &defender : nullptr, attackerDies ? &attacker : nullptr}) {
        if (!fallen || !unitHas(*fallen, UnitEffectKind::Martyr)) continue;
        const TypeIndex relic = rules_->greatWorkType("RELIC");
        for (City& c : state_.cities) {
            const TypeIndex slot = c.owner == fallen->owner && relic != kNone ? freeGreatWorkSlot(c, relic) : kNone;
            if (slot == kNone) continue;
            GreatWork w;
            w.type = relic;
            w.building = slot;
            c.greatWorks.push_back(w);
            break;
        }
    }
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
    struct Source { Hex pos; int religion; int amount; int range; PlayerId owner; };
    std::vector<Source> sources;
    for (City& c : state_.cities) {
        fitPressure(c, n);
        const int maj = cityMajorityReligion(c);
        if (maj < 0) continue;
        const Player& founder = state_.players[at(state_.religions[static_cast<size_t>(maj)].founder)];
        int amount = base * 10;  // tenths, so percentage beliefs keep their precision
        if (holySite != kNone && c.district(holySite, true)) amount *= rules_->globalInt("RELIGION_SPREAD_HOLY_SITE_PRESSURE_MULTIPLIER");
        // Jerusalem (08: suzerain): its suzerain's cities with a Holy Site press as if they were Holy Cities.
        const bool asHoly = holySite != kNone && c.district(holySite, true) && suzerainBonus(c.owner, "CITYSTATE_JERUSALEM");
        if (state_.religions[static_cast<size_t>(maj)].holyCity == c.id || asHoly) amount *= rules_->globalInt("RELIGION_SPREAD_HOLY_CITY_PRESSURE_MULTIPLIER");
        amount = amount * (100 + static_cast<int>(sumPlayerModifiers(state_, *rules_, founder, ModEffect::ReligionPressurePercent).toInt())) / 100;
        amount = amount * (100 + static_cast<int>(sumCityModifiers(state_, *rules_, c, ModEffect::CityReligionPressurePercent).toInt())) / 100;  // Bishop
        const int range = baseRange + static_cast<int>(sumPlayerModifiers(state_, *rules_, founder, ModEffect::ReligionPressureRange).toInt());
        sources.push_back({c.pos, maj, amount, range, c.owner});
    }
    std::vector<std::pair<CityId, int>> before;  // city-states' cities and their religion before the pressure
    for (const City& c : state_.cities) {
        if (isCityState(c.owner)) before.push_back({c.id, cityMajorityReligion(c)});
    }
    for (const Source& src : sources) {
        for (City& c : state_.cities) {
            if (c.pos == src.pos || state_.grid.distance(c.pos, src.pos) > src.range) continue;
            if (c.owner != src.owner && alliance(c.owner, src.owner) == AllianceType::Religious) continue;  // 08: no pressure between allies
            c.pressure[static_cast<size_t>(src.religion)] += src.amount / 10;
        }
    }
    // Religious Unity (06): converting a city-state awards its founder an envoy.
    for (const auto& [id, was] : before) {
        const City* c = state_.city(id);
        const int now = c ? cityMajorityReligion(*c) : -1;
        if (now < 0 || now == was) continue;
        const PlayerId founder = state_.religions[static_cast<size_t>(now)].founder;
        if (playerHasBelief(founder, Bf::ReligiousUnity) && !policyIs(founder, "POLICY_ROGUE_STATE")) ++state_.players[at(founder)].envoyTokens;
    }
}

}  // namespace sov
