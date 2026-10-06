// Espionage (08-diplomacy-city-states-governors.md, Espionage; data: diplomacy-espionage.md).
// Spies are off-map agents beside the leader's assassins, within a capacity civics and techs
// grant. A spy travels to a city (3 turns), then works an operation: Counterspy and Listening
// Post go on until changed; Gain Sources and the offensive operations end after their turns
// and, for the offensive ones, roll 3d6 against base - 2, less the spy's level and Gain
// Sources' bonus, plus a counterspy's defence. Success raises the spy a level; failure means
// escape or capture, and the target remembers.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }

// Chance in percent that 3d6 rolls at least `need`.
int chance3d6(int need) {
    if (need <= 3) return 100;
    if (need > 18) return 0;
    int hits = 0;
    for (int a = 1; a <= 6; ++a)
        for (int b = 1; b <= 6; ++b)
            for (int c = 1; c <= 6; ++c) hits += a + b + c >= need ? 1 : 0;
    return hits * 100 / 216;
}

int roll3d6(Rng& rng) { return rng.range(1, 6) + rng.range(1, 6) + rng.range(1, 6); }

const char* operationId(SpyMission m) {
    switch (m) {
        case SpyMission::Counterspy: return "SPYOP_COUNTERSPY";
        case SpyMission::ListeningPost: return "SPYOP_LISTENING_POST";
        case SpyMission::GainSources: return "SPYOP_GAIN_SOURCES";
        case SpyMission::SiphonFunds: return "SPYOP_SIPHON_FUNDS";
        case SpyMission::StealTechBoost: return "SPYOP_STEAL_TECH_BOOST";
        case SpyMission::SabotageProduction: return "SPYOP_SABOTAGE_PRODUCTION";
        case SpyMission::NeutralizeGovernor: return "SPYOP_NEUTRALIZE_GOVERNOR";
        case SpyMission::FomentUnrest: return "SPYOP_FOMENT_UNREST";
        case SpyMission::GreatWorkHeist: return "SPYOP_GREAT_WORK_HEIST";
        case SpyMission::RecruitPartisans: return "SPYOP_RECRUIT_PARTISANS";
        case SpyMission::BreachDam: return "SPYOP_BREACH_DAM";
        case SpyMission::DisruptRocketry: return "SPYOP_DISRUPT_ROCKETRY";
        case SpyMission::FabricateScandal: return "SPYOP_FABRICATE_SCANDAL";
        case SpyMission::None: break;
    }
    return "";
}
}  // namespace

int Game::spyPromotionTotal(const Agent& spy, int SpyPromotionType::*field) const {
    int n = 0;
    for (TypeIndex p : spy.promotions) n += at(p) < rules_->spyPromotions.size() ? rules_->spyPromotions[at(p)].*field : 0;
    return n;
}

int Game::spyOperationLevels(const Agent& spy, SpyMission m) const {
    const TypeIndex op = rules_->spyOperation(operationId(m));
    int n = spyPromotionTotal(spy, &SpyPromotionType::allLevels);
    for (TypeIndex p : spy.promotions) {
        if (op != kNone && at(p) < rules_->spyPromotions.size()) n += rules_->spyPromotions[at(p)].levels[at(op)];
    }
    return n;
}

int Game::spyCapacity(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    int n = 0;
    for (size_t i = 0; i < rules_->techs.size(); ++i) n += p.techs.has(static_cast<TypeIndex>(i)) ? rules_->techs[i].spies : 0;
    for (size_t i = 0; i < rules_->civics.size(); ++i) n += p.civics.has(static_cast<TypeIndex>(i)) ? rules_->civics[i].spies : 0;
    return n;
}

int Game::spiesOf(PlayerId pid) const {
    return static_cast<int>(std::count_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& a) { return a.spy && a.owner == pid; }));
}

const SpyOperationType* Game::spyOperationFor(SpyMission m) const {
    const TypeIndex op = rules_->spyOperation(operationId(m));
    return op == kNone ? nullptr : &rules_->spyOperations[at(op)];
}

bool Game::canSpyMission(PlayerId pid, int32_t spyId, SpyMission m, CityId cityId, CommandError* why) const {
    auto fail = [&]() {
        if (why) *why = CommandError::CannotSpy;
        return false;
    };
    const Agent* a = agent(spyId);
    if (!a || !a->spy || a->owner != pid || static_cast<int>(m) >= kNumSpyMissions) return fail();
    if (m == SpyMission::None) return true;  // home
    const City* c = state_.city(cityId);
    const SpyOperationType* op = spyOperationFor(m);
    if (!c || !op) return fail();
    if (m == SpyMission::Counterspy) {
        if (c->owner != pid) return fail();
    } else if (m == SpyMission::FabricateScandal) {
        // A city-state where another civ holds envoys to lose.
        const PlayerId suz = isCityState(c->owner) ? suzerainOf(c->owner) : kNoPlayer;
        if (suz == kNoPlayer || suz == pid || !hasMet(pid, suz)) return fail();
    } else if (c->owner == pid || !isMajorCiv(c->owner) || !hasMet(pid, c->owner)) {
        return fail();
    }
    if (m == SpyMission::GreatWorkHeist && c->greatWorks.empty()) return fail();
    if (op->needsDistrict && (op->district == kNone || !c->district(op->district, true))) return fail();
    if (m == SpyMission::NeutralizeGovernor) {
        PlayerId holder = kNoPlayer;
        if (!establishedGovernor(*c, &holder) || holder != c->owner) return fail();
    }
    if (why) *why = CommandError::Ok;
    return true;
}

int Game::spySuccessPercent(int32_t spyId, SpyMission m, CityId cityId) const {
    const Agent* a = agent(spyId);
    const SpyOperationType* op = spyOperationFor(m);
    const City* c = state_.city(cityId);
    if (!a || !op || !c || op->base <= 0) return 100;
    int need = op->base - 2 - (a->level - 1 + spyOperationLevels(*a, m)) * op->levelChange;
    if (a->sourcesCity == cityId && state_.turn <= a->sourcesUntil) need -= rules_->globalInt("ESPIONAGE_BONUS_GAIN_SOURCES");
    // The city's best counterspy, and Amani's Local Informants (+3 levels), defend.
    int defender = 0;
    for (const Agent& o : state_.agents) {
        if (o.spy && o.owner == c->owner && o.city == cityId && o.travel == 0 && o.mission == SpyMission::Counterspy)
            defender = std::max(defender, o.level + spyPromotionTotal(o, &SpyPromotionType::counterspyLevels) + spyPromotionTotal(o, &SpyPromotionType::allLevels));
    }
    PlayerId holder = kNoPlayer;
    if (const Governor* g = establishedGovernor(*c, &holder); g && holder == c->owner && governorHasPromotion(*g, "GOVERNOR_PROMOTION_LOCAL_INFORMANTS"))
        defender += 3;
    if (defender > 0) need += op->enemyChange + op->enemyLevelChange * (defender - 1);
    return chance3d6(need);
}

void Game::processSpies(PlayerId pid) {
    const int speed = rules_->speeds[at(rules_->speed(state_.setup.speed))].costPercent;
    std::vector<int32_t> ids;
    for (const Agent& a : state_.agents) {
        if (a.spy && a.owner == pid) ids.push_back(a.id);
    }
    for (int32_t id : ids) {
        auto it = std::find_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& x) { return x.id == id; });
        if (it == state_.agents.end() || it->city == kNoCity) continue;
        Agent& a = *it;
        const City* c = state_.city(a.city);
        // A city razed, or one that changed hands under the operation, sends the spy home.
        const bool wrongSide = c && a.mission != SpyMission::None &&
                               ((a.mission == SpyMission::Counterspy) != (c->owner == pid));
        if (!c || wrongSide) {
            a.city = kNoCity;
            a.mission = SpyMission::None;
            a.travel = a.missionTurns = 0;
            continue;
        }
        if (a.travel > 0) {
            --a.travel;
            continue;
        }
        if (a.mission == SpyMission::None || a.mission == SpyMission::Counterspy || a.mission == SpyMission::ListeningPost) continue;
        if (--a.missionTurns > 0) continue;
        if (a.mission == SpyMission::GainSources) {
            const SpyOperationType* op = spyOperationFor(SpyMission::GainSources);
            a.sourcesCity = a.city;
            a.sourcesUntil = state_.turn + (op ? op->turns : 8) * rules_->globalInt("ESPIONAGE_GAIN_SOURCES_DURATION_MULTIPLIER") * speed / 100;
            a.mission = SpyMission::None;
            continue;
        }
        resolveSpyOperation(a);
    }
}

void Game::resolveSpyOperation(Agent& a) {
    City& c = *state_.city(a.city);
    const PlayerId sender = a.owner;
    // Fabricate Scandal wrongs the city-state's suzerain; every other operation the city's owner.
    const PlayerId victim = a.mission == SpyMission::FabricateScandal && isCityState(c.owner) && suzerainOf(c.owner) != kNoPlayer ? suzerainOf(c.owner) : c.owner;
    const SpyMission m = a.mission;
    const int percent = spySuccessPercent(a.id, m, c.id);
    const SpyOperationType* op = spyOperationFor(m);
    // The roll that gives the shown chance: success at or above the need.
    int need = 3;
    while (need <= 18 && chance3d6(need) > percent) ++need;
    Rng& rng = state_.rng.get(RngStream::Combat);
    const int roll = roll3d6(rng);
    a.mission = SpyMission::None;
    if (op && op->base > 0 && roll < need) {
        // Failure: escape home, or capture (08: Outcomes; escape base ESPIONAGE_ESCAPE_BASE_CHANCE).
        int escapeNeed = rules_->globalInt("ESPIONAGE_ESCAPE_BASE_CHANCE") - (a.level - 1) * rules_->globalInt("ESPIONAGE_ESCAPE_LEVEL_BOOST") -
                         spyPromotionTotal(a, &SpyPromotionType::escape);
        for (const Agent& o : state_.agents) {
            if (o.spy && o.owner == victim && o.city == c.id && o.mission == SpyMission::Counterspy)
                escapeNeed -= rules_->globalInt("ESPIONAGE_ESCAPE_COUNTERSPY_LEVEL_MODIFIER") * o.level;
        }
        const bool escaped = roll3d6(rng) >= escapeNeed;
        remember(victim, sender, MemoryKind::SpyCaught, escaped ? -6 : -12, escaped ? 40 : 60);
        addGrievance(victim, sender, 25);  // espionage caught (Sovereign's base)
        pushEvent(EventKind::SpyCaught, sender, victim, escaped ? 1 : 0);
        if (escaped) {
            a.city = kNoCity;
        } else {
            const int32_t gone = a.id;
            state_.agents.erase(std::remove_if(state_.agents.begin(), state_.agents.end(), [&](const Agent& x) { return x.id == gone; }),
                                state_.agents.end());
        }
        return;
    }
    // Success: the operation's effect; the spy rises a level and stays in the city.
    Player& thief = state_.players[at(sender)];
    Player& mark = state_.players[at(victim)];
    switch (m) {
        case SpyMission::SiphonFunds: {
            // Sovereign reading (the engine's formula is unverified): the city's gold per turn
            // times (3 + the spy's level), as far as the treasury goes.
            const Fixed perTurn = cityReport(c.id).yields[static_cast<size_t>(YieldType::Gold)];
            Fixed take = perTurn * (3 + a.level);
            if (take > mark.gold) take = mark.gold;
            if (take > Fixed()) {
                mark.gold -= take;
                thief.gold += take;
            }
            break;
        }
        case SpyMission::StealTechBoost: {
            std::vector<size_t> options;
            for (size_t i = 0; i < rules_->techs.size(); ++i) {
                if (mark.techs.done[i] && !thief.techs.done[i] && !thief.techs.boosted[i]) options.push_back(i);
            }
            if (!options.empty()) {
                const size_t t = options[rng.below(static_cast<uint32_t>(options.size()))];
                const int boostPct = rules_->techs[t].boost.percent > 0 ? rules_->techs[t].boost.percent : 40;
                thief.techs.boosted[t] = 1;
                thief.techs.progress[t] += Fixed::fromInt(techCost(static_cast<TypeIndex>(t))) * boostPct / 100;
            }
            break;
        }
        case SpyMission::SabotageProduction:
            // The works are set back: the current item's progress and any overflow are lost.
            if (!c.queue.empty()) {
                for (ProductionProgress& pp : c.progress) {
                    if (pp.item == c.queue.front()) pp.amount = Fixed();
                }
            }
            c.overflow = Fixed();
            break;
        case SpyMission::NeutralizeGovernor:
            for (Governor& g : mark.governors) {
                if (g.city == c.id) g.establishTurns = rules_->globalInt("ESPIONAGE_NEUTRALIZE_GOVERNOR_BASE_TURNS");
            }
            break;
        case SpyMission::FomentUnrest:
            c.loyalty = std::max(0, c.loyalty + rules_->globalInt("ESPIONAGE_FOMENT_UNREST_BASE_LOYALTY_CHANGE") +
                                        rules_->globalInt("ESPIONAGE_FOMENT_UNREST_LEVEL_LOYALTY_CHANGE") * a.level);
            break;
        case SpyMission::GreatWorkHeist: {
            // The first Great Work with a free slot of its kind in one of the thief's cities moves there.
            for (size_t w = 0; w < c.greatWorks.size(); ++w) {
                const TypeIndex type = c.greatWorks[w].type;
                for (City& home : state_.cities) {
                    if (home.owner != sender) continue;
                    const TypeIndex slot = freeGreatWorkSlot(home, type);
                    if (slot == kNone) continue;
                    GreatWork moved = c.greatWorks[w];
                    moved.building = slot;
                    home.greatWorks.push_back(moved);
                    c.greatWorks.erase(c.greatWorks.begin() + static_cast<std::ptrdiff_t>(w));
                    w = c.greatWorks.size();
                    break;
                }
            }
            break;
        }
        case SpyMission::RecruitPartisans: {
            // Two rebels (barbarians) of the strongest melee unit the city's owner can field rise beside it.
            const PlayerId bp = barbarianPlayer();
            TypeIndex best = kNone;
            for (size_t i = 0; i < rules_->units.size(); ++i) {
                const UnitType& ut = rules_->units[i];
                if (ut.unitClass != "MELEE" || ut.domain != Domain::Land || ut.uniqueTo != kNone || !hasUnlocked(victim, ut.unlock)) continue;
                if (best == kNone || ut.combat > rules_->units[at(best)].combat) best = static_cast<TypeIndex>(i);
            }
            int raised = 0;
            for (const Hex& h : state_.grid.within(c.pos, 2)) {
                if (bp == kNoPlayer || best == kNone || raised >= 2) break;
                if (h == c.pos || !isLandPassable(state_, *rules_, h) || state_.unitAt(h, UnitLayer::Military, *rules_) || state_.cityAt(h)) continue;
                spawnUnit(best, bp, h);
                ++raised;
            }
            break;
        }
        case SpyMission::BreachDam:
            // The river floods: the city's floodplain improvements are pillaged and units there are hurt.
            for (const Hex& h : state_.grid.within(c.pos, 3)) {
                Plot& pl = state_.plot(h);
                if (pl.city != c.id || pl.feature == kNone || rules_->features[at(pl.feature)].id.rfind("FEATURE_FLOODPLAINS", 0) != 0) continue;
                if (pl.improvement != kNone) pl.pillagedTurns = 5;
                for (Unit& u : state_.units) {
                    if (u.pos == h && !isLeader(u)) u.hp = std::max(1, u.hp - 30);
                }
            }
            break;
        case SpyMission::DisruptRocketry:
            // The space race project under way in the city loses its progress.
            for (ProductionProgress& pp : c.progress) {
                if (pp.item.kind == ProductionKind::Project && rules_->projects[at(pp.item.type)].spaceRace) pp.amount = Fixed();
            }
            break;
        case SpyMission::FabricateScandal: {
            // The suzerain loses 2 envoys there, +1 per spy level beyond the first (envoys are kept by the sender, per city-state).
            Player& suzerain = state_.players[at(victim)];
            if (at(c.owner) < suzerain.envoys.size()) suzerain.envoys[at(c.owner)] = std::max(0, suzerain.envoys[at(c.owner)] - (1 + a.level));
            break;
        }
        default: break;
    }
    if (a.level < rules_->globalInt("ESPIONAGE_MAX_LEVEL")) {
        ++a.level;
        ++a.promotionsPending;  // a promotion to choose (08: Espionage levels)
    }
    // A narrow success is noticed: the target knows who it was.
    if (op && roll < need + 2) remember(victim, sender, MemoryKind::SpyCaught, -4, 30);
    pushEvent(EventKind::SpyOperation, sender, victim, static_cast<int>(m));
}

}  // namespace sov
