// City-state quests (08-diplomacy-city-states-governors.md, Quests; data: diplomacy-espionage, City-state
// quests). Each living city-state keeps one open quest for each major civ that has met it, drawn from
// what that civ could do: convert its capital to the civ's religion, send it a trade route, clear a
// barbarian camp within 5 tiles of it, train a unit, build a district, trigger a Eureka or an
// Inspiration, or recruit a great person of a class. Fulfilling it puts an envoy there (the quest's
// reward) and a new quest follows on a later world turn.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

const Quest* Game::questFor(PlayerId cityState, PlayerId major) const {
    for (const Quest& q : state_.quests) {
        if (q.cityState == cityState && q.major == major) return &q;
    }
    return nullptr;
}

void Game::assignQuests() {
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    for (const Player& cs : state_.players) {
        if (!isCityState(cs.id) || !cs.alive) continue;
        const City* home = nullptr;
        for (const City& c : state_.cities) home = c.owner == cs.id ? &c : home;
        if (!home) continue;
        for (const Player& p : state_.players) {
            // Met: the major has seen the city-state's city (as for envoys).
            if (!isMajorCiv(p.id) || !p.alive || visibility(p.id, home->pos) == Visibility::Unrevealed || questFor(cs.id, p.id) || atWar(p.id, cs.id)) continue;
            std::vector<Quest> options;
            const auto add = [&](QuestKind k, int32_t arg) { options.push_back({cs.id, p.id, k, arg}); };
            if (p.religion >= 0 && cityMajorityReligion(*home) != p.religion) add(QuestKind::Convert, p.religion);
            if (tradeRoutesOf(p.id) < tradeRouteCapacity(p.id)) add(QuestKind::TradeRoute, cs.id);
            for (const Camp& camp : state_.camps) {
                if (state_.grid.distance(camp.pos, home->pos) <= 5) {
                    add(QuestKind::ClearCamp, camp.id);
                    break;
                }
            }
            // A unit, a district, a Eureka, an Inspiration it could get now, and a great person class.
            for (const City& c : state_.cities) {
                if (c.owner != p.id) continue;
                std::vector<int32_t> units, districts;
                for (const ProductionItem& it : buildableItems(c.id)) {
                    if (it.kind == ProductionKind::Unit && rules_->units[at(it.type)].layer == UnitLayer::Military) units.push_back(it.type);
                    if (it.kind == ProductionKind::District) districts.push_back(it.type);
                }
                if (!units.empty()) add(QuestKind::TrainUnit, units[rng.below(static_cast<uint32_t>(units.size()))]);
                if (!districts.empty()) add(QuestKind::BuildDistrict, districts[rng.below(static_cast<uint32_t>(districts.size()))]);
                break;  // the capital (first city) decides
            }
            for (int civic = 0; civic < 2; ++civic) {
                const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
                const TreeProgress& t = civic ? p.civics : p.techs;
                std::vector<int32_t> open;
                const std::vector<TypeIndex> avail = civic ? availableCivics(p.id) : availableTechs(p.id);
                for (TypeIndex n : avail) {
                    const Boost& b = nodes[at(n)].boost;
                    if (b.percent > 0 && b.kind != BoostKind::None && b.kind != BoostKind::NotTracked && !t.boosted[at(n)]) open.push_back(n);
                }
                if (!open.empty()) add(civic ? QuestKind::Inspiration : QuestKind::Eureka, open[rng.below(static_cast<uint32_t>(open.size()))]);
            }
            if (!rules_->greatPersonClasses.empty())
                add(QuestKind::GreatPerson, static_cast<int32_t>(rng.below(static_cast<uint32_t>(rules_->greatPersonClasses.size()))));
            if (!options.empty()) state_.quests.push_back(options[rng.below(static_cast<uint32_t>(options.size()))]);
        }
    }
}

void Game::questDone(PlayerId major, QuestKind kind, int32_t arg) {
    if (major < 0 || !isMajorCiv(major)) return;
    std::vector<PlayerId> rewarded;
    state_.quests.erase(std::remove_if(state_.quests.begin(), state_.quests.end(),
                                       [&](const Quest& q) {
                                           if (q.major != major || q.kind != kind || q.arg != arg) return false;
                                           rewarded.push_back(q.cityState);
                                           return true;
                                       }),
                        state_.quests.end());
    Player& p = state_.players[at(major)];
    if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
    if (policyIs(major, "POLICY_ROGUE_STATE")) return;  // Rogue State: no envoys (09)
    for (PlayerId cs : rewarded) ++p.envoys[at(cs)];  // the quest's reward: an envoy there
}

void Game::checkQuests() {
    std::vector<std::pair<PlayerId, Quest>> done;
    for (const Quest& q : state_.quests) {
        const Player& cs = state_.players[at(q.cityState)];
        if (!cs.alive) {
            done.push_back({kNoPlayer, q});
            continue;
        }
        bool met = false;
        if (q.kind == QuestKind::Convert) {
            for (const City& c : state_.cities) met = met || (c.owner == q.cityState && cityMajorityReligion(c) == q.arg);
        } else if (q.kind == QuestKind::TradeRoute) {
            for (const TradeRoute& r : state_.tradeRoutes) {
                const City* d = state_.city(r.destination);
                met = met || (r.owner == q.major && d && d->owner == q.cityState);
            }
        } else if (q.kind == QuestKind::ClearCamp) {
            // A camp cleared by someone else ends the quest without reward.
            const bool gone = std::none_of(state_.camps.begin(), state_.camps.end(), [&](const Camp& c) { return c.id == q.arg; });
            if (gone) done.push_back({kNoPlayer, q});
        }
        if (met) done.push_back({q.major, q});
    }
    for (const auto& entry : done) {
        const Quest q = entry.second;  // a copy: the lambda below may not capture a structured binding (C++17)
        if (entry.first != kNoPlayer) {
            questDone(entry.first, q.kind, q.arg);
        } else {
            state_.quests.erase(std::remove_if(state_.quests.begin(), state_.quests.end(),
                                               [&](const Quest& o) { return o.cityState == q.cityState && o.major == q.major; }),
                                state_.quests.end());
        }
    }
}

std::string Game::questText(const Quest& q) const {
    switch (q.kind) {
        case QuestKind::Convert: return "convert its city to your religion";
        case QuestKind::TradeRoute: return "send it a trade route";
        case QuestKind::ClearCamp: return "clear the barbarian camp near it";
        case QuestKind::TrainUnit: return "train a " + rules_->units[at(q.arg)].name;
        case QuestKind::BuildDistrict: return "build a " + rules_->districts[at(q.arg)].name;
        case QuestKind::Eureka: return "trigger the Eureka for " + rules_->techs[at(q.arg)].name;
        case QuestKind::Inspiration: return "trigger the Inspiration for " + rules_->civics[at(q.arg)].name;
        case QuestKind::GreatPerson: return "recruit a " + rules_->greatPersonClasses[at(q.arg)].name;
    }
    return "?";
}

}  // namespace sov
