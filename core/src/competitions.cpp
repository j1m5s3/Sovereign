// Scored competitions [GS] (08: Scored Competitions; data: world-congress-emergencies, Emergencies
// and competitions, rewards and score sources). A World Congress session calls one competition the
// world's era allows when none is running; it lasts 29 turns and every major civ takes part.
//   World's Fair (Industrial): great person points earned, Prophets aside.
//   World Games (Modern): Stadiums and Aquatics Centers held at the end.
//   Nobel Prize in Literature / Physics (Modern): Writers, Artists, Musicians / Scientists, Engineers,
//     Merchants recruited.
//   Nobel Peace Prize (Atomic): Diplomatic Favor gained.
//   Climate Accords (Atomic, or once the climate has warmed): the least CO2 emitted.
//   International Space Station (Information): 30 per space race project completed.
// First place takes the Diplomatic Victory points the data lists (World's Fair, World Games, Peace,
// Space Station 1; Climate Accords 2); the rest of the top half takes the high-tier reward and the
// others with any score the low-tier one (Sovereign's reading of the tiers). Carried rewards: the
// points, Diplomatic Favor (high tier 50, Climate Accords 100 high and 50 low), the World's Fair
// winner's +100 points toward every great person class, and the Nobel Prize in Physics' Eurekas.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

void Game::startCompetition() {
    for (const Competition& c : state_.competitions) {
        if (!c.settled) return;
    }
    const auto era = [&](const char* id) { const TypeIndex e = rules_->era(id); return e == kNone ? 99 : e; };
    std::vector<CompetitionKind> pool;
    if (state_.gameEra >= era("ERA_INDUSTRIAL")) pool.push_back(CompetitionKind::WorldsFair);
    if (state_.gameEra >= era("ERA_MODERN")) {
        pool.push_back(CompetitionKind::WorldGames);
        pool.push_back(CompetitionKind::NobelLiterature);
        pool.push_back(CompetitionKind::NobelPhysics);
    }
    if (state_.gameEra >= era("ERA_ATOMIC")) pool.push_back(CompetitionKind::NobelPeace);
    if (state_.gameEra >= era("ERA_ATOMIC") || state_.climatePhase > 0) pool.push_back(CompetitionKind::ClimateAccords);
    if (state_.gameEra >= era("ERA_INFORMATION")) pool.push_back(CompetitionKind::SpaceStation);
    // Not the one just held.
    if (!state_.competitions.empty() && pool.size() > 1) {
        const CompetitionKind last = state_.competitions.back().kind;
        pool.erase(std::remove(pool.begin(), pool.end(), last), pool.end());
    }
    if (pool.empty()) return;
    Competition c;
    c.kind = pool[state_.rng.get(RngStream::Gameplay).below(static_cast<uint32_t>(pool.size()))];
    c.endTurn = state_.turn + 29;
    c.scores.assign(state_.players.size(), 0);
    c.baseline.assign(state_.players.size(), 0);
    for (const Player& p : state_.players) {
        c.baseline[at(p.id)] = c.kind == CompetitionKind::NobelPeace ? p.favor : c.kind == CompetitionKind::ClimateAccords ? p.co2 : 0;
    }
    state_.competitions.push_back(std::move(c));
}

// Aid Request and Military Aid Request [GS] (08: Scored Competitions, Special sessions; data: their rewards
// and score sources). A disaster that costs a major civ population, or a war against a civ it holds at
// least WORLD_CONGRESS_REQUEST_FOR_MILITARY_AID_GRIEVANCES_MIN grievances against, calls a special session
// when no competition runs and WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS have passed since the last;
// for 30 turns the others may run the Send Aid project in their cities, each completion sending the civ
// 200 Gold and scoring 200 (a civ at war with it may not send aid).
bool Game::specialSessionDue() const {
    return state_.lastSpecialSession == 0 || state_.turn - state_.lastSpecialSession >= rules_->globalInt("WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS");
}

void Game::requestAid(PlayerId victim, bool military) {
    if (!isMajorCiv(victim) || !specialSessionDue()) return;
    for (const Competition& c : state_.competitions) {
        if (!c.settled) return;
    }
    state_.lastSpecialSession = state_.turn;
    Competition c;
    c.kind = military ? CompetitionKind::MilitaryAidRequest : CompetitionKind::AidRequest;
    c.endTurn = state_.turn + 30;
    c.scores.assign(state_.players.size(), 0);
    c.baseline.assign(state_.players.size(), 0);
    c.beneficiary = victim;
    state_.competitions.push_back(std::move(c));
}

const Competition* Game::runningAidRequest() const {
    for (const Competition& c : state_.competitions) {
        if (!c.settled && (c.kind == CompetitionKind::AidRequest || c.kind == CompetitionKind::MilitaryAidRequest)) return &c;
    }
    return nullptr;
}

void Game::checkMilitaryAid() {
    const int need = rules_->globalInt("WORLD_CONGRESS_REQUEST_FOR_MILITARY_AID_GRIEVANCES_MIN");
    for (const Player& p : state_.players) {
        if (!isMajorCiv(p.id) || !p.alive) continue;
        for (const Player& o : state_.players) {
            if (o.id != p.id && isMajorCiv(o.id) && atWar(p.id, o.id) && grievances(p.id, o.id) >= need) {
                requestAid(p.id, true);
                return;
            }
        }
    }
}

void Game::competitionScore(PlayerId player, CompetitionKind kind, int amount) {
    if (amount == 0 || player < 0 || !isMajorCiv(player)) return;
    for (Competition& c : state_.competitions) {
        if (c.settled || c.kind != kind) continue;
        if (c.scores.size() < state_.players.size()) c.scores.resize(state_.players.size(), 0);
        c.scores[at(player)] += amount;
    }
}

static_assert(static_cast<int>(CompetitionKind::WorldGames) == 1 && static_cast<int>(CompetitionKind::SpaceStation) == 6,
              "rules.cpp maps competition projects to these values");

int Game::competitionStanding(const Competition& c, PlayerId player) const {
    const Player& p = state_.players[at(player)];
    const int64_t base = at(player) < c.baseline.size() ? c.baseline[at(player)] : 0;
    switch (c.kind) {
        case CompetitionKind::WorldGames: {
            int n = 0;
            const TypeIndex stadium = rules_->building("BUILDING_STADIUM"), aquatics = rules_->building("BUILDING_AQUATICS_CENTER");
            for (const City& city : state_.cities) {
                if (city.owner != player) continue;
                n += (stadium != kNone && city.has(stadium) ? 1 : 0) + (aquatics != kNone && city.has(aquatics) ? 1 : 0);
            }
            return n + (at(player) < c.scores.size() ? c.scores[at(player)] : 0);  // plus Train Athletes
        }
        case CompetitionKind::NobelPeace: return static_cast<int>(std::max<int64_t>(0, p.favor - base));
        case CompetitionKind::ClimateAccords: return -static_cast<int>(std::min<int64_t>(1000000, std::max<int64_t>(0, p.co2 - base)));
        default: return at(player) < c.scores.size() ? c.scores[at(player)] : 0;
    }
}

void Game::processCompetitions() {
    for (Competition& c : state_.competitions) {
        if (c.settled || state_.turn < c.endTurn) continue;
        c.settled = true;
        std::vector<std::pair<int, PlayerId>> table;
        for (const Player& p : state_.players) {
            if (isMajorCiv(p.id) && p.alive) table.push_back({competitionStanding(c, p.id), p.id});
        }
        if (table.empty()) continue;
        for (auto& [score, pid] : table) {
            if (at(pid) < c.scores.size()) c.scores[at(pid)] = score;  // the final standings, kept for the record
        }
        // Highest first; ties to the lower player id.
        std::stable_sort(table.begin(), table.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        const bool climate = c.kind == CompetitionKind::ClimateAccords;
        // Only a civ that did something places (any score; for the Climate Accords every civ does).
        const auto counts = [&](int score) { return climate || score > 0; };
        const size_t topHalf = (table.size() + 1) / 2;
        for (size_t rank = 0; rank < table.size(); ++rank) {
            const auto [score, pid] = table[rank];
            if (!counts(score)) continue;
            Player& p = state_.players[at(pid)];
            if (rank == 0) {
                int dvp = 0;
                switch (c.kind) {
                    case CompetitionKind::WorldsFair:
                    case CompetitionKind::WorldGames:
                    case CompetitionKind::NobelPeace:
                    case CompetitionKind::SpaceStation: dvp = 1; break;
                    case CompetitionKind::ClimateAccords:
                    case CompetitionKind::AidRequest:
                    case CompetitionKind::MilitaryAidRequest: dvp = 2; break;
                    default: break;
                }
                p.diplomaticVictoryPoints += dvp;
                if (c.kind == CompetitionKind::WorldsFair) {
                    for (int& gpp : p.greatPersonPoints) gpp += 100;
                }
            }
            const bool high = rank < topHalf;
            const bool aid = c.kind == CompetitionKind::AidRequest || c.kind == CompetitionKind::MilitaryAidRequest;
            if (high && c.kind != CompetitionKind::NobelLiterature && c.kind != CompetitionKind::NobelPhysics) p.favor += climate || aid ? 100 : 50;
            if (!high && (climate || aid)) p.favor += 50;
            if (c.kind == CompetitionKind::NobelPhysics) {
                // A Eureka toward an Industrial-or-later tech it lacks.
                const TypeIndex industrial = rules_->era("ERA_INDUSTRIAL");
                for (size_t t = 0; t < rules_->techs.size(); ++t) {
                    if (p.techs.done[t] || p.techs.boosted[t] || rules_->techs[t].era < industrial) continue;
                    const int pct = rules_->techs[t].boost.percent > 0 ? rules_->techs[t].boost.percent : 40;
                    p.techs.boosted[t] = 1;
                    p.techs.progress[t] += Fixed::fromInt(techCost(static_cast<TypeIndex>(t))) * pct / 100;
                    break;
                }
            }
        }
    }
}

}  // namespace sov
