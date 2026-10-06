// Trade routes and roads (07-economy-trade-great-people.md, Trade routes; 01, Routes).
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

TEST(capacity_from_foreign_trade_markets_and_lighthouses) {
    GameState s = tradeState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_MARKET"));
    s.cities[0].buildings.push_back(rules().building("BUILDING_LIGHTHOUSE"));  // no extra: the city has a Market
    s.cities[1].buildings.push_back(rules().building("BUILDING_LIGHTHOUSE"));
    for (City& c : s.cities) std::sort(c.buildings.begin(), c.buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->tradeRouteCapacity(0), 3);  // civic + Market + Lighthouse without a Market
    CHECK_EQ(g->tradeRouteCapacity(1), 0);
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
    CHECK_EQ(g->tradeRoutesOf(0), 1);
    CHECK_EQ(g->cityReport(origin).yields[yi(YieldType::Food)], before + Fixed::fromInt(1));
    CHECK(g->state().plot({8, 6}).route >= 0);  // a road along the way
    CHECK_EQ(g->state().tradeRoutes[0].turnsLeft, 20);
    endTurnsAuto(*g, 2 * 20);
    CHECK(g->state().tradeRoutes.empty());
    CHECK(g->state().city(home)->hasTradingPost(0));  // the route left a Trading Post (07)
    CHECK(!sovtest::hasMoment(*g, 0, "MOMENT_TRADING_POST_ESTABLISHED_IN_NEW_CIVILIZATION"));  // its own city: no new civ
    int traders = 0;
    for (const Unit& u : g->state().units) traders += rules().units[at(u.type)].id == "UNIT_TRADER";
    CHECK_EQ(traders, 1);  // back home
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
