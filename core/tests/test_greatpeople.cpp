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
