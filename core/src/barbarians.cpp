// Barbarians (01-map-and-terrain.md, Barbarians; 05-units-and-combat.md,
// Barbarian AI combat; data: barbarians-goody-huts.md, BARBARIAN_* globals).
// The barbarian player has no turn of its own and issues no commands: it acts
// inside the world turn (beginGlobalTurn), so replaying the command log
// replays it exactly. Its behaviour is deliberately simple until the real AI
// (MVP-6): camps appear out of sight, release units, and those units attack
// when the odds look good, raid nearby players once bold enough, and
// otherwise stay near their camp. A new camp first sends out a Scout (01: Barbarians): only once the
// Scout has seen a city and made it home does the camp raid or attack cities; killing the Scout first
// keeps it quiet until the next one goes out.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

PlayerId Game::barbarianPlayer() const {
    for (const Player& p : state_.players) {
        if (p.barbarian && !p.freeCity) return p.id;
    }
    return kNoPlayer;
}

const Camp* Game::campAt(Hex h) const {
    for (const Camp& c : state_.camps) {
        if (c.pos == h) return &c;
    }
    return nullptr;
}

void Game::linkBarbarians() {
    // The barbarians and the Free Cities are at war with everyone.
    for (const Player& b : state_.players) {
        if (!b.barbarian) continue;
        for (Player& p : state_.players) {
            if (p.id == b.id) continue;
            p.relations[static_cast<size_t>(b.id)].war = true;
            state_.players[static_cast<size_t>(b.id)].relations[static_cast<size_t>(p.id)].war = true;
        }
    }
}

void Game::noteKill(const Unit& victim, const Unit* killer) {
    // Civ uniques: a kill heals (Scara) or brings the loser in as a Builder (Jaguar Warrior).
    if (killer && killer->owner != victim.owner) {
        // Leader ability: Faith from kills, and the capital's mood from this era's kills (Flower Wars).
        Player& kp = state_.players[static_cast<size_t>(killer->owner)];
        ++kp.killsThisEra;
        if (const int pct = civAbility(killer->owner).killFaithPercent; pct > 0)
            kp.faith += Fixed::fromInt(rules_->units[static_cast<size_t>(victim.type)].combat * pct / 100);
        if (Unit* k = state_.unit(killer->id)) {
            k->hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), k->hp + unitEffectTotal(*k, UnitEffectKind::HealOnKill));
            if (unitHas(*k, UnitEffectKind::CaptureAsBuilder) && rules_->units[static_cast<size_t>(victim.type)].domain == Domain::Land &&
                rules_->units[static_cast<size_t>(victim.type)].layer == UnitLayer::Military)
                captures_.push_back({k->owner, victim.pos});
        }
    }
    // Dedications (09): naval kills (Hic Sunt Dracones), Corps and Armies killed (To Arms!), kills by a Giant Death Robot.
    if (killer && killer->owner != victim.owner && !state_.players[static_cast<size_t>(victim.owner)].barbarian) {
        if (rules_->units[static_cast<size_t>(victim.type)].domain == Domain::Sea) dedicationScore(killer->owner, "DEDICATION_HIC_SUNT_DRACONES", 1);
        if (victim.formation > 0) dedicationScore(killer->owner, "DEDICATION_TO_ARMS", victim.formation);
        if (rules_->units[static_cast<size_t>(killer->type)].id == "UNIT_GIANT_DEATH_ROBOT") dedicationScore(killer->owner, "DEDICATION_AUTOMATON_WARFARE", 1);
    }
    // Camp boldness: +15 per kill, -10 per unit lost, -5 per scout lost (BARBARIAN_BOLDNESS_PER_*).
    for (Camp& c : state_.camps) {
        if (killer && killer->camp == c.id) c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_KILL");
        if (victim.camp != c.id) continue;
        if (isBarbarianScout(victim)) {
            c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_SCOUT_LOST");
            c.scoutSaw = false;  // its news dies with it
        } else {
            c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_UNIT_LOST");
        }
    }
}

void Game::spawnCaptures() {
    const TypeIndex builder = rules_->unit("UNIT_BUILDER");
    for (const auto& [owner, at] : captures_) {
        if (builder == kNone || !state_.players[static_cast<size_t>(owner)].alive) continue;
        if (state_.unitAt(at, UnitLayer::Civilian, *rules_) || !isLandPassable(state_, *rules_, at)) continue;
        spawnUnit(builder, owner, at).movesLeft = Fixed();
    }
    captures_.clear();
}

void Game::enterPlot(Unit& unit) {
    const Player& owner = state_.players[static_cast<size_t>(unit.owner)];
    // Natural wonders' permanent abilities (01): land units beside Everest, land units entering the
    // Fountain of Youth, ships entering the Bermuda Triangle.
    {
        const UnitType& ut = rules_->units[static_cast<size_t>(unit.type)];
        const Plot& here = state_.plot(unit.pos);
        const std::string& id = here.feature != kNone ? rules_->features[static_cast<size_t>(here.feature)].id : std::string();
        if (ut.domain == Domain::Land && nextToNaturalWonder(unit.pos, "FEATURE_MOUNT_EVEREST")) unit.wonderAbilities |= 1;
        if (ut.domain == Domain::Land && id == "FEATURE_FOUNTAIN_OF_YOUTH") unit.wonderAbilities |= 2;
        if (ut.domain == Domain::Sea && id == "FEATURE_BERMUDA_TRIANGLE") unit.wonderAbilities |= 4;
    }
    if (state_.plot(unit.pos).village && isMajorCiv(unit.owner)) {
        const UnitId id = unit.id;
        enterVillage(unit);
        if (!state_.unit(id)) return;
    }
    if (owner.barbarian || rules_->units[static_cast<size_t>(unit.type)].layer != UnitLayer::Military) return;
    auto it = std::find_if(state_.camps.begin(), state_.camps.end(), [&](const Camp& c) { return c.pos == unit.pos; });
    if (it == state_.camps.end()) return;
    // Clearing a camp pays gold; its surviving units roam on without a home.
    const int32_t cleared = it->id;
    state_.camps.erase(it);
    questDone(unit.owner, QuestKind::ClearCamp, cleared);  // 08: Quests
    const int pct = 100 + (difficultyHuman(unit.owner) ? difficulty().humanCampGoldPercent : 0);  // 00-overview: Difficulty levels
    state_.players[static_cast<size_t>(unit.owner)].gold += Fixed::fromInt(rules_->globalInt("BARBARIAN_CAMP_CLEAR_GOLD") * pct / 100);
    awardMoment(unit.owner, "MOMENT_BARBARIAN_CAMP_DESTROYED");
}

void Game::processBarbarians() {
    const PlayerId bp = barbarianPlayer();
    if (bp == kNoPlayer) return;
    healAndFortify(bp);
    for (Unit& u : state_.units) {
        if (u.owner == bp) u.movesLeft = Fixed::fromInt(maxMoves(u));
    }
    refreshVisibility(bp);
    placeCamps(bp);
    for (Camp& c : state_.camps) {
        c.boldness += rules_->globalInt("BARBARIAN_BOLDNESS_PER_TURN");
        if (--c.spawnTimer <= 0) {
            // An unalerted camp replaces a lost Scout before anything else.
            bool scouting = c.alerted;
            for (const Unit& u : state_.units) scouting = scouting || (u.camp == c.id && isBarbarianScout(u));
            if (scouting || !releaseScout(c, bp)) releaseUnit(c, bp);
            c.spawnTimer = rules_->barbarianTribes[static_cast<size_t>(c.tribe)].spawnTurns;
        }
    }
    std::vector<UnitId> ids;
    for (const Unit& u : state_.units) {
        if (u.owner == bp) ids.push_back(u.id);
    }
    for (UnitId id : ids) barbarianAct(id);
}

void Game::placeCamps(PlayerId bp) {
    int majors = 0;
    for (const Player& p : state_.players) majors += p.alive && !p.barbarian ? 1 : 0;
    const int target = rules_->globalInt("BARBARIAN_CAMP_MAX_PER_MAJOR_CIV") * majors;
    if (static_cast<int>(state_.camps.size()) >= target) return;
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    // A third of the target on the first world turn, then a small chance each turn.
    int toAdd = 0;
    if (state_.turn == 2) {
        toAdd = target * rules_->globalInt("BARBARIAN_CAMP_FIRST_TURN_PERCENT_OF_TARGET_TO_ADD") / 100;
    } else if (rng.chance(static_cast<uint32_t>(rules_->globalInt("BARBARIAN_CAMP_ODDS_OF_NEW_CAMP_SPAWNING")))) {
        toAdd = 1;
    }
    if (toAdd <= 0) return;

    // Camps keep one more tile away per difficulty level below Prince (BARBARIAN_CAMP_EXTRA_DISTANCE_PER_LOW_DIFFICULTY).
    const int below = std::max(0, 3 - state_.setup.difficulty);
    const int cityGap = rules_->globalInt("BARBARIAN_CAMP_MINIMUM_DISTANCE_CITY") + below * rules_->globalInt("BARBARIAN_CAMP_EXTRA_DISTANCE_PER_LOW_DIFFICULTY");
    const int campGap = rules_->globalInt("BARBARIAN_CAMP_MINIMUM_DISTANCE_ANOTHER_CAMP");
    std::vector<Hex> spots;
    for (int i = 0; i < state_.grid.size(); ++i) {
        const Hex h = state_.grid.at(i);
        if (!isLandPassable(state_, *rules_, h) || state_.plot(h).owner != kNoPlayer) continue;
        bool ok = true;
        for (const Unit& u : state_.units) ok = ok && u.pos != h;
        for (const Player& p : state_.players) {
            if (!p.barbarian && p.alive && p.visibility[static_cast<size_t>(i)] == static_cast<uint8_t>(Visibility::Visible))
                ok = false;
        }
        for (const City& c : state_.cities) ok = ok && state_.grid.distance(c.pos, h) >= cityGap;
        for (const Camp& c : state_.camps) ok = ok && state_.grid.distance(c.pos, h) >= campGap;
        if (ok) spots.push_back(h);
    }
    for (int n = 0; n < toAdd && !spots.empty(); ++n) {
        const Hex at = spots[rng.below(static_cast<uint32_t>(spots.size()))];
        spots.erase(std::remove_if(spots.begin(), spots.end(),
                                   [&](const Hex& h) { return state_.grid.distance(h, at) < campGap; }),
                    spots.end());
        // Tribe: a naval tribe at a coastal camp (01: Barbarians), else the first land tribe whose
        // resource (if any) lies near the camp.
        bool coast = false;
        for (const Hex& h : state_.grid.within(at, 1)) coast = coast || rules_->terrains[static_cast<size_t>(state_.plot(h).terrain)].shallowWater;
        TypeIndex tribe = kNone;
        for (size_t t = 0; t < rules_->barbarianTribes.size() && tribe == kNone && coast; ++t) {
            if (rules_->barbarianTribes[t].coastal) tribe = static_cast<TypeIndex>(t);
        }
        for (size_t t = 0; t < rules_->barbarianTribes.size() && tribe == kNone; ++t) {
            const BarbarianTribe& bt = rules_->barbarianTribes[t];
            if (bt.coastal) continue;
            bool fits = bt.resource == kNone;
            for (const Hex& h : state_.grid.within(at, bt.resourceRange)) {
                if (fits) break;
                fits = state_.plot(h).resource == bt.resource;
            }
            if (fits) tribe = static_cast<TypeIndex>(t);
        }
        if (tribe == kNone) return;
        Camp camp;
        camp.id = state_.nextCampId++;
        camp.pos = at;
        camp.tribe = tribe;
        camp.spawnTimer = rules_->barbarianTribes[static_cast<size_t>(tribe)].spawnTurns;
        camp.alerted = false;
        state_.camps.push_back(camp);  // ids only grow, so the vector stays sorted
        if (!releaseScout(state_.camps.back(), bp)) {
            state_.camps.back().alerted = true;  // nowhere to scout from: it knows its neighbours already
            releaseUnit(state_.camps.back(), bp);
        }
    }
}

void Game::releaseUnit(Camp& camp, PlayerId bp) {
    int count = 0;
    for (const Unit& u : state_.units) count += u.camp == camp.id ? 1 : 0;
    if (count >= rules_->globalInt("BARBARIAN_MAX_UNITS_PER_CAMP")) return;
    const BarbarianTribe& tribe = rules_->barbarianTribes[static_cast<size_t>(camp.tribe)];
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    const bool ranged = rng.chance(static_cast<uint32_t>(tribe.rangedPercent));
    Domain domain = tribe.coastal ? Domain::Sea : Domain::Land;  // naval tribes put to sea
    // The strongest generic unit of the class that at least half the majors can build (BARBARIAN_TECH_PERCENT).
    auto best = [&](const std::string& cls) {
        TypeIndex pick = kNone;
        int majors = 0;
        for (const Player& p : state_.players) majors += p.alive && !p.barbarian ? 1 : 0;
        for (size_t i = 0; i < rules_->units.size(); ++i) {
            const UnitType& ut = rules_->units[i];
            if (ut.unitClass != cls || ut.domain != domain || ut.layer != UnitLayer::Military) continue;
            int knowing = 0;
            for (const Player& p : state_.players) {
                if (p.alive && !p.barbarian && hasUnlocked(p.id, ut.unlock)) ++knowing;
            }
            if (knowing * 100 < majors * rules_->globalInt("BARBARIAN_TECH_PERCENT")) continue;
            const int strength = ranged ? ut.ranged : ut.combat;
            const UnitType* cur = pick == kNone ? nullptr : &rules_->units[static_cast<size_t>(pick)];
            if (!cur || strength > (ranged ? cur->ranged : cur->combat)) pick = static_cast<TypeIndex>(i);
        }
        return pick;
    };
    TypeIndex type = best(ranged ? (tribe.coastal ? "NAVAL_RANGED" : "RANGED") : tribe.unitClass);
    if (type == kNone && !ranged) type = best(tribe.coastal ? "NAVAL_MELEE" : "MELEE");
    if (type == kNone && ranged) type = best(tribe.unitClass);
    // Before anyone sails, a naval camp sends its people out on foot.
    if (type == kNone && tribe.coastal) {
        domain = Domain::Land;
        type = best("MELEE");
    }
    if (type == kNone) return;
    const bool atSea = domain == Domain::Sea;
    std::optional<Hex> spot;
    for (const Hex& h : state_.grid.within(camp.pos, 1)) {
        if (spot) break;
        const bool fits = atSea ? rules_->terrains[static_cast<size_t>(state_.plot(h).terrain)].shallowWater : isLandPassable(state_, *rules_, h);
        if (fits && !state_.unitAt(h, UnitLayer::Military, *rules_) && !state_.foreignUnitAt(h, bp) && !state_.cityAt(h)) spot = h;
    }
    if (!spot) return;
    Unit& u = spawnUnit(type, bp, *spot);
    u.camp = camp.id;
}

bool Game::isBarbarianScout(const Unit& u) const {
    return u.camp != 0 && state_.players[static_cast<size_t>(u.owner)].barbarian && rules_->units[static_cast<size_t>(u.type)].id == "UNIT_SCOUT";
}

bool Game::releaseScout(Camp& camp, PlayerId bp) {
    const TypeIndex scout = rules_->unit("UNIT_SCOUT");
    if (scout == kNone) return false;
    for (const Hex& h : state_.grid.within(camp.pos, 1)) {
        if (!isLandPassable(state_, *rules_, h) || state_.unitAt(h, UnitLayer::Military, *rules_) || state_.foreignUnitAt(h, bp) || state_.cityAt(h)) continue;
        Unit& u = spawnUnit(scout, bp, h);
        u.camp = camp.id;
        return true;
    }
    return false;
}

// The Scout wanders within 10 plots of its camp until a city comes within its sight, then goes home with
// the news; reaching its camp (or next to it) alerts the camp.
void Game::barbarianScoutAct(UnitId id) {
    Unit* u = state_.unit(id);
    Camp* camp = nullptr;
    for (Camp& c : state_.camps) camp = c.id == u->camp ? &c : camp;
    if (!camp) {
        u->activity = Activity::Fortify;
        return;
    }
    if (!camp->scoutSaw) {
        for (const City& c : state_.cities) {
            if (!state_.players[static_cast<size_t>(c.owner)].barbarian && state_.grid.distance(u->pos, c.pos) <= unitSight(*u) && lineOfSight(u->pos, c.pos))
                camp->scoutSaw = true;
        }
    }
    if (camp->scoutSaw && state_.grid.distance(u->pos, camp->pos) <= 1) {
        camp->alerted = true;
        camp->scoutSaw = false;
    }
    if (camp->alerted) {
        u->moveTarget.reset();
        return;  // its work is done; it keeps watch at home
    }
    std::optional<Hex> goal = camp->scoutSaw ? std::optional<Hex>(camp->pos) : u->moveTarget;
    if (!goal || *goal == u->pos || !findPath(id, *goal)) {
        goal.reset();
        Rng& rng = state_.rng.get(RngStream::Gameplay);
        const std::vector<Hex> around = state_.grid.within(camp->pos, 10);
        for (int tries = 0; tries < 12 && !goal; ++tries) {
            const Hex h = around[rng.below(static_cast<uint32_t>(around.size()))];
            if (h != u->pos && isLandPassable(state_, *rules_, h) && !state_.cityAt(h) && findPath(id, h)) goal = h;
        }
    }
    if (!goal) return;
    u->moveTarget = *goal;
    u->activity = Activity::Awake;
    advanceUnit(id);
    if (Unit* after = state_.unit(id); after && camp->scoutSaw && state_.grid.distance(after->pos, camp->pos) <= 1) {
        camp->alerted = true;
        camp->scoutSaw = false;
        after->moveTarget.reset();
    }
}

void Game::barbarianAct(UnitId id) {
    const Unit* u = state_.unit(id);
    if (!u) return;
    if (isBarbarianScout(*u)) {
        barbarianScoutAct(id);
        return;
    }
    // Barbarians pillage what they stand on (01: Barbarians; 05: Pillage), then act with what moves remain.
    if (pillageProblem(u->owner, id) == CommandError::Ok) {
        pillage(id);
        u = state_.unit(id);
        if (!u || u->movesLeft <= Fixed()) return;
    }
    // Barbarian ships raid the coast beside them (05: Coastal raid).
    for (const Hex& h : state_.grid.within(u->pos, 1)) {
        if (coastalRaidProblem(u->owner, id, h) != CommandError::Ok) continue;
        pillage(id, h);
        u = state_.unit(id);
        if (!u || u->movesLeft <= Fixed()) return;
        break;
    }
    const Camp* camp = nullptr;
    for (const Camp& c : state_.camps) {
        if (c.id == u->camp) camp = &c;
    }
    // Units whose camp is gone roam at full boldness.
    const int bold = camp ? camp->boldness : 100;
    const bool alerted = !camp || camp->alerted;  // a camp whose scout has not come home stays put
    const BarbarianTribe* tribe = camp ? &rules_->barbarianTribes[static_cast<size_t>(camp->tribe)] : nullptr;
    const int raidAt = tribe ? tribe->raidBoldness : 0;
    const int cityAt = tribe ? tribe->attackBoldness : 0;
    const PlayerId me = u->owner;

    auto tryAttack = [&]() {
        const Unit* self = state_.unit(id);
        if (!self || self->movesLeft <= Fixed()) return false;
        const bool ranged = unitRange(*self) > 0;
        for (const Hex& h : state_.grid.within(self->pos, std::max(1, unitRange(*self)))) {
            CombatPreview pv = previewAttack(id, h, ranged);
            if (!pv.valid) continue;
            if (pv.city != kNoCity ? bold < cityAt || !alerted : !pv.capture && pv.damageToAttackerMax > pv.damageToDefenderMax) continue;
            applyCombat(ranged ? Command::rangedAttack(me, id, h) : Command::attack(me, id, h));
            return true;
        }
        return false;
    };
    if (tryAttack()) return;

    // Pick where to go: the nearest target within 8 plots once bold enough, else home.
    u = state_.unit(id);
    std::optional<Hex> goal;
    int bestDist = 9;
    if (bold >= raidAt && alerted) {
        for (const Unit& e : state_.units) {
            const int d = state_.grid.distance(u->pos, e.pos);
            if (atWar(me, e.owner) && d < bestDist) {
                bestDist = d;
                goal = e.pos;
            }
        }
        for (const City& c : state_.cities) {
            const int d = state_.grid.distance(u->pos, c.pos);
            if (bold >= cityAt && d < bestDist) {
                bestDist = d;
                goal = c.pos;
            }
        }
    }
    if (!goal && camp && state_.grid.distance(u->pos, camp->pos) > 2) goal = camp->pos;
    if (!goal) {
        state_.unit(id)->activity = Activity::Fortify;
        return;
    }
    // Head for the free plot next to the goal (or the goal itself) closest to the unit.
    std::vector<Hex> around = state_.grid.within(*goal, 1);
    std::stable_sort(around.begin(), around.end(), [&](const Hex& a, const Hex& b) {
        return state_.grid.distance(u->pos, a) < state_.grid.distance(u->pos, b);
    });
    for (const Hex& n : around) {
        if (n == u->pos) break;  // already as close as it gets
        if (state_.unitAt(n, UnitLayer::Military, *rules_) || state_.foreignUnitAt(n, me) || state_.cityAt(n)) continue;
        if (!findPath(id, n)) continue;
        Unit* mover = state_.unit(id);
        mover->moveTarget = n;
        mover->activity = Activity::Awake;
        advanceUnit(id);
        if (Unit* after = state_.unit(id)) after->moveTarget.reset();  // re-decided every turn
        break;
    }
    tryAttack();
}

}  // namespace sov
