// War and peace, unit combat, zone of control, XP, promotions, healing and
// strategic fuel (05-units-and-combat.md; constants from global-parameters.md
// COMBAT, EXPERIENCE and DIPLOMACY) and city combat: walls, strikes, capture,
// razing and elimination (02-cities.md, City combat).
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
const UnitType& typeOf(const Rules& r, const Unit& u) { return r.units[static_cast<size_t>(u.type)]; }

bool hasCivicFlag(const Rules& r, const Player& p, bool TreeNode::*flag) {
    for (size_t i = 0; i < r.civics.size(); ++i) {
        if (r.civics[i].*flag && p.civics.has(static_cast<TypeIndex>(i))) return true;
    }
    return false;
}

// Everything a combat condition can ask about one side of a fight.
struct ConditionContext {
    const GameState* s;
    const Rules* r;
    const Unit* unit;
    const Unit* opponent;   // null when fighting a city
    const City* city;       // the opposing city, if any
    bool attacking;
    bool ranged;
    int cityMaxHp;
};

bool atomHolds(const CombatCondition& c, const ConditionContext& x) {
    const UnitType* opp = x.opponent ? &typeOf(*x.r, *x.opponent) : nullptr;
    const Plot& plot = x.s->plot(x.unit->pos);
    bool ok = false;
    switch (c.atom) {
        case CombatAtom::Untracked: return false;  // unknown conditions never hold, negated or not
        case CombatAtom::Attacking: ok = x.attacking; break;
        case CombatAtom::VsClass: ok = opp && opp->unitClass == c.value; break;
        case CombatAtom::VsDomain: ok = opp ? static_cast<int>(opp->domain) == c.arg : c.arg == 0; break;
        case CombatAtom::VsDistrict: ok = x.city != nullptr; break;
        case CombatAtom::CombatType: ok = (c.arg == 1) == x.ranged; break;
        case CombatAtom::TileHills: ok = x.r->terrains[static_cast<size_t>(plot.terrain)].relief == Relief::Hills; break;
        case CombatAtom::TileFeature: ok = plot.feature == c.ref; break;
        case CombatAtom::TileTerrain: ok = plot.terrain == c.ref; break;
        case CombatAtom::OpponentFortified: ok = x.opponent && x.opponent->fortifyTurns > 0; break;
        case CombatAtom::OpponentWounded:
            ok = x.opponent ? x.opponent->hp < x.r->globalInt("COMBAT_MAX_HIT_POINTS") : x.city && x.city->hp < x.cityMaxHp;
            break;
        case CombatAtom::DistrictTile: ok = x.s->cityAt(x.unit->pos) != nullptr; break;
        case CombatAtom::OwnTerritory: ok = plot.owner == x.unit->owner; break;
    }
    return c.negate ? !ok : ok;
}

bool conditionsHold(const UnitEffect& e, const ConditionContext& x) {
    for (const auto& any : e.when) {
        bool one = false;
        for (const CombatCondition& c : any) one = one || atomHolds(c, x);
        if (!one) return false;
    }
    return true;
}

// round(a / b) for a >= 0, b > 0, halves up.
int roundDiv(int64_t a, int64_t b) { return static_cast<int>((2 * a + b) / (2 * b)); }

// Only these classes take a city (02-cities.md, City combat: Capture).
// A religious unit of another player and another religion on the plot: a theological foe (06).
const Unit* religiousFoeAt(const GameState& s, const Rules& r, const Unit& attacker, Hex at) {
    for (const Unit& o : s.units) {
        if (o.pos != at || o.owner == attacker.owner || o.religion < 0 || o.religion == attacker.religion) continue;
        if (r.units[static_cast<size_t>(o.type)].religiousStrength > 0) return &o;
    }
    return nullptr;
}

bool capturesCities(const UnitType& ut) {
    return ut.unitClass == "MELEE" || ut.unitClass == "ANTI_CAVALRY" || ut.unitClass == "LIGHT_CAVALRY" ||
           ut.unitClass == "HEAVY_CAVALRY";
}

bool anyBuilding(const Rules& r, const City& c, bool BuildingType::*flag) {
    for (TypeIndex b : c.buildings) if (r.buildings[static_cast<size_t>(b)].*flag) return true;
    return false;
}
}  // namespace

// ------------------------------------------------------------------ war and peace

// The barbarian player is at war with everyone from the start (linkBarbarians);
// war and peace with it cannot be declared or made.
bool Game::atWar(PlayerId a, PlayerId b) const {
    if (a == b || a < 0 || b < 0) return false;
    const Player& p = state_.players[static_cast<size_t>(a)];
    return static_cast<size_t>(b) < p.relations.size() && p.relations[static_cast<size_t>(b)].war;
}

bool Game::canDeclareWar(PlayerId player, PlayerId target) const {
    if (target < 0 || static_cast<size_t>(target) >= state_.players.size() || target == player) return false;
    const Player& t = state_.players[static_cast<size_t>(target)];
    if (!t.alive || t.barbarian || state_.players[static_cast<size_t>(player)].barbarian) return false;
    const Relation& rel = state_.players[static_cast<size_t>(player)].relations[static_cast<size_t>(target)];
    if (rel.war || state_.turn < rules_->globalInt("DIPLOMACY_EARLIEST_MAJOR_DOW_TURN")) return false;
    if (friends(player, target)) return false;  // a declared friend cannot be attacked while it lasts (08)
    // After a peace treaty, war may not resume for DIPLOMACY_PEACE_MIN_TURNS.
    return rel.since == 0 || state_.turn - rel.since >= rules_->globalInt("DIPLOMACY_PEACE_MIN_TURNS");
}

bool Game::canMakePeace(PlayerId player, PlayerId target) const {
    if (!atWar(player, target)) return false;
    if (state_.players[static_cast<size_t>(player)].barbarian || state_.players[static_cast<size_t>(target)].barbarian)
        return false;
    const Relation& rel = state_.players[static_cast<size_t>(player)].relations[static_cast<size_t>(target)];
    return !rel.peaceOffered && state_.turn - rel.since >= rules_->globalInt("DIPLOMACY_WAR_MIN_TURNS");
}

// ------------------------------------------------------------------ unit effects

std::vector<TypeIndex> Game::unitAbilities(const Unit& unit) const {
    const UnitType& ut = typeOf(*rules_, unit);
    std::vector<TypeIndex> out = ut.abilities;
    for (TypeIndex a : grantedAbilities(state_, *rules_, state_.players[static_cast<size_t>(unit.owner)])) {
        const AbilityType& at = rules_->abilities[static_cast<size_t>(a)];
        if (std::find(at.classes.begin(), at.classes.end(), ut.unitClass) != at.classes.end() &&
            std::find(out.begin(), out.end(), a) == out.end())
            out.push_back(a);
    }
    return out;
}

namespace {
template <typename Fn>
void forEachEffect(const Rules& r, const Unit& unit, const std::vector<TypeIndex>& abilities, Fn&& fn) {
    for (TypeIndex a : abilities) {
        for (const UnitEffect& e : r.abilities[static_cast<size_t>(a)].effects) fn(e);
    }
    for (TypeIndex p : unit.promotions) {
        for (const UnitEffect& e : r.promotions[static_cast<size_t>(p)].effects) fn(e);
    }
}
}  // namespace

int Game::unitEffectTotal(const Unit& unit, UnitEffectKind kind) const {
    int total = 0;
    forEachEffect(*rules_, unit, unitAbilities(unit), [&](const UnitEffect& e) {
        if (e.kind == kind) total += e.amount != 0 ? e.amount : 1;
    });
    return total;
}

int Game::maxMoves(const Unit& unit) const {
    if (isEmbarked(unit)) {
        // Embarked units move at a base rate raised by later techs (05: Embarkation).
        int moves = rules_->globalInt("MOVEMENT_WHILE_EMBARKED_BASE");
        const Player& p = state_.players[static_cast<size_t>(unit.owner)];
        for (size_t i = 0; i < rules_->techs.size(); ++i) {
            if (p.techs.has(static_cast<TypeIndex>(i))) moves += rules_->techs[i].embarkedMoves;
        }
        return std::max(1, moves);
    }
    int moves = typeOf(*rules_, unit).moves + unitEffectTotal(unit, UnitEffectKind::Moves) + greatPersonAuraMoves(unit);
    if (!isLeader(unit)) return moves;
    for (TypeIndex g : unit.gear) {
        if (g != kNone) moves += rules_->gear[static_cast<size_t>(g)].moves;  // mounts add, heavy armor subtracts
    }
    return std::max(1, moves);
}

int Game::unitRange(const Unit& unit) const {
    const TypeIndex weapon = unit.gear[static_cast<size_t>(GearSlot::Weapon)];
    const int base = !isLeader(unit) ? typeOf(*rules_, unit).range
                     : weapon == kNone ? 0 : rules_->gear[static_cast<size_t>(weapon)].range;
    return base > 0 ? base + unitEffectTotal(unit, UnitEffectKind::Range) : 0;
}

int Game::unitSight(const Unit& unit) const {
    return typeOf(*rules_, unit).sight + unitEffectTotal(unit, UnitEffectKind::Sight);
}

int Game::maxAttacks(const Unit& unit) const {
    return rules_->globalInt("COMBAT_MAX_NUM_ATTACKS") + unitEffectTotal(unit, UnitEffectKind::Attacks);
}

bool Game::inEnemyZoc(const Unit& mover, Hex plot) const {
    if (unitHas(mover, UnitEffectKind::IgnoreZoc)) return false;
    for (const Hex& h : state_.grid.within(plot, 1)) {
        if (h == plot) continue;
        const City* c = state_.cityAt(h);
        if (c && atWar(mover.owner, c->owner)) return true;
        for (const Unit& u : state_.units) {
            if (u.pos == h && atWar(mover.owner, u.owner) && exertsZoc(u)) return true;
        }
    }
    return false;
}

bool Game::exertsZoc(const Unit& u) const {
    const UnitType& ut = typeOf(*rules_, u);
    return ut.layer == UnitLayer::Military && (ut.zoneOfControl || unitHas(u, UnitEffectKind::ExertZoc));
}

std::vector<uint8_t> Game::zocMap(const Unit& mover) const {
    std::vector<uint8_t> out;
    if (unitHas(mover, UnitEffectKind::IgnoreZoc)) return out;
    auto mark = [&](Hex center) {
        if (out.empty()) out.assign(static_cast<size_t>(state_.grid.size()), 0);
        for (const Hex& h : state_.grid.within(center, 1)) {
            if (h != center) out[static_cast<size_t>(state_.grid.index(h))] = 1;
        }
    };
    for (const City& c : state_.cities) if (atWar(mover.owner, c.owner)) mark(c.pos);
    for (const Unit& u : state_.units) if (atWar(mover.owner, u.owner) && exertsZoc(u)) mark(u.pos);
    return out;
}

// ------------------------------------------------------------------ strength and damage

int Game::combatStrength(const Unit& unit, const Unit& opponent, bool attacking, bool ranged) const {
    return unitStrength(unit, &opponent, nullptr, attacking, ranged);
}

int Game::combatStrengthVsCity(const Unit& unit, const City& city, bool attacking, bool ranged) const {
    return unitStrength(unit, nullptr, &city, attacking, ranged);
}

int Game::unitStrength(const Unit& unit, const Unit* oppUnit, const City* oppCity, bool attacking, bool ranged) const {
    const UnitType& ut = typeOf(*rules_, unit);
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    const Hex oppPos = oppUnit ? oppUnit->pos : oppCity->pos;
    const PlayerId oppOwner = oppUnit ? oppUnit->owner : oppCity->owner;
    const bool bombard = attacking && ranged && ut.ranged == 0 && ut.bombard > 0;
    int s = !(attacking && ranged) ? meleeStrength(unit) : bombard ? ut.bombard : rangedStrength(unit);
    const bool embarked = isEmbarked(unit);
    if (!attacking && embarked) {
        // An embarked unit defends with a strength set by its owner's era (05: Embarkation).
        const int era = std::clamp(playerEra(unit.owner), 0, static_cast<int>(rules_->eras.size()) - 1);
        s = rules_->eras[static_cast<size_t>(era)].embarkedStrength;
    }
    if (!attacking && isLeader(unit) && !embarked) {
        const TypeIndex armor = unit.gear[static_cast<size_t>(GearSlot::Armor)];
        if (armor != kNone) s += rules_->gear[static_cast<size_t>(armor)].defense;  // armor counts when defending
    }

    // Promotions and abilities.
    const ConditionContext ctx{&state_, rules_, &unit, oppUnit, oppCity, attacking, ranged, cityMaxHp()};
    int flankPercent = 100, supportPercent = 100;
    bool noRiver = false, noWounded = false, bombardPenalty = false, districtPenalty = false;
    forEachEffect(*rules_, unit, unitAbilities(unit), [&](const UnitEffect& e) {
        switch (e.kind) {
            case UnitEffectKind::Strength: if (conditionsHold(e, ctx)) s += e.amount; break;
            case UnitEffectKind::FlankingPercent: flankPercent += e.amount; break;
            case UnitEffectKind::SupportPercent: supportPercent += e.amount; break;
            case UnitEffectKind::NoRiverPenalty: noRiver = true; break;
            case UnitEffectKind::NoWoundedPenalty: noWounded = true; break;
            case UnitEffectKind::BombardVsUnit: bombardPenalty = true; break;
            case UnitEffectKind::RangedVsDistrict: districtPenalty = true; break;
            default: break;
        }
    });
    if (bombard && bombardPenalty && oppUnit) s -= rules_->globalInt("COMBAT_BOMBARD_VS_UNIT_STRENGTH_MODIFIER");
    if (attacking && ranged && districtPenalty && oppCity) s -= rules_->globalInt("COMBAT_RANGED_VS_DISTRICT_STRENGTH_MODIFIER");
    // The leader's presence aura for its military units nearby (leader doc §1).
    if (ut.layer == UnitLayer::Military) {
        const Unit* leader = leaderOf(unit.owner);
        if (leader && state_.grid.distance(leader->pos, unit.pos) <= auraRange(*leader))
            s += rules_->globalInt("LEADER_AURA_STRENGTH") + unitEffectTotal(*leader, UnitEffectKind::AuraStrength);
    }
    // A Great General or Admiral nearby (05: +5 for units of its era or the next).
    s += greatPersonAuraStrength(unit);
    // Defender of the Faith / Crusade: in the lands of a city following the player's religion (06).
    if (owner.religion >= 0 && ut.layer == UnitLayer::Military) {
        const CityId cid = state_.plot(unit.pos).city;
        if (const City* c = cid != kNoCity ? state_.city(cid) : nullptr; c && cityMajorityReligion(*c) == owner.religion) {
            for (const Modifier& m : rules_->modifiers) {
                if (m.effect != ModEffect::UnitStrengthNearFollowingCity || m.sourceKind != ModSource::Belief) continue;
                if (!religionHas(state_, owner.religion, m.sourceIndex) || m.foreign != (c->owner != unit.owner)) continue;
                s += static_cast<int>(m.amount.toInt());
            }
        }
    }
    // Policies such as Discipline (+5 against barbarians).
    s += sumUnitStrength(state_, *rules_, owner, ut.unitClass,
                         oppOwner >= 0 && state_.players[static_cast<size_t>(oppOwner)].barbarian);

    if (!attacking) {
        // Terrain defence (hills, woods, marsh...) and fortification.
        const Plot& p = state_.plot(unit.pos);
        s += rules_->terrains[static_cast<size_t>(p.terrain)].defense;
        if (p.feature != kNone) s += rules_->features[static_cast<size_t>(p.feature)].defense;
        s += std::min(unit.fortifyTurns, rules_->globalInt("FORTIFY_TURN_MAX")) * rules_->globalInt("FORTIFY_BONUS_PER_TURN");
    } else if (!ranged) {
        // Attacking across a river (05: COMBAT_RIVER_DEFENSE).
        auto d = state_.grid.directionTo(unit.pos, oppPos);
        if (d && !noRiver && hasRiver(state_, unit.pos, *d)) s -= rules_->globalInt("COMBAT_RIVER_DEFENSE");
    }

    // Flanking (melee attacker) and support (defender), once Military Tradition is known.
    if (hasCivicFlag(*rules_, owner, &TreeNode::combatAdjacency) && (!attacking || !ranged)) {
        const Hex around = attacking ? oppPos : unit.pos;
        int friends = 0;
        for (const Unit& u : state_.units) {
            if (u.id == unit.id || u.owner != unit.owner || typeOf(*rules_, u).layer != UnitLayer::Military) continue;
            if (u.pos != around && state_.grid.distance(u.pos, around) == 1) ++friends;
        }
        const int per = rules_->globalInt(attacking ? "COMBAT_FLANKING_BONUS_MODIFIER" : "COMBAT_SUPPORT_BONUS_MODIFIER");
        s += friends * per * (attacking ? flankPercent : supportPercent) / 100;
    }

    // Wounded: round(10 - hp/10) at 100 max HP (COMBAT_WOUNDED_DAMAGE_MULTIPLIER).
    const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    if (!noWounded && unit.hp < maxHp)
        s -= roundDiv(static_cast<int64_t>(rules_->globalInt("COMBAT_WOUNDED_DAMAGE_MULTIPLIER")) * (maxHp - unit.hp), maxHp);

    // Unpaid strategic maintenance [GS].
    if (ut.resourceMaintenance > 0 && ut.strategicResource != kNone &&
        owner.fuelShort[static_cast<size_t>(ut.strategicResource)])
        s -= rules_->globalInt("COMBAT_STRENGTH_REDUCTION_INSUFFICIENT_FUEL");
    return s;
}

// ------------------------------------------------------------------ cities

int Game::cityMaxHp() const {
    const TypeIndex center = rules_->district("DISTRICT_CITY_CENTER");  // the loader guarantees it
    return rules_->districts[static_cast<size_t>(center)].hp;
}

int Game::cityMaxWallHp(const City& city) const {
    int hp = 0;
    for (TypeIndex b : city.buildings) hp += rules_->buildings[static_cast<size_t>(b)].outerDefenseHp;
    return hp;
}

int Game::cityStrength(const City& city) const {
    // max(strongest unit built - 10, garrison) + walls + modifiers (Palace) + center terrain
    // - 1 per 10% of city HP lost (02-cities.md, City combat).
    const Player& owner = state_.players[static_cast<size_t>(city.owner)];
    int s = std::max(0, owner.strongestUnit - rules_->globalInt("CITY_STRENGTH_BELOW_STRONGEST_UNIT"));
    const Unit* garrison = state_.unitAt(city.pos, UnitLayer::Military, *rules_);
    if (garrison && garrison->owner == city.owner) s = std::max(s, typeOf(*rules_, *garrison).combat);
    for (TypeIndex b : city.buildings) s += rules_->buildings[static_cast<size_t>(b)].defense;
    s += static_cast<int>(sumCityModifiers(state_, *rules_, city, ModEffect::CityDefense).toInt());
    const Plot& p = state_.plot(city.pos);
    s += rules_->terrains[static_cast<size_t>(p.terrain)].defense;
    if (p.feature != kNone) s += rules_->features[static_cast<size_t>(p.feature)].defense;
    const int maxHp = cityMaxHp();
    if (city.hp < maxHp)
        s -= roundDiv(static_cast<int64_t>(rules_->globalInt("COMBAT_WOUNDED_DISTRICT_DAMAGE_MULTIPLIER")) *
                          (maxHp - std::max(0, city.hp)), maxHp);
    return s;
}

bool Game::cityUnderSiege(const City& city) const {
    for (const Hex& n : state_.grid.within(city.pos, 1)) {
        if (n == city.pos) continue;
        bool held = false;
        for (const Unit& u : state_.units) {
            if (!atWar(city.owner, u.owner)) continue;
            const int d = state_.grid.distance(u.pos, n);
            if (d == 0 || (d == 1 && exertsZoc(u))) {
                held = true;
                break;
            }
        }
        if (!held) return false;
    }
    return true;
}

bool Game::canCityStrike(CityId id, Hex target) const {
    const City* c = state_.city(id);
    if (!c || c->struck || cityMaxWallHp(*c) <= 0) return false;  // strikes need walls
    auto t = state_.grid.normalize(target);
    if (!t || *t != target) return false;
    const Unit* u = defenderAt(*t);
    if (!u || !atWar(c->owner, u->owner)) return false;
    const TypeIndex center = rules_->district("DISTRICT_CITY_CENTER");
    const int dist = state_.grid.distance(c->pos, *t);
    if (dist < 1 || dist > rules_->districts[static_cast<size_t>(center)].attackRange) return false;
    return visibility(c->owner, *t) == Visibility::Visible && lineOfSight(c->pos, *t);
}

bool Game::canRazeCity(PlayerId player, CityId id) const {
    const City* c = state_.city(id);
    if (!c || c->owner != player || c->capturedTurn != state_.turn) return false;
    return !c->originalCapital || rules_->globalInt("COMBAT_RAZE_ANY_CITY") != 0;
}

int Game::combatDamage(int strengthDifference, int roll) const {
    // (COMBAT_BASE_DAMAGE + roll) * e^(COMBAT_POWER_SCALING * difference), at least COMBAT_MINIMUM_DAMAGE.
    const Fixed scale = Fixed::exp(rules_->global("COMBAT_POWER_SCALING") * static_cast<int64_t>(strengthDifference));
    const Fixed dmg = Fixed::fromInt(rules_->globalInt("COMBAT_BASE_DAMAGE") + roll) * scale;
    return std::max(rules_->globalInt("COMBAT_MINIMUM_DAMAGE"), static_cast<int>(dmg.round()));
}

int Game::wallDamagePercent(const Unit& attacker, const City& city, bool ranged) const {
    // Walls take hits first, scaled by attack type (05: Walls; 02-cities.md, City combat).
    if (city.wallHp <= 0) return -1;
    const UnitType& ut = typeOf(*rules_, attacker);
    if (ranged) {
        const bool bombard = ut.ranged == 0 && ut.bombard > 0;
        return rules_->globalInt(bombard ? "COMBAT_DEFENSE_DAMAGE_PERCENT_BOMBARD" : "COMBAT_DEFENSE_DAMAGE_PERCENT_RANGED");
    }
    // Support units next to the city: Siege Tower lets melee past the walls, Battering Ram gives full damage.
    bool bypass = false, ram = false;
    for (const Unit& s : state_.units) {
        if (s.owner != attacker.owner || state_.grid.distance(s.pos, city.pos) != 1) continue;
        bypass = bypass || unitHas(s, UnitEffectKind::BypassWalls);
        ram = ram || unitHas(s, UnitEffectKind::WallFullDamage);
    }
    if (bypass && !anyBuilding(*rules_, city, &BuildingType::wallsCannotBeBypassed)) return -1;
    if (anyBuilding(*rules_, city, &BuildingType::meleeCannotDamageWalls)) return 0;
    return ram ? 100 : rules_->globalInt("COMBAT_DEFENSE_DAMAGE_PERCENT_MELEE");
}

CombatPreview Game::previewAttack(UnitId attackerId, Hex target, bool ranged) const {
    CombatPreview out;
    const Unit* a = state_.unit(attackerId);
    if (!a) return out;
    Command c = ranged ? Command::rangedAttack(a->owner, attackerId, target) : Command::attack(a->owner, attackerId, target);
    // Validation here ignores whose turn it is so the AI and UI can ask at any time.
    if (validateCombat(c) != CommandError::Ok) return out;
    out.valid = true;
    out.ranged = ranged;
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
    if (const City* city = state_.cityAt(target)) {
        out.city = city->id;
        if (!ranged && city->hp <= 0) {
            out.captureCity = true;
            return out;
        }
        out.attackerStrength = combatStrengthVsCity(*a, *city, true, ranged);
        out.defenderStrength = cityStrength(*city);
        const int diff = out.attackerStrength - out.defenderStrength;
        const int wallPercent = wallDamagePercent(*a, *city, ranged);
        out.hitsWalls = wallPercent >= 0;
        const int percent = out.hitsWalls ? wallPercent : 100;
        out.damageToDefenderMin = roundDiv(static_cast<int64_t>(combatDamage(diff, 0)) * percent, 100);
        out.damageToDefenderMax = roundDiv(static_cast<int64_t>(combatDamage(diff, extra)) * percent, 100);
        if (!ranged) {
            out.damageToAttackerMin = combatDamage(-diff, 0);
            out.damageToAttackerMax = combatDamage(-diff, extra);
        }
        return out;
    }
    const Unit* d = defenderAt(target);
    if (!d) {
        out.capture = true;
        return out;
    }
    out.defender = d->id;
    out.attackerStrength = combatStrength(*a, *d, true, ranged);
    out.defenderStrength = combatStrength(*d, *a, false, ranged);
    const int diff = out.attackerStrength - out.defenderStrength;
    out.damageToDefenderMin = combatDamage(diff, 0);
    out.damageToDefenderMax = combatDamage(diff, extra);
    if (!ranged) {
        out.damageToAttackerMin = combatDamage(-diff, 0);
        out.damageToAttackerMax = combatDamage(-diff, extra);
    }
    return out;
}

// ------------------------------------------------------------------ XP and promotions

int Game::xpForNextLevel(const Unit& unit) const {
    return rules_->globalInt("EXPERIENCE_PER_LEVEL") * unit.level();
}

bool Game::canPromote(UnitId id, TypeIndex promotion) const {
    const Unit* u = state_.unit(id);
    if (!u || promotion < 0 || static_cast<size_t>(promotion) >= rules_->promotions.size()) return false;
    const PromotionType& pr = rules_->promotions[static_cast<size_t>(promotion)];
    if (pr.promotionClass.empty() || pr.promotionClass != typeOf(*rules_, *u).promotionClass) return false;
    if (u->xp < xpForNextLevel(*u) || u->movesLeft <= Fixed()) return false;
    if (std::find(u->promotions.begin(), u->promotions.end(), promotion) != u->promotions.end()) return false;
    // A leader finishes only one branch per reign (leader doc §3).
    if (!pr.branch.empty() && pr.tier >= 2) {
        for (TypeIndex have : u->promotions) {
            const PromotionType& h = rules_->promotions[static_cast<size_t>(have)];
            if (h.tier >= 2 && !h.branch.empty() && h.branch != pr.branch) return false;
        }
    }
    if (pr.prereqs.empty()) return true;
    for (TypeIndex req : pr.prereqs) {
        if (std::find(u->promotions.begin(), u->promotions.end(), req) != u->promotions.end()) return true;
    }
    return false;
}

std::vector<TypeIndex> Game::availablePromotions(UnitId id) const {
    std::vector<TypeIndex> out;
    for (size_t i = 0; i < rules_->promotions.size(); ++i) {
        if (canPromote(id, static_cast<TypeIndex>(i))) out.push_back(static_cast<TypeIndex>(i));
    }
    return out;
}

void Game::gainXp(Unit& unit, int ownBase, int enemyBase, bool ranged, bool attacker, bool killed, bool vsBarbarian) {
    if (ownBase <= 0) return;
    // 05: enemy base / own base (x2 for a kill), +2 melee or +1 ranged, +1 attacker; at most 8.
    int xp = enemyBase * (killed ? rules_->globalInt("EXPERIENCE_KILL_BONUS") : 1) / ownBase;
    xp += rules_->globalInt(ranged ? "EXPERIENCE_COMBAT_RANGED" : "EXPERIENCE_NOT_COMBAT_RANGED");
    if (attacker) xp += rules_->globalInt("EXPERIENCE_COMBAT_ATTACKER_BONUS");
    awardXp(unit, std::min(xp, rules_->globalInt("EXPERIENCE_MAXIMUM_ONE_COMBAT")), vsBarbarian);
}

void Game::awardXp(Unit& unit, int xp, bool vsBarbarian) {
    const UnitType& ut = typeOf(*rules_, unit);
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    if (ut.promotionClass.empty() || owner.barbarian) return;  // barbarians never promote
    // Fights with barbarians cannot take a unit past level 2 (EXPERIENCE_MAX_BARB_LEVEL).
    if (vsBarbarian && unit.level() >= rules_->globalInt("EXPERIENCE_MAX_BARB_LEVEL")) return;
    const int percent = 100 + static_cast<int>(sumUnitXpPercent(state_, *rules_, owner, ut.unitClass).toInt()) +
                        unitEffectTotal(unit, UnitEffectKind::XpPercent);
    xp = xp * percent / 100;
    // XP stops at the next level until the promotion is taken.
    unit.xp = std::min(unit.xp + xp, xpForNextLevel(unit));
}

// ------------------------------------------------------------------ commands

CommandError Game::validateCombat(const Command& c) const {
    // Range-check arg before narrowing it to a player or promotion index.
    const bool playerArg = c.arg >= 0 && static_cast<size_t>(c.arg) < state_.players.size();
    switch (c.type) {
        case CommandType::DeclareWar:
            return playerArg && canDeclareWar(c.player, static_cast<PlayerId>(c.arg)) ? CommandError::Ok
                                                                                     : CommandError::CannotDeclareWar;
        case CommandType::MakePeace:
            return playerArg && canMakePeace(c.player, static_cast<PlayerId>(c.arg)) ? CommandError::Ok
                                                                                    : CommandError::CannotMakePeace;
        case CommandType::CityStrike: {
            const City* city = state_.city(c.id);
            if (!city) return CommandError::BadCity;
            if (city->owner != c.player) return CommandError::NotYourCity;
            return canCityStrike(c.id, c.target) ? CommandError::Ok : CommandError::CannotStrike;
        }
        case CommandType::RazeCity: {
            const City* city = state_.city(c.id);
            if (!city) return CommandError::BadCity;
            if (city->owner != c.player) return CommandError::NotYourCity;
            return canRazeCity(c.player, c.id) ? CommandError::Ok : CommandError::CannotRaze;
        }
        default: break;
    }
    const Unit* u = state_.unit(c.id);
    if (!u) return CommandError::BadUnit;
    if (u->owner != c.player) return CommandError::NotYourUnit;
    if (c.type == CommandType::Promote) {
        const bool known = c.arg >= 0 && static_cast<size_t>(c.arg) < rules_->promotions.size();
        return known && canPromote(c.id, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotPromote;
    }

    const UnitType& ut = typeOf(*rules_, *u);
    auto t = state_.grid.normalize(c.target);
    if (!t || *t != c.target) return CommandError::BadTarget;
    if (ut.religiousStrength > 0) {
        // Theological combat: an adjacent religious unit of another religion, no war needed (06).
        if (c.type != CommandType::Attack || u->religion < 0 || u->movesLeft <= Fixed() || state_.grid.distance(u->pos, *t) != 1)
            return CommandError::CannotAttack;
        return religiousFoeAt(state_, *rules_, *u, *t) ? CommandError::Ok : CommandError::CannotAttack;
    }
    if ((ut.layer != UnitLayer::Military && ut.layer != UnitLayer::Leader) || u->movesLeft <= Fixed() ||
        u->attacks >= maxAttacks(*u))
        return CommandError::CannotAttack;
    // Attacks on a city hit the city, whoever garrisons it; elsewhere the military unit,
    // then a leader, defends the plot (leader doc §1: escorts first).
    const City* city = state_.cityAt(*t);
    const Unit* defender = city ? nullptr : defenderAt(*t);
    if (city && (city->owner == c.player || !atWar(c.player, city->owner))) return CommandError::CannotAttack;
    if (defender && !atWar(c.player, defender->owner)) return CommandError::CannotAttack;
    if (isEmbarked(*u)) return CommandError::CannotAttack;  // embarked units cannot attack (05: Embarkation)

    if (c.type == CommandType::RangedAttack) {
        if ((rangedStrength(*u) <= 0 && ut.bombard <= 0) || (!defender && !city)) return CommandError::CannotAttack;
        if (u->moved && unitHas(*u, UnitEffectKind::NoAttackAfterMove) && !unitHas(*u, UnitEffectKind::AttackAfterMove))
            return CommandError::CannotAttack;
        const int dist = state_.grid.distance(u->pos, *t);
        if (dist < 1 || dist > unitRange(*u)) return CommandError::CannotAttack;
        if (visibility(c.player, *t) != Visibility::Visible || !lineOfSight(u->pos, *t)) return CommandError::CannotAttack;
        return CommandError::Ok;
    }
    // Melee: ranged and siege units cannot; the target must be adjacent and enterable.
    if (meleeStrength(*u) <= 0 || rangedStrength(*u) > 0 || ut.bombard > 0) return CommandError::CannotAttack;
    if (state_.grid.distance(u->pos, *t) != 1 || !terrainCost(*u, u->pos, *t)) return CommandError::CannotAttack;
    // Land units fight on land; ships fight on the water and against coastal cities.
    if (ut.domain == Domain::Land && rules_->terrains[static_cast<size_t>(state_.plot(*t).terrain)].water) return CommandError::CannotAttack;
    if (city) {
        // A city at 0 HP is only entered by a unit that can take it; barbarians never take cities.
        const bool takes = capturesCities(ut) && !state_.players[static_cast<size_t>(c.player)].barbarian;
        return city->hp > 0 || takes ? CommandError::Ok : CommandError::CannotAttack;
    }
    if (defender) return CommandError::Ok;
    // No military unit: capture the civilians there if they belong to an enemy.
    bool any = false;
    for (const Unit& o : state_.units) {
        if (o.pos != *t) continue;
        if (!atWar(c.player, o.owner)) return CommandError::CannotAttack;
        any = true;
    }
    return any ? CommandError::Ok : CommandError::CannotAttack;
}

void Game::removeUnit(UnitId id) {
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(), [&](const Unit& x) { return x.id == id; }),
                       state_.units.end());
}

void Game::seizeCivilian(UnitId id, PlayerId captor) {
    Unit* o = state_.unit(id);
    TypeIndex becomes = typeOf(*rules_, *o).capturedAs;
    // Barbarians cannot found cities: a Settler they take becomes a Builder (01: Barbarians).
    if (becomes != kNone && state_.players[static_cast<size_t>(captor)].barbarian &&
        rules_->units[static_cast<size_t>(becomes)].foundCity)
        becomes = rules_->unit("UNIT_BUILDER");
    if (becomes == kNone) {
        removeUnit(id);
        return;
    }
    o->owner = captor;
    o->type = becomes;
    if (o->charges == 0) o->charges = rules_->units[static_cast<size_t>(becomes)].buildCharges;
    o->movesLeft = Fixed();
    o->activity = Activity::Awake;
    o->moveTarget.reset();
}

void Game::afterAttack(Unit& u) {
    ++u.attacks;
    u.attacked = true;
    u.fortifyTurns = 0;
    u.activity = Activity::Awake;
    u.moveTarget.reset();
    if (unitHas(u, UnitEffectKind::MoveAfterAttack)) {
        u.movesLeft = u.movesLeft > Fixed::fromInt(1) ? u.movesLeft - Fixed::fromInt(1) : Fixed();
    } else if (u.attacks >= maxAttacks(u)) {
        u.movesLeft = Fixed();
    }
}

void Game::applyCombat(const Command& c) {
    switch (c.type) {
        case CommandType::DeclareWar: {
            Relation& mine = state_.players[static_cast<size_t>(c.player)].relations[static_cast<size_t>(c.arg)];
            Relation& theirs = state_.players[static_cast<size_t>(c.arg)].relations[static_cast<size_t>(c.player)];
            onWarDeclared(c.player, static_cast<PlayerId>(c.arg));
            for (Relation* r : {&mine, &theirs}) {
                r->war = true;
                r->since = state_.turn;
                r->peaceOffered = false;
            }
            return;
        }
        case CommandType::MakePeace: {
            Relation& mine = state_.players[static_cast<size_t>(c.player)].relations[static_cast<size_t>(c.arg)];
            Relation& theirs = state_.players[static_cast<size_t>(c.arg)].relations[static_cast<size_t>(c.player)];
            mine.peaceOffered = true;
            if (theirs.peaceOffered) onPeace(c.player, static_cast<PlayerId>(c.arg));
            return;
        }
        case CommandType::Promote: {
            Unit* u = state_.unit(c.id);
            u->promotions.push_back(static_cast<TypeIndex>(c.arg));
            u->xp = 0;  // excess XP is lost on promotion
            u->hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), u->hp + rules_->globalInt("EXPERIENCE_PROMOTE_HEALED"));
            u->movesLeft = Fixed();  // promoting ends the unit's turn
            return;
        }
        case CommandType::CityStrike: {
            // The city fires at a unit: ranged combat, only the unit takes damage.
            City& city = *state_.city(c.id);
            Unit* target = state_.unit(defenderAt(c.target)->id);
            const PlayerId them = target->owner;
            const int sa = std::max(rules_->globalInt("COMBAT_MINIMUM_CITY_STRIKE_STRENGTH"), cityStrength(city));
            const int sd = combatStrengthVsCity(*target, city, false, true);
            const int roll = state_.rng.get(RngStream::Combat).range(0, rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE"));
            target->hp -= combatDamage(sa - sd, roll);
            city.struck = true;
            if (target->hp <= 0 && isLeader(*target)) {
                leaderLost(target->id, city.owner, false);  // a ranged kill
            } else if (target->hp <= 0) {
                noteKill(*target, nullptr);
                removeUnit(target->id);
            }
            refreshVisibility(them);
            return;
        }
        case CommandType::RazeCity: {
            // Burning a city stains the ruler's name (leader doc §8.1).
            Player& p = state_.players[static_cast<size_t>(c.player)];
            p.reputation = std::max(-100, p.reputation - rules_->globalInt("REPUTATION_PER_RAZE"));
            ++p.citiesRazed;  // every civ hears of it (agendas)
            razeCity(c.id);
            return;
        }
        default: break;
    }
    if (Unit* a = state_.unit(c.id); a && typeOf(*rules_, *a).religiousStrength > 0) {
        theologicalCombat(*a, *state_.unit(religiousFoeAt(state_, *rules_, *a, c.target)->id));
        return;
    }
    if (const City* city = state_.cityAt(c.target)) {
        attackCity(c, *state_.city(city->id));
        return;
    }

    const UnitId attackerId = c.id;
    const PlayerId me = c.player;
    const Hex target = c.target;
    const Unit* d = defenderAt(target);
    const bool ranged = c.type == CommandType::RangedAttack;
    Rng& rng = state_.rng.get(RngStream::Combat);
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");

    if (!d) {
        // Capture: civilians become the attacker's (Settler, Builder) or are destroyed.
        std::vector<UnitId> there;
        for (const Unit& o : state_.units) if (o.pos == target) there.push_back(o.id);
        for (UnitId id : there) {
            const PlayerId lost = state_.unit(id)->owner;
            seizeCivilian(id, me);
            refreshVisibility(lost);
        }
        Unit* a = state_.unit(attackerId);
        const Fixed cost = *terrainCost(*a, a->pos, target);
        a->pos = target;
        a->moved = true;
        a->fortifyTurns = 0;
        a->activity = Activity::Awake;
        a->moveTarget.reset();
        a->movesLeft = a->movesLeft >= cost ? a->movesLeft - cost : Fixed();
        if (inEnemyZoc(*a, target)) a->movesLeft = Fixed();
        enterPlot(*a);
        refreshVisibility(me);
        for (const Player& p : state_.players) if (p.id != me) checkElimination(p.id);
        return;
    }

    const UnitId defenderId = d->id;
    Unit* a = state_.unit(attackerId);
    Unit* def = state_.unit(defenderId);
    const int sa = combatStrength(*a, *def, true, ranged);
    const int sd = combatStrength(*def, *a, false, ranged);
    // Melee with a human's leader stack waits for its live battle (leader doc §9).
    if (!ranged && state_.setup.liveBattles) {
        const PlayerId side = liveBattleSide(*a, *def);
        if (side != kNoPlayer) {
            PendingBattle& b = state_.pendingBattle;
            b = PendingBattle{};
            b.active = true;
            b.attacker = attackerId;
            b.defender = defenderId;
            b.target = target;
            b.liveFor = side;
            const Unit* l = leaderOf(side);
            b.leader = l ? l->id : kNoUnit;
            b.expectedToDefender = combatDamage(sa - sd, extra / 2);
            b.expectedToAttacker = combatDamage(sd - sa, extra / 2);
            return;
        }
    }
    const int toDefender = combatDamage(sa - sd, rng.range(0, extra));
    const int toAttacker = ranged ? 0 : combatDamage(sd - sa, rng.range(0, extra));
    resolveUnitFight(attackerId, defenderId, target, ranged, toDefender, toAttacker);
}

PlayerId Game::liveBattleSide(const Unit& attacker, const Unit& defender) const {
    // A side's leader stack: the leader itself, or a unit on the same plot as its own leader.
    auto stackOf = [&](const Unit& u, Hex at) -> PlayerId {
        const Player& p = state_.players[static_cast<size_t>(u.owner)];
        if (!p.human) return kNoPlayer;
        if (isLeader(u)) return u.owner;
        const Unit* l = state_.unitAt(at, UnitLayer::Leader, *rules_);
        return l && l->owner == u.owner ? u.owner : kNoPlayer;
    };
    const PlayerId att = stackOf(attacker, attacker.pos);
    return att != kNoPlayer ? att : stackOf(defender, defender.pos);
}

CommandError Game::validateBattle(const Command& c) const {
    const PendingBattle& b = state_.pendingBattle;
    if (!b.active) return CommandError::NoBattle;
    if (c.type == CommandType::BattleResult) return c.player == b.liveFor ? CommandError::Ok : CommandError::BattlePending;
    if (c.type == CommandType::AutoResolveBattle) {
        // Either side may settle it with the normal roll (the AI never plays it live).
        const Unit* a = state_.unit(b.attacker);
        const Unit* d = state_.unit(b.defender);
        const City* city = state_.city(b.city);
        const bool party = (a && a->owner == c.player) || (d && d->owner == c.player) || (city && city->owner == c.player) ||
                           c.player == b.liveFor;
        return party ? CommandError::Ok : CommandError::BattlePending;
    }
    return CommandError::BattlePending;
}

void Game::applyBattle(const Command& c) {
    const PendingBattle b = state_.pendingBattle;
    state_.pendingBattle = PendingBattle{};
    if (b.city != kNoCity && c.type == CommandType::AutoResolveBattle) {
        Rng& rng = state_.rng.get(RngStream::Combat);
        const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
        const Unit& a = *state_.unit(b.attacker);
        const City& city = *state_.city(b.city);
        const int sa = combatStrengthVsCity(a, city, true, false), sd = cityStrength(city);
        const int dealt = combatDamage(sa - sd, rng.range(0, extra));
        const int toAttacker = combatDamage(sd - sa, rng.range(0, extra));
        resolveCityAssault(b.attacker, b.city, false, dealt, toAttacker);
        return;
    }
    if (c.type == CommandType::AutoResolveBattle) {
        Rng& rng = state_.rng.get(RngStream::Combat);
        const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
        const Unit& a = *state_.unit(b.attacker);
        const Unit& d = *state_.unit(b.defender);
        const int sa = combatStrength(a, d, true, false), sd = combatStrength(d, a, false, false);
        const int toDefender = combatDamage(sa - sd, rng.range(0, extra));
        const int toAttacker = combatDamage(sd - sa, rng.range(0, extra));
        resolveUnitFight(b.attacker, b.defender, b.target, false, toDefender, toAttacker);
        return;
    }
    // The field result moves the expected Civ result at most LIVE_BATTLE_BAND_PERCENT either way (§9).
    const int band = rules_->globalInt("LIVE_BATTLE_BAND_PERCENT");
    auto clampToBand = [&](int field, int expected) {
        const int lo = expected * (100 - band) / 100, hi = (expected * (100 + band) + 99) / 100;
        return std::clamp(field, lo, hi);
    };
    const int toDefender = clampToBand(c.arg, b.expectedToDefender);
    const int toAttacker = clampToBand(c.arg2, b.expectedToAttacker);
    const int wound = std::clamp(c.target.x, 0, rules_->globalInt("LIVE_BATTLE_LEADER_MAX_WOUND"));
    if (b.city != kNoCity) resolveCityAssault(b.attacker, b.city, false, toDefender, toAttacker);
    else resolveUnitFight(b.attacker, b.defender, b.target, false, toDefender, toAttacker);
    // The leader fought in person: it may come out hurt, never killed by the wound alone.
    Unit* l = state_.unit(b.leader);
    if (l && l->id != b.attacker && l->id != b.defender) l->hp = std::max(1, l->hp - wound);
}

void Game::resolveUnitFight(UnitId attackerId, UnitId defenderId, Hex target, bool ranged, int toDefender, int toAttacker) {
    Unit* a = state_.unit(attackerId);
    Unit* def = state_.unit(defenderId);
    const PlayerId me = a->owner;
    const PlayerId them = def->owner;
    const UnitType& at = typeOf(*rules_, *a);
    const int baseA = !ranged ? meleeStrength(*a) : rangedStrength(*a) > 0 ? rangedStrength(*a) : at.bombard;
    const int baseD = meleeStrength(*def);
    def->hp -= toDefender;
    a->hp -= toAttacker;
    const bool barbA = state_.players[static_cast<size_t>(me)].barbarian;
    const bool barbD = state_.players[static_cast<size_t>(them)].barbarian;
    // Barbarians wound a leader but never kill or capture it (leader doc §1).
    const bool leaderD = isLeader(*def), leaderA = isLeader(*a);
    if (leaderD && barbA) barbarianWound(*def);
    if (leaderA && barbD) barbarianWound(*a);
    const bool defenderDied = def->hp <= 0;
    const bool attackerDied = a->hp <= 0;
    if (!attackerDied) gainXp(*a, baseA, baseD, ranged, true, defenderDied, barbD);
    if (!defenderDied) gainXp(*def, baseD, baseA, ranged, false, attackerDied, barbA);
    if (!attackerDied) afterAttack(*a);
    if (defenderDied && !leaderD) noteKill(*def, attackerDied ? nullptr : a);
    if (attackerDied && !leaderA) noteKill(*a, defenderDied ? nullptr : def);

    // A beaten leader is captured by a melee victor and killed otherwise (leader doc §5).
    if (defenderDied) {
        if (leaderD) leaderLost(defenderId, me, !ranged && !attackerDied);
        else removeUnit(defenderId);
    }
    if (attackerDied) {
        if (leaderA) leaderLost(attackerId, them, false);
        else removeUnit(attackerId);
    }
    // The victor does not advance while an enemy leader still stands on the plot.
    const Unit* leaderLeft = state_.unitAt(target, UnitLayer::Leader, *rules_);
    if (defenderDied && !attackerDied && !ranged && !leaderLeft) {
        // The melee victor advances, capturing any civilians the defender escorted.
        for (const Unit& o : state_.units) {
            if (o.pos == target && o.owner == them) {
                seizeCivilian(o.id, me);
                break;
            }
        }
        Unit* winner = state_.unit(attackerId);
        winner->pos = target;
        winner->moved = true;
        enterPlot(*winner);
    }
    refreshVisibility(me);
    refreshVisibility(them);
    checkElimination(them);
}

void Game::attackCity(const Command& c, City& city) {
    const bool ranged = c.type == CommandType::RangedAttack;
    Unit* a = state_.unit(c.id);
    if (!ranged && city.hp <= 0) {
        captureCity(city, c.id);  // a city at 0 HP falls to the first melee unit to enter
        return;
    }
    const int sa = combatStrengthVsCity(*a, city, true, ranged);
    const int sd = cityStrength(city);
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
    // An assault (not a bombardment) on a city holding a human's leader, or by a human's leader
    // stack, waits for its live battle (leader doc §9).
    if (!ranged && state_.setup.liveBattles) {
        const PlayerId side = liveAssaultSide(*a, city);
        if (side != kNoPlayer) {
            PendingBattle& b = state_.pendingBattle;
            b = PendingBattle{};
            b.active = true;
            b.attacker = c.id;
            b.city = city.id;
            b.target = city.pos;
            b.liveFor = side;
            const Unit* l = leaderOf(side);
            b.leader = l ? l->id : kNoUnit;
            b.expectedToDefender = combatDamage(sa - sd, extra / 2);
            b.expectedToAttacker = combatDamage(sd - sa, extra / 2);
            return;
        }
    }
    Rng& rng = state_.rng.get(RngStream::Combat);
    const int dealt = combatDamage(sa - sd, rng.range(0, extra));
    const int toAttacker = ranged ? 0 : combatDamage(sd - sa, rng.range(0, extra));
    resolveCityAssault(c.id, city.id, ranged, dealt, toAttacker);
}

PlayerId Game::liveAssaultSide(const Unit& attacker, const City& city) const {
    const Player& ap = state_.players[static_cast<size_t>(attacker.owner)];
    if (ap.human) {
        if (isLeader(attacker)) return attacker.owner;
        const Unit* l = state_.unitAt(attacker.pos, UnitLayer::Leader, *rules_);
        if (l && l->owner == attacker.owner) return attacker.owner;
    }
    const Unit* inside = state_.unitAt(city.pos, UnitLayer::Leader, *rules_);
    if (inside && inside->owner == city.owner && state_.players[static_cast<size_t>(city.owner)].human) return city.owner;
    return kNoPlayer;
}

void Game::resolveCityAssault(UnitId attackerId, CityId cityId, bool ranged, int dealt, int toAttacker) {
    City& city = *state_.city(cityId);
    Unit* a = state_.unit(attackerId);
    const PlayerId them = city.owner;
    const PlayerId me = a->owner;
    const int wallPercent = wallDamagePercent(*a, city, ranged);
    if (wallPercent >= 0) {
        city.wallHp = std::max(0, city.wallHp - roundDiv(static_cast<int64_t>(dealt) * wallPercent, 100));
    } else {
        city.hp = std::max(0, city.hp - dealt);
    }
    city.lastAttackedTurn = state_.turn;
    a->hp -= toAttacker;
    if (a->hp <= 0) {
        if (isLeader(*a)) {
            leaderLost(attackerId, them, false);  // a leader killed storming the walls (§5)
        } else {
            noteKill(*a, nullptr);
            removeUnit(attackerId);
        }
        refreshVisibility(me);
        return;
    }
    afterAttack(*a);
    const bool takes = !ranged && city.hp <= 0 && capturesCities(typeOf(*rules_, *a)) &&
                       !state_.players[static_cast<size_t>(me)].barbarian;
    if (takes) {
        captureCity(city, attackerId);
        return;
    }
    awardXp(*a, rules_->globalInt("EXPERIENCE_UNIT_VS_DISTRICT_NOT_CITY_CAPTURED"), false);
    refreshVisibility(me);
    refreshVisibility(them);
}

void Game::captureCity(City& city, UnitId attackerId) {
    Unit* a = state_.unit(attackerId);
    const PlayerId me = a->owner;
    const PlayerId lost = city.owner;
    const CityId cid = city.id;
    const Hex at = city.pos;
    // The garrison dies; civilians on the center are captured or destroyed.
    std::vector<UnitId> there;
    for (const Unit& o : state_.units) if (o.pos == at && o.owner != me) there.push_back(o.id);
    for (UnitId id : there) {
        if (!state_.unit(id)) continue;  // gone with a regicide earlier in this loop
        const UnitLayer layer = typeOf(*rules_, *state_.unit(id)).layer;
        if (layer == UnitLayer::Leader) leaderLost(id, me, true);  // taken with the city
        else if (layer == UnitLayer::Military) removeUnit(id);
        else seizeCivilian(id, me);
    }

    City& c = *state_.city(cid);
    if (isMajorCiv(lost)) {
        ++state_.players[static_cast<size_t>(me)].citiesCaptured;
        remember(lost, me, MemoryKind::CapturedCity, -10, 60);
    }
    if (c.originalCapital && c.originalOwner != me && !isCityState(c.originalOwner)) awardMoment(me, "MOMENT_FOREIGN_CAPITAL_TAKEN");
    c.owner = me;
    // 25% of the population is lost and the city is left at half HP with no walls.
    const int64_t lossRaw = rules_->global("CITY_POPULATION_LOSS_TO_CONQUEST_PERCENTAGE").raw() * c.population;
    c.population = std::max(1, c.population - static_cast<int>(Fixed::fromRaw(lossRaw).toInt()));
    const int maxHp = cityMaxHp();
    c.hp = maxHp - maxHp * rules_->globalInt("CITY_CAPTURED_DAMAGE_PERCENTAGE") / 100;
    c.wallHp = 0;
    c.queue.clear();
    c.progress.clear();
    c.overflow = Fixed();
    c.locked.clear();
    c.capturedTurn = state_.turn;
    c.lastAttackedTurn = state_.turn;
    c.loyalty = rules_->globalInt("LOYALTY_AFTER_TRANSFERRED_BY_COMBAT");
    c.struck = true;
    const bool wasCapital = c.capital;
    if (wasCapital) {
        // The Palace stays with its owner, who moves it to their oldest remaining city.
        c.capital = false;
        c.buildings.erase(std::remove_if(c.buildings.begin(), c.buildings.end(),
                                         [&](TypeIndex b) { return rules_->buildings[static_cast<size_t>(b)].granted; }),
                          c.buildings.end());
    }
    for (Plot& p : state_.plots) {
        if (p.city == cid) p.owner = me;
    }
    assignCitizens(c);

    Unit* winner = state_.unit(attackerId);
    winner->pos = at;
    winner->moved = true;
    winner->movesLeft = Fixed();
    awardXp(*winner, rules_->globalInt("EXPERIENCE_CITY_CAPTURED"), false);

    if (wasCapital) {
        City* next = nullptr;
        for (City& o : state_.cities) {
            if (o.owner == lost && (!next || o.foundedTurn < next->foundedTurn)) next = &o;
        }
        if (next) {
            next->capital = true;
            for (size_t b = 0; b < rules_->buildings.size(); ++b) {
                if (!rules_->buildings[b].granted) continue;
                auto it = std::lower_bound(next->buildings.begin(), next->buildings.end(), static_cast<TypeIndex>(b));
                if (it == next->buildings.end() || *it != static_cast<TypeIndex>(b))
                    next->buildings.insert(it, static_cast<TypeIndex>(b));
            }
            assignCitizens(*next);
        }
    }
    refreshVisibility(me);
    refreshVisibility(lost);
    checkElimination(lost);
}

void Game::razeCity(CityId id) {
    const PlayerId owner = state_.city(id)->owner;
    for (Plot& p : state_.plots) {
        if (p.city != id) continue;
        p.city = kNoCity;
        p.owner = kNoPlayer;
    }
    state_.cities.erase(std::remove_if(state_.cities.begin(), state_.cities.end(), [&](const City& c) { return c.id == id; }),
                        state_.cities.end());
    refreshVisibility(owner);
}

void Game::checkElimination(PlayerId pid) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    if (!p.alive || p.barbarian) return;
    for (const City& c : state_.cities) if (c.owner == pid) return;
    // No cities: out once it has lost a city, or before its first city once it has no units left.
    bool units = false;
    for (const Unit& u : state_.units) units = units || u.owner == pid;
    if (p.citiesFounded == 0 && units) return;
    p.alive = false;
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(), [&](const Unit& u) { return u.owner == pid; }),
                       state_.units.end());
}

void Game::healCities(PlayerId pid) {
    // City HP heals unless besieged; walls repair after a quiet spell (02-cities.md, City combat).
    const int maxHp = cityMaxHp();
    for (City& c : state_.cities) {
        if (c.owner != pid) continue;
        if (c.hp < maxHp && !cityUnderSiege(c)) c.hp = std::min(maxHp, c.hp + rules_->globalInt("COMBAT_HEAL_CITY_GARRISON"));
        const int maxWalls = cityMaxWallHp(c);
        if (c.wallHp < maxWalls && state_.turn - c.lastAttackedTurn > rules_->globalInt("COMBAT_HEAL_OUTER_DEFENSES_COOLDOWN"))
            c.wallHp = std::min(maxWalls, c.wallHp + rules_->globalInt("COMBAT_HEAL_CITY_OUTER_DEFENSES"));
        c.struck = false;
    }
}

// ------------------------------------------------------------------ turn upkeep

void Game::payUnitFuel(PlayerId pid) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    std::vector<int> needed(rules_->resources.size(), 0);
    for (const Unit& u : state_.units) {
        const UnitType& ut = typeOf(*rules_, u);
        if (u.owner == pid && ut.resourceMaintenance > 0 && ut.strategicResource != kNone)
            needed[static_cast<size_t>(ut.strategicResource)] += ut.resourceMaintenance;
        // A leader's mount costs twice its upkeep unit's resource maintenance (leader doc §8.8).
        const TypeIndex mount = u.gear[static_cast<size_t>(GearSlot::Mount)];
        if (u.owner == pid && isLeader(u) && mount != kNone) {
            const TypeIndex as = rules_->gear[static_cast<size_t>(mount)].upkeepAs;
            const UnitType* mt = as == kNone ? nullptr : &rules_->units[static_cast<size_t>(as)];
            if (mt && mt->resourceMaintenance > 0 && mt->strategicResource != kNone)
                needed[static_cast<size_t>(mt->strategicResource)] += 2 * mt->resourceMaintenance;
        }
    }
    for (size_t r = 0; r < needed.size(); ++r) {
        if (needed[r] == 0) {
            p.fuelShort[r] = 0;
        } else if (p.stockpile[r] >= needed[r]) {
            p.stockpile[r] -= needed[r];
            p.fuelShort[r] = 0;
        } else {
            p.stockpile[r] = 0;
            p.fuelShort[r] = 1;
        }
    }
}

void Game::healAndFortify(PlayerId pid) {
    const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    const int fortifyMax = rules_->globalInt("FORTIFY_TURN_MAX");
    const Player& owner = state_.players[static_cast<size_t>(pid)];
    for (Unit& u : state_.units) {
        if (u.owner != pid) continue;
        const UnitType& ut = typeOf(*rules_, u);
        const bool acted = u.moved || u.attacked;
        const bool fuelShort = ut.resourceMaintenance > 0 && ut.strategicResource != kNone &&
                               owner.fuelShort[static_cast<size_t>(ut.strategicResource)];
        if (u.hp < maxHp && !fuelShort && (!acted || unitHas(u, UnitEffectKind::HealAfterAction))) {
            const Plot& p = state_.plot(u.pos);
            const City* c = state_.cityAt(u.pos);
            const bool naval = ut.domain == Domain::Sea;
            int heal;
            if (c && c->owner == pid) heal = rules_->globalInt("COMBAT_HEAL_CITY_GARRISON");
            else if (p.owner == pid) heal = rules_->globalInt(naval ? "COMBAT_HEAL_NAVAL_FRIENDLY" : "COMBAT_HEAL_LAND_FRIENDLY");
            else if (p.owner != kNoPlayer && atWar(pid, p.owner))
                heal = rules_->globalInt(naval ? "COMBAT_HEAL_NAVAL_ENEMY" : "COMBAT_HEAL_LAND_ENEMY");
            else heal = rules_->globalInt(naval ? "COMBAT_HEAL_NAVAL_NEUTRAL" : "COMBAT_HEAL_LAND_NEUTRAL");
            u.hp = std::min(maxHp, u.hp + heal);
        }
        if (u.activity == Activity::Fortify && !acted) u.fortifyTurns = std::min(fortifyMax, u.fortifyTurns + 1);
        u.moved = false;
        u.attacked = false;
        u.attacks = 0;
    }
}

}  // namespace sov
