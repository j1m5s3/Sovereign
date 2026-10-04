// War and peace, unit combat, zone of control, XP, promotions, healing and
// strategic fuel (05-units-and-combat.md; constants from global-parameters.md
// COMBAT, EXPERIENCE and DIPLOMACY). City combat and barbarians arrive in MVP-4b.
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
    const Unit* opponent;
    bool attacking;
    bool ranged;
};

bool atomHolds(const CombatCondition& c, const ConditionContext& x) {
    const UnitType& opp = typeOf(*x.r, *x.opponent);
    const Plot& plot = x.s->plot(x.unit->pos);
    bool ok = false;
    switch (c.atom) {
        case CombatAtom::Untracked: return false;  // unknown conditions never hold, negated or not
        case CombatAtom::Attacking: ok = x.attacking; break;
        case CombatAtom::VsClass: ok = opp.unitClass == c.value; break;
        case CombatAtom::VsDomain: ok = static_cast<int>(opp.domain) == c.arg; break;
        case CombatAtom::VsDistrict: ok = false; break;  // units only until city combat
        case CombatAtom::CombatType: ok = (c.arg == 1) == x.ranged; break;
        case CombatAtom::TileHills: ok = x.r->terrains[static_cast<size_t>(plot.terrain)].relief == Relief::Hills; break;
        case CombatAtom::TileFeature: ok = plot.feature == c.ref; break;
        case CombatAtom::TileTerrain: ok = plot.terrain == c.ref; break;
        case CombatAtom::OpponentFortified: ok = x.opponent->fortifyTurns > 0; break;
        case CombatAtom::OpponentWounded: ok = x.opponent->hp < x.r->globalInt("COMBAT_MAX_HIT_POINTS"); break;
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
}  // namespace

// ------------------------------------------------------------------ war and peace

bool Game::atWar(PlayerId a, PlayerId b) const {
    if (a == b || a < 0 || b < 0) return false;
    const Player& p = state_.players[static_cast<size_t>(a)];
    return static_cast<size_t>(b) < p.relations.size() && p.relations[static_cast<size_t>(b)].war;
}

bool Game::canDeclareWar(PlayerId player, PlayerId target) const {
    if (target < 0 || static_cast<size_t>(target) >= state_.players.size() || target == player) return false;
    if (!state_.players[static_cast<size_t>(target)].alive) return false;
    const Relation& rel = state_.players[static_cast<size_t>(player)].relations[static_cast<size_t>(target)];
    if (rel.war || state_.turn < rules_->globalInt("DIPLOMACY_EARLIEST_MAJOR_DOW_TURN")) return false;
    // After a peace treaty, war may not resume for DIPLOMACY_PEACE_MIN_TURNS.
    return rel.since == 0 || state_.turn - rel.since >= rules_->globalInt("DIPLOMACY_PEACE_MIN_TURNS");
}

bool Game::canMakePeace(PlayerId player, PlayerId target) const {
    if (!atWar(player, target)) return false;
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
    return typeOf(*rules_, unit).moves + unitEffectTotal(unit, UnitEffectKind::Moves);
}

int Game::unitRange(const Unit& unit) const {
    const int base = typeOf(*rules_, unit).range;
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
    const UnitType& ut = typeOf(*rules_, unit);
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    const bool bombard = attacking && ranged && ut.ranged == 0 && ut.bombard > 0;
    int s = !(attacking && ranged) ? ut.combat : bombard ? ut.bombard : ut.ranged;

    // Promotions and abilities.
    const ConditionContext ctx{&state_, rules_, &unit, &opponent, attacking, ranged};
    int flankPercent = 100, supportPercent = 100;
    bool noRiver = false, noWounded = false, bombardPenalty = false;
    forEachEffect(*rules_, unit, unitAbilities(unit), [&](const UnitEffect& e) {
        switch (e.kind) {
            case UnitEffectKind::Strength: if (conditionsHold(e, ctx)) s += e.amount; break;
            case UnitEffectKind::FlankingPercent: flankPercent += e.amount; break;
            case UnitEffectKind::SupportPercent: supportPercent += e.amount; break;
            case UnitEffectKind::NoRiverPenalty: noRiver = true; break;
            case UnitEffectKind::NoWoundedPenalty: noWounded = true; break;
            case UnitEffectKind::BombardVsUnit: bombardPenalty = true; break;
            default: break;
        }
    });
    if (bombard && bombardPenalty) s -= rules_->globalInt("COMBAT_BOMBARD_VS_UNIT_STRENGTH_MODIFIER");

    if (!attacking) {
        // Terrain defence (hills, woods, marsh...) and fortification.
        const Plot& p = state_.plot(unit.pos);
        s += rules_->terrains[static_cast<size_t>(p.terrain)].defense;
        if (p.feature != kNone) s += rules_->features[static_cast<size_t>(p.feature)].defense;
        s += std::min(unit.fortifyTurns, rules_->globalInt("FORTIFY_TURN_MAX")) * rules_->globalInt("FORTIFY_BONUS_PER_TURN");
    } else if (!ranged) {
        // Attacking across a river (05: COMBAT_RIVER_DEFENSE).
        auto d = state_.grid.directionTo(unit.pos, opponent.pos);
        if (d && !noRiver && hasRiver(state_, unit.pos, *d)) s -= rules_->globalInt("COMBAT_RIVER_DEFENSE");
    }

    // Flanking (melee attacker) and support (defender), once Military Tradition is known.
    if (hasCivicFlag(*rules_, owner, &TreeNode::combatAdjacency) && (!attacking || !ranged)) {
        const Hex around = attacking ? opponent.pos : unit.pos;
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

int Game::combatDamage(int strengthDifference, int roll) const {
    // (COMBAT_BASE_DAMAGE + roll) * e^(COMBAT_POWER_SCALING * difference), at least COMBAT_MINIMUM_DAMAGE.
    const Fixed scale = Fixed::exp(rules_->global("COMBAT_POWER_SCALING") * static_cast<int64_t>(strengthDifference));
    const Fixed dmg = Fixed::fromInt(rules_->globalInt("COMBAT_BASE_DAMAGE") + roll) * scale;
    return std::max(rules_->globalInt("COMBAT_MINIMUM_DAMAGE"), static_cast<int>(dmg.round()));
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
    const Unit* d = state_.unitAt(target, UnitLayer::Military, *rules_);
    if (!d) {
        out.capture = true;
        return out;
    }
    out.defender = d->id;
    out.attackerStrength = combatStrength(*a, *d, true, ranged);
    out.defenderStrength = combatStrength(*d, *a, false, ranged);
    const int diff = out.attackerStrength - out.defenderStrength;
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");
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

void Game::gainXp(Unit& unit, int ownBase, int enemyBase, bool ranged, bool attacker, bool killed) {
    const UnitType& ut = typeOf(*rules_, unit);
    if (ut.promotionClass.empty() || ownBase <= 0) return;
    // 05: enemy base / own base (x2 for a kill), +2 melee or +1 ranged, +1 attacker; at most 8.
    int xp = enemyBase * (killed ? rules_->globalInt("EXPERIENCE_KILL_BONUS") : 1) / ownBase;
    xp += rules_->globalInt(ranged ? "EXPERIENCE_COMBAT_RANGED" : "EXPERIENCE_NOT_COMBAT_RANGED");
    if (attacker) xp += rules_->globalInt("EXPERIENCE_COMBAT_ATTACKER_BONUS");
    xp = std::min(xp, rules_->globalInt("EXPERIENCE_MAXIMUM_ONE_COMBAT"));
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    const int percent = 100 + static_cast<int>(sumUnitXpPercent(state_, *rules_, owner, ut.unitClass).toInt()) +
                        unitEffectTotal(unit, UnitEffectKind::XpPercent);
    xp = xp * percent / 100;
    // XP stops at the next level until the promotion is taken.
    unit.xp = std::min(unit.xp + xp, xpForNextLevel(unit));
}

// ------------------------------------------------------------------ commands

CommandError Game::validateCombat(const Command& c) const {
    switch (c.type) {
        case CommandType::DeclareWar:
            return canDeclareWar(c.player, c.arg) ? CommandError::Ok : CommandError::CannotDeclareWar;
        case CommandType::MakePeace:
            return canMakePeace(c.player, c.arg) ? CommandError::Ok : CommandError::CannotMakePeace;
        default: break;
    }
    const Unit* u = state_.unit(c.id);
    if (!u) return CommandError::BadUnit;
    if (u->owner != c.player) return CommandError::NotYourUnit;
    if (c.type == CommandType::Promote) return canPromote(c.id, c.arg) ? CommandError::Ok : CommandError::CannotPromote;

    const UnitType& ut = typeOf(*rules_, *u);
    auto t = state_.grid.normalize(c.target);
    if (!t || *t != c.target) return CommandError::BadTarget;
    if (ut.layer != UnitLayer::Military || u->movesLeft <= Fixed() || u->attacks >= maxAttacks(*u))
        return CommandError::CannotAttack;
    if (state_.cityAt(*t)) return CommandError::CannotAttack;  // city combat: MVP-4b
    const Unit* defender = state_.unitAt(*t, UnitLayer::Military, *rules_);
    if (defender && !atWar(c.player, defender->owner)) return CommandError::CannotAttack;

    if (c.type == CommandType::RangedAttack) {
        if ((ut.ranged <= 0 && ut.bombard <= 0) || !defender) return CommandError::CannotAttack;
        if (u->moved && unitHas(*u, UnitEffectKind::NoAttackAfterMove) && !unitHas(*u, UnitEffectKind::AttackAfterMove))
            return CommandError::CannotAttack;
        const int dist = state_.grid.distance(u->pos, *t);
        if (dist < 1 || dist > unitRange(*u)) return CommandError::CannotAttack;
        if (visibility(c.player, *t) != Visibility::Visible || !lineOfSight(u->pos, *t)) return CommandError::CannotAttack;
        return CommandError::Ok;
    }
    // Melee: ranged and siege units cannot; the target must be adjacent and enterable.
    if (ut.combat <= 0 || ut.ranged > 0 || ut.bombard > 0) return CommandError::CannotAttack;
    if (state_.grid.distance(u->pos, *t) != 1 || !terrainCost(*u, u->pos, *t)) return CommandError::CannotAttack;
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
            mine = theirs = Relation{true, state_.turn, false};
            return;
        }
        case CommandType::MakePeace: {
            Relation& mine = state_.players[static_cast<size_t>(c.player)].relations[static_cast<size_t>(c.arg)];
            Relation& theirs = state_.players[static_cast<size_t>(c.arg)].relations[static_cast<size_t>(c.player)];
            mine.peaceOffered = true;
            if (theirs.peaceOffered) mine = theirs = Relation{false, state_.turn, false};
            return;
        }
        case CommandType::Promote: {
            Unit* u = state_.unit(c.id);
            u->promotions.push_back(c.arg);
            u->xp = 0;  // excess XP is lost on promotion
            u->hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), u->hp + rules_->globalInt("EXPERIENCE_PROMOTE_HEALED"));
            u->movesLeft = Fixed();  // promoting ends the unit's turn
            return;
        }
        default: break;
    }

    const UnitId attackerId = c.id;
    const PlayerId me = c.player;
    const Hex target = c.target;
    const Unit* d = state_.unitAt(target, UnitLayer::Military, *rules_);
    const bool ranged = c.type == CommandType::RangedAttack;
    Rng& rng = state_.rng.get(RngStream::Combat);
    const int extra = rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE");

    if (!d) {
        // Capture: civilians become the attacker's (Settler, Builder) or are destroyed.
        std::vector<UnitId> there;
        for (const Unit& o : state_.units) if (o.pos == target) there.push_back(o.id);
        for (UnitId id : there) {
            Unit* o = state_.unit(id);
            const TypeIndex becomes = typeOf(*rules_, *o).capturedAs;
            if (becomes == kNone) {
                removeUnit(id);
                continue;
            }
            const PlayerId lost = o->owner;
            o->owner = me;
            o->type = becomes;
            o->movesLeft = Fixed();
            o->activity = Activity::Awake;
            o->moveTarget.reset();
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
        refreshVisibility(me);
        return;
    }

    const UnitId defenderId = d->id;
    const PlayerId them = d->owner;
    Unit* a = state_.unit(attackerId);
    Unit* def = state_.unit(defenderId);
    const int sa = combatStrength(*a, *def, true, ranged);
    const int sd = combatStrength(*def, *a, false, ranged);
    const UnitType& at = typeOf(*rules_, *a);
    const UnitType& dt = typeOf(*rules_, *def);
    const int baseA = !ranged ? at.combat : at.ranged > 0 ? at.ranged : at.bombard;
    const int baseD = dt.combat;

    const int toDefender = combatDamage(sa - sd, rng.range(0, extra));
    const int toAttacker = ranged ? 0 : combatDamage(sd - sa, rng.range(0, extra));
    def->hp -= toDefender;
    a->hp -= toAttacker;
    const bool defenderDied = def->hp <= 0;
    const bool attackerDied = a->hp <= 0;
    if (!attackerDied) gainXp(*a, baseA, baseD, ranged, true, defenderDied);
    if (!defenderDied) gainXp(*def, baseD, baseA, ranged, false, attackerDied);
    if (!attackerDied) afterAttack(*a);

    if (defenderDied) removeUnit(defenderId);
    if (attackerDied) removeUnit(attackerId);
    if (defenderDied && !attackerDied && !ranged) {
        // The melee victor advances, capturing any civilians the defender escorted.
        Unit* civ = nullptr;
        for (Unit& o : state_.units) if (o.pos == target && o.owner == them) civ = &o;
        if (civ) {
            const TypeIndex becomes = typeOf(*rules_, *civ).capturedAs;
            if (becomes == kNone) {
                removeUnit(civ->id);
            } else {
                civ->owner = me;
                civ->type = becomes;
                civ->movesLeft = Fixed();
                civ->activity = Activity::Awake;
                civ->moveTarget.reset();
            }
        }
        Unit* winner = state_.unit(attackerId);
        winner->pos = target;
        winner->moved = true;
    }
    refreshVisibility(me);
    refreshVisibility(them);
}

// ------------------------------------------------------------------ turn upkeep

void Game::payUnitFuel(PlayerId pid) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    std::vector<int> needed(rules_->resources.size(), 0);
    for (const Unit& u : state_.units) {
        const UnitType& ut = typeOf(*rules_, u);
        if (u.owner == pid && ut.resourceMaintenance > 0 && ut.strategicResource != kNone)
            needed[static_cast<size_t>(ut.strategicResource)] += ut.resourceMaintenance;
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
