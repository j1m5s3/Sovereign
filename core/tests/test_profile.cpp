// Player modelling (leader doc §10, AI layer 2): the profile each major civ builds up from play.
#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t cls(ProfileClass c) { return static_cast<size_t>(c); }

GameState twoCivs() {
    GameState s = flatState(30, 14, 2);
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 1, {20, 6}, true, 3);
    return s;
}

void sleepAll(GameState& s) {
    for (Unit& u : s.units) u.activity = Activity::Sleep;
}
}  // namespace

TEST(profile_tracks_army_mix_and_militarism) {
    GameState s = twoCivs();
    for (int i = 0; i < 3; ++i) addUnit(s, "UNIT_HORSEMAN", 0, {4, static_cast<int32_t>(8 + i)});
    addUnit(s, "UNIT_WARRIOR", 0, {5, 8});
    addUnit(s, "UNIT_WARRIOR", 1, {20, 8});
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->profile(0) == nullptr || g->profile(0)->turnsObserved == 0);
    endTurns(*g, 2 * 6);  // six world turns
    const PlayerProfile* p = g->profile(0);
    REQUIRE(p != nullptr);
    CHECK_EQ(p->turnsObserved, 6);
    CHECK(p->army[cls(ProfileClass::LightCavalry)] > p->army[cls(ProfileClass::Melee)]);
    CHECK(p->army[cls(ProfileClass::Melee)] > 0);
    CHECK(p->militarism > g->profile(1)->militarism);
}

TEST(profile_remembers_wars_declared_and_armies_massed) {
    GameState s = twoCivs();
    for (int i = 0; i < 3; ++i) addUnit(s, "UNIT_WARRIOR", 0, {static_cast<int32_t>(17 + i), 4});  // camped by player 1's city
    sleepAll(s);
    auto g = Game::fromScenario(rules(), s);
    endTurns(*g, 4);
    const int massed = g->profile(0)->aggression;
    CHECK(massed > 0);
    CHECK_EQ(g->profile(1)->aggression, 0);
    auto g2 = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g2->submit(Command::declareWar(0, 1)) == CommandError::Ok);  // a surprise war
    endTurns(*g2, 4);
    CHECK_EQ(g2->profile(0)->warsDeclared, 1);
    CHECK_EQ(g2->profile(0)->surpriseWars, 1);
    CHECK(g2->profile(0)->aggression > massed);
}

TEST(profile_watches_the_leader) {
    if (rules().leaderUnit == kNone) return;
    GameState s = twoCivs();
    addUnit(s, rules().units[static_cast<size_t>(rules().leaderUnit)].id.c_str(), 0, {9, 6});  // out in the field
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    endTurns(*g, 6);
    CHECK(g->profile(0)->leaderOutside > 0);
}

TEST(profiles_survive_a_save) {
    GameState s = twoCivs();
    addUnit(s, "UNIT_HORSEMAN", 0, {5, 8});
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    endTurns(*g, 6);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    REQUIRE(loaded->profile(0) != nullptr);
    CHECK_EQ(loaded->profile(0)->army[cls(ProfileClass::LightCavalry)], g->profile(0)->army[cls(ProfileClass::LightCavalry)]);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(ai_counters_a_cavalry_neighbour_with_pikes) {
    // Player 0 (AI) has two Warriors (one garrisoned) and knows Archery and Bronze Working: it would
    // add an Archer for balance, but a horse-heavy neighbour makes it train Spearmen instead.
    auto scene = [](bool cavalryNeighbour, int difficulty) {
        GameState s = twoCivs();
        s.cities.erase(s.cities.begin() + 1);
        addCity(s, 1, {14, 6}, true, 3);
        s.setup.difficulty = difficulty;
        Player& p = s.players[0];
        p.techs.resize(rules().techs.size());
        for (const char* t : {"TECH_ARCHERY", "TECH_BRONZE_WORKING"}) p.techs.done[static_cast<size_t>(rules().tech(t))] = 1;
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
        addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
        s.cities[0].queue.clear();
        s.cities[0].population = 1;  // no Settler yet
        // At war, so a soldier outranks the builders and buildings.
        s.players[0].relations.resize(2);
        s.players[1].relations.resize(2);
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        s.profiles.resize(s.players.size());
        PlayerProfile& theirs = s.profiles[1];
        theirs.turnsObserved = 10;
        theirs.army[static_cast<size_t>(cavalryNeighbour ? ProfileClass::LightCavalry : ProfileClass::Melee)] = 900;
        sleepAll(s);
        return Game::fromScenario(rules(), std::move(s));
    };
    auto produces = [](const Game& g) {
        const City& c = g.state().cities[0];
        return c.queue.empty() || c.queue.front().kind != ProductionKind::Unit ? std::string() : rules().units[static_cast<size_t>(c.queue.front().type)].id;
    };
    auto g = scene(true, 3);
    ai::playTurn(*g);
    auto plain = scene(false, 3);
    ai::playTurn(*plain);
    CHECK_EQ(produces(*plain), std::string("UNIT_ARCHER"));
    CHECK_EQ(produces(*g), std::string("UNIT_SPEARMAN"));
    // Settler and Chieftain AIs ignore the profile.
    auto low = scene(true, 0);
    ai::playTurn(*low);
    CHECK(produces(*low) != "UNIT_SPEARMAN");
}

TEST(profiles_carry_between_games_as_text) {
    PlayerProfile p;
    p.turnsObserved = 240;
    p.army[cls(ProfileClass::HeavyCavalry)] = 640;
    p.battleFlank = 410;
    p.surpriseWars = 2;
    p.citiesHeld = 3;
    const std::string text = profileToText(p);
    CHECK(text.rfind("sovereign-profile 1\n", 0) == 0);
    PlayerProfile back;
    REQUIRE(profileFromText(text + "futureField 7\n", back));  // unknown keys are skipped
    CHECK_EQ(back.army[cls(ProfileClass::HeavyCavalry)], 640);
    CHECK_EQ(back.battleFlank, 410);
    CHECK_EQ(back.surpriseWars, 2);
    PlayerProfile bad;
    CHECK(!profileFromText("not a profile\n", bad));
    CHECK(!profileFromText("sovereign-profile 1\nbattles lots\n", bad));
    // A new game starts from it: the human is known from turn 1 (this game's conquests start at zero).
    GameSetup setup;
    setup.seed = 5;
    setup.mapSize = "MAPSIZE_DUEL";
    setup.players.push_back({rules().civs[0].id, true, true, back});
    setup.players.push_back({rules().civs[1].id, false});
    std::string err;
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    REQUIRE(g->profile(0) != nullptr);
    CHECK_EQ(g->profile(0)->army[cls(ProfileClass::HeavyCavalry)], 640);
    CHECK_EQ(g->profile(0)->citiesHeld, 0);
    CHECK_EQ(g->profile(1)->turnsObserved, 0);
    auto loaded = loadGame(rules(), saveGame(*g), &err);  // the setup's profile is saved too
    REQUIRE(loaded);
    CHECK(loaded->state().setup.players[0].hasProfile);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(rivals_carry_between_games_as_text) {
    RivalMemory rome;
    rome.civ = "CIVILIZATION_ROME";
    rome.games = 3;
    rome.leadersLost = 2;
    rome.friendTurns = 40;
    RivalMemory egypt;
    egypt.civ = "CIVILIZATION_EGYPT";
    egypt.betrayals = 1;
    const std::string text = rivalsToText({rome, egypt});
    CHECK(text.rfind("sovereign-rivals 1\n", 0) == 0);
    std::vector<RivalMemory> back;
    REQUIRE(rivalsFromText(text + "CIVILIZATION_ROME.futureField 9\n", back));  // unknown fields are skipped
    REQUIRE(back.size() == 2u);
    CHECK_EQ(back[0].civ, std::string("CIVILIZATION_ROME"));
    CHECK_EQ(back[0].games, 3);
    CHECK_EQ(back[0].leadersLost, 2);
    CHECK_EQ(back[0].friendTurns, 40);
    CHECK_EQ(back[1].betrayals, 1);
    std::vector<RivalMemory> bad;
    CHECK(!rivalsFromText("sovereign-profile 1\n", bad));
    CHECK(!rivalsFromText("sovereign-rivals 1\nCIVILIZATION_ROME.games many\n", bad));
    CHECK(!rivalsFromText("sovereign-rivals 1\nnodot 3\n", bad));
}

TEST(rivals_remember_grudges_and_respect) {
    // Player 1's civ remembers human 0 from earlier games: two of its rulers taken and a betrayal.
    GameState s = twoCivs();
    s.players[0].human = true;
    RivalMemory m;
    m.civ = rules().civs[static_cast<size_t>(s.players[1].civ)].id;
    m.leadersLost = 2;
    m.betrayals = 1;
    m.friendTurns = 100;
    s.setup.players[0].rivals = {m};
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->rivalMemory(1, 0) != nullptr);
    CHECK(g->rivalMemory(0, 1) == nullptr);  // only AI civs remember, and only humans
    CHECK_EQ(g->rivalGrudge(1, 0), 17);
    CHECK_EQ(g->rivalRespect(1, 0), 10);
    int past = 0;
    for (const OpinionReason& r : g->opinionReasons(1, 0)) past += r.kind == OpinionReasonKind::PastGames ? r.value : 0;
    CHECK_EQ(past, -7);
    CHECK(std::string(opinionReasonName(OpinionReasonKind::PastGames)).size() > 0);
    // Turned off in the setup, nothing is remembered.
    s.setup.rivalMemory = false;
    auto off = Game::fromScenario(rules(), std::move(s));
    CHECK(off->rivalMemory(1, 0) == nullptr);
    for (const OpinionReason& r : off->opinionReasons(1, 0)) CHECK(r.kind != OpinionReasonKind::PastGames);
}

TEST(rivals_tally_this_game_and_survive_a_save) {
    GameState s = twoCivs();
    s.players[0].human = true;
    RivalMemory m;
    m.civ = rules().civs[static_cast<size_t>(s.players[1].civ)].id;
    m.games = 2;
    m.wars = 1;
    s.setup.players[0].rivals = {m};
    for (Player& p : s.players) p.met.assign(s.players.size(), 1);
    sleepAll(s);
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);  // a surprise war: a betrayal
    endTurns(*g, 4);
    const std::vector<RivalMemory> mem = g->rivalMemories(0);
    REQUIRE(mem.size() == 1u);
    CHECK_EQ(mem[0].games, 3);  // two earlier games and this one
    CHECK_EQ(mem[0].wars, 2);
    CHECK_EQ(mem[0].betrayals, 1);
    CHECK(g->rivalMemories(1).empty());
    // The tally and the carried memory are part of the saved game.
    std::string err;
    auto back = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(back);
    CHECK_EQ(back->stateHash(), g->stateHash());
    CHECK_EQ(back->rivalMemories(0)[0].betrayals, 1);
    CHECK_EQ(back->rivalGrudge(1, 0), 2);
}
// Rival memory also counts rulers killed by assassins, promises the human broke, and turns as allies (player-retention:
// "captured or assassinated leaders", "broken promises", "long alliances carry over").
TEST(rivals_remember_assassinations_broken_promises_and_alliances) {
    GameState s = twoCivs();
    s.players[0].human = true;
    for (Player& p : s.players) {
        p.met.assign(s.players.size(), 1);
        p.relations.resize(s.players.size());
    }
    sleepAll(s);
    const int turn = s.turn;
    s.events.push_back({turn, EventKind::AssassinKilledLeader, 1, 0, 50});  // its assassin killed the human's ruler
    s.events.push_back({turn, EventKind::AssassinKilledLeader, 0, 1, 50});  // and the human's killed its ruler
    s.events.push_back({turn, EventKind::AssassinWoundedLeader, 0, 1, 20});  // a wound takes no one
    s.promises.push_back({0, 1, PromiseKind::NoSettling, turn + 30, turn});  // broken by the human
    s.promises.push_back({1, 0, PromiseKind::NoSettling, turn + 30, turn});  // broken by the AI: not the human's
    s.promises.push_back({0, 1, PromiseKind::NoSettling, turn + 30, 0});     // kept
    for (int i = 0; i < 2; ++i) {
        Relation& r = s.players[static_cast<size_t>(i)].relations[static_cast<size_t>(1 - i)];
        r.alliance = AllianceType::Research;
        r.allianceUntil = turn + 100;
    }
    RivalMemory carried;  // from an earlier game
    carried.civ = rules().civs[static_cast<size_t>(s.players[1].civ)].id;
    carried.promisesBroken = 1;
    s.setup.players[0].rivals = {carried};
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(!g->friends(0, 1));
    REQUIRE(g->alliance(0, 1) == AllianceType::Research);
    endTurns(*g, 4);  // two turns
    const std::vector<RivalMemory> mem = g->rivalMemories(0);
    REQUIRE(mem.size() == 1u);
    CHECK_EQ(mem[0].leadersTaken, 1);
    CHECK_EQ(mem[0].leadersLost, 1);
    CHECK_EQ(mem[0].promisesBroken, 2);  // one carried, one this game
    CHECK_EQ(mem[0].friendTurns, 2);
    // The text file carries the new count; a broken promise weighs 4 in the grudge.
    std::vector<RivalMemory> back;
    REQUIRE(rivalsFromText(rivalsToText(mem), back));
    CHECK_EQ(back[0].promisesBroken, 2);
    RivalMemory m;
    m.civ = mem[0].civ;
    m.promisesBroken = 2;
    GameState t = twoCivs();
    t.players[0].human = true;
    t.setup.players[0].rivals = {m};
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->rivalGrudge(1, 0), 8);
}
