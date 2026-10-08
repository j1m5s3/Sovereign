// Tourism from the land (07-great-people-great-works-tourism.md: Tourism sources, National Parks, Seaside
// Resorts and Ski Resorts, Rock Bands).
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

// Two civs; player 0's capital at (4,6) sits in woods; the sea to the north (row 0 and 1).
GameState landState() {
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(2, uint8_t{1});
    }
    s.majorsAtStart = 2;
    for (int x = 0; x < 24; ++x) {
        for (int y = 0; y < 2; ++y) s.plot({x, y}).terrain = rules().terrain("TERRAIN_COAST");
    }
    for (int x = 2; x < 9; ++x) {
        for (int y = 3; y < 10; ++y) s.plot({x, y}).feature = rules().feature("FEATURE_FOREST");
    }
    const CityId mine = addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {16, 6}, true, 6);
    for (const Hex& h : s.grid.within({4, 6}, 2)) {  // two rings of land
        s.plot(h).owner = 0;
        s.plot(h).city = mine;
    }
    return s;
}
}  // namespace

// Each of a park's four plots must fit: with one the park would take held by another civ instead, it goes on four
// plots of the civ's own.
TEST(every_plot_of_a_national_park_must_fit) {
    GameState s = landState();
    const UnitId naturalist = addUnit(s, "UNIT_NATURALIST", 0, {5, 7});
    const auto first = Game::fromScenario(rules(), s)->parkPlots(naturalist);
    REQUIRE(first);
    for (size_t i = 1; i < first->size(); ++i) {
        GameState t = s;
        t.plot((*first)[i]).owner = 1;
        auto g = Game::fromScenario(rules(), t);
        const auto plots = g->parkPlots(naturalist);
        REQUIRE(plots);
        for (const Hex& h : *plots) CHECK_EQ(g->state().plot(h).owner, 0);
    }
}

TEST(improvements_with_a_tourism_source_from_the_data) {
    const Rules& r = rules();
    const ImprovementType& resort = r.improvements[at(r.improvement("IMPROVEMENT_SEASIDE_RESORT"))];
    CHECK_EQ(resort.tourismSource, std::string("APPEAL"));
    CHECK_EQ(resort.minAppeal, 4);
    CHECK(resort.coastal);
    const ImprovementType& pasture = r.improvements[at(r.improvement("IMPROVEMENT_PASTURE"))];
    CHECK_EQ(pasture.tourismSource, std::string("CULTURE"));
    CHECK(!pasture.tourismAfter.none());
    CHECK_EQ(r.rockBandResults.size(), 6u);
}

TEST(a_national_park_draws_tourism_and_cheers_its_city) {
    GameState s = landState();
    const UnitId naturalist = addUnit(s, "UNIT_NATURALIST", 0, {5, 7});
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->plotAppeal({5, 7}) >= 2);
    const auto plots = g->parkPlots(naturalist);
    REQUIRE(plots);
    const int amenities = g->cityReport(g->state().cities[0].id).amenities;
    const int tourism = g->tourismPerTurn(0);
    const size_t radio = at(rules().tech("TECH_RADIO"));
    CHECK_EQ(g->state().players[0].techs.boosted[radio], 0);
    REQUIRE(g->submit(Command::designatePark(0, naturalist)) == CommandError::Ok);
    CHECK(!g->state().unit(naturalist));
    CHECK(sovtest::hasMoment(*g, 0, "MOMENT_WORLD_S_FIRST_NATIONAL_PARK"));  // 09
    CHECK_EQ(g->state().players[0].techs.boosted[radio], 1);                 // 04: Radio's Eureka
    int appeal = 0;
    for (const Hex& h : *plots) {
        CHECK(g->state().plot(h).park);
        appeal += g->plotAppeal(h);
    }
    CHECK_EQ(g->tourismPerTurn(0), tourism + appeal);
    CHECK_EQ(g->cityReport(g->state().cities[0].id).amenities, amenities + rules().globalInt("NATIONAL_PARK_AMENITIES_OWNING_CITY"));
    CHECK(!g->canImproveAt(0, (*plots)[1], rules().improvement("IMPROVEMENT_FARM")));  // the park stays as it is
    // A game made from a state that has the park already (a scenario, a saved game) counts it the same.
    auto again = Game::fromScenario(rules(), g->state());
    CHECK_EQ(again->tourismPerTurn(0), tourism + appeal);
    CHECK_EQ(again->cityReport(again->state().cities[0].id).amenities, amenities + rules().globalInt("NATIONAL_PARK_AMENITIES_OWNING_CITY"));
}

TEST(a_national_park_cheers_its_civs_nearest_other_cities) {
    GameState s = landState();
    std::vector<CityId> others;  // player 0's other cities, ever farther from the park's city
    for (int x = 7; x <= 15; x += 2) others.push_back(addCity(s, 0, {x, 10}, false));
    for (size_t k = 1; k < others.size(); ++k) REQUIRE(s.grid.distance({4, 6}, {5 + 2 * static_cast<int>(k), 10}) < s.grid.distance({4, 6}, {7 + 2 * static_cast<int>(k), 10}));
    const UnitId naturalist = addUnit(s, "UNIT_NATURALIST", 0, {5, 7});
    auto g = Game::fromScenario(rules(), s);
    std::vector<int> before;
    for (const City& c : g->state().cities) before.push_back(g->cityReport(c.id).amenities);
    REQUIRE(g->submit(Command::designatePark(0, naturalist)) == CommandError::Ok);
    const size_t nearest = static_cast<size_t>(rules().globalInt("NATIONAL_PARK_NUM_OTHER_AMENITY_CITIES"));
    REQUIRE(nearest + 1 == others.size());
    for (size_t i = 0; i < g->state().cities.size(); ++i) {
        const City& c = g->state().cities[i];
        const size_t rank = static_cast<size_t>(std::find(others.begin(), others.end(), c.id) - others.begin());
        int gain = rank < nearest ? 1 : 0;  // the farthest of player 0's cities and player 1's city gain nothing
        if (c.owner == 0 && c.capital) gain = rules().globalInt("NATIONAL_PARK_AMENITIES_OWNING_CITY");
        CHECK_EQ(g->cityReport(c.id).amenities, before[i] + gain);
    }
}

TEST(naturalists_and_rock_bands_are_bought_with_faith_without_a_religion) {
    GameState s = landState();
    s.players[0].civics.done[at(rules().civic("CIVIC_CONSERVATION"))] = 1;
    s.players[0].civics.done[at(rules().civic("CIVIC_COLD_WAR"))] = 1;
    auto g = Game::fromScenario(rules(), s);
    const City& c = g->state().cities[0];
    CHECK(g->faithPurchaseCost(0, c, {ProductionKind::Unit, rules().unit("UNIT_NATURALIST")}) > 0);
    CHECK(g->faithPurchaseCost(0, c, {ProductionKind::Unit, rules().unit("UNIT_ROCK_BAND")}) > 0);
}

TEST(a_rock_band_plays_a_foreign_city_for_tourism) {
    GameState s = landState();
    const UnitId band = addUnit(s, "UNIT_ROCK_BAND", 0, {16, 6});  // in player 1's capital
    const UnitId home = addUnit(s, "UNIT_ROCK_BAND", 0, {4, 6});
    auto g = Game::fromScenario(rules(), s);
    CHECK(g->submit(Command::performConcert(0, home)) == CommandError::BadTarget);  // not at home
    REQUIRE(g->submit(Command::performConcert(0, band)) == CommandError::Ok);
    const auto& to = g->state().players[0].tourismTo;
    const bool played = to.size() > 1 && to[1] >= 0;
    CHECK(played);
    const Unit* after = g->state().unit(band);
    CHECK(!after || after->movesLeft == Fixed());
}

TEST(a_bands_promotion_adds_its_burst_where_it_plays) {
    GameState s = landState();
    CityDistrict campus;
    campus.type = rules().district("DISTRICT_CAMPUS");
    campus.pos = {17, 6};
    campus.complete = true;
    s.cities[1].districts.push_back(campus);
    s.plot({17, 6}).owner = 1;
    s.plot({17, 6}).city = s.cities[1].id;
    const UnitId band = addUnit(s, "UNIT_ROCK_BAND", 0, {17, 6});
    s.units.back().promotions.push_back(rules().promotion("PROMOTION_SPACE_ROCK"));
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex space = rules().promotion("PROMOTION_SPACE_ROCK");
    bool burst = false;
    for (const UnitEffect& e : rules().promotions[at(space)].effects) burst = burst || (e.kind == UnitEffectKind::BandBurst && e.at == "DISTRICT_CAMPUS");
    REQUIRE(burst);
    REQUIRE(g->submit(Command::performConcert(0, band)) == CommandError::Ok);
    CHECK(g->state().players[0].tourismTo[1] >= 500);
}

// Hallyu (04): a Rock Band chooses its promotion (Promote) instead of drawing one, when bought and when a concert
// earns another. Choosing ends its turn and keeps its level, which its XP counts (07).
TEST(hallyu_lets_rock_bands_choose_their_promotions) {
    const auto slot = [](GameState& s, bool hallyu) {
        Player& p = s.players[0];
        p.government = rules().government("GOVERNMENT_CHIEFDOM");
        p.policies.assign(static_cast<size_t>(rules().governments[at(p.government)].totalSlots()), kNone);
        if (hallyu) p.policies[0] = rules().policy("POLICY_HALLYU");
    };
    size_t bandPromotions = 0;
    for (const PromotionType& pr : rules().promotions) bandPromotions += pr.promotionClass == "PROMOTION_CLASS_ROCK_BAND" ? 1 : 0;
    const TypeIndex glam = rules().promotion("PROMOTION_GLAM_ROCK"), indie = rules().promotion("PROMOTION_INDIE");
    const auto bought = [&](bool hallyu) {
        GameState s = landState();
        s.players[0].civics.done[at(rules().civic("CIVIC_COLD_WAR"))] = 1;
        s.players[0].faith = Fixed::fromInt(1000);
        slot(s, hallyu);
        auto g = Game::fromScenario(rules(), std::move(s));
        const ProductionItem band{ProductionKind::Unit, rules().unit("UNIT_ROCK_BAND")};
        REQUIRE(g->submit(Command::purchaseWithFaith(0, g->state().cities[0].id, band)) == CommandError::Ok);
        return g;
    };
    auto drawn = bought(false);
    const Unit& d = drawn->state().units.back();
    CHECK_EQ(d.promotions.size(), 1u);  // drawn (07)
    CHECK(drawn->availablePromotions(d.id).empty());

    auto g = bought(true);
    const UnitId band = g->state().units.back().id;
    CHECK(g->state().unit(band)->promotions.empty());
    CHECK_EQ(g->availablePromotions(band).size(), bandPromotions);  // any of them
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().unit(band)->promotionPicks, 1);  // the choice is saved
    REQUIRE(g->submit(Command::promote(0, band, glam)) == CommandError::Ok);
    const Unit& chosen = *g->state().unit(band);
    CHECK(chosen.promotions == std::vector<TypeIndex>{glam});
    CHECK_EQ(chosen.promotionPicks, 0);
    CHECK_EQ(chosen.xp, 0);  // still level 1
    CHECK(chosen.movesLeft == Fixed());
    CHECK(g->submit(Command::promote(0, band, indie)) == CommandError::CannotPromote);
    // A band never has more to choose than the promotions it lacks.
    Unit& almost = g->stateMutForTests().units.back();
    for (size_t i = 0; i < rules().promotions.size() && almost.promotions.size() + 1 < bandPromotions; ++i) {
        const auto pr = static_cast<TypeIndex>(i);
        if (rules().promotions[i].promotionClass == "PROMOTION_CLASS_ROCK_BAND" && pr != glam) almost.promotions.push_back(pr);
    }
    g->grantBandPromotion(almost);
    g->grantBandPromotion(almost);
    CHECK_EQ(almost.promotionPicks, 1);

    // A concert that earns another promotion leaves it to choose next turn, and the level it gains stays.
    const auto concert = [&](bool hallyu, uint64_t seed) {
        GameState s = landState();
        slot(s, hallyu);
        s.rng.seed(seed);
        addUnit(s, "UNIT_ROCK_BAND", 0, {16, 6});  // in player 1's capital
        s.units.back().promotions.push_back(glam);
        s.units.back().xp = 1;  // level 2
        auto game = Game::fromScenario(rules(), std::move(s));
        REQUIRE(game->submit(Command::performConcert(0, game->state().units.back().id)) == CommandError::Ok);
        return game;
    };
    bool earned = false;
    for (uint64_t seed = 1; seed <= 40 && !earned; ++seed) {
        auto plain = concert(false, seed);
        const Unit* p = plain->state().units.empty() ? nullptr : &plain->state().units.back();
        if (!p || p->promotions.size() < 2) continue;  // broke up, or no promotion this time
        earned = true;
        auto chooser = concert(true, seed);
        const Unit& u = chooser->state().units.back();
        CHECK(u.promotions == std::vector<TypeIndex>{glam});
        CHECK_EQ(u.promotionPicks, 1);
        CHECK_EQ(u.xp, p->xp);
        CHECK(chooser->availablePromotions(u.id).empty());  // its moves went on the concert
        const UnitId id = u.id;
        sovtest::endTurns(*chooser, 2);
        REQUIRE(chooser->submit(Command::promote(0, id, indie)) == CommandError::Ok);
        CHECK(chooser->state().unit(id)->promotions == (std::vector<TypeIndex>{glam, indie}));
        CHECK_EQ(chooser->state().unit(id)->xp, p->xp);
    }
    CHECK(earned);
}
