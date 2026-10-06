// City-states and envoys (08-diplomacy-city-states-governors.md, City-States; data:
// city-states.md). City-states are one-city players placed at the start; major civs earn
// envoys (meeting one first, civics, influence from their government) and send them for the
// tier bonuses of each city-state's type; the player with the most envoys (3+) is suzerain.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }
}  // namespace

void placeCityStates(GameState& s, const Rules& rules) {
    const TypeIndex size = rules.mapSize(s.setup.mapSize);
    const int want = s.setup.cityStates >= 0 ? s.setup.cityStates : size == kNone ? 0 : rules.mapSizes[at(size)].defaultCityStates;
    if (want <= 0 || rules.cityStates.empty()) return;
    const HexGrid& g = s.grid;
    const int fromMajor = rules.globalInt("START_DISTANCE_MINOR_MAJOR_CIVILIZATION");
    const int fromMinor = rules.globalInt("START_DISTANCE_MINOR_CIVILIZATION_START");
    std::vector<Hex> majors;
    for (const Player& p : s.players) majors.push_back(p.startPos);
    // Candidate plots in a seeded order, then the first that keep their distances.
    std::vector<int> cands;
    for (int i = 0; i < g.size(); ++i) {
        const Hex h = g.at(i);
        if (!isLandPassable(s, rules, h) || rules.terrains[at(s.plot(h).terrain)].base == "SNOW") continue;
        if (std::min(h.y, g.height() - 1 - h.y) < 2) continue;
        cands.push_back(i);
    }
    Rng& rng = s.rng.get(RngStream::MapGen);
    for (size_t i = cands.size(); i > 1; --i) std::swap(cands[i - 1], cands[rng.below(static_cast<uint32_t>(i))]);
    std::vector<TypeIndex> kinds;
    for (size_t i = 0; i < rules.cityStates.size(); ++i) kinds.push_back(static_cast<TypeIndex>(i));
    for (size_t i = kinds.size(); i > 1; --i) std::swap(kinds[i - 1], kinds[rng.below(static_cast<uint32_t>(i))]);
    std::vector<Hex> placed;
    for (int idx : cands) {
        if (static_cast<int>(placed.size()) >= want || placed.size() >= kinds.size()) break;
        const Hex h = g.at(idx);
        bool ok = true;
        for (const Hex& m : majors) ok = ok && g.distance(m, h) >= fromMajor;
        for (const Hex& m : placed) ok = ok && g.distance(m, h) >= fromMinor;
        if (!ok) continue;
        Player p;
        p.id = static_cast<PlayerId>(s.players.size());
        p.cityState = kinds[placed.size()];
        p.startPos = h;
        p.leaderName = rules.cityStates[at(p.cityState)].name;
        Game::fitPlayerToRules(p, rules);
        p.visibility.assign(static_cast<size_t>(g.size()), 0);
        s.players.push_back(std::move(p));
        placed.push_back(h);
    }
    for (Player& p : s.players) p.relations.resize(s.players.size());
}

bool Game::isCityState(PlayerId p) const {
    return p >= 0 && at(p) < state_.players.size() && state_.players[at(p)].cityState != kNone;
}

int Game::envoysAt(PlayerId player, PlayerId cs) const {
    const Player& p = state_.players[at(player)];
    // Amani serving there counts as envoys (08: Governors, Messenger and Puppeteer).
    return (at(cs) < p.envoys.size() ? p.envoys[at(cs)] : 0) + governorEnvoys(player, cs);
}

PlayerId Game::suzerainOf(PlayerId cs) const {
    // The most envoys, at least INFLUENCE_TOKENS_MINIMUM_FOR_SUZERAIN, and more than anyone else.
    PlayerId best = kNoPlayer;
    int most = 0;
    bool tie = false;
    for (const Player& p : state_.players) {
        const int n = envoysAt(p.id, cs);
        if (n > most) {
            most = n;
            best = p.id;
            tie = false;
        } else if (n == most && n > 0) {
            tie = true;
        }
    }
    return !tie && most >= rules_->globalInt("INFLUENCE_TOKENS_MINIMUM_FOR_SUZERAIN") ? best : kNoPlayer;
}

bool Game::canSendEnvoy(PlayerId player, PlayerId cs) const {
    const Player& p = state_.players[at(player)];
    if (!isMajor(p) || p.envoyTokens <= 0 || !isCityState(cs) || !state_.players[at(cs)].alive) return false;
    // Met: the player has seen its city.
    for (const City& c : state_.cities) {
        if (c.owner == cs && visibility(player, c.pos) != Visibility::Unrevealed) return true;
    }
    return false;
}

void Game::processEnvoys(PlayerId pid) {
    Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return;
    if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
    // Influence from the government, in batches of envoys (08: Influence Points).
    if (p.government != kNone && p.anarchyTurns == 0) {
        const GovernmentType& gov = rules_->governments[at(p.government)];
        p.influence += gov.influencePerTurn + static_cast<int>(sumPlayerModifiers(state_, *rules_, p, ModEffect::InfluencePerTurn).toInt());  // + Charismatic Leader...
        if (gov.influenceThreshold > 0 && p.influence >= gov.influenceThreshold) {
            p.influence -= gov.influenceThreshold;
            if (!policyIs(p.id, "POLICY_ROGUE_STATE")) p.envoyTokens += gov.envoysPerThreshold;  // Rogue State: no envoys (09)
        }
    }
    // The first major civ to meet a city-state gets an envoy there (INFLUENCE_TOKENS_FREE_FOR_FIRST_PLAYER_MEET).
    for (Player& cs : state_.players) {
        if (cs.cityState == kNone || !cs.alive || cs.firstMetBy != kNoPlayer) continue;
        for (const City& c : state_.cities) {
            if (c.owner == cs.id && visibility(pid, c.pos) != Visibility::Unrevealed) {
                cs.firstMetBy = pid;
                p.envoys[at(cs.id)] += rules_->globalInt("INFLUENCE_TOKENS_FREE_FOR_FIRST_PLAYER_MEET");
                break;
            }
        }
    }
}

Yields Game::envoyYields(const City& city) const {
    Yields out{};
    const Player& owner = state_.players[at(city.owner)];
    if (!isMajor(owner)) return out;
    for (const Player& cs : state_.players) {
        if (cs.cityState == kNone || !cs.alive || atWar(owner.id, cs.id)) continue;
        const int n = envoysAt(owner.id, cs.id);
        if (n <= 0) continue;
        const CityStateKind kind = rules_->cityStates[at(cs.cityState)].kind;
        for (const EnvoyBonus& b : rules_->envoyBonuses) {
            if (b.kind != kind || n < b.envoys || b.amount <= 0) continue;
            if ((b.capital && city.capital) || (b.building != kNone && city.has(b.building)))
                out[static_cast<size_t>(b.yield)] += Fixed::fromInt(b.amount);
        }
    }
    return out;
}

int Game::envoyProduction(const City& city, ProductionItem item) const {
    const Player& owner = state_.players[at(city.owner)];
    if (!isMajor(owner)) return 0;
    const EnvoyToward toward = item.kind == ProductionKind::Unit ? EnvoyToward::Units
                               : item.kind == ProductionKind::District ? EnvoyToward::Districts
                                                                       : EnvoyToward::Buildings;
    int total = 0;
    for (const Player& cs : state_.players) {
        if (cs.cityState == kNone || !cs.alive || atWar(owner.id, cs.id)) continue;
        const int n = envoysAt(owner.id, cs.id);
        if (n <= 0) continue;
        const CityStateKind kind = rules_->cityStates[at(cs.cityState)].kind;
        for (const EnvoyBonus& b : rules_->envoyBonuses) {
            if (b.kind != kind || n < b.envoys || b.production <= 0 || b.toward != toward) continue;
            const bool here = (b.capital && city.capital) ||
                              std::any_of(b.buildings.begin(), b.buildings.end(), [&](TypeIndex x) { return city.has(x); });
            if (here) total += b.production;
        }
    }
    return total;
}

}  // namespace sov
