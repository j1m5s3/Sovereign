#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "helpers.h"
#include "../tools/random_bot.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::rules;

namespace {
std::unique_ptr<Game> playedGame(uint64_t seed, int turns) {
    std::string err;
    auto g = Game::create(rules(), sovtest::duelSetup(seed), &err);
    if (g) sovbot::playTurns(*g, seed * 31 + 7, turns);
    return g;
}

std::string hex64(uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}
}  // namespace

TEST(save_round_trip_is_exact) {
    auto g = playedGame(11, 25);
    REQUIRE(g);
    std::vector<uint8_t> bytes = saveGame(*g);
    std::string err;
    auto loaded = loadGame(rules(), bytes, &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
    CHECK_EQ(loaded->log().size(), g->log().size());
    CHECK(saveGame(*loaded) == bytes);
}

TEST(turn_statistics_are_kept_each_turn_and_saved) {
    // Each world turn records every major civ alive once, for the end-game graphs; a save keeps them.
    auto g = playedGame(11, 25);
    REQUIRE(g);
    const GameState& s = g->state();
    int majors = 0;
    for (const Player& p : s.players) majors += p.alive && g->isMajorCiv(p.id) ? 1 : 0;
    REQUIRE(majors == 2);
    CHECK_EQ(s.turnStats.size(), static_cast<size_t>(majors * (s.turn - 1)));
    std::vector<int32_t> techs(s.players.size(), 0);
    for (size_t i = 0; i < s.turnStats.size(); ++i) {
        const TurnStats& t = s.turnStats[i];
        CHECK_EQ(t.turn, static_cast<int32_t>(2 + i / 2));
        CHECK(g->isMajorCiv(t.player));
        CHECK(t.techs >= techs[static_cast<size_t>(t.player)]);
        techs[static_cast<size_t>(t.player)] = t.techs;
        CHECK(t.cities >= 1);
        CHECK(t.population >= t.cities);
        CHECK(t.score > 0);
        if (t.turn == s.turn && t.player > s.currentPlayer) continue;  // its turn has not begun
        CHECK(t.science > 0);  // the turn's yields, filled in as the civ's cities yielded
        CHECK(t.culture > 0);
    }
    // The last turn's figures are the game's as that turn began.
    const TurnStats& last = s.turnStats.back();
    CHECK_EQ(last.turn, s.turn);
    int cities = 0, military = 0;
    for (const City& c : s.cities) cities += c.owner == last.player ? 1 : 0;
    for (const Unit& u : s.units) {
        const UnitType& t = rules().units[static_cast<size_t>(u.type)];
        if (u.owner == last.player && t.layer == UnitLayer::Military) military += std::max(t.combat, t.ranged) * u.hp / 100;
    }
    CHECK(military > 0);
    if (s.currentPlayer < last.player) {  // the civ has not moved yet this turn
        CHECK_EQ(last.cities, cities);
        CHECK_EQ(last.military, military);
    }
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->state().turnStats.size() == s.turnStats.size());
    CHECK_EQ(loaded->state().turnStats.back().military, last.military);
    CHECK_EQ(loaded->state().turnStats.back().score, last.score);
}

TEST(save_then_continue_matches_uninterrupted_play) {
    auto a = playedGame(12, 15);
    REQUIRE(a);
    std::string err;
    auto b = loadGame(rules(), saveGame(*a), &err);
    REQUIRE(b);
    sovbot::playTurns(*a, 999, 15);
    sovbot::playTurns(*b, 999, 15);
    CHECK_EQ(a->stateHash(), b->stateHash());
}

TEST(save_rejects_bad_input) {
    auto g = playedGame(13, 5);
    REQUIRE(g);
    std::vector<uint8_t> bytes = saveGame(*g);
    std::string err;
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + static_cast<long>(bytes.size() / 2));
    CHECK(!loadGame(rules(), truncated, &err));
    std::vector<uint8_t> badMagic = bytes;
    badMagic[0] = 'X';
    CHECK(!loadGame(rules(), badMagic, &err));
    std::vector<uint8_t> badVersion = bytes;
    badVersion[4] = 99;
    CHECK(!loadGame(rules(), badVersion, &err));
    CHECK(err.find("version") != std::string::npos);
    // Different rules data (a mod layer) must not load this save.
    Rules other;
    std::map<std::string, std::string> extra = {{"globals.json", R"({"globals": {"CITY_MIN_RANGE": 4}})"}};
    std::vector<std::map<std::string, std::string>> layers;
    std::map<std::string, std::string> base;
    for (const std::string& name : Rules::fileNames()) {
        std::ifstream in(std::string(SOVEREIGN_RULES_DIR) + "/" + name, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        base[name] = ss.str();
    }
    REQUIRE(other.loadFromText({base, extra}, &err));
    CHECK(!loadGame(other, bytes, &err));
    CHECK(err.find("rules") != std::string::npos);
    // Random corruption never crashes.
    for (size_t i = 8; i < bytes.size(); i += 97) {
        std::vector<uint8_t> c = bytes;
        c[i] ^= 0x5A;
        (void)loadGame(rules(), c, &err);
    }
}

// Golden file: the state hash of a fixed game must match on every compiler
// and platform. A changed hash means the rules or save format changed; if that
// was intended, bump kSaveVersion when the format changed and regenerate with
// SOVEREIGN_UPDATE_GOLDEN=1.
TEST(save_golden_hash_is_stable) {
    auto g = playedGame(2026, 30);
    REQUIRE(g);
    const std::string path = std::string(SOVEREIGN_GOLDEN_DIR) + "/duel_seed2026_30turns.hash";
    const std::string actual = hex64(g->stateHash()) + " " + hex64(rules().checksum()) + "\n";
    if (std::getenv("SOVEREIGN_UPDATE_GOLDEN")) {
        std::ofstream(path, std::ios::binary) << actual;
        std::printf("  wrote %s\n", path.c_str());
        return;
    }
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    CHECK_EQ(ss.str(), actual);
}
