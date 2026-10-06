// Governors (08: Governors [R&F]): titles from civics, appointing, promoting along the tree,
// establishing, loyalty, promotion effects in the city, Amani as envoys, losing a city, saves.
#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex gov(const char* id) { return rules().governor(id); }
TypeIndex promo(const char* id) { return rules().governorPromotion(id); }

// Player 0 has two title civics; both players have a capital.
GameState govState() {
    GameState s = flatState(30, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    for (const char* c : {"CIVIC_STATE_WORKFORCE", "CIVIC_EARLY_EMPIRE"}) s.players[0].civics.done[at(rules().civic(c))] = 1;
    s.players[0].human = true;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {16, 6}, true, 6);
    s.majorsAtStart = 2;
    return s;
}
}  // namespace

TEST(governor_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.governors.size(), 7u);  // Ibrahim belongs to a civ the roster does not have
    CHECK_EQ(r.governorTitleCivics.size(), 14u);
    const GovernorType& victor = r.governors[at(gov("GOVERNOR_VICTOR"))];
    CHECK_EQ(victor.establishPercent, 150);
    CHECK(r.governorPromotions[at(victor.promotions.front())].base);
    CHECK(r.governors[at(gov("GOVERNOR_AMANI"))].cityStates);
    CHECK_EQ(r.governorPromotions[at(promo("GOVERNOR_PROMOTION_EMBRASURE"))].prerequisites.size(), 2u);
}

TEST(titles_appoint_and_promote_along_the_tree) {
    auto g = Game::fromScenario(rules(), govState());
    CHECK_EQ(g->governorTitles(0), 2);
    CHECK_EQ(g->governorTitles(1), 0);
    const TypeIndex pingala = gov("GOVERNOR_PINGALA");
    REQUIRE(g->submit(Command::appointGovernor(0, pingala)) == CommandError::Ok);
    CHECK_EQ(g->governorTitlesLeft(0), 1);
    CHECK(g->submit(Command::appointGovernor(0, pingala)) == CommandError::CannotGovern);  // already serving
    // Grants needs Connoisseur or Researcher first.
    CHECK(g->submit(Command::promoteGovernor(0, pingala, promo("GOVERNOR_PROMOTION_GRANTS"))) == CommandError::CannotGovern);
    REQUIRE(g->submit(Command::promoteGovernor(0, pingala, promo("GOVERNOR_PROMOTION_RESEARCHER"))) == CommandError::Ok);
    CHECK_EQ(g->governorTitlesLeft(0), 0);
    CHECK(g->submit(Command::appointGovernor(0, gov("GOVERNOR_MAGNUS"))) == CommandError::CannotGovern);  // no titles left
    CHECK(g->canPromoteGovernor(0, pingala, promo("GOVERNOR_PROMOTION_GRANTS")) == false);
}

TEST(an_established_governor_steadies_and_improves_its_city) {
    auto g = Game::fromScenario(rules(), govState());
    const TypeIndex pingala = gov("GOVERNOR_PINGALA");
    const CityId capital = g->state().cities[0].id;
    REQUIRE(g->submit(Command::appointGovernor(0, pingala)) == CommandError::Ok);
    REQUIRE(g->submit(Command::promoteGovernor(0, pingala, promo("GOVERNOR_PROMOTION_RESEARCHER"))) == CommandError::Ok);
    const Fixed science = g->cityReport(capital).yields[static_cast<size_t>(YieldType::Science)];
    const Fixed loyalty = g->loyaltyPerTurn(capital);
    REQUIRE(g->submit(Command::assignGovernor(0, pingala, capital)) == CommandError::Ok);
    CHECK_EQ(g->governor(0, pingala)->establishTurns, 5);
    CHECK(g->establishedGovernor(g->state().cities[0]) == nullptr);
    sovtest::endTurns(*g, 2 * 5);
    REQUIRE(g->establishedGovernor(g->state().cities[0]) != nullptr);
    // +8 loyalty per turn, and Librarian (+15%) with Researcher (+1 Science per citizen).
    CHECK(g->loyaltyPerTurn(capital) >= loyalty + Fixed::fromInt(8));
    CHECK(g->cityReport(capital).yields[static_cast<size_t>(YieldType::Science)] > science + Fixed::fromInt(5));
}

TEST(victor_establishes_faster_and_defends) {
    auto g = Game::fromScenario(rules(), govState());
    const TypeIndex victor = gov("GOVERNOR_VICTOR");
    const City& capital = g->state().cities[0];
    const int before = g->cityStrength(capital);
    REQUIRE(g->submit(Command::appointGovernor(0, victor)) == CommandError::Ok);
    REQUIRE(g->submit(Command::assignGovernor(0, victor, capital.id)) == CommandError::Ok);
    CHECK_EQ(g->governor(0, victor)->establishTurns, 3);
    sovtest::endTurns(*g, 2 * 3);
    CHECK_EQ(g->cityStrength(g->state().cities[0]), before + 5);  // Redoubt
}

TEST(amani_in_a_city_state_counts_as_envoys) {
    GameState s = govState();
    s.players.push_back(Player{});
    Player& cs = s.players.back();
    cs.id = 2;
    cs.cityState = 0;
    Game::fitPlayerToRules(cs, rules());
    for (Player& p : s.players) p.relations.resize(3);
    addCity(s, 2, {10, 10}, true, 3);
    s.players[0].met.assign(3, 1);
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex amani = gov("GOVERNOR_AMANI");
    REQUIRE(g->submit(Command::appointGovernor(0, amani)) == CommandError::Ok);
    const CityId csCity = g->state().cities.back().id;
    CHECK(!g->canAssignGovernor(0, gov("GOVERNOR_PINGALA"), csCity));  // only Amani serves city-states
    REQUIRE(g->submit(Command::assignGovernor(0, amani, csCity)) == CommandError::Ok);
    CHECK_EQ(g->envoysAt(0, 2), 0);
    sovtest::endTurns(*g, 3 * 5);  // three players
    CHECK_EQ(g->envoysAt(0, 2), 2);
}

TEST(a_lost_city_sends_its_governor_home) {
    GameState start = govState();
    const CityId second = addCity(start, 0, {9, 10}, false, 3);
    auto g = Game::fromScenario(rules(), std::move(start));
    const TypeIndex magnus = gov("GOVERNOR_MAGNUS");
    REQUIRE(g->submit(Command::appointGovernor(0, magnus)) == CommandError::Ok);
    REQUIRE(g->submit(Command::assignGovernor(0, magnus, second)) == CommandError::Ok);
    GameState s = g->state();
    s.city(second)->owner = 1;  // taken
    auto g2 = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g2, 2);
    CHECK_EQ(g2->governor(0, magnus)->city, kNoCity);
}

TEST(governors_survive_a_save) {
    auto g = Game::fromScenario(rules(), govState());
    REQUIRE(g->submit(Command::appointGovernor(0, gov("GOVERNOR_LIANG"))) == CommandError::Ok);
    REQUIRE(g->submit(Command::assignGovernor(0, gov("GOVERNOR_LIANG"), g->state().cities[0].id)) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->governor(0, gov("GOVERNOR_LIANG")));
    CHECK_EQ(loaded->governor(0, gov("GOVERNOR_LIANG"))->establishTurns, 5);
    CHECK_EQ(loaded->state().players[0].governorTitlesSpent, 1);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(the_ai_appoints_and_places_governors) {
    GameState s = govState();
    s.players[0].human = false;
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    const Player& p = g->state().players[0];
    REQUIRE(p.governors.size() == 2u);
    CHECK(p.governors[0].city != kNoCity);
    CHECK_EQ(g->governorTitlesLeft(0), 0);
}

// ---- the later promotions the core carries (08: Governors)

namespace {
// Player 0's capital with an established governor holding the given promotion.
GameState withPromotion(const char* governor, const char* promotion) {
    GameState s = govState();
    Governor g;
    g.type = gov(governor);
    g.city = s.cities[0].id;
    g.promotions.push_back(rules().governors[at(g.type)].promotions.front());
    g.promotions.push_back(promo(promotion));
    s.players[0].governors.push_back(g);
    return s;
}
}  // namespace

TEST(liang_water_works_and_pingala_curator) {
    GameState base = govState();
    CityDistrict hood;
    hood.type = rules().district("DISTRICT_NEIGHBORHOOD");
    hood.pos = {5, 7};
    hood.complete = true;
    base.cities[0].districts.push_back(hood);
    auto plain = Game::fromScenario(rules(), base);
    GameState s = withPromotion("GOVERNOR_LIANG", "GOVERNOR_PROMOTION_WATER_WORKS");
    s.cities[0].districts.push_back(hood);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->districtHousing(g->state().cities[0]) == plain->districtHousing(plain->state().cities[0]) + Fixed::fromInt(2));

    // Curator: Great Works' tourism doubled.
    GameState t = withPromotion("GOVERNOR_PINGALA", "GOVERNOR_PROMOTION_CURATOR");
    auto probe = Game::fromScenario(rules(), t);
    TypeIndex work = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size() && work == kNone; ++w) {
        if (rules().greatWorkTypes[w].tourism > 0 && probe->freeGreatWorkSlot(probe->state().cities[0], static_cast<TypeIndex>(w)) != kNone) work = static_cast<TypeIndex>(w);
    }
    REQUIRE(work != kNone);
    GreatWork gw;
    gw.type = work;
    gw.building = probe->freeGreatWorkSlot(probe->state().cities[0], work);
    t.cities[0].greatWorks.push_back(gw);
    GameState u = govState();
    u.cities[0].greatWorks.push_back(gw);
    auto curated = Game::fromScenario(rules(), std::move(t));
    auto uncurated = Game::fromScenario(rules(), std::move(u));
    CHECK_EQ(curated->tourismPerTurn(0) - uncurated->tourismPerTurn(0), rules().greatWorkTypes[at(work)].tourism);
}

TEST(victor_embrasure_trains_veterans) {
    GameState s = withPromotion("GOVERNOR_VICTOR", "GOVERNOR_PROMOTION_EMBRASURE");
    s.cities[0].queue = {{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")}};
    s.cities[0].progress.push_back({s.cities[0].queue.front(), Fixed::fromInt(1000)});
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    const Unit* trained = nullptr;
    for (const Unit& u : g->state().units) {
        if (u.owner == 0 && g->rules().units[at(u.type)].id == "UNIT_WARRIOR") trained = &u;
    }
    REQUIRE(trained);
    CHECK(trained->xp >= g->xpForNextLevel(*trained));
}
