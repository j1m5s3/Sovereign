// Governors (08-diplomacy-city-states-governors.md, Governors [R&F]; data: governors.md). Titles
// come from civics; each title appoints a governor (with its base ability) or buys a promotion.
// A governor assigned to a city takes 5 turns to establish (faster for Victor), then adds +8
// loyalty per turn there and its promotions apply to the city through the modifier model
// (ModSource::Governor). Amani may serve in a city-state, where she counts as envoys.
#include <algorithm>
#include <string_view>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
}  // namespace

int Game::governorTitles(PlayerId pid) const {
    const Player& p = state_.players[static_cast<size_t>(pid)];
    int n = 0;
    for (const auto& [civic, titles] : rules_->governorTitleCivics) n += p.civics.has(civic) ? titles : 0;
    const TypeIndex extra = civAbility(pid).extraGovernorTitleCivic;  // Persia
    if (extra != kNone && p.civics.has(extra)) ++n;
    return n + p.futureCivics;  // a title per Future Civic [GS] (04)
}

int Game::governorTitlesLeft(PlayerId pid) const {
    return governorTitles(pid) - state_.players[static_cast<size_t>(pid)].governorTitlesSpent;
}

const Governor* Game::governor(PlayerId pid, TypeIndex type) const {
    for (const Governor& g : state_.players[static_cast<size_t>(pid)].governors) {
        if (g.type == type) return &g;
    }
    return nullptr;
}

const Governor* Game::establishedGovernor(const City& city, PlayerId* owner) const {
    const Player& cityOwner = state_.players[static_cast<size_t>(city.owner)];
    auto find = [&](const Player& p) -> const Governor* {
        for (const Governor& g : p.governors) {
            if (g.city == city.id && g.establishTurns == 0) return &g;
        }
        return nullptr;
    };
    if (cityOwner.cityState == kNone) {
        if (owner) *owner = city.owner;
        return find(cityOwner);
    }
    for (const Player& p : state_.players) {  // a city-state: whoever's Amani serves there
        if (const Governor* g = find(p)) {
            if (owner) *owner = p.id;
            return g;
        }
    }
    return nullptr;
}

bool Game::cityGovernorHas(const City& city, const char* promotionId) const {
    PlayerId holder = kNoPlayer;
    const Governor* g = establishedGovernor(city, &holder);
    return g && holder == city.owner && governorHasPromotion(*g, promotionId);
}

bool Game::territoryGovernorHas(Hex at, PlayerId owner, const char* promotionId) const {
    const Plot& p = state_.plot(at);
    if (p.owner != owner || p.city == kNoCity) return false;
    const City* c = state_.city(p.city);
    return c && cityGovernorHas(*c, promotionId);
}

bool Game::governorHasPromotion(const Governor& g, const char* promotionId) const {
    // One of its promotions has that name (ids are unique), found without a search of them all.
    const std::string_view id(promotionId);
    return std::any_of(g.promotions.begin(), g.promotions.end(), [&](TypeIndex p) {
        return p >= 0 && at(p) < rules_->governorPromotions.size() && rules_->governorPromotions[at(p)].id == id;
    });
}

int Game::governorEstablishTurns(TypeIndex type) const {
    const int base = rules_->globalInt("GOVERNOR_BASE_TURNS_TO_ESTABLISH");
    return std::max(1, base * 100 / rules_->governors[at(type)].establishPercent);
}

bool Game::canAppointGovernor(PlayerId pid, TypeIndex type) const {
    if (type < 0 || at(type) >= rules_->governors.size() || !isMajorCiv(pid) || governor(pid, type)) return false;
    return governorTitlesLeft(pid) > 0;
}

bool Game::canPromoteGovernor(PlayerId pid, TypeIndex type, TypeIndex promotion) const {
    const Governor* g = governor(pid, type);
    if (!g || promotion < 0 || at(promotion) >= rules_->governorPromotions.size() || governorTitlesLeft(pid) <= 0) return false;
    const GovernorPromotionType& p = rules_->governorPromotions[at(promotion)];
    if (p.governor != type || std::find(g->promotions.begin(), g->promotions.end(), promotion) != g->promotions.end()) return false;
    // The tree: any one of its prerequisites (Civ VI draws the lines as alternatives).
    if (p.prerequisites.empty()) return true;
    return std::any_of(p.prerequisites.begin(), p.prerequisites.end(),
                       [&](TypeIndex r) { return std::find(g->promotions.begin(), g->promotions.end(), r) != g->promotions.end(); });
}

bool Game::canAssignGovernor(PlayerId pid, TypeIndex type, CityId cityId) const {
    const Governor* g = governor(pid, type);
    const City* c = state_.city(cityId);
    if (!g || !c || g->city == cityId) return false;
    if (c->owner != pid) {
        // Only a governor who serves city-states may go to one, met and at peace.
        const Player& cs = state_.players[static_cast<size_t>(c->owner)];
        if (!rules_->governors[at(type)].cityStates || cs.cityState == kNone || atWar(pid, c->owner)) return false;
        const auto& met = state_.players[static_cast<size_t>(pid)].met;
        bool seen = static_cast<size_t>(c->owner) < met.size() && met[static_cast<size_t>(c->owner)];
        seen = seen || visibility(pid, c->pos) != Visibility::Unrevealed;
        if (!seen) return false;
    }
    // One governor of a player per city.
    for (const Governor& o : state_.players[static_cast<size_t>(pid)].governors) {
        if (o.city == cityId) return false;
    }
    return true;
}

int Game::governorEnvoys(PlayerId pid, PlayerId cs) const {
    for (const Governor& g : state_.players[static_cast<size_t>(pid)].governors) {
        const City* c = state_.city(g.city);
        if (!c || c->owner != cs || g.establishTurns > 0) continue;
        return governorHasPromotion(g, "GOVERNOR_PROMOTION_PUPPETEER") ? 4 : 2;
    }
    return 0;
}

CommandError Game::validateGovernor(const Command& c) const {
    const TypeIndex type = static_cast<TypeIndex>(c.arg);
    switch (c.type) {
        case CommandType::AppointGovernor: return canAppointGovernor(c.player, type) ? CommandError::Ok : CommandError::CannotGovern;
        case CommandType::PromoteGovernor:
            return canPromoteGovernor(c.player, type, static_cast<TypeIndex>(c.arg2)) ? CommandError::Ok : CommandError::CannotGovern;
        case CommandType::AssignGovernor: return canAssignGovernor(c.player, type, c.id) ? CommandError::Ok : CommandError::CannotGovern;
        default: return CommandError::CannotGovern;
    }
}

void Game::applyGovernor(const Command& c) {
    Player& p = state_.players[static_cast<size_t>(c.player)];
    const TypeIndex type = static_cast<TypeIndex>(c.arg);
    switch (c.type) {
        case CommandType::AppointGovernor: {
            Governor g;
            g.type = type;
            g.promotions.push_back(rules_->governors[at(type)].promotions.front());  // the base ability
            p.governors.push_back(g);
            ++p.governorTitlesSpent;
            if (p.governors.size() >= rules_->governors.size()) awardOnce(c.player, "MOMENT_ALL_GOVERNORS_APPOINTED");  // 09
            return;
        }
        case CommandType::PromoteGovernor:
            for (Governor& g : p.governors) {
                if (g.type == type) {
                    g.promotions.push_back(static_cast<TypeIndex>(c.arg2));
                    if (g.promotions.size() >= rules_->governors[at(type)].promotions.size()) awardMoment(c.player, "MOMENT_GOVERNOR_FULLY_PROMOTED");  // 09
                }
            }
            ++p.governorTitlesSpent;
            return;
        case CommandType::AssignGovernor:
            for (Governor& g : p.governors) {
                if (g.type != type) continue;
                g.city = c.id;
                g.establishTurns = governorEstablishTurns(type);
            }
            return;
        default: return;
    }
}

void Game::processGovernors(PlayerId pid) {
    for (Governor& g : state_.players[static_cast<size_t>(pid)].governors) {
        if (g.city == kNoCity) continue;
        const City* c = state_.city(g.city);
        // A city lost (or razed, or a city-state we went to war with) sends its governor home.
        const bool kept = c && (c->owner == pid || (state_.players[static_cast<size_t>(c->owner)].cityState != kNone && !atWar(pid, c->owner)));
        if (!kept) {
            g.city = kNoCity;
            g.establishTurns = 0;
            continue;
        }
        if (g.establishTurns > 0) --g.establishTurns;
    }
}

}  // namespace sov
