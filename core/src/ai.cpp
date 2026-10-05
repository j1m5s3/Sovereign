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
constexpr int kMaxCities = 12;
constexpr int kWarRatioPercent = 130;   // own strength vs target's to declare war
constexpr int kPeaceRatioPercent = 80;  // below this, offer peace
constexpr int kWarWeariness = 50;       // turns of war before peace is offered anyway
constexpr int kNeighbourRange = 14;     // a target's city must be this close to one of ours
constexpr int kThreatRange = 4;
constexpr int kHealBelow = 40;

size_t at(TypeIndex i) { return static_cast<size_t>(i); }
size_t yi(YieldType y) { return static_cast<size_t>(y); }

// Yield weights per point (DefaultYieldBias favours production and gold; food
// matters most to a young empire).
int yieldValue(const Yields& y) {
    static const int w[kNumYields] = {3, 3, 2, 3, 2, 1};
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
    for (const Player& p : s.players) {
        if (p.alive && !p.barbarian && v.hostile(p.id)) v.enemies.push_back(p.id);
    }
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
        bool near = false;
        for (const City& c : s.cities) {
            if (c.owner != p.id || v.game.visibility(v.me, c.pos) == Visibility::Unrevealed) continue;
            if (distanceToCity(s, v.me, c.pos) <= kNeighbourRange) near = true;
        }
        const int theirs = militaryStrength(v.game, p.id);
        if (near && mine * 100 >= theirs * kWarRatioPercent && theirs < pickStrength) {
            pick = p.id;
            pickStrength = theirs;
        }
    }
    if (pick != kNoPlayer && v.game.submit(Command::declareWar(v.me, pick)) == CommandError::Ok) {
        v.target = pick;
        survey(v);
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
        if (is(t.unlock)) value += isArmy(t) && power(t) > bestPower ? 4 + (v.enemies.empty() ? 0 : 4) : 1;
    }
    for (const BuildingType& b : r.buildings) {
        if (is(b.unlock)) value += 3 + b.outerDefenseHp / 50;
    }
    for (const DistrictType& d : r.districts) {
        if (is(d.unlock)) value += 4;
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
            auto path = v.game.findPath(settler, site.pos);
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
    if (site && v.game.submit(Command::move(v.me, id, site->pos)) == CommandError::Ok) {
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
        if (p.owner != v.me || p.city == kNoCity || p.improvement != kNone || s.districtAt(h) || s.cityAt(h)) return -1;
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
    if (best && v.game.submit(Command::move(v.me, id, *best)) == CommandError::Ok) {
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
    if (onto) return u->pos == goal || v.game.submit(Command::move(v.me, id, goal)) == CommandError::Ok;
    if (v.s().grid.distance(u->pos, goal) <= 1) return true;
    std::vector<Hex> ring;
    for (const Hex& h : v.s().grid.within(goal, 1)) {
        if (h != goal) ring.push_back(h);
    }
    std::stable_sort(ring.begin(), ring.end(), [&](Hex a, Hex b) {
        return v.s().grid.distance(u->pos, a) < v.s().grid.distance(u->pos, b);
    });
    for (const Hex& h : ring) {
        if (v.game.submit(Command::move(v.me, id, h)) == CommandError::Ok) return true;
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
        if (v.game.submit(Command::move(v.me, id, frontier[i].second)) == CommandError::Ok) return true;
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
        if (!l->moveTarget || *l->moveTarget != capital->pos) v.game.submit(Command::move(v.me, l->id, capital->pos));
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
    const Unit* now = s.unit(id);
    if (now->activity != Activity::Sleep && now->movesLeft > Fixed()) v.game.submit(Command::setActivity(v.me, id, Activity::Sleep));
}

// --- production --------------------------------------------------------------------
int desiredArmy(const View& v) {
    const int n = static_cast<int>(v.cities.size());
    int want = n + 1 + n / 2;
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
        const bool wantArmy = v.military < desiredArmy(v);
        const bool wantSettler = static_cast<int>(v.cities.size()) + v.settlers < kMaxCities && v.settlers < 2 &&
                                 c.population >= 2 && !threatened && v.enemies.empty() && s.turn < 200;
        const bool wantBuilder = v.builders < (static_cast<int>(v.cities.size()) + 1) * 2 / 3 + 1 - (s.turn < 10 ? 1 : 0);
        const Fixed popRoom = rep.housing - Fixed::fromInt(c.population);
        // Assassins for wars against civs with a leader (leader doc §6), one in training at a time.
        bool assassinQueued = false;
        for (CityId other : v.cities) {
            const City& oc = *s.city(other);
            assassinQueued |= !oc.queue.empty() && oc.queue.front().kind == ProductionKind::Unit && v.r.units[at(oc.queue.front().type)].agent;
        }
        const bool wantAssassin = !v.enemies.empty() && !assassinQueued && g.agentsOf(v.me) < g.agentCapacity(v.me);

        std::optional<ProductionItem> best;
        Hex bestAt{};
        int64_t bestScore = INT64_MIN;
        for (const ProductionItem& it : items) {
            int value = 0;
            Hex where{};
            switch (it.kind) {
                case ProductionKind::Unit: {
                    const UnitType& t = v.r.units[at(it.type)];
                    if (t.agent) value = wantAssassin ? 250 : 0;
                    else if (t.foundCity) value = wantSettler ? 400 : 0;
                    else if (t.buildCharges > 0) value = wantBuilder ? 160 : 0;
                    else if (soldier && it == *soldier) value = (needGuard || threatened) ? 700 : wantArmy ? 220 : 0;
                    break;
                }
                case ProductionKind::Building: {
                    const BuildingType& b = v.r.buildings[at(it.type)];
                    value = 15 + yieldValue(b.yields) * 10;
                    if (popRoom <= Fixed::fromInt(1)) value += static_cast<int>((b.housing * 30).round());
                    if (rep.amenities < rep.amenitiesNeeded) value += b.amenities * 25;
                    if (b.outerDefenseHp > 0) value += threatened ? 500 : v.enemies.empty() ? 0 : 60;
                    break;
                }
                case ProductionKind::District: {
                    const DistrictType& d = v.r.districts[at(it.type)];
                    if (const CityDistrict* placed = c.district(it.type, false)) {
                        value = 60 + yieldValue(g.districtAdjacency(v.me, it.type, placed->pos)) * 10;
                        where = placed->pos;
                    } else {
                        where = districtSpot(v, cid, it.type);
                        value = 40 + yieldValue(g.districtAdjacency(v.me, it.type, where)) * 10;
                        // At war, the first Encampment also opens assassins (leader doc §6).
                        if (d.id == "DISTRICT_ENCAMPMENT") value = v.enemies.empty() ? 10 : g.agentCapacity(v.me) == 0 ? 120 : 40;
                    }
                    break;
                }
            }
            if (value <= 0) continue;
            const int64_t score = static_cast<int64_t>(value) * 1000 / (g.productionCost(v.me, it) + 40);
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
    const int reserve = 150 + 30 * static_cast<int>(v.cities.size());
    for (CityId cid : v.cities) {
        const City& c = *v.s().city(cid);
        if (c.queue.empty()) continue;
        const int cost = g.purchaseCost(v.me, c.queue.front());
        if (cost > 0 && v.s().players[at(v.me)].gold >= Fixed::fromInt(cost + reserve)) {
            g.submit(Command::purchase(v.me, cid, c.queue.front()));
        }
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
        if (a.owner == me && a.target == kNoPlayer) idle.push_back(a.id);
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
    if (game.gameOver()) return;
    succession(game, game.state().currentPlayer);
    sendAssassins(game, game.state().currentPlayer);
    View v(game, game.state().currentPlayer);
    survey(v);
    diplomacy(v);
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
        if (u.owner == v.me && layer != UnitLayer::Military && layer != UnitLayer::Leader) civilians.push_back(u.id);
    }
    for (UnitId id : civilians) {
        const Unit* u = game.state().unit(id);
        if (!u) continue;
        const UnitType& t = v.r.units[at(u->type)];
        if (t.foundCity) settle(v, id);
        else if (u->charges > 0) build(v, id);
        else if (!u->moveTarget) game.submit(Command::setActivity(v.me, id, Activity::Skip));
    }
    survey(v);
    military(v);
    attacks(v);  // units that moved into reach
    leader(v);
    production(v);
    purchases(v);
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
