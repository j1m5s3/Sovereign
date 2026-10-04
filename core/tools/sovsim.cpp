// Headless simulator: generates a game, plays it with the random bot, checks
// that replaying the command log reproduces the same state, and prints the
// state hash (compare hashes across machines to catch nondeterminism).
//
//   sovsim [--rules DIR]... [--seed N] [--turns N] [--players N] [--size MAPSIZE_X] [--save FILE] [--map] [--cities]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "random_bot.h"
#include "sovereign/game.h"
#include "sovereign/serialize.h"

using namespace sov;

static void printMap(const Game& g) {
    const GameState& s = g.state();
    for (int y = 0; y < s.grid.height(); ++y) {
        std::string line(static_cast<size_t>(y & 1), ' ');
        for (int x = 0; x < s.grid.width(); ++x) {
            Hex h{x, y};
            const Plot& p = s.plot(h);
            const TerrainType& t = g.rules().terrains[static_cast<size_t>(p.terrain)];
            char c = '?';
            if (t.water) c = t.shallowWater ? '-' : '~';
            else if (t.relief == Relief::Mountain) c = '^';
            else if (t.relief == Relief::Hills) c = 'n';
            else c = t.base[0] + ('a' - 'A');
            if (p.feature != kNone && !t.water) c = '%';
            if (s.cityAt(h)) c = 'C';
            line += c;
            line += ' ';
        }
        std::printf("%s\n", line.c_str());
    }
}

int main(int argc, char** argv) {
    std::vector<std::string> rulesDirs;
    GameSetup setup;
    setup.seed = 1;
    int turns = 50, players = 2;
    std::string savePath;
    bool showMap = false, showCities = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rules") rulesDirs.push_back(next());
        else if (a == "--seed") setup.seed = std::strtoull(next().c_str(), nullptr, 10);
        else if (a == "--turns") turns = std::atoi(next().c_str());
        else if (a == "--players") players = std::atoi(next().c_str());
        else if (a == "--size") setup.mapSize = next();
        else if (a == "--save") savePath = next();
        else if (a == "--map") showMap = true;
        else if (a == "--cities") showCities = true;
        else {
            std::fprintf(stderr, "unknown argument %s\n", a.c_str());
            return 2;
        }
    }
    if (rulesDirs.empty()) rulesDirs.push_back("data/rules");
    Rules rules;
    std::string err;
    if (!rules.load(rulesDirs, &err)) {
        std::fprintf(stderr, "rules: %s\n", err.c_str());
        return 1;
    }
    for (int i = 0; i < players; ++i) {
        setup.players.push_back({rules.civs[static_cast<size_t>(i) % rules.civs.size()].id, false});
    }
    auto game = Game::create(rules, setup, &err);
    if (!game) {
        std::fprintf(stderr, "create: %s\n", err.c_str());
        return 1;
    }
    sovbot::playTurns(*game, setup.seed ^ 0x5EEDull, turns);
    auto replayed = Game::replay(rules, setup, game->log(), &err);
    if (!replayed || replayed->stateHash() != game->stateHash()) {
        std::fprintf(stderr, "REPLAY MISMATCH %s\n", err.c_str());
        return 1;
    }
    if (showMap) printMap(*game);
    if (showCities) {
        for (const Player& p : game->state().players) {
            auto count = [](const std::vector<uint8_t>& v) { return std::count(v.begin(), v.end(), 1); };
            std::printf("player %d (%s): gold %s, science %s/turn, culture %s/turn, techs %ld, civics %ld, government %s, policies",
                        p.id, p.barbarian ? "Barbarians" : rules.civs[static_cast<size_t>(p.civ)].name.c_str(),
                        p.gold.toString().c_str(),
                        game->sciencePerTurn(p.id).toString().c_str(), game->culturePerTurn(p.id).toString().c_str(),
                        static_cast<long>(count(p.techs.done)), static_cast<long>(count(p.civics.done)),
                        p.government == kNone ? "none" : rules.governments[static_cast<size_t>(p.government)].name.c_str());
            for (TypeIndex pol : p.policies) {
                std::printf(" %s", pol == kNone ? "-" : rules.policies[static_cast<size_t>(pol)].name.c_str());
            }
            long units = 0, promoted = 0;
            for (const Unit& u : game->state().units) {
                if (u.owner != p.id) continue;
                ++units;
                promoted += u.promotions.empty() ? 0 : 1;
            }
            std::printf(", units %ld (%ld promoted), at war with", units, promoted);
            for (const Player& o : game->state().players) {
                if (game->atWar(p.id, o.id)) std::printf(" p%d", o.id);
            }
            std::printf("\n");
        }
        for (const City& c : game->state().cities) {
            CityReport r = game->cityReport(c.id);
            std::printf("  %-14s p%d pop %2d food %s/%d housing %s amenities %d/%d buildings %zu yields F%s P%s G%s S%s C%s\n",
                        c.name.c_str(), c.owner, c.population, c.food.toString().c_str(), game->growthThreshold(c.population),
                        r.housing.toString().c_str(), r.amenities, r.amenitiesNeeded, c.buildings.size(),
                        r.yields[0].toString().c_str(), r.yields[1].toString().c_str(), r.yields[2].toString().c_str(),
                        r.yields[3].toString().c_str(), r.yields[4].toString().c_str());
        }
    }
    if (showCities) {
        long wars = 0, attacks = 0, promotions = 0, strikes = 0, razed = 0;
        for (const Command& c : game->log()) {
            wars += c.type == CommandType::DeclareWar;
            attacks += c.type == CommandType::Attack || c.type == CommandType::RangedAttack;
            promotions += c.type == CommandType::Promote;
            strikes += c.type == CommandType::CityStrike;
            razed += c.type == CommandType::RazeCity;
        }
        long captured = 0, eliminated = 0;
        for (const City& c : game->state().cities) captured += c.owner != c.originalOwner;
        for (const Player& p : game->state().players) eliminated += !p.alive;
        long districts = 0, districtsDone = 0;
        for (const City& c : game->state().cities) {
            districts += static_cast<long>(c.districts.size());
            for (const CityDistrict& d : c.districts) districtsDone += d.complete;
        }
        std::printf("districts placed %ld, finished %ld\n", districts, districtsDone);
        const long camps = static_cast<long>(game->state().camps.size());
        std::printf("wars declared %ld, attacks %ld, promotions %ld, city strikes %ld, cities held by a conqueror %ld, "
                    "razed %ld, players eliminated %ld, barbarian camps %ld standing / %ld cleared\n",
                    wars, attacks, promotions, strikes, captured, razed, eliminated, camps,
                    static_cast<long>(game->state().nextCampId - 1) - camps);
    }
    if (!savePath.empty()) {
        std::vector<uint8_t> bytes = saveGame(*game);
        std::ofstream(savePath, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                                        static_cast<std::streamsize>(bytes.size()));
    }
    std::printf("turn %d, %zu cities, %zu units, %zu commands, rules %016llx, state %016llx\n", game->state().turn,
                game->state().cities.size(), game->state().units.size(), game->log().size(),
                static_cast<unsigned long long>(rules.checksum()),
                static_cast<unsigned long long>(game->stateHash()));
    return 0;
}
