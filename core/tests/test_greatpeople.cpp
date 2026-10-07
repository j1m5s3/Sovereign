// Great people and Great Works (07-economy-trade-great-people.md).
#include <algorithm>

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
TypeIndex cls(const char* id) { return rules().greatPersonClass(id); }
TypeIndex person(const char* id) { return rules().greatPerson(id); }

// Player 0 with a capital at (6,6) holding a finished district and some buildings.
GameState cityState(const char* district = nullptr, std::vector<const char*> buildings = {}) {
    GameState s = flatState(20, 14, 2);
    for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
    const CityId c = addCity(s, 0, {6, 6}, true, 4);
    addCity(s, 1, {15, 6}, true, 1);
    City& city = s.cities[0];
    if (district) city.districts.push_back({rules().district(district), {7, 6}, true});
    for (const char* b : buildings) city.buildings.push_back(rules().building(b));
    std::sort(city.buildings.begin(), city.buildings.end());
    for (const Hex& h : s.grid.within({6, 6}, 2)) {
        s.plot(h).owner = 0;
        s.plot(h).city = c;
    }
    return s;
}

UnitId addGreatPerson(GameState& s, const char* who, Hex at) {
    const GreatPersonType& g = rules().greatPeople[::at(person(who))];
    const UnitId id = addUnit(s, rules().units[::at(rules().greatPersonClasses[::at(g.cls)].unit)].id.c_str(), 0, at);
    Unit& u = s.units.back();
    u.greatPerson = person(who);
    u.charges = g.greatWorkCount > 0 ? g.greatWorkCount : g.charges;
    return id;
}
}  // namespace

TEST(great_people_rules_data) {
    const Rules& r = rules();
    CHECK_EQ(r.greatPersonClasses.size(), 9u);
    CHECK_EQ(r.greatPersonClasses[at(cls("GREAT_PERSON_CLASS_PROPHET"))].maxPerPlayer, 1);
    const DistrictType& campus = r.districts[at(r.district("DISTRICT_CAMPUS"))];
    REQUIRE(campus.greatPersonPoints.size() == 1u);
    CHECK_EQ(campus.greatPersonPoints[0].first, cls("GREAT_PERSON_CLASS_SCIENTIST"));
    CHECK_EQ(r.districts[at(r.district("DISTRICT_THEATER_SQUARE"))].greatPersonPoints.size(), 3u);
    const BuildingType& amph = r.buildings[at(r.building("BUILDING_AMPHITHEATER"))];
    REQUIRE(amph.greatWorkSlots.size() == 1u);
    CHECK_EQ(amph.greatWorkSlots[0].first, std::string("WRITING"));
    CHECK_EQ(amph.greatWorkSlots[0].second, 2);
    const GreatPersonType& sunTzu = r.greatPeople[at(person("GREAT_PERSON_SUN_TZU"))];
    CHECK(sunTzu.hasAura);
    CHECK_EQ(sunTzu.greatWorkCount, 1);
    CHECK_EQ(r.eras[1].greatPersonBaseCost, 60);
}

TEST(each_class_offers_one_person_at_a_rising_cost) {
    auto g = Game::fromScenario(rules(), cityState());
    CHECK_EQ(g->worldEra(), 0);
    const TypeIndex sci = g->currentGreatPerson(cls("GREAT_PERSON_CLASS_SCIENTIST"));
    REQUIRE(sci != kNone);
    CHECK_EQ(rules().greatPeople[at(sci)].era, 1);  // Classical, the earliest scientists
    CHECK_EQ(g->greatPersonCost(sci), 78);          // 60, +30% for one era ahead of the world
    // No Engineers before the Medieval era's people.
    CHECK_EQ(rules().greatPeople[at(g->currentGreatPerson(cls("GREAT_PERSON_CLASS_ENGINEER")))].era, 2);
}

TEST(districts_and_buildings_earn_points_and_recruit) {
    GameState s = cityState("DISTRICT_CAMPUS", {"BUILDING_LIBRARY"});
    s.players[0].greatPersonPoints[at(cls("GREAT_PERSON_CLASS_SCIENTIST"))] = 76;
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex sci = cls("GREAT_PERSON_CLASS_SCIENTIST");
    CHECK_EQ(g->greatPersonPointsPerTurn(0, sci), 2);
    const TypeIndex first = g->currentGreatPerson(sci);
    sovtest::endTurns(*g, 2);  // round to player 0 again
    bool found = false;
    for (const Unit& u : g->state().units) found |= u.owner == 0 && u.greatPerson == first;
    CHECK(found);
    CHECK_EQ(g->state().players[0].greatPersonPoints[at(sci)], 0);  // 76 + 2 - 78
    CHECK(g->currentGreatPerson(sci) != first);                     // the next one is offered
}

TEST(patronage_buys_the_current_person) {
    GameState s = cityState();
    s.players[0].gold = Fixed::fromInt(2000);
    s.players[0].greatPersonPoints[at(cls("GREAT_PERSON_CLASS_MERCHANT"))] = 28;
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex m = cls("GREAT_PERSON_CLASS_MERCHANT");
    CHECK_EQ(g->patronageCost(0, m, false), 200 + 15 * (78 - 28));
    CHECK_EQ(g->patronageCost(0, m, true), 150 + 10 * (78 - 28));
    CHECK_EQ(g->submit(Command::patronizeGreatPerson(0, m, true)), CommandError::NotEnoughFaith);
    REQUIRE(g->submit(Command::patronizeGreatPerson(0, m, false)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].gold, Fixed::fromInt(2000 - 950));
    CHECK_EQ(g->state().players[0].greatPeopleRecruited[at(m)], 1);
}

TEST(a_passed_person_is_not_recruited_and_prophets_are_one_each) {
    GameState s = cityState();
    const TypeIndex sci = cls("GREAT_PERSON_CLASS_SCIENTIST"), prophet = cls("GREAT_PERSON_CLASS_PROPHET");
    s.players[0].greatPersonPoints[at(sci)] = 500;
    s.players[0].greatPersonPoints[at(prophet)] = 500;
    s.players[0].greatPeopleRecruited[at(prophet)] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::passGreatPerson(0, sci)) == CommandError::Ok);
    sovtest::endTurns(*g, 2);  // round to player 0 again
    CHECK_EQ(g->state().players[0].greatPeopleRecruited[at(sci)], 0);
    CHECK_EQ(g->state().players[0].greatPeopleRecruited[at(prophet)], 1);  // already has one
    CHECK_EQ(g->patronageCost(0, prophet, true), -1);
}

TEST(a_great_scientist_is_used_on_a_campus) {
    GameState s = cityState("DISTRICT_CAMPUS");
    const UnitId hypatia = addGreatPerson(s, "GREAT_PERSON_HYPATIA", {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::activateGreatPerson(0, hypatia)), CommandError::CannotActivate);  // not on the Campus
    const Fixed before = g->cityReport(g->state().cities[0].id).yields[static_cast<size_t>(YieldType::Science)];
    REQUIRE(g->submit(Command::move(0, hypatia, {7, 6})) == CommandError::Ok);
    REQUIRE(g->submit(Command::activateGreatPerson(0, hypatia)) == CommandError::Ok);
    CHECK(!g->state().unit(hypatia));  // one charge, spent
    const City& c = g->state().cities[0];
    CHECK(c.has(rules().building("BUILDING_LIBRARY")));  // granted
    // The Library's +2 and Hypatia's +1 on every Library.
    CHECK_EQ(g->cityReport(c.id).yields[static_cast<size_t>(YieldType::Science)], before + Fixed::fromInt(3));
}

TEST(a_great_writer_fills_writing_slots) {
    GameState s = cityState("DISTRICT_THEATER_SQUARE", {"BUILDING_AMPHITHEATER"});
    const UnitId homer = addGreatPerson(s, "GREAT_PERSON_HOMER", {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId cid = g->state().cities[0].id;
    const Fixed before = g->cityReport(cid).yields[static_cast<size_t>(YieldType::Culture)];
    REQUIRE(g->submit(Command::activateGreatPerson(0, homer)) == CommandError::Ok);
    REQUIRE(g->state().unit(homer));  // a second work to write
    REQUIRE(g->submit(Command::activateGreatPerson(0, homer)) == CommandError::Ok);
    CHECK(!g->state().unit(homer));
    CHECK_EQ(g->state().city(cid)->greatWorks.size(), 2u);
    CHECK_EQ(g->cityReport(cid).yields[static_cast<size_t>(YieldType::Culture)], before + Fixed::fromInt(4));
    CHECK_EQ(g->greatWorkSlots(*g->state().city(cid), "WRITING"), 2);
}

TEST(a_great_general_strengthens_nearby_units_of_its_era) {
    GameState s = cityState();
    addGreatPerson(s, "GREAT_PERSON_HANNIBAL_BARCA", {9, 6});
    const UnitId near = addUnit(s, "UNIT_SWORDSMAN", 0, {10, 6});  // Classical
    const UnitId old = addUnit(s, "UNIT_WARRIOR", 0, {9, 7});      // Ancient: too old
    const UnitId far = addUnit(s, "UNIT_SWORDSMAN", 0, {13, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->greatPersonAuraStrength(*g->state().unit(near)), 5);
    CHECK_EQ(g->greatPersonAuraStrength(*g->state().unit(old)), 0);
    CHECK_EQ(g->greatPersonAuraStrength(*g->state().unit(far)), 0);
    CHECK_EQ(g->maxMoves(*g->state().unit(near)), rules().units[at(rules().unit("UNIT_SWORDSMAN"))].moves + 1);
}

TEST(great_people_survive_a_save) {
    GameState s = cityState("DISTRICT_THEATER_SQUARE", {"BUILDING_AMPHITHEATER"});
    addGreatPerson(s, "GREAT_PERSON_HOMER", {6, 6});
    s.players[0].greatPersonPoints[at(cls("GREAT_PERSON_CLASS_WRITER"))] = 12;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::activateGreatPerson(0, g->state().units.back().id)) == CommandError::Ok);
    const std::vector<uint8_t> bytes = saveGame(*g);
    std::string err;
    auto loaded = loadGame(rules(), bytes, &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().cities[0].greatWorks.size(), 1u);
    CHECK_EQ(loaded->state().units.back().greatPerson, person("GREAT_PERSON_HOMER"));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(a_retired_great_person_gives_the_civs_units_an_ability) {
    GameState s = cityState();
    const UnitId zhukov = addGreatPerson(s, "GREAT_PERSON_GEORGY_ZHUKOV", {9, 6});
    const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {10, 6});
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {10, 7});
    const UnitId theirs = addUnit(s, "UNIT_WARRIOR", 1, {14, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex flanking = rules().ability("ABILITY_GEORGY_ZHUKOV_FLANKING_BONUS");
    const auto has = [&](UnitId id) {
        const std::vector<TypeIndex> a = g->unitAbilities(*g->state().unit(id));
        return std::find(a.begin(), a.end(), flanking) != a.end();
    };
    CHECK(!has(warrior));
    REQUIRE(g->submit(Command::activateGreatPerson(0, zhukov)) == CommandError::Ok);
    CHECK(has(warrior));
    CHECK(!has(builder));  // not a class it applies to
    CHECK(!has(theirs));
    CHECK_EQ(g->unitEffectTotal(*g->state().unit(warrior), UnitEffectKind::FlankingPercent), 50);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->unitEffectTotal(*loaded->state().unit(warrior), UnitEffectKind::FlankingPercent), 50);
}

TEST(a_great_person_leaves_lasting_effects_in_its_city_and_realm) {
    // Sudirman in the capital: +6 loyalty a turn there. Ibn Khaldun on a Campus: +1 Amenity, +2 Housing there.
    GameState s = cityState("DISTRICT_CAMPUS");
    const UnitId sudirman = addGreatPerson(s, "GREAT_PERSON_SUDIRMAN", {6, 6});
    const UnitId khaldun = addGreatPerson(s, "GREAT_PERSON_IBN_KHALDUN", {7, 6});
    const UnitId ike = addGreatPerson(s, "GREAT_PERSON_DWIGHT_EISENHOWER", {5, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId cid = g->state().cities[0].id;
    const Fixed loyalty = g->loyaltyPerTurn(cid);
    const CityReport before = g->cityReport(cid);
    REQUIRE(g->submit(Command::activateGreatPerson(0, sudirman)) == CommandError::Ok);
    REQUIRE(g->submit(Command::activateGreatPerson(0, khaldun)) == CommandError::Ok);
    CHECK_EQ(g->loyaltyPerTurn(cid), loyalty + Fixed::fromInt(6));
    const CityReport after = g->cityReport(cid);
    CHECK_EQ(after.amenities, before.amenities + 1);
    CHECK_EQ(after.housing, before.housing + Fixed::fromInt(2));
    // Eisenhower: +5% toward military units everywhere, not civilians.
    const City& c = g->state().cities[0];
    const Fixed military = sumUnitProductionPercent(g->state(), rules(), c, rules().unit("UNIT_WARRIOR"));
    REQUIRE(g->submit(Command::activateGreatPerson(0, ike)) == CommandError::Ok);
    CHECK_EQ(sumUnitProductionPercent(g->state(), rules(), g->state().cities[0], rules().unit("UNIT_WARRIOR")), military + Fixed::fromInt(5));
    CHECK_EQ(sumUnitProductionPercent(g->state(), rules(), g->state().cities[0], rules().unit("UNIT_BUILDER")), Fixed());
    // The city remembers who was used there, through a save.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().cities[0].greatPeopleHere.size(), 3u);  // all three stood on its land
    CHECK_EQ(loaded->loyaltyPerTurn(cid), g->loyaltyPerTurn(cid));
}

TEST(great_people_effects_in_code) {
    // John Roebling in the City Center: +1 Amenity, +2 Housing there. Marco Polo on the Commercial Hub: other civs' routes
    // to the city carry +2 Gold. Crassus on an unowned plot beside the player's land claims it.
    GameState s = cityState("DISTRICT_COMMERCIAL_HUB");
    const UnitId roebling = addGreatPerson(s, "GREAT_PERSON_JOHN_ROEBLING", {6, 6});
    const UnitId polo = addGreatPerson(s, "GREAT_PERSON_MARCO_POLO", {7, 6});
    const UnitId crassus = addGreatPerson(s, "GREAT_PERSON_MARCUS_LICINIUS_CRASSUS", {9, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const CityId cid = g->state().cities[0].id;
    const CityReport before = g->cityReport(cid);
    const size_t gold = static_cast<size_t>(YieldType::Gold);
    const Fixed route = g->tradeRouteYields(g->state().cities[1], g->state().cities[0])[gold];
    REQUIRE(g->submit(Command::activateGreatPerson(0, roebling)) == CommandError::Ok);
    REQUIRE(g->submit(Command::activateGreatPerson(0, polo)) == CommandError::Ok);
    const CityReport after = g->cityReport(cid);
    CHECK_EQ(after.amenities, before.amenities + 1);
    CHECK_EQ(after.housing, before.housing + Fixed::fromInt(2));
    CHECK_EQ(g->tradeRouteYields(g->state().cities[1], g->state().cities[0])[gold], route + Fixed::fromInt(2));
    REQUIRE(g->state().plot({9, 6}).owner == kNoPlayer);
    REQUIRE(g->submit(Command::activateGreatPerson(0, crassus)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({9, 6}).owner, 0);
    CHECK_EQ(g->state().plot({9, 6}).city, cid);
}

TEST(luxury_corporations_supply_their_products) {
    GameState s = cityState("DISTRICT_COMMERCIAL_HUB");
    const UnitId lauder = addGreatPerson(s, "GREAT_PERSON_EST_E_LAUDER", {7, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex perfume = rules().resource("RESOURCE_PERFUME");
    CHECK(!g->hasLuxury(0, perfume));
    const int amenities = g->cityReport(g->state().cities[0].id).amenities;
    REQUIRE(g->submit(Command::activateGreatPerson(0, lauder)) == CommandError::Ok);
    CHECK_EQ(g->luxuryCopies(0, perfume), 2);
    CHECK(g->hasLuxury(0, perfume));
    CHECK_EQ(g->cityReport(g->state().cities[0].id).amenities, amenities + 1);
}

TEST(colaeus_takes_a_lasting_copy_of_the_luxury_he_stands_on) {
    // Wine on (7,7), unimproved: the city has no copy of it yet.
    const TypeIndex wine = rules().resource("RESOURCE_WINE");
    GameState s = cityState();
    s.plot({7, 7}).resource = wine;
    const UnitId away = addGreatPerson(s, "GREAT_PERSON_COLAEUS", {5, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->submit(Command::activateGreatPerson(0, away)), CommandError::CannotActivate);  // no luxury on his plot
    GameState t = cityState();
    t.plot({7, 7}).resource = wine;
    const UnitId colaeus = addGreatPerson(t, "GREAT_PERSON_COLAEUS", {7, 7});
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->luxuryCopies(0, wine), 0);
    const Fixed faith = h->state().players[0].faith;
    const int amenities = h->cityReport(h->state().cities[0].id).amenities;
    REQUIRE(h->submit(Command::activateGreatPerson(0, colaeus)) == CommandError::Ok);
    CHECK_EQ(h->luxuryCopies(0, wine), 1);
    CHECK(h->hasLuxury(0, wine));
    CHECK_EQ(h->cityReport(h->state().cities[0].id).amenities, amenities + 1);
    CHECK(h->state().players[0].faith > faith);  // and his Faith
    // The copy stays with the civ, through a save.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*h), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->luxuryCopies(0, wine), 1);
    CHECK_EQ(loaded->stateHash(), h->stateHash());
}

TEST(james_young_reveals_oil) {
    GameState s = cityState("DISTRICT_CAMPUS");
    s.plot({8, 6}).resource = rules().resource("RESOURCE_OIL");
    const UnitId young = addGreatPerson(s, "GREAT_PERSON_JAMES_YOUNG", {7, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(!g->resourceVisible(0, {8, 6}));
    REQUIRE(g->submit(Command::activateGreatPerson(0, young)) == CommandError::Ok);
    CHECK(g->resourceVisible(0, {8, 6}));
}

TEST(medici_opens_slots_in_banks) {
    GameState s = cityState("DISTRICT_COMMERCIAL_HUB", {"BUILDING_MARKET", "BUILDING_BANK"});
    const UnitId medici = addGreatPerson(s, "GREAT_PERSON_GIOVANNI_DE_MEDICI", {7, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    const int before = g->greatWorkSlots(g->state().cities[0], "PALACE");
    REQUIRE(g->submit(Command::activateGreatPerson(0, medici)) == CommandError::Ok);
    CHECK_EQ(g->greatWorkSlots(g->state().cities[0], "PALACE"), before + 2);
    CHECK(g->freeGreatWorkSlot(g->state().cities[0], rules().greatWorkType("WRITING")) != kNone);
}

TEST(great_people_one_time_gifts) {
    // On the Commercial Hub: Irene of Athens (+1 governor title), Jakob Fugger (+2 envoys), Marco Polo (+1 trade route).
    GameState s = cityState("DISTRICT_COMMERCIAL_HUB");
    const UnitId irene = addGreatPerson(s, "GREAT_PERSON_IRENE_OF_ATHENS", {7, 6});
    const UnitId fugger = addGreatPerson(s, "GREAT_PERSON_JAKOB_FUGGER", {7, 6});
    const UnitId polo = addGreatPerson(s, "GREAT_PERSON_MARCO_POLO", {7, 6});
    // Bi Sheng in the capital: one more district there. Grace Hopper anywhere: two techs.
    const UnitId bisheng = addGreatPerson(s, "GREAT_PERSON_BI_SHENG", {6, 6});
    const UnitId hopper = addGreatPerson(s, "GREAT_PERSON_GRACE_HOPPER", {5, 5});
    // Timur with a Swordsman: the unit learns faster for good. El Cid makes it a Corps.
    const UnitId sword = addUnit(s, "UNIT_SWORDSMAN", 0, {6, 7});
    const UnitId timur = addGreatPerson(s, "GREAT_PERSON_TIMUR", {6, 7});
    const UnitId cid = addGreatPerson(s, "GREAT_PERSON_EL_CID", {6, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    const int titles = g->governorTitlesLeft(0);
    const int tokens = g->state().players[0].envoyTokens;
    const int routes = g->tradeRouteCapacity(0);
    const int limit = g->districtLimit(g->state().cities[0]);
    auto doneTechs = [&]() { return std::count(g->state().players[0].techs.done.begin(), g->state().players[0].techs.done.end(), uint8_t{1}); };
    const auto techs = doneTechs();
    for (UnitId id : {irene, fugger, polo, bisheng, hopper, timur, cid}) REQUIRE(g->submit(Command::activateGreatPerson(0, id)) == CommandError::Ok);
    CHECK_EQ(g->governorTitlesLeft(0), titles + 1);
    CHECK_EQ(g->state().players[0].envoyTokens, tokens + 2);
    CHECK_EQ(g->tradeRouteCapacity(0), routes + 1);
    CHECK_EQ(g->districtLimit(g->state().cities[0]), limit + 1);
    CHECK_EQ(doneTechs(), techs + 2);
    CHECK_EQ(g->state().unit(sword)->xpBonus, 25);
    CHECK_EQ(static_cast<int>(g->state().unit(sword)->formation), 1);
}

TEST(great_people_are_used_only_where_their_effect_applies) {
    // The plots beside (7,6) other than the capital's.
    const auto besideOf = [](const GameState& s, Hex h) {
        std::vector<Hex> out;
        for (const Hex& n : s.grid.within(h, 1)) {
            if (n != h && n != Hex{6, 6}) out.push_back(n);
        }
        return out;
    };
    const auto usable = [](const GameState& s, UnitId id) { return Game::fromScenario(rules(), s)->canActivateGreatPerson(id); };
    const auto built = [](GameState& s, const char* building) {
        s.cities[0].buildings.push_back(rules().building(building));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    };
    // Galileo beside Mountains, Janaki Ammal beside Rainforest, Darwin by a natural wonder: Science.
    {
        GameState s = cityState();
        const std::vector<Hex> around = besideOf(s, {7, 6});
        const UnitId galileo = addGreatPerson(s, "GREAT_PERSON_GALILEO_GALILEI", {7, 6});
        const UnitId janaki = addGreatPerson(s, "GREAT_PERSON_JANAKI_AMMAL", {7, 6});
        const UnitId darwin = addGreatPerson(s, "GREAT_PERSON_CHARLES_DARWIN", {7, 6});
        CHECK(!usable(s, galileo));
        CHECK(!usable(s, janaki));
        CHECK(!usable(s, darwin));
        GameState peak = s;
        peak.plot({7, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        CHECK(!usable(peak, galileo));  // on a Mountain is not beside one
        s.plot(around[0]).terrain = s.plot(around[1]).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        s.plot(around[2]).feature = rules().feature("FEATURE_JUNGLE");
        s.plot(around[3]).feature = rules().feature("FEATURE_CRATER_LAKE");
        auto g = Game::fromScenario(rules(), std::move(s));
        const auto science = [&] { return g->state().players[0].techs.overflow; };
        REQUIRE(g->submit(Command::activateGreatPerson(0, galileo)) == CommandError::Ok);
        CHECK_EQ(science(), Fixed::fromInt(500));  // 250 for each Mountain
        REQUIRE(g->submit(Command::activateGreatPerson(0, janaki)) == CommandError::Ok);
        CHECK_EQ(science(), Fixed::fromInt(900));
        REQUIRE(g->submit(Command::activateGreatPerson(0, darwin)) == CommandError::Ok);
        CHECK_EQ(science(), Fixed::fromInt(1400));
    }
    // Isidore of Miletus and Imhotep on the plot of a wonder their city is building: the Production goes into the
    // wonder, whatever the city works on now; not once it is built.
    {
        GameState s = cityState();
        const Hex site = besideOf(s, {7, 6})[0];
        const TypeIndex pyramids = rules().building("BUILDING_PYRAMIDS");
        s.cities[0].wonders.push_back({pyramids, site});  // being built; the city works on a Monument now
        GameState t = s;
        const auto progressOf = [](const Game& g, const char* building) {
            for (const ProductionProgress& p : g.state().cities[0].progress) {
                if (p.item.kind == ProductionKind::Building && p.item.type == rules().building(building)) return p.amount;
            }
            return Fixed();
        };
        const UnitId isidore = addGreatPerson(s, "GREAT_PERSON_ISIDORE_OF_MILETUS", {7, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK(!g->canActivateGreatPerson(isidore));
        REQUIRE(g->submit(Command::move(0, isidore, site)) == CommandError::Ok);
        REQUIRE(g->submit(Command::activateGreatPerson(0, isidore)) == CommandError::Ok);
        CHECK_EQ(progressOf(*g, "BUILDING_PYRAMIDS"), Fixed::fromInt(215));
        CHECK_EQ(progressOf(*g, "BUILDING_MONUMENT"), Fixed());
        const UnitId imhotep = addGreatPerson(t, "GREAT_PERSON_IMHOTEP", site);
        auto h = Game::fromScenario(rules(), t);
        REQUIRE(h->submit(Command::activateGreatPerson(0, imhotep)) == CommandError::Ok);
        CHECK_EQ(progressOf(*h, "BUILDING_PYRAMIDS"), Fixed::fromInt(350));  // an Ancient wonder
        built(t, "BUILDING_PYRAMIDS");
        CHECK(!usable(t, imhotep));
    }
    // Sergei Korolev on a Spaceport whose city builds a space race project.
    {
        GameState s = cityState("DISTRICT_SPACEPORT");
        const UnitId korolev = addGreatPerson(s, "GREAT_PERSON_SERGEI_KOROLEV", {7, 6});
        CHECK(!usable(s, korolev));  // a Monument
        s.cities[0].queue = {{ProductionKind::Project, rules().project("PROJECT_LAUNCH_EARTH_SATELLITE")}};
        CHECK(usable(s, korolev));
    }
    // Mary Leakey on a Theater Square whose city holds an Artifact.
    {
        GameState s = cityState("DISTRICT_THEATER_SQUARE", {"BUILDING_AMPHITHEATER", "BUILDING_ARCHAEOLOGICAL_MUSEUM"});
        const UnitId leakey = addGreatPerson(s, "GREAT_PERSON_MARY_LEAKEY", {7, 6});
        CHECK(!usable(s, leakey));
        GreatWork artifact;
        artifact.type = rules().greatWorkType("ARTIFACT");
        artifact.building = rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM");
        s.cities[0].greatWorks.push_back(artifact);
        CHECK(usable(s, leakey));
    }
    // Jeanne d'Arc while some city has room for her Relic; El Cid on a unit that is not a Corps yet.
    {
        GameState s = cityState();
        s.cities[0].buildings.clear();  // no Palace, whose slot takes any Great Work
        const UnitId jeanne = addGreatPerson(s, "GREAT_PERSON_JEANNE_D_ARC", {7, 6});
        const UnitId sword = addUnit(s, "UNIT_SWORDSMAN", 0, {5, 6});
        s.units.back().formation = 1;
        const UnitId cid = addGreatPerson(s, "GREAT_PERSON_EL_CID", {5, 6});
        CHECK(!usable(s, jeanne));
        CHECK(!usable(s, cid));
        built(s, "BUILDING_TEMPLE");
        for (Unit& u : s.units) u.formation = u.id == sword ? 0 : u.formation;
        CHECK(usable(s, jeanne));
        CHECK(usable(s, cid));
    }
    // Zhou Daguan in the land of a city-state at peace with the player.
    {
        GameState s = cityState();
        s.players[1].civ = kNone;
        s.players[1].cityState = rules().cityState("CITYSTATE_MITLA");
        for (Player& p : s.players) {
            p.envoys.assign(2, 0);
            p.relations.resize(2);
        }
        REQUIRE(s.plot({14, 6}).owner == 1);
        const UnitId zhou = addGreatPerson(s, "GREAT_PERSON_ZHOU_DAGUAN", {7, 6});
        CHECK(!usable(s, zhou));  // at home
        s.units.back().pos = {14, 6};
        CHECK(usable(s, zhou));
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        CHECK(!usable(s, zhou));
    }
    // Tupac Amaru in an enemy's land: a Musketman of his in each finished district of that city.
    {
        GameState s = cityState();
        s.cities[1].districts.push_back({rules().district("DISTRICT_CAMPUS"), {16, 6}, true});
        for (Player& p : s.players) p.relations.resize(2);
        const UnitId tupac = addGreatPerson(s, "GREAT_PERSON_TUPAC_AMARU", {14, 6});
        CHECK(!usable(s, tupac));  // at peace
        s.players[0].relations[1].war = s.players[1].relations[0].war = true;
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::activateGreatPerson(0, tupac)) == CommandError::Ok);
        const Unit* rebel = g->state().unitAt({16, 6}, UnitLayer::Military, rules());
        REQUIRE(rebel);
        CHECK_EQ(rebel->owner, 0);
        CHECK_EQ(rebel->type, rules().unit("UNIT_MUSKETMAN"));
    }
    // Boudica beside barbarians: they join her.
    {
        GameState s = flatState(20, 14, 3);
        for (Player& p : s.players) Game::fitPlayerToRules(p, rules());
        s.players[2].barbarian = true;
        s.players[2].civ = kNone;
        addCity(s, 0, {6, 6}, true, 4);
        const UnitId boudica = addGreatPerson(s, "GREAT_PERSON_BOUDICA", {7, 6});
        CHECK(!usable(s, boudica));
        const UnitId raider = addUnit(s, "UNIT_WARRIOR", 2, besideOf(s, {7, 6})[0]);
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->submit(Command::activateGreatPerson(0, boudica)) == CommandError::Ok);
        CHECK_EQ(g->state().unit(raider)->owner, 0);
    }
}

// Nikola Tesla and Joseph Paxton (07; data: adjust district extra regional yield / entertainment, +3 regional range):
// the Industrial Zone or Entertainment Complex they are used on reaches 3 tiles farther, and each city its regional
// buildings reach, its own included, gets +2 Production (Tesla) or +1 Amenity (Paxton), once.
TEST(tesla_and_paxton_strengthen_regional_buildings) {
    const size_t prod = static_cast<size_t>(YieldType::Production);
    // A capital with an Industrial Zone (Workshop, Factory, Coal Power Plant: two regional buildings) and an
    // Entertainment Complex (Arena, Zoo); towns 4 and 8 tiles away.
    const auto setup = [&](const char* who, bool pillaged = false) {
        GameState s = flatState(30, 12, 1);
        Game::fitPlayerToRules(s.players[0], rules());
        const CityId a = addCity(s, 0, {5, 5}, true, 3);
        addCity(s, 0, {9, 5}, false, 3);
        addCity(s, 0, {13, 5}, false, 3);
        City& host = *s.city(a);
        host.districts.push_back({rules().district("DISTRICT_INDUSTRIAL_ZONE"), {5, 6}, true});
        host.districts.push_back({rules().district("DISTRICT_ENTERTAINMENT_COMPLEX"), {4, 5}, true});
        host.districts[0].pillagedTurns = pillaged ? 5 : 0;
        for (const char* b : {"BUILDING_WORKSHOP", "BUILDING_FACTORY", "BUILDING_COAL_POWER_PLANT", "BUILDING_ARENA", "BUILDING_ZOO"})
            host.buildings.push_back(rules().building(b));
        std::sort(host.buildings.begin(), host.buildings.end());
        for (const Hex& h : s.grid.within({5, 5}, 2)) {
            s.plot(h).owner = 0;
            s.plot(h).city = a;
        }
        if (who) host.greatPeopleHere.push_back(person(who));
        return s;
    };
    auto plain = Game::fromScenario(rules(), setup(nullptr));
    auto tesla = Game::fromScenario(rules(), setup("GREAT_PERSON_NIKOLA_TESLA"));
    auto paxton = Game::fromScenario(rules(), setup("GREAT_PERSON_JOSEPH_PAXTON"));
    const CityId host = plain->state().cities[0].id, near = plain->state().cities[1].id, far = plain->state().cities[2].id;
    REQUIRE(plain->state().grid.distance(plain->state().city(host)->pos, plain->state().city(far)->pos) == 8);
    // The report's Production carries the city's mood percentage, which Tesla leaves alone.
    const auto more = [&](const Game& with, const Game& without, CityId c, int amount) {
        REQUIRE(with.cityReport(c).happiness == without.cityReport(c).happiness);
        const int pct = 100 + rules().happiness[static_cast<size_t>(without.cityReport(c).happiness)].yieldPercent;
        return without.cityReport(c).yields[prod] + Fixed::fromInt(amount) * pct / 100;
    };
    // Tesla: +2 Production wherever the Factory reaches, and the town 8 tiles away comes into reach (+3, and +2).
    CHECK_EQ(tesla->cityReport(host).yields[prod], more(*tesla, *plain, host, 2));
    CHECK_EQ(tesla->cityReport(near).yields[prod], more(*tesla, *plain, near, 2));
    CHECK_EQ(tesla->cityReport(far).yields[prod], more(*tesla, *plain, far, 5));
    // A town with its own Factory still gets Tesla's +2 (the two Factories' +3 does not stack).
    const auto withFactory = [&](const char* who) {
        GameState s = setup(who);
        City& town = s.cities[1];
        town.districts.push_back({rules().district("DISTRICT_INDUSTRIAL_ZONE"), {9, 6}, true});
        for (const char* b : {"BUILDING_WORKSHOP", "BUILDING_FACTORY"}) town.buildings.push_back(rules().building(b));
        std::sort(town.buildings.begin(), town.buildings.end());
        return Game::fromScenario(rules(), std::move(s));
    };
    CHECK_EQ(withFactory("GREAT_PERSON_NIKOLA_TESLA")->cityReport(near).yields[prod], more(*withFactory("GREAT_PERSON_NIKOLA_TESLA"), *withFactory(nullptr), near, 2));
    for (YieldType y : {YieldType::Food, YieldType::Gold, YieldType::Science, YieldType::Culture})
        CHECK_EQ(tesla->cityReport(far).yields[static_cast<size_t>(y)], plain->cityReport(far).yields[static_cast<size_t>(y)]);
    CHECK_EQ(tesla->cityReport(far).amenities, plain->cityReport(far).amenities);  // the Zoo still stops at 6
    // Paxton: +1 Amenity likewise, and the Zoo's Amenity reaches the far town.
    CHECK_EQ(paxton->cityReport(host).amenities, plain->cityReport(host).amenities + 1);
    CHECK_EQ(paxton->cityReport(near).amenities, plain->cityReport(near).amenities + 1);
    CHECK_EQ(paxton->cityReport(far).amenities, plain->cityReport(far).amenities + 2);
    // An Industrial Zone with no regional building gives nothing.
    const auto bare = [&](const char* who) {
        GameState s = setup(who);
        std::vector<TypeIndex>& b = s.cities[0].buildings;
        for (const char* r : {"BUILDING_FACTORY", "BUILDING_COAL_POWER_PLANT"}) b.erase(std::find(b.begin(), b.end(), rules().building(r)));
        return Game::fromScenario(rules(), std::move(s));
    };
    CHECK_EQ(bare("GREAT_PERSON_NIKOLA_TESLA")->cityReport(host).yields[prod], bare(nullptr)->cityReport(host).yields[prod]);
    CHECK_EQ(bare("GREAT_PERSON_NIKOLA_TESLA")->cityReport(near).yields[prod], bare(nullptr)->cityReport(near).yields[prod]);
    // A pillaged Industrial Zone gives nothing, Tesla or not.
    auto pillaged = Game::fromScenario(rules(), setup("GREAT_PERSON_NIKOLA_TESLA", true));
    auto pillagedPlain = Game::fromScenario(rules(), setup(nullptr, true));
    CHECK_EQ(pillaged->cityReport(host).yields[prod], pillagedPlain->cityReport(host).yields[prod]);
    CHECK_EQ(pillaged->cityReport(far).yields[prod], plain->cityReport(far).yields[prod]);
    // Tesla is used on the Industrial Zone and counts from then on.
    GameState s = setup(nullptr);
    const UnitId t = addGreatPerson(s, "GREAT_PERSON_NIKOLA_TESLA", {5, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::activateGreatPerson(0, t)) == CommandError::Ok);
    CHECK_EQ(g->cityReport(far).yields[prod], tesla->cityReport(far).yields[prod]);
}

// Shah Jahan (07; Civilopedia): on the plot of a wonder his city is building, Production toward it, capped at half the
// treasury, for twice as much Gold.
TEST(shah_jahan_buys_a_wonder_with_gold) {
    const TypeIndex pyramids = rules().building("BUILDING_PYRAMIDS");
    const ProductionItem item{ProductionKind::Building, pyramids, 0};
    const auto use = [&](int gold, int done, Hex where = {7, 7}) {
        GameState s = cityState();
        s.cities[0].wonders.push_back({pyramids, {7, 7}});  // being built; the city works on something else now
        if (done > 0) s.cities[0].progress.push_back({item, Fixed::fromInt(done)});
        s.players[0].gold = Fixed::fromInt(gold);
        const UnitId shah = addGreatPerson(s, "GREAT_PERSON_SHAH_JAH_N", where);
        auto g = Game::fromScenario(rules(), std::move(s));
        const CommandError expected = where == Hex{7, 7} ? CommandError::Ok : CommandError::CannotActivate;
        CHECK_EQ(g->submit(Command::activateGreatPerson(0, shah)), expected);
        return g;
    };
    const auto progress = [&](const Game& g) {
        Fixed amount;
        for (const ProductionProgress& p : g.state().cities[0].progress) amount += p.item == item ? p.amount : Fixed();
        return amount;
    };
    const auto rich = use(10000, 100);
    const int cost = rich->productionCost(0, item, &rich->state().cities[0]);
    REQUIRE(cost > 150);
    CHECK_EQ(progress(*rich), Fixed::fromInt(cost));  // the rest, no more
    CHECK_EQ(rich->state().players[0].gold, Fixed::fromInt(10000 - 2 * (cost - 100)));
    CHECK(rich->state().city(rich->state().cities[0].id)->progress.size() == 1);
    const auto poor = use(101, 0);  // half the treasury: 50
    CHECK_EQ(progress(*poor), Fixed::fromInt(50));
    CHECK_EQ(poor->state().players[0].gold, Fixed::fromInt(1));
    for (int gold : {0, -40}) {  // nothing to spend: nothing bought, and he is used up
        const auto broke = use(gold, 0);
        CHECK(broke->state().cities[0].progress.empty());
        CHECK_EQ(broke->state().players[0].gold, Fixed::fromInt(gold));
        CHECK(std::none_of(broke->state().units.begin(), broke->state().units.end(), [](const Unit& u) { return u.greatPerson != kNone; }));
    }
    use(10000, 0, {5, 6});  // not on the wonder's plot
}
