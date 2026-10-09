// Archaeology (07-great-people-great-works-tourism.md, Archaeology; data: ARCHAEOLOGY_*). While the
// world era is at most ARCHAEOLOGY_MAX_ERA, fought-over plots are remembered. Once any civ knows Natural
// History, ARCHAEOLOGY_SITES_PER_CIV_LAND antiquity sites and ARCHAEOLOGY_SITES_PER_CIV_SEA shipwrecks per
// major civ appear: on those battle plots first, then on other open plots (Sovereign reading of "similar
// history"). Only civs that know Natural History see them. An Archaeologist (three excavations) on a
// site in its civ's land, unowned land, or land whose owner opened its borders to it digs an Artifact
// into a free artifact slot (an Archaeological Museum's) of one of its cities.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::seesAntiquity(PlayerId player, int kind) const {
    const TypeIndex nh = rules_->civic(kind == 2 ? "CIVIC_CULTURAL_HERITAGE" : "CIVIC_NATURAL_HISTORY");
    return nh != kNone && player >= 0 && state_.players[at(player)].civics.has(nh);
}

void Game::noteBattle(Hex plot, PlayerId attacker) {
    if (state_.antiquityPlaced || state_.gameEra > rules_->globalInt("ARCHAEOLOGY_MAX_ERA") || state_.battleSites.size() >= 400) return;
    const int32_t i = state_.grid.index(plot);
    if (std::find(state_.battleSites.begin(), state_.battleSites.end(), i) != state_.battleSites.end()) return;
    state_.battleSites.push_back(i);
    // The Artifact found here later belongs to the attacker's civilization and the era of the fight.
    const TypeIndex civ = attacker >= 0 ? state_.players[at(attacker)].civ : kNone;
    state_.battleHistory.push_back(state_.gameEra * 4096 + civ + 1);
}

// Theming (07: Theming bonuses; data: buildings' theming flags): every slot of the building full, and its
// works by different people and of one object type (Art Museum), or of one era and from different
// civilizations (Archaeological Museum).
bool Game::themed(const City& city, TypeIndex building) const {
    const BuildingType& b = rules_->buildings[at(building)];
    if (!b.theming) return false;
    int slots = 0;
    for (const auto& [slot, n] : b.greatWorkSlots) slots += n;
    std::vector<const GreatWork*> works;
    for (const GreatWork& w : city.greatWorks) {
        if (w.building == building) works.push_back(&w);
    }
    if (slots <= 1 || static_cast<int>(works.size()) < slots) return false;
    for (size_t i = 0; i < works.size(); ++i) {
        for (size_t k = i + 1; k < works.size(); ++k) {
            const GreatWork& x = *works[i];
            const GreatWork& y = *works[k];
            if (b.theming->sameObject && x.type != y.type) return false;
            if (b.theming->uniquePerson && (x.creator == kNone || x.creator == y.creator)) return false;
            if (b.theming->sameEra && (x.era < 0 || x.era != y.era)) return false;
            if (b.theming->uniqueCivs && (x.civ == kNone || x.civ == y.civ)) return false;
        }
    }
    return true;
}

// Moving Great Works (07: Great Works): any of a civ's works to a free, compatible slot in any of its cities.
int Game::freeSlotsFor(const City& city, TypeIndex building, TypeIndex workType) const {
    if (building < 0 || static_cast<size_t>(building) >= rules_->buildings.size() || !city.has(building)) return 0;
    const GreatWorkType& w = rules_->greatWorkTypes[at(workType)];
    int free = std::find(w.slots.begin(), w.slots.end(), "PALACE") != w.slots.end() ? extraPalaceSlots(city, building) : 0;  // Medici (07)
    if (std::find(w.slots.begin(), w.slots.end(), "PRODUCT") != w.slots.end()) free += extraProductSlots(building);  // Monopolies (07)
    for (const auto& [slot, count] : rules_->buildings[at(building)].greatWorkSlots) {
        if (std::find(w.slots.begin(), w.slots.end(), slot) != w.slots.end()) free += count;
    }
    for (const GreatWork& g : city.greatWorks) free -= g.building == building ? 1 : 0;
    return std::max(0, free);
}

CommandError Game::moveGreatWorkProblem(PlayerId player, CityId from, int index, CityId to, TypeIndex building) const {
    const City* a = state_.city(from);
    const City* b = state_.city(to);
    if (!a || !b || a->owner != player || b->owner != player) return CommandError::NotYourCity;
    if (index < 0 || static_cast<size_t>(index) >= a->greatWorks.size()) return CommandError::BadTarget;
    const GreatWork& w = a->greatWorks[static_cast<size_t>(index)];
    if (from == to && w.building == building) return CommandError::BadTarget;
    if (state_.turn < w.lockedUntil) return CommandError::BadTarget;  // art just moved or traded (07)
    return freeSlotsFor(*b, building, w.type) > 0 ? CommandError::Ok : CommandError::BadTarget;
}

void Game::moveGreatWork(CityId from, int index, CityId to, TypeIndex building) {
    City& a = *state_.city(from);
    GreatWork w = a.greatWorks[static_cast<size_t>(index)];
    a.greatWorks.erase(a.greatWorks.begin() + index);
    w.building = building;
    lockArt(w);
    state_.city(to)->greatWorks.push_back(w);
}

// Art (sculpture, portrait, landscape, religious art) moved or traded is locked for GREATWORK_ART_LOCK_TIME turns (07).
void Game::lockArt(GreatWork& w) const {
    const std::string& kind = rules_->greatWorkTypes[at(w.type)].id;
    if (kind == "SCULPTURE" || kind == "PORTRAIT" || kind == "LANDSCAPE" || kind == "RELIGIOUS")
        w.lockedUntil = state_.turn + rules_->globalInt("GREATWORK_ART_LOCK_TIME");
}

const GreatWork* Game::dealWork(const DealItem& item) const {
    const City* c = state_.city(item.amount);
    if (!c || c->owner != item.from || item.resource < 0 || static_cast<size_t>(item.resource) >= c->greatWorks.size()) return nullptr;
    return &c->greatWorks[static_cast<size_t>(item.resource)];
}

bool Game::workCompletesTheme(PlayerId player, const GreatWork& work) const {
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (TypeIndex b : c.buildings) {
            const BuildingType& bt = rules_->buildings[at(b)];
            if (!bt.theming || themed(c, b)) continue;
            const GreatWorkType& t = rules_->greatWorkTypes[at(work.type)];
            bool fits = false;
            for (const auto& [slot, n] : bt.greatWorkSlots) fits = fits || std::find(t.slots.begin(), t.slots.end(), slot) != t.slots.end();
            if (!fits) continue;
            int slots = 0;
            for (const auto& [slot, n] : bt.greatWorkSlots) slots += n;
            const bool byObject = bt.theming->sameObject || bt.theming->uniquePerson;
            const int key = byObject ? work.type : work.era;
            const int who = byObject ? work.creator : work.civ;
            if (key < 0 || who < 0) continue;
            // Distinct people (or civilizations) of the work's theme the player already holds outside themed buildings.
            std::vector<int> seen;
            for (const City& o : state_.cities) {
                if (o.owner != player) continue;
                for (const GreatWork& w : o.greatWorks) {
                    if (themed(o, w.building)) continue;
                    if ((byObject ? w.type : w.era) != key) continue;
                    const int x = byObject ? w.creator : w.civ;
                    if (x >= 0 && std::find(seen.begin(), seen.end(), x) == seen.end()) seen.push_back(x);
                }
            }
            if (std::find(seen.begin(), seen.end(), who) == seen.end() && static_cast<int>(seen.size()) == slots - 1) return true;
        }
    }
    return false;
}

std::vector<Command> Game::themingMoves(PlayerId player, CityId cityId, TypeIndex building) const {
    std::vector<Command> moves;
    const City* city = state_.city(cityId);
    if (!city || city->owner != player || building < 0 || !city->has(building) || themed(*city, building)) return moves;
    const BuildingType& b = rules_->buildings[at(building)];
    if (!b.theming) return moves;
    int slots = 0;
    for (const auto& [slot, n] : b.greatWorkSlots) slots += n;
    // Every work the player could put here: in this building, or elsewhere outside a themed building.
    struct Cand { CityId city; int index; const GreatWork* work; };
    std::vector<Cand> cands;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (size_t i = 0; i < c.greatWorks.size(); ++i) {
            const GreatWork& w = c.greatWorks[i];
            const bool here = c.id == cityId && w.building == building;
            if (!here && (themed(c, w.building) || state_.turn < w.lockedUntil)) continue;
            const GreatWorkType& t = rules_->greatWorkTypes[at(w.type)];
            bool fits = false;
            for (const auto& [slot, n] : b.greatWorkSlots) fits = fits || std::find(t.slots.begin(), t.slots.end(), slot) != t.slots.end();
            if (fits) cands.push_back({c.id, static_cast<int>(i), &w});
        }
    }
    // The theme: one object type (by distinct people) or one era (from distinct civilizations).
    const bool byObject = b.theming->sameObject || b.theming->uniquePerson;
    const auto key = [&](const GreatWork& w) { return byObject ? static_cast<int>(w.type) : static_cast<int>(w.era); };
    const auto who = [&](const GreatWork& w) { return byObject ? static_cast<int>(w.creator) : static_cast<int>(w.civ); };
    std::vector<int> keys;
    for (const Cand& c : cands) {
        if (std::find(keys.begin(), keys.end(), key(*c.work)) == keys.end()) keys.push_back(key(*c.work));
    }
    for (int k : keys) {
        if (k < 0) continue;
        // Distinct people (or civilizations) of this key, preferring works already in place.
        std::vector<const Cand*> pick;
        std::vector<int> seen;
        for (int pass = 0; pass < 2; ++pass) {
            for (const Cand& c : cands) {
                const bool here = c.city == cityId && c.work->building == building;
                if ((pass == 0) != here || key(*c.work) != k || who(*c.work) < 0) continue;
                if (std::find(seen.begin(), seen.end(), who(*c.work)) != seen.end()) continue;
                seen.push_back(who(*c.work));
                if (static_cast<int>(pick.size()) < slots) pick.push_back(&c);
            }
        }
        if (static_cast<int>(pick.size()) < slots) continue;
        // Works in the building that are not picked must leave first, to any free compatible slot elsewhere;
        // then the picked works come in. Moves are planned by work, and turned into indices by replaying them.
        struct Planned { CityId from; const GreatWork* work; CityId to; TypeIndex into; };
        std::vector<Planned> plan;
        std::vector<std::pair<CityId, TypeIndex>> used;
        bool ok = true;
        for (const GreatWork& w : city->greatWorks) {
            if (w.building != building || std::any_of(pick.begin(), pick.end(), [&](const Cand* p) { return p->work == &w; })) continue;
            if (state_.turn < w.lockedUntil) {  // locked art cannot leave
                ok = false;
                continue;
            }
            bool placed = false;
            for (const City& c : state_.cities) {
                if (c.owner != player || placed) continue;
                for (TypeIndex other : c.buildings) {
                    if (other == building && c.id == cityId) continue;
                    const int taken = static_cast<int>(std::count(used.begin(), used.end(), std::make_pair(c.id, other)));
                    if (freeSlotsFor(c, other, w.type) - taken <= 0) continue;
                    used.push_back({c.id, other});
                    plan.push_back({cityId, &w, c.id, other});
                    placed = true;
                    break;
                }
            }
            ok = ok && placed;
        }
        if (!ok) continue;
        for (const Cand* p : pick) {
            if (!(p->city == cityId && p->work->building == building)) plan.push_back({p->city, p->work, cityId, building});
        }
        std::vector<std::pair<CityId, std::vector<const GreatWork*>>> order;
        const auto listOf = [&](CityId id) -> std::vector<const GreatWork*>& {
            for (auto& [cid, list] : order) {
                if (cid == id) return list;
            }
            std::vector<const GreatWork*> list;
            for (const GreatWork& w : state_.city(id)->greatWorks) list.push_back(&w);
            order.emplace_back(id, std::move(list));
            return order.back().second;
        };
        for (const Planned& m : plan) {
            std::vector<const GreatWork*>& src = listOf(m.from);
            const auto it = std::find(src.begin(), src.end(), m.work);
            moves.push_back(Command::moveGreatWork(player, m.from, static_cast<int>(it - src.begin()), m.to, m.into));
            src.erase(it);
            listOf(m.to).push_back(m.work);
        }
        return moves;
    }
    return {};
}

void Game::placeAntiquity() {
    if (state_.antiquityPlaced) return;
    bool known = false;
    int majors = 0;
    for (const Player& p : state_.players) {
        if (!isMajorCiv(p.id)) continue;
        ++majors;
        known = known || seesAntiquity(p.id);
    }
    if (!known) return;
    state_.antiquityPlaced = true;
    const auto open = [&](Hex h, bool sea) {
        const Plot& p = state_.plot(h);
        const TerrainType& t = rules_->terrains[at(p.terrain)];
        if (state_.cityAt(h) || state_.districtAt(h) || state_.wonderAt(h) != kNone || p.village || p.antiquity) return false;
        if (p.feature != kNone && rules_->features[at(p.feature)].naturalWonder) return false;
        return sea ? t.water : isLandPassable(state_, *rules_, h);
    };
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    for (int sea = 0; sea < 2; ++sea) {
        int left = majors * rules_->globalInt(sea ? "ARCHAEOLOGY_SITES_PER_CIV_SEA" : "ARCHAEOLOGY_SITES_PER_CIV_LAND");
        for (int32_t i : state_.battleSites) {
            if (left <= 0) break;
            const Hex h = state_.grid.at(i);
            if (!open(h, sea != 0)) continue;
            state_.plot(h).antiquity = static_cast<uint8_t>(sea ? 2 : 1);
            --left;
        }
        for (int tries = 0; tries < 4000 && left > 0; ++tries) {
            const Hex h = state_.grid.at(static_cast<int>(rng.below(static_cast<uint32_t>(state_.grid.size()))));
            if (!open(h, sea != 0)) continue;
            if (sea && !rules_->terrains[at(state_.plot(h).terrain)].shallowWater) continue;  // shipwrecks near the coast
            state_.plot(h).antiquity = static_cast<uint8_t>(sea ? 2 : 1);
            --left;
        }
    }
}

CommandError Game::excavateProblem(PlayerId player, UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (rules_->units[at(u->type)].excavations <= 0 || u->charges <= 0 || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    const Plot& p = state_.plot(u->pos);
    if (p.antiquity == 0 || !seesAntiquity(player, p.antiquity)) return CommandError::BadTarget;
    if (p.owner != kNoPlayer && p.owner != player && !grantsOpenBorders(p.owner, player)) return CommandError::BadTarget;
    TypeIndex artifact = kNone;
    for (size_t w = 0; w < rules_->greatWorkTypes.size(); ++w) artifact = rules_->greatWorkTypes[w].id == "ARTIFACT" ? static_cast<TypeIndex>(w) : artifact;
    if (artifact == kNone) return CommandError::BadTarget;
    for (const City& c : state_.cities) {
        if (c.owner == player && freeGreatWorkSlot(c, artifact) != kNone) return CommandError::Ok;
    }
    return CommandError::CannotImprove;  // nowhere to put it
}

void Game::excavate(UnitId id) {
    Unit& u = *state_.unit(id);
    const PlayerId landOwner = state_.plot(u.pos).owner;
    if (landOwner != kNoPlayer && landOwner != u.owner) breakPromises(u.owner, landOwner, PromiseKind::NoDigging);  // 08 [GS]
    dedicationScore(u.owner, "DEDICATION_WISH_YOU_WERE_HERE", 1);  // 09: an artifact extracted
    eventBoost(u.owner, BoostKind::Artifact);                         // 04: Combustion
    if (rules_->terrains[at(state_.plot(u.pos).terrain)].water)
        awardFirst(u.owner, "MOMENT_WORLD_S_FIRST_SHIPWRECK_EXCAVATED", "MOMENT_FIRST_SHIPWRECK_EXCAVATED", 0);
    else
        awardMoment(u.owner, "MOMENT_ARTIFACT_EXTRACTED");
    state_.plot(u.pos).antiquity = 0;
    TypeIndex artifact = kNone;
    for (size_t w = 0; w < rules_->greatWorkTypes.size(); ++w) artifact = rules_->greatWorkTypes[w].id == "ARTIFACT" ? static_cast<TypeIndex>(w) : artifact;
    // Its history: the battle fought here, or (a site placed on open ground) a civilization in the game and an
    // era before the present one, drawn at random.
    int8_t era = -1;
    TypeIndex civ = kNone;
    const int32_t here = state_.grid.index(u.pos);
    for (size_t i = 0; i < state_.battleSites.size() && i < state_.battleHistory.size(); ++i) {
        if (state_.battleSites[i] != here) continue;
        era = static_cast<int8_t>(state_.battleHistory[i] / 4096);
        civ = static_cast<TypeIndex>(state_.battleHistory[i] % 4096 - 1);
    }
    if (civ == kNone) {
        Rng& rng = state_.rng.get(RngStream::Gameplay);
        std::vector<TypeIndex> civs;
        for (const Player& p : state_.players) {
            if (isMajorCiv(p.id) && p.civ != kNone) civs.push_back(p.civ);
        }
        if (!civs.empty()) civ = civs[rng.below(static_cast<uint32_t>(civs.size()))];
        era = static_cast<int8_t>(rng.below(static_cast<uint32_t>(std::max(1, std::min(state_.gameEra, rules_->globalInt("ARCHAEOLOGY_MAX_ERA") + 1)))));
    }
    for (City& c : state_.cities) {
        if (c.owner != u.owner) continue;
        const TypeIndex slot = freeGreatWorkSlot(c, artifact);
        if (slot == kNone) continue;
        GreatWork gw;
        gw.type = artifact;
        gw.building = slot;
        gw.era = era;
        gw.civ = civ;
        c.greatWorks.push_back(gw);
        break;
    }
    u.movesLeft = Fixed();
    if (--u.charges <= 0) removeUnit(id);
}

}  // namespace sov
