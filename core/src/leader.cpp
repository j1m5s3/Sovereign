// The playable leader in classic control (specs/sovereign/leader-character-brainstorm.md
// §1, §2, §5, §8.8): one unit per major civ on its own layer, strength from tech-gated
// gear, escorts, capture and barbarian safety. Data in data/rules/leader.json.
#include <algorithm>
#include <optional>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
const UnitType& typeOf(const Rules& r, const Unit& u) { return r.units[static_cast<size_t>(u.type)]; }

const GearType* worn(const Rules& r, const Unit& u, GearSlot slot) {
    const TypeIndex g = u.gear[static_cast<size_t>(slot)];
    return g == kNone ? nullptr : &r.gear[static_cast<size_t>(g)];
}
}  // namespace

bool Game::isLeader(const Unit& unit) const { return typeOf(*rules_, unit).layer == UnitLayer::Leader; }

const Unit* Game::leaderOf(PlayerId player) const {
    for (const Unit& u : state_.units) {
        if (u.owner == player && isLeader(u)) return &u;
    }
    return nullptr;
}

const Unit* Game::escortOf(const Unit& leader) const {
    for (const Unit& u : state_.units) {
        if (u.escorting == leader.id && u.owner == leader.owner && u.pos == leader.pos) return &u;
    }
    return nullptr;
}

Unit* Game::escortMut(const Unit& leader) {
    const Unit* e = escortOf(leader);
    return e ? state_.unit(e->id) : nullptr;
}

const Unit* Game::defenderAt(Hex plot) const {
    if (const Unit* m = state_.unitAt(plot, UnitLayer::Military, *rules_)) return m;
    return state_.unitAt(plot, UnitLayer::Leader, *rules_);
}

int Game::meleeStrength(const Unit& unit) const {
    if (!isLeader(unit)) return typeOf(*rules_, unit).combat;
    const GearType* w = worn(*rules_, unit, GearSlot::Weapon);
    return w ? w->combat : 0;
}

int Game::rangedStrength(const Unit& unit) const {
    if (!isLeader(unit)) return typeOf(*rules_, unit).ranged;
    const GearType* w = worn(*rules_, unit, GearSlot::Weapon);
    return w ? w->ranged : 0;
}

int Game::gearCost(TypeIndex gear) const {
    const int percent = rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent;
    return rules_->gear[static_cast<size_t>(gear)].goldCost * percent / 100;
}

bool Game::gearUnlocked(PlayerId player, TypeIndex gear) const {
    return hasUnlocked(player, rules_->gear[static_cast<size_t>(gear)].unlock);
}

bool Game::canEquip(UnitId leaderId, TypeIndex gear, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    const Unit* u = state_.unit(leaderId);
    if (!u || !isLeader(*u)) return fail(CommandError::BadUnit);
    if (gear < 0 || static_cast<size_t>(gear) >= rules_->gear.size()) return fail(CommandError::CannotEquip);
    // Gear changes only in one of your own cities and take the leader's turn (§2).
    const City* c = state_.cityAt(u->pos);
    if (!c || c->owner != u->owner || u->movesLeft <= Fixed()) return fail(CommandError::CannotEquip);
    const GearType& g = rules_->gear[static_cast<size_t>(gear)];
    if (u->gear[static_cast<size_t>(g.slot)] == gear || !gearUnlocked(u->owner, gear)) return fail(CommandError::CannotEquip);
    const Player& p = state_.players[static_cast<size_t>(u->owner)];
    if (p.gold < Fixed::fromInt(gearCost(gear))) return fail(CommandError::NotEnoughGold);
    if (g.strategicResource != kNone && p.stockpile[static_cast<size_t>(g.strategicResource)] < g.strategicCost)
        return fail(CommandError::NotEnoughResources);
    if (why) *why = CommandError::Ok;
    return true;
}

int Game::leaderUpkeep(PlayerId player) const {
    const Unit* l = leaderOf(player);
    if (!l) return 0;
    const GearType* m = worn(*rules_, *l, GearSlot::Mount);
    if (!m || m->upkeepAs == kNone) return 0;
    return 2 * rules_->units[static_cast<size_t>(m->upkeepAs)].maintenance;
}

void Game::spawnLeader(PlayerId p, Hex at) {
    Unit& u = spawnUnit(rules_->leaderUnit, p, at);
    // Start with the first item of each slot that needs no tech (Club and Hide).
    for (size_t i = 0; i < rules_->gear.size(); ++i) {
        const GearType& g = rules_->gear[i];
        TypeIndex& slot = u.gear[static_cast<size_t>(g.slot)];
        if (slot == kNone && g.unlock.none() && g.goldCost == 0) slot = static_cast<TypeIndex>(i);
    }
    u.movesLeft = Fixed::fromInt(maxMoves(u));
}

int Game::auraRange(const Unit& leader) const {
    const int per = std::max(1, rules_->globalInt("LEADER_AURA_LEVELS_PER_RANGE"));
    return rules_->globalInt("LEADER_AURA_RANGE") + (leader.level() - 1) / per;
}

bool Game::hasHeir(PlayerId player) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    const Dynasty* d = rules_->dynastyOf(p.civ);
    return d && p.dynastyNext >= 0 && static_cast<size_t>(p.dynastyNext) < d->names.size();
}

std::vector<UnitId> Game::successorUnits(PlayerId player) const {
    std::vector<UnitId> out;
    const int minLevel = rules_->globalInt("LEADER_SUCCESSOR_MIN_LEVEL");
    for (const Unit& u : state_.units) {
        if (u.owner == player && typeOf(*rules_, u).layer == UnitLayer::Military && u.level() >= minLevel) out.push_back(u.id);
    }
    return out;
}

bool Game::canSucceed(PlayerId player, Succession kind, UnitId unit, CommandError* why) const {
    auto result = [&](bool ok) {
        if (why) *why = ok ? CommandError::Ok : CommandError::CannotSucceed;
        return ok;
    };
    if (!state_.players[static_cast<size_t>(player)].successionPending) return result(false);
    const std::vector<UnitId> units = successorUnits(player);
    switch (kind) {
        case Succession::Heir: return result(hasHeir(player));
        case Succession::Unit: return result(std::find(units.begin(), units.end(), unit) != units.end());
        case Succession::Regent: return result(!hasHeir(player) && units.empty());
    }
    return result(false);
}

CommandError Game::validateLeader(const Command& c) const {
    const Player& p = state_.players[static_cast<size_t>(c.player)];
    if (c.type == CommandType::ChooseSuccessor) {
        if (c.arg < 0 || c.arg > static_cast<int32_t>(Succession::Regent)) return CommandError::CannotSucceed;
        // An heir may keep one of the fallen leader's promotions (§5).
        if (c.arg2 != kNone && (static_cast<Succession>(c.arg) != Succession::Heir ||
                                std::find(p.savedPromotions.begin(), p.savedPromotions.end(), c.arg2) == p.savedPromotions.end()))
            return CommandError::CannotSucceed;
        CommandError why = CommandError::Ok;
        canSucceed(c.player, static_cast<Succession>(c.arg), c.id, &why);
        return why;
    }
    if (c.type == CommandType::CityStance) {
        if (c.arg < 0 || c.arg > static_cast<int32_t>(Stance::Fear)) return CommandError::CannotTakeStance;
        CommandError why = CommandError::Ok;
        canTakeStance(c.player, c.id, static_cast<Stance>(c.arg), &why);
        return why;
    }
    if (c.type == CommandType::SendAssassin) {
        const Agent* a = agent(c.id);
        if (!a || a->owner != c.player) return CommandError::CannotSendAgent;
        if (c.arg == kNoPlayer) return CommandError::Ok;
        if (c.arg < 0 || static_cast<size_t>(c.arg) >= state_.players.size() || c.arg == c.player) return CommandError::CannotSendAgent;
        const Player& t = state_.players[static_cast<size_t>(c.arg)];
        return t.alive && !t.barbarian && t.cityState == kNone ? CommandError::Ok : CommandError::CannotSendAgent;
    }
    if (c.type == CommandType::AbandonLeader) return p.captor != kNoPlayer ? CommandError::Ok : CommandError::CannotSucceed;
    const Unit* u = state_.unit(c.id);
    if (!u) return CommandError::BadUnit;
    if (u->owner != c.player) return CommandError::NotYourUnit;
    if (c.type == CommandType::EquipGear) {
        if (c.arg >= 0) {
            CommandError why = CommandError::Ok;
            canEquip(c.id, static_cast<TypeIndex>(c.arg), &why);
            return why;
        }
        // Taking an item off (mounts): in your own city, the slot must hold something.
        if (!isLeader(*u) || c.arg != -1 || c.arg2 < 0 || c.arg2 >= kNumGearSlots) return CommandError::CannotEquip;
        const City* city = state_.cityAt(u->pos);
        if (!city || city->owner != c.player || u->gear[static_cast<size_t>(c.arg2)] == kNone) return CommandError::CannotEquip;
        if (static_cast<GearSlot>(c.arg2) != GearSlot::Mount) return CommandError::CannotEquip;  // weapons and armor are swapped, not removed
        return CommandError::Ok;
    }
    // LinkEscort: a military unit and its own leader on the same plot.
    if (typeOf(*rules_, *u).layer != UnitLayer::Military) return CommandError::CannotEscort;
    if (c.arg == -1) return CommandError::Ok;
    const Unit* l = state_.unit(c.arg);
    if (!l || !isLeader(*l) || l->owner != c.player || l->pos != u->pos) return CommandError::CannotEscort;
    return CommandError::Ok;
}

void Game::applyLeader(const Command& c) {
    Player& p = state_.players[static_cast<size_t>(c.player)];
    if (c.type == CommandType::AbandonLeader) {
        // The captive is given up; the empire crowns someone else (§5).
        p.captor = kNoPlayer;
        p.successionPending = true;
        startInterregnum(p);
        return;
    }
    if (c.type == CommandType::CityStance) {
        City& city = *state_.city(c.id);
        const int effect = rules_->globalInt("STANCE_EFFECT_TURNS");
        const int step = rules_->globalInt("REPUTATION_PER_STANCE");
        if (static_cast<Stance>(c.arg) == Stance::Benevolence) {
            // Hear petitions, give alms, hold a feast: amenities for a while (§4).
            p.gold -= Fixed::fromInt(benevolenceCost(city));
            city.benevolenceUntil = state_.turn + effect;
            p.reputation = std::min(100, p.reputation + step);
        } else {
            // Punishments, a show of force, curfews: order now, resentment later (§4).
            city.loyalty = std::min(rules_->globalInt("LOYALTY_MAXIMUM"), city.loyalty + rules_->globalInt("STANCE_FEAR_LOYALTY"));
            city.fearUntil = state_.turn + effect;
            city.fearAfterUntil = city.fearUntil + rules_->globalInt("STANCE_FEAR_AFTER_TURNS");
            p.reputation = std::max(-100, p.reputation - step);
        }
        city.stanceTurn = state_.turn;
        return;
    }
    if (c.type == CommandType::SendAssassin) {
        for (Agent& a : state_.agents) {
            if (a.id != c.id) continue;
            a.target = static_cast<PlayerId>(c.arg);
            a.travel = c.arg == kNoPlayer ? 0 : std::max(1, rules_->globalInt("ASSASSIN_TRAVEL_TURNS"));
        }
        return;
    }
    if (c.type == CommandType::ChooseSuccessor) {
        const Succession kind = static_cast<Succession>(c.arg);
        const CivType& civ = rules_->civs[static_cast<size_t>(p.civ)];
        std::optional<Hex> at;
        for (const City& city : state_.cities) {
            if (city.owner == c.player && (city.capital || !at)) at = city.pos;
        }
        if (kind == Succession::Unit) {
            const Unit* u = state_.unit(c.id);
            if (!at) at = u->pos;
            p.leaderName = civ.name + " Warlord";
            removeUnit(c.id);
        } else if (kind == Succession::Heir) {
            p.leaderName = rules_->dynastyOf(p.civ)->names[static_cast<size_t>(p.dynastyNext++)];
        } else {
            p.leaderName = civ.name + " Regent";
        }
        if (!at) {
            for (const Unit& o : state_.units) {
                if (o.owner == c.player) {
                    at = o.pos;
                    break;
                }
            }
        }
        spawnLeader(c.player, at.value_or(p.startPos));
        Unit& l = state_.units.back();
        for (size_t slot = 0; slot < l.gear.size(); ++slot) {
            if (p.savedGear[slot] != kNone) l.gear[slot] = p.savedGear[slot];  // the throne's armory passes on
        }
        if (c.arg2 != kNone) l.promotions = {static_cast<TypeIndex>(c.arg2)};
        l.movesLeft = Fixed();
        p.successionPending = false;
        refreshVisibility(c.player);
        return;
    }
    Unit* u = state_.unit(c.id);
    if (c.type == CommandType::LinkEscort) {
        if (c.arg != -1) {
            for (Unit& o : state_.units) {
                if (o.escorting == c.arg) o.escorting = kNoUnit;  // one escort per leader
            }
        }
        u->escorting = c.arg;
        u->moveTarget.reset();
        return;
    }
    if (c.arg == -1) {
        u->gear[static_cast<size_t>(c.arg2)] = kNone;
    } else {
        const TypeIndex gear = static_cast<TypeIndex>(c.arg);
        const GearType& g = rules_->gear[static_cast<size_t>(gear)];
        p.gold -= Fixed::fromInt(gearCost(gear));
        if (g.strategicResource != kNone) p.stockpile[static_cast<size_t>(g.strategicResource)] -= g.strategicCost;
        u->gear[static_cast<size_t>(g.slot)] = gear;
    }
    u->movesLeft = Fixed();  // changing gear takes the leader's turn
    u->moveTarget.reset();
}

void Game::startInterregnum(Player& p) {
    // Policy slots stand empty for a few turns, like Civ's government-change anarchy (§5).
    std::fill(p.policies.begin(), p.policies.end(), kNone);
    p.interregnumTurns = std::max(1, rules_->globalInt("LEADER_INTERREGNUM_TURNS"));
    p.freeChanges = false;
}

void Game::leaderLost(UnitId leader, PlayerId by, bool captured) {
    const Unit* l = state_.unit(leader);
    const PlayerId owner = l->owner;
    Player& p = state_.players[static_cast<size_t>(owner)];
    p.savedGear = l->gear;
    p.savedPromotions = l->promotions;
    for (Unit& o : state_.units) {
        if (o.escorting == leader) o.escorting = kNoUnit;
    }
    removeUnit(leader);
    if (state_.setup.regicide) {
        regicide(owner, by);
        return;
    }
    if (captured) p.captor = by;  // held for ransom; the throne stands empty meanwhile
    else p.successionPending = true;
    startInterregnum(p);
}

void Game::regicide(PlayerId loser, PlayerId by) {
    Player& p = state_.players[static_cast<size_t>(loser)];
    const bool heir = by >= 0 && static_cast<size_t>(by) < state_.players.size() && by != loser &&
                      state_.players[static_cast<size_t>(by)].alive && !state_.players[static_cast<size_t>(by)].barbarian;
    std::vector<CityId> lost;
    for (const City& c : state_.cities) {
        if (c.owner == loser) lost.push_back(c.id);
    }
    for (CityId id : lost) {
        if (!heir) {
            razeCity(id);
            continue;
        }
        City& c = *state_.city(id);
        c.owner = by;
        c.capital = false;
        c.queue.clear();
        c.progress.clear();
        for (Plot& plot : state_.plots) {
            if (plot.city == id) plot.owner = by;
        }
        assignCitizens(c);
    }
    p.alive = false;
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(), [&](const Unit& u) { return u.owner == loser; }),
                       state_.units.end());
    if (heir) refreshVisibility(by);
    // A player who loses the leader on its own turn hands the turn on.
    if (state_.currentPlayer == loser) applyEndTurn(Command::endTurn(loser));
}

// ------------------------------------------------------------------ stances and reputation (§4, §8.1)

int Game::benevolenceCost(const City& city) const {
    const int percent = rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent;
    return rules_->globalInt("STANCE_BENEVOLENCE_GOLD_PER_POP") * city.population * percent / 100;
}

bool Game::fearActive(const City& city) const { return state_.turn < city.fearUntil; }

bool Game::beloved(PlayerId player) const {
    return state_.players[static_cast<size_t>(player)].reputation >= rules_->globalInt("REPUTATION_THRESHOLD");
}

bool Game::feared(PlayerId player) const {
    return state_.players[static_cast<size_t>(player)].reputation <= -rules_->globalInt("REPUTATION_THRESHOLD");
}

bool Game::canTakeStance(PlayerId player, CityId cityId, Stance stance, CommandError* why) const {
    auto result = [&](bool ok) {
        if (why) *why = ok ? CommandError::Ok : CommandError::CannotTakeStance;
        return ok;
    };
    const City* c = state_.city(cityId);
    if (!c || c->owner != player) return result(false);
    // The leader must be there in person, once per cooldown (§4).
    const Unit* l = leaderOf(player);
    if (!l || l->pos != c->pos) return result(false);
    if (state_.turn - c->stanceTurn < rules_->globalInt("STANCE_COOLDOWN_TURNS")) return result(false);
    if (stance == Stance::Benevolence) {
        if (state_.players[static_cast<size_t>(player)].gold < Fixed::fromInt(benevolenceCost(*c))) {
            if (why) *why = CommandError::NotEnoughGold;
            return false;
        }
        return result(true);
    }
    // Fear needs soldiers in the city, like martial law.
    const Unit* m = state_.unitAt(c->pos, UnitLayer::Military, *rules_);
    return result(m && m->owner == player);
}

void Game::rebellion(City& city) {
    const PlayerId barb = barbarianPlayer();
    if (barb == kNoPlayer) return;
    // The rebels carry the strongest melee arms the owner can field.
    TypeIndex best = kNone;
    for (size_t i = 0; i < rules_->units.size(); ++i) {
        const UnitType& t = rules_->units[i];
        if (t.unitClass != "MELEE" || t.domain != Domain::Land || !t.trainable || !hasUnlocked(city.owner, t.unlock)) continue;
        if (best == kNone || t.combat > rules_->units[static_cast<size_t>(best)].combat) best = static_cast<TypeIndex>(i);
    }
    if (best == kNone) return;
    int left = rules_->globalInt("REBELLION_UNITS");
    for (const Hex& h : state_.grid.within(city.pos, 1)) {
        if (left <= 0) break;
        if (h == city.pos || state_.cityAt(h) || state_.unitAt(h, UnitLayer::Military, *rules_)) continue;
        if (!isLandPassable(state_, *rules_, h) || state_.foreignUnitAt(h, barb)) continue;
        spawnUnit(best, barb, h);
        --left;
    }
    pushEvent(EventKind::Rebellion, barb, city.owner, city.id);
    refreshVisibility(barb);
    refreshVisibility(city.owner);
}

// ------------------------------------------------------------------ assassins (§6)

int Game::playerEra(PlayerId player) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    int era = 0;
    for (size_t i = 0; i < rules_->techs.size(); ++i) {
        if (p.techs.has(static_cast<TypeIndex>(i))) era = std::max(era, static_cast<int>(rules_->techs[i].era));
    }
    for (size_t i = 0; i < rules_->civics.size(); ++i) {
        if (p.civics.has(static_cast<TypeIndex>(i))) era = std::max(era, static_cast<int>(rules_->civics[i].era));
    }
    return era;
}

int Game::agentCapacity(PlayerId player) const {
    const TypeIndex encampment = rules_->district("DISTRICT_ENCAMPMENT");
    int n = 0;
    for (const City& c : state_.cities) {
        if (c.owner == player && encampment != kNone && c.district(encampment, true)) ++n;
    }
    return n * rules_->globalInt("ASSASSIN_PER_ENCAMPMENT");
}

int Game::agentsOf(PlayerId player) const {
    return static_cast<int>(std::count_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& a) { return a.owner == player; }));
}

const Agent* Game::agent(int32_t id) const {
    for (const Agent& a : state_.agents) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

int Game::assassinPower(const Agent& a) const {
    return rules_->globalInt("ASSASSIN_BASE_POWER") + rules_->globalInt("ASSASSIN_POWER_PER_LEVEL") * (a.level - 1) +
           rules_->globalInt("ASSASSIN_POWER_PER_ERA") * playerEra(a.owner);
}

int Game::leaderDefenseVsAssassin(const Unit& leader) const {
    int d = meleeStrength(leader);
    const GearType* armor = worn(*rules_, leader, GearSlot::Armor);
    if (armor) d += armor->defense;
    const Plot& p = state_.plot(leader.pos);
    d += rules_->terrains[static_cast<size_t>(p.terrain)].defense;
    if (p.feature != kNone) d += rules_->features[static_cast<size_t>(p.feature)].defense;
    const int maxHp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    d -= rules_->globalInt("COMBAT_WOUNDED_DAMAGE_MULTIPLIER") * (maxHp - leader.hp) / std::max(1, maxHp);
    d += unitEffectTotal(leader, UnitEffectKind::AssassinDefense);
    // Guards on or next to its plot join the fight.
    for (const Unit& u : state_.units) {
        if (u.owner != leader.owner || typeOf(*rules_, u).layer != UnitLayer::Military) continue;
        if (state_.grid.distance(u.pos, leader.pos) <= 1)
            d += meleeStrength(u) * u.hp / std::max(1, maxHp) * rules_->globalInt("ASSASSIN_GUARD_PERCENT") / 100;
    }
    return d;
}

bool Game::leaderExposed(const Unit& leader) const {
    const City* c = state_.cityAt(leader.pos);
    if (!c) return true;
    for (const Unit& u : state_.units) {
        if (u.owner == leader.owner && typeOf(*rules_, u).layer == UnitLayer::Military &&
            state_.grid.distance(u.pos, leader.pos) <= 1)
            return false;
    }
    return true;
}

int Game::assassinSuccessPercent(const Agent& a, const Unit& leader) const {
    const int diff = assassinPower(a) - leaderDefenseVsAssassin(leader);
    int percent = 50 + diff * rules_->globalInt("ASSASSIN_ODDS_PER_POINT");
    // Resentful locals in a city ruled by Fear, and a Feared ruler, help the assassin (§4, §8.1).
    const City* c = state_.cityAt(leader.pos);
    if (c && c->owner == leader.owner && state_.turn < c->fearAfterUntil) percent += rules_->globalInt("STANCE_FEAR_ASSASSIN_BONUS");
    if (feared(leader.owner)) percent += rules_->globalInt("REPUTATION_FEARED_ASSASSIN_BONUS");
    return std::clamp(percent, rules_->globalInt("ASSASSIN_MIN_SUCCESS"), rules_->globalInt("ASSASSIN_MAX_SUCCESS"));
}

void Game::pushEvent(EventKind kind, PlayerId actor, PlayerId target, int value) {
    state_.events.push_back({state_.turn, kind, actor, target, value});
    const size_t cap = 64;
    if (state_.events.size() > cap) state_.events.erase(state_.events.begin(), state_.events.end() - static_cast<long>(cap));
}

void Game::processAgents() {
    std::vector<int32_t> ids;
    for (const Agent& a : state_.agents) ids.push_back(a.id);
    Rng& rng = state_.rng.get(RngStream::Combat);
    for (int32_t id : ids) {
        auto it = std::find_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& x) { return x.id == id; });
        if (it == state_.agents.end() || it->target == kNoPlayer) continue;
        Agent& a = *it;
        const Player& target = state_.players[static_cast<size_t>(a.target)];
        if (!target.alive) {
            a.target = kNoPlayer;  // nothing left to hunt: come home
            continue;
        }
        if (a.travel > 0) {
            --a.travel;
            continue;
        }
        const Unit* leader = leaderOf(a.target);
        if (!leader || !leaderExposed(*leader)) continue;  // wait for an opening
        const int success = assassinSuccessPercent(a, *leader);
        const PlayerId sender = a.owner, victim = a.target;
        const UnitId leaderId = leader->id;
        if (static_cast<int>(rng.below(100)) < success) {
            const int diff = assassinPower(a) - leaderDefenseVsAssassin(*leader);
            const int dmg = combatDamage(diff, rng.range(0, rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE")));
            Unit* l = state_.unit(leaderId);
            l->hp -= dmg;
            a.level = std::min(4, a.level + 1);  // it comes home a level higher
            a.target = kNoPlayer;
            ++state_.players[static_cast<size_t>(sender)].assassinsSent;  // the sender is known
            remember(victim, sender, MemoryKind::Assassin, -15, 60);
            if (l->hp <= 0) {
                pushEvent(EventKind::AssassinKilledLeader, sender, victim, dmg);
                leaderLost(leaderId, sender, false);
            } else {
                pushEvent(EventKind::AssassinWoundedLeader, sender, victim, dmg);
            }
            continue;
        }
        // A miss: the assassin dies in the attempt or is taken alive (the sender is revealed).
        const bool killed = static_cast<int>(rng.below(100)) < rules_->globalInt("ASSASSIN_KILLED_PERCENT");
        state_.agents.erase(it);
        if (killed) {
            awardXp(*state_.unit(leaderId), rules_->globalInt("ASSASSIN_LEADER_XP"), false);
            pushEvent(EventKind::AssassinKilled, sender, victim, 0);
        } else {
            ++state_.players[static_cast<size_t>(sender)].assassinsSent;
            remember(victim, sender, MemoryKind::Assassin, -15, 60);
            pushEvent(EventKind::AssassinCaptured, sender, victim, 0);
        }
    }
}

void Game::barbarianWound(Unit& leader) {
    leader.hp = std::max(leader.hp, 1);
    if (leader.hp > rules_->globalInt("LEADER_RETREAT_HP")) return;
    for (const City& c : state_.cities) {
        if (c.owner != leader.owner || !c.capital || c.pos == leader.pos) continue;
        // Back to the capital (leader doc §1); it shares the plot like any garrison.
        if (Unit* e = escortMut(leader)) e->escorting = kNoUnit;
        leader.pos = c.pos;
        leader.moveTarget.reset();
        leader.movesLeft = Fixed();
        refreshVisibility(leader.owner);
        return;
    }
}

}  // namespace sov
