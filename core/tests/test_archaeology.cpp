// Archaeology (07-great-people-great-works-tourism.md: Archaeology).
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

TEST(battles_become_antiquity_sites_and_archaeologists_dig_them) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 2;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {14, 6}, true, 6);
    s.battleSites = {s.grid.index({6, 6})};
    s.battleHistory = {1 * 4096 + s.players[1].civ + 1};  // player 1 attacked here in the Classical era
    City& home = s.cities[0];
    home.buildings.push_back(rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM"));
    std::sort(home.buildings.begin(), home.buildings.end());
    auto early = Game::fromScenario(rules(), s);
    early->placeAntiquity();
    CHECK(!early->state().antiquityPlaced);  // nobody knows Natural History yet
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    const UnitId dig = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->state().unit(dig)->charges, 3);
    g->placeAntiquity();
    REQUIRE(g->state().antiquityPlaced);
    CHECK_EQ(g->state().plot({6, 6}).antiquity, 1);  // the battle plot first
    int land = 0;
    for (const Plot& p : g->state().plots) land += p.antiquity == 1 ? 1 : 0;
    CHECK_EQ(land, 2 * rules().globalInt("ARCHAEOLOGY_SITES_PER_CIV_LAND"));
    const size_t works = g->state().cities[0].greatWorks.size();
    const size_t combustion = static_cast<size_t>(rules().tech("TECH_COMBUSTION"));
    CHECK_EQ(g->state().players[0].techs.boosted[combustion], 0);
    REQUIRE(g->submit(Command::excavate(0, dig)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).antiquity, 0);
    CHECK(sovtest::hasMoment(*g, 0, "MOMENT_ARTIFACT_EXTRACTED"));  // 09
    CHECK_EQ(g->state().players[0].techs.boosted[combustion], 1);    // 04: Combustion's Eureka
    REQUIRE(g->state().cities[0].greatWorks.size() == works + 1);
    CHECK_EQ(g->state().cities[0].greatWorks.back().era, 1);
    CHECK_EQ(g->state().cities[0].greatWorks.back().civ, g->state().players[1].civ);
    CHECK_EQ(g->state().unit(dig)->charges, 2);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

// ---- theming (07: Theming bonuses)

TEST(a_themed_museum_doubles_its_works) {
    GameState s = flatState(20, 12, 3);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 3;
    addCity(s, 0, {4, 6}, true, 6);
    City& c = s.cities[0];
    const TypeIndex art = rules().building("BUILDING_ART_MUSEUM"), dig = rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM");
    c.buildings = {art, dig};
    std::sort(c.buildings.begin(), c.buildings.end());
    TypeIndex sculpture = kNone, portrait = kNone, artifact = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size(); ++w) {
        const std::string& id = rules().greatWorkTypes[w].id;
        if (id == "SCULPTURE") sculpture = static_cast<TypeIndex>(w);
        if (id == "PORTRAIT") portrait = static_cast<TypeIndex>(w);
        if (id == "ARTIFACT") artifact = static_cast<TypeIndex>(w);
    }
    REQUIRE(sculpture != kNone && portrait != kNone && artifact != kNone);
    for (TypeIndex by : {TypeIndex{1}, TypeIndex{2}, TypeIndex{3}}) c.greatWorks.push_back({sculpture, art, by, -1, kNone});
    for (int k = 0; k < 3; ++k) c.greatWorks.push_back({artifact, dig, kNone, 1, s.players[static_cast<size_t>(k)].civ});
    auto g = Game::fromScenario(rules(), s);
    CHECK(g->themed(g->state().cities[0], art));
    CHECK(g->themed(g->state().cities[0], dig));
    const int themedTourism = g->tourismPerTurn(0);
    // A portrait among the sculptures, and two artifacts of one civilization, break both themes.
    s.cities[0].greatWorks[2].type = portrait;
    s.cities[0].greatWorks[4].civ = s.cities[0].greatWorks[3].civ;
    auto h = Game::fromScenario(rules(), std::move(s));
    CHECK(!h->themed(h->state().cities[0], art));
    CHECK(!h->themed(h->state().cities[0], dig));
    CHECK(h->tourismPerTurn(0) < themedTourism);
    CHECK_EQ(themedTourism - h->tourismPerTurn(0), 2 * 3 * rules().greatWorkTypes[static_cast<size_t>(sculpture)].tourism / 2 + 3 * rules().greatWorkTypes[static_cast<size_t>(artifact)].tourism);
}

TEST(works_move_between_slots_to_theme_a_museum) {
    GameState s = flatState(24, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 2;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 0, {12, 6}, false, 6);
    const TypeIndex art = rules().building("BUILDING_ART_MUSEUM");
    TypeIndex sculpture = kNone, portrait = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size(); ++w) {
        if (rules().greatWorkTypes[w].id == "SCULPTURE") sculpture = static_cast<TypeIndex>(w);
        if (rules().greatWorkTypes[w].id == "PORTRAIT") portrait = static_cast<TypeIndex>(w);
    }
    for (City& c : s.cities) c.buildings = {art};
    s.cities[0].greatWorks = {{portrait, art, 1, -1, kNone}, {sculpture, art, 2, -1, kNone}};
    s.cities[1].greatWorks = {{sculpture, art, 3, -1, kNone}, {sculpture, art, 4, -1, kNone}};
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId a = g->state().cities[0].id, b = g->state().cities[1].id;
    CHECK(!g->themed(g->state().cities[0], art));
    // A single move straight in needs a free slot.
    CHECK(g->submit(Command::moveGreatWork(0, b, 0, a, art)) == CommandError::Ok);
    CHECK(g->submit(Command::moveGreatWork(0, b, 0, a, art)) == CommandError::BadTarget);  // now full
    // Moved art is locked for GREATWORK_ART_LOCK_TIME turns (07), so it cannot go straight back.
    CHECK(g->state().city(a)->greatWorks[2].lockedUntil == g->state().turn + rules().globalInt("GREATWORK_ART_LOCK_TIME"));
    CHECK(g->submit(Command::moveGreatWork(0, a, 2, b, art)) == CommandError::BadTarget);
    const std::vector<Command> moves = g->themingMoves(0, a, art);
    REQUIRE(!moves.empty());
    for (const Command& m : moves) REQUIRE(g->submit(m) == CommandError::Ok);
    CHECK(g->themed(*g->state().city(a), art));
    CHECK(g->state().city(b)->greatWorks.size() == 1u);  // the portrait went over
    CHECK(g->themingMoves(0, a, art).empty());           // nothing left to do
}

TEST(a_great_work_that_completes_a_theme_is_worth_buying) {
    GameState s = flatState(24, 12, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(2, uint8_t{1});
        p.gold = Fixed::fromInt(1000);
    }
    s.players[0].human = true;
    s.majorsAtStart = 2;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {16, 6}, true, 6);
    const TypeIndex art = rules().building("BUILDING_ART_MUSEUM"), palace = rules().building("BUILDING_PALACE");
    TypeIndex sculpture = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size(); ++w) {
        if (rules().greatWorkTypes[w].id == "SCULPTURE") sculpture = static_cast<TypeIndex>(w);
    }
    s.cities[0].buildings = {palace};
    s.cities[0].greatWorks = {{sculpture, palace, 3, -1, kNone}};
    s.cities[1].buildings = {art};
    s.cities[1].greatWorks = {{sculpture, art, 1, -1, kNone}, {sculpture, art, 2, -1, kNone}};
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId mine = g->state().cities[0].id, theirs = g->state().cities[1].id;
    CHECK(g->workCompletesTheme(1, g->state().cities[0].greatWorks[0]));
    const std::vector<DealItem> offer = {{DealItemKind::GreatWork, 0, mine, 0}, {DealItemKind::Gold, 1, 250, kNone}};
    REQUIRE(g->dealProblem(Deal{0, 0, 1, 1, offer}) == CommandError::Ok);
    CHECK(describeDeal(rules(), g->state(), Deal{0, 0, 1, 1, offer}).find("sculpture") != std::string::npos);
    REQUIRE(g->submit(Command::proposeDeal(0, 1, offer)) == CommandError::Ok);
    CHECK(g->state().events.back().kind == EventKind::DealAccepted);
    CHECK(g->state().city(mine)->greatWorks.empty());
    REQUIRE(g->state().city(theirs)->greatWorks.size() == 3u);
    CHECK(g->themed(*g->state().city(theirs), art));
    // A work in a themed museum is not for sale.
    const std::vector<DealItem> back = {{DealItemKind::GreatWork, 1, theirs, 0}, {DealItemKind::Gold, 0, 600, kNone}};
    CHECK(!g->wouldAccept(1, Deal{0, 0, 1, 1, back}));
}

TEST(shipwrecks_wait_for_cultural_heritage) {
    GameState s = flatState(20, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.majorsAtStart = 2;
    addCity(s, 0, {4, 6}, true, 6);
    addCity(s, 1, {14, 6}, true, 6);
    City& home = s.cities[0];
    home.buildings.push_back(rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM"));
    std::sort(home.buildings.begin(), home.buildings.end());
    s.plot({8, 3}).terrain = rules().terrain("TERRAIN_COAST");
    s.plot({8, 3}).antiquity = 2;
    s.plot({8, 8}).antiquity = 1;
    s.antiquityPlaced = true;
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    const UnitId wreck = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {8, 3});
    const UnitId site = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {8, 8});
    GameState later = s;
    later.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_CULTURAL_HERITAGE"))] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->seesAntiquity(0) && !g->seesAntiquity(0, 2));
    CHECK(g->excavateProblem(0, wreck) == CommandError::BadTarget);
    CHECK(g->excavateProblem(0, site) == CommandError::Ok);
    auto h = Game::fromScenario(rules(), std::move(later));
    CHECK(h->seesAntiquity(0, 2));
    CHECK(h->excavateProblem(0, wreck) == CommandError::Ok);
}

TEST(an_ai_archaeologist_passes_a_shipwreck_it_cannot_dig) {
    GameState s = flatState(20, 12, 1);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    addCity(s, 0, {3, 6}, true, 6);
    s.cities[0].buildings.push_back(rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
    s.plot({7, 6}).antiquity = 2;  // a Shipwreck beside it
    s.plot({12, 6}).antiquity = 1;  // an Antiquity Site further on
    s.antiquityPlaced = true;
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    const UnitId dig = addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {8, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    REQUIRE(g->state().unit(dig));
    CHECK(g->state().grid.distance(g->state().unit(dig)->pos, {12, 6}) < 4);  // on its way to the site it can dig
}

// One Archaeologist per Archaeological Museum the civ has (07: Archaeology).
TEST(one_archaeologist_per_archaeological_museum) {
    GameState s = flatState(24, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    const CityId a = addCity(s, 0, {4, 6}, true, 6);
    const CityId b = addCity(s, 0, {14, 6}, false, 6);
    addUnit(s, "UNIT_ARCHAEOLOGIST", 1, {20, 2});  // another civ's counts against its own museums only
    for (City& c : s.cities) {
        c.buildings.push_back(rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM"));
        std::sort(c.buildings.begin(), c.buildings.end());
    }
    const ProductionItem dig{ProductionKind::Unit, rules().unit("UNIT_ARCHAEOLOGIST")};
    auto two = Game::fromScenario(rules(), s);
    CHECK(two->canProduce(*two->state().city(a), dig));
    addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {6, 6});
    auto one = Game::fromScenario(rules(), s);
    CHECK(one->canProduce(*one->state().city(a), dig));  // the other museum still supports one
    addUnit(s, "UNIT_ARCHAEOLOGIST", 0, {8, 6});
    auto full = Game::fromScenario(rules(), std::move(s));
    CHECK(!full->canProduce(*full->state().city(a), dig));
    CHECK(!full->canProduce(*full->state().city(b), dig));
}
