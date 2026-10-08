// The computer opponent (MVP-6; 10-ai-ui-implementation.md, AI architecture).
#include <algorithm>
#include <string>

#include "../tools/random_bot.h"
#include "helpers.h"
#include "sovereign/ai.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

int citiesOf(const Game& g, PlayerId p) {
    int n = 0;
    for (const City& c : g.state().cities) n += c.owner == p;
    return n;
}

int populationOf(const Game& g, PlayerId p) {
    int n = 0;
    for (const City& c : g.state().cities) n += c.owner == p ? c.population : 0;
    return n;
}

bool capitalTaken(const Game& g) {
    for (const City& c : g.state().cities) {
        if (c.originalCapital && c.owner != c.originalOwner) return true;
    }
    return false;
}

void learn(GameState& s, PlayerId p, const char* tech) {
    Player& pl = s.players[at(p)];
    pl.techs.resize(rules().techs.size());
    pl.techs.done[at(rules().tech(tech))] = 1;
}
}  // namespace

TEST(ai_founds_capital_and_fills_every_order) {
    GameState s = flatState(20, 14, 1);
    addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
    addUnit(s, "UNIT_BUILDER", 0, {6, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    const int turn = g->state().turn;
    ai::playTurn(*g);
    CHECK_EQ(g->state().turn, turn + 1);  // the turn ended: nothing was left without orders
    REQUIRE(citiesOf(*g, 0) == 1);
    const City& capital = g->state().cities.front();
    CHECK_EQ(capital.pos, (Hex{6, 6}));
    CHECK(!capital.queue.empty());
    const Player& p = g->state().players[0];
    CHECK(p.techs.current != kNone);
    CHECK(p.civics.current != kNone);
    // A few more turns: the Builder improves a plot and the city keeps producing.
    for (int i = 0; i < 6; ++i) ai::playTurn(*g);
    int improved = 0;
    for (const Plot& pl : g->state().plots) improved += pl.improvement != kNone;
    CHECK(improved >= 1);
    CHECK(!g->state().cities.front().queue.empty());
}

TEST(ai_settle_score_prefers_good_sites) {
    GameState s = flatState(24, 14, 1);
    addCity(s, 0, {4, 6}, true);
    // A river at (12,6); (18,6) is plain grassland as far from the capital. Desert around (12,9).
    s.plot({12, 6}).riverEdges = kRiverE;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(ai::settleScore(*g, 0, {5, 6}), -1);  // too close to the capital
    CHECK(ai::settleScore(*g, 0, {8, 6}) > ai::settleScore(*g, 0, {12, 7}));  // nearer home
    CHECK(ai::settleScore(*g, 0, {12, 6}) > ai::settleScore(*g, 0, {12, 8}));  // fresh water
    GameState d = flatState(24, 14, 1);
    for (const Hex& h : d.grid.within({12, 6}, 3)) d.plot(h).terrain = rules().terrain("TERRAIN_DESERT");
    auto dry = Game::fromScenario(rules(), std::move(d));
    CHECK(ai::settleScore(*dry, 0, {12, 6}) < ai::settleScore(*dry, 0, {6, 6}));
}

// A Builder heads for the best plot no other player's unit stands on: a stranger stands on the Wheat east of the
// capital, so it farms the Wheat to the west, where one of its own units stands, or else a plain plot.
TEST(ai_builders_pass_over_plots_others_stand_on) {
    const auto farmed = [](bool ownWest) {
        GameState s = flatState(20, 14, 2);
        addCity(s, 0, {6, 6}, true);
        addCity(s, 1, {16, 6}, true);
        s.plot({7, 6}).resource = rules().resource("RESOURCE_WHEAT");
        addUnit(s, "UNIT_WARRIOR", 1, {7, 6});
        if (ownWest) {
            s.plot({5, 6}).resource = rules().resource("RESOURCE_WHEAT");
            addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
        }
        const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {6, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        ai::playTurn(*g);
        CHECK(g->state().plot({7, 6}).improvement == kNone);
        const Unit* u = g->state().unit(builder);
        REQUIRE(u);
        CHECK(g->state().plot(u->pos).improvement != kNone);  // it improved the plot it went to
        return u->pos;
    };
    const Hex plain = farmed(false);
    CHECK(plain != (Hex{6, 6}));
    CHECK(plain != (Hex{7, 6}));
    CHECK(farmed(true) == (Hex{5, 6}));
}

// Two of the AI's units of a type side by side merge into a Corps once it has Nationalism (05); another player's unit
// of the type beside one, or one of its own of another type, does not.
TEST(ai_merges_twins_into_a_corps) {
    GameState s = flatState(20, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.players[0].civics.done[at(rules().civic("CIVIC_NATIONALISM"))] = 1;
    addCity(s, 0, {4, 6}, true);
    addCity(s, 1, {16, 6}, true);
    const UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {9, 6});
    addUnit(s, "UNIT_WARRIOR", 1, {10, 6});
    addUnit(s, "UNIT_SPEARMAN", 0, {9, 7});
    auto alone = Game::fromScenario(rules(), s);
    ai::playTurn(*alone);
    for (const Unit& u : alone->state().units) CHECK_EQ(u.formation, 0);
    const UnitId b = addUnit(s, "UNIT_WARRIOR", 0, {8, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    const Unit* ua = g->state().unit(a);
    const Unit* ub = g->state().unit(b);
    CHECK((ua == nullptr) != (ub == nullptr));  // one joined the other
    REQUIRE((ua ? ua : ub) != nullptr);
    CHECK_EQ((ua ? ua : ub)->formation, 1);
}

// The AI's Builders ask builderCanImprove of every plot near their cities: on every plot of a game well under way, with
// city-states in it, it says what improvementsAt says of Builder work, for every player.
TEST(ai_builder_work_matches_the_improvements_listed) {
    GameSetup setup;
    setup.seed = 3;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    while (g->state().turn < 80 && !g->gameOver()) ai::playTurn(*g);
    int work = 0, wrong = 0;
    for (const Player& p : g->state().players) {
        for (int i = 0; i < g->state().grid.size(); ++i) {
            const Hex h = g->state().grid.at(i);
            const std::vector<TypeIndex> listed = g->improvementsAt(p.id, h);
            const bool any = std::any_of(listed.begin(), listed.end(), [](TypeIndex im) { return rules().improvements[at(im)].builtBy == kNone; });
            work += any ? 1 : 0;
            wrong += g->builderCanImprove(p.id, h) != any ? 1 : 0;
        }
    }
    CHECK(work > 0);
    CHECK_EQ(wrong, 0);
}

// The AI's explorers try only the plots in a unit's move reach: in a game well under way (foreign land and units,
// city-states, coasts), a unit's reach is every plot near it that a move order for it finds a path to, overland or not.
TEST(move_reach_matches_the_paths_found_in_a_game) {
    GameSetup setup;
    setup.seed = 3;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    while (g->state().turn < 60 && !g->gameOver()) ai::playTurn(*g);
    const GameState& s = g->state();
    std::vector<int> looked(s.players.size(), 0);
    int units = 0, reached = 0, unreached = 0, wrong = 0;
    for (const Unit& u : s.units) {
        if (looked[static_cast<size_t>(u.owner)]++ >= 4) continue;  // a few of each player's
        ++units;
        // A linked escort's order moves the pair: it is planned for the leader.
        const Unit* leader = u.escorting != kNoUnit ? s.unit(u.escorting) : nullptr;
        const UnitId mover = leader && leader->pos == u.pos && leader->owner == u.owner ? leader->id : u.id;
        for (bool overland : {false, true}) {
            const std::vector<uint8_t> reach = g->moveReach(u.id, overland);
            for (const Hex& h : s.grid.within(u.pos, 6)) {
                const bool path = g->findPath(mover, h, overland).has_value();
                reached += path ? 1 : 0;
                unreached += path ? 0 : 1;
                wrong += (reach[static_cast<size_t>(s.grid.index(h))] != 0) != path ? 1 : 0;
            }
        }
    }
    CHECK(units >= 8);
    CHECK(reached > 0);
    CHECK(unreached > 0);
    CHECK_EQ(wrong, 0);
}

// A scout sent exploring heads for the nearest unexplored ground it can get to: here not the island just off the coast
// (it cannot embark yet), but the land the other way.
TEST(ai_scouts_explore_past_ground_they_cannot_reach) {
    GameState s = flatState(24, 14, 1);
    for (int y = 0; y < 14; ++y) {
        for (int x = 7; x < 24; ++x) s.plot({x, y}).terrain = rules().terrain("TERRAIN_COAST");
    }
    s.plot({9, 6}).terrain = rules().terrain("TERRAIN_GRASS");
    Player& p = s.players[0];
    Game::fitPlayerToRules(p, rules());
    p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    for (const Hex& h : {Hex{10, 6}, Hex{1, 6}}) p.visibility[static_cast<size_t>(s.grid.index(h))] = static_cast<uint8_t>(Visibility::Unrevealed);
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    const Unit* u = g->state().unit(scout);
    REQUIRE(u);
    CHECK(u->pos.x < 6);
}

// Two Builders split the work: the first heads for the Wheat (on land already seen), and the second leaves it to the
// first. They start off the city's land, too far out to reach the Wheat this turn or when the next one begins.
TEST(ai_builders_split_up_over_the_work) {
    GameState s = flatState(20, 14, 1);
    addCity(s, 0, {6, 6}, true);
    for (const Hex& h : s.grid.within({6, 6}, 3)) sovtest::claimFor(s, s.cities[0], h);
    const Hex wheat{9, 6};
    s.plot(wheat).resource = rules().resource("RESOURCE_WHEAT");
    s.players[0].visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    const UnitId first = addUnit(s, "UNIT_BUILDER", 0, {1, 6});
    const UnitId second = addUnit(s, "UNIT_BUILDER", 0, {1, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);
    int bound = 0;
    for (const UnitId id : {first, second}) {
        const Unit* u = g->state().unit(id);
        REQUIRE(u);
        bound += u->pos == wheat || u->moveTarget == wheat ? 1 : 0;
    }
    CHECK_EQ(bound, 1);
    CHECK(g->state().plot(wheat).improvement == kNone);  // not reached yet
}

// An Archer at war shoots what it can reach with no unit there: an Encampment, or else the city itself (an attack
// looks for its targets among the other players' units, cities and districts).
TEST(ai_shoots_an_undefended_encampment_or_city) {
    const auto shoot = [](bool encampment) {
        GameState s = flatState(24, 14, 2);
        addCity(s, 0, {4, 6}, true, 3);
        addCity(s, 1, {16, 6}, true, 3);
        if (encampment) {
            CityDistrict camp;
            camp.type = rules().district("DISTRICT_ENCAMPMENT");
            camp.pos = {13, 6};
            camp.complete = true;
            s.cities[1].districts.push_back(camp);
            sovtest::claimFor(s, s.cities[1], camp.pos);
        }
        addUnit(s, "UNIT_ARCHER", 0, encampment ? Hex{11, 6} : Hex{14, 6});  // two plots from its target
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
        const City& before = g->state().cities[1];
        const int hp = before.hp, damage = encampment ? before.districts[0].damage : 0;
        ai::playTurn(*g);
        const City& after = g->state().cities[1];
        return encampment ? after.districts[0].damage > damage && after.hp == hp : after.hp < hp;
    };
    CHECK(shoot(true));
    CHECK(shoot(false));
}

// A walled city strikes an enemy beside it, and its Encampment one beside that (03: Defense).
TEST(ai_cities_and_encampments_strike_enemies_in_reach) {
    const auto struck = [](bool encampment) {
        GameState s = flatState(24, 14, 2);
        addCity(s, 0, {4, 6}, true, 3);
        addCity(s, 1, {20, 6}, true, 3);
        City& home = s.cities[0];
        home.buildings.push_back(rules().building("BUILDING_ANCIENT_WALLS"));
        std::sort(home.buildings.begin(), home.buildings.end());
        home.wallHp += rules().buildings[at(rules().building("BUILDING_ANCIENT_WALLS"))].outerDefenseHp;
        if (encampment) {
            CityDistrict camp;
            camp.type = rules().district("DISTRICT_ENCAMPMENT");
            camp.pos = {7, 6};
            camp.complete = true;
            home.districts.push_back(camp);
            sovtest::claimFor(s, home, camp.pos);
        }
        // Beside the city, or beside the Encampment and out of the city's reach.
        const UnitId foe = addUnit(s, "UNIT_WARRIOR", 1, encampment ? Hex{9, 6} : Hex{5, 6});
        for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Visible));
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
        ai::playTurn(*g);
        const Unit* u = g->state().unit(foe);
        return !u || u->hp < 100;
    };
    CHECK(struck(false));
    CHECK(struck(true));
}

TEST(ai_wins_a_fight_it_should_win) {
    GameState s = flatState(20, 14, 2);
    addCity(s, 0, {4, 6}, true);
    addCity(s, 1, {15, 6}, true);
    const UnitId mine = addUnit(s, "UNIT_WARRIOR", 0, {9, 6});
    const UnitId theirs = addUnit(s, "UNIT_WARRIOR", 1, {10, 6});
    s.unit(theirs)->hp = 20;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->state().currentPlayer == 0);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    ai::playTurn(*g);
    CHECK(g->state().unit(theirs) == nullptr);  // a sure kill is taken
    CHECK(g->state().unit(mine) != nullptr);
}

TEST(ai_declares_war_on_a_weak_neighbour) {
    GameState s = flatState(24, 14, 2);
    addCity(s, 0, {4, 6}, true);
    addCity(s, 0, {4, 10}, false);
    addCity(s, 1, {12, 6}, true);
    learn(s, 0, "TECH_BRONZE_WORKING");
    for (int i = 0; i < 6; ++i) addUnit(s, "UNIT_WARRIOR", 0, {static_cast<int32_t>(3 + i), 3});
    addUnit(s, "UNIT_WARRIOR", 1, {12, 6});
    s.turn = 60;
    GameState far = s;
    addUnit(s, "UNIT_SCOUT", 0, {10, 6});  // has seen the neighbour's city
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->visibility(0, {12, 6}) != Visibility::Unrevealed);
    ai::playTurn(*g);
    CHECK(g->atWar(0, 1));
    // A neighbour it has never seen is left alone.
    auto blind = Game::fromScenario(rules(), std::move(far));
    REQUIRE(blind->visibility(0, {12, 6}) == Visibility::Unrevealed);
    ai::playTurn(*blind);
    CHECK(!blind->atWar(0, 1));
}

TEST(ai_beats_the_random_bot) {
    GameSetup setup;
    setup.seed = 3;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    Rng botRng(99);
    while (g->state().turn < 120 && !g->gameOver()) {
        if (g->state().currentPlayer < 2) ai::playTurn(*g);
        else sovbot::playTurn(*g, botRng);
    }
    const int aiCities = citiesOf(*g, 0) + citiesOf(*g, 1), botCities = citiesOf(*g, 2) + citiesOf(*g, 3);
    const int aiPop = populationOf(*g, 0) + populationOf(*g, 1), botPop = populationOf(*g, 2) + populationOf(*g, 3);
    CHECK(aiCities > botCities);
    CHECK(aiPop > botPop);
}

TEST(ai_soak_takes_a_capital_and_replays) {
    GameSetup setup;
    setup.seed = 2;
    setup.mapSize = "MAPSIZE_TINY";
    for (int i = 0; i < 4; ++i) setup.players.push_back({rules().civs[at(static_cast<TypeIndex>(i))].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    while (g->state().turn < 250 && !capitalTaken(*g) && !g->gameOver()) ai::playTurn(*g);
    CHECK(capitalTaken(*g));
    auto again = Game::replay(rules(), setup, g->log(), &err);
    REQUIRE(again);
    CHECK_EQ(again->stateHash(), g->stateHash());
}

namespace {
bool holds(const Game& g, PlayerId p, ai::Strategy s) {
    const std::vector<ai::Strategy> on = ai::strategies(g, p);
    return std::find(on.begin(), on.end(), s) != on.end();
}
}  // namespace

TEST(ai_grand_strategy_rapid_expansion_and_dark_age) {
    GameState s = flatState(30, 20, 1);
    addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    ai::playTurn(*g);  // founds the capital
    REQUIRE(citiesOf(*g, 0) == 1);
    GameState s1 = g->state();
    for (uint8_t& v : s1.players[0].visibility) v = std::max(v, static_cast<uint8_t>(Visibility::Revealed));  // open land all round, explored
    auto g1 = Game::fromScenario(rules(), s1);
    CHECK(holds(*g1, 0, ai::Strategy::RapidExpansion));
    CHECK(!holds(*g1, 0, ai::Strategy::ScienceVictory));  // no victory strategy in the Ancient era
    GameState s2 = s1;
    s2.players[0].age = Age::Dark;
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK(holds(*g2, 0, ai::Strategy::DarkAge));
    CHECK(!holds(*g2, 0, ai::Strategy::RapidExpansion));  // Rapid Expansion is forbidden in a Dark Age
    CHECK_EQ(std::string(ai::strategyName(ai::Strategy::DarkAge)), std::string("Dark Age"));
}

TEST(ai_grand_strategy_domination_after_taking_a_capital) {
    GameState s = flatState(30, 14, 3);
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 0, {10, 6}, false, 4);
    s.cities.back().originalOwner = 1;
    s.cities.back().originalCapital = true;  // player 1's old capital
    addCity(s, 1, {18, 6}, false, 3);
    addCity(s, 2, {25, 6}, true, 3);
    for (int i = 0; i < 4; ++i) addUnit(s, "UNIT_SWORDSMAN", 0, {4, 8 + i % 2});
    s.gameEra = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(holds(*g, 0, ai::Strategy::DominationVictory));  // took a capital, two rivals, the strongest army
    CHECK(!holds(*g, 1, ai::Strategy::DominationVictory));
}

// Being a fifth above the other majors' average in Science, Culture or Faith is one of the conditions its victory
// counts. Player 0 meets two of the others for one victory (three are needed) and too few for the rest, so the lead
// decides it; of two victories met as well, the first listed is taken, unless the other's condition wins outright.
TEST(ai_grand_strategy_counts_a_yield_lead) {
    enum { Campus = 1, Library = 2, Works = 4, RivalWorks = 8, Religion = 16, Shrine = 32, Points = 64 };
    // The victory strategy player 0 picks in the Classical era (Count: none), against one rival with two cities.
    const auto victory = [](int has) {
        GameState s = flatState(30, 14, 2);
        addCity(s, 0, {6, 6}, true);
        addCity(s, 1, {16, 6}, true);
        addCity(s, 1, {24, 6}, false);
        const auto add = [&](City& c, const char* district, Hex at, std::initializer_list<const char*> buildings) {
            c.districts.push_back({rules().district(district), at, true});
            for (const char* b : buildings) c.buildings.push_back(rules().building(b));
            std::sort(c.buildings.begin(), c.buildings.end());
        };
        // Three Great Works of Writing: two in an Amphitheater, one in the Palace.
        const auto works = [&](City& c) {
            add(c, "DISTRICT_THEATER_SQUARE", {c.pos.x, c.pos.y + 1}, {"BUILDING_AMPHITHEATER"});
            for (const char* b : {"BUILDING_AMPHITHEATER", "BUILDING_AMPHITHEATER", "BUILDING_PALACE"}) {
                GreatWork w;
                w.type = rules().greatWorkType("WRITING");
                w.building = rules().building(b);
                c.greatWorks.push_back(w);
            }
        };
        if (has & Campus) add(s.cities[0], "DISTRICT_CAMPUS", {7, 6}, {});
        if (has & Library) s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
        if (has & Library) s.cities[0].buildings.push_back(rules().building("BUILDING_UNIVERSITY"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        if (has & Works) works(s.cities[0]);
        if (has & RivalWorks) works(s.cities[1]);
        if (has & Religion) {
            s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {rules().belief("BELIEF_TITHE")}});
            s.players[0].religion = 0;
        }
        if (has & Shrine) add(s.cities[0], "DISTRICT_HOLY_SITE", {5, 6}, {"BUILDING_SHRINE"});
        if (has & Points) {
            const int need = rules().globalInt("DIPLOMATIC_VICTORY_POINTS_REQUIRED");
            s.players[0].diplomaticVictoryPoints = need * 6 / 10;  // near enough to win outright
            s.players[1].diplomaticVictoryPoints = need - 1;      // but behind the rival
        }
        s.gameEra = 1;
        auto g = Game::fromScenario(rules(), std::move(s));
        for (ai::Strategy v : {ai::Strategy::ScienceVictory, ai::Strategy::CultureVictory, ai::Strategy::ReligiousVictory,
                               ai::Strategy::DominationVictory, ai::Strategy::DiplomaticVictory}) {
            if (holds(*g, 0, v)) return v;
        }
        return ai::Strategy::Count;
    };
    // Science: a Campus (one a city) and as many techs as the rival; the Library and University give the lead.
    CHECK(victory(Campus) == ai::Strategy::Count);
    CHECK(victory(Campus | Library) == ai::Strategy::ScienceVictory);
    // Culture: three Great Works, whose own culture is the lead unless the rival has as many.
    CHECK(victory(Works) == ai::Strategy::CultureVictory);
    CHECK(victory(Works | RivalWorks) == ai::Strategy::Count);
    // Religion: its own, and the rival's two cities follow none; the Shrine's faith gives the lead (none on either side
    // is no lead).
    CHECK(victory(Religion) == ai::Strategy::Count);
    CHECK(victory(Religion | Shrine) == ai::Strategy::ReligiousVictory);
    // Science and Culture both met: Science, listed first. Diplomatic points near a win take it over either.
    CHECK(victory(Campus | Library | Works) == ai::Strategy::ScienceVictory);
    CHECK(victory(Campus | Library | Points) == ai::Strategy::DiplomaticVictory);
}

TEST(ai_grand_strategy_agendas) {
    TypeIndex qin = kNone;
    for (size_t i = 0; i < rules().civs.size(); ++i) {
        if (rules().civs[i].agenda == Agenda::FirstEmperor) qin = static_cast<TypeIndex>(i);
    }
    REQUIRE(qin != kNone);
    GameState s = flatState(20, 14, 1);
    s.players[0].civ = qin;
    addCity(s, 0, {6, 6}, true, 3);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(holds(*g, 0, ai::Strategy::WonderObsessed));
}

TEST(ai_gathers_before_assaulting_walls) {
    GameState s = flatState(30, 14, 2);
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 1, {20, 6}, true, 6);
    s.cities.back().wallHp = 100;  // walled
    s.players[0].visibility.resize(static_cast<size_t>(s.grid.size()), 0);
    for (const Hex& h : s.grid.within({20, 6}, 3)) s.players[0].visibility[static_cast<size_t>(s.grid.index(h))] = static_cast<uint8_t>(Visibility::Revealed);
    std::vector<UnitId> army;
    for (int i = 0; i < 2; ++i) army.push_back(addUnit(s, "UNIT_WARRIOR", 0, {12, static_cast<int32_t>(5 + i)}));
    addUnit(s, "UNIT_WARRIOR", 0, {4, 6});  // the garrison
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    for (int t = 0; t < 4; ++t) {
        ai::playTurn(*g);
        while (g->state().currentPlayer != 0 && !g->gameOver()) ai::playTurn(*g);
    }
    // Two Warriors are far short of three times a walled city's strength: they wait at the staging ring.
    for (UnitId id : army) {
        const Unit* u = g->state().unit(id);
        if (u) CHECK(g->state().grid.distance(u->pos, {20, 6}) >= 3);
    }
}

TEST(ai_uses_raffles_on_a_city_state_it_leads) {
    // Stamford Raffles walks into the land of a city-state the AI is suzerain of and absorbs it.
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.players[1].civ = kNone;
    s.players[1].cityState = rules().cityState("CITYSTATE_MITLA");
    for (Player& p : s.players) {
        p.envoys.assign(2, 0);
        p.met.assign(2, 0);
        p.relations.resize(2);
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    }
    s.players[0].met[1] = s.players[1].met[0] = 1;
    s.players[0].envoys[1] = 3;
    addCity(s, 0, {6, 6}, true, 4);
    const CityId town = addCity(s, 1, {12, 6}, true, 2);
    const TypeIndex gp = rules().greatPerson("GREAT_PERSON_STAMFORD_RAFFLES");
    const UnitId raffles = addUnit(s, rules().units[at(rules().greatPersonClasses[at(rules().greatPeople[at(gp)].cls)].unit)].id.c_str(), 0, {6, 6});
    s.units.back().greatPerson = gp;
    s.units.back().charges = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    for (int i = 0; i < 12 && g->state().unit(raffles); ++i) ai::playTurn(*g);
    CHECK(!g->state().unit(raffles));
    CHECK_EQ(g->state().city(town)->owner, 0);
}

TEST(ai_walks_great_people_to_where_they_work) {
    // Galileo goes beside the most Mountains on the city's land, Zhou Daguan into a known city-state's land and
    // Isidore of Miletus onto the plot of the wonder the city is building.
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    s.players[1].civ = kNone;
    s.players[1].cityState = rules().cityState("CITYSTATE_MITLA");
    for (Player& p : s.players) {
        p.envoys.assign(2, 0);
        p.met.assign(2, 0);
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    }
    s.players[0].met[1] = s.players[1].met[0] = 1;
    const CityId capital = addCity(s, 0, {6, 6}, true, 4);
    addCity(s, 1, {16, 6}, true, 2);
    for (const Hex& h : s.grid.within({6, 6}, 3)) {
        s.plot(h).owner = 0;
        s.plot(h).city = capital;
    }
    // Two Mountains on the edge of the city's land, both beside (9,6).
    std::vector<Hex> peaks;
    for (const Hex& h : s.grid.within({9, 6}, 1)) {
        if (h != Hex{9, 6} && s.grid.distance(h, {6, 6}) == 3 && peaks.size() < 2) peaks.push_back(h);
    }
    REQUIRE(peaks.size() == 2u);
    for (const Hex& h : peaks) s.plot(h).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    auto spawn = [&](const char* who) {
        const TypeIndex gp = rules().greatPerson(who);
        const UnitId id = addUnit(s, rules().units[at(rules().greatPersonClasses[at(rules().greatPeople[at(gp)].cls)].unit)].id.c_str(), 0, {6, 6});
        s.units.back().greatPerson = gp;
        s.units.back().charges = 1;
        return id;
    };
    const UnitId galileo = spawn("GREAT_PERSON_GALILEO_GALILEI");
    const UnitId zhou = spawn("GREAT_PERSON_ZHOU_DAGUAN");
    const UnitId isidore = spawn("GREAT_PERSON_ISIDORE_OF_MILETUS");
    const TypeIndex pyramids = rules().building("BUILDING_PYRAMIDS");
    s.cities[0].wonders.push_back({pyramids, {4, 5}});
    auto g = Game::fromScenario(rules(), std::move(s));
    Hex used = {6, 6};  // where Galileo was when he was used
    for (int i = 0; i < 16 && (g->state().unit(galileo) || g->state().unit(zhou) || g->state().unit(isidore)); ++i) {
        if (const Unit* gal = g->state().unit(galileo)) used = gal->pos;
        ai::playTurn(*g);  // both players' turns
    }
    CHECK(!g->state().unit(galileo));
    for (const Hex& h : peaks) CHECK_EQ(g->state().grid.distance(used, h), 1);
    CHECK(!g->state().unit(zhou));
    const std::vector<TypeIndex>& done = g->state().players[0].greatPeopleActivated;
    CHECK(std::count(done.begin(), done.end(), rules().greatPerson("GREAT_PERSON_GALILEO_GALILEI")) == 1);
    CHECK(std::count(done.begin(), done.end(), rules().greatPerson("GREAT_PERSON_ZHOU_DAGUAN")) == 1);
    CHECK(g->state().players[0].envoys[1] >= 3);
    CHECK(std::count(done.begin(), done.end(), rules().greatPerson("GREAT_PERSON_ISIDORE_OF_MILETUS")) >= 1);
    const City& home = g->state().cities[0];
    CHECK(home.has(pyramids) || std::any_of(home.progress.begin(), home.progress.end(), [&](const ProductionProgress& p) {
              return p.item.kind == ProductionKind::Building && p.item.type == pyramids && p.amount >= Fixed::fromInt(215);
          }));
}
