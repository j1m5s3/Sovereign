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
    // Golden Ages lift a city's loyalty, Dark Ages weigh on it (0.5 per citizen).
    CHECK_EQ(g->ageLoyalty(g->state().cities[0]), g->state().cities[0].population / 2);
    CHECK_EQ(g->ageLoyalty(g->state().cities[1]), -(g->state().cities[1].population / 2));
    CHECK(g->ageLoyalty(g->state().cities[0]) > 0);
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
    s.players[0].age = Age::Golden;
    s.gameEra = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].eraScore, 7);
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
