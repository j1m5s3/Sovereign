// Trade routes and roads (07-economy-trade-great-people.md, Trade routes; 01, Routes).
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
size_t yi(YieldType y) { return static_cast<size_t>(y); }

void giveCivic(GameState& s, PlayerId p, const char* civic) {
    Player& pl = s.players[at(p)];
    Game::fitPlayerToRules(pl, rules());
    pl.civics.done[at(rules().civic(civic))] = 1;
}

// Ends turns, keeping every city building and every unit asleep so nothing blocks.
void endTurnsAuto(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        const PlayerId me = g.state().currentPlayer;
        for (CityId c : g.citiesNeedingProduction(me)) g.submit(Command::setProduction(me, c, g.buildableItems(c).front()));
        for (UnitId u : g.unitsNeedingOrders(me)) g.submit(Command::setActivity(me, u, Activity::Sleep));
        sovtest::endTurns(g, 1);
    }
}

// Player 0: cities at (4,6) and (12,6); player 1: a city at (20,6). Foreign Trade known.
GameState tradeState() {
    GameState s = flatState(30, 14, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    }
    giveCivic(s, 0, "CIVIC_FOREIGN_TRADE");
    addCity(s, 0, {4, 6}, true, 3);
    addCity(s, 0, {12, 6}, false, 3);
    addCity(s, 1, {20, 6}, true, 3);
    return s;
}
}  // namespace

TEST(trade_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.buildings[at(r.building("BUILDING_MARKET"))].tradeCapacity, 1);
    const BuildingType& lighthouse = r.buildings[at(r.building("BUILDING_LIGHTHOUSE"))];
    CHECK_EQ(lighthouse.tradeCapacity, 1);
    CHECK_EQ(lighthouse.tradeCapacityUnless, r.building("BUILDING_MARKET"));
    CHECK(r.civics[at(r.civic("CIVIC_FOREIGN_TRADE"))].tradeCapacity);
    const DistrictType& center = r.districts[at(r.district("DISTRICT_CITY_CENTER"))];
    CHECK_EQ(center.tradeDomestic[yi(YieldType::Food)], Fixed::fromInt(1));
    CHECK_EQ(center.tradeInternational[yi(YieldType::Gold)], Fixed::fromInt(3));
    REQUIRE(r.routes.size() == 5u);  // four roads by era, then the railroad
    CHECK(r.routes[4].unitOnly);
    CHECK_EQ(r.routes[4].moveCost, Fixed::ratio(1, 4));
    CHECK_EQ(r.routes[4].resourceCost.size(), 2u);
    CHECK(!r.routes[0].bridges);
    CHECK(r.routes[1].bridges);
    CHECK_EQ(r.routes[3].moveCost, Fixed::ratio(1, 2));
}

// GAME_PROGRESS 400 (data: units): a Trader's 40 rises by up to 400% of itself with the larger share of the tech or
// civic tree its player has completed (districts' Cost progression).
TEST(traders_cost_more_as_the_game_progresses) {
    const auto cost = [](size_t techs, size_t civics) {
        GameState s = tradeState();
        Player& p = s.players[0];
        std::fill(p.techs.done.begin(), p.techs.done.end(), 0);
        std::fill(p.civics.done.begin(), p.civics.done.end(), 0);
        for (size_t t = 0; t < techs; ++t) p.techs.done[t] = 1;
        for (size_t c = 0; c < civics; ++c) p.civics.done[c] = 1;
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->productionCost(0, {ProductionKind::Unit, rules().unit("UNIT_TRADER")});
    };
    const size_t techs = rules().techs.size(), civics = rules().civics.size();
    CHECK_EQ(cost(0, 0), 40);
    CHECK_EQ(cost(techs, 0), 200);
    CHECK_EQ(cost(0, civics), 200);
    CHECK_EQ(cost(techs / 2, civics), 200);  // the larger share counts
    const int half = cost(techs / 2, 0);     // about 40 x (1 + 4 x 1/2)
    CHECK(half >= 115 && half <= 120);
}

TEST(capacity_from_foreign_trade_markets_and_lighthouses) {
    GameState s = tradeState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_MARKET"));
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIGHTHOUSE"));  // no extra: the city has a Market
    s.cities[1].buildings.push_back(rules().building("BUILDING_LIGHTHOUSE"));
    for (City& c : s.cities) std::sort(c.buildings.begin(), c.buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->tradeRouteCapacity(0), 3);  // civic + Market + Lighthouse without a Market
    CHECK_EQ(g->tradeRouteCapacity(1), 0);
    // Rome's Forum is its Market: a Lighthouse beside it adds nothing either.
    GameState t = tradeState();
    t.cities[0].buildings.push_back(rules().building("BUILDING_FORUM"));
    t.cities[0].buildings.push_back(rules().building("BUILDING_LIGHTHOUSE"));
    std::sort(t.cities[0].buildings.begin(), t.cities[0].buildings.end());
    auto rome = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(rome->tradeRouteCapacity(0), 2);  // civic + Forum
}

TEST(route_yields_follow_the_destination_districts) {
    GameState s = tradeState();
    s.cities[2].districts.push_back({rules().district("DISTRICT_CAMPUS"), {21, 6}, true});
    auto g = Game::fromScenario(rules(), std::move(s));
    const City& a = g->state().cities[0];
    const Yields home = g->tradeRouteYields(a, g->state().cities[1]);
    CHECK_EQ(home[yi(YieldType::Food)], Fixed::fromInt(1));
    CHECK_EQ(home[yi(YieldType::Production)], Fixed::fromInt(1));
    const Yields abroad = g->tradeRouteYields(a, g->state().cities[2]);
    CHECK_EQ(abroad[yi(YieldType::Gold)], Fixed::fromInt(3));
    CHECK_EQ(abroad[yi(YieldType::Science)], Fixed::fromInt(1));  // the Campus there
}

TEST(a_trader_runs_a_route_lays_roads_and_comes_home) {
    GameState s = tradeState();
    const UnitId trader = addUnit(s, "UNIT_TRADER", 0, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId origin = g->state().cities[0].id, home = g->state().cities[1].id, abroad = g->state().cities[2].id;
    CHECK(g->canStartTradeRoute(trader, home));
    CHECK(g->canStartTradeRoute(trader, abroad));  // 16 tiles, but range refuels in our city on the way (07)
    const Fixed before = g->cityReport(origin).yields[yi(YieldType::Food)];
    REQUIRE(g->submit(Command::startTradeRoute(0, trader, home)) == CommandError::Ok);
    CHECK(!g->state().unit(trader));
    REQUIRE(g->state().tradeRoutes.size() == 1u);
    const std::vector<int32_t>& way = g->state().tradeRoutes[0].path;  // from the origin, 8 steps to the destination
    REQUIRE(way.size() == 9u);
    CHECK_EQ(way.front(), g->state().grid.index({4, 6}));
    CHECK_EQ(way.back(), g->state().grid.index({12, 6}));
    CHECK_EQ(g->tradeRoutesOf(0), 1);
    CHECK_EQ(g->cityReport(origin).yields[yi(YieldType::Food)], before + Fixed::fromInt(1));
    CHECK(g->state().plot({8, 6}).route >= 0);  // a road along the way
    // Whole round trips of 16 turns that reach the 20-turn minimum (07: Duration).
    CHECK_EQ(g->tradeRouteLength(), 20);
    CHECK_EQ(g->state().tradeRoutes[0].turnsLeft, 32);
    const auto traders = [&] {
        int n = 0;
        for (const Unit& u : g->state().units) n += rules().units[at(u.type)].id == "UNIT_TRADER";
        return n;
    };
    endTurnsAuto(*g, 2 * 31);
    CHECK_EQ(g->state().tradeRoutes.size(), 1u);  // still on the road
    const int waiting = traders();
    endTurnsAuto(*g, 2);
    CHECK(g->state().tradeRoutes.empty());
    CHECK(g->state().city(home)->hasTradingPost(0));  // the route left a Trading Post (07)
    CHECK(!sovtest::hasMoment(*g, 0, "MOMENT_TRADING_POST_ESTABLISHED_IN_NEW_CIVILIZATION"));  // its own city: no new civ
    CHECK_EQ(traders(), waiting + 1);  // back home
}

TEST(trading_posts_extend_range_and_pay_on_the_way) {
    GameState s = tradeState();
    s.cities[1].owner = 1;  // the middle city is foreign now
    for (Plot& p : s.plots) p.owner = p.city == s.cities[1].id ? 1 : p.owner;
    const UnitId trader = addUnit(s, "UNIT_TRADER", 0, {4, 6});
    auto none = Game::fromScenario(rules(), s);
    CHECK(!none->canStartTradeRoute(trader, none->state().cities[2].id));  // 16 tiles: out of a land route's 15
    s.cities[1].tradingPosts = {1, 0};
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId origin = g->state().cities[0].id, far = g->state().cities[2].id;
    REQUIRE(g->canStartTradeRoute(trader, far));
    const Fixed before = g->cityReport(origin).yields[yi(YieldType::Gold)];
    const Fixed route = g->tradeRouteYields(g->state().cities[0], g->state().cities[2])[yi(YieldType::Gold)];
    REQUIRE(g->submit(Command::startTradeRoute(0, trader, far)) == CommandError::Ok);
    CHECK_EQ(g->cityReport(origin).yields[yi(YieldType::Gold)], before + route + Fixed::fromInt(1));  // the post in a foreign city
}

// The cities a Trader may start a route to are those canStartTradeRoute allows one by one: not its own city, nor one of
// a dead, barbarian or free player, of an enemy, never seen, or out of range: overland, or by sea once Traders sail (07),
// on the Ocean once the civ may enter it.
TEST(a_trader_lists_the_cities_it_may_start_a_route_to) {
    const auto setUp = [](const char* strait, std::initializer_list<const char*> techs) {
        GameState s = flatState(36, 14, 7);
        for (int y = 0; y < 14; ++y) {
            for (int x = 22; x <= 24; ++x) s.plot({x, y}).terrain = rules().terrain(strait);  // land beyond it
        }
        for (Player& p : s.players) {
            Game::fitPlayerToRules(p, rules());
            p.relations.resize(s.players.size());
            p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
        }
        giveCivic(s, 0, "CIVIC_FOREIGN_TRADE");
        for (const char* t : techs) s.players[0].techs.done[at(rules().tech(t))] = 1;
        addCity(s, 0, {4, 6}, true, 3);    // the Trader's
        addCity(s, 0, {12, 6}, false, 3);  // ours: range refuels here
        addCity(s, 1, {20, 6}, true, 3);   // 16 tiles away, 8 past ours
        addCity(s, 2, {8, 11}, true, 3);   // an enemy's
        addCity(s, 3, {16, 2}, true, 3);   // a dead player's
        addCity(s, 4, {16, 11}, true, 3);  // a barbarian's
        addCity(s, 5, {8, 1}, true, 3);    // a free city
        addCity(s, 6, {20, 11}, true, 3);  // never seen
        addCity(s, 1, {27, 6}, false, 3);  // over the strait, 15 tiles past ours
        s.players[0].relations[2].war = s.players[2].relations[0].war = true;
        s.players[3].alive = false;
        s.players[4].barbarian = true;
        s.players[5].freeCity = true;
        s.players[0].visibility[static_cast<size_t>(s.grid.index({20, 11}))] = static_cast<uint8_t>(Visibility::Unrevealed);
        addUnit(s, "UNIT_TRADER", 0, {4, 6});
        return s;
    };
    const auto listed = [](const Game& g, UnitId trader) {
        std::vector<CityId> one;
        for (const City& c : g.state().cities) {
            if (g.canStartTradeRoute(trader, c.id)) one.push_back(c.id);
        }
        CHECK(g.tradeDestinations(trader) == one);
        return g.tradeDestinations(trader);
    };
    GameState s = setUp("TERRAIN_COAST", {});
    const UnitId trader = s.units[0].id;
    const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {4, 6});
    auto g = Game::fromScenario(rules(), s);
    const std::vector<City>& cities = g->state().cities;
    const std::vector<CityId> overland{cities[1].id, cities[2].id}, all{cities[1].id, cities[2].id, cities[8].id};
    CHECK(listed(*g, trader) == overland);
    CHECK(listed(*g, warrior).empty());
    CHECK(listed(*Game::fromScenario(rules(), setUp("TERRAIN_COAST", {"TECH_CELESTIAL_NAVIGATION"})), trader) == all);
    CHECK(listed(*Game::fromScenario(rules(), setUp("TERRAIN_OCEAN", {"TECH_CELESTIAL_NAVIGATION"})), trader) == overland);
    CHECK(listed(*Game::fromScenario(rules(), setUp("TERRAIN_OCEAN", {"TECH_CELESTIAL_NAVIGATION", "TECH_CARTOGRAPHY"})), trader) == all);
    // None with every route taken, or once the Trader has moved this turn.
    TradeRoute r;
    r.id = s.nextTradeRouteId++;
    r.owner = 0;
    r.origin = cities[1].id;
    r.destination = cities[2].id;
    r.traderType = rules().unit("UNIT_TRADER");
    r.turnsLeft = 5;
    s.tradeRoutes.push_back(r);
    CHECK(listed(*Game::fromScenario(rules(), s), trader).empty());
    REQUIRE(g->submit(Command::move(0, trader, {5, 6})) == CommandError::Ok);
    REQUIRE(g->submit(Command::move(0, trader, {4, 7})) == CommandError::Ok);  // back beside its city, no moves left
    CHECK(g->state().unit(trader)->movesLeft <= Fixed());
    CHECK(listed(*g, trader).empty());
}

// Mountains turn a Trader's way aside, and a range right across the land stops it.
TEST(mountains_turn_a_traders_way_aside) {
    GameState s = tradeState();
    for (int y = 3; y < 14; ++y) s.plot({16, y}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");  // a pass to the north
    const UnitId trader = addUnit(s, "UNIT_TRADER", 0, {4, 6});
    auto g = Game::fromScenario(rules(), s);
    const CityId home = g->state().cities[1].id, abroad = g->state().cities[2].id;
    CHECK(g->tradeDestinations(trader) == (std::vector<CityId>{home, abroad}));
    const std::vector<Hex> way = g->tradePath(0, rules().unit("UNIT_TRADER"), g->state().cities[0], g->state().cities[2]);
    CHECK(std::any_of(way.begin(), way.end(), [](Hex h) { return h.x == 16 && h.y < 3; }));
    for (int y = 0; y < 3; ++y) s.plot({16, y}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    auto closed = Game::fromScenario(rules(), std::move(s));
    CHECK(closed->tradeDestinations(trader) == (std::vector<CityId>{home}));
    CHECK(!closed->canStartTradeRoute(trader, abroad));
}

TEST(a_raider_at_war_plunders_a_route) {
    GameState s = tradeState();
    const UnitId trader = addUnit(s, "UNIT_TRADER", 0, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::startTradeRoute(0, trader, g->state().cities[1].id)) == CommandError::Ok);
    GameState after = g->state();
    addUnit(after, "UNIT_WARRIOR", 1, {8, 6});
    auto g2 = Game::fromScenario(rules(), std::move(after));
    REQUIRE(g2->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    const Fixed gold = g2->state().players[1].gold;
    endTurnsAuto(*g2, 2);  // player 0's next turn checks the road
    CHECK(g2->state().tradeRoutes.empty());
    CHECK(g2->state().players[1].gold >= gold + Fixed::fromInt(100));  // plus its own income that turn
}

TEST(roads_replace_terrain_costs) {
    GameState s = tradeState();
    s.plot({6, 6}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    s.plot({5, 6}).route = 0;
    s.plot({6, 6}).route = 0;
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {5, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->moveCost(*g->state().unit(w), {5, 6}, {6, 6}).value_or(Fixed()), Fixed::fromInt(1));  // hills, but a road
    CHECK_EQ(g->moveCost(*g->state().unit(w), {6, 6}, {7, 6}).value_or(Fixed()), Fixed::fromInt(1));  // off the road: grassland
    CHECK_EQ(g->roadFor(0), 0);  // an Ancient player lays Ancient roads
}

TEST(trade_routes_survive_a_save) {
    GameState s = tradeState();
    const UnitId trader = addUnit(s, "UNIT_TRADER", 0, {4, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::startTradeRoute(0, trader, g->state().cities[1].id)) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().tradeRoutes.size(), 1u);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

// A route lasts the fewest whole round trips, twice the way's length each, that reach the minimum (07: Duration).
TEST(trade_routes_last_whole_round_trips) {
    auto g = Game::fromScenario(rules(), tradeState());
    REQUIRE(g->tradeRouteLength() == 20);
    CHECK_EQ(g->tradeRouteDuration(1), 20);   // 10 trips of 2
    CHECK_EQ(g->tradeRouteDuration(3), 24);   // 4 trips of 6
    CHECK_EQ(g->tradeRouteDuration(10), 20);  // one trip of 20
    CHECK_EQ(g->tradeRouteDuration(11), 22);  // one trip of 22
    CHECK_EQ(g->tradeRouteDuration(0), 20);   // no shorter than a step
}
