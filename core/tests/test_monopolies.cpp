// Monopolies and Corporations mode (07-economy-trade-great-people.md, Monopolies & Corporations).
#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
TypeIndex luxury() {
    for (size_t r = 0; r < rules().resources.size(); ++r) {
        const ResourceType& rt = rules().resources[r];
        if (rt.cls == ResourceClass::Luxury && rt.frequency > 0) return static_cast<TypeIndex>(r);
    }
    return kNone;
}

// Player 0's city at (5,5) with two improved plots of one luxury at (6,5) and (5,6); player 1 one more at (15,5).
GameState monopolyState(bool economics, bool electricity) {
    GameState s = flatState(20, 12, 2);
    s.setup.monopolies = true;
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId mine = addCity(s, 0, {5, 5}, true, 4);
    const CityId theirs = addCity(s, 1, {15, 5}, true, 4);
    for (const Hex& h : s.grid.within({5, 5}, 2)) s.plot(h).owner = 0, s.plot(h).city = mine;
    for (const Hex& h : s.grid.within({15, 5}, 2)) s.plot(h).owner = 1, s.plot(h).city = theirs;
    const TypeIndex lux = luxury();
    for (const Hex& h : {Hex{6, 5}, Hex{5, 6}, Hex{16, 5}}) {
        Plot& p = s.plot(h);
        p.resource = lux;
        // The luxury's own improvement, as resourceImproved wants it.
        for (size_t i = 0; i < rules().improvements.size(); ++i) {
            const auto& valid = rules().improvements[i].validResources;
            if (std::find(valid.begin(), valid.end(), lux) != valid.end()) p.improvement = static_cast<TypeIndex>(i);
        }
    }
    for (Player& p : s.players) {
        for (size_t t = 0; t < rules().techs.size(); ++t) p.techs.done[t] = 1;  // every luxury revealed
        if (!economics) p.techs.done[static_cast<size_t>(rules().tech("TECH_ECONOMICS"))] = 0;
        if (!electricity) p.techs.done[static_cast<size_t>(rules().tech("TECH_ELECTRICITY"))] = 0;
    }
    return s;
}
}  // namespace

TEST(industries_need_economics_and_one_per_luxury) {
    GameState s = monopolyState(false, false);
    auto early = Game::fromScenario(rules(), s);
    CHECK_EQ(early->industryProblem(0, {6, 5}), CommandError::CannotImprove);  // before Economics
    GameState t = monopolyState(true, false);
    const UnitId builder = addUnit(t, "UNIT_BUILDER", 0, {6, 5});
    auto g = Game::fromScenario(rules(), std::move(t));
    REQUIRE(g->resourceImproved({6, 5}));
    const City& c = g->state().cities[0];
    const Fixed gold = g->plotYields({6, 5}, c)[static_cast<size_t>(YieldType::Gold)];
    REQUIRE(g->submit(Command::buildIndustry(0, builder)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 5}).industry, 1);
    CHECK_EQ(g->plotYields({6, 5}, g->state().cities[0])[static_cast<size_t>(YieldType::Gold)], gold + Fixed::fromInt(2));
    CHECK_EQ(g->industryProblem(0, {5, 6}), CommandError::CannotImprove);  // one Industry per luxury type
    CHECK_EQ(g->industryProblem(0, {6, 5}), CommandError::CannotImprove);  // a Corporation waits for Electricity
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().plot({6, 5}).industry, 1);
}

TEST(a_monopoly_pays_gold_and_tourism) {
    auto g = Game::fromScenario(rules(), monopolyState(true, true));
    // Two of the world's three improved sources: 67%, a monopoly.
    CHECK(g->hasMonopoly(0, luxury()));
    CHECK(!g->hasMonopoly(1, luxury()));
    CHECK_EQ(g->monopolySources(0), 2);
    GameState off = monopolyState(true, true);
    off.setup.monopolies = false;
    auto plain = Game::fromScenario(rules(), std::move(off));
    CHECK_EQ(g->goldPerTurn(0), plain->goldPerTurn(0) + Fixed::fromInt(6));
    CHECK_EQ(plain->monopolySources(0), 0);
}

TEST(only_improved_sources_count_toward_a_monopoly) {
    GameState s = monopolyState(true, true);
    // Two more of the luxury on player 1's land, neither improved: player 0 still holds two of the world's three
    // improved sources.
    for (const Hex& h : {Hex{14, 5}, Hex{15, 6}}) s.plot(h).resource = luxury();
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->hasMonopoly(0, luxury()));
    CHECK_EQ(g->monopolySources(0), 2);
}
