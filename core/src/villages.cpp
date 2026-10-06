// Tribal villages (01-map-and-terrain.md, Tribal Villages; data: barbarians-goody-huts, Tribal village
// rewards). The first unit of a major civ to enter one consumes it and gains EXPERIENCE_ACTIVATE_GOODY_HUT
// XP; the reward is drawn in two stages: one of the categories with an eligible reward (equal weights),
// then a reward inside it by weight, skipping those whose minimum turn has not come and those that need a
// city the civ does not yet have.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

void Game::enterVillage(Unit& unit) {
    state_.plot(unit.pos).village = false;
    const PlayerId pid = unit.owner;
    Player& p = state_.players[at(pid)];
    unit.xp += rules_->globalInt("EXPERIENCE_ACTIVATE_GOODY_HUT");
    const City* nearest = nullptr;
    for (const City& c : state_.cities) {
        if (c.owner == pid && (!nearest || state_.grid.distance(c.pos, unit.pos) < state_.grid.distance(nearest->pos, unit.pos))) nearest = &c;
    }
    std::vector<const GoodyType*> eligible;
    for (const GoodyType& g : rules_->goodies) {
        if (state_.turn >= g.minTurn && (!g.needsCity || nearest)) eligible.push_back(&g);
    }
    if (eligible.empty()) return;
    std::vector<std::string> categories;
    for (const GoodyType* g : eligible) {
        if (std::find(categories.begin(), categories.end(), g->category) == categories.end()) categories.push_back(g->category);
    }
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    const std::string category = categories[rng.below(static_cast<uint32_t>(categories.size()))];
    int total = 0;
    for (const GoodyType* g : eligible) total += g->category == category ? g->weight : 0;
    int roll = static_cast<int>(rng.below(static_cast<uint32_t>(std::max(1, total))));
    const GoodyType* pick = nullptr;
    for (const GoodyType* g : eligible) {
        if (g->category != category) continue;
        roll -= g->weight;
        if (roll < 0) {
            pick = g;
            break;
        }
    }
    if (!pick) return;
    const int speed = rules_->speeds[at(rules_->speed(state_.setup.speed))].costPercent;
    const auto boost = [&](bool civic, int count) {
        TreeProgress& t = civic ? p.civics : p.techs;
        const std::vector<TypeIndex> open = civic ? availableCivics(pid) : availableTechs(pid);
        for (TypeIndex n : open) {
            if (count <= 0) break;
            if (t.boosted[at(n)]) continue;
            const Boost& b = (civic ? rules_->civics : rules_->techs)[at(n)].boost;
            const int pct = b.percent > 0 ? b.percent : 40;
            t.boosted[at(n)] = 1;
            t.progress[at(n)] += Fixed::fromInt(civic ? civicCost(n) : techCost(n)) * pct / 100;
            --count;
        }
    };
    switch (pick->kind) {
        case GoodyKind::Relic: {
            // A relic in a free relic slot of one of its cities; with none, its worth in Faith (Sovereign reading).
            TypeIndex relic = kNone;
            for (size_t w = 0; w < rules_->greatWorkTypes.size(); ++w) relic = rules_->greatWorkTypes[w].id == "RELIC" ? static_cast<TypeIndex>(w) : relic;
            bool placed = false;
            for (City& c : state_.cities) {
                if (placed || c.owner != pid || relic == kNone) continue;
                const TypeIndex slot = freeGreatWorkSlot(c, relic);
                if (slot == kNone) continue;
                GreatWork gw;
                gw.type = relic;
                gw.building = slot;
                c.greatWorks.push_back(gw);
                placed = true;
            }
            if (!placed) p.faith += Fixed::fromInt(60);
            break;
        }
        case GoodyKind::Inspiration: boost(true, pick->amount); break;
        case GoodyKind::Eureka: boost(false, pick->amount); break;
        case GoodyKind::GovernorTitle: --p.governorTitlesSpent; break;  // one more title to spend
        case GoodyKind::Envoy: ++p.envoyTokens; break;
        case GoodyKind::Favor: p.favor += pick->amount; break;
        case GoodyKind::Faith: p.faith += Fixed::fromInt(pick->amount); break;
        case GoodyKind::Gold: p.gold += Fixed::fromInt(pick->amount); break;
        case GoodyKind::Xp: unit.xp += pick->amount; break;
        case GoodyKind::Heal: unit.hp = std::min(rules_->globalInt("COMBAT_MAX_HIT_POINTS"), unit.hp + pick->amount); break;
        case GoodyKind::Strategic: {
            // The most advanced strategic resource it has revealed (latest reveal tech).
            TypeIndex best = kNone;
            int bestCost = -1;
            for (size_t r = 0; r < rules_->resources.size(); ++r) {
                const ResourceType& rt = rules_->resources[r];
                if (rt.cls != ResourceClass::Strategic || !hasUnlocked(pid, rt.reveal)) continue;
                const int cost = rt.reveal.none() ? 0 : (rt.reveal.civic ? rules_->civics : rules_->techs)[at(rt.reveal.index)].cost;
                if (cost > bestCost) {
                    bestCost = cost;
                    best = static_cast<TypeIndex>(r);
                }
            }
            if (best != kNone) p.stockpile[at(best)] += pick->amount * speed / 100;
            else p.gold += Fixed::fromInt(40);
            break;
        }
        case GoodyKind::Tech: {
            const std::vector<TypeIndex> open = availableTechs(pid);
            if (!open.empty()) {
                const TypeIndex t = open[rng.below(static_cast<uint32_t>(open.size()))];
                p.techs.progress[at(t)] = Fixed::fromInt(techCost(t));  // completes when research next runs
                p.techs.boosted[at(t)] = 1;
            }
            break;
        }
        case GoodyKind::Population:
            if (nearest) {
                City& c = *state_.city(nearest->id);
                ++c.population;
                assignCitizens(c);
            }
            break;
        case GoodyKind::Unit:
            if (nearest) {
                if (auto spot = unitSpawnPlot(*nearest, pick->unit)) spawnUnit(pick->unit, pid, *spot);
            }
            break;
    }
    pushEvent(EventKind::GoodyHut, pid, kNoPlayer, static_cast<int>(pick - rules_->goodies.data()));
}

}  // namespace sov
