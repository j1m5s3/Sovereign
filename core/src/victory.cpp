// Victories and score (09-civs-eras-victory-climate.md, Victory conditions;
// 00-overview.md, Score). Science, Culture, Religious and Diplomatic victories
// wait for their systems.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

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
    }
    return total;
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
        // one that never founded a city is skipped once it is out).
        for (const Player& p : state_.players) {
            if (p.barbarian || p.cityState != kNone || !p.alive) continue;
            bool all = true, any = false;
            for (const Player& q : state_.players) {
                if (q.barbarian || q.cityState != kNone || q.id == p.id) continue;
                const City* capital = nullptr;
                for (const City& c : state_.cities) {
                    if (c.originalCapital && c.originalOwner == q.id) capital = &c;
                }
                if (!capital) {
                    if (q.alive) all = false;
                    continue;
                }
                if (capital->owner != p.id) all = false;
                else any = true;
            }
            if (all && any) return win(p.id, Victory::Domination);
        }
    } else if (majors > 1 && alive == 1) {
        return win(last, Victory::LastStanding);  // VICTORY_DEFAULT
    }
    if (state_.setup.diplomaticVictory) {
        for (const Player& p : state_.players) {
            if (p.alive && p.diplomaticVictoryPoints >= rules_->globalInt("DIPLOMATIC_VICTORY_POINTS_REQUIRED")) return win(p.id, Victory::Diplomatic);
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
