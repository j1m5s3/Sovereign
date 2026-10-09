// Victories and score (09-civs-eras-victory-climate.md, Victory conditions;
// 00-overview.md, Score). Science, Culture, Religious and Diplomatic victories
// wait for their systems.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

int Game::lightYearsRequired() const {
    // Short Reign's 100 turns leave no room for Standard's 50 light-years (player-retention: "scaled costs and victory
    // conditions"); the speed names its share.
    const TypeIndex speed = rules_->speed(state_.setup.speed);
    const int percent = speed == kNone ? 100 : rules_->speeds[static_cast<size_t>(speed)].scienceVictoryPercent;
    return std::max(1, rules_->globalInt(HotGlobal::ScienceVictoryPointsRequired) * percent / 100);
}

int Game::turnLimit() const {
    if (state_.setup.turnLimit > 0) return state_.setup.turnLimit;
    return rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].turns;
}

int Game::score(PlayerId player) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (p.barbarian || p.cityState != kNone) return 0;
    auto count = [](const std::vector<uint8_t>& done) { return static_cast<int>(std::count(done.begin(), done.end(), 1)); };
    int total = 3 * count(p.civics.done) + 2 * count(p.techs.done);
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        total += 5 + c.population;
        for (const CityDistrict& d : c.districts) total += d.complete ? 2 : 0;
        for (TypeIndex b : c.buildings) total += rules_->buildings[static_cast<size_t>(b)].wonder ? 15 : 0;  // wonders
    }
    // Great people and a founded religion (09: Score).
    for (int n : p.greatPeopleRecruited) total += 5 * n;
    if (p.religion >= 0) total += 5;
    return total + p.eraScoreTotal;  // era score, a point each (data: LINE_ITEM_ERA_SCORE)
}

void Game::checkVictory() {
    if (state_.winner != kNoPlayer) return;
    auto win = [&](PlayerId p, Victory v) {
        state_.winner = p;
        state_.victory = v;
    };
    int majors = 0, alive = 0;
    PlayerId last = kNoPlayer;
    for (const Player& p : state_.players) {
        if (p.barbarian || p.cityState != kNone) continue;
        ++majors;
        if (p.alive) {
            ++alive;
            last = p.id;
        }
    }
    if (state_.setup.dominationVictory) {
        // Domination: hold the original capital of every other major civ (alive or not;
        // one that never founded a city is skipped once it is out). Each civ's original capital is
        // looked up once: the last city in the list founded as its capital.
        std::vector<const City*> originalCapital(state_.players.size(), nullptr);  // by player id
        for (const City& c : state_.cities) {
            if (c.originalCapital && c.originalOwner >= 0 && static_cast<size_t>(c.originalOwner) < originalCapital.size())
                originalCapital[static_cast<size_t>(c.originalOwner)] = &c;
        }
        for (const Player& p : state_.players) {
            if (p.barbarian || p.cityState != kNone || !p.alive) continue;
            bool all = true, any = false;
            for (const Player& q : state_.players) {
                if (q.barbarian || q.cityState != kNone || q.id == p.id) continue;
                const City* capital = q.id >= 0 && static_cast<size_t>(q.id) < originalCapital.size() ? originalCapital[static_cast<size_t>(q.id)] : nullptr;
                if (!capital) {
                    if (q.alive) all = false;
                } else if (capital->owner != p.id) {
                    all = false;
                } else {
                    any = true;
                }
                if (!all) break;  // one civ's capital not held settles it
            }
            if (all && any) return win(p.id, Victory::Domination);
        }
    } else if (majors > 1 && alive == 1) {
        return win(last, Victory::LastStanding);  // VICTORY_DEFAULT
    }
    if (state_.setup.scienceVictory) {
        // The exoplanet expedition arrives (SCIENCE_VICTORY_POINTS_REQUIRED light-years).
        const PlayerId arrived = [&] {
            PlayerId best = kNoPlayer;
            const int required = lightYearsRequired();
            for (const Player& p : state_.players) {
                if (p.alive && p.lightYears >= required &&
                    (best == kNoPlayer || p.lightYears > state_.players[static_cast<size_t>(best)].lightYears))
                    best = p.id;
            }
            return best;
        }();
        if (arrived != kNoPlayer) return win(arrived, Victory::Science);
    }
    if (state_.setup.diplomaticVictory) {
        const int required = rules_->globalInt(HotGlobal::DiplomaticVictoryPointsRequired);
        for (const Player& p : state_.players) {
            if (p.alive && p.diplomaticVictoryPoints >= required) return win(p.id, Victory::Diplomatic);
        }
    }
    if (state_.setup.cultureVictory) {
        const PlayerId c = cultureVictor();
        if (c != kNoPlayer) return win(c, Victory::Culture);
    }
    if (state_.setup.religiousVictory) {
        const PlayerId r = religiousVictor();
        if (r != kNoPlayer) return win(r, Victory::Religious);
    }
    if (state_.setup.scoreVictory && state_.turn > turnLimit()) {
        // Highest score; ties go to the lowest player id (Civ's tie rule is unverified).
        PlayerId best = kNoPlayer;
        int bestScore = -1;
        for (const Player& p : state_.players) {
            if (p.barbarian || p.cityState != kNone || !p.alive) continue;
            const int s = score(p.id);
            if (s > bestScore) {
                bestScore = s;
                best = p.id;
            }
        }
        if (best != kNoPlayer) win(best, Victory::Score);
    }
}

}  // namespace sov
