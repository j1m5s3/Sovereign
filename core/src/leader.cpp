// The playable leader in classic control (specs/sovereign/leader-character-brainstorm.md
// §1, §2, §5, §8.8): one unit per major civ on its own layer, strength from tech-gated
// gear, escorts, capture and barbarian safety. Data in data/rules/leader.json.
#include <algorithm>
#include <optional>

#include "sovereign/game.h"

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
        CommandError why = CommandError::Ok;
        canSucceed(c.player, static_cast<Succession>(c.arg), c.id, &why);
        return why;
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
