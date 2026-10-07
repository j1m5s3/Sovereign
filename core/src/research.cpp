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

namespace {
// Nodes of eras before the world's cost less, of later eras more (04: [GS] TECH_COST_PERCENT_CHANGE_*).
int worldEraPercent(const GameState& s, const Rules& r, int nodeEra) {
    if (nodeEra < s.gameEra) return 100 + r.globalInt("TECH_COST_PERCENT_CHANGE_BEFORE_GAME_ERA");
    if (nodeEra > s.gameEra) return 100 + r.globalInt("TECH_COST_PERCENT_CHANGE_AFTER_GAME_ERA");
    return 100;
}
}  // namespace

int Game::techCost(TypeIndex tech) const {
    const TreeNode& n = rules_->techs[static_cast<size_t>(tech)];
    return std::max(1, n.cost * speedPercent(state_, *rules_) / 100 * worldEraPercent(state_, *rules_, n.era) / 100);
}

int Game::civicCost(TypeIndex civic) const {
    const TreeNode& n = rules_->civics[static_cast<size_t>(civic)];
    return std::max(1, n.cost * speedPercent(state_, *rules_) / 100 * worldEraPercent(state_, *rules_, n.era) / 100);
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

// Research and Cultural alliances, level 3: 10% of the ally's Science or Culture from its cities
// (08: alliance levels; the ally's own share does not count again).
Fixed Game::allianceShare(PlayerId player, YieldType yield) const {
    const AllianceType type = yield == YieldType::Science ? AllianceType::Research : yield == YieldType::Culture ? AllianceType::Cultural : AllianceType::None;
    if (type == AllianceType::None) return Fixed();
    Fixed share;
    for (const Player& ally : state_.players) {
        if (alliance(player, ally.id) != type || allianceLevel(player, ally.id) < 3 || ally.anarchyTurns > 0) continue;
        Fixed theirs;
        for (const City& c : state_.cities) {
            if (c.owner == ally.id) theirs += cityReport(c.id).yields[static_cast<size_t>(yield)];
        }
        share += theirs * Fixed::ratio(1, 10);
    }
    return share;
}

Game::Output Game::outputPerTurn(PlayerId player) const {
    Output out;
    const bool anarchy = state_.players[static_cast<size_t>(player)].anarchyTurns > 0;  // no science or culture
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        const Yields y = cityReport(c.id).yields;
        if (!anarchy) {
            out.science += y[idx(YieldType::Science)];
            out.culture += y[idx(YieldType::Culture)];
        }
        out.faith += y[idx(YieldType::Faith)];
    }
    if (!anarchy) {
        out.science += allianceShare(player, YieldType::Science);
        out.culture += allianceShare(player, YieldType::Culture);
    }
    return out;
}

bool Game::boostMet(PlayerId player, const Boost& b) const {
    std::optional<ImprovedPlots> improved;
    return boostMet(player, b, improved);
}

bool Game::boostMet(PlayerId player, const Boost& b, std::optional<ImprovedPlots>& improved) const {
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
    // A civ's unique unit stands in for the unit it replaces.
    auto unitIs = [&](TypeIndex type, TypeIndex ref) { return type == ref || rules_->units[static_cast<size_t>(type)].replaces == ref; };
    // The player's improved plots, counted on first need; a count from one of their lists.
    auto plots = [&]() -> const ImprovedPlots& {
        if (!improved) improved = improvedPlots(player);
        return *improved;
    };
    auto count = [](const std::vector<int>& by, TypeIndex k) { return k >= 0 && static_cast<size_t>(k) < by.size() ? by[static_cast<size_t>(k)] : 0; };
    auto wonderEra = [&](const BuildingType& bt) {
        return bt.unlock.none() ? 0 : (bt.unlock.civic ? rules_->civics : rules_->techs)[static_cast<size_t>(bt.unlock.index)].era;
    };
    switch (b.kind) {
        case BoostKind::None:
        case BoostKind::NotTracked:
        case BoostKind::KillWith:
        case BoostKind::KillUnit:
        case BoostKind::ClearCamp:
        case BoostKind::WarDeclaredOn:
        case BoostKind::CasusBelliWar:
        case BoostKind::Artifact:
        case BoostKind::NationalPark:
        case BoostKind::NaturalWonder:
            return false;  // events: Game::eventBoost
        case BoostKind::BarbarianKills: return p.barbarianKills >= b.count;
        case BoostKind::CoastalCity:
            return countCities([&](const City& c) { return isCoastal(state_, *rules_, c); }) > 0;
        case BoostKind::Building:
            return countCities([&](const City& c) { return cityHasBuilding(c, *rules_, b.ref); }) >= b.count;  // a civ's unique counts
        case BoostKind::OwnUnits:
            return countUnits([&](const Unit& u) { return unitIs(u.type, b.ref); }) >= b.count;
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
        case BoostKind::Improvement: return count(plots().byImprovement, b.ref) >= b.count;
        case BoostKind::ImprovementOnResource: return count(plots().onResource, b.ref) >= b.count;
        case BoostKind::ImprovedTiles: return plots().total >= b.count;
        case BoostKind::ImproveResource: return count(plots().byResource, b.ref) > 0;
        case BoostKind::District:
            return countCities([&](const City& c) { return c.district(b.ref, true) != nullptr; }) >= b.count;
        case BoostKind::SpecialtyDistricts: {
            std::vector<uint8_t> seen(rules_->districts.size(), 0);
            int kinds = 0;
            for (const City& c : state_.cities) {
                if (c.owner != player) continue;
                for (const CityDistrict& d : c.districts) {
                    const size_t k = static_cast<size_t>(d.type);
                    if (!d.complete || !rules_->districts[k].needsPopulation || seen[k]) continue;
                    seen[k] = 1;
                    ++kinds;
                }
            }
            return kinds >= b.count;
        }
        case BoostKind::TradeRoutes:
            return std::count_if(state_.tradeRoutes.begin(), state_.tradeRoutes.end(), [&](const TradeRoute& r) { return r.owner == player; }) >= b.count;
        case BoostKind::MetCivs:
        case BoostKind::MetCityStates: {
            int n = 0;
            for (const Player& o : state_.players) {
                if (o.id == player || static_cast<size_t>(o.id) >= p.met.size() || !p.met[static_cast<size_t>(o.id)]) continue;
                n += (b.kind == BoostKind::MetCivs ? isMajorCiv(o.id) : isCityState(o.id)) ? 1 : 0;
            }
            return n >= b.count;
        }
        case BoostKind::Pantheon: return p.pantheon != kNone;
        case BoostKind::Religion: return p.religion >= 0;
        case BoostKind::FollowingCities: {
            if (p.religion < 0) return false;
            int n = 0;
            for (const City& c : state_.cities) n += cityMajorityReligion(c) == p.religion ? 1 : 0;
            return n >= b.count;
        }
        case BoostKind::Alliance:
            for (const Player& o : state_.players) {
                if (o.id != player && allianceLevel(player, o.id) >= b.count) return true;
            }
            return false;
        case BoostKind::GreatPeople: {
            int n = 0;
            for (int k : p.greatPeopleRecruited) n += k;
            return n >= b.count;
        }
        case BoostKind::Corps:
        case BoostKind::Armies: {
            const uint8_t formation = b.kind == BoostKind::Corps ? 1 : 2;
            return countUnits([&](const Unit& u) { return u.formation == formation; }) >= b.count;
        }
        case BoostKind::DistrictAppeal:
            for (const City& c : state_.cities) {
                const CityDistrict* d = c.owner == player ? c.district(b.ref, true) : nullptr;
                if (d && plotAppeal(d->pos) >= b.count) return true;
            }
            return false;
        case BoostKind::ThemedBuildings: {
            int n = 0;
            for (const City& c : state_.cities) {
                if (c.owner != player) continue;
                for (TypeIndex bi : c.buildings) n += themed(c, bi) ? 1 : 0;
            }
            return n >= b.count;
        }
        case BoostKind::BuildingNextToMountain:
            // The building's district (the University's Campus) stands next to a Mountain.
            for (const City& c : state_.cities) {
                if (c.owner != player || !cityHasBuilding(c, *rules_, b.ref)) continue;
                const CityDistrict* d = c.district(rules_->buildings[static_cast<size_t>(b.ref)].districtType, true);
                if (!d) continue;
                for (const Hex& n : state_.grid.within(d->pos, 1)) {
                    if (rules_->terrains[static_cast<size_t>(state_.plot(n).terrain)].relief == Relief::Mountain) return true;
                }
            }
            return false;
        case BoostKind::Wonders:
        case BoostKind::WonderFromEra: {
            int n = 0;
            for (const City& c : state_.cities) {
                if (c.owner != player) continue;
                for (TypeIndex bi : c.buildings) {
                    const BuildingType& bt = rules_->buildings[static_cast<size_t>(bi)];
                    if (bt.wonder && (b.kind == BoostKind::Wonders || wonderEra(bt) >= b.count)) ++n;
                }
            }
            return n >= (b.kind == BoostKind::Wonders ? b.count : 1);
        }
        case BoostKind::UnitAndImprovement:
            if (countUnits([&](const Unit& u) { return unitIs(u.type, b.ref); }) == 0) return false;
            for (const Plot& pl : state_.plots) {
                if (pl.owner == player && pl.improvement == b.improvement && (b.resource == kNone || pl.resource == b.resource)) return true;
            }
            return false;
        case BoostKind::AirBaseAbroad: {
            // An Aerodrome or an Airstrip on another continent than the capital's (Sovereign: every landmass is one).
            const City* capital = nullptr;
            for (const City& c : state_.cities) capital = c.owner == player && c.capital ? &c : capital;
            if (!capital) return false;
            const int16_t home = state_.plot(capital->pos).continent;
            for (const City& c : state_.cities) {
                if (c.owner != player) continue;
                for (const CityDistrict& d : c.districts) {
                    if (d.complete && rules_->districts[static_cast<size_t>(d.type)].airSlots > 0 && state_.plot(d.pos).continent != home) return true;
                }
            }
            for (const Plot& pl : state_.plots) {
                if (pl.owner == player && pl.improvement != kNone && rules_->improvements[static_cast<size_t>(pl.improvement)].airSlots > 0 &&
                    pl.continent != home)
                    return true;
            }
            return false;
        }
        case BoostKind::Continents: {
            // Land of this many continents revealed (Sovereign: every landmass is one).
            std::vector<int16_t> seen;
            for (size_t i = 0; i < state_.plots.size() && i < p.visibility.size(); ++i) {
                const int16_t k = state_.plots[i].continent;
                if (k < 0 || p.visibility[i] == static_cast<uint8_t>(Visibility::Unrevealed) || std::find(seen.begin(), seen.end(), k) != seen.end()) continue;
                seen.push_back(k);
                if (static_cast<int>(seen.size()) >= b.count) return true;
            }
            return false;
        }
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

PolicySlot Game::policySlotType(PlayerId player, int slot) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (p.government == kNone) return PolicySlot::Wildcard;
    const GovernmentType& g = rules_->governments[static_cast<size_t>(p.government)];
    if (slot < g.totalSlots()) return slotType(g, slot);
    slot -= g.totalSlots();
    int extra[4] = {0, 0, 0, 0};
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (TypeIndex b : c.buildings) {
            for (size_t k = 0; k < 4; ++k) extra[k] += rules_->buildings[static_cast<size_t>(b)].policySlots[k];
        }
    }
    for (size_t k = 0; k < 4; ++k) {
        if (slot < extra[k]) return static_cast<PolicySlot>(k);
        slot -= extra[k];
    }
    return PolicySlot::Wildcard;
}

void Game::syncPolicySlots(PlayerId player) {
    Player& p = state_.players[static_cast<size_t>(player)];
    if (p.government == kNone) return;
    size_t want = static_cast<size_t>(rules_->governments[static_cast<size_t>(p.government)].totalSlots());
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (TypeIndex b : c.buildings) {
            for (int n : rules_->buildings[static_cast<size_t>(b)].policySlots) want += static_cast<size_t>(n);
        }
    }
    // World Ideology (World Congress): a Wildcard slot more (A) or fewer (B) under the chosen government.
    if (const PassedResolution* wi = passed(ResolutionKind::WorldIdeology); wi && wi->target == p.government)
        want = wi->option == 0 ? want + 1 : (want > 0 ? want - 1 : 0);
    if (p.policies.size() == want) return;
    p.policies.resize(want, kNone);
    // A wonder lost reorders the extra slots: a card left in a slot of another type comes out.
    for (size_t i = 0; i < p.policies.size(); ++i) {
        const TypeIndex pol = p.policies[i];
        if (pol == kNone) continue;
        const PolicySlot st = policySlotType(player, static_cast<int>(i));
        if (st != PolicySlot::Wildcard && st != rules_->policies[static_cast<size_t>(pol)].slot) p.policies[i] = kNone;
    }
}

bool Game::policyAvailable(PlayerId player, TypeIndex policy) const {
    if (!inRange(policy, rules_->policies.size())) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    const PolicyType& pt = rules_->policies[static_cast<size_t>(policy)];
    // Dark Age cards (09: Ages): in a Dark Age, while the world is in their era window.
    if (pt.darkAge) return p.age == Age::Dark && state_.gameEra >= pt.minEra && state_.gameEra <= pt.maxEra;
    // Legacy cards (04: PolicyToUnlock): a Wildcard carrying a government's bonus, once the player has left that government.
    static const std::pair<const char*, const char*> kLegacy[] = {
        {"POLICY_AUTOCRATIC_LEGACY", "GOVERNMENT_AUTOCRACY"},   {"POLICY_OLIGARCHIC_LEGACY", "GOVERNMENT_OLIGARCHY"},
        {"POLICY_REPUBLICAN_LEGACY", "GOVERNMENT_CLASSICAL_REPUBLIC"}, {"POLICY_MONARCHIC_LEGACY", "GOVERNMENT_MONARCHY"},
        {"POLICY_THEOCRATIC_LEGACY", "GOVERNMENT_THEOCRACY"},   {"POLICY_MERCANTILE_LEGACY", "GOVERNMENT_MERCHANT_REPUBLIC"},
        {"POLICY_FASCIST_LEGACY", "GOVERNMENT_FASCISM"},        {"POLICY_COMMUNIST_LEGACY", "GOVERNMENT_COMMUNISM"},
        {"POLICY_DEMOCRATIC_LEGACY", "GOVERNMENT_DEMOCRACY"}};
    for (const auto& [card, gov] : kLegacy) {
        if (pt.id != card) continue;
        const TypeIndex g = rules_->government(gov);
        return g != kNone && p.government != g && static_cast<size_t>(g) < p.governmentUses.size() && p.governmentUses[static_cast<size_t>(g)] > 0;
    }
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
        const PolicySlot st = policySlotType(player, slot);
        if (st != PolicySlot::Wildcard && st != rules_->policies[static_cast<size_t>(policy)].slot)
            return fail(CommandError::CannotSetPolicy);
    }
    if (why) *why = CommandError::Ok;
    return true;
}

// Out of a civic's free window, changes cost Gold, rising with the civics done (04). The formula is a Sovereign
// reading of the unverified globals: BASE + (INCREASE x civics)^1.5, rounded down to VISIBLE_DIVISOR.
int Game::policyChangeCost(PlayerId player) const {
    const Player& p = state_.players[static_cast<size_t>(player)];
    int64_t done = 0;
    for (uint8_t d : p.civics.done) done += d ? 1 : 0;
    const int64_t x = rules_->globalInt("POLICY_COST_INCREASE_TO_BE_EXPONENTED") * done;
    int64_t root = 0;
    while ((root + 1) * (root + 1) <= x) ++root;
    const int64_t cost = rules_->globalInt("POLICY_COST_BASE") + x * root;  // x^1.5, with the root rounded down
    const int64_t div = std::max(1, rules_->globalInt("POLICY_COST_VISIBLE_DIVISOR"));
    return static_cast<int>(cost / div * div);
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
        case CommandType::BuyPolicyChanges:
            if (p.government == kNone || p.anarchyTurns > 0 || p.interregnumTurns > 0 || p.freeChanges) return CommandError::ChangesLocked;
            if (p.gold < Fixed::fromInt(policyChangeCost(c.player))) return CommandError::NotEnoughGold;
            return CommandError::Ok;
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
            syncPolicySlots(c.player);
            if (first) p.freeChanges = true;
            break;
        }
        case CommandType::SetPolicy:
            p.policies[static_cast<size_t>(c.id)] = static_cast<TypeIndex>(c.arg);
            break;
        case CommandType::BuyPolicyChanges:
            p.gold -= Fixed::fromInt(policyChangeCost(c.player));
            p.freeChanges = true;
            break;
        default: break;
    }
}

// --------------------------------------------------------------- processing

void Game::completeNode(PlayerId pid, bool civic, TypeIndex node) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    TreeProgress& tree = civic ? p.civics : p.techs;
    // Future Tech and Future Civic repeat [GS] (04): +5% toward projects each time; +50 Diplomatic Favor and a
    // Governor title each time.
    if ((civic ? rules_->civics : rules_->techs)[static_cast<size_t>(node)].id == (civic ? "CIVIC_FUTURE_CIVIC" : "TECH_FUTURE_TECH")) {
        tree.progress[static_cast<size_t>(node)] = Fixed();
        if (tree.current == node) tree.current = kNone;
        if (civic) {
            ++p.futureCivics;
            p.favor += 50;
            p.freeChanges = true;
        } else {
            ++p.futureTechs;
        }
        return;
    }
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
        if (!policyIs(pid, "POLICY_ROGUE_STATE")) p.envoyTokens += rules_->civics[static_cast<size_t>(node)].envoys;  // 08: civics that grant envoys
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

// A Research alliance at level 2 (08; data: alliance research agreement, Amount=30): every 30 turns at standard
// speed, a random Eureka toward a tech the ally has researched or boosted and this civ has neither.
void Game::allianceEurekas(PlayerId pid) {
    const int period = std::max(1, 30 * speedPercent(state_, *rules_) / 100);
    Player& me = state_.players[static_cast<size_t>(pid)];
    for (size_t o = 0; o < me.relations.size() && o < state_.players.size(); ++o) {
        Relation& rel = me.relations[o];
        if (rel.alliance != AllianceType::Research || allianceLevel(pid, static_cast<PlayerId>(o)) < 2) {
            rel.sharedBoostTurns = 0;
            continue;
        }
        if (++rel.sharedBoostTurns < period) continue;
        rel.sharedBoostTurns = 0;
        const TreeProgress& mine = me.techs;
        const TreeProgress& theirs = state_.players[o].techs;
        std::vector<size_t> open;
        for (size_t t = 0; t < rules_->techs.size() && t < mine.done.size() && t < theirs.done.size(); ++t) {
            if ((theirs.done[t] || theirs.boosted[t]) && !mine.done[t] && !mine.boosted[t]) open.push_back(t);
        }
        if (!open.empty()) grantBoost(pid, false, open[state_.rng.get(RngStream::Gameplay).below(static_cast<uint32_t>(open.size()))]);
    }
}

void Game::updateBoosts(PlayerId pid) {
    std::optional<ImprovedPlots> improved;  // the player's improved plots, counted for the first boost that needs them
    for (int civic = 0; civic < 2; ++civic) {
        const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
        const TreeProgress& t = civic ? state_.players[static_cast<size_t>(pid)].civics : state_.players[static_cast<size_t>(pid)].techs;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const Boost& b = nodes[i].boost;
            if (b.percent <= 0 || b.kind == BoostKind::None || b.kind == BoostKind::NotTracked) continue;
            if (t.done[i] || t.boosted[i] || !boostMet(pid, b, improved)) continue;
            grantBoost(pid, civic != 0, i);
        }
    }
}

void Game::grantBoost(PlayerId pid, bool civic, size_t node) {
    TreeProgress& t = civic ? state_.players[static_cast<size_t>(pid)].civics : state_.players[static_cast<size_t>(pid)].techs;
    if (node >= t.done.size() || t.done[node] || t.boosted[node]) return;
    const Boost& b = (civic ? rules_->civics : rules_->techs)[node].boost;
    const int cost = civic ? civicCost(static_cast<TypeIndex>(node)) : techCost(static_cast<TypeIndex>(node));
    // Dedications (09): Free Inquiry (Eurekas) and Pen, Brush and Voice (Inspirations): +10 points in a
    // Golden Age, +1 era score otherwise.
    const char* const ded = civic ? "DEDICATION_PEN_BRUSH_AND_VOICE" : "DEDICATION_FREE_INQUIRY";
    const int pct = (b.percent > 0 ? b.percent : 40) + (goldenDedication(pid, ded) ? 10 : 0);
    t.boosted[node] = 1;
    t.progress[node] += Fixed::fromInt(cost) * pct / 100;
    dedicationScore(pid, ded, 1);
    questDone(pid, civic ? QuestKind::Inspiration : QuestKind::Eureka, static_cast<int32_t>(node));  // 08: Quests
}

// Boosts earned by an event as it happens (04): a kill with or of a unit type (its civ uniques stand in), a camp
// cleared, a war declared, an artifact, a National Park, a natural wonder.
void Game::eventBoost(PlayerId pid, BoostKind kind, TypeIndex ref) {
    if (!inRange(pid, state_.players.size())) return;
    const bool unitRef = kind == BoostKind::KillWith || kind == BoostKind::KillUnit;
    const TypeIndex base = unitRef && inRange(ref, rules_->units.size()) ? rules_->units[static_cast<size_t>(ref)].replaces : kNone;
    for (int civic = 0; civic < 2; ++civic) {
        const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const Boost& b = nodes[i].boost;
            if (b.percent > 0 && b.kind == kind && (!unitRef || b.ref == ref || (base != kNone && b.ref == base))) grantBoost(pid, civic != 0, i);
        }
    }
}

}  // namespace sov
