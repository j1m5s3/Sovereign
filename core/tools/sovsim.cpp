// Headless simulator: generates a game, plays it with the random bot or the AI, checks
// that replaying the command log reproduces the same state, and prints the
// state hash (compare hashes across machines to catch nondeterminism).
//
//   sovsim [--rules DIR]... [--seed N] [--turns N] [--players N] [--size MAPSIZE_X] [--save FILE] [--load FILE] [--map] [--cities]
//          [--ai] [--ai-seats N] [--turn-limit N] [--disasters N] [--difficulty N] [--bench N] [--speed GAMESPEED_X] [--era ERA_X] [--clans] [--monopolies]   (--ai: the AI plays every seat; --ai-seats N: the first N seats, the bot the rest;
//          --turn-limit: Score victory after this turn instead of the speed's calendar; --disasters N: intensity 0-4, -1 none; --difficulty N: 0 Settler .. 3 Prince .. 7 Deity;
//          --bench N: the pace benchmark over seeds 1..N, averages at checkpoints up to --turns; --clans: the Barbarian Clans mode; --monopolies: the Monopolies and Corporations mode).
//          Stops early when someone wins.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "random_bot.h"
#include "sovereign/ai.h"
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

// The pace benchmark: all-AI games on seeds 1..n, the major civs' averages at each checkpoint.
static int runBench(const Rules& rules, GameSetup setup, int n, int turns) {
    std::vector<int> checkpoints;
    for (int t : {25, 50, 75, 100, 150, 200, 250, 300, 400, 500}) {
        if (t <= turns) checkpoints.push_back(t);
    }
    std::vector<ai::PaceSample> sum(checkpoints.size());
    std::vector<int> games(checkpoints.size(), 0);
    std::vector<std::vector<int>> picked(checkpoints.size(), std::vector<int>(static_cast<size_t>(ai::Strategy::Count), 0));
    for (int seed = 1; seed <= n; ++seed) {
        setup.seed = static_cast<uint64_t>(seed);
        std::string err;
        auto game = Game::create(rules, setup, &err);
        if (!game) {
            std::fprintf(stderr, "create: %s\n", err.c_str());
            return 1;
        }
        for (size_t k = 0; k < checkpoints.size(); ++k) {
            while (game->state().turn < checkpoints[k] && !game->gameOver()) ai::playTurn(*game);
            if (game->gameOver()) break;
            const ai::PaceSample p = ai::measurePace(*game);
            ai::PaceSample& t = sum[k];
            t.cities += p.cities, t.population += p.population, t.techs += p.techs, t.civics += p.civics, t.era += p.era;
            t.science += p.science, t.culture += p.culture, t.production += p.production, t.gold += p.gold;
            ++games[k];
            for (const Player& pl : game->state().players) {
                if (!pl.alive || pl.barbarian || pl.freeCity || pl.cityState != kNone) continue;
                for (ai::Strategy st : ai::strategies(*game, pl.id)) ++picked[k][static_cast<size_t>(st)];
            }
        }
    }
    std::printf("turn  games  cities    pop  techs civics   era  science culture  prod   gold\n");
    for (size_t k = 0; k < checkpoints.size(); ++k) {
        if (games[k] == 0) continue;
        const ai::PaceSample& t = sum[k];
        const auto avg = [&](int64_t v) { return static_cast<double>(v) / 100.0 / games[k]; };
        std::printf("%4d  %5d  %6.1f %6.1f %6.1f %6.1f %5.1f  %7.1f %7.1f %5.1f %6.0f\n", checkpoints[k], games[k], avg(t.cities), avg(t.population),
                    avg(t.techs), avg(t.civics), avg(t.era), avg(t.science), avg(t.culture), avg(t.production), avg(t.gold));
    }
    std::printf("strategies held by major civs at each checkpoint:\n");
    for (size_t k = 0; k < checkpoints.size(); ++k) {
        if (games[k] == 0) continue;
        std::printf("%4d ", checkpoints[k]);
        for (size_t i = 0; i < picked[k].size(); ++i) {
            if (picked[k][i] > 0) std::printf(" %s %d,", ai::strategyName(static_cast<ai::Strategy>(i)), picked[k][i]);
        }
        std::printf("\n");
    }
    return 0;
}

int main(int argc, char** argv) {
    std::vector<std::string> rulesDirs;
    GameSetup setup;
    setup.seed = 1;
    int turns = 50, players = 2, aiSeats = 0, bench = 0;
    std::string savePath, loadPath, era;
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
        else if (a == "--load") loadPath = next();
        else if (a == "--map") showMap = true;
        else if (a == "--cities") showCities = true;
        else if (a == "--ai") aiSeats = 1 << 20;
        else if (a == "--ai-seats") aiSeats = std::atoi(next().c_str());
        else if (a == "--turn-limit") setup.turnLimit = std::atoi(next().c_str());
        else if (a == "--disasters") setup.disasterIntensity = std::atoi(next().c_str());
        else if (a == "--clans") setup.barbarianClans = true;
        else if (a == "--monopolies") setup.monopolies = true;
        else if (a == "--difficulty") setup.difficulty = std::atoi(next().c_str());
        else if (a == "--speed") setup.speed = next();
        else if (a == "--era") era = next();
        else if (a == "--bench") bench = std::atoi(next().c_str());
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
    if (!era.empty()) {
        if (rules.era(era) == kNone) {
            std::fprintf(stderr, "unknown era %s\n", era.c_str());
            return 1;
        }
        setup.startEra = static_cast<int>(rules.era(era));
    }
    if (bench > 0) return runBench(rules, setup, bench, turns);
    std::unique_ptr<Game> game;
    if (!loadPath.empty()) {
        std::ifstream in(loadPath, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        game = loadGame(rules, bytes, &err);
    } else {
        game = Game::create(rules, setup, &err);
    }
    if (!game) {
        std::fprintf(stderr, "create: %s\n", err.c_str());
        return 1;
    }
    {
        Rng botRng(setup.seed ^ 0x5EEDull);
        const int stopAt = game->state().turn + turns;
        while (game->state().turn < stopAt && !game->gameOver()) {
            if (game->state().currentPlayer < aiSeats) ai::playTurn(*game);
            else sovbot::playTurn(*game, botRng);
        }
    }
    auto replayed = loadPath.empty() ? Game::replay(rules, setup, game->log(), &err) : nullptr;
    if (loadPath.empty() && (!replayed || replayed->stateHash() != game->stateHash())) {
        std::fprintf(stderr, "REPLAY MISMATCH %s\n", err.c_str());
        return 1;
    }
    if (showMap) printMap(*game);
    if (showCities) {
        for (const Player& p : game->state().players) {
            auto count = [](const std::vector<uint8_t>& v) { return std::count(v.begin(), v.end(), 1); };
            std::printf("player %d (%s): gold %s, science %s/turn, culture %s/turn, techs %ld, civics %ld, government %s, policies",
                        p.id, p.barbarian ? "Barbarians" : p.cityState != kNone ? rules.cityStates[static_cast<size_t>(p.cityState)].name.c_str()
                                                   : p.civ == kNone ? "?" : rules.civs[static_cast<size_t>(p.civ)].name.c_str(),
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
            long cities = 0, pop = 0;
            for (const City& c : game->state().cities) {
                if (c.owner != p.id) continue;
                ++cities;
                pop += c.population;
            }
            std::printf(", units %ld (%ld promoted), cities %ld (pop %ld), leader", units, promoted, cities, pop);
            if (const Unit* l = game->leaderOf(p.id)) {
                for (TypeIndex g : l->gear) std::printf(" %s", g == kNone ? "-" : rules.gear[static_cast<size_t>(g)].name.c_str());
            } else {
                std::printf(" none");
            }
            std::printf(", at war with");
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
        long wars = 0, attacks = 0, promotions = 0, strikes = 0, razed = 0, clanDeals = 0;
        for (const Command& c : game->log()) {
            wars += c.type == CommandType::DeclareWar;
            attacks += c.type == CommandType::Attack || c.type == CommandType::RangedAttack;
            promotions += c.type == CommandType::Promote;
            strikes += c.type == CommandType::CityStrike;
            razed += c.type == CommandType::RazeCity;
            clanDeals += c.type == CommandType::BribeCamp || c.type == CommandType::HireFromCamp || c.type == CommandType::InciteCamp;
        }
        long captured = 0, eliminated = 0;
        long capitals = 0;
        for (const City& c : game->state().cities) {
            captured += c.owner != c.originalOwner;
            capitals += c.originalCapital && c.owner != c.originalOwner;
        }
        for (const Player& p : game->state().players) eliminated += !p.alive;
        long districts = 0, districtsDone = 0;
        for (const City& c : game->state().cities) {
            districts += static_cast<long>(c.districts.size());
            for (const CityDistrict& d : c.districts) districtsDone += d.complete;
        }
        std::printf("districts placed %ld, finished %ld\n", districts, districtsDone);
        int kinds[5] = {0, 0, 0, 0, 0};
        long disasters = 0;
        for (const GameEvent& e : game->state().events) {
            if (static_cast<int>(e.kind) < 5) ++kinds[static_cast<int>(e.kind)];
            disasters += e.kind == EventKind::Disaster;
        }
        std::printf("agents %ld; recent assassinations: %d leaders killed, %d wounded, %d assassins killed, %d captured\n",
                    static_cast<long>(game->state().agents.size()), kinds[1], kinds[2], kinds[3], kinds[4]);
        long freeCities = 0, wavering = 0;
        for (const City& c : game->state().cities) {
            freeCities += game->state().players[static_cast<size_t>(c.owner)].freeCity;
            wavering += c.loyalty <= 75;
        }
        std::printf("free cities %ld, cities at 75 loyalty or less %ld\n", freeCities, wavering);
        std::printf("climate: CO2 %lld, phase %d, +%d.%d degrees, recent disasters %ld, droughts %zu\n",
                    static_cast<long long>(game->state().co2), game->state().climatePhase, game->temperatureTenths() / 10,
                    game->temperatureTenths() % 10, disasters, game->state().droughts.size());
        const long camps = static_cast<long>(game->state().camps.size());
        std::printf("wars declared %ld, attacks %ld, promotions %ld, city strikes %ld, cities held by a conqueror %ld (capitals %ld), "
                    "razed %ld, players eliminated %ld, barbarian camps %ld standing / %ld cleared\n",
                    wars, attacks, promotions, strikes, captured, capitals, razed, eliminated, camps,
                    static_cast<long>(game->state().nextCampId - 1) - camps);
        if (game->state().setup.monopolies) {
            long industries = 0, corporations = 0, monopolies = 0;
            for (const Plot& pl : game->state().plots) {
                industries += pl.industry == 1;
                corporations += pl.industry == 2;
            }
            for (const Player& p : game->state().players) monopolies += game->monopolySources(p.id);
            std::printf("monopolies: %ld industries, %ld corporations, %ld luxury sources under a monopoly\n", industries, corporations, monopolies);
        }
        if (game->state().setup.barbarianClans) {
            long fromClans = 0;
            for (const Player& p : game->state().players) fromClans += p.cityState != kNone && p.startPos != Hex{} && p.id > game->barbarianPlayer() ? 1 : 0;
            std::printf("barbarian clans: %ld dealings (bribes, hires, incitements), %ld city-states grown from camps\n", clanDeals, fromClans);
        }
    }
    if (!savePath.empty()) {
        std::vector<uint8_t> bytes = saveGame(*game);
        std::ofstream(savePath, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                                        static_cast<std::streamsize>(bytes.size()));
    }
    if (game->gameOver()) {
        static const char* names[] = {"none", "Domination", "Score", "last civ standing", "Religious", "Culture", "Diplomatic", "Science"};
        const PlayerId w = game->state().winner;
        std::printf("winner: player %d (%s), %s victory on turn %d, score %d\n", w,
                    rules.civs[static_cast<size_t>(game->state().players[static_cast<size_t>(w)].civ)].name.c_str(),
                    names[static_cast<size_t>(game->state().victory)], game->state().turn, game->score(w));
    }
    std::printf("turn %d, %zu cities, %zu units, %zu commands, rules %016llx, state %016llx\n", game->state().turn,
                game->state().cities.size(), game->state().units.size(), game->log().size(),
                static_cast<unsigned long long>(rules.checksum()),
                static_cast<unsigned long long>(game->stateHash()));
    return 0;
}
