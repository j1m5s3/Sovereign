// Eras, ages and tourism (09-civs-eras-victory-climate.md, Era score and Ages [R&F];
// 07-economy-trade-great-people.md, Tourism and Culture Victory). Historic moments earn era
// score; when the world moves to its next era each civ's score against its thresholds sets a
// Dark, Normal, Golden or Heroic Age. Tourism from Great Works, wonders and Holy Cities draws
// visiting tourists; the culture victory goes to a civ whose visitors outnumber every other
// civ's domestic tourists.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }
int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }
}  // namespace

void Game::awardMoment(PlayerId pid, const char* id) {
    Player& p = state_.players[at(pid)];
    const TypeIndex m = rules_->moment(id);
    if (!isMajor(p) || m == kNone) return;
    const MomentType& mt = rules_->moments[at(m)];
    if (mt.obsoleteEra >= 0 && state_.gameEra >= mt.obsoleteEra) return;
    if (p.momentEras.size() < rules_->moments.size()) p.momentEras.resize(rules_->moments.size(), 0);
    p.momentEras[at(m)] = static_cast<int8_t>(state_.gameEra + 1);
    p.eraScore += mt.eraScore;
    pushEvent(EventKind::HistoricMoment, pid, kNoPlayer, m);
}

// Dedications [R&F] (09: Dedications; data: CommemorationTypes' era windows). At each new era a civ chooses
// one (three in a Heroic Age) among those whose window holds the world's new era. Outside a Golden Age a
// dedication earns era score for its deeds (dedicationScore); in one, its bonus applies (goldenDedication).
std::vector<TypeIndex> Game::availableDedications(PlayerId player) const {
    std::vector<TypeIndex> out;
    const Player& p = state_.players[at(player)];
    if (!isMajor(p) || p.dedicationsPending <= 0) return out;
    for (size_t d = 0; d < rules_->dedications.size(); ++d) {
        const DedicationType& dt = rules_->dedications[d];
        if (state_.gameEra < dt.eraMin || (dt.eraMax >= 0 && state_.gameEra > dt.eraMax)) continue;
        if (std::find(p.dedications.begin(), p.dedications.end(), static_cast<TypeIndex>(d)) != p.dedications.end()) continue;
        out.push_back(static_cast<TypeIndex>(d));
    }
    return out;
}

CommandError Game::dedicationProblem(PlayerId player, TypeIndex dedication) const {
    const std::vector<TypeIndex> open = availableDedications(player);
    return std::find(open.begin(), open.end(), dedication) != open.end() ? CommandError::Ok : CommandError::BadTarget;
}

void Game::chooseDedication(PlayerId player, TypeIndex dedication) {
    Player& p = state_.players[at(player)];
    p.dedications.push_back(dedication);
    --p.dedicationsPending;
    // Automaton Warfare in a Golden Age: one Giant Death Robot in the capital.
    if (goldenDedication(player, "DEDICATION_AUTOMATON_WARFARE")) {
        const TypeIndex gdr = rules_->unit("UNIT_GIANT_DEATH_ROBOT");
        for (const City& c : state_.cities) {
            if (c.owner != player || !c.capital || gdr == kNone) continue;
            if (auto spot = unitSpawnPlot(c, gdr)) spawnUnit(gdr, player, *spot);
            break;
        }
    }
}

bool Game::dedicated(PlayerId player, const char* id) const {
    if (player < 0 || at(player) >= state_.players.size()) return false;
    const TypeIndex d = rules_->dedication(id);
    const std::vector<TypeIndex>& mine = state_.players[at(player)].dedications;
    return d != kNone && std::find(mine.begin(), mine.end(), d) != mine.end();
}

bool Game::goldenDedication(PlayerId player, const char* id) const {
    if (!dedicated(player, id)) return false;
    const Age a = state_.players[at(player)].age;
    return a == Age::Golden || a == Age::Heroic;
}

void Game::dedicationScore(PlayerId player, const char* id, int amount) {
    if (!dedicated(player, id) || goldenDedication(player, id)) return;
    state_.players[at(player)].eraScore += amount;
}

void Game::awardFirst(PlayerId pid, const char* worldId, const char* ownId, int key) {
    const Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return;
    // A world's first goes to the first civ for each key (an era, a government tier...); later
    // civs earn the ordinary moment once per key.
    const TypeIndex w = rules_->moment(worldId);
    if (state_.worldMoments.size() < rules_->moments.size()) state_.worldMoments.resize(rules_->moments.size(), 0);
    if (w != kNone && state_.worldMoments[at(w)] < key + 1) {
        state_.worldMoments[at(w)] = static_cast<int8_t>(key + 1);
        awardMoment(pid, worldId);
        return;
    }
    const TypeIndex o = ownId ? rules_->moment(ownId) : kNone;
    if (o == kNone) return;
    if (at(o) < p.momentEras.size() && p.momentEras[at(o)] >= key + 1) return;
    awardMoment(pid, ownId);
    state_.players[at(pid)].momentEras[at(o)] = static_cast<int8_t>(key + 1);
}

std::pair<int, int> Game::ageThresholds(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    int shift = rules_->globalInt("THRESHOLD_SHIFT_PER_PAST_GOLDEN_AGE") * p.pastGoldenAges +
                rules_->globalInt("THRESHOLD_SHIFT_PER_PAST_DARK_AGE") * p.pastDarkAges;
    shift += rules_->eras[at(static_cast<TypeIndex>(std::clamp(state_.gameEra, 0, static_cast<int>(rules_->eras.size()) - 1)))].eraScoreShift;
    for (const City& c : state_.cities) shift += c.owner == pid ? rules_->globalInt("THRESHOLD_SHIFT_PER_CITY") : 0;
    shift += rules_->globalInt("ERA_SCORE_THRESHOLD_ADJUST");  // Sovereign: fewer moments are modelled yet
    return {rules_->globalInt("DARK_AGE_SCORE_BASE_THRESHOLD") + shift, rules_->globalInt("GOLDEN_AGE_SCORE_BASE_THRESHOLD") + shift};
}

void Game::processEras() {
    if (state_.gameEra + 1 >= static_cast<int>(rules_->eras.size())) return;
    const EraType& era = rules_->eras[at(static_cast<TypeIndex>(state_.gameEra))];
    const int speed = speedPercent(state_, *rules_);
    const int turns = state_.turn - state_.gameEraStart;
    const int minTurns = era.minTurns * speed / 100, maxTurns = era.maxTurns * speed / 100;
    int majors = 0, ahead = 0;
    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        ++majors;
        ahead += playerEra(p.id) > state_.gameEra ? 1 : 0;
    }
    // Between the era's minimum and maximum length, the world moves on once half the civs have
    // (the trigger between those bounds is engine; Sovereign's reading).
    const bool advance = majors > 0 && ((minTurns > 0 && turns >= minTurns && ahead * 2 >= majors) || (maxTurns > 0 && turns >= maxTurns));
    if (!advance) return;
    for (Player& p : state_.players) {
        if (!isMajor(p)) continue;
        const auto [dark, golden] = ageThresholds(p.id);
        const Age before = p.age;
        if (p.eraScore < dark) {
            p.age = Age::Dark;
            ++p.pastDarkAges;
        } else if (p.eraScore >= golden) {
            p.age = before == Age::Dark ? Age::Heroic : Age::Golden;
            ++p.pastGoldenAges;
        } else {
            p.age = Age::Normal;
        }
        p.eraScore = 0;
        // Dedications for the new era: one, or three in a Heroic Age (COMMEMORATE_*).
        p.dedications.clear();
        p.dedicationsPending = rules_->globalInt(p.age == Age::Heroic ? "COMMEMORATE_OPTIONS_MAX" : "COMMEMORATE_BASE_CHOICES_ALLOWED");
        pushEvent(EventKind::NewAge, p.id, kNoPlayer, static_cast<int>(p.age));
    }
    ++state_.gameEra;
    state_.gameEraStart = state_.turn;
    for (Player& p : state_.players) p.killsThisEra = 0;
    // Difficulty: AI civs at Immortal and Deity get free Eurekas and Inspirations in the new era's trees.
    const int free = difficulty().aiFreeBoosts;
    for (Player& p : state_.players) {
        if (free <= 0 || !difficultyAi(p.id)) continue;
        for (int civic = 0; civic < 2; ++civic) {
            const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
            TreeProgress& t = civic ? p.civics : p.techs;
            int given = 0;
            for (size_t i = 0; i < nodes.size() && given < free; ++i) {
                if (nodes[i].era != state_.gameEra || i >= t.done.size() || t.done[i] || t.boosted[i]) continue;
                const int cost = civic ? civicCost(static_cast<TypeIndex>(i)) : techCost(static_cast<TypeIndex>(i));
                const int pct = nodes[i].boost.percent > 0 ? nodes[i].boost.percent : 40;
                t.boosted[i] = 1;
                t.progress[i] += Fixed::fromInt(cost) * pct / 100;
                ++given;
            }
        }
    }
}

int Game::ageLoyalty(const City& city) const {
    // Golden Ages raise and Dark Ages lower citizen loyalty pressure by 0.5 per citizen
    // (GOLDEN_AGE_CITY_IDENTITY / DARK_AGE_CITY_IDENTITY), applied here to the city's own loyalty.
    const Age age = state_.players[at(city.owner)].age;
    if (age == Age::Golden || age == Age::Heroic) return city.population / 2;
    if (age == Age::Dark) return -(city.population / 2);
    return 0;
}

int Game::tourismPerTurn(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return 0;
    int total = 0;
    const int era = playerEra(pid);
    const bool wish = goldenDedication(pid, "DEDICATION_WISH_YOU_WERE_HERE");
    total += improvementTourism(pid) + parkTourism(pid);  // 07: resorts, improvements after Flight, National Parks
    for (const City& c : state_.cities) {
        if (c.owner != pid) continue;
        const int before = total;
        const int curator = cityGovernorHas(c, "GOVERNOR_PROMOTION_CURATOR") ? 2 : 1;  // Pingala
        for (const GreatWork& w : c.greatWorks) {
            const int pct = themed(c, w.building) ? 100 + rules_->buildings[at(w.building)].theming->tourismPercent : 100;  // 07: Theming
            total += rules_->greatWorkTypes[at(w.type)].tourism * curator * pct / 100;
        }
        for (TypeIndex b : c.buildings) {
            const BuildingType& bt = rules_->buildings[at(b)];
            if (!bt.wonder) continue;
            const int wonderEra = bt.unlock.none() ? 0 : (bt.unlock.civic ? rules_->civics : rules_->techs)[at(bt.unlock.index)].era;
            total += rules_->globalInt("TOURISM_BASE_FROM_WONDER") + rules_->globalInt("TOURISM_ADVANCED_ERA_WONDER") * std::max(0, era - wonderEra);
        }
        if (p.religion >= 0 && state_.religions[static_cast<size_t>(p.religion)].holyCity == c.id) total += rules_->globalInt("TOURISM_FROM_HOLY_CITY");
        // Wish You Were Here (Golden Age): +50% tourism from cities with an established governor.
        PlayerId holder = kNoPlayer;
        if (wish && establishedGovernor(c, &holder) && holder == pid) total += (total - before) / 2;
    }
    return total;
}

void Game::processTourism(PlayerId pid) {
    Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return;
    // Meeting a civ (seeing one of its cities or units) is a moment (09: Met New Civilization).
    if (p.met.size() < state_.players.size()) p.met.resize(state_.players.size(), 0);
    for (const Player& x : state_.players) {
        if (x.id == pid || !isMajor(x) || state_.players[at(pid)].met[at(x.id)]) continue;
        bool seen = false;
        for (const City& c : state_.cities) seen = seen || (c.owner == x.id && visibility(pid, c.pos) == Visibility::Visible);
        for (const Unit& u : state_.units) seen = seen || (u.owner == x.id && visibility(pid, u.pos) == Visibility::Visible);
        if (!seen) continue;
        state_.players[at(pid)].met[at(x.id)] = 1;
        awardMoment(pid, "MOMENT_MET_NEW_CIVILIZATION");
    }
    if (p.tourismTo.size() < state_.players.size()) p.tourismTo.resize(state_.players.size(), 0);
    const int t = tourismPerTurn(pid);
    if (t <= 0) return;
    for (const Player& x : state_.players) {
        if (x.id == pid || !isMajor(x)) continue;
        // +25% toward a civ we run a trade route to (TOURISM_TRADE_ROUTE_BONUS).
        const bool route = std::any_of(state_.tradeRoutes.begin(), state_.tradeRoutes.end(), [&](const TradeRoute& r) {
            const City* d = state_.city(r.destination);
            return r.owner == pid && d && d->owner == x.id;
        });
        // +25% toward a civ that opens its borders to us (08: Open Borders).
        const int borders = grantsOpenBorders(x.id, pid) ? 25 : 0;
        p.tourismTo[at(x.id)] += t * (100 + (route ? rules_->globalInt("TOURISM_TRADE_ROUTE_BONUS") : 0) + borders) / 100;
    }
}

int Game::visitingTourists(PlayerId pid, PlayerId from) const {
    const Player& p = state_.players[at(pid)];
    const int majors = std::max(1, state_.majorsAtStart);
    const int per = rules_->globalInt("TOURISM_TOURISM_TO_MOVE_CITIZEN") * majors;
    return at(from) < p.tourismTo.size() ? p.tourismTo[at(from)] / per : 0;
}

int Game::visitingTourists(PlayerId pid) const {
    int total = 0;
    for (const Player& x : state_.players) {
        if (x.id != pid && isMajor(x)) total += visitingTourists(pid, x.id);
    }
    return total;
}

int Game::domesticTourists(PlayerId pid) const {
    int n = static_cast<int>((state_.players[at(pid)].lifetimeCulture / rules_->globalInt("TOURISM_CULTURE_PER_CITIZEN")).toInt());
    for (const Player& x : state_.players) {
        if (x.id != pid && isMajor(x)) n -= visitingTourists(x.id, pid);  // our people visiting them
    }
    return std::max(0, n);
}

PlayerId Game::cultureVictor() const {
    // Sovereign floor: at least kMinTouristsPerRival visiting tourists per rival major as well. With few
    // civs one early tourist (a tribal village's relic) otherwise wins, as each tourist both counts for
    // the host and comes off the rival's domestic tourists (07: Tourism).
    constexpr int kMinTouristsPerRival = 5;
    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        const int visitors = visitingTourists(p.id);
        if (visitors <= 0) continue;
        bool all = true;
        int rivals = 0;
        for (const Player& x : state_.players) {
            if (x.id == p.id || !isMajor(x)) continue;
            ++rivals;
            if (visitors <= domesticTourists(x.id)) all = false;
        }
        if (all && rivals > 0 && visitors >= kMinTouristsPerRival * rivals) return p.id;
    }
    return kNoPlayer;
}

}  // namespace sov
