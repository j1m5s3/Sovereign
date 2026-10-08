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

namespace {
// Barbarian Clans mode (01: Barbarians; Sovereign values, the spec gives only the outline): a camp becomes a
// city-state at 100 points, earning 2 a turn and 10 for each bribe or hire, losing 10 for each unit killed.
// Bribes and incitements last 10 turns (scaled by game speed); a camp hires out a unit at most every 5 turns.
constexpr int kClanPoints = 100, kClanPerTurn = 2, kClanDeal = 10, kClanUnitLost = 10, kClanTurns = 10, kHireTurns = 5;
}  // namespace

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

void Game::killReward(Player& to, const UnitEffect& e, const UnitType& victim) {
    if (e.kind != UnitEffectKind::KillYield) return;
    for (const auto& group : e.when) {
        for (const CombatCondition& c : group) {
            if (c.atom == CombatAtom::VsDomain && static_cast<int>(victim.domain) != c.arg) return;
        }
    }
    const Fixed amount = Fixed::fromInt(victim.combat * e.amount / 100);
    if (e.at == "GOLD") to.gold += amount;
    else if (e.at == "FAITH") to.faith += amount;
    else if (e.at == "CULTURE" && to.civics.current != kNone) to.civics.progress[static_cast<size_t>(to.civics.current)] += amount;
}

void Game::noteKill(const Unit& victim, const Unit* killer) {
    // Barbarian Clans mode: a clan that loses a unit falls back from becoming a city-state.
    if (state_.setup.barbarianClans && victim.camp != 0) {
        for (Camp& c : state_.camps) {
            if (c.id == victim.camp) c.progress = std::max(0, c.progress - kClanUnitLost);
        }
    }
    // Civ uniques: a kill heals (Scara) or brings the loser in as a Builder (Jaguar Warrior).
    if (killer && killer->owner != victim.owner) {
        // Leader ability: Faith from kills, and the capital's mood from this era's kills (Flower Wars).
        Player& kp = state_.players[static_cast<size_t>(killer->owner)];
        ++kp.killsThisEra;
        // Historic moments (09): a veteran (two promotions or more) or a formation beaten; a victory beside a Great General or Admiral.
        if (isMajorCiv(killer->owner) && isMajorCiv(victim.owner)) {
            if (victim.promotions.size() >= 2) awardMoment(killer->owner, "MOMENT_ENEMY_VETERAN_DEFEATED");
            if (victim.formation > 0) awardMoment(killer->owner, "MOMENT_ENEMY_FORMATION_DEFEATED");
            for (const Unit& gp : state_.units) {
                if (gp.owner != killer->owner || state_.grid.distance(gp.pos, killer->pos) > 2) continue;
                const std::string& gid = rules_->units[static_cast<size_t>(gp.type)].id;
                if (gid == "UNIT_GREAT_GENERAL" || gid == "UNIT_GREAT_ADMIRAL") {
                    awardMoment(killer->owner, gid == "UNIT_GREAT_GENERAL" ? "MOMENT_GENERAL_DEFEATS_ENEMY" : "MOMENT_ADMIRAL_DEFEATS_ENEMY");
                    break;
                }
            }
        }
        if (const int pct = civAbility(killer->owner).killFaithPercent; pct > 0)
            kp.faith += Fixed::fromInt(rules_->units[static_cast<size_t>(victim.type)].combat * pct / 100);
        // Disciples (06): a Warrior Monk's victory presses its religion (+100) on the cities within 4 plots.
        if (const Unit* k = state_.unit(killer->id); k && kp.religion >= 0 && static_cast<size_t>(kp.religion) < state_.religions.size()) {
            const TypeIndex disciples = rules_->promotion("PROMOTION_DISCIPLES");
            if (disciples != kNone && std::find(k->promotions.begin(), k->promotions.end(), disciples) != k->promotions.end()) {
                for (City& c : state_.cities) {
                    if (state_.grid.distance(c.pos, victim.pos) > 4) continue;
                    if (c.pressure.size() < state_.religions.size()) c.pressure.resize(state_.religions.size(), 0);
                    c.pressure[static_cast<size_t>(kp.religion)] += 100;
                }
            }
        }
        // God of War (06): Faith of half the victim's strength for a victory next to one of the victor's Holy Sites.
        if (playerHasBelief(killer->owner, Bf::GodOfWar)) {
            const TypeIndex holy = rules_->district("DISTRICT_HOLY_SITE");
            bool near = false;
            for (const City& c : state_.cities) {
                if (c.owner != killer->owner) continue;
                for (const CityDistrict& d : c.districts) near = near || (d.complete && d.type == holy && state_.grid.distance(d.pos, victim.pos) <= 1);
            }
            if (near) kp.faith += Fixed::fromInt(rules_->units[static_cast<size_t>(victim.type)].combat / 2);
        }
        // Kill rewards of its promotions and abilities (Boarding: Gold from ships it sinks).
        if (const Unit* k = state_.unit(killer->id)) {
            const UnitType& vt = rules_->units[static_cast<size_t>(victim.type)];
            for (TypeIndex a : unitAbilities(*k)) {
                for (const UnitEffect& e : rules_->abilities[static_cast<size_t>(a)].effects) killReward(kp, e, vt);
            }
            for (TypeIndex pr : k->promotions) {
                for (const UnitEffect& e : rules_->promotions[static_cast<size_t>(pr)].effects) killReward(kp, e, vt);
            }
        }
        // Wolin (08: suzerain): a land victory over a civ's or a city-state's unit earns Great General points, a naval one
        // Great Admiral points, of a quarter of the beaten unit's strength.
        if (!state_.players[static_cast<size_t>(victim.owner)].barbarian && suzerainBonus(killer->owner, Cs::Wolin)) {
            if (const Unit* k = state_.unit(killer->id)) {
                const std::vector<TypeIndex> abilities = unitAbilities(*k);
                const auto has = [&](const char* id) { return std::find(abilities.begin(), abilities.end(), rules_->ability(id)) != abilities.end(); };
                const TypeIndex cls = has("ABILITY_GREAT_GENERAL_POINTS")  ? rules_->greatPersonClass("GREAT_PERSON_CLASS_GENERAL")
                                      : has("ABILITY_WOLIN_NAVAL_UNITS") ? rules_->greatPersonClass("GREAT_PERSON_CLASS_ADMIRAL")
                                                                          : kNone;
                if (cls != kNone && static_cast<size_t>(cls) < kp.greatPersonPoints.size())
                    kp.greatPersonPoints[static_cast<size_t>(cls)] += rules_->units[static_cast<size_t>(victim.type)].combat / 4;
            }
        }
        // War Department (03): the victor heals 20.
        if (Unit* k = state_.unit(killer->id); k && buildingsOwned(killer->owner, "BUILDING_WAR_DEPARTMENT") > 0)
            k->hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), k->hp + 20);
        // Native Conquest (04): gold of half the victim's strength.
        if (policyIs(killer->owner, "POLICY_NATIVE_CONQUEST")) kp.gold += Fixed::fromInt(rules_->units[static_cast<size_t>(victim.type)].combat / 2);
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
    // Boosts (04): a kill with a unit type (Archery's Slinger...) or of one (Guidance Systems' Fighter), and the
    // barbarians killed (Bronze Working).
    if (killer && killer->owner != victim.owner) {
        const Player& loser = state_.players[static_cast<size_t>(victim.owner)];
        if (loser.barbarian && !loser.freeCity) ++state_.players[static_cast<size_t>(killer->owner)].barbarianKills;
        eventBoost(killer->owner, BoostKind::KillWith, killer->type);
        eventBoost(killer->owner, BoostKind::KillUnit, victim.type);
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
        // Pilgrim (06): more charges the first time it stands next to a natural wonder (bit 0x40 marks it spent).
        if (const int more = ut.spreadCharges > 0 && !unit.promotions.empty() ? unitEffectTotal(unit, UnitEffectKind::WonderCharges) : 0;
            more > 0 && !(unit.wonderAbilities & 0x40)) {
            bool wonder = false;
            for (const Hex& h : state_.grid.within(unit.pos, 1)) {
                const TypeIndex f = state_.plot(h).feature;
                wonder = wonder || (f != kNone && rules_->features[static_cast<size_t>(f)].naturalWonder);
            }
            if (wonder) {
                unit.charges += more;
                unit.wonderAbilities |= 0x40;
            }
        }
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
    eventBoost(unit.owner, BoostKind::ClearCamp);           // 04: Military Tradition
    const int pct = 100 + (difficultyHuman(unit.owner) ? difficulty().humanCampGoldPercent : 0);  // 00-overview: Difficulty levels
    state_.players[static_cast<size_t>(unit.owner)].gold += Fixed::fromInt(rules_->globalInt("BARBARIAN_CAMP_CLEAR_GOLD") * pct / 100);
    // Initiation Rites (06): +50 Faith, and the unit that cleared it heals fully.
    if (playerHasBelief(unit.owner, Bf::InitiationRites)) {
        state_.players[static_cast<size_t>(unit.owner)].faith += Fixed::fromInt(50);
        unit.hp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    }
    awardMoment(unit.owner, "MOMENT_BARBARIAN_CAMP_DESTROYED");
    // A camp within 6 plots of one of its cities was a threat (09; Sovereign reading of "threatening").
    for (const City& home : state_.cities) {
        if (home.owner == unit.owner && state_.grid.distance(home.pos, unit.pos) <= 6) {
            awardMoment(unit.owner, "MOMENT_THREATENING_CAMP_DESTROYED");
            break;
        }
    }
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
    // Barbarian Clans mode: camps grow toward city-states; dealings run out.
    if (state_.setup.barbarianClans) {
        std::vector<int32_t> ready;
        for (Camp& c : state_.camps) {
            c.progress += kClanPerTurn;
            c.bribes.erase(std::remove_if(c.bribes.begin(), c.bribes.end(), [&](const auto& bribe) { return bribe.second < state_.turn; }), c.bribes.end());
            if (c.incitedUntil < state_.turn) c.incitedAgainst = kNoPlayer;
            if (c.progress >= kClanPoints) ready.push_back(c.id);
        }
        for (int32_t id : ready) convertCamp(id);
    }
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
    Domain domain = Domain::Land;
    const TypeIndex type = campUnitType(camp, ranged, domain);
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

TypeIndex Game::campUnitType(const Camp& camp, bool ranged, Domain& domain) const {
    const BarbarianTribe& tribe = rules_->barbarianTribes[static_cast<size_t>(camp.tribe)];
    domain = tribe.coastal ? Domain::Sea : Domain::Land;  // naval tribes put to sea
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
    return type;
}

namespace {
const Camp* findCamp(const GameState& s, int32_t id) {
    for (const Camp& c : s.camps) {
        if (c.id == id) return &c;
    }
    return nullptr;
}
}  // namespace

int Game::clanCost(PlayerId player, int32_t id, CommandType action) const {
    const Camp* c = findCamp(state_, id);
    if (!c || player < 0 || static_cast<size_t>(player) >= state_.players.size()) return -1;
    const int era = std::max(0, playerEra(player));
    const int speed = rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent;
    if (action == CommandType::BribeCamp) return 50 * (era + 1) * speed / 100;
    if (action == CommandType::InciteCamp) return 100 * (era + 1) * speed / 100;
    if (action == CommandType::HireFromCamp) {
        Domain domain = Domain::Land;
        const TypeIndex t = campUnitType(*c, false, domain);
        if (t == kNone) return -1;
        const int cost = purchaseCost(player, {ProductionKind::Unit, t});
        return cost > 0 ? cost : rules_->units[static_cast<size_t>(t)].cost * 2 * speed / 100;
    }
    return -1;
}

CommandError Game::clanProblem(PlayerId player, int32_t id, CommandType action, PlayerId against) const {
    const Camp* c = findCamp(state_, id);
    if (!state_.setup.barbarianClans || !c || !isMajorCiv(player) || visibility(player, c->pos) == Visibility::Unrevealed)
        return CommandError::CannotTreatWithClan;
    if (action == CommandType::BribeCamp) {
        for (const auto& [who, until] : c->bribes) {
            if (who == player && until >= state_.turn) return CommandError::CannotTreatWithClan;
        }
    } else if (action == CommandType::HireFromCamp) {
        if (state_.turn < c->hiredUntil) return CommandError::CannotTreatWithClan;
        Domain domain = Domain::Land;
        const TypeIndex t = campUnitType(*c, false, domain);
        if (t == kNone) return CommandError::CannotTreatWithClan;
        bool room = false;
        for (const Hex& h : state_.grid.within(c->pos, 1)) {
            const bool fits = domain == Domain::Sea ? rules_->terrains[static_cast<size_t>(state_.plot(h).terrain)].shallowWater : isLandPassable(state_, *rules_, h);
            room = room || (h != c->pos && fits && !state_.unitAt(h, UnitLayer::Military, *rules_) && !state_.cityAt(h));
        }
        if (!room) return CommandError::CannotTreatWithClan;
    } else if (action == CommandType::InciteCamp) {
        if (against == player || !isMajorCiv(against) || !hasMet(player, against)) return CommandError::CannotTreatWithClan;
        if (c->incitedAgainst != kNoPlayer && c->incitedUntil >= state_.turn) return CommandError::CannotTreatWithClan;
    } else {
        return CommandError::CannotTreatWithClan;
    }
    const int cost = clanCost(player, id, action);
    if (cost < 0) return CommandError::CannotTreatWithClan;
    return state_.players[static_cast<size_t>(player)].gold >= Fixed::fromInt(cost) ? CommandError::Ok : CommandError::NotEnoughGold;
}

bool Game::campLeavesAlone(const Camp& camp, PlayerId player) const {
    if (player == kNoPlayer) return false;
    for (const auto& [who, until] : camp.bribes) {
        if (who == player && until >= state_.turn) return true;
    }
    return camp.incitedAgainst != kNoPlayer && camp.incitedUntil >= state_.turn && camp.incitedAgainst != player;
}

void Game::applyClan(const Command& c) {
    Camp* camp = nullptr;
    for (Camp& k : state_.camps) camp = k.id == c.id ? &k : camp;
    if (!camp) return;
    const int cost = clanCost(c.player, c.id, c.type);
    state_.players[static_cast<size_t>(c.player)].gold -= Fixed::fromInt(cost);
    const int turns = kClanTurns * rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent / 100;
    if (c.type == CommandType::BribeCamp) {
        camp->bribes.erase(std::remove_if(camp->bribes.begin(), camp->bribes.end(), [&](const auto& b) { return b.first == c.player; }), camp->bribes.end());
        camp->bribes.push_back({c.player, state_.turn + turns});
        camp->progress += kClanDeal;
    } else if (c.type == CommandType::HireFromCamp) {
        Domain domain = Domain::Land;
        const TypeIndex t = campUnitType(*camp, false, domain);
        for (const Hex& h : state_.grid.within(camp->pos, 1)) {
            const bool fits = domain == Domain::Sea ? rules_->terrains[static_cast<size_t>(state_.plot(h).terrain)].shallowWater : isLandPassable(state_, *rules_, h);
            if (h == camp->pos || !fits || state_.unitAt(h, UnitLayer::Military, *rules_) || state_.cityAt(h)) continue;
            spawnUnit(t, c.player, h);
            break;
        }
        camp->hiredUntil = state_.turn + kHireTurns;
        camp->progress += kClanDeal;
        refreshVisibility(c.player);
    } else if (c.type == CommandType::InciteCamp) {
        camp->incitedAgainst = static_cast<PlayerId>(c.arg);
        camp->incitedUntil = state_.turn + turns;
        camp->alerted = true;
        camp->boldness = std::max(camp->boldness, 100);
    }
}

void Game::convertCamp(int32_t id) {
    auto it = std::find_if(state_.camps.begin(), state_.camps.end(), [&](const Camp& c) { return c.id == id; });
    if (it == state_.camps.end()) return;
    const Hex at = it->pos;
    // Not beside a city, and only while a city-state of the setup's list is still unused.
    for (const City& c : state_.cities) {
        if (state_.grid.distance(c.pos, at) < 4) return;
    }
    TypeIndex kind = kNone;
    for (size_t k = 0; k < rules_->cityStates.size() && kind == kNone; ++k) {
        bool used = false;
        for (const Player& p : state_.players) used = used || p.cityState == static_cast<TypeIndex>(k);
        if (!used) kind = static_cast<TypeIndex>(k);
    }
    if (kind == kNone) return;
    state_.camps.erase(it);
    Player p;
    p.id = static_cast<PlayerId>(state_.players.size());
    p.cityState = kind;
    p.startPos = at;
    p.leaderName = rules_->cityStates[static_cast<size_t>(kind)].name;
    fitPlayerToRules(p, *rules_);
    p.visibility.assign(static_cast<size_t>(state_.grid.size()), 0);
    // It knows what at least half the major civs know.
    int majors = 0;
    for (const Player& o : state_.players) majors += isMajorCiv(o.id) ? 1 : 0;
    for (size_t t = 0; t < rules_->techs.size(); ++t) {
        int knowing = 0;
        for (const Player& o : state_.players) knowing += isMajorCiv(o.id) && o.techs.has(static_cast<TypeIndex>(t)) ? 1 : 0;
        if (majors > 0 && knowing * 2 >= majors) p.techs.done[t] = 1;
    }
    const PlayerId pid = p.id;
    state_.players.push_back(std::move(p));
    for (Player& o : state_.players) o.relations.resize(state_.players.size());
    linkBarbarians();
    // The clan's warriors serve the new city-state; one of them founds the city.
    for (Unit& u : state_.units) {
        if (u.camp != id) continue;
        u.owner = pid;
        u.camp = 0;
    }
    const TypeIndex settler = rules_->unit("UNIT_SETTLER");
    if (settler != kNone && !state_.unitAt(at, UnitLayer::Civilian, *rules_)) {
        Unit& s = spawnUnit(settler, pid, at);
        applyFoundCity(Command::foundCity(pid, s.id));
    }
    refreshVisibility(pid);
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
    std::optional<std::vector<PathStep>> path;  // to the goal
    if (goal && *goal != u->pos) path = findPath(id, *goal);
    if (!path) {
        goal.reset();
        Rng& rng = state_.rng.get(RngStream::Gameplay);
        const std::vector<Hex> around = state_.grid.within(camp->pos, 10);
        for (int tries = 0; tries < 12 && !goal; ++tries) {
            const Hex h = around[rng.below(static_cast<uint32_t>(around.size()))];
            if (h == u->pos || !isLandPassable(state_, *rules_, h) || state_.cityAt(h)) continue;
            path = findPath(id, h);
            if (path) goal = h;
        }
    }
    if (!goal) return;
    u->moveTarget = *goal;
    u->activity = Activity::Awake;
    // The first step takes the path just found, as a checked move's does (advanceUnit): the order and activity set
    // since do not enter the search.
    if (!u->moveOverland) checkedPath_ = CheckedPath{id, std::move(*path)};
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
    // Barbarian Clans mode: its units leave bribed civs alone, and an incited camp's go after their target.
    const bool incited = camp && camp->incitedAgainst != kNoPlayer && camp->incitedUntil >= state_.turn;
    auto spared = [&](PlayerId owner) { return camp && campLeavesAlone(*camp, owner); };
    auto ownerAt = [&](Hex h) {
        if (const City* c = state_.cityAt(h)) return c->owner;
        for (const Unit& o : state_.units) {
            if (o.pos == h && o.owner != me) return o.owner;
        }
        return kNoPlayer;
    };

    auto tryAttack = [&]() {
        const Unit* self = state_.unit(id);
        if (!self || self->movesLeft <= Fixed()) return false;
        const bool ranged = unitRange(*self) > 0;
        for (const Hex& h : state_.grid.within(self->pos, std::max(1, unitRange(*self)))) {
            if (spared(ownerAt(h))) continue;
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
    int bestDist = incited ? 16 : 9;  // an incited camp's units go further for their target
    if (bold >= raidAt && alerted) {
        for (const Unit& e : state_.units) {
            const int d = state_.grid.distance(u->pos, e.pos);
            if (atWar(me, e.owner) && !spared(e.owner) && d < bestDist) {
                bestDist = d;
                goal = e.pos;
            }
        }
        for (const City& c : state_.cities) {
            const int d = state_.grid.distance(u->pos, c.pos);
            if (bold >= cityAt && !spared(c.owner) && d < bestDist) {
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
        std::optional<std::vector<PathStep>> path = findPath(id, n);
        if (!path) continue;
        Unit* mover = state_.unit(id);
        mover->moveTarget = n;
        mover->activity = Activity::Awake;
        if (!mover->moveOverland) checkedPath_ = CheckedPath{id, std::move(*path)};  // the first step takes it (as the scout's)
        advanceUnit(id);
        if (Unit* after = state_.unit(id)) after->moveTarget.reset();  // re-decided every turn
        break;
    }
    tryAttack();
}

}  // namespace sov
