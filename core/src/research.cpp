// Research and government (specs/civ6/04-tech-civics-government.md): the tech
// and civic trees, boosts, governments, anarchy and policy cards.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
constexpr size_t idx(YieldType y) { return static_cast<size_t>(y); }

int speedPercent(const GameState& s, const Rules& r) {
    return r.speeds[static_cast<size_t>(r.speed(s.setup.speed))].costPercent;
}

bool inRange(int32_t i, size_t n) { return i >= 0 && static_cast<size_t>(i) < n; }

bool isCoastal(const GameState& s, const Rules& r, const City& c) {
    for (const Hex& n : s.grid.within(c.pos, 1)) {
        if (n != c.pos && r.terrains[static_cast<size_t>(s.plot(n).terrain)].shallowWater) return true;
    }
    return false;
}
}  // namespace

void Game::fitPlayerToRules(Player& p, const Rules& rules) {
    p.unitsTrained.resize(rules.units.size(), 0);
    p.techs.resize(rules.techs.size());
    p.civics.resize(rules.civics.size());
    p.governmentUses.resize(rules.governments.size(), 0);
    p.stockpile.resize(rules.resources.size(), 0);
    p.fuelShort.resize(rules.resources.size(), 0);
    p.greatPersonPoints.resize(rules.greatPersonClasses.size(), 0);
    p.greatPeopleRecruited.resize(rules.greatPersonClasses.size(), 0);
    p.projectsDone.resize(rules.projects.size(), 0);
    p.wmds.resize(rules.wmds.size(), 0);
    if (p.leaderName.empty() && p.cityState != kNone && static_cast<size_t>(p.cityState) < rules.cityStates.size())
        p.leaderName = rules.cityStates[static_cast<size_t>(p.cityState)].name;
    if (p.leaderName.empty() && !p.barbarian && p.civ >= 0 && static_cast<size_t>(p.civ) < rules.civs.size()) {
        const Dynasty* d = rules.dynastyOf(p.civ);
        p.leaderName = d ? d->names.front() : rules.civs[static_cast<size_t>(p.civ)].name;
    }
}

// ------------------------------------------------------------------ queries

int Game::techCost(TypeIndex tech) const {
    return std::max(1, rules_->techs[static_cast<size_t>(tech)].cost * speedPercent(state_, *rules_) / 100);
}

int Game::civicCost(TypeIndex civic) const {
    return std::max(1, rules_->civics[static_cast<size_t>(civic)].cost * speedPercent(state_, *rules_) / 100);
}

bool Game::hasUnlocked(PlayerId player, Unlock u) const {
    if (u.none()) return true;
    const Player& p = state_.players[static_cast<size_t>(player)];
    return (u.civic ? p.civics : p.techs).has(u.index);
}

bool Game::canResearch(PlayerId player, TypeIndex tech) const {
    if (!inRange(tech, rules_->techs.size())) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (p.techs.has(tech)) return false;
    for (TypeIndex pre : rules_->techs[static_cast<size_t>(tech)].prereqs) {
        if (!p.techs.has(pre)) return false;
    }
    return true;
}

bool Game::canStudyCivic(PlayerId player, TypeIndex civic) const {
    if (!inRange(civic, rules_->civics.size())) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (p.civics.has(civic)) return false;
    for (TypeIndex pre : rules_->civics[static_cast<size_t>(civic)].prereqs) {
        if (!p.civics.has(pre)) return false;
    }
    return true;
}

std::vector<TypeIndex> Game::availableTechs(PlayerId player) const {
    std::vector<TypeIndex> out;
    for (size_t i = 0; i < rules_->techs.size(); ++i) {
        if (canResearch(player, static_cast<TypeIndex>(i))) out.push_back(static_cast<TypeIndex>(i));
    }
    return out;
}

std::vector<TypeIndex> Game::availableCivics(PlayerId player) const {
    std::vector<TypeIndex> out;
    for (size_t i = 0; i < rules_->civics.size(); ++i) {
        if (canStudyCivic(player, static_cast<TypeIndex>(i))) out.push_back(static_cast<TypeIndex>(i));
    }
    return out;
}

Fixed Game::sciencePerTurn(PlayerId player) const {
    Fixed total;
    if (state_.players[static_cast<size_t>(player)].anarchyTurns > 0) return total;
    for (const City& c : state_.cities) {
        if (c.owner == player) total += cityReport(c.id).yields[idx(YieldType::Science)];
    }
    return total;
}

Fixed Game::culturePerTurn(PlayerId player) const {
    Fixed total;
    if (state_.players[static_cast<size_t>(player)].anarchyTurns > 0) return total;
    for (const City& c : state_.cities) {
        if (c.owner == player) total += cityReport(c.id).yields[idx(YieldType::Culture)];
    }
    return total;
}

bool Game::boostMet(PlayerId player, const Boost& b) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    auto countCities = [&](auto pred) {
        int n = 0;
        for (const City& c : state_.cities) {
            if (c.owner == player && pred(c)) ++n;
        }
        return n;
    };
    auto countUnits = [&](auto pred) {
        int n = 0;
        for (const Unit& u : state_.units) {
            if (u.owner == player && pred(u)) ++n;
        }
        return n;
    };
    switch (b.kind) {
        case BoostKind::None:
        case BoostKind::NotTracked:
            return false;
        case BoostKind::CoastalCity:
            return countCities([&](const City& c) { return isCoastal(state_, *rules_, c); }) > 0;
        case BoostKind::Building:
            return countCities([&](const City& c) { return c.has(b.ref); }) >= b.count;
        case BoostKind::OwnUnits:
            return countUnits([&](const Unit& u) { return u.type == b.ref; }) >= b.count;
        case BoostKind::Tech: return p.techs.has(b.ref);
        case BoostKind::Civic: return p.civics.has(b.ref);
        case BoostKind::GovernmentTier:
            return p.government != kNone && rules_->governments[static_cast<size_t>(p.government)].tier >= b.count;
        case BoostKind::TotalPopulation: {
            int pop = 0;
            for (const City& c : state_.cities) {
                if (c.owner == player) pop += c.population;
            }
            return pop >= b.count;
        }
        case BoostKind::CityPopulation:
            return countCities([&](const City& c) { return c.population >= b.count; }) > 0;
        case BoostKind::LandCombatUnits:
            return countUnits([&](const Unit& u) {
                       const UnitType& t = rules_->units[static_cast<size_t>(u.type)];
                       return t.domain == Domain::Land && t.layer == UnitLayer::Military && t.combat > 0;
                   }) >= b.count;
        case BoostKind::Improvement: return countImprovedPlots(player, b.ref, false) >= b.count;
        case BoostKind::ImprovementOnResource: return countImprovedPlots(player, b.ref, true) >= b.count;
        case BoostKind::ImprovedTiles: return countImprovedPlots(player, kNone, false) >= b.count;
        case BoostKind::ImproveResource:
            for (size_t i = 0; i < state_.plots.size(); ++i) {
                const Plot& pl = state_.plots[i];
                if (pl.owner == player && pl.resource == b.ref && pl.improvement != kNone &&
                    resourceImproved(state_.grid.at(static_cast<int>(i))))
                    return true;
            }
            return false;
    }
    return false;
}

bool Game::canAdoptGovernment(PlayerId player, TypeIndex gov, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (!inRange(gov, rules_->governments.size()) || gov == p.government || p.anarchyTurns > 0)
        return fail(CommandError::CannotAdoptGovernment);
    const GovernmentType& g = rules_->governments[static_cast<size_t>(gov)];
    // A government with no unlock (Chiefdom) arrives with the first civic.
    if (g.unlock.none()) {
        if (std::find(p.civics.done.begin(), p.civics.done.end(), 1) == p.civics.done.end())
            return fail(CommandError::CannotAdoptGovernment);
    } else if (!hasUnlocked(player, g.unlock)) {
        return fail(CommandError::CannotAdoptGovernment);
    }
    if (p.government != kNone && !p.freeChanges) return fail(CommandError::ChangesLocked);
    if (why) *why = CommandError::Ok;
    return true;
}

PolicySlot Game::slotType(const GovernmentType& g, int slot) {
    for (size_t k = 0; k < kNumGovernmentSlotTypes; ++k) {
        if (slot < g.slots[k]) return static_cast<PolicySlot>(k);
        slot -= g.slots[k];
    }
    return PolicySlot::Wildcard;
}

bool Game::policyAvailable(PlayerId player, TypeIndex policy) const {
    if (!inRange(policy, rules_->policies.size())) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    const PolicyType& pt = rules_->policies[static_cast<size_t>(policy)];
    if (pt.unlock.none() || !hasUnlocked(player, pt.unlock)) return false;
    for (TypeIndex replacement : pt.obsoletedBy) {
        const Unlock& u = rules_->policies[static_cast<size_t>(replacement)].unlock;
        if (!u.none() && hasUnlocked(player, u)) return false;
    }
    return pt.government == kNone || pt.government == p.government;
}

bool Game::canSetPolicy(PlayerId player, int slot, TypeIndex policy, CommandError* why) const {
    auto fail = [&](CommandError e) {
        if (why) *why = e;
        return false;
    };
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (p.government == kNone || p.anarchyTurns > 0) return fail(CommandError::CannotSetPolicy);
    if (p.interregnumTurns > 0) return fail(CommandError::ChangesLocked);  // leader doc §5
    if (!p.freeChanges) return fail(CommandError::ChangesLocked);
    if (!inRange(slot, p.policies.size())) return fail(CommandError::CannotSetPolicy);
    if (policy == kNone) {
        if (p.policies[static_cast<size_t>(slot)] == kNone) return fail(CommandError::CannotSetPolicy);
    } else {
        if (!policyAvailable(player, policy)) return fail(CommandError::CannotSetPolicy);
        if (std::find(p.policies.begin(), p.policies.end(), policy) != p.policies.end())
            return fail(CommandError::CannotSetPolicy);
        const PolicySlot st = slotType(rules_->governments[static_cast<size_t>(p.government)], slot);
        if (st != PolicySlot::Wildcard && st != rules_->policies[static_cast<size_t>(policy)].slot)
            return fail(CommandError::CannotSetPolicy);
    }
    if (why) *why = CommandError::Ok;
    return true;
}

// ---------------------------------------------------------------- commands

CommandError Game::validateResearch(const Command& c) const {
    const Player& p = state_.players[static_cast<size_t>(c.player)];
    CommandError why = CommandError::Ok;
    switch (c.type) {
        case CommandType::ChooseResearch:
            if (!inRange(c.id, rules_->techs.size()) || !canResearch(c.player, static_cast<TypeIndex>(c.id)) ||
                p.techs.current == c.id)
                return CommandError::CannotResearch;
            return CommandError::Ok;
        case CommandType::ChooseCivic:
            if (!inRange(c.id, rules_->civics.size()) || !canStudyCivic(c.player, static_cast<TypeIndex>(c.id)) ||
                p.civics.current == c.id)
                return CommandError::CannotResearch;
            return CommandError::Ok;
        case CommandType::ChangeGovernment:
            if (!inRange(c.id, rules_->governments.size())) return CommandError::CannotAdoptGovernment;
            canAdoptGovernment(c.player, static_cast<TypeIndex>(c.id), &why);
            return why;
        case CommandType::SetPolicy:
            if (c.arg != -1 && !inRange(c.arg, rules_->policies.size())) return CommandError::CannotSetPolicy;
            canSetPolicy(c.player, c.id, static_cast<TypeIndex>(c.arg), &why);
            return why;
        default: break;
    }
    return CommandError::BadTarget;
}

void Game::applyResearch(const Command& c) {
    Player& p = state_.players[static_cast<size_t>(c.player)];
    switch (c.type) {
        case CommandType::ChooseResearch: p.techs.current = static_cast<TypeIndex>(c.id); break;
        case CommandType::ChooseCivic: p.civics.current = static_cast<TypeIndex>(c.id); break;
        case CommandType::ChangeGovernment: {
            const TypeIndex gov = static_cast<TypeIndex>(c.id);
            const bool first = p.government == kNone;
            int& uses = p.governmentUses[static_cast<size_t>(gov)];
            // Returning to a government used before costs anarchy, longer each time
            // (Sovereign reading of GOVERNMENT_BASE_ANARCHY_TURNS; the spec's minimum is 3).
            if (uses > 0) p.anarchyTurns = rules_->globalInt("GOVERNMENT_BASE_ANARCHY_TURNS") + uses;
            ++uses;
            p.government = gov;
            if (const int tier = rules_->governments[static_cast<size_t>(gov)].tier; tier >= 1 && tier <= 4) {
                const std::string world = "MOMENT_FIRST_TIER_" + std::to_string(tier) + "_GOVERNMENT_IN_WORLD";
                const std::string own = "MOMENT_FIRST_TIER_" + std::to_string(tier) + "_GOVERNMENT";
                awardFirst(c.player, world.c_str(), own.c_str(), tier);
            }
            p.policies.assign(static_cast<size_t>(rules_->governments[static_cast<size_t>(gov)].totalSlots()), kNone);
            if (first) p.freeChanges = true;
            break;
        }
        case CommandType::SetPolicy:
            p.policies[static_cast<size_t>(c.id)] = static_cast<TypeIndex>(c.arg);
            break;
        default: break;
    }
}

// --------------------------------------------------------------- processing

void Game::completeNode(PlayerId pid, bool civic, TypeIndex node) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    TreeProgress& tree = civic ? p.civics : p.techs;
    {
        // The first tech or civic of a new era is a moment, a world's first for the first civ (09).
        const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
        const int era = nodes[static_cast<size_t>(node)].era;
        int before = 0;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (tree.done[i]) before = std::max(before, nodes[i].era);
        }
        if (era > before && era > 0) {
            awardFirst(pid, civic ? "MOMENT_WORLD_S_FIRST_CIVIC_OF_NEW_ERA" : "MOMENT_WORLD_S_FIRST_TECHNOLOGY_OF_NEW_ERA",
                       civic ? "MOMENT_FIRST_CIVIC_OF_NEW_ERA" : "MOMENT_FIRST_TECHNOLOGY_OF_NEW_ERA", era);
        }
    }
    tree.done[static_cast<size_t>(node)] = 1;
    if (tree.current == node) tree.current = kNone;
    if (civic) {
        // A finished civic opens a free window to change government and
        // policies, and retires obsolete cards.
        p.freeChanges = true;
        p.envoyTokens += rules_->civics[static_cast<size_t>(node)].envoys;  // 08: civics that grant envoys
        for (TypeIndex& slotted : p.policies) {
            if (slotted != kNone && !policyAvailable(pid, slotted)) slotted = kNone;
        }
    } else {
        // A tech may reveal resources, which changes the best plots to work.
        std::vector<CityId> ids;
        for (const City& c : state_.cities) {
            if (c.owner == pid) ids.push_back(c.id);
        }
        for (CityId id : ids) assignCitizens(*state_.city(id));
    }
}

void Game::processResearch(PlayerId pid, Fixed science, Fixed culture) {
    auto advance = [&](bool civic, Fixed amount) {
        TreeProgress& tree = civic ? state_.players[static_cast<size_t>(pid)].civics
                                   : state_.players[static_cast<size_t>(pid)].techs;
        if (tree.current == kNone) {
            tree.overflow += amount;
            return;
        }
        const TypeIndex node = tree.current;
        Fixed& progress = tree.progress[static_cast<size_t>(node)];
        progress += amount + tree.overflow;
        tree.overflow = Fixed();
        const Fixed cost = Fixed::fromInt(civic ? civicCost(node) : techCost(node));
        if (progress >= cost) {
            tree.overflow = progress - cost;
            progress = cost;
            completeNode(pid, civic, node);
        }
    };
    advance(false, science);
    advance(true, culture);
}

void Game::updateBoosts(PlayerId pid) {
    for (int civic = 0; civic < 2; ++civic) {
        const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const Boost& b = nodes[i].boost;
            if (b.percent <= 0 || b.kind == BoostKind::None || b.kind == BoostKind::NotTracked) continue;
            TreeProgress& t = civic ? state_.players[static_cast<size_t>(pid)].civics
                                    : state_.players[static_cast<size_t>(pid)].techs;
            if (t.done[i] || t.boosted[i] || !boostMet(pid, b)) continue;
            const int cost = civic ? civicCost(static_cast<TypeIndex>(i)) : techCost(static_cast<TypeIndex>(i));
            t.boosted[i] = 1;
            t.progress[i] += Fixed::fromInt(cost) * b.percent / 100;
        }
    }
}

}  // namespace sov
