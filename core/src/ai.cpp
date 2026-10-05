// The computer opponent; see ai.h. Weights follow the Civ VI defaults quoted in
// 10-ai-ui-implementation.md (StandardSettlePlot, DefaultYieldBias) where the spec
// gives them; the rest are Sovereign tuning, kept here as named constants.
#include "sovereign/ai.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sovereign/mapgen.h"

namespace sov::ai {

namespace {

// --- tuning ------------------------------------------------------------------
constexpr int kFreshWater = 20;       // StandardSettlePlot
constexpr int kCoastal = 12;          // StandardSettlePlot
constexpr int kPerTileFromCity = 10;  // StandardSettlePlot: nearest friendly city, per tile
constexpr int kNewResource = 4;       // StandardSettlePlot
constexpr int kMinSiteScore = 150;    // below this a site is not worth a Settler
constexpr int kSettleSearch = 9;      // plots from the Settler searched for a site
constexpr int kTravelPenalty = 8;     // site value lost per turn of travel
constexpr int kMaxCities = 16;
constexpr int kSiteSurvey = 8;
constexpr int kEarlyTurns = 120;      // the expansion phase: settlers weigh more
constexpr int kLongBuild = 12;        // turns beyond which an item loses value in proportion        // plots around our cities searched for a free site
constexpr int kWarRatioPercent = 130;   // own strength vs target's to declare war
constexpr int kPeaceRatioPercent = 80;  // below this, offer peace
constexpr int kWarWeariness = 50;       // turns of war before peace is offered anyway
constexpr int kNeighbourRange = 14;     // a target's city must be this close to one of ours
constexpr int kFriendOpinion = 15;      // at or above: offer friendship, never pick as a war target
constexpr int kDenounceOpinion = -25;   // at or below: denounce
constexpr int kProposalGap = 10;        // turns between deals put to the same civ
constexpr int kThreatRange = 4;
constexpr int kHealBelow = 40;

size_t at(TypeIndex i) { return static_cast<size_t>(i); }
size_t yi(YieldType y) { return static_cast<size_t>(y); }

// Yield weights per point (DefaultYieldBias favours production and gold; food
// matters most to a young empire).
int yieldValue(const Yields& y) {
    static const int w[kNumYields] = {3, 3, 2, 3, 3, 1};
    Fixed total;
    for (size_t i = 0; i < kNumYields; ++i) total += y[i] * w[i];
    return static_cast<int>(total.round());
}

int power(const UnitType& t) { return std::max(t.combat, t.ranged); }

bool isArmy(const UnitType& t) { return t.layer == UnitLayer::Military && t.domain == Domain::Land && power(t) > 0; }

// --- per-turn view ---------------------------------------------------------------
struct View {
    Game& game;
    const Rules& r;
    PlayerId me;
    std::vector<CityId> cities;
    std::vector<int> threat;       // per entry of `cities`: visible hostile strength nearby
    std::vector<PlayerId> enemies; // majors at war with us
    PlayerId target = kNoPlayer;   // the major whose cities the army marches on
    int military = 0, ranged = 0, settlers = 0, builders = 0;
    std::vector<Hex> claimed;      // sites and plots other units are already heading for
    bool majorWar = false;         // at war with a major civ (city-state wars do not stop expansion)
    int sites = -1;                // free city sites near our cities (-1: not counted yet this turn)

    View(Game& g, PlayerId p) : game(g), r(g.rules()), me(p) {}
    const GameState& s() const { return game.state(); }
    bool hostile(PlayerId other) const { return game.atWar(me, other); }
    bool claimedNear(Hex h, int range) const {
        for (const Hex& c : claimed) {
            if (s().grid.distance(c, h) <= range) return true;
        }
        return false;
    }
};

void survey(View& v) {
    const GameState& s = v.s();
    v.cities.clear();
    for (const City& c : s.cities) {
        if (c.owner == v.me) v.cities.push_back(c.id);
    }
    v.threat.assign(v.cities.size(), 0);
    v.military = v.ranged = v.settlers = v.builders = 0;
    v.claimed.clear();
    for (const Unit& u : s.units) {
        const UnitType& t = v.r.units[at(u.type)];
        if (u.owner == v.me) {
            if (isArmy(t)) {
                ++v.military;
                v.ranged += t.range > 0;
            }
            v.settlers += t.foundCity;
            v.builders += t.buildCharges > 0;
            if (u.moveTarget && (t.foundCity || t.buildCharges > 0)) v.claimed.push_back(*u.moveTarget);
            continue;
        }
        if (!v.hostile(u.owner) || !isArmy(t) || v.game.visibility(v.me, u.pos) != Visibility::Visible) continue;
        for (size_t i = 0; i < v.cities.size(); ++i) {
            if (s.grid.distance(s.city(v.cities[i])->pos, u.pos) <= kThreatRange) v.threat[i] += power(t) * u.hp / 100;
        }
    }
    for (CityId id : v.cities) {
        const City& c = *s.city(id);
        if (!c.queue.empty() && c.queue.front().kind == ProductionKind::Unit &&
            v.r.units[at(c.queue.front().type)].foundCity)
            ++v.settlers;
    }
    v.enemies.clear();
    v.majorWar = false;
    for (const Player& p : s.players) {
        if (p.alive && !p.barbarian && v.hostile(p.id)) {
            v.enemies.push_back(p.id);
            v.majorWar |= v.game.isMajorCiv(p.id);
        }
    }
    v.sites = -1;
}

bool hasGarrison(const View& v, const City& c) {
    const Unit* u = v.s().unitAt(c.pos, UnitLayer::Military, v.r);
    return u && u->owner == v.me;
}

int cityIndex(const View& v, CityId id) {
    for (size_t i = 0; i < v.cities.size(); ++i) {
        if (v.cities[i] == id) return static_cast<int>(i);
    }
    return -1;
}

// Distance from a plot to the nearest city of `owner` (INT_MAX when it has none).
int distanceToCity(const GameState& s, PlayerId owner, Hex h) {
    int best = INT_MAX;
    for (const City& c : s.cities) {
        if (c.owner == owner) best = std::min(best, s.grid.distance(c.pos, h));
    }
    return best;
}

// Free sites worth a Settler within kSiteSurvey of our cities (counted once per turn).
int freeSites(View& v) {
    if (v.sites >= 0) return v.sites;
    const GameState& s = v.s();
    std::vector<Hex> found;
    for (CityId id : v.cities) {
        for (const Hex& h : s.grid.within(s.city(id)->pos, kSiteSurvey)) {
            if (v.game.visibility(v.me, h) == Visibility::Unrevealed || v.claimedNear(h, 3)) continue;
            bool near = false;
            for (const Hex& f : found) near = near || s.grid.distance(f, h) < 4;
            if (!near && settleScore(v.game, v.me, h) >= kMinSiteScore) found.push_back(h);
        }
    }
    v.sites = static_cast<int>(found.size());
    return v.sites;
}

// --- diplomacy ---------------------------------------------------------------------
void diplomacy(View& v) {
    const GameState& s = v.s();
    const int mine = militaryStrength(v.game, v.me);
    for (PlayerId e : v.enemies) {
        const int theirs = militaryStrength(v.game, e);
        const Relation& rel = s.players[at(v.me)].relations[at(e)];
        const bool theyOffer = s.players[at(e)].relations[at(v.me)].peaceOffered;
        const bool losing = mine * 100 < theirs * kPeaceRatioPercent;
        // Weariness only ends a war that is not clearly being won.
        const bool tired = s.turn - rel.since >= kWarWeariness && mine * 100 < theirs * kWarRatioPercent * 2;
        const bool accept = theyOffer && mine * 100 < theirs * kWarRatioPercent;
        if ((losing || tired || accept) && v.game.canMakePeace(v.me, e)) v.game.submit(Command::makePeace(v.me, e));
    }
    survey(v);
    if (!v.enemies.empty() || v.cities.size() < 2) {
        // Keep marching on the nearest enemy.
        int best = INT_MAX;
        for (PlayerId e : v.enemies) {
            for (CityId id : v.cities) {
                const int d = distanceToCity(s, e, s.city(id)->pos);
                if (d < best) {
                    best = d;
                    v.target = e;
                }
            }
        }
        return;
    }
    // War on the weakest neighbour we clearly outmatch whose cities we have seen.
    PlayerId pick = kNoPlayer;
    int pickStrength = INT_MAX;
    for (const Player& p : s.players) {
        if (!p.alive || p.barbarian || p.id == v.me || !v.game.canDeclareWar(v.me, p.id)) continue;
        // City-states only once there is no more room to settle.
        if (v.game.isCityState(p.id) && freeSites(v) > 0) continue;
        bool near = false;
        for (const City& c : s.cities) {
            if (c.owner != p.id || v.game.visibility(v.me, c.pos) == Visibility::Unrevealed) continue;
            if (distanceToCity(s, v.me, c.pos) <= kNeighbourRange) near = true;
        }
        const int theirs = militaryStrength(v.game, p.id);
        if (near && mine * 100 >= theirs * kWarRatioPercent && theirs < pickStrength && v.game.opinionOf(v.me, p.id) < kFriendOpinion) {
            pick = p.id;
            pickStrength = theirs;
        }
    }
    if (pick == kNoPlayer) return;
    // A formal war: denounce first and wait out DIPLOMACY_DENOUNCE_WAR_DELAY, unless the target is
    // so much weaker that a surprise is worth the grievance (08: War types).
    const Relation& rel = s.players[at(v.me)].relations[at(pick)];
    const bool ready = v.game.denouncing(v.me, pick) && s.turn - rel.denouncedOn >= v.r.globalInt("DIPLOMACY_DENOUNCE_WAR_DELAY");
    const bool overwhelming = mine >= 2 * pickStrength;
    if (!ready && !overwhelming) {
        if (v.game.canDenounce(v.me, pick)) {
            v.game.submit(Command::denounce(v.me, pick));
            return;
        }
        if (v.game.denouncing(v.me, pick)) return;
    }
    if (v.game.submit(Command::declareWar(v.me, pick)) == CommandError::Ok) {
        v.target = pick;
        survey(v);
    }
}

// Deals with civs at peace: friendship with those it likes, luxury swaps, open borders; and
// denouncing those it loathes. It asks only for deals it gains from, and asks an AI only when
// that AI would say yes (a human always hears the offer, at most every kProposalGap turns).
void deals(View& v) {
    const GameState& s = v.s();
    for (const Player& o : s.players) {
        if (o.id == v.me || !v.game.isMajorCiv(o.id) || !v.game.hasMet(v.me, o.id) || v.game.atWar(v.me, o.id)) continue;
        const int opinion = v.game.opinionOf(v.me, o.id);
        if (opinion <= kDenounceOpinion && v.game.canDenounce(v.me, o.id)) {
            v.game.submit(Command::denounce(v.me, o.id));
            continue;
        }
        const Relation& rel = s.players[at(v.me)].relations[at(o.id)];
        if (rel.lastProposal > 0 && s.turn - rel.lastProposal < kProposalGap) continue;
        std::vector<std::vector<DealItem>> ideas;
        if (opinion >= kFriendOpinion) ideas.push_back({{DealItemKind::Friendship, v.me, 0, kNone}});
        TypeIndex give = kNone, get = kNone;
        for (size_t r = 0; r < v.r.resources.size(); ++r) {
            if (v.r.resources[r].cls != ResourceClass::Luxury) continue;
            const TypeIndex res = static_cast<TypeIndex>(r);
            if (give == kNone && v.game.luxuryCopies(v.me, res) - v.game.luxuryCopiesTraded(v.me, res) >= 2 && !v.game.hasLuxury(o.id, res)) give = res;
            if (get == kNone && v.game.luxuryCopies(o.id, res) - v.game.luxuryCopiesTraded(o.id, res) >= 2 && !v.game.hasLuxury(v.me, res)) get = res;
        }
        if (give != kNone && get != kNone) ideas.push_back({{DealItemKind::Resource, v.me, 1, give}, {DealItemKind::Resource, o.id, 1, get}});
        if (opinion >= 0) ideas.push_back({{DealItemKind::OpenBorders, v.me, 0, kNone}, {DealItemKind::OpenBorders, o.id, 0, kNone}});
        for (const std::vector<DealItem>& idea : ideas) {
            const Deal d{0, v.me, o.id, s.turn, idea};
            if (v.game.dealProblem(d) != CommandError::Ok || v.game.dealValue(v.me, d) < 0) continue;
            if (!o.human && !v.game.wouldAccept(o.id, d)) continue;
            v.game.submit(Command::proposeDeal(v.me, o.id, idea));
            break;  // one proposal per civ per turn
        }
    }
}

// --- espionage (08: Espionage) -----------------------------------------------------------
// Idle spies work against the strongest rival met (the war enemy first): steal a tech boost
// from a city with a Campus, else siphon funds from a Commercial Hub, else foment unrest; with
// no rival worth it, they guard the capital as counterspies.
void spies(View& v) {
    const GameState& s = v.s();
    PlayerId rival = v.enemies.empty() ? kNoPlayer : v.enemies.front();
    if (rival == kNoPlayer) {
        int best = INT_MIN;
        for (const Player& p : s.players) {
            if (p.id == v.me || !v.game.isMajorCiv(p.id) || !v.game.hasMet(v.me, p.id)) continue;
            const int sc = v.game.score(p.id);
            if (sc > best) {
                best = sc;
                rival = p.id;
            }
        }
    }
    for (const Agent& a : s.agents) {
        if (!a.spy || a.owner != v.me || a.mission != SpyMission::None || a.travel > 0) continue;
        int32_t bestCity = kNoCity;
        SpyMission bestMission = SpyMission::None;
        int bestValue = 0;
        for (const City& c : s.cities) {
            if (rival == kNoPlayer || c.owner != rival || v.game.visibility(v.me, c.pos) == Visibility::Unrevealed) continue;
            static const std::pair<SpyMission, int> tries[] = {
                {SpyMission::StealTechBoost, 30}, {SpyMission::SiphonFunds, 25}, {SpyMission::SabotageProduction, 20}, {SpyMission::FomentUnrest, 10}};
            for (const auto& [m, worth] : tries) {
                if (!v.game.canSpyMission(v.me, a.id, m, c.id)) continue;
                const int value = worth * v.game.spySuccessPercent(a.id, m, c.id) / 100 + (c.capital ? 0 : 2);
                if (value > bestValue) {
                    bestValue = value;
                    bestCity = c.id;
                    bestMission = m;
                }
            }
        }
        if (bestMission == SpyMission::None) {
            for (CityId c : v.cities) {
                if (v.game.state().city(c)->capital && v.game.canSpyMission(v.me, a.id, SpyMission::Counterspy, c)) {
                    bestCity = c;
                    bestMission = SpyMission::Counterspy;
                }
            }
        }
        if (bestMission != SpyMission::None && !(a.city == bestCity && a.mission == bestMission))
            v.game.submit(Command::spyMission(v.me, a.id, bestMission, bestCity));
    }
}

// --- governors (08: Governors) -----------------------------------------------------------
// Appoint up to four in a fixed order of usefulness, then spend titles on promotions; place
// each where it pays: Pingala in the capital, Victor in the most threatened city, Amani with
// the city-state we court most, the rest in the largest cities without one.
void governors(View& v) {
    static const char* const order[] = {"GOVERNOR_PINGALA", "GOVERNOR_MAGNUS", "GOVERNOR_LIANG", "GOVERNOR_VICTOR",
                                        "GOVERNOR_REYNA",   "GOVERNOR_MOKSHA", "GOVERNOR_AMANI"};
    const GameState& s = v.s();
    for (int guard = 0; guard < 8 && v.game.governorTitlesLeft(v.me) > 0; ++guard) {
        int appointed = 0;
        for (const char* id : order) appointed += v.game.governor(v.me, v.r.governor(id)) ? 1 : 0;
        bool spent = false;
        if (appointed < 4) {
            for (const char* id : order) {
                const TypeIndex g = v.r.governor(id);
                if (g != kNone && v.game.canAppointGovernor(v.me, g)) {
                    spent = v.game.submit(Command::appointGovernor(v.me, g)) == CommandError::Ok;
                    break;
                }
            }
        } else {
            for (const char* id : order) {
                const TypeIndex g = v.r.governor(id);
                if (g == kNone || !v.game.governor(v.me, g)) continue;
                std::vector<TypeIndex> promos = v.r.governors[at(g)].promotions;
                std::stable_sort(promos.begin(), promos.end(),
                                 [&](TypeIndex a, TypeIndex b) { return v.r.governorPromotions[at(a)].tier < v.r.governorPromotions[at(b)].tier; });
                for (TypeIndex p : promos) {
                    if (v.game.canPromoteGovernor(v.me, g, p)) {
                        spent = v.game.submit(Command::promoteGovernor(v.me, g, p)) == CommandError::Ok;
                        break;
                    }
                }
                if (spent) break;
            }
        }
        if (!spent) break;
    }
    // Places for unassigned governors.
    std::vector<CityId> taken;
    for (const Governor& g : s.players[at(v.me)].governors) {
        if (g.city != kNoCity) taken.push_back(g.city);
    }
    auto open = [&](CityId c) { return std::find(taken.begin(), taken.end(), c) == taken.end(); };
    for (const Governor& g : s.players[at(v.me)].governors) {
        if (g.city != kNoCity) continue;
        const std::string& id = v.r.governors[at(g.type)].id;
        CityId pick = kNoCity;
        if (id == "GOVERNOR_AMANI") {
            int most = -1;
            for (const Player& cs : s.players) {
                if (cs.cityState == kNone || !cs.alive) continue;
                for (const City& c : s.cities) {
                    if (c.owner != cs.id || !open(c.id) || !v.game.canAssignGovernor(v.me, g.type, c.id)) continue;
                    const int n = v.game.envoysAt(v.me, cs.id);
                    if (n > most) {
                        most = n;
                        pick = c.id;
                    }
                }
            }
        }
        if (pick == kNoCity && id == "GOVERNOR_PINGALA") {
            for (CityId c : v.cities) {
                if (s.city(c)->capital && open(c)) pick = c;
            }
        }
        if (pick == kNoCity && id == "GOVERNOR_VICTOR") {
            int worst = 0;
            for (size_t i = 0; i < v.cities.size(); ++i) {
                if (v.threat[i] > worst && open(v.cities[i])) {
                    worst = v.threat[i];
                    pick = v.cities[i];
                }
            }
        }
        if (pick == kNoCity) {
            int biggest = -1;
            for (CityId c : v.cities) {
                const City* city = s.city(c);
                if (open(c) && city->population > biggest) {
                    biggest = city->population;
                    pick = c;
                }
            }
        }
        if (pick != kNoCity && v.game.submit(Command::assignGovernor(v.me, g.type, pick)) == CommandError::Ok) taken.push_back(pick);
    }
}

// --- research and government ---------------------------------------------------------
// Value of what a tech or civic unlocks for us.
int unlockValue(const View& v, Unlock node) {
    const Rules& r = v.r;
    auto is = [&](const Unlock& u) { return u.civic == node.civic && u.index == node.index; };
    int bestPower = 0;
    for (size_t i = 0; i < r.units.size(); ++i) {
        if (isArmy(r.units[i]) && v.game.hasUnlocked(v.me, r.units[i].unlock)) bestPower = std::max(bestPower, power(r.units[i]));
    }
    int value = 1;
    for (const UnitType& t : r.units) {
        if (is(t.unlock)) value += isArmy(t) && power(t) > bestPower ? 2 + (v.enemies.empty() ? 0 : 6) : 1;
    }
    for (const BuildingType& b : r.buildings) {
        if (is(b.unlock)) value += 3 + yieldValue(b.yields) / 2 + b.outerDefenseHp / 50;
    }
    for (const DistrictType& d : r.districts) {
        if (is(d.unlock)) value += 6;
    }
    for (const ImprovementType& im : r.improvements) {
        if (is(im.unlock)) value += 3;
    }
    for (const ResourceType& res : r.resources) {
        if (is(res.reveal)) value += 2;
    }
    for (const GovernmentType& g : r.governments) {
        if (is(g.unlock)) value += 6;
    }
    for (const PolicyType& p : r.policies) {
        if (is(p.unlock)) value += 2;
    }
    return value;
}

template <typename Cost>
TypeIndex pickNode(const View& v, const std::vector<TypeIndex>& options, const TreeProgress& tree, bool civic, Cost cost) {
    TypeIndex best = kNone;
    int64_t bestScore = -1;
    for (TypeIndex n : options) {
        const Fixed left = Fixed::fromInt(cost(n)) - (at(n) < tree.progress.size() ? tree.progress[at(n)] : Fixed());
        const int64_t remaining = std::max<int64_t>(1, left.ceil());
        const int64_t score = static_cast<int64_t>(unlockValue(v, {civic, n})) * 100000 / remaining;
        if (score > bestScore) {
            bestScore = score;
            best = n;
        }
    }
    return best;
}

int policyValue(const View& v, TypeIndex policy) {
    const PolicyType& p = v.r.policies[at(policy)];
    int value = 1;
    for (const Modifier& m : v.r.modifiers) {
        if (m.source == p.id) value += 3;
    }
    if (p.slot == PolicySlot::Military && !v.enemies.empty()) value += 2;
    return value;
}

void research(View& v) {
    Game& g = v.game;
    const Player& pl = v.s().players[at(v.me)];
    if (pl.techs.current == kNone) {
        TypeIndex t = pickNode(v, g.availableTechs(v.me), pl.techs, false, [&](TypeIndex n) { return g.techCost(n); });
        if (t != kNone) g.submit(Command::chooseResearch(v.me, t));
    }
    if (pl.civics.current == kNone) {
        TypeIndex c = pickNode(v, g.availableCivics(v.me), pl.civics, true, [&](TypeIndex n) { return g.civicCost(n); });
        if (c != kNone) g.submit(Command::chooseCivic(v.me, c));
    }
    // The highest-tier government we can adopt.
    for (size_t gi = v.r.governments.size(); gi-- > 0;) {
        const Player& p = v.s().players[at(v.me)];
        const int current = p.government == kNone ? -1 : v.r.governments[at(p.government)].tier;
        if (v.r.governments[gi].tier > current && g.submit(Command::changeGovernment(v.me, static_cast<TypeIndex>(gi))) == CommandError::Ok)
            break;
    }
    // Fill empty slots; when changes are free, upgrade a slot to a better card.
    const size_t slots = v.s().players[at(v.me)].policies.size();
    for (size_t slot = 0; slot < slots; ++slot) {
        const Player& p = v.s().players[at(v.me)];
        const TypeIndex current = p.policies[slot];
        if (current != kNone && !p.freeChanges) continue;
        TypeIndex best = kNone;
        int bestValue = current == kNone ? 0 : policyValue(v, current);
        for (size_t k = 0; k < v.r.policies.size(); ++k) {
            const auto pol = static_cast<TypeIndex>(k);
            if (pol == current || !g.canSetPolicy(v.me, static_cast<int>(slot), pol)) continue;
            const int value = policyValue(v, pol);
            if (value > bestValue) {
                bestValue = value;
                best = pol;
            }
        }
        if (best != kNone) g.submit(Command::setPolicy(v.me, static_cast<int>(slot), best));
    }
}

// --- settling ----------------------------------------------------------------------
struct Site {
    Hex pos;
    int score = 0;
};

// The best reachable site for a Settler standing at `from`, travel time included.
std::optional<Site> bestSite(const View& v, UnitId settler, Hex from) {
    const GameState& s = v.s();
    std::vector<Site> sites;
    for (const Hex& h : s.grid.within(from, kSettleSearch)) {
        if (v.game.visibility(v.me, h) == Visibility::Unrevealed || v.claimedNear(h, 3)) continue;
        const int score = settleScore(v.game, v.me, h);
        if (score >= kMinSiteScore) sites.push_back({h, score});
    }
    std::stable_sort(sites.begin(), sites.end(), [](const Site& a, const Site& b) { return a.score > b.score; });
    if (sites.size() > 6) sites.resize(6);
    std::optional<Site> best;
    for (Site site : sites) {
        int turns = 0;
        if (site.pos != from) {
            auto path = v.game.findPath(settler, site.pos, true);
            if (!path || path->empty()) continue;
            turns = path->back().turn + 1;
        }
        site.score -= kTravelPenalty * turns;
        if (!best || site.score > best->score) best = site;
    }
    return best;
}

void settle(View& v, UnitId id) {
    const Unit* u = v.s().unit(id);
    if (v.cities.empty() && v.game.submit(Command::foundCity(v.me, id)) == CommandError::Ok) {
        survey(v);
        return;
    }
    if (u->moveTarget && v.game.canFoundCityAt(v.me, *u->moveTarget)) {
        v.claimed.push_back(*u->moveTarget);
        return;  // still on its way
    }
    std::optional<Site> site = bestSite(v, id, u->pos);
    if (site && site->pos == u->pos) {
        if (v.game.submit(Command::foundCity(v.me, id)) == CommandError::Ok) survey(v);
        return;
    }
    if (site && v.game.submit(Command::move(v.me, id, site->pos, true)) == CommandError::Ok) {
        v.claimed.push_back(site->pos);
        u = v.s().unit(id);
        if (u && u->pos == site->pos && u->movesLeft > Fixed() &&
            v.game.submit(Command::foundCity(v.me, id)) == CommandError::Ok)
            survey(v);
        return;
    }
    // Nowhere good in reach: settle here if allowed, else wait.
    if (!site && v.game.submit(Command::foundCity(v.me, id)) == CommandError::Ok) {
        survey(v);
        return;
    }
    v.game.submit(Command::setActivity(v.me, id, Activity::Skip));
}

// --- builders ----------------------------------------------------------------------
void build(View& v, UnitId id) {
    const GameState& s = v.s();
    const Unit* u = s.unit(id);
    if (u->moveTarget) return;
    auto worth = [&](Hex h) -> int {
        const Plot& p = s.plot(h);
        if (p.owner != v.me || p.city == kNoCity || p.improvement != kNone || s.districtAt(h) || s.wonderAt(h) != kNone || s.cityAt(h)) return -1;
        if (v.game.improvementsAt(v.me, h).empty()) return -1;
        int w = 10;
        if (p.resource != kNone && v.game.resourceVisible(v.me, h)) w += 20;
        const City* c = s.city(p.city);
        if (c && std::binary_search(c->worked.begin(), c->worked.end(), s.grid.index(h))) w += 10;
        return w;
    };
    if (worth(u->pos) >= 0) {
        // The resource's own improvement comes first in the list.
        std::vector<TypeIndex> options = v.game.improvementsAt(v.me, u->pos);
        if (v.game.submit(Command::buildImprovement(v.me, id, options.front())) == CommandError::Ok) return;
    }
    std::optional<Hex> best;
    int bestScore = INT_MIN;
    for (CityId cid : v.cities) {
        for (const Hex& h : s.grid.within(s.city(cid)->pos, 3)) {
            const int w = worth(h);
            if (w < 0 || v.claimedNear(h, 0) || s.foreignUnitAt(h, v.me)) continue;
            const int score = w * 10 - s.grid.distance(u->pos, h) * 15;
            if (score > bestScore) {
                bestScore = score;
                best = h;
            }
        }
    }
    if (best && v.game.submit(Command::move(v.me, id, *best, true)) == CommandError::Ok) {
        v.claimed.push_back(*best);
        u = s.unit(id);
        if (u && u->pos == *best && u->movesLeft > Fixed()) {
            std::vector<TypeIndex> options = v.game.improvementsAt(v.me, u->pos);
            if (!options.empty()) v.game.submit(Command::buildImprovement(v.me, id, options.front()));
        }
        return;
    }
    v.game.submit(Command::setActivity(v.me, id, Activity::Skip));
}

// --- military -------------------------------------------------------------------------
// Expected value of an attack in points; INT_MIN when it is not worth making.
int attackValue(const View& v, const Unit& u, const CombatPreview& pv) {
    if (!pv.valid) return INT_MIN;
    if (pv.captureCity) return 100000;
    if (pv.capture) return 5000;
    const UnitType& t = v.r.units[at(u.type)];
    const int dealt = (pv.damageToDefenderMin + pv.damageToDefenderMax) / 2;
    const int taken = (pv.damageToAttackerMin + pv.damageToAttackerMax) / 2;
    if (!pv.ranged && u.hp - pv.damageToAttackerMax <= 15) return INT_MIN;  // could die
    int value = dealt * 10 - taken * 12;
    if (pv.defender != kNoUnit) {
        const Unit* d = v.s().unit(pv.defender);
        if (d && pv.damageToDefenderMin >= d->hp) value += 1000;  // a sure kill
    } else if (pv.city != kNoCity) {
        // Melee into walls only with siege help; ranged chip damage is always welcome.
        if (pv.hitsWalls && !pv.ranged && t.bombard == 0) return INT_MIN;
        value += 200;
    }
    return value > 0 ? value : INT_MIN;
}

// Makes the best attack each unit has, ranged units first; repeats while attacks land.
void attacks(View& v) {
    for (int round = 0; round < 4; ++round) {
        std::vector<UnitId> order;
        for (const Unit& u : v.s().units) {
            if (u.owner == v.me && isArmy(v.r.units[at(u.type)]) && u.movesLeft > Fixed() && u.attacks < v.game.maxAttacks(u))
                order.push_back(u.id);
        }
        std::stable_sort(order.begin(), order.end(), [&](UnitId a, UnitId b) {
            return v.game.unitRange(*v.s().unit(a)) > v.game.unitRange(*v.s().unit(b));
        });
        bool any = false;
        for (UnitId id : order) {
            const Unit* u = v.s().unit(id);
            if (!u) continue;
            const int range = v.game.unitRange(*u);
            const bool ranged = range > 0;
            std::optional<Hex> best;
            int bestValue = INT_MIN;
            for (const Hex& h : v.s().grid.within(u->pos, std::max(1, range))) {
                const int value = attackValue(v, *u, v.game.previewAttack(id, h, ranged));
                if (value > bestValue) {
                    bestValue = value;
                    best = h;
                }
            }
            if (!best) continue;
            const Command c = ranged ? Command::rangedAttack(v.me, id, *best) : Command::attack(v.me, id, *best);
            any |= v.game.submit(c) == CommandError::Ok;
        }
        if (!any) break;
    }
    survey(v);
}

// Moves a unit next to `goal` (or onto it when `onto`); false when no route exists.
bool approach(View& v, UnitId id, Hex goal, bool onto) {
    const Unit* u = v.s().unit(id);
    if (onto) return u->pos == goal || v.game.submit(Command::move(v.me, id, goal, true)) == CommandError::Ok;
    if (v.s().grid.distance(u->pos, goal) <= 1) return true;
    std::vector<Hex> ring;
    for (const Hex& h : v.s().grid.within(goal, 1)) {
        if (h != goal) ring.push_back(h);
    }
    std::stable_sort(ring.begin(), ring.end(), [&](Hex a, Hex b) {
        return v.s().grid.distance(u->pos, a) < v.s().grid.distance(u->pos, b);
    });
    for (const Hex& h : ring) {
        if (v.game.submit(Command::move(v.me, id, h, true)) == CommandError::Ok) return true;
    }
    return false;
}

void rest(View& v, UnitId id) {
    const Unit* u = v.s().unit(id);
    if (u->activity == Activity::Fortify || u->activity == Activity::Sleep) return;
    const Activity a = v.r.units[at(u->type)].layer == UnitLayer::Military ? Activity::Fortify : Activity::Skip;
    if (v.game.submit(Command::setActivity(v.me, id, a)) != CommandError::Ok)
        v.game.submit(Command::setActivity(v.me, id, Activity::Skip));
}

// A revealed plot next to unexplored ground, nearest first.
bool explore(View& v, UnitId id) {
    const GameState& s = v.s();
    const Unit* u = s.unit(id);
    std::vector<std::pair<int, Hex>> frontier;
    for (const Hex& h : s.grid.within(u->pos, 8)) {
        if (h == u->pos || v.game.visibility(v.me, h) == Visibility::Unrevealed || !isLandPassable(s, v.r, h)) continue;
        bool edge = false;
        for (const Hex& n : s.grid.within(h, 1)) edge |= v.game.visibility(v.me, n) == Visibility::Unrevealed;
        if (edge) frontier.push_back({s.grid.distance(u->pos, h), h});
    }
    std::stable_sort(frontier.begin(), frontier.end(),
                     [](const std::pair<int, Hex>& a, const std::pair<int, Hex>& b) { return a.first < b.first; });
    for (size_t i = 0; i < frontier.size() && i < 6; ++i) {
        if (v.game.submit(Command::move(v.me, id, frontier[i].second, true)) == CommandError::Ok) return true;
    }
    return false;
}

void military(View& v) {
    const GameState& s = v.s();
    std::vector<UnitId> army;
    for (const Unit& u : s.units) {
        if (u.owner == v.me && isArmy(v.r.units[at(u.type)])) army.push_back(u.id);
    }
    std::vector<uint8_t> used(army.size(), 0);
    // 1. Garrisons: most threatened cities first; the unit already inside stays.
    std::vector<size_t> order(v.cities.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return v.threat[a] > v.threat[b]; });
    for (size_t ci : order) {
        const City& c = *s.city(v.cities[ci]);
        size_t pick = army.size();
        int bestDist = INT_MAX;
        for (size_t k = 0; k < army.size(); ++k) {
            if (used[k]) continue;
            const Unit& u = *s.unit(army[k]);
            const int d = s.grid.distance(u.pos, c.pos);
            if (d < bestDist && (d == 0 || u.hp >= kHealBelow)) {
                bestDist = d;
                pick = k;
            }
        }
        if (pick == army.size() || bestDist > 10) continue;
        used[pick] = 1;
        const UnitId id = army[pick];
        if (bestDist == 0) rest(v, id);
        else if (!approach(v, id, c.pos, true)) used[pick] = 0;
    }
    // 2. The rest: heal, march on the target, clear camps, scout, or stand guard.
    std::optional<Hex> targetCity;
    if (v.target != kNoPlayer && !v.cities.empty()) {
        const Hex home = s.city(v.cities.front())->pos;
        int best = INT_MAX;
        for (const City& c : s.cities) {
            if (c.owner != v.target || v.game.visibility(v.me, c.pos) == Visibility::Unrevealed) continue;
            const int d = s.grid.distance(home, c.pos);
            if (d < best) {
                best = d;
                targetCity = c.pos;
            }
        }
    }
    bool scouted = false;
    for (size_t k = 0; k < army.size(); ++k) {
        if (used[k]) continue;
        const UnitId id = army[k];
        const Unit* u = s.unit(id);
        if (!u || u->movesLeft <= Fixed()) continue;
        const UnitType& t = v.r.units[at(u->type)];
        if (u->hp < kHealBelow) {
            const Plot& here = s.plot(u->pos);
            if (here.owner == v.me) {
                v.game.submit(Command::setActivity(v.me, id, Activity::Skip));
                continue;
            }
            int best = INT_MAX;
            std::optional<Hex> home;
            for (CityId cid : v.cities) {
                const int d = s.grid.distance(u->pos, s.city(cid)->pos);
                if (d < best) {
                    best = d;
                    home = s.city(cid)->pos;
                }
            }
            if (home && approach(v, id, *home, false)) continue;
        }
        if (targetCity && t.unitClass != "RECON" && approach(v, id, *targetCity, false)) continue;
        if (v.enemies.empty() && power(t) >= 15) {
            const Camp* nearest = nullptr;
            for (const Camp& camp : s.camps) {
                if (v.game.visibility(v.me, camp.pos) == Visibility::Unrevealed) continue;
                if (distanceToCity(s, v.me, camp.pos) > 12) continue;
                if (!nearest || s.grid.distance(u->pos, camp.pos) < s.grid.distance(u->pos, nearest->pos)) nearest = &camp;
            }
            if (nearest) {
                const bool empty = !s.unitAt(nearest->pos, UnitLayer::Military, v.r);
                if (approach(v, id, nearest->pos, empty)) continue;
            }
        }
        if ((!scouted || t.unitClass == "RECON") && explore(v, id)) {
            scouted = true;
            continue;
        }
        if (s.plot(u->pos).owner == v.me || v.cities.empty()) rest(v, id);
        else if (!approach(v, id, s.city(v.cities.front())->pos, false)) rest(v, id);
    }
}

// --- the leader ------------------------------------------------------------------------
// Value of a piece of gear for the leader (its strength in that slot).
int gearValue(const GearType& g) { return g.slot == GearSlot::Armor ? g.defense : g.combat; }

// Keeps the leader home: in the capital, asleep behind the garrison, re-equipped with the
// best affordable melee weapon and armor while there (leader doc §7: never leave it exposed).
void leader(View& v) {
    const Unit* l = v.game.leaderOf(v.me);
    if (!l || v.cities.empty()) {
        if (l && !l->moveTarget) v.game.submit(Command::setActivity(v.me, l->id, Activity::Skip));
        return;
    }
    const GameState& s = v.s();
    const City* capital = nullptr;
    for (CityId id : v.cities) {
        if (s.city(id)->capital) capital = s.city(id);
    }
    if (!capital) capital = s.city(v.cities.front());
    if (l->pos != capital->pos) {
        if (!l->moveTarget || *l->moveTarget != capital->pos) v.game.submit(Command::move(v.me, l->id, capital->pos, true));
        l = v.game.leaderOf(v.me);
        if (l && l->pos != capital->pos && !l->moveTarget) v.game.submit(Command::setActivity(v.me, l->id, Activity::Skip));
        return;
    }
    const UnitId id = l->id;
    for (GearSlot slot : {GearSlot::Weapon, GearSlot::Armor}) {
        const Unit* cur = s.unit(id);
        const TypeIndex worn = cur->gear[at(static_cast<TypeIndex>(slot))];
        int bestValue = worn == kNone ? -1 : gearValue(v.r.gear[at(worn)]);
        TypeIndex best = kNone;
        for (size_t g = 0; g < v.r.gear.size(); ++g) {
            const GearType& gt = v.r.gear[g];
            if (gt.slot != slot || gt.ranged > 0 || gearValue(gt) <= bestValue) continue;
            // Keep a reserve for emergencies (purchases, upkeep).
            const Fixed reserve = Fixed::fromInt(v.game.gearCost(static_cast<TypeIndex>(g)) + 60);
            if (s.players[at(v.me)].gold < reserve || !v.game.canEquip(id, static_cast<TypeIndex>(g))) continue;
            bestValue = gearValue(gt);
            best = static_cast<TypeIndex>(g);
        }
        if (best != kNone && v.game.submit(Command::equipGear(v.me, id, best)) == CommandError::Ok) return;  // that took its turn
    }
    // Stances in the capital (§4): Fear when loyalty slips, Benevolence when amenities run short and gold allows.
    const CityReport rep = v.game.cityReport(capital->id);
    if (capital->loyalty < 60 && v.game.canTakeStance(v.me, capital->id, Stance::Fear))
        v.game.submit(Command::cityStance(v.me, capital->id, Stance::Fear));
    else if (rep.amenities < rep.amenitiesNeeded &&
             s.players[at(v.me)].gold > Fixed::fromInt(3 * v.game.benevolenceCost(*capital)) &&
             v.game.canTakeStance(v.me, capital->id, Stance::Benevolence))
        v.game.submit(Command::cityStance(v.me, capital->id, Stance::Benevolence));
    const Unit* now = s.unit(id);
    if (now->activity != Activity::Sleep && now->movesLeft > Fixed()) v.game.submit(Command::setActivity(v.me, id, Activity::Sleep));
}

// --- production --------------------------------------------------------------------
int desiredArmy(const View& v) {
    const int n = static_cast<int>(v.cities.size());
    int want = n + 1 + n / 3;
    if (!v.enemies.empty()) want += 2 * n + 2;
    return std::min(want, 4 * n + 4);
}

std::optional<ProductionItem> bestMilitaryUnit(const View& v, const std::vector<ProductionItem>& items) {
    const bool wantRanged = v.ranged * 2 < v.military - v.ranged;
    std::optional<ProductionItem> best;
    int bestScore = INT_MIN;
    for (const ProductionItem& it : items) {
        if (it.kind != ProductionKind::Unit) continue;
        const UnitType& t = v.r.units[at(it.type)];
        if (!isArmy(t) || t.unitClass == "RECON" || (t.bombard > 0 && v.enemies.empty())) continue;
        int score = power(t) * 100 - v.game.productionCost(v.me, it) / 2;
        if ((t.range > 0) == wantRanged) score += 800;
        if (score > bestScore) {
            bestScore = score;
            best = it;
        }
    }
    return best;
}

Hex districtSpot(const View& v, CityId cid, TypeIndex district) {
    const GameState& s = v.s();
    Hex spot{};
    int best = INT_MIN;
    for (const Hex& h : v.game.districtPlots(cid, district)) {
        int score = yieldValue(v.game.districtAdjacency(v.me, district, h)) * 10;
        const Plot& p = s.plot(h);
        if (p.resource != kNone) score -= 15;  // keep resources for improvements
        if (p.improvement != kNone) score -= 10;
        if (score > best) {
            best = score;
            spot = h;
        }
    }
    return spot;
}

void production(View& v) {
    const GameState& s = v.s();
    Game& g = v.game;
    for (CityId cid : g.citiesNeedingProduction(v.me)) {
        const City& c = *s.city(cid);
        const int ci = cityIndex(v, cid);
        const bool threatened = ci >= 0 && v.threat[static_cast<size_t>(ci)] > 0;
        const CityReport rep = g.cityReport(cid);
        std::vector<ProductionItem> items = g.buildableItems(cid);
        if (items.empty()) continue;
        std::optional<ProductionItem> soldier = bestMilitaryUnit(v, items);
        const bool needGuard = !hasGarrison(v, c) && v.military < static_cast<int>(v.cities.size());
        // No new army while the treasury runs down (maintenance), unless at war.
        const bool wantArmy = v.military < desiredArmy(v) && (!v.enemies.empty() || g.goldPerTurn(v.me) > Fixed());
        const bool minor = g.isCityState(v.me);  // a city-state: one city, no expansion, no trade, no wonders
        const int nCities = static_cast<int>(v.cities.size());
        const bool wantSettler = !minor && nCities + v.settlers < kMaxCities && v.settlers < 1 + nCities / 2 && c.population >= 2 &&
                                 !threatened && !v.majorWar && v.settlers < freeSites(v);
        int traders = 0;
        for (const Unit& u : s.units) traders += u.owner == v.me && v.r.units[at(u.type)].id == "UNIT_TRADER";
        for (CityId other : v.cities) {
            const City& oc = *s.city(other);
            traders += !oc.queue.empty() && oc.queue.front().kind == ProductionKind::Unit && v.r.units[at(oc.queue.front().type)].id == "UNIT_TRADER";
        }
        const bool wantTrader = !minor && g.tradeRoutesOf(v.me) + traders < g.tradeRouteCapacity(v.me);
        const bool wantBuilder = v.builders < (static_cast<int>(v.cities.size()) + 1) * 2 / 3 + 1 - (s.turn < 10 ? 1 : 0);
        const Fixed popRoom = rep.housing - Fixed::fromInt(c.population);
        // Assassins for wars against civs with a leader (leader doc §6), one in training at a time.
        bool assassinQueued = false;
        for (CityId other : v.cities) {
            const City& oc = *s.city(other);
            assassinQueued |= !oc.queue.empty() && oc.queue.front().kind == ProductionKind::Unit && v.r.units[at(oc.queue.front().type)].agent;
        }
        const bool wantAssassin = !v.enemies.empty() && !assassinQueued && g.agentsOf(v.me) < g.agentCapacity(v.me);
        const bool wantSpy = !assassinQueued && g.spiesOf(v.me) < g.spyCapacity(v.me);

        std::optional<ProductionItem> best;
        Hex bestAt{};
        int64_t bestScore = INT64_MIN;
        for (const ProductionItem& it : items) {
            int value = 0;
            Hex where{};
            switch (it.kind) {
                case ProductionKind::Unit: {
                    const UnitType& t = v.r.units[at(it.type)];
                    if (t.spy) value = wantSpy ? 200 : 0;
                    else if (t.agent) value = wantAssassin ? 250 : 0;
                    else if (t.id == "UNIT_TRADER") value = wantTrader ? 260 : 0;
                    else if (t.foundCity) value = wantSettler ? (s.turn < kEarlyTurns ? 600 : 400) : 0;
                    else if (t.buildCharges > 0) value = wantBuilder ? 160 : 0;
                    else if (soldier && it == *soldier) value = (needGuard || threatened) ? 700 : wantArmy ? (v.enemies.empty() ? 150 : 260) : 0;
                    break;
                }
                case ProductionKind::Building: {
                    const BuildingType& b = v.r.buildings[at(it.type)];
                    value = 30 + yieldValue(b.yields) * 25;
                    if (popRoom <= Fixed::fromInt(1)) value += static_cast<int>((b.housing * 30).round());
                    if (rep.amenities < rep.amenitiesNeeded) value += b.amenities * 25;
                    if (b.outerDefenseHp > 0) value += threatened ? 500 : v.enemies.empty() ? 0 : 60;
                    for (const auto& gpp : b.greatPersonPoints) value += 20 * gpp.second;  // great people (07)
                    for (const auto& slot : b.greatWorkSlots) value += 10 * slot.second;
                    if (b.wonder && minor) {
                        value = 0;
                        break;
                    }
                    if (b.wonder) {
                        // Wonders in a productive, safe city; on the plot the city has, or its first choice.
                        const Fixed prod = rep.yields[static_cast<size_t>(YieldType::Production)];
                        if (threatened || prod < Fixed::fromInt(4)) {
                            value = 0;
                            break;
                        }
                        value += 120 + static_cast<int>(b.wonderEffects.size()) * 40 + b.tradeCapacity * 60;
                        if (std::none_of(c.wonders.begin(), c.wonders.end(), [&](const CityWonder& w) { return w.building == it.type; })) {
                            const std::vector<Hex> plots = g.wonderPlots(cid, it.type);
                            if (plots.empty()) {
                                value = 0;
                                break;
                            }
                            where = plots.front();
                        }
                    }
                    break;
                }
                case ProductionKind::District: {
                    const DistrictType& d = v.r.districts[at(it.type)];
                    int gpp = 0;  // a specialty district also earns great people and opens its buildings
                    for (const auto& p : d.greatPersonPoints) gpp += p.second;
                    if (const CityDistrict* placed = c.district(it.type, false)) {
                        value = 100 + 40 * gpp + yieldValue(g.districtAdjacency(v.me, it.type, placed->pos)) * 25;
                        where = placed->pos;
                    } else {
                        where = districtSpot(v, cid, it.type);
                        value = 100 + 40 * gpp + yieldValue(g.districtAdjacency(v.me, it.type, where)) * 25;
                        // The first Holy Site while religions remain to be founded (06): the race for a Prophet.
                        if (d.id == "DISTRICT_HOLY_SITE" && s.players[at(v.me)].religion < 0 &&
                            static_cast<int>(s.religions.size()) < g.maxReligions() &&
                            std::none_of(v.cities.begin(), v.cities.end(), [&](CityId o) { return s.city(o)->district(it.type, false) != nullptr; }))
                            value += 150;
                        // Housing and amenities when the city runs short (Aqueduct, Neighborhood, Entertainment Complex ...).
                        if (popRoom <= Fixed::fromInt(1)) value += (d.aqueduct ? 6 : d.housing + (d.appealHousing.empty() ? 0 : 2)) * 40;
                        if (rep.amenities < rep.amenitiesNeeded) value += d.amenities * 80;
                        // At war, the first Encampment also opens assassins (leader doc §6).
                        if (d.id == "DISTRICT_ENCAMPMENT") value = v.enemies.empty() ? 10 : g.agentCapacity(v.me) == 0 ? 120 : 40;
                    }
                    break;
                }
            }
            if (value <= 0) continue;
            // Value per cost, and long builds lose value in a weak city (it should grow first).
            const int cost = g.productionCost(v.me, it);
            int64_t score = static_cast<int64_t>(value) * 1000 / (cost + 40);
            const int turns = cost / std::max(1, static_cast<int>(rep.yields[yi(YieldType::Production)].toInt()));
            if (turns > kLongBuild) score = score * kLongBuild / turns;
            if (score > bestScore) {
                bestScore = score;
                best = it;
                bestAt = where;
            }
        }
        if (!best) best = soldier ? *soldier : items.front();
        if (g.submit(Command::setProduction(v.me, cid, *best, bestAt)) != CommandError::Ok)
            g.submit(Command::setProduction(v.me, cid, items.front()));
        if (best->kind == ProductionKind::Unit) {
            const UnitType& t = v.r.units[at(best->type)];
            v.settlers += t.foundCity;
            v.builders += t.buildCharges > 0;
            v.military += isArmy(t);
        }
    }
}

// Gold: buy a defender for a threatened city without one, else spend a large surplus.
void purchases(View& v) {
    Game& g = v.game;
    for (size_t i = 0; i < v.cities.size(); ++i) {
        const City& c = *v.s().city(v.cities[i]);
        if (v.threat[i] == 0 || hasGarrison(v, c)) continue;
        std::optional<ProductionItem> soldier = bestMilitaryUnit(v, g.buildableItems(c.id));
        if (!soldier) continue;
        const int cost = g.purchaseCost(v.me, *soldier);
        if (cost > 0 && v.s().players[at(v.me)].gold >= Fixed::fromInt(cost)) g.submit(Command::purchase(v.me, c.id, *soldier));
    }
    // Savings (DefaultSavings: units 4, slush fund 3): a small reserve, less for growth items.
    const int reserve = 60 + 15 * static_cast<int>(v.cities.size());
    for (CityId cid : v.cities) {
        const City& c = *v.s().city(cid);
        if (c.queue.empty()) continue;
        const ProductionItem& front = c.queue.front();
        const bool growth = front.kind == ProductionKind::Unit && (v.r.units[at(front.type)].foundCity || v.r.units[at(front.type)].buildCharges > 0);
        const int cost = g.purchaseCost(v.me, front) + (growth ? 0 : reserve);
        if (cost > 0 && v.s().players[at(v.me)].gold >= Fixed::fromInt(cost)) {
            g.submit(Command::purchase(v.me, cid, c.queue.front()));
        }
    }
    // Still well above the reserve: buy the building that yields most per gold in any city.
    for (int guard = 0; guard < 4; ++guard) {
        const Fixed gold = v.s().players[at(v.me)].gold;
        std::optional<std::pair<CityId, ProductionItem>> best;
        int64_t bestScore = 0;
        for (CityId cid : v.cities) {
            for (const ProductionItem& it : g.buildableItems(cid)) {
                if (it.kind != ProductionKind::Building || v.r.buildings[at(it.type)].wonder) continue;
                const int cost = g.purchaseCost(v.me, it);
                if (cost <= 0 || gold < Fixed::fromInt(cost + reserve)) continue;
                const BuildingType& b = v.r.buildings[at(it.type)];
                const int64_t score = static_cast<int64_t>(30 + yieldValue(b.yields) * 25 + b.amenities * 25) * 1000 / cost;
                if (score > bestScore) {
                    bestScore = score;
                    best = std::make_pair(cid, it);
                }
            }
        }
        if (!best || g.submit(Command::purchase(v.me, best->first, best->second)) != CommandError::Ok) break;
    }
}

// Great people: used where they stand when they can be, otherwise walked to the nearest of
// our plots that suits them (a city with a free Great Work slot, their district, a city).
void greatPerson(View& v, UnitId id) {
    Game& g = v.game;
    const Unit* u = v.s().unit(id);
    if (g.canActivateGreatPerson(id)) {
        g.submit(Command::activateGreatPerson(v.me, id));
        return;
    }
    const GreatPersonType& gp = v.r.greatPeople[at(u->greatPerson)];
    if (gp.hasAura && gp.greatWorkCount == 0 && gp.effects.empty()) {
        g.submit(Command::setActivity(v.me, id, Activity::Sleep));  // a general's aura, kept at home
        return;
    }
    std::optional<Hex> best;
    int bestDist = INT_MAX;
    for (CityId cid : v.cities) {
        const City& c = *v.s().city(cid);
        std::vector<Hex> spots;
        if (gp.greatWorkCount > 0) {
            if (g.freeGreatWorkSlot(c, gp.greatWorkType) != kNone) spots.push_back(c.pos);
        } else if (gp.district != kNone && v.r.districts[at(gp.district)].id != "DISTRICT_CITY_CENTER") {
            const CityDistrict* d = c.district(gp.district, true);
            if (d) spots.push_back(d->pos);
        } else if (gp.unitDomain < 0) {
            spots.push_back(c.pos);
        }
        for (const Hex& h : spots) {
            const int d = v.s().grid.distance(u->pos, h);
            if (h != u->pos && d < bestDist) {
                bestDist = d;
                best = h;
            }
        }
    }
    if (best && g.submit(Command::move(v.me, id, *best, true)) == CommandError::Ok) return;
    g.submit(Command::setActivity(v.me, id, Activity::Sleep));  // nowhere to use it yet
}

// Religion (06): a pantheon as soon as Faith allows, a religion when a Prophet reaches a Holy
// Site, Apostles filling the belief classes, Missionaries bought and sent to the nearest city
// that does not follow the religion yet (our own first).
TypeIndex firstBelief(const View& v, BeliefClass cls) {
    TypeIndex fallback = kNone;
    for (TypeIndex b : v.game.availableBeliefs(cls)) {
        if (v.game.beliefModelled(b)) return b;
        if (fallback == kNone) fallback = b;
    }
    return fallback;
}

void pantheon(View& v) {
    if (v.s().players[at(v.me)].pantheon != kNone) return;
    const TypeIndex b = firstBelief(v, BeliefClass::Pantheon);
    if (b != kNone && v.game.canFoundPantheon(v.me, b)) v.game.submit(Command::foundPantheon(v.me, b));
}

void prophet(View& v, UnitId id) {
    Game& g = v.game;
    const Unit* u = v.s().unit(id);
    TypeIndex religion = kNone;
    for (size_t r = 0; r < v.r.religions.size() && religion == kNone; ++r) {
        bool taken = false;
        for (const FoundedReligion& f : v.s().religions) taken |= f.type == static_cast<TypeIndex>(r);
        if (!taken) religion = static_cast<TypeIndex>(r);
    }
    const TypeIndex founder = firstBelief(v, BeliefClass::Founder), follower = firstBelief(v, BeliefClass::Follower);
    if (g.canFoundReligion(id, religion, founder, follower)) {
        g.submit(Command::foundReligion(v.me, id, religion, founder, follower));
        return;
    }
    const TypeIndex holySite = v.r.district("DISTRICT_HOLY_SITE");
    std::optional<Hex> best;
    int bestDist = INT_MAX;
    for (CityId cid : v.cities) {
        const CityDistrict* d = v.s().city(cid)->district(holySite, true);
        if (d && d->pos != u->pos && v.s().grid.distance(u->pos, d->pos) < bestDist) {
            bestDist = v.s().grid.distance(u->pos, d->pos);
            best = d->pos;
        }
    }
    if (!best || g.submit(Command::move(v.me, id, *best, true)) != CommandError::Ok)
        g.submit(Command::setActivity(v.me, id, Activity::Sleep));
}

void religiousUnit(View& v, UnitId id) {
    Game& g = v.game;
    const Unit* u = v.s().unit(id);
    for (int cls = static_cast<int>(BeliefClass::Follower); cls < kNumBeliefClasses; ++cls) {
        const TypeIndex b = firstBelief(v, static_cast<BeliefClass>(cls));
        if (b != kNone && g.canEvangelize(id, b)) {
            g.submit(Command::evangelizeBelief(v.me, id, b));
            return;
        }
    }
    const CityId here = v.s().plot(u->pos).city;
    if (here != kNoCity && g.cityMajorityReligion(*v.s().city(here)) != u->religion && g.canSpreadReligion(id)) {
        g.submit(Command::spreadReligion(v.me, id));
        return;
    }
    // The nearest city not following our religion, ours first; walk next to its center.
    std::optional<Hex> best;
    int bestScore = INT_MAX;
    for (const City& c : v.s().cities) {
        if (g.cityMajorityReligion(c) == u->religion || v.game.visibility(v.me, c.pos) == Visibility::Unrevealed) continue;
        const int score = v.s().grid.distance(u->pos, c.pos) + (c.owner == v.me ? 0 : 6);
        if (score < bestScore) {
            bestScore = score;
            best = c.pos;
        }
    }
    if (best && approach(v, id, *best, false)) return;
    g.submit(Command::setActivity(v.me, id, Activity::Skip));
}

void buyReligion(View& v) {
    Game& g = v.game;
    const Player& p = v.s().players[at(v.me)];
    if (p.religion < 0) return;
    int missionaries = 0;
    for (const Unit& u : v.s().units) missionaries += u.owner == v.me && u.religion >= 0;
    for (CityId cid : v.cities) {
        const City& c = *v.s().city(cid);
        if (g.cityMajorityReligion(c) != p.religion) continue;
        for (size_t b = 0; b < v.r.buildings.size(); ++b) {  // a worship building first
            const ProductionItem item{ProductionKind::Building, static_cast<TypeIndex>(b)};
            const int cost = g.faithPurchaseCost(v.me, c, item);
            if (cost > 0 && v.s().players[at(v.me)].faith >= Fixed::fromInt(cost + 50)) g.submit(Command::purchaseWithFaith(v.me, cid, item));
        }
        if (missionaries >= 3) continue;
        for (const char* type : {"UNIT_APOSTLE", "UNIT_MISSIONARY"}) {
            const ProductionItem item{ProductionKind::Unit, v.r.unit(type)};
            const int cost = g.faithPurchaseCost(v.me, c, item);
            if (cost > 0 && v.s().players[at(v.me)].faith >= Fixed::fromInt(cost + 25) &&
                g.submit(Command::purchaseWithFaith(v.me, cid, item)) == CommandError::Ok) {
                ++missionaries;
                break;
            }
        }
    }
}

// Traders take the route paying most to their city (07); one with nowhere to go waits.
void trader(View& v, UnitId id) {
    Game& g = v.game;
    const Unit* u = v.s().unit(id);
    const City* origin = g.tradeOrigin(id);
    if (!origin) {
        // Walk home to the nearest of our cities first.
        std::optional<Hex> home;
        int bestDist = INT_MAX;
        for (CityId cid : v.cities) {
            const int d = v.s().grid.distance(u->pos, v.s().city(cid)->pos);
            if (d < bestDist) {
                bestDist = d;
                home = v.s().city(cid)->pos;
            }
        }
        if (home && approach(v, id, *home, false)) return;
        g.submit(Command::setActivity(v.me, id, Activity::Skip));
        return;
    }
    std::optional<CityId> best;
    int bestValue = INT_MIN;
    for (CityId dest : g.tradeDestinations(id)) {
        const int value = yieldValue(g.tradeRouteYields(*origin, *v.s().city(dest)));
        if (value > bestValue) {
            bestValue = value;
            best = dest;
        }
    }
    if (best && g.submit(Command::startTradeRoute(v.me, id, *best)) == CommandError::Ok) return;
    g.submit(Command::setActivity(v.me, id, Activity::Skip));
}

// Envoys (08): toward a city-state where we are close to the next tier or to suzerainty,
// then the nearest one we have met.
void envoys(View& v) {
    Game& g = v.game;
    for (int guard = 0; guard < 8 && v.s().players[at(v.me)].envoyTokens > 0; ++guard) {
        PlayerId best = kNoPlayer;
        int bestScore = INT_MIN;
        for (const Player& cs : v.s().players) {
            if (!g.canSendEnvoy(v.me, cs.id)) continue;
            const int mine = g.envoysAt(v.me, cs.id);
            const PlayerId suz = g.suzerainOf(cs.id);
            int score = mine == 0 || mine == 2 || mine == 5 ? 30 : 10;  // the next tier or suzerainty
            if (suz != kNoPlayer && suz != v.me) score -= 5;
            score -= mine > 6 ? 40 : 0;
            if (score > bestScore) {
                bestScore = score;
                best = cs.id;
            }
        }
        if (best == kNoPlayer || g.submit(Command::sendEnvoy(v.me, best)) != CommandError::Ok) break;
    }
}

// Buys a great person when the price is a small part of the treasury.
void patronage(View& v) {
    Game& g = v.game;
    if (g.isCityState(v.me)) return;
    for (size_t c = 0; c < v.r.greatPersonClasses.size(); ++c) {
        const int cost = g.patronageCost(v.me, static_cast<TypeIndex>(c), false);
        if (cost > 0 && v.s().players[at(v.me)].gold >= Fixed::fromInt(cost * 3))
            g.submit(Command::patronizeGreatPerson(v.me, static_cast<TypeIndex>(c), false));
    }
}

void cityActions(View& v) {
    for (CityId cid : v.cities) {
        const City* c = v.s().city(cid);
        for (const Hex& h : v.s().grid.within(c->pos, 2)) {
            if (v.game.canCityStrike(cid, h) && v.game.submit(Command::cityStrike(v.me, cid, h)) == CommandError::Ok) break;
        }
    }
}

}  // namespace

int militaryStrength(const Game& game, PlayerId player) {
    int total = 0;
    for (const Unit& u : game.state().units) {
        const UnitType& t = game.rules().units[at(u.type)];
        if (u.owner == player && isArmy(t)) total += power(t) * u.hp / 100;
    }
    return total;
}

PaceSample measurePace(const Game& game) {
    const GameState& s = game.state();
    PaceSample out;
    out.turn = s.turn;
    int majors = 0;
    for (const Player& p : s.players) {
        if (!p.alive || p.barbarian || p.freeCity || p.cityState != kNone) continue;
        ++majors;
        out.techs += std::count(p.techs.done.begin(), p.techs.done.end(), 1);
        out.civics += std::count(p.civics.done.begin(), p.civics.done.end(), 1);
        out.era += game.playerEra(p.id);
        out.science += game.sciencePerTurn(p.id).toInt();
        out.culture += game.culturePerTurn(p.id).toInt();
        out.gold += p.gold.toInt();
        for (const City& c : s.cities) {
            if (c.owner != p.id) continue;
            ++out.cities;
            out.population += c.population;
            out.production += game.cityReport(c.id).yields[yi(YieldType::Production)].toInt();
        }
    }
    if (majors > 0) {
        for (int64_t* f : {&out.cities, &out.population, &out.techs, &out.civics, &out.era, &out.science, &out.culture, &out.production, &out.gold})
            *f = *f * 100 / majors;
    }
    return out;
}

int settleScore(const Game& game, PlayerId player, Hex plot) {
    const GameState& s = game.state();
    const Rules& r = game.rules();
    if (!game.canFoundCityAt(player, plot)) return -1;
    for (const Camp& camp : s.camps) {
        if (s.grid.distance(camp.pos, plot) <= 3) return -1;
    }
    int score = 0;
    bool fresh = isRiverAdjacent(s, plot), coastal = false;
    for (const Hex& h : s.grid.within(plot, 3)) {
        const Plot& p = s.plot(h);
        const int ring = s.grid.distance(plot, h);
        const TerrainType& t = r.terrains[at(p.terrain)];
        if (ring == 1) {
            if (p.feature != kNone && r.features[at(p.feature)].freshWater) fresh = true;
            if (t.shallowWater) coastal = true;
        }
        if (p.owner != kNoPlayer && p.owner != player) continue;
        Yields y = t.yields;
        if (p.feature != kNone) {
            for (size_t i = 0; i < kNumYields; ++i) y[i] += r.features[at(p.feature)].yields[i];
        }
        int value = 0;
        if (p.resource != kNone && game.resourceVisible(player, h)) {
            const ResourceType& res = r.resources[at(p.resource)];
            for (size_t i = 0; i < kNumYields; ++i) y[i] += res.yields[i];
            value += kNewResource + (res.cls == ResourceClass::Bonus ? 0 : 2);
        }
        // Food and production weigh double (StandardSettlePlot per-yield weights).
        Fixed v = y[yi(YieldType::Food)] * 2 + y[yi(YieldType::Production)] * 2;
        for (YieldType o : {YieldType::Gold, YieldType::Science, YieldType::Culture, YieldType::Faith}) v += y[yi(o)];
        value += static_cast<int>(v.round());
        score += value * (ring <= 1 ? 3 : ring == 2 ? 2 : 1);
    }
    if (r.terrains[at(s.plot(plot).terrain)].relief == Relief::Hills) score += 6;
    if (fresh) score += kFreshWater;
    else if (coastal) score += kCoastal;
    const int nearest = distanceToCity(s, player, plot);
    if (nearest != INT_MAX && nearest > 4) score -= kPerTileFromCity * (nearest - 4);
    for (const City& c : s.cities) {
        if (c.owner != player && s.grid.distance(c.pos, plot) <= 5) score -= 20;
    }
    return std::max(0, score);
}

// Crowns a successor (leader doc §5): the dynasty's heir, else the most seasoned unit,
// else a regent. A captured leader is given up at once (no ransom until deals exist).
void succession(Game& game, PlayerId me) {
    if (game.state().players[at(me)].captor != kNoPlayer) game.submit(Command::abandonLeader(me));
    if (!game.state().players[at(me)].successionPending) return;
    if (game.submit(Command::chooseSuccessor(me, Succession::Heir)) == CommandError::Ok) return;
    UnitId best = kNoUnit;
    int bestLevel = 0;
    for (UnitId id : game.successorUnits(me)) {
        const Unit* u = game.state().unit(id);
        if (u->level() > bestLevel) {
            bestLevel = u->level();
            best = id;
        }
    }
    if (best != kNoUnit && game.submit(Command::chooseSuccessor(me, Succession::Unit, best)) == CommandError::Ok) return;
    game.submit(Command::chooseSuccessor(me, Succession::Regent));
}

// Idle assassins go after the leader of a civ we are at war with, the most exposed first.
void sendAssassins(Game& game, PlayerId me) {
    std::vector<int32_t> idle;
    for (const Agent& a : game.state().agents) {
        if (a.owner == me && !a.spy && a.target == kNoPlayer) idle.push_back(a.id);
    }
    if (idle.empty()) return;
    PlayerId best = kNoPlayer;
    int bestScore = INT_MIN;
    for (const Player& p : game.state().players) {
        if (!p.alive || p.barbarian || !game.atWar(me, p.id)) continue;
        const Unit* l = game.leaderOf(p.id);
        if (!l) continue;
        const int score = (game.leaderExposed(*l) ? 1000 : 0) - game.leaderDefenseVsAssassin(*l);
        if (score > bestScore) {
            bestScore = score;
            best = p.id;
        }
    }
    if (best == kNoPlayer) return;
    for (int32_t id : idle) game.submit(Command::sendAssassin(me, id, best));
}

void playTurn(Game& game) {
    // A live battle against a human is waiting; the turn resumes once it is settled.
    if (game.gameOver() || game.battlePending()) return;
    succession(game, game.state().currentPlayer);
    sendAssassins(game, game.state().currentPlayer);
    View v(game, game.state().currentPlayer);
    const bool cityState = game.isCityState(v.me);
    survey(v);
    if (!cityState) diplomacy(v);  // city-states never start wars (08)
    if (!cityState) deals(v);
    if (!cityState) governors(v);
    if (!cityState) spies(v);
    research(v);
    cityActions(v);
    // Promotions as soon as they are earned (the first offered; a planner can come later).
    for (const Unit& u : game.state().units) {
        if (u.owner != v.me) continue;
        std::vector<TypeIndex> promos = game.availablePromotions(u.id);
        if (!promos.empty()) game.submit(Command::promote(v.me, u.id, promos.front()));
    }
    attacks(v);
    std::vector<UnitId> civilians;
    for (const Unit& u : game.state().units) {
        const UnitLayer layer = v.r.units[at(u.type)].layer;
        if (u.owner == v.me && ((layer != UnitLayer::Military && layer != UnitLayer::Leader) || u.religion >= 0)) civilians.push_back(u.id);
    }
    for (UnitId id : civilians) {
        const Unit* u = game.state().unit(id);
        if (!u) continue;
        const UnitType& t = v.r.units[at(u->type)];
        if (v.r.units[at(u->type)].foundReligion) prophet(v, id);
        else if (u->religion >= 0) religiousUnit(v, id);
        else if (u->greatPerson != kNone) greatPerson(v, id);
        else if (t.id == "UNIT_TRADER") trader(v, id);
        else if (t.foundCity) settle(v, id);
        else if (u->charges > 0) build(v, id);
        else if (!u->moveTarget) game.submit(Command::setActivity(v.me, id, Activity::Skip));
    }
    survey(v);
    military(v);
    attacks(v);  // units that moved into reach
    leader(v);
    production(v);
    purchases(v);
    patronage(v);
    envoys(v);
    pantheon(v);
    buyReligion(v);
    for (UnitId id : game.unitsNeedingOrders(v.me)) game.submit(Command::setActivity(v.me, id, Activity::Skip));
    // Captured cities are kept (never razed).
    if (game.submit(Command::endTurn(v.me)) == CommandError::Ok) return;
    // Something still blocks the turn: fill whatever is missing with the first option.
    for (CityId cid : game.citiesNeedingProduction(v.me)) {
        std::vector<ProductionItem> items = game.buildableItems(cid);
        if (!items.empty()) game.submit(Command::setProduction(v.me, cid, items.front()));
    }
    const Player& pl = game.state().players[at(v.me)];
    if (pl.techs.current == kNone && !game.availableTechs(v.me).empty())
        game.submit(Command::chooseResearch(v.me, game.availableTechs(v.me).front()));
    if (pl.civics.current == kNone && !game.availableCivics(v.me).empty())
        game.submit(Command::chooseCivic(v.me, game.availableCivics(v.me).front()));
    for (UnitId id : game.unitsNeedingOrders(v.me)) game.submit(Command::setActivity(v.me, id, Activity::Skip));
    game.submit(Command::endTurn(v.me));
}

}  // namespace sov::ai
