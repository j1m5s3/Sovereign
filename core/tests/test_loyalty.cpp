// Loyalty and the Free Cities [R&F] (02-cities.md, Loyalty).
#include <algorithm>

#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
template <typename Edit>
std::unique_ptr<Game> world(Edit edit) {
    GameState s = flatState(24, 14, 2);
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}

void pass(Game& g, int n) {
    for (int i = 0; i < n; ++i) {
        const PlayerId me = g.state().currentPlayer;
        for (UnitId id : g.unitsNeedingOrders(me)) g.submit(Command::setActivity(me, id, Activity::Skip));
        for (CityId id : g.citiesNeedingProduction(me)) g.submit(Command::setProduction(me, id, g.buildableItems(id).front()));
        sovtest::endTurns(g, 1);
    }
}

City& cityRef(GameState& s, CityId id) {
    return *std::find_if(s.cities.begin(), s.cities.end(), [&](const City& c) { return c.id == id; });
}
}  // namespace

TEST(loyalty_tables_from_civ_data) {
    REQUIRE(rules().loyaltyLevels.size() == 4u);
    CHECK_EQ(rules().loyaltyLevels.front().minLoyalty, 0);
    CHECK_EQ(rules().loyaltyLevels.front().yieldPercent, -100);    // Unrest
    CHECK_EQ(rules().loyaltyLevels[1].growthPercent, 25);          // Disloyal
    CHECK_EQ(rules().loyaltyLevels.back().minLoyalty, 76);         // Loyal
    int ecstatic = 0;
    for (const HappinessLevel& h : rules().happiness) if (h.id == "HAPPINESS_ECSTATIC") ecstatic = h.loyaltyPerTurn;
    CHECK_EQ(ecstatic, 6);
}

TEST(citizen_pressure_follows_the_civ_formula) {
    CityId small = kNoCity, home = kNoCity;
    auto g = world([&](GameState& s) {
        home = addCity(s, 0, {2, 7}, true, 3);
        small = addCity(s, 0, {9, 7}, false, 1);
        addCity(s, 1, {12, 7}, true, 8);  // a big foreign capital three plots away
    });
    // Small city: domestic = 1 x 10 + 3 x (10 - 7) x 2 (capital) = 28; foreign = 8 x 7 x 2 = 112.
    // 10 x (28 - 112) / (28 + 0.5) = -29.5, capped at -20.
    CHECK_EQ(g->loyaltyPressure(*g->state().city(small)), Fixed::fromInt(-20));
    CHECK(g->loyaltyPressure(*g->state().city(home)) > Fixed());
}

TEST(monument_and_loyalty_levels) {
    CityId c = kNoCity;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {5, 7}, true, 3);
        cityRef(s, c).buildings.push_back(rules().building("BUILDING_MONUMENT"));
        std::sort(cityRef(s, c).buildings.begin(), cityRef(s, c).buildings.end());
    });
    const Fixed withMonument = g->loyaltyPerTurn(c);
    GameState s = g->state();
    auto& b = cityRef(s, c).buildings;
    b.erase(std::remove(b.begin(), b.end(), rules().building("BUILDING_MONUMENT")), b.end());
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(withMonument, g2->loyaltyPerTurn(c) + Fixed::fromInt(1));

    // A Disloyal city (26-50) loses half its yields.
    const Fixed loyalProd = g->cityReport(c).yields[static_cast<size_t>(YieldType::Production)];
    GameState s3 = g->state();
    cityRef(s3, c).loyalty = 40;
    auto g3 = Game::fromScenario(rules(), std::move(s3));
    CHECK_EQ(g3->loyaltyLevel(*g3->state().city(c))->id, std::string("LOYALTY_DISLOYAL"));
    CHECK_EQ(g3->cityReport(c).yields[static_cast<size_t>(YieldType::Production)], loyalProd / 2);
}

TEST(a_city_at_zero_loyalty_revolts_to_the_free_cities) {
    CityId small = kNoCity;
    UnitId guard = 0;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        small = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, small).loyalty = 10;
        guard = addUnit(s, "UNIT_WARRIOR", 0, {9, 7});
        addCity(s, 1, {12, 7}, true, 8);
    });
    CHECK(g->freeCityPlayer() == kNoPlayer);
    pass(*g, 2);  // back to player 0: its cities' loyalty is processed
    const PlayerId fc = g->freeCityPlayer();
    REQUIRE(fc != kNoPlayer);
    const Player& f = g->state().players[static_cast<size_t>(fc)];
    CHECK(f.freeCity && f.barbarian);
    CHECK_EQ(g->state().city(small)->owner, fc);
    CHECK_EQ(g->state().unit(guard)->owner, fc);  // the garrison goes with it
    CHECK(g->atWar(0, fc) && g->atWar(1, fc));
    CHECK(g->barbarianPlayer() != fc);
}

TEST(free_cities_join_the_strongest_neighbour) {
    CityId fcity = kNoCity;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        fcity = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, fcity).loyalty = 1;
        addCity(s, 1, {12, 7}, true, 8);
    });
    pass(*g, 2);  // it revolts
    const PlayerId fc = g->freeCityPlayer();
    REQUIRE(fc != kNoPlayer && g->state().city(fcity)->owner == fc);
    CHECK(g->loyaltyPerTurn(fcity) < Fixed());  // +10 for itself, -20 from the big neighbour
    for (int i = 0; i < 12 && g->state().city(fcity)->owner == fc; ++i) pass(*g, 2);
    CHECK_EQ(g->state().city(fcity)->owner, 1);
    CHECK_EQ(g->state().city(fcity)->loyalty, rules().globalInt("LOYALTY_AFTER_TRANSFERRED_BY_CULTURAL_IDENTITY"));
}

TEST(captured_cities_start_half_loyal) {
    CityId target = kNoCity;
    UnitId sword = 0;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        target = addCity(s, 0, {9, 7}, false, 1);
        cityRef(s, target).hp = 1;
        addCity(s, 1, {20, 7}, true, 3);
        sword = addUnit(s, "UNIT_SWORDSMAN", 1, {10, 7});
    });
    g->submit(Command::declareWar(0, 1));
    pass(*g, 1);
    REQUIRE(g->submit(Command::attack(1, sword, {9, 7})) == CommandError::Ok);
    CHECK_EQ(g->state().city(target)->owner, 1);
    CHECK_EQ(g->state().city(target)->loyalty, rules().globalInt("LOYALTY_AFTER_TRANSFERRED_BY_COMBAT"));
}

// ---- citizen stances, reputation and rebellion (leader doc §4, §8.1-8.2)

namespace {
UnitId addLeaderAt(GameState& s, PlayerId owner, Hex pos) {
    const UnitId id = addUnit(s, "UNIT_SOVEREIGN", owner, pos);
    s.units.back().gear = {rules().gearType("GEAR_CLUB"), rules().gearType("GEAR_HIDE"), kNone};
    return id;
}
}  // namespace

TEST(benevolence_needs_the_leader_and_gold) {
    CityId c = kNoCity, other = kNoCity;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {5, 7}, true, 4);
        other = addCity(s, 0, {12, 3}, false, 2);
        addLeaderAt(s, 0, {5, 7});
        s.players[0].gold = Fixed::fromInt(200);
    });
    const int cost = g->benevolenceCost(*g->state().city(c));
    CHECK_EQ(cost, rules().globalInt("STANCE_BENEVOLENCE_GOLD_PER_POP") * 4);
    CHECK_EQ(g->submit(Command::cityStance(0, other, Stance::Benevolence)), CommandError::CannotTakeStance);  // the leader is elsewhere
    const int amenities = g->cityReport(c).amenities;
    CHECK_EQ(g->submit(Command::cityStance(0, c, Stance::Benevolence)), CommandError::Ok);
    CHECK_EQ(g->state().players[0].gold, Fixed::fromInt(200 - cost));
    CHECK_EQ(g->cityReport(c).amenities, amenities + rules().globalInt("STANCE_BENEVOLENCE_AMENITIES"));
    CHECK_EQ(g->state().players[0].reputation, rules().globalInt("REPUTATION_PER_STANCE"));
    CHECK_EQ(g->submit(Command::cityStance(0, c, Stance::Benevolence)), CommandError::CannotTakeStance);  // cooldown
    pass(*g, 2 * rules().globalInt("STANCE_EFFECT_TURNS"));
    CHECK_EQ(g->cityReport(c).amenities, amenities);  // it wore off
}

TEST(fear_needs_soldiers_and_leaves_resentment) {
    CityId c = kNoCity;
    UnitId leader = 0;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {5, 7}, true, 4);
        cityRef(s, c).loyalty = 20;  // Unrest
        leader = addLeaderAt(s, 0, {5, 7});
    });
    CHECK_EQ(g->submit(Command::cityStance(0, c, Stance::Fear)), CommandError::CannotTakeStance);  // no garrison
    GameState s = g->state();
    addUnit(s, "UNIT_WARRIOR", 0, {5, 7});
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g2->loyaltyLevel(*g2->state().city(c))->id, std::string("LOYALTY_UNREST"));
    const int amenities = g2->cityReport(c).amenities;
    CHECK_EQ(g2->submit(Command::cityStance(0, c, Stance::Fear)), CommandError::Ok);
    const City& city = *g2->state().city(c);
    CHECK_EQ(city.loyalty, 20 + rules().globalInt("STANCE_FEAR_LOYALTY"));
    CHECK(g2->fearActive(city));
    CHECK_EQ(g2->state().players[0].reputation, -rules().globalInt("REPUTATION_PER_STANCE"));
    // Assassins find resentful locals: the same assassin has better odds against the leader here.
    Agent a;
    a.owner = 1;
    a.level = 1;
    GameState s3 = g2->state();
    cityRef(s3, c).fearAfterUntil = 0;
    auto calm = Game::fromScenario(rules(), std::move(s3));
    CHECK_EQ(g2->assassinSuccessPercent(a, *g2->state().unit(leader)),
             std::min(rules().globalInt("ASSASSIN_MAX_SUCCESS"),
                      calm->assassinSuccessPercent(a, *calm->state().unit(leader)) + rules().globalInt("STANCE_FEAR_ASSASSIN_BONUS")));
    pass(*g2, 2 * rules().globalInt("STANCE_EFFECT_TURNS") + 2);  // order has worn off; resentment remains
    CHECK(!g2->fearActive(*g2->state().city(c)));
    CHECK_EQ(g2->cityReport(c).amenities, amenities - rules().globalInt("STANCE_FEAR_AFTER_AMENITIES"));
}

TEST(fear_keeps_a_city_out_of_unrest) {
    CityId c = kNoCity;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {5, 7}, true, 4);
        cityRef(s, c).loyalty = 5;
        cityRef(s, c).fearUntil = 10;
    });
    CHECK_EQ(g->loyaltyLevel(*g->state().city(c))->id, std::string("LOYALTY_DISLOYAL"));
}

TEST(reputation_beloved_and_feared) {
    CityId c = kNoCity, small = kNoCity;
    auto g = world([&](GameState& s) {
        c = addCity(s, 0, {2, 7}, true, 3);
        small = addCity(s, 0, {9, 7}, false, 1);
        addCity(s, 1, {12, 7}, true, 8);
    });
    const int amenities = g->cityReport(c).amenities;
    const Fixed pressure = g->loyaltyPerTurn(small);
    GameState s = g->state();
    s.players[0].reputation = rules().globalInt("REPUTATION_THRESHOLD");
    auto loved = Game::fromScenario(rules(), std::move(s));
    CHECK(loved->beloved(0));
    CHECK_EQ(loved->cityReport(c).amenities, amenities + rules().globalInt("REPUTATION_BELOVED_AMENITIES"));
    GameState s2 = g->state();
    s2.players[0].reputation = -rules().globalInt("REPUTATION_THRESHOLD");
    auto dread = Game::fromScenario(rules(), std::move(s2));
    CHECK(dread->feared(0));
    CHECK_EQ(dread->cityReport(c).amenities, amenities - rules().globalInt("REPUTATION_FEARED_AMENITIES"));
    CHECK(dread->loyaltyPerTurn(small) > pressure);  // losses from citizen pressure are halved
}

TEST(iron_fist_breeds_rebellion) {
    // A Feared ruler's city in Unrest raises rebels (barbarian units) sooner or later.
    bool rebels = false;
    for (uint64_t seed = 1; seed <= 10 && !rebels; ++seed) {
        CityId c = kNoCity;
        auto g = world([&](GameState& s) {
            s.rng.seed(seed);
            c = addCity(s, 0, {6, 7}, false, 4);
            cityRef(s, c).loyalty = 25;
            addCity(s, 1, {9, 7}, true, 10);  // its pressure keeps the city in Unrest
            s.players[0].reputation = -100;
            Player b;
            b.id = 2;
            b.barbarian = true;
            s.players.push_back(b);
        });
        for (int i = 0; i < 6 && !rebels; ++i) {
            pass(*g, 2);
            for (const GameEvent& e : g->state().events) rebels |= e.kind == EventKind::Rebellion;
        }
        if (rebels) {
            int barbs = 0;
            for (const Unit& u : g->state().units) barbs += u.owner == 2;
            CHECK(barbs > 0);
        }
    }
    CHECK(rebels);
}

TEST(razing_stains_the_reputation) {
    CityId target = kNoCity;
    UnitId sword = 0;
    auto g = world([&](GameState& s) {
        addCity(s, 0, {2, 7}, true, 3);
        target = addCity(s, 0, {9, 7}, false, 2);
        cityRef(s, target).hp = 1;
        addCity(s, 1, {20, 7}, true, 3);
        sword = addUnit(s, "UNIT_SWORDSMAN", 1, {10, 7});
    });
    g->submit(Command::declareWar(0, 1));
    pass(*g, 1);
    REQUIRE(g->submit(Command::attack(1, sword, {9, 7})) == CommandError::Ok);
    REQUIRE(g->submit(Command::razeCity(1, target)) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].reputation, -rules().globalInt("REPUTATION_PER_RAZE"));
}

TEST(a_player_whose_last_city_revolts_hands_the_turn_on) {
    auto g = world([&](GameState& s) {
        const CityId only = addCity(s, 0, {9, 7}, true, 1);
        cityRef(s, only).loyalty = 1;
        addCity(s, 1, {12, 7}, true, 10);
    });
    pass(*g, 2);  // player 0's turn begins, its only city revolts
    CHECK(!g->state().players[0].alive);
    CHECK_EQ(g->state().currentPlayer, 1);
    CHECK_EQ(g->submit(Command::endTurn(1)), CommandError::Ok);
}
