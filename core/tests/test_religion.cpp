// Religion (06-religion.md): pantheons, founding, pressure and followers, religious units,
// theological combat, founder beliefs, worship buildings, the religious victory.
#include "helpers.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex belief(const char* id) { return rules().belief(id); }

// Two players: player 0's capital at (5,6) with a finished Holy Site, a second city at (11,6),
// player 1's city at (17,6).
GameState religionState() {
    GameState s = flatState(24, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId cap = addCity(s, 0, {5, 6}, true, 4);
    addCity(s, 0, {11, 6}, false, 4);
    addCity(s, 1, {17, 6}, true, 4);
    s.cities[0].districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {6, 6}, true});
    s.cities[0].buildings.push_back(rules().building("BUILDING_SHRINE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    for (const Hex& h : s.grid.within({5, 6}, 2)) {
        s.plot(h).owner = 0;
        s.plot(h).city = cap;
    }
    for (Player& p : s.players) p.visibility.assign(static_cast<size_t>(s.grid.size()), static_cast<uint8_t>(Visibility::Revealed));
    return s;
}

UnitId addProphet(GameState& s, Hex at) {
    const UnitId id = addUnit(s, "UNIT_GREAT_PROPHET", 0, at);
    s.units.back().greatPerson = rules().greatPerson("GREAT_PERSON_CONFUCIUS");
    return id;
}

// Player 0 founds Buddhism with Tithe and Feed the World on the capital's Holy Site.
std::unique_ptr<Game> withReligion(GameState s) {
    const UnitId prophet = addProphet(s, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CommandError e = g->submit(Command::foundReligion(0, prophet, rules().religion("RELIGION_BUDDHISM"), belief("BELIEF_TITHE"),
                                                           belief("BELIEF_FEED_THE_WORLD")));
    if (e != CommandError::Ok) std::printf("  foundReligion: %s\n", commandErrorName(e));
    return g;
}
}  // namespace

TEST(religion_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.beliefs.size(), 59u);
    CHECK_EQ(r.religions.size(), 12u);
    const UnitType& m = r.units[at(r.unit("UNIT_MISSIONARY"))];
    CHECK_EQ(m.religiousStrength, 100);
    CHECK_EQ(m.spreadCharges, 3);
    CHECK_EQ(m.evictPercent, 10);
    CHECK(r.units[at(r.unit("UNIT_GREAT_PROPHET"))].foundReligion);
    CHECK(r.buildings[at(r.building("BUILDING_CATHEDRAL"))].faithOnly);
    CHECK_EQ(r.beliefs[at(belief("BELIEF_CATHEDRAL"))].worshipBuilding, r.building("BUILDING_CATHEDRAL"));
    CHECK_EQ(r.beliefs[at(belief("BELIEF_FERTILITY_RITES"))].grantUnit, r.unit("UNIT_BUILDER"));
    CHECK_EQ(r.mapSizes[at(r.mapSize("MAPSIZE_TINY"))].maxReligions, 3);
}

TEST(a_pantheon_needs_faith_and_is_unique) {
    GameState s = religionState();
    s.players[0].faith = Fixed::fromInt(20);
    s.players[1].faith = Fixed::fromInt(40);
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex rites = belief("BELIEF_FERTILITY_RITES");
    CHECK(!g->canFoundPantheon(0, rites));  // 20 < 25
    CHECK(!g->canFoundPantheon(0, belief("BELIEF_TITHE")));  // not a pantheon belief
    const size_t units = g->state().units.size();
    REQUIRE(g->submit(Command::foundPantheon(0, rites)) == CommandError::CannotFoundReligion);
    sovtest::endTurns(*g, 1);  // player 1's turn
    REQUIRE(g->submit(Command::foundPantheon(1, rites)) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].faith, Fixed::fromInt(15));
    CHECK_EQ(g->state().units.size(), units + 1);  // the free Builder
    CHECK(!g->canFoundPantheon(0, rites));         // taken
}

TEST(a_great_prophet_founds_a_religion_on_a_holy_site) {
    GameState s = religionState();
    s.players[0].pantheon = belief("BELIEF_GOD_OF_THE_OPEN_SKY");
    const UnitId elsewhere = addProphet(s, {4, 6});
    auto g = withReligion(std::move(s));
    REQUIRE(g->state().religions.size() == 1u);
    const FoundedReligion& r = g->state().religions[0];
    CHECK_EQ(r.founder, 0);
    CHECK_EQ(r.holyCity, g->state().cities[0].id);
    CHECK_EQ(r.beliefs.size(), 3u);  // the pantheon joins the religion
    CHECK_EQ(g->state().players[0].religion, 0);
    CHECK_EQ(g->cityMajorityReligion(g->state().cities[0]), 0);  // the Holy City converts at once
    CHECK_EQ(g->cityMajorityReligion(g->state().cities[2]), -1);
    // One religion each.
    CHECK(!g->canFoundReligion(elsewhere, rules().religion("RELIGION_ISLAM"), belief("BELIEF_PILGRIMAGE"), belief("BELIEF_CHORAL_MUSIC")));
}

TEST(passive_pressure_spreads_from_the_holy_city) {
    auto g = withReligion(religionState());
    const CityId near = g->state().cities[1].id;
    const int before = g->state().city(near)->pressure.empty() ? 0 : g->state().city(near)->pressure[0];
    sovtest::endTurns(*g, 2);  // one world turn
    // Holy City with a Holy Site: 1 x2 x4 per turn to cities within 10 tiles.
    CHECK_EQ(g->state().city(near)->pressure[0] - before, 8);
}

TEST(missionaries_bought_with_faith_convert_cities) {
    GameState s = religionState();
    s.players[0].faith = Fixed::fromInt(500);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem missionary{ProductionKind::Unit, rules().unit("UNIT_MISSIONARY")};
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), missionary), 75);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, missionary)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].faith, Fixed::fromInt(425));
    const Unit& m = g->state().units.back();
    CHECK_EQ(m.religion, 0);
    CHECK_EQ(m.charges, 3);
    const UnitId id = m.id;
    // Walk next to the second city, then spread: 2 x 100 pressure at full health.
    Game& game = *g;
    REQUIRE(game.submit(Command::move(0, id, {10, 6})) == CommandError::Ok);
    sovtest::endTurns(game, 2);
    if (game.state().unit(id)->pos != Hex{10, 6}) sovtest::endTurns(game, 2);
    REQUIRE(game.state().unit(id)->pos == (Hex{10, 6}));
    const City& target = *game.state().city(game.state().cities[1].id);
    CHECK(game.canSpreadReligion(id));
    REQUIRE(game.submit(Command::spreadReligion(0, id)) == CommandError::Ok);
    CHECK_EQ(game.cityMajorityReligion(*game.state().city(target.id)), 0);
    CHECK_EQ(game.state().unit(id)->charges, 2);
}

TEST(founder_beliefs_pay_per_following_city) {
    auto g = withReligion(religionState());
    // Tithe: +3 Gold for each city following the religion (the Holy City so far).
    CHECK_EQ(g->founderYields(0)[static_cast<size_t>(YieldType::Gold)], Fixed::fromInt(3));
    // Feed the World: +3 Food from the Shrine in a following city.
    const Fixed food = g->cityReport(g->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)];
    GameState plain = religionState();
    auto g2 = Game::fromScenario(rules(), std::move(plain));
    CHECK_EQ(food - g2->cityReport(g2->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)], Fixed::fromInt(3));
}

TEST(worship_buildings_need_their_belief_and_faith) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(1000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem cathedral{ProductionKind::Building, rules().building("BUILDING_CATHEDRAL")};
    CHECK(!g->canProduce(*g->state().city(holy), cathedral));         // never built with production
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), cathedral), -1);  // no Cathedral belief yet
    // An Apostle adds the Cathedral belief.
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const UnitId a = g->state().units.back().id;
    REQUIRE(g->submit(Command::evangelizeBelief(0, a, belief("BELIEF_CATHEDRAL"))) == CommandError::Ok);
    CHECK(!g->state().unit(a));
    CHECK_EQ(g->faithPurchaseCost(0, *g->state().city(holy), cathedral), 190);
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, cathedral)) == CommandError::Ok);
    CHECK(g->state().city(holy)->has(cathedral.type));
}

TEST(theological_combat_needs_no_war) {
    GameState s = religionState();
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
    s.religions.push_back({rules().religion("RELIGION_ISLAM"), 1, s.cities[2].id, {belief("BELIEF_PILGRIMAGE")}});
    s.players[0].religion = 0;
    s.players[1].religion = 1;
    const UnitId apostle = addUnit(s, "UNIT_APOSTLE", 0, {13, 6});
    s.units.back().religion = 0;
    s.units.back().charges = 3;
    const UnitId missionary = addUnit(s, "UNIT_MISSIONARY", 1, {14, 6});
    s.units.back().religion = 1;
    s.units.back().charges = 3;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(!g->atWar(0, 1));
    REQUIRE(g->submit(Command::attack(0, apostle, {14, 6})) == CommandError::Ok);
    const Unit* m = g->state().unit(missionary);
    CHECK(!m || m->hp < 100);
    // A unit of the same religion is no target.
    CHECK_EQ(g->submit(Command::attack(0, apostle, {12, 6})), CommandError::CannotAttack);
}

TEST(a_religion_followed_across_every_civ_wins) {
    GameState s = religionState();
    s.religions.push_back({rules().religion("RELIGION_BUDDHISM"), 0, s.cities[0].id, {belief("BELIEF_TITHE")}});
    s.players[0].religion = 0;
    for (City& c : s.cities) c.pressure = {10000};
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->religiousVictor(), 0);
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().winner, 0);
    CHECK(g->state().victory == Victory::Religious);
}

TEST(religion_survives_a_save) {
    GameState s = religionState();
    s.players[0].pantheon = belief("BELIEF_STONE_CIRCLES");
    auto g = withReligion(std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().religions.size(), 1u);
    CHECK_EQ(loaded->state().players[0].pantheon, belief("BELIEF_STONE_CIRCLES"));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(each_new_apostle_gets_a_promotion) {
    GameState s = religionState();
    s.cities[0].buildings.push_back(rules().building("BUILDING_TEMPLE"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].faith = Fixed::fromInt(5000);
    auto g = withReligion(std::move(s));
    const CityId holy = g->state().cities[0].id;
    const ProductionItem apostle{ProductionKind::Unit, rules().unit("UNIT_APOSTLE")};
    REQUIRE(g->submit(Command::purchaseWithFaith(0, holy, apostle)) == CommandError::Ok);
    const Unit& a = g->state().units.back();
    REQUIRE(a.promotions.size() == 1u);
    CHECK_EQ(rules().promotions[static_cast<size_t>(a.promotions[0])].promotionClass, std::string("PROMOTION_CLASS_RELIGIOUS_APOSTLE"));
    // Orator: two more charges; Debater: +20 theological strength.
    GameState t = g->state();
    t.units.back().promotions = {rules().promotion("PROMOTION_DEBATER")};
    auto h = Game::fromScenario(rules(), t);
    const int base = rules().units[static_cast<size_t>(rules().unit("UNIT_APOSTLE"))].religiousStrength;
    CHECK(h->religiousStrength(h->state().units.back(), false) >= base + 20);
    t.units.back().promotions = {rules().promotion("PROMOTION_ORATOR")};
    auto o = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(o->unitEffectTotal(o->state().units.back(), UnitEffectKind::SpreadCharges), 2);
}
