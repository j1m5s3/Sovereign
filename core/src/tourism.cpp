// Tourism from the land (07-great-people-great-works-tourism.md: Tourism sources, National Parks, Seaside
// Resorts and Ski Resorts, Rock Bands; data: Improvement_Tourism, NATIONAL_PARK_*, Rock Band results).
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

// Improvements with a tourism source give it once their tech is known: resorts their plot's appeal, after
// Flight pastures, plantations and others their Culture (or Faith, Food) (07; generated `tourism`).
int Game::improvementTourism(PlayerId player) const {
    int total = 0;
    const bool cristo = holdsWonder(player, W::Cristo);  // Cristo Redentor (03): Seaside Resorts double
    for (int i = 0; i < state_.grid.size(); ++i) {
        const Plot& p = state_.plots[at(i)];
        if (p.owner != player || p.improvement == kNone || p.pillagedTurns > 0 || p.city == kNoCity) continue;
        const ImprovementType& im = rules_->improvements[at(p.improvement)];
        if (im.tourismSource.empty() || (!im.tourismAfter.none() && !hasUnlocked(player, im.tourismAfter))) continue;
        const Hex h = state_.grid.at(i);
        int amount = 0;
        if (im.tourismSource == "APPEAL") {
            amount = std::max(0, plotAppeal(h));
        } else {
            const City* c = state_.city(p.city);
            if (!c) continue;
            const Yields y = plotYields(h, *c);
            const YieldType t = im.tourismSource == "FAITH" ? YieldType::Faith : im.tourismSource == "FOOD" ? YieldType::Food
                              : im.tourismSource == "GOLD" ? YieldType::Gold : YieldType::Culture;
            amount = static_cast<int>(y[static_cast<size_t>(t)].toInt());
        }
        total += amount * im.tourismPercent / 100 * (cristo && im.id == "IMPROVEMENT_SEASIDE_RESORT" ? 2 : 1);
    }
    return total;
}

// A National Park (07): the Naturalist's plot and three beside it in a diamond (the plot, a neighbour, and
// the two plots beside both), all owned by one of its civ's cities, each Charming or better (appeal 2+),
// with no city, district, wonder or improvement.
std::optional<std::array<Hex, 4>> Game::parkPlots(UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u) return std::nullopt;
    return parkPlotsAt(u->owner, u->pos);
}

std::optional<std::array<Hex, 4>> Game::parkPlotsAt(PlayerId player, Hex here) const {
    const auto fits = [&](Hex h, CityId city) {
        const Plot& p = state_.plot(h);
        return p.owner == player && p.city == city && city != kNoCity && !p.park && p.improvement == kNone && !state_.cityAt(h) &&
               !state_.districtAt(h) && state_.wonderAt(h) == kNone && isLandPassable(state_, *rules_, h) && plotAppeal(h) >= 2;
    };
    const CityId city = state_.plot(here).city;
    if (!fits(here, city)) return std::nullopt;
    for (int d = 0; d < kNumDirs; ++d) {
        const auto a = state_.grid.neighbor(here, static_cast<Dir>(d));
        const auto b = state_.grid.neighbor(here, static_cast<Dir>((d + 1) % kNumDirs));
        if (!a || !b) continue;
        // The fourth plot touches both a and b, away from the Naturalist's plot.
        std::optional<Hex> c;
        for (const Hex& n : state_.grid.within(*a, 1)) {
            if (n != here && n != *a && n != *b && state_.grid.distance(n, *b) == 1) c = n;
        }
        if (c && fits(*a, city) && fits(*b, city) && fits(*c, city)) return std::array<Hex, 4>{here, *a, *b, *c};
    }
    return std::nullopt;
}

CommandError Game::parkProblem(PlayerId player, UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (rules_->units[at(u->type)].id != "UNIT_NATURALIST" || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    return parkPlots(id) ? CommandError::Ok : CommandError::BadTarget;
}

void Game::designatePark(UnitId id) {
    const auto plots = parkPlots(id);
    if (plots) for (const Hex& h : *plots) state_.plot(h).park = true;
    removeUnit(id);
}

// Tourism equal to the appeal of its plots (07).
int Game::parkTourism(PlayerId player) const {
    int total = 0;
    for (int i = 0; i < state_.grid.size(); ++i) {
        const Plot& p = state_.plots[at(i)];
        if (p.park && p.owner == player) total += std::max(0, plotAppeal(state_.grid.at(i)));
    }
    return total;
}

// Amenities (07; data: NATIONAL_PARK_AMENITIES_OWNING_CITY, NATIONAL_PARK_NUM_OTHER_AMENITY_CITIES): each park
// gives its city 2, and 1 to each of that civ's NUM_OTHER nearest other cities (Sovereign reading of "nearby").
int Game::parkAmenities(const City& city) const {
    int total = 0;
    for (const City& holder : state_.cities) {
        if (holder.owner != city.owner) continue;
        int plots = 0;
        for (int i = 0; i < state_.grid.size(); ++i) plots += state_.plots[at(i)].park && state_.plots[at(i)].city == holder.id ? 1 : 0;
        const int parks = plots / 4;
        if (parks == 0) continue;
        if (holder.id == city.id) {
            total += parks * rules_->globalInt("NATIONAL_PARK_AMENITIES_OWNING_CITY");
            continue;
        }
        // Is this city among the holder's nearest others?
        std::vector<std::pair<int, CityId>> near;
        for (const City& o : state_.cities) {
            if (o.owner == holder.owner && o.id != holder.id) near.push_back({state_.grid.distance(o.pos, holder.pos), o.id});
        }
        std::sort(near.begin(), near.end());
        const size_t n = std::min(near.size(), static_cast<size_t>(std::max(0, rules_->globalInt("NATIONAL_PARK_NUM_OTHER_AMENITY_CITIES"))));
        for (size_t k = 0; k < n; ++k) total += near[k].second == city.id ? parks : 0;
    }
    return total;
}

// A Rock Band concert [GS] (07; data: Rock Band results): in a foreign major's city center, district or
// wonder plot. The outcome is drawn by the results' base probabilities, each level of the band moving 2 points
// from the two worst outcomes to the two best. The band's owner gains its album sales plus its tourism bomb as
// tourism toward the city's civ (a negative bomb takes from the sales; Sovereign reading); a band may gain a
// level (up to ROCK_BAND_MAX_LEVEL) or break up. Its level is kept in its XP (it has no promotions).
CommandError Game::concertProblem(PlayerId player, UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (rules_->units[at(u->type)].id != "UNIT_ROCK_BAND" || u->movesLeft <= Fixed() || rules_->rockBandResults.empty()) return CommandError::BadUnit;
    const Plot& p = state_.plot(u->pos);
    if (p.owner == kNoPlayer || p.owner == player || !isMajorCiv(p.owner) || atWar(player, p.owner)) return CommandError::BadTarget;
    const bool resort = p.improvement != kNone && rules_->improvements[at(p.improvement)].tourismSource == "APPEAL";
    const bool natural = p.feature != kNone && rules_->features[at(p.feature)].naturalWonder;
    if (!state_.cityAt(u->pos) && !state_.districtAt(u->pos) && state_.wonderAt(u->pos) == kNone && !p.park && !resort && !natural) return CommandError::BadTarget;
    return CommandError::Ok;
}

void Game::grantApostlePromotion(Unit& apostle) {
    // Mont St. Michel (03: Wonders): every Apostle is also a Martyr.
    const TypeIndex martyr = buildingsOwned(apostle.owner, "BUILDING_MONT_ST_MICHEL") > 0 ? rules_->promotion("PROMOTION_MARTYR") : kNone;
    if (martyr != kNone) apostle.promotions.push_back(martyr);
    std::vector<TypeIndex> open;
    for (size_t i = 0; i < rules_->promotions.size(); ++i) {
        if (rules_->promotions[i].promotionClass == "PROMOTION_CLASS_RELIGIOUS_APOSTLE" && static_cast<TypeIndex>(i) != martyr)
            open.push_back(static_cast<TypeIndex>(i));
    }
    if (open.empty()) return;
    const TypeIndex pick = open[state_.rng.get(RngStream::Gameplay).below(static_cast<uint32_t>(open.size()))];
    apostle.promotions.push_back(pick);
    apostle.charges += unitEffectTotal(apostle, UnitEffectKind::SpreadCharges);  // Orator
}

void Game::grantBandPromotion(Unit& band) {
    std::vector<TypeIndex> open;
    for (size_t i = 0; i < rules_->promotions.size(); ++i) {
        if (rules_->promotions[i].promotionClass != "PROMOTION_CLASS_ROCK_BAND") continue;
        if (std::find(band.promotions.begin(), band.promotions.end(), static_cast<TypeIndex>(i)) == band.promotions.end()) open.push_back(static_cast<TypeIndex>(i));
    }
    if (open.empty()) return;
    band.promotions.push_back(open[state_.rng.get(RngStream::Gameplay).below(static_cast<uint32_t>(open.size()))]);
}

void Game::performConcert(UnitId id) {
    Unit& u = *state_.unit(id);
    const PlayerId host = state_.plot(u.pos).owner;
    const std::vector<RockBandResult>& results = rules_->rockBandResults;
    std::vector<int> weights;
    for (const RockBandResult& r : results) weights.push_back(r.probability);
    // The band's promotions (07): extra levels and a tourism burst where this concert is held.
    std::vector<std::string> places;
    if (state_.cityAt(u.pos)) places.push_back("DISTRICT_CITY_CENTER");
    if (const CityDistrict* d = state_.districtAt(u.pos)) places.push_back(rules_->districts[at(d->type)].id);
    if (state_.wonderAt(u.pos) != kNone) places.push_back("WONDER");
    const Plot& here = state_.plot(u.pos);
    if (here.park) places.push_back("NATIONAL_PARK");
    if (here.improvement != kNone) places.push_back(rules_->improvements[at(here.improvement)].id);
    if (here.feature != kNone && rules_->features[at(here.feature)].naturalWonder) places.push_back("NATURAL_WONDER");
    int bonusLevels = 0, burst = 0;
    for (TypeIndex pr : u.promotions) {
        for (const UnitEffect& e : rules_->promotions[at(pr)].effects) {
            if (std::find(places.begin(), places.end(), e.at) == places.end()) continue;
            if (e.kind == UnitEffectKind::BandLevel) bonusLevels += e.amount;
            if (e.kind == UnitEffectKind::BandBurst) burst += e.amount;
        }
    }
    const int level = 1 + u.xp + bonusLevels;
    for (int l = 1; l < level; ++l) {
        for (size_t k = 0; k < 2 && k < weights.size(); ++k) weights[k] += 2;
        for (size_t k = 0; k < 2 && k + 2 <= weights.size(); ++k) weights[weights.size() - 1 - k] = std::max(1, weights[weights.size() - 1 - k] - 2);
    }
    int total = 0;
    for (int w : weights) total += w;
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    int roll = static_cast<int>(rng.below(static_cast<uint32_t>(std::max(1, total))));
    size_t pick = 0;
    for (; pick + 1 < weights.size() && roll >= weights[pick]; ++pick) roll -= weights[pick];
    const RockBandResult& r = results[pick];
    Player& owner = state_.players[at(u.owner)];
    if (owner.tourismTo.size() < state_.players.size()) owner.tourismTo.resize(state_.players.size(), 0);
    int earned = std::max(0, r.albumSales + r.tourismBomb) + burst;
    // Flower Power (09): +50% concert tourism while at peace.
    if (policyIs(u.owner, "POLICY_FLOWER_POWER") && std::none_of(state_.players.begin(), state_.players.end(), [&](const Player& o) { return atWar(u.owner, o.id); }))
        earned = earned * 3 / 2;
    owner.tourismTo[at(host)] += earned;
    // Promotions acting on the city the concert is held in (07): Religious Rock converts it to the band's religion,
    // Indie costs it 40 loyalty.
    if (City* venue = state_.city(here.city); venue && venue->owner != u.owner) {
        for (TypeIndex pr : u.promotions) {
            const std::string& pid = rules_->promotions[at(pr)].id;
            if (pid == "PROMOTION_RELIGIOUS_ROCK" && owner.religion >= 0 && static_cast<size_t>(owner.religion) < state_.religions.size()) {
                if (venue->pressure.size() < state_.religions.size()) venue->pressure.resize(state_.religions.size(), 0);
                int32_t top = 0;
                for (int32_t pv : venue->pressure) top = std::max(top, pv);
                venue->pressure[static_cast<size_t>(owner.religion)] = top + 100;
            }
            if (pid == "PROMOTION_INDIE") venue->loyalty = std::max(0, venue->loyalty - 40);
        }
    }
    if (r.gainsLevel && 1 + u.xp < rules_->globalInt("ROCK_BAND_MAX_LEVEL")) ++u.xp;
    if (r.extraPromotion) grantBandPromotion(u);
    u.movesLeft = Fixed();
    if (r.dies) removeUnit(id);
}

}  // namespace sov
