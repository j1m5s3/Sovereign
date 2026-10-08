// Tribal villages (01-map-and-terrain.md: Tribal Villages).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

TEST(village_rewards_from_the_data) {
    const Rules& r = rules();
    REQUIRE(!r.goodies.empty());
    bool gold = false, builder = false, eureka = false;
    for (const GoodyType& g : r.goodies) {
        gold = gold || (g.kind == GoodyKind::Gold && g.amount == 40);
        builder = builder || (g.kind == GoodyKind::Unit && g.unit == r.unit("UNIT_BUILDER"));
        eureka = eureka || g.kind == GoodyKind::Eureka;
    }
    CHECK(gold);
    CHECK(builder);
    CHECK(eureka);
}

TEST(entering_a_village_consumes_it_for_a_reward) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {4, 6}, true, 3);
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
    s.plot({7, 6}).village = true;
    auto g = Game::fromScenario(rules(), s);
    const int xp = g->state().unit(scout)->xp;
    REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
    CHECK(!g->state().plot({7, 6}).village);
    CHECK_EQ(g->state().unit(scout)->xp >= xp + rules().globalInt("EXPERIENCE_ACTIVATE_GOODY_HUT"), true);
    REQUIRE(!g->state().events.empty());
    CHECK(g->state().events.back().kind == EventKind::GoodyHut);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(a_village_contacted_in_the_ancient_era_is_a_moment) {
    // 09: Tribal Village Contacted counts only while the world is in the Ancient Era; the camp moments until the Medieval.
    const auto contacted = [](int era) {
        GameState s = flatState(16, 12, 1);
        Game::fitPlayerToRules(s.players[0], rules());
        addCity(s, 0, {4, 6}, true, 3);
        const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
        s.plot({7, 6}).village = true;
        s.gameEra = era;
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
        return sovtest::hasMoment(*g, 0, "MOMENT_TRIBAL_VILLAGE_CONTACTED");
    };
    CHECK(contacted(0));
    CHECK(!contacted(1));
    const MomentType& camp = rules().moments[static_cast<size_t>(rules().moment("MOMENT_BARBARIAN_CAMP_DESTROYED"))];
    CHECK_EQ(camp.eraMin, static_cast<int>(rules().era("ERA_ANCIENT")));
    CHECK_EQ(camp.eraMax, static_cast<int>(rules().era("ERA_MEDIEVAL")));
}

TEST(the_map_script_scatters_villages) {
    std::string err;
    GameSetup setup = sovtest::duelSetup(5);
    auto g = Game::create(rules(), setup, &err);
    REQUIRE(g);
    int villages = 0;
    for (const Plot& p : g->state().plots) villages += p.village ? 1 : 0;
    CHECK(villages > 0);
    for (int i = 0; i < g->state().grid.size(); ++i) {
        if (!g->state().plots[static_cast<size_t>(i)].village) continue;
        for (const Player& p : g->state().players) CHECK(g->state().grid.distance(p.startPos, g->state().grid.at(i)) >= 4);
    }
    setup.tribalVillages = false;
    auto none = Game::create(rules(), setup, &err);
    REQUIRE(none);
    int left = 0;
    for (const Plot& p : none->state().plots) left += p.village ? 1 : 0;
    CHECK_EQ(left, 0);
}

// A meteor site, the goody hut a meteor shower leaves (09), grants the best Heavy Cavalry unit the civ can train in its
// nearest city (data: Meteor Goodies), its unique one where it has one; with none unlocked yet, the class's first.
TEST(a_meteor_site_grants_heavy_cavalry) {
    const auto explore = [](const Rules& r, const char* civ, std::initializer_list<const char*> techs) {
        GameState s = flatState(16, 12, 1);
        s.players[0].civ = r.civ(civ);
        Game::fitPlayerToRules(s.players[0], r);
        for (const char* tech : techs) s.players[0].techs.done[static_cast<size_t>(r.tech(tech))] = 1;
        addCity(s, 0, {4, 6}, true, 3);
        const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
        s.plot({7, 6}).village = s.plot({7, 6}).meteorSite = true;
        auto g = Game::fromScenario(r, std::move(s));
        const int xp = g->state().unit(scout)->xp;
        REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
        CHECK(!g->state().plot({7, 6}).village);
        CHECK(!g->state().plot({7, 6}).meteorSite);
        CHECK(g->state().unit(scout)->xp >= xp + r.globalInt("EXPERIENCE_ACTIVATE_GOODY_HUT"));
        CHECK(!sovtest::hasMoment(*g, 0, "MOMENT_TRIBAL_VILLAGE_CONTACTED"));  // a village's moment
        REQUIRE(!g->state().events.empty());
        const GameEvent& e = g->state().events.back();
        CHECK(e.kind == EventKind::GoodyHut);
        CHECK_EQ(r.goodies[static_cast<size_t>(e.value)].category, std::string("METEOR"));
        std::string granted;
        for (const Unit& u : g->state().units) {
            if (u.id != scout && u.owner == 0) granted += r.units[static_cast<size_t>(u.type)].id;
        }
        return granted;
    };
    CHECK_EQ(explore(rules(), "CIVILIZATION_ENGLAND", {}), std::string("UNIT_HEAVY_CHARIOT"));
    CHECK_EQ(explore(rules(), "CIVILIZATION_ENGLAND", {"TECH_WHEEL"}), std::string("UNIT_HEAVY_CHARIOT"));
    CHECK_EQ(explore(rules(), "CIVILIZATION_ENGLAND", {"TECH_WHEEL", "TECH_STIRRUPS"}), std::string("UNIT_KNIGHT"));
    CHECK_EQ(explore(rules(), "CIVILIZATION_ARABIA", {"TECH_WHEEL", "TECH_STIRRUPS"}), std::string("UNIT_MAMLUK"));
    CHECK_EQ(explore(rules(), "CIVILIZATION_EGYPT", {}), std::string("UNIT_WAR_CHARIOT"));
    // Never a unit it cannot train, nor a city-state's own.
    Rules untrained = rules();
    untrained.units[static_cast<size_t>(untrained.unit("UNIT_KNIGHT"))].trainable = false;
    CHECK_EQ(explore(untrained, "CIVILIZATION_ENGLAND", {"TECH_WHEEL", "TECH_STIRRUPS"}), std::string("UNIT_HEAVY_CHARIOT"));
    Rules cityStates = rules();
    cityStates.units[static_cast<size_t>(cityStates.unit("UNIT_KNIGHT"))].cityState = 0;
    CHECK_EQ(explore(cityStates, "CIVILIZATION_ENGLAND", {"TECH_WHEEL", "TECH_STIRRUPS"}), std::string("UNIT_HEAVY_CHARIOT"));
}

// A camp may stand on a goody hut: the unit that enters takes the reward and clears the camp, even when the reward is a
// unit (which moves the unit list).
TEST(entering_a_camp_on_a_goody_hut_takes_both) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    addCity(s, 0, {4, 6}, true, 3);
    const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {6, 6});
    s.plot({7, 6}).village = s.plot({7, 6}).meteorSite = true;
    Camp camp;
    camp.id = s.nextCampId++;
    camp.pos = {7, 6};
    camp.tribe = 0;
    s.camps.push_back(camp);
    auto g = Game::fromScenario(rules(), std::move(s));
    const Fixed gold = g->state().players[0].gold;
    REQUIRE(g->submit(Command::move(0, warrior, {7, 6})) == CommandError::Ok);
    CHECK(!g->state().plot({7, 6}).village);
    CHECK(g->state().camps.empty());
    CHECK(g->state().players[0].gold > gold);
    REQUIRE(g->state().unit(warrior));
    CHECK(g->state().unit(warrior)->pos == (Hex{7, 6}));
    CHECK_EQ(g->state().units.size(), size_t{2});  // and the Heavy Chariot it found
}

// A village never draws the meteor site's reward, and a meteor site draws nothing else.
TEST(villages_and_meteor_sites_keep_their_own_rewards) {
    for (int k = 0; k < 40; ++k) {
        for (bool meteor : {false, true}) {
            GameState s = flatState(16, 12, 1);
            Game::fitPlayerToRules(s.players[0], rules());
            s.rng.seed(static_cast<uint64_t>(100 + k));
            s.turn = 100;  // every reward's minimum turn has come
            addCity(s, 0, {4, 6}, true, 3);
            const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {6, 6});
            s.plot({7, 6}).village = true;
            s.plot({7, 6}).meteorSite = meteor;
            auto g = Game::fromScenario(rules(), std::move(s));
            REQUIRE(g->submit(Command::move(0, scout, {7, 6})) == CommandError::Ok);
            REQUIRE(!g->state().events.empty());
            const GameEvent& e = g->state().events.back();
            REQUIRE(e.kind == EventKind::GoodyHut);
            CHECK_EQ(rules().goodies[static_cast<size_t>(e.value)].category == "METEOR", meteor);
        }
    }
}

TEST(a_meteor_site_survives_a_save) {
    GameState s = flatState(16, 12, 1);
    Game::fitPlayerToRules(s.players[0], rules());
    s.plot({7, 6}).village = s.plot({7, 6}).meteorSite = true;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->state().plot({7, 6}).meteorSite);
    CHECK(loaded->state().plot({7, 6}).village);
    CHECK(!loaded->state().plot({8, 6}).meteorSite);
}
