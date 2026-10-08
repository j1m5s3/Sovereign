// The weekly challenge (player-retention §3): the week's setup and goal, and the check of a submitted
// game by replay (see challenge.h). The server that keeps the board runs this same check.
#include "sovereign/challenge.h"

#include "sovereign/game.h"
#include "sovereign/serialize.h"

namespace sov {

namespace {
constexpr int64_t kFirstMonday = 1767571200;  // 2026-01-05 00:00 UTC
constexpr int64_t kWeekSeconds = 7 * 24 * 3600;

uint64_t mix(uint64_t x) {  // splitmix64: the week's seed
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

bool sameSetup(const GameSetup& a, const GameSetup& b) {
    if (a.seed != b.seed || a.mapSize != b.mapSize || a.speed != b.speed || a.wrapX != b.wrapX || a.players.size() != b.players.size() ||
        a.barbarians != b.barbarians || a.tribalVillages != b.tribalVillages || a.dominationVictory != b.dominationVictory ||
        a.scoreVictory != b.scoreVictory || a.religiousVictory != b.religiousVictory || a.cultureVictory != b.cultureVictory ||
        a.diplomaticVictory != b.diplomaticVictory || a.scienceVictory != b.scienceVictory || a.disasterIntensity != b.disasterIntensity ||
        a.difficulty != b.difficulty || a.cityStates != b.cityStates || a.turnLimit != b.turnLimit || a.liveBattles != b.liveBattles ||
        a.regicide != b.regicide || a.barbarianClans != b.barbarianClans || a.monopolies != b.monopolies || a.rivalMemory != b.rivalMemory ||
        a.startEra != b.startEra || a.mods != b.mods)
        return false;
    for (size_t i = 0; i < a.players.size(); ++i) {
        const PlayerSetup& x = a.players[i];
        const PlayerSetup& y = b.players[i];
        if (x.civ != y.civ || x.human != y.human || x.hasProfile != y.hasProfile || !x.rivals.empty() || !y.rivals.empty()) return false;
    }
    return true;
}
}  // namespace

int32_t challengeWeek(int64_t unixSeconds) {
    return unixSeconds < kFirstMonday ? 0 : static_cast<int32_t>((unixSeconds - kFirstMonday) / kWeekSeconds);
}

Challenge weeklyChallenge(const Rules& rules, int32_t week) {
    Challenge c;
    c.week = week;
    const uint64_t h = mix(static_cast<uint64_t>(week) + 0x50564552ull);
    GameSetup& s = c.setup;
    s.seed = h;
    s.mapSize = "MAPSIZE_SMALL";
    s.speed = "GAMESPEED_SHORT_REIGN";
    s.difficulty = 3;  // Prince
    s.liveBattles = true;
    s.rivalMemory = false;  // the same rivals for everyone
    s.startEra = static_cast<int>((h >> 8) % 3);  // Ancient, Classical or Medieval
    const size_t civs = rules.civs.size();
    const size_t first = civs > 0 ? static_cast<size_t>((h >> 16) % civs) : 0;
    for (size_t i = 0; i < 4 && i < civs; ++i) {
        PlayerSetup p;
        p.civ = rules.civs[(first + i * 3) % civs].id;
        p.human = i == 0;
        s.players.push_back(p);
    }
    c.goal = static_cast<ChallengeGoal>(week % kNumChallengeGoals);
    static const char* const kGoals[] = {"win any victory", "win without declaring a war", "win a Science victory", "win a Culture victory",
                                         "score the most you can by the last turn"};
    static const char* const kEras[] = {"Ancient", "Classical", "Medieval"};
    const std::string civ = civs > 0 ? rules.civs[first].name : std::string("?");
    c.text = "Week " + std::to_string(week) + " challenge: " + kGoals[static_cast<size_t>(c.goal)] + ", as " + civ + " (Short Reign, " +
             kEras[static_cast<size_t>(s.startEra)] + " start)";
    return c;
}

ChallengeResult judgeChallenge(const Game& game, const Challenge& challenge) {
    ChallengeResult r;
    r.valid = true;
    const GameState& s = game.state();
    r.turn = s.turn;
    r.score = s.players.empty() ? 0 : game.score(0);
    const bool won = s.winner == 0;
    switch (challenge.goal) {
        case ChallengeGoal::AnyVictory: r.goalMet = won; break;
        case ChallengeGoal::Peaceful: r.goalMet = won && game.profile(0) && game.profile(0)->warsDeclared == 0; break;
        case ChallengeGoal::Science: r.goalMet = won && s.victory == Victory::Science; break;
        case ChallengeGoal::Culture: r.goalMet = won && s.victory == Victory::Culture; break;
        case ChallengeGoal::Score: r.goalMet = game.gameOver(); break;
    }
    return r;
}

ChallengeResult checkChallenge(const Rules& rules, const Challenge& challenge, const std::vector<uint8_t>& save) {
    ChallengeResult r;
    GameSetup setup;
    if (!peekSaveSetup(save, setup)) {
        r.error = "not a save of this version";
        return r;
    }
    if (!sameSetup(setup, challenge.setup)) {
        r.error = "not this week's challenge";
        return r;
    }
    std::string err;
    const std::unique_ptr<Game> played = loadGame(rules, save, &err);
    if (!played) {
        r.error = "save: " + err;
        return r;
    }
    // Replay the log from the week's setup; a live battle's result must lie within the band.
    std::unique_ptr<Game> g = Game::create(rules, challenge.setup, &err);
    if (!g) {
        r.error = "setup: " + err;
        return r;
    }
    const int band = rules.globalInt("LIVE_BATTLE_BAND_PERCENT");
    for (size_t i = 0; i < played->log().size(); ++i) {
        const Command& c = played->log()[i];
        if (c.type == CommandType::BattleResult) {
            const PendingBattle& b = g->state().pendingBattle;
            const auto inBand = [&](int field, int expected) {
                return field >= expected * (100 - band) / 100 && field <= (expected * (100 + band) + 99) / 100;
            };
            if (!inBand(c.arg, b.expectedToDefender) || !inBand(c.arg2, b.expectedToAttacker)) {
                r.error = "command " + std::to_string(i) + ": a live battle's result lies outside the band";
                return r;
            }
        }
        if (g->submit(c) != CommandError::Ok) {
            r.error = "command " + std::to_string(i) + " (" + describe(c) + ") does not replay";
            return r;
        }
    }
    if (g->stateHash() != played->stateHash()) {
        r.error = "the log does not replay to the saved game";
        return r;
    }
    return judgeChallenge(*g, challenge);
}

}  // namespace sov
