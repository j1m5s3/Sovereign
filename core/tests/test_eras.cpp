// Eras, ages, historic moments and tourism (09: Era score and Ages; 07: Tourism and Culture Victory).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
int score(const Rules& r, const char* moment) { return r.moments[at(r.moment(moment))].eraScore; }

GameState eraState() {
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    addCity(s, 0, {4, 6}, true, 4);
    addCity(s, 1, {16, 6}, true, 4);
    s.majorsAtStart = 2;
    return s;
}

// A 12-column world, nothing seen yet beyond each civ's capital (columns 5 +- the city's sight).
GameState roundWorld(bool wrap) {
    GameState s = flatState(12, 8, 2, wrap);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Unrevealed));
    }
    addCity(s, 0, {5, 3}, true, 1);
    addCity(s, 1, {5, 6}, true, 1);
    s.majorsAtStart = 2;
    return s;
}

void reveal(GameState& s, PlayerId pid, int firstColumn, int lastColumn) {
    for (int x = firstColumn; x <= lastColumn; ++x)
        s.players[static_cast<size_t>(pid)].visibility[static_cast<size_t>(s.grid.index({x, 0}))] = static_cast<uint8_t>(Visibility::Revealed);
}

// Two of player 0's cities, at (4,6) and (10,6), with track on (5,6), (6,6), (8,6) and (9,6); a Military
// Engineer on (7,6) and a Builder on (9,6).
GameState railState(bool gap) {
    GameState s = flatState(16, 12, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    for (const char* t : {"TECH_STEAM_POWER", "TECH_CHEMISTRY"}) s.players[0].techs.done[at(rules().tech(t))] = 1;
    for (const char* r : {"RESOURCE_IRON", "RESOURCE_COAL"}) s.players[0].stockpile[at(rules().resource(r))] = 3;
    addCity(s, 0, {4, 6}, true, 1);
    addCity(s, 0, {10, 6}, false, 1);
    addCity(s, 1, {12, 1}, true, 1);
    s.majorsAtStart = 2;
    TypeIndex rr = kNone;
    for (size_t i = 0; i < rules().routes.size() && rr == kNone; ++i) {
        if (rules().routes[i].unitOnly) rr = static_cast<TypeIndex>(i);
    }
    for (const Hex h : {Hex{5, 6}, Hex{6, 6}, Hex{8, 6}, Hex{9, 6}}) s.plot(h).route = static_cast<int8_t>(rr);
    s.plot({9, 6}).routePillaged = gap;
    sovtest::addUnit(s, "UNIT_MILITARY_ENGINEER", 0, {7, 6});
    sovtest::addUnit(s, "UNIT_BUILDER", 0, {9, 6});
    return s;
}
}  // namespace

TEST(era_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.moments.size(), 150u);
    CHECK_EQ(score(r, "MOMENT_WORLD_WONDER_COMPLETED"), 4);
    CHECK_EQ(score(r, "MOMENT_GREAT_PERSON_RECRUITED"), 1);
    CHECK_EQ(r.eras[0].minTurns, 40);
    CHECK_EQ(r.eras[0].maxTurns, 60);
    CHECK_EQ(r.eras[0].eraScoreShift, -3);
}

TEST(moments_score_the_first_civ_more) {
    GameState s = eraState();
    s.players[0].faith = Fixed::fromInt(30);
    s.players[1].faith = Fixed::fromInt(30);
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::foundPantheon(0, rules().belief("BELIEF_STONE_CIRCLES"))) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].eraScore, score(rules(), "MOMENT_WORLD_S_FIRST_PANTHEON"));
    sovtest::endTurns(*g, 1);
    REQUIRE(g->submit(Command::foundPantheon(1, rules().belief("BELIEF_GOD_OF_THE_SEA"))) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].eraScore, score(rules(), "MOMENT_PANTHEON_FOUNDED"));
}

TEST(era_score_counts_in_the_score_across_eras) {
    GameState s = eraState();
    s.players[0].faith = Fixed::fromInt(30);
    auto g = Game::fromScenario(rules(), std::move(s));
    const int before = g->score(0);
    REQUIRE(g->submit(Command::foundPantheon(0, rules().belief("BELIEF_STONE_CIRCLES"))) == CommandError::Ok);
    const int moment = score(rules(), "MOMENT_WORLD_S_FIRST_PANTHEON");
    REQUIRE(moment > 0);
    CHECK_EQ(g->state().players[0].eraScoreTotal, moment);
    CHECK_EQ(g->score(0), before + moment);  // a point of score per point of era score (00: Score)
    GameState next = g->state();
    next.players[0].eraScore = 0;  // a new era starts the era's count again; the total stays
    auto later = Game::fromScenario(rules(), std::move(next));
    CHECK_EQ(later->score(0), before + moment);
}

TEST(the_world_era_sets_each_civs_age) {
    GameState s = eraState();
    s.players[0].eraScore = 100;  // far above the Golden threshold
    s.players[1].eraScore = 0;    // below the Dark one
    s.turn = 70;                  // past the Ancient era's 60 turns
    auto g = Game::fromScenario(rules(), std::move(s));
    const auto [dark, golden] = g->ageThresholds(0);
    CHECK(dark < golden);
    sovtest::endTurns(*g, 2);  // a world turn passes
    CHECK_EQ(g->state().gameEra, 1);
    CHECK(g->state().players[0].age == Age::Golden);
    CHECK(g->state().players[1].age == Age::Dark);
    CHECK_EQ(g->state().players[0].eraScore, 0);
    // Out of a Dark Age straight into a Golden one: a Heroic Age.
    GameState next = g->state();
    next.players[1].eraScore = 100;
    next.turn = next.gameEraStart + 61;
    auto g2 = Game::fromScenario(rules(), std::move(next));
    sovtest::endTurns(*g2, 2);
    CHECK(g2->state().players[1].age == Age::Heroic);
}

TEST(wonders_and_holy_cities_bring_tourism) {
    GameState s = eraState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_PYRAMIDS"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {rules().belief("BELIEF_TITHE")}});
    s.players[0].religion = 0;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->tourismPerTurn(0), 2 + 8);  // an Ancient wonder in the Ancient era, the Holy City
    CHECK_EQ(g->tourismPerTurn(1), 0);
}

TEST(visitors_beyond_every_rivals_home_tourists_win) {
    GameState s = eraState();
    s.players[0].tourismTo = {0, 2 * 200 * 6};  // six visitors from player 1 (the Sovereign floor: 5 per rival)
    s.players[1].lifetimeCulture = Fixed::fromInt(900);  // nine domestic tourists, six of them visiting us
    GameState few = s;
    few.players[0].tourismTo = {0, 2 * 200 * 2};  // two visitors beat one domestic tourist, but are under the floor
    few.players[1].lifetimeCulture = Fixed::fromInt(300);
    auto early = Game::fromScenario(rules(), std::move(few));
    CHECK_EQ(early->visitingTourists(0, 1), 2);
    CHECK_EQ(early->domesticTourists(1), 1);
    CHECK_EQ(early->cultureVictor(), kNoPlayer);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->visitingTourists(0, 1), 6);
    CHECK_EQ(g->domesticTourists(1), 3);
    CHECK_EQ(g->cultureVictor(), 0);
    sovtest::endTurns(*g, 1);
    CHECK(g->state().victory == Victory::Culture);
}

TEST(eras_survive_a_save) {
    GameState s = eraState();
    s.players[0].eraScore = 7;
    s.players[0].eraScoreTotal = 19;
    s.players[0].age = Age::Golden;
    s.gameEra = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].eraScore, 7);
    CHECK_EQ(loaded->state().players[0].eraScoreTotal, 19);
    CHECK(loaded->state().players[0].age == Age::Golden);
    CHECK_EQ(loaded->state().gameEra, 1);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

// ---- dedications [R&F] (09: Dedications)

TEST(dedications_come_from_the_data_and_the_code_knows_them_all) {
    const Rules& r = rules();
    CHECK_EQ(r.dedications.size(), 12u);
    for (const char* id : {"DEDICATION_FREE_INQUIRY", "DEDICATION_PEN_BRUSH_AND_VOICE", "DEDICATION_MONUMENTALITY", "DEDICATION_EXODUS_OF_THE_EVANGELISTS",
                           "DEDICATION_HIC_SUNT_DRACONES", "DEDICATION_REFORM_THE_COINAGE", "DEDICATION_HEARTBEAT_OF_STEAM", "DEDICATION_TO_ARMS",
                           "DEDICATION_WISH_YOU_WERE_HERE", "DEDICATION_SKY_AND_STARS", "DEDICATION_BODYGUARD_OF_LIES", "DEDICATION_AUTOMATON_WARFARE"})
        CHECK(r.dedication(id) != kNone);
    const DedicationType& monument = r.dedications[at(r.dedication("DEDICATION_MONUMENTALITY"))];
    CHECK_EQ(monument.eraMin, r.era("ERA_CLASSICAL"));
    CHECK_EQ(monument.eraMax, r.era("ERA_RENAISSANCE"));
    CHECK_EQ(r.dedications[at(r.dedication("DEDICATION_SKY_AND_STARS"))].eraMax, -1);
}

TEST(a_dedication_scores_in_a_normal_age_and_rewards_in_a_golden_one) {
    GameState s = eraState();
    s.gameEra = rules().era("ERA_CLASSICAL");
    s.players[0].dedicationsPending = 1;
    s.players[0].gold = Fixed::fromInt(2000);
    auto g = Game::fromScenario(rules(), s);
    const TypeIndex monument = rules().dedication("DEDICATION_MONUMENTALITY");
    const std::vector<TypeIndex> open = g->availableDedications(0);
    CHECK(std::find(open.begin(), open.end(), monument) != open.end());
    CHECK(std::find(open.begin(), open.end(), rules().dedication("DEDICATION_SKY_AND_STARS")) == open.end());  // not this era
    REQUIRE(g->submit(Command::chooseDedication(0, monument)) == CommandError::Ok);
    CHECK(g->availableDedications(0).empty());  // one choice in a Normal Age
    // Normal Age: a district built is +1 era score.
    const int before = g->state().players[0].eraScore;
    City& c = g->stateMutForTests().cities[0];
    CityDistrict d;
    d.type = rules().district("DISTRICT_CAMPUS");
    d.pos = {5, 7};
    c.districts.push_back(d);
    g->completeItem(c, {ProductionKind::District, d.type});
    CHECK_EQ(g->state().players[0].eraScore, before + 1);
    // Golden Age: no score, but Builders cost 30% less and move 2 further.
    const ProductionItem builder{ProductionKind::Unit, rules().unit("UNIT_BUILDER")};
    const int normalCost = g->purchaseCost(0, builder);
    s.players[0].age = Age::Golden;
    s.players[0].dedications = {monument};
    s.players[0].dedicationsPending = 0;
    const UnitId b = sovtest::addUnit(s, "UNIT_BUILDER", 0, {4, 7});
    auto h = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(h->purchaseCost(0, builder), normalCost * 70 / 100 / 5 * 5);
    CHECK_EQ(h->maxMoves(*h->state().unit(b)), rules().units[at(rules().unit("UNIT_BUILDER"))].moves + 2);
}

TEST(to_arms_in_a_golden_age_opens_the_golden_age_war) {
    GameState s = eraState();
    s.gameEra = rules().era("ERA_INDUSTRIAL");
    for (Player& p : s.players) {
        p.relations.resize(2);
        p.met.assign(2, uint8_t{1});
    }
    s.players[0].age = Age::Golden;
    s.players[0].dedications = {rules().dedication("DEDICATION_TO_ARMS")};
    auto g = Game::fromScenario(rules(), s);
    CHECK(!g->hasCasusBelli(0, 1, CasusBelli::GoldenAge));  // not denouncing yet
    s.players[0].relations[1].denouncedOn = s.turn;
    auto h = Game::fromScenario(rules(), std::move(s));
    CHECK(h->hasCasusBelli(0, 1, CasusBelli::GoldenAge));
    CHECK_EQ(h->casusBelliGrievancePercent(CasusBelli::GoldenAge), 25);
}

// Circumnavigation (09: Historic moments): a plot seen in every column of a world that wraps.
TEST(seeing_every_column_of_a_round_world_is_a_circumnavigation) {
    GameState s = roundWorld(true);
    reveal(s, 0, 0, 10);  // all but the last column
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK(!sovtest::hasMoment(*g, 0, "MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION"));
    // The last column seen: the world's first, at the start of the civ's next turn.
    GameState t = g->state();
    reveal(t, 0, 11, 11);
    auto h = Game::fromScenario(rules(), std::move(t));
    sovtest::endTurns(*h, 2);
    CHECK(sovtest::hasMoment(*h, 0, "MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION"));
    CHECK(!sovtest::hasMoment(*h, 0, "MOMENT_WORLD_CIRCUMNAVIGATED"));
    // A later civ earns the ordinary moment, once.
    GameState u = h->state();
    reveal(u, 1, 0, 11);
    auto k = Game::fromScenario(rules(), std::move(u));
    sovtest::endTurns(*k, 1);
    CHECK(sovtest::hasMoment(*k, 1, "MOMENT_WORLD_CIRCUMNAVIGATED"));
    CHECK(!sovtest::hasMoment(*k, 1, "MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION"));
    const int later = k->state().players[1].eraScoreTotal;
    sovtest::endTurns(*k, 2);
    CHECK_EQ(k->state().players[1].eraScoreTotal, later);
    CHECK(!sovtest::hasMoment(*k, 0, "MOMENT_WORLD_CIRCUMNAVIGATED"));  // the first civ is not counted again
    // A world that does not wrap cannot be sailed round.
    GameState flat = roundWorld(false);
    reveal(flat, 0, 0, 11);
    auto f = Game::fromScenario(rules(), std::move(flat));
    sovtest::endTurns(*f, 2);
    CHECK(!sovtest::hasMoment(*f, 0, "MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION"));
}

// A railroad connection (09: Historic moments): track joining two of the civ's cities.
TEST(the_first_railroad_between_two_cities_is_a_moment) {
    auto g = Game::fromScenario(rules(), railState(false));
    const UnitId engineer = g->state().units[0].id;
    const int before = g->state().players[0].eraScore;
    REQUIRE(g->submit(Command::buildRailroad(0, engineer)) == CommandError::Ok);
    CHECK(sovtest::hasMoment(*g, 0, "MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD"));
    CHECK_EQ(g->state().players[0].eraScore, before + score(rules(), "MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD"));
    // A pillaged plot breaks the line until a Builder mends it.
    auto h = Game::fromScenario(rules(), railState(true));
    REQUIRE(h->submit(Command::buildRailroad(0, h->state().units[0].id)) == CommandError::Ok);
    CHECK(!sovtest::hasMoment(*h, 0, "MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD"));
    REQUIRE(h->submit(Command::repairImprovement(0, h->state().units[1].id)) == CommandError::Ok);
    CHECK(sovtest::hasMoment(*h, 0, "MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD"));
}
