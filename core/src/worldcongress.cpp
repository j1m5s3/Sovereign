// Grievances, Diplomatic Favor and the World Congress (08-diplomacy-city-states-governors.md:
// War, Grievances; Diplomatic Favor and World Congress [GS]; data: world-congress-emergencies.md).
// Grievances a civ holds against another come from wars declared on it, cities taken or razed,
// denunciations, spies caught and cities it still holds; they fade by era and weigh on opinion
// and on the offender's favor. Favor comes from the government and suzerainties. From the
// Medieval era the Congress meets every 30 turns on two resolutions; each civ has a free vote
// on each and may buy more with favor. The winning option and target hold until the next
// session; the Diplomatic Victory resolution moves Diplomatic Victory Points toward 20.
#include <algorithm>
#include <map>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }
}  // namespace

// ------------------------------------------------------------------ grievances

int Game::grievances(PlayerId holder, PlayerId against) const {
    const auto& g = state_.players[at(holder)].grievances;
    return at(against) < g.size() ? g[at(against)] : 0;
}

void Game::addGrievance(PlayerId holder, PlayerId against, int amount) {
    if (holder == against || holder < 0 || against < 0 || amount <= 0) return;
    if (!isMajor(state_.players[at(holder)]) || !isMajor(state_.players[at(against)])) return;
    // Public Relations: grievances against its target count double (A) or half (B).
    if (const PassedResolution* pr = passed(ResolutionKind::PublicRelations); pr && pr->target == against)
        amount = pr->option == 0 ? amount * 2 : amount / 2;
    auto& g = state_.players[at(holder)].grievances;
    if (g.size() < state_.players.size()) g.resize(state_.players.size(), 0);
    g[at(against)] += amount;
}

void Game::processGrievances() {
    // Cities a civ holds that were another's (GRIEVANCES_POSSESS_*): the old owner remembers.
    for (const City& c : state_.cities) {
        if (c.originalOwner == c.owner || c.originalOwner < 0 || at(c.originalOwner) >= state_.players.size()) continue;
        addGrievance(c.originalOwner, c.owner,
                     rules_->globalInt(c.originalCapital ? "GRIEVANCES_POSSESS_CAPITAL_PER_TURN" : "GRIEVANCES_POSSESS_NON_CAPITAL_PER_TURN"));
    }
    const int era = std::clamp(state_.gameEra, 0, static_cast<int>(rules_->eras.size()) - 1);
    const int decay = rules_->eras[at(era)].grievanceDecay;
    for (Player& p : state_.players) {
        for (int32_t& g : p.grievances) g = std::max(0, g - decay);
    }
}

int Game::favorPerTurn(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return 0;
    int favor = rules_->globalInt("WORLD_CONGRESS_BASELINE_FAVOR_PER_TURN");
    if (p.government != kNone && p.anarchyTurns == 0) favor += rules_->governments[at(p.government)].favor;
    for (const Player& cs : state_.players) {
        if (cs.cityState != kNone && cs.alive && suzerainOf(cs.id) == pid) favor += rules_->globalInt("WORLD_CONGRESS_SUZERAIN_FAVOR_PER_TURN");
        if (alliance(pid, cs.id) != AllianceType::None) favor += rules_->globalInt("WORLD_CONGRESS_ALLIANCE_FAVOR_PER_TURN");
    }
    // Grievances held against it beyond FAVOR_GRIEVANCES_START cost 1 per FAVOR_GRIEVANCES_DIVISOR.
    int held = 0;
    for (const Player& o : state_.players) held += o.id != pid ? grievances(o.id, pid) : 0;
    const int over = held - rules_->globalInt("FAVOR_GRIEVANCES_START");
    if (over > 0) favor += std::max(rules_->globalInt("FAVOR_GRIEVANCES_MINIMUM"), -(over / std::max(1, rules_->globalInt("FAVOR_GRIEVANCES_DIVISOR"))));
    // A civ's share of the world's CO2 costs favor (09 [GS]): 1 per FAVOR_CO2_DIVISOR %, down to FAVOR_CO2_MINIMUM.
    if (state_.co2 > 0 && p.co2 > 0) {
        const int share = static_cast<int>(p.co2 * 100 / state_.co2);
        favor += std::clamp(-(share / std::max(1, rules_->globalInt("FAVOR_CO2_DIVISOR"))), rules_->globalInt("FAVOR_CO2_MINIMUM"),
                            rules_->globalInt("FAVOR_CO2_MAXIMUM"));
    }
    return favor;
}

// ------------------------------------------------------------------ the Congress

const PassedResolution* Game::passed(ResolutionKind kind) const {
    for (const PassedResolution& r : state_.passedResolutions) {
        if (rules_->resolutions[at(r.resolution)].kind == kind) return &r;
    }
    return nullptr;
}

bool Game::hasVoted(PlayerId pid, int item) const {
    if (item < 0 || at(item) >= state_.congress.size()) return false;
    for (const CongressVote& v : state_.congress[at(item)].votes) {
        if (v.player == pid) return true;
    }
    return false;
}

std::string Game::candidateName(const CongressItem& item, int candidate) const {
    if (candidate < 0 || at(candidate) >= item.candidates.size()) return "?";
    const int32_t c = item.candidates[at(candidate)];
    switch (rules_->resolutions[at(item.resolution)].target) {
        case ResolutionTarget::Player: {
            const Player& p = state_.players[at(c)];
            return p.civ == kNone ? std::string("?") : rules_->civs[at(p.civ)].name;
        }
        case ResolutionTarget::GreatPersonClass: return rules_->greatPersonClasses[at(c)].name;
        case ResolutionTarget::District: return rules_->districts[at(c)].name;
        case ResolutionTarget::PromotionClass: return rules_->promotionClasses[at(c)];
        case ResolutionTarget::Other: break;
    }
    return "?";
}

void Game::openCongressSession() {
    std::vector<TypeIndex> pool;
    for (size_t i = 0; i < rules_->resolutions.size(); ++i) {
        const ResolutionType& r = rules_->resolutions[i];
        if (r.kind == ResolutionKind::Unsupported || r.target == ResolutionTarget::Other) continue;
        if ((r.minEra >= 0 && state_.gameEra < r.minEra) || (r.maxEra >= 0 && state_.gameEra > r.maxEra)) continue;
        if (r.kind == ResolutionKind::DiplomaticVictory && !state_.setup.diplomaticVictory) continue;
        pool.push_back(static_cast<TypeIndex>(i));
    }
    if (pool.empty()) return;
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    state_.congress.clear();
    // The Diplomatic Victory resolution comes up at every session once it may (engine: injected).
    for (TypeIndex r : pool) {
        if (rules_->resolutions[at(r)].kind == ResolutionKind::DiplomaticVictory) {
            state_.congress.push_back({r, {}, {}});
            pool.erase(std::find(pool.begin(), pool.end(), r));
            break;
        }
    }
    while (state_.congress.size() < 2 && !pool.empty()) {
        const size_t k = rng.below(static_cast<uint32_t>(pool.size()));
        state_.congress.push_back({pool[k], {}, {}});
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(k));
    }
    for (CongressItem& item : state_.congress) {
        switch (rules_->resolutions[at(item.resolution)].target) {
            case ResolutionTarget::Player:
                for (const Player& p : state_.players) {
                    if (isMajor(p)) item.candidates.push_back(p.id);
                }
                break;
            case ResolutionTarget::GreatPersonClass:
                for (size_t i = 0; i < rules_->greatPersonClasses.size(); ++i) item.candidates.push_back(static_cast<int32_t>(i));
                break;
            case ResolutionTarget::District:
                for (size_t i = 0; i < rules_->districts.size(); ++i) {
                    if (rules_->districts[i].id != "DISTRICT_CITY_CENTER") item.candidates.push_back(static_cast<int32_t>(i));
                }
                break;
            case ResolutionTarget::PromotionClass:
                for (size_t i = 0; i < rules_->promotionClasses.size(); ++i) item.candidates.push_back(static_cast<int32_t>(i));
                break;
            case ResolutionTarget::Other: break;
        }
    }
    state_.congressOpenedTurn = state_.turn;
    pushEvent(EventKind::CongressSession, kNoPlayer, kNoPlayer, 0);
    for (const Player& p : state_.players) {
        if (isMajor(p) && !p.human) aiCongressVotes(p.id);
    }
}

void Game::closeCongressSession() {
    state_.passedResolutions.clear();
    for (const CongressItem& item : state_.congress) {
        int options[2] = {0, 0};
        std::map<int32_t, int> targets[2];
        for (const CongressVote& v : item.votes) {
            options[v.option & 1] += v.votes;
            targets[v.option & 1][v.target] += v.votes;
        }
        if (options[0] + options[1] == 0 || item.candidates.empty()) continue;
        const uint8_t option = options[1] > options[0] ? 1 : 0;  // a tie goes to A
        // The target with the most votes among those who chose the winning option.
        int32_t best = 0;
        int most = -1;
        for (const auto& [t, n] : targets[option]) {
            if (n > most) {
                most = n;
                best = t;
            }
        }
        PassedResolution passedRes{item.resolution, option, item.candidates[at(best)]};
        const ResolutionType& rt = rules_->resolutions[at(item.resolution)];
        if (rt.kind == ResolutionKind::DiplomaticVictory) {
            // A one-time change, not a standing effect.
            Player& p = state_.players[at(passedRes.target)];
            p.diplomaticVictoryPoints = std::max(0, p.diplomaticVictoryPoints + (option == 0 ? 2 : -2));
        } else {
            state_.passedResolutions.push_back(passedRes);
        }
        pushEvent(EventKind::ResolutionPassed, kNoPlayer, rt.target == ResolutionTarget::Player ? static_cast<PlayerId>(passedRes.target) : kNoPlayer, item.resolution);
    }
    state_.congress.clear();
    state_.congressOpenedTurn = 0;
}

void Game::processWorldCongress() {
    if (state_.congressOpenedTurn > 0 && state_.turn > state_.congressOpenedTurn) closeCongressSession();
    if (state_.nextCongressTurn == 0) {
        // It first convenes once a civ reaches the Medieval era (WORLD_CONGRESS_INITIAL_ERA).
        bool ready = false;
        for (const Player& p : state_.players) ready = ready || (isMajor(p) && playerEra(p.id) >= rules_->globalInt("WORLD_CONGRESS_INITIAL_ERA"));
        if (!ready) return;
        state_.nextCongressTurn = state_.turn;
    }
    if (state_.congressOpenedTurn == 0 && state_.turn >= state_.nextCongressTurn) {
        const int speed = rules_->speeds[at(rules_->speed(state_.setup.speed))].costPercent;
        state_.nextCongressTurn = state_.turn + std::max(1, rules_->globalInt("WORLD_CONGRESS_MAX_TIME_BETWEEN_MEETINGS") * speed / 100);
        openCongressSession();
    }
}

void Game::aiCongressVotes(PlayerId me) {
    Player& self = state_.players[at(me)];
    PlayerId leader = kNoPlayer, warmonger = kNoPlayer;
    int bestScore = -1, worst = 0;
    for (const Player& p : state_.players) {
        if (!isMajor(p) || p.id == me) continue;
        const int sc = score(p.id);
        if (sc > bestScore) {
            bestScore = sc;
            leader = p.id;
        }
        if (grievances(me, p.id) > worst) {
            worst = grievances(me, p.id);
            warmonger = p.id;
        }
    }
    auto indexOf = [](const CongressItem& item, int32_t value) {
        for (size_t i = 0; i < item.candidates.size(); ++i) {
            if (item.candidates[i] == value) return static_cast<int32_t>(i);
        }
        return static_cast<int32_t>(0);
    };
    for (size_t k = 0; k < state_.congress.size(); ++k) {
        CongressItem& item = state_.congress[k];
        const ResolutionKind kind = rules_->resolutions[at(item.resolution)].kind;
        uint8_t option = 0;
        int32_t target = indexOf(item, me);
        int stake = 1;
        switch (kind) {
            case ResolutionKind::DiplomaticVictory:
                // Points for itself, unless a rival is close to winning.
                if (leader != kNoPlayer && state_.players[at(leader)].diplomaticVictoryPoints > self.diplomaticVictoryPoints + 4) {
                    option = 1;
                    target = indexOf(item, leader);
                }
                stake = 3;
                break;
            case ResolutionKind::TradePolicy:
            case ResolutionKind::MigrationTreaty:
                break;  // the good option for itself
            case ResolutionKind::PublicRelations:
                // Fewer grievances for itself after wars, or more against the civ it resents most.
                if (self.warsDeclared == 0 && warmonger != kNoPlayer) {
                    target = indexOf(item, warmonger);
                } else {
                    option = 1;
                }
                break;
            case ResolutionKind::Patronage: {
                int bestPts = -1;
                for (size_t i = 0; i < item.candidates.size(); ++i) {
                    const int pts = greatPersonPointsPerTurn(me, static_cast<TypeIndex>(item.candidates[i]));
                    if (pts > bestPts) {
                        bestPts = pts;
                        target = static_cast<int32_t>(i);
                    }
                }
                break;
            }
            case ResolutionKind::MilitaryAdvisory: {
                std::map<std::string, int> count;
                for (const Unit& u : state_.units) {
                    if (u.owner == me) ++count[rules_->units[at(u.type)].promotionClass];
                }
                int most = -1;
                for (size_t i = 0; i < item.candidates.size(); ++i) {
                    const int n = count[rules_->promotionClasses[at(item.candidates[i])]];
                    if (n > most) {
                        most = n;
                        target = static_cast<int32_t>(i);
                    }
                }
                break;
            }
            case ResolutionKind::UrbanDevelopment: {
                std::map<TypeIndex, int> count;
                for (const City& c : state_.cities) {
                    if (c.owner != me) continue;
                    for (const CityDistrict& d : c.districts) ++count[d.type];
                }
                int most = -1;
                for (size_t i = 0; i < item.candidates.size(); ++i) {
                    const int n = count[static_cast<TypeIndex>(item.candidates[i])];
                    if (n > most) {
                        most = n;
                        target = static_cast<int32_t>(i);
                    }
                }
                break;
            }
            case ResolutionKind::Unsupported: break;
        }
        // Extra votes where it matters most, while the favor lasts.
        int extra = 0;
        while (extraVoteCost(extra + 1) <= self.favor / 2 && extra < stake) ++extra;
        self.favor -= extraVoteCost(extra);
        item.votes.push_back({me, option, target, 1 + extra});
    }
}

}  // namespace sov
