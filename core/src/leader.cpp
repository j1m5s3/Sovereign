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

bool Game::canAppointBodyguard(PlayerId player, UnitId unit, TypeIndex governor) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    const Unit* leader = leaderOf(player);
    if (!leader || static_cast<int>(p.bodyguards.size()) >= rules_->globalInt("BODYGUARD_MAX")) return false;
    if (unit == kNoUnit) {
        // An appointed governor, wherever it serves.
        return std::any_of(p.governors.begin(), p.governors.end(), [&](const Governor& g) { return g.type == governor && governor != kNone; });
    }
    const Unit* u = state_.unit(unit);
    if (!u || u->owner != player || u->pos != leader->pos || governor != kNone) return false;
    if (u->greatPerson != kNone) {
        const std::vector<UnitId> commanders = successorGreatPeople(player);  // Great Generals and Admirals
        return std::find(commanders.begin(), commanders.end(), unit) != commanders.end();
    }
    return typeOf(*rules_, *u).layer == UnitLayer::Military && u->level() >= rules_->globalInt("BODYGUARD_MIN_LEVEL");
}

int Game::bodyguardDefense(PlayerId player) const {
    int d = 0;
    for (const Bodyguard& b : state_.players[static_cast<size_t>(player)].bodyguards) {
        const char* base = b.kind == BodyguardKind::Commander ? "BODYGUARD_COMMANDER_DEFENSE" : b.kind == BodyguardKind::Steward ? "BODYGUARD_STEWARD_DEFENSE" : "BODYGUARD_SOLDIER_DEFENSE";
        d += rules_->globalInt(base) + rules_->globalInt("BODYGUARD_DEFENSE_PER_LEVEL") * (b.level - 1);
    }
    return d;
}

const Unit* Game::escortOf(const Unit& escorted) const {
    for (const Unit& u : state_.units) {
        if (u.escorting == escorted.id && u.owner == escorted.owner && u.pos == escorted.pos) return &u;
    }
    return nullptr;
}

Unit* Game::escortMut(const Unit& escorted) {
    const Unit* e = escortOf(escorted);
    return e ? state_.unit(e->id) : nullptr;
}

const Unit* Game::defenderAt(Hex plot) const {
    // The first military unit there, else the first leader (as unitAt finds each), from one look at the units.
    const Unit* leader = nullptr;
    for (const Unit& u : state_.units) {
        if (u.pos != plot) continue;
        const UnitLayer layer = rules_->units[static_cast<size_t>(u.type)].layer;
        if (layer == UnitLayer::Military) return &u;
        if (layer == UnitLayer::Leader && !leader) leader = &u;
    }
    return leader;
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

std::vector<UnitId> Game::successorGreatPeople(PlayerId player) const {
    std::vector<UnitId> out;
    const TypeIndex general = rules_->greatPersonClass("GREAT_PERSON_CLASS_GENERAL");
    const TypeIndex admiral = rules_->greatPersonClass("GREAT_PERSON_CLASS_ADMIRAL");
    for (const Unit& u : state_.units) {
        if (u.owner != player || u.greatPerson == kNone) continue;
        const TypeIndex cls = rules_->greatPeople[static_cast<size_t>(u.greatPerson)].cls;
        if (cls != kNone && (cls == general || cls == admiral)) out.push_back(u.id);
    }
    return out;
}

std::vector<TypeIndex> Game::successorGovernors(PlayerId player) const {
    std::vector<TypeIndex> out;
    for (const Governor& g : state_.players[static_cast<size_t>(player)].governors) out.push_back(g.type);
    return out;
}

std::vector<TypeIndex> Game::successorPromotions(PlayerId /*player*/, Succession kind, UnitId unit) const {
    // Sovereign tuning (§5): a veteran unit brings the Warlord's first promotion, a Great General or Admiral
    // both (a higher level and a stronger aura), and a governor the first of the branch its specialty seeds:
    // Victor Warlord; Amani, Pingala and Moksha Statesman; Magnus, Liang and Reyna Builder-King.
    const TypeIndex warlord = rules_->promotion("PROMOTION_SOVEREIGN_WEAPON_MASTER");
    if (kind == Succession::Unit) return {warlord};
    if (kind == Succession::GreatPerson) return {warlord, rules_->promotion("PROMOTION_SOVEREIGN_MARSHAL")};
    if (kind != Succession::Governor || unit < 0 || static_cast<size_t>(unit) >= rules_->governors.size()) return {};
    const std::string& id = rules_->governors[static_cast<size_t>(unit)].id;
    if (id == "GOVERNOR_VICTOR") return {warlord};
    if (id == "GOVERNOR_AMANI" || id == "GOVERNOR_PINGALA" || id == "GOVERNOR_MOKSHA") return {rules_->promotion("PROMOTION_SOVEREIGN_WARY")};
    return {rules_->promotion("PROMOTION_SOVEREIGN_OVERSEER")};
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
        // Governors and great people do not bar a regent: giving one up is a choice (§5).
        case Succession::Regent: return result(!hasHeir(player) && units.empty());
        case Succession::Governor: {
            const std::vector<TypeIndex> govs = successorGovernors(player);
            return result(std::find(govs.begin(), govs.end(), static_cast<TypeIndex>(unit)) != govs.end());
        }
        case Succession::GreatPerson: {
            const std::vector<UnitId> people = successorGreatPeople(player);
            return result(std::find(people.begin(), people.end(), unit) != people.end());
        }
    }
    return result(false);
}

CommandError Game::validateLeader(const Command& c) const {
    const Player& p = state_.players[static_cast<size_t>(c.player)];
    if (c.type == CommandType::ChooseSuccessor) {
        if (c.arg < 0 || c.arg > static_cast<int32_t>(Succession::GreatPerson)) return CommandError::CannotSucceed;
        // An heir may keep one of the fallen leader's promotions (§5).
        if (c.arg2 != kNone && (static_cast<Succession>(c.arg) != Succession::Heir ||
                                std::find(p.savedPromotions.begin(), p.savedPromotions.end(), c.arg2) == p.savedPromotions.end()))
            return CommandError::CannotSucceed;
        CommandError why = CommandError::Ok;
        canSucceed(c.player, static_cast<Succession>(c.arg), c.id, &why);
        return why;
    }
    if (c.type == CommandType::AppointBodyguard) return canAppointBodyguard(c.player, c.id, static_cast<TypeIndex>(c.arg)) ? CommandError::Ok : CommandError::CannotGuard;
    if (c.type == CommandType::CityStance) {
        if (c.arg < 0 || c.arg > static_cast<int32_t>(Stance::Fear)) return CommandError::CannotTakeStance;
        CommandError why = CommandError::Ok;
        canTakeStance(c.player, c.id, static_cast<Stance>(c.arg), &why);
        return why;
    }
    if (c.type == CommandType::SendAssassin) {
        const Agent* a = agent(c.id);
        if (!a || a->owner != c.player || a->spy) return CommandError::CannotSendAgent;  // spies take SpyMission
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
    // LinkEscort: a military unit and its own leader or civilian on the same plot (05: Formations).
    if (typeOf(*rules_, *u).layer != UnitLayer::Military) return CommandError::CannotEscort;
    if (c.arg == -1) return CommandError::Ok;
    const Unit* l = state_.unit(c.arg);
    if (!l || !(isLeader(*l) || typeOf(*rules_, *l).layer == UnitLayer::Civilian) || l->owner != c.player || l->pos != u->pos)
        return CommandError::CannotEscort;
    return CommandError::Ok;
}

void Game::applyLeader(const Command& c) {
    Player& p = state_.players[static_cast<size_t>(c.player)];
    if (c.type == CommandType::AppointBodyguard) {
        // Sworn to the ruler (§8.3): the unit, Great Person or governor leaves its post for good.
        Bodyguard b;
        if (c.id == kNoUnit) {
            const TypeIndex type = static_cast<TypeIndex>(c.arg);
            auto& govs = p.governors;
            const auto g = std::find_if(govs.begin(), govs.end(), [&](const Governor& x) { return x.type == type; });
            b = {rules_->governors[static_cast<size_t>(type)].name, BodyguardKind::Steward, 1 + static_cast<int32_t>(g->promotions.size())};
            govs.erase(g);
        } else {
            const Unit& u = *state_.unit(c.id);
            if (u.greatPerson != kNone) b = {rules_->greatPeople[static_cast<size_t>(u.greatPerson)].name, BodyguardKind::Commander, 1};
            else b = {typeOf(*rules_, u).name + " veteran", BodyguardKind::Soldier, u.level()};
            removeUnit(c.id);
        }
        b.level = std::min(b.level, rules_->globalInt("BODYGUARD_MAX_LEVEL"));
        p.bodyguards.push_back(b);
        return;
    }
    if (c.type == CommandType::AbandonLeader) {
        // The captive is given up; the empire crowns someone else at a heavier loyalty cost (§5).
        p.captor = kNoPlayer;
        p.successionPending = true;
        successionShock(c.player, rules_->globalInt("LEADER_ABANDON_LOYALTY"));
        startInterregnum(p);
        return;
    }
    if (c.type == CommandType::CityStance) {
        City& city = *state_.city(c.id);
        const int effect = rules_->globalInt("STANCE_EFFECT_TURNS");
        const int step = rules_->globalInt("REPUTATION_PER_STANCE");
        // A Statesman handles citizens better (§3): each of its promotions strengthens the outcome.
        const Unit* ruler = leaderOf(c.player);
        const int power = 100 + (ruler ? unitEffectTotal(*ruler, UnitEffectKind::StancePower) : 0);
        if (static_cast<Stance>(c.arg) == Stance::Benevolence) {
            // Hear petitions, give alms, hold a feast: amenities for a while (§4).
            p.gold -= Fixed::fromInt(benevolenceCost(city));
            city.benevolenceUntil = state_.turn + effect * power / 100;
            p.reputation = std::min(100, p.reputation + step);
        } else {
            // Punishments, a show of force, curfews: order now, resentment later (§4).
            city.loyalty = std::min(rules_->globalInt("LOYALTY_MAXIMUM"), city.loyalty + rules_->globalInt("STANCE_FEAR_LOYALTY") * power / 100);
            city.fearUntil = state_.turn + effect;
            city.fearAfterUntil = city.fearUntil + rules_->globalInt("STANCE_FEAR_AFTER_TURNS");
            p.reputation = std::max(-100, p.reputation - step);
            fearGrievances(c.player, city);
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
        std::optional<Hex> at = throneCity(c.player);
        const std::vector<TypeIndex> seeded = successorPromotions(c.player, kind, c.id);
        if (kind == Succession::Unit || kind == Succession::GreatPerson) {
            const Unit* u = state_.unit(c.id);
            if (!at) at = u->pos;
            p.leaderName = civ.name + (kind == Succession::Unit ? " Warlord" : " Marshal");
            p.rulingHeir = -1;
            removeUnit(c.id);  // the unit or the Great Person is gone
        } else if (kind == Succession::Governor) {
            // The governor leaves their post; the titles spent on them are lost with them.
            const TypeIndex type = static_cast<TypeIndex>(c.id);
            auto& govs = p.governors;
            const auto g = std::find_if(govs.begin(), govs.end(), [&](const Governor& x) { return x.type == type; });
            if (const City* seat = g->city != kNoCity ? state_.city(g->city) : nullptr; !at && seat) at = seat->pos;
            p.leaderName = rules_->governors[static_cast<size_t>(type)].name;
            p.rulingHeir = -1;
            govs.erase(g);
        } else if (kind == Succession::Heir) {
            p.rulingHeir = p.dynastyNext;  // the heir's trait rules with them
            p.leaderName = rules_->dynastyOf(p.civ)->names[static_cast<size_t>(p.dynastyNext++)];
        } else {
            p.leaderName = civ.name + " Regent";
            p.rulingHeir = -1;
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
        for (TypeIndex promo : seeded) {
            if (promo != kNone) l.promotions.push_back(promo);
        }
        l.movesLeft = Fixed();
        p.successionPending = false;
        refreshVisibility(c.player);
        // value: the Succession kind, and for an heir its place in the dynasty (x16) for the chronicle.
        pushEvent(EventKind::Succession, c.player, kNoPlayer, c.arg + (kind == Succession::Heir ? 16 * p.rulingHeir : 0));
        return;
    }
    Unit* u = state_.unit(c.id);
    if (c.type == CommandType::LinkEscort) {
        if (c.arg != -1) {
            for (Unit& o : state_.units) {
                if (o.escorting == c.arg) o.escorting = kNoUnit;  // one escort per unit
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

void Game::leaderLost(UnitId leader, PlayerId by, bool captured, bool inBattle) {
    const Unit* l = state_.unit(leader);
    const PlayerId owner = l->owner;
    Player& p = state_.players[static_cast<size_t>(owner)];
    p.savedGear = l->gear;
    p.savedPromotions = l->promotions;
    p.bodyguards.clear();  // they fall or are taken with their ruler
    for (Unit& o : state_.units) {
        if (o.escorting == leader) o.escorting = kNoUnit;
    }
    removeUnit(leader);
    if (inBattle) pushEvent(EventKind::LeaderLost, by, owner, captured ? 1 : 0);
    if (state_.setup.regicide) {
        regicide(owner, by);
        return;
    }
    if (captured) {
        p.captor = by;  // held for ransom; the throne stands empty meanwhile
        p.capturedTurn = state_.turn;
    } else {
        p.successionPending = true;
        successionShock(owner, rules_->globalInt("LEADER_LOSS_LOYALTY"));
        addGrievance(owner, by, rules_->globalInt("LEADER_KILLED_GRIEVANCES"));  // the killer is known (§5)
    }
    startInterregnum(p);
}

void Game::leaderVisit(const Unit& leader) {
    const City* c = state_.landCity(leader.pos);
    if (!c || c->owner != leader.owner || (c->pos != leader.pos && !state_.districtAt(leader.pos))) return;
    std::vector<int32_t>& seen = state_.players[static_cast<size_t>(leader.owner)].leaderVisits;
    const int32_t key = static_cast<int32_t>(state_.grid.index(leader.pos));
    const auto it = std::lower_bound(seen.begin(), seen.end(), key);
    if (it != seen.end() && *it == key) return;
    seen.insert(it, key);
    leaderXp(leader.owner, rules_->globalInt("LEADER_XP_FIRST_VISIT"));
}

void Game::inPersonVisit(const Unit& leader) {
    Player& p = state_.players[static_cast<size_t>(leader.owner)];
    for (const City& c : state_.cities) {
        if (state_.grid.distance(c.pos, leader.pos) > 1 || !isCityState(c.owner) || atWar(leader.owner, c.owner)) continue;
        // Once per city-state city a game, kept with the ruler's visits (a city center plot is never also its own).
        std::vector<int32_t>& seen = p.leaderVisits;
        const int32_t key = static_cast<int32_t>(state_.grid.index(c.pos));
        const auto it = std::lower_bound(seen.begin(), seen.end(), key);
        if (it != seen.end() && *it == key) continue;
        seen.insert(it, key);
        if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
        p.envoys[static_cast<size_t>(c.owner)] += rules_->globalInt("IN_PERSON_ENVOYS");
    }
}

bool Game::rulerVisiting(PlayerId player, PlayerId host) const {
    const Unit* l = leaderOf(player);
    if (!l) return false;
    for (const City& c : state_.cities) {
        if (c.owner == host && c.capital) return state_.grid.distance(c.pos, l->pos) <= 1;
    }
    return false;
}

void Game::duelWon(PlayerId winner, PlayerId loser) {
    const int points = rules_->globalInt("DUEL_WAR_WEARINESS");
    addWarWeariness(loser, winner, points);
    std::vector<int32_t>& mine = state_.players[static_cast<size_t>(winner)].warWeariness;
    if (static_cast<size_t>(loser) < mine.size()) mine[static_cast<size_t>(loser)] = std::max(0, mine[static_cast<size_t>(loser)] - points);
}

void Game::leaderXp(PlayerId player, int xp) {
    if (xp <= 0 || player < 0) return;
    if (const Unit* l = leaderOf(player)) awardXp(*state_.unit(l->id), xp, false);
}

void Game::fearGrievances(PlayerId ruler, const City& city) {
    const int amount = rules_->globalInt("STANCE_FEAR_GRIEVANCES");
    const int faith = civReligion(ruler);
    for (const Player& o : state_.players) {
        if (o.id == ruler || !isMajorCiv(o.id)) continue;  // alliance() is None for the original owner with itself
        const bool coreligionist = faith >= 0 && civReligion(o.id) == faith;
        const bool ally = city.originalOwner >= 0 && city.originalOwner != ruler &&
                          alliance(o.id, city.originalOwner) != AllianceType::None;
        if (coreligionist || ally) addGrievance(o.id, ruler, amount);
    }
}

void Game::successionShock(PlayerId owner, int loyaltyDrop) {
    for (City& c : state_.cities) {
        if (c.owner == owner) c.loyalty = std::max(0, c.loyalty - loyaltyDrop);
    }
    Player& p = state_.players[static_cast<size_t>(owner)];
    const int lost = std::clamp(p.eraScore, 0, rules_->globalInt("LEADER_LOSS_ERA_SCORE"));
    p.eraScore -= lost;
    p.eraScoreTotal -= lost;
}

std::optional<Hex> Game::throneCity(PlayerId player) const {
    std::optional<Hex> at;
    for (const City& city : state_.cities) {
        if (city.owner == player && (city.capital || !at)) at = city.pos;
    }
    return at;
}

void Game::ransomRuler(PlayerId owner) {
    // The captive comes home to the capital with its loadout and promotions; the interregnum ends next turn (§5).
    Player& p = state_.players[static_cast<size_t>(owner)];
    pushEvent(EventKind::RulerRansomed, p.captor, owner, 0);
    p.captor = kNoPlayer;
    spawnLeader(owner, throneCity(owner).value_or(p.startPos));
    Unit& l = state_.units.back();
    for (size_t slot = 0; slot < l.gear.size(); ++slot) {
        if (p.savedGear[slot] != kNone) l.gear[slot] = p.savedGear[slot];
    }
    l.promotions = p.savedPromotions;
    l.movesLeft = Fixed();
    p.interregnumTurns = std::min(p.interregnumTurns, 1);
    refreshVisibility(owner);
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
    return state_.players[static_cast<size_t>(player)].reputation >= rules_->globalInt(HotGlobal::ReputationThreshold);
}

bool Game::feared(PlayerId player) const {
    return state_.players[static_cast<size_t>(player)].reputation <= -rules_->globalInt(HotGlobal::ReputationThreshold);
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
    // The rebels carry the strongest melee arms the owner can field (a city-state's own, Lahore's Nihang, aside).
    TypeIndex best = kNone;
    for (size_t i = 0; i < rules_->units.size(); ++i) {
        const UnitType& t = rules_->units[i];
        if (t.unitClass != "MELEE" || t.domain != Domain::Land || !t.trainable || t.cityState != kNone || !hasUnlocked(city.owner, t.unlock)) continue;
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
    // The latest era of a tech or civic researched, read off compact lists of their eras.
    // Bytes, each era masked by whether it is done (0xFF or 0) and maxed rather than branched on, so the compiler can
    // do many at once.
    auto latest = [](const std::vector<uint8_t>& done, const std::vector<uint8_t>& eras, uint8_t era) {
        const size_t n = std::min(done.size(), eras.size());
        for (size_t i = 0; i < n; ++i) era = std::max(era, static_cast<uint8_t>(eras[i] & static_cast<uint8_t>(-(done[i] != 0))));
        return era;
    };
    return latest(p.civics.done, civicEras_, latest(p.techs.done, techEras_, 0));
}

int Game::techEra(PlayerId player) const {
    const std::vector<uint8_t>& done = state_.players[static_cast<size_t>(player)].techs.done;
    int era = 0;
    for (size_t i = 0; i < done.size() && i < techEras_.size(); ++i) {
        if (done[i]) era = std::max(era, static_cast<int>(techEras_[i]));
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
    // Assassins only: spies have their own capacity (08: Espionage).
    return static_cast<int>(std::count_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& a) { return !a.spy && a.owner == player; }));
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
    d += bodyguardDefense(leader.owner);  // sworn companions, always at its side (§8.3)
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
    if (chronicleWorthy(kind)) recordChronicle(state_.events.back());
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
            Player& prey = state_.players[static_cast<size_t>(victim)];
            if (prey.bodyDoubles > 0 && static_cast<int>(rng.below(100)) < rules_->globalInt("BODY_DOUBLE_PERCENT")) {
                // The blow falls on a body double (§8.6): it dies, the ruler is unhurt, and the sender is known.
                --prey.bodyDoubles;
                a.target = kNoPlayer;
                ++state_.players[static_cast<size_t>(sender)].assassinsSent;
                remember(victim, sender, MemoryKind::Assassin, -15, 60);
                addGrievance(victim, sender, rules_->globalInt("ASSASSIN_SENDER_GRIEVANCES"));
                pushEvent(EventKind::AssassinKilledDouble, sender, victim, 0);
                continue;
            }
            if (!prey.bodyguards.empty() && static_cast<int>(rng.below(100)) < rules_->globalInt("BODYGUARD_SHIELD_PERCENT")) {
                // A bodyguard throws themself in the way (§8.3): the lowest-level one dies, and the sender is known.
                const auto weakest = std::min_element(prey.bodyguards.begin(), prey.bodyguards.end(),
                                                      [](const Bodyguard& x, const Bodyguard& y) { return x.level < y.level; });
                prey.bodyguards.erase(weakest);
                a.target = kNoPlayer;
                ++state_.players[static_cast<size_t>(sender)].assassinsSent;
                remember(victim, sender, MemoryKind::Assassin, -15, 60);
                addGrievance(victim, sender, rules_->globalInt("ASSASSIN_SENDER_GRIEVANCES"));
                pushEvent(EventKind::AssassinKilledGuard, sender, victim, 0);
                continue;
            }
            const int diff = assassinPower(a) - leaderDefenseVsAssassin(*leader);
            const int dmg = combatDamage(diff, rng.range(0, rules_->globalInt("COMBAT_MAX_EXTRA_DAMAGE")));
            Unit* l = state_.unit(leaderId);
            l->hp -= dmg;
            a.level = std::min(4, a.level + 1);  // it comes home a level higher
            a.target = kNoPlayer;
            ++state_.players[static_cast<size_t>(sender)].assassinsSent;  // the sender is known
            remember(victim, sender, MemoryKind::Assassin, -15, 60);
            addGrievance(victim, sender, rules_->globalInt("ASSASSIN_SENDER_GRIEVANCES"));  // §6
            if (l->hp <= 0) {
                pushEvent(EventKind::AssassinKilledLeader, sender, victim, dmg);
                leaderLost(leaderId, sender, false, false);
            } else {
                pushEvent(EventKind::AssassinWoundedLeader, sender, victim, dmg);
            }
            continue;
        }
        // A miss: the assassin dies in the attempt or is taken alive (the sender is revealed).
        const bool killed = static_cast<int>(rng.below(100)) < rules_->globalInt("ASSASSIN_KILLED_PERCENT");
        state_.agents.erase(it);
        // Each bodyguard who saw off an assassin grows in skill.
        for (Bodyguard& b : state_.players[static_cast<size_t>(victim)].bodyguards) b.level = std::min(b.level + 1, rules_->globalInt("BODYGUARD_MAX_LEVEL"));
        if (killed) {
            awardXp(*state_.unit(leaderId), rules_->globalInt("ASSASSIN_LEADER_XP"), false);
            pushEvent(EventKind::AssassinKilled, sender, victim, 0);
        } else {
            ++state_.players[static_cast<size_t>(sender)].assassinsSent;
            remember(victim, sender, MemoryKind::Assassin, -15, 60);
            addGrievance(victim, sender, rules_->globalInt("ASSASSIN_SENDER_GRIEVANCES"));  // the captive names its sender (§6)
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

std::vector<LeaderGoal> Game::leaderGoals(PlayerId player) const {
    std::vector<LeaderGoal> out;
    const Unit* leader = leaderOf(player);
    if (!leader) return out;
    // An assassin in place to strike is reported, not who sent it (§6).
    for (const Agent& a : state_.agents) {
        if (!a.spy && a.owner != player && a.target == player && a.travel == 0) {
            out.push_back({LeaderGoalKind::AssassinNear, leaderExposed(*leader) ? 1 : 0, -1, leader->pos});
            break;
        }
    }
    // Cities in Unrest, unhappy enough to rebel, or within 10 turns of revolting: a visit's stance helps (§4).
    std::vector<LeaderGoal> cities;
    const int unrestBelow = rules_->loyaltyLevels.size() > 1 ? rules_->loyaltyLevels[1].minLoyalty : 0;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        const int perTurn = static_cast<int>(loyaltyPerTurn(c.id).toInt());
        const bool revolting = perTurn < 0 && c.loyalty <= -perTurn * 10;
        if (c.loyalty < unrestBelow || c.rebellion > 0 || revolting) cities.push_back({LeaderGoalKind::CityUnrest, c.loyalty, c.id, c.pos});
    }
    std::sort(cities.begin(), cities.end(), [](const LeaderGoal& a, const LeaderGoal& b) { return a.value != b.value ? a.value < b.value : a.id < b.id; });
    out.insert(out.end(), cities.begin(), cities.end());
    // Rival leaders in sight within 3 plots: a melee on one goes live (leader doc §9).
    std::vector<LeaderGoal> rivals;
    for (const Unit& u : state_.units) {
        if (u.owner == player || !isLeader(u) || visibility(player, u.pos) != Visibility::Visible) continue;
        const int d = state_.grid.distance(leader->pos, u.pos);
        if (d <= 3) rivals.push_back({LeaderGoalKind::RivalLeaderNear, d, u.owner, u.pos});
    }
    std::sort(rivals.begin(), rivals.end(), [](const LeaderGoal& a, const LeaderGoal& b) { return a.value != b.value ? a.value < b.value : a.id < b.id; });
    out.insert(out.end(), rivals.begin(), rivals.end());
    // A promotion to choose, or one within half a level's XP.
    const int next = xpForNextLevel(*leader);
    if (leader->xp >= next) out.push_back({LeaderGoalKind::Promotion, 0, leader->id, leader->pos});
    else if ((next - leader->xp) * 2 <= next) out.push_back({LeaderGoalKind::PromotionSoon, next - leader->xp, leader->id, leader->pos});
    return out;
}

}  // namespace sov
