// Civ uniques (leaders-and-art-style.md: Civ abilities, uniques and dynasties).
#include <algorithm>

#include "helpers.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::claimFor;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
ProductionItem unit(const char* id) { return {ProductionKind::Unit, rules().unit(id)}; }
TypeIndex civ(const char* id) { return rules().civ(id); }

// Player 0 plays `civ0`, player 1 `civ1`; each has a city, both know `techs`.
GameState pair(const char* civ0, const char* civ1, std::initializer_list<const char*> techs) {
    GameState s = flatState(24, 14, 2);
    s.players[0].civ = civ(civ0);
    s.players[1].civ = civ(civ1);
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 1, {16, 6}, true, 5);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        for (const char* t : techs) p.techs.done[at(rules().tech(t))] = 1;
        p.relations.resize(2);
    }
    return s;
}
}  // namespace

TEST(unique_units_are_built_on_their_base) {
    const Rules& r = rules();
    const UnitType& longbow = r.units[at(r.unit("UNIT_LONGBOWMAN"))];
    const UnitType& crossbow = r.units[at(r.unit("UNIT_CROSSBOWMAN"))];
    CHECK_EQ(longbow.uniqueTo, civ("CIVILIZATION_ENGLAND"));
    CHECK_EQ(longbow.replaces, r.unit("UNIT_CROSSBOWMAN"));
    CHECK_EQ(longbow.cost, 162);                    // 10% cheaper
    CHECK_EQ(longbow.ranged, crossbow.ranged);      // the rest comes from the Crossbowman
    CHECK(longbow.unlock.civic == crossbow.unlock.civic && longbow.unlock.index == crossbow.unlock.index);
    CHECK_EQ(longbow.upgradesTo, crossbow.upgradesTo);
    CHECK_EQ(r.units[at(r.unit("UNIT_LEGIONARY"))].combat, 39);
    int uniques = 0;
    for (const UnitType& u : r.units) uniques += u.uniqueTo != kNone ? 1 : 0;
    CHECK_EQ(uniques, 12);
}

TEST(a_unique_unit_replaces_its_base_for_its_civ_only) {
    auto g = Game::fromScenario(rules(), pair("CIVILIZATION_ENGLAND", "CIVILIZATION_FRANCE", {"TECH_MACHINERY"}));
    const City& english = g->state().cities[0];
    const City& french = g->state().cities[1];
    CHECK(g->canProduce(english, unit("UNIT_LONGBOWMAN")));
    CHECK(!g->canProduce(english, unit("UNIT_CROSSBOWMAN")));
    CHECK(g->canProduce(french, unit("UNIT_CROSSBOWMAN")));
    CHECK(!g->canProduce(french, unit("UNIT_LONGBOWMAN")));
}

TEST(upgrades_lead_to_the_civs_unique) {
    GameState s = pair("CIVILIZATION_ROME", "CIVILIZATION_FRANCE", {"TECH_IRON_WORKING", "TECH_BRONZE_WORKING"});
    const UnitId w = addUnit(s, "UNIT_WARRIOR", 0, {4, 7});
    for (const Hex& h : s.grid.within({4, 6}, 2)) s.plot(h).owner = 0;
    s.players[0].gold = Fixed::fromInt(500);
    s.players[0].stockpile[at(rules().resource("RESOURCE_IRON"))] = 40;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->upgradeTarget(*g->state().unit(w)), rules().unit("UNIT_LEGIONARY"));
    REQUIRE(g->submit(Command::upgradeUnit(0, w)) == CommandError::Ok);
    CHECK_EQ(g->state().unit(w)->type, rules().unit("UNIT_LEGIONARY"));
}

TEST(hoplites_stand_together_and_immortals_fight_both_ways) {
    GameState s = pair("CIVILIZATION_GREECE", "CIVILIZATION_PERSIA", {"TECH_BRONZE_WORKING", "TECH_IRON_WORKING"});
    const UnitId h1 = addUnit(s, "UNIT_HOPLITE", 0, {8, 6});
    const UnitId foe = addUnit(s, "UNIT_IMMORTAL", 1, {9, 6});
    auto alone = Game::fromScenario(rules(), s);
    const int single = alone->combatStrength(*alone->state().unit(h1), *alone->state().unit(foe), false, false);
    addUnit(s, "UNIT_HOPLITE", 0, {8, 7});
    auto pairUp = Game::fromScenario(rules(), s);
    CHECK_EQ(pairUp->combatStrength(*pairUp->state().unit(h1), *pairUp->state().unit(foe), false, false), single + 10);
    // The Immortal can shoot or charge.
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    s.currentPlayer = 1;
    auto war = Game::fromScenario(rules(), std::move(s));
    CHECK(war->previewAttack(foe, {8, 6}, true).valid);
    CHECK(war->previewAttack(foe, {8, 6}, false).valid);
}

TEST(jaguar_warriors_bring_the_defeated_home_as_builders) {
    GameState s = pair("CIVILIZATION_AZTEC", "CIVILIZATION_INCA", {});
    const UnitId jaguar = addUnit(s, "UNIT_JAGUAR_WARRIOR", 0, {8, 6});
    const UnitId victim = addUnit(s, "UNIT_WARRIOR", 1, {9, 6});
    s.unit(victim)->hp = 5;
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::attack(0, jaguar, {9, 6})) == CommandError::Ok);
    CHECK(g->state().unit(victim) == nullptr);
    int builders = 0;
    for (const Unit& u : g->state().units) builders += u.owner == 0 && u.type == rules().unit("UNIT_BUILDER") ? 1 : 0;
    CHECK_EQ(builders, 1);
}

TEST(unique_buildings_replace_their_base) {
    GameState s = pair("CIVILIZATION_FRANCE", "CIVILIZATION_ENGLAND", {"TECH_ASTROLOGY"});
    for (City& c : s.cities) {
        c.districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {c.pos.x + 1, c.pos.y}, true});
        c.buildings.push_back(rules().building("BUILDING_SHRINE"));  // the Temple needs a Shrine
        std::sort(c.buildings.begin(), c.buildings.end());
    }
    for (const Hex& h : s.grid.within({4, 6}, 2)) s.plot(h).city = s.cities[0].id;
    for (const Hex& h : s.grid.within({16, 6}, 2)) s.plot(h).city = s.cities[1].id;
    for (Player& p : s.players) p.civics.done[at(rules().civic("CIVIC_THEOLOGY"))] = 1;
    auto g = Game::fromScenario(rules(), s);
    const ProductionItem abbey{ProductionKind::Building, rules().building("BUILDING_ROYAL_ABBEY")};
    const ProductionItem temple{ProductionKind::Building, rules().building("BUILDING_TEMPLE")};
    CHECK(g->canProduce(g->state().cities[0], abbey));
    CHECK(!g->canProduce(g->state().cities[0], temple));
    CHECK(g->canProduce(g->state().cities[1], temple));
    CHECK(!g->canProduce(g->state().cities[1], abbey));
    // It counts as a Temple wherever a Temple is asked for.
    s.cities[0].buildings.push_back(rules().building("BUILDING_ROYAL_ABBEY"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    CHECK(cityHasBuilding(s.cities[0], rules(), rules().building("BUILDING_TEMPLE")));
    CHECK_EQ(rules().buildings[at(rules().building("BUILDING_ROYAL_ABBEY"))].yields[static_cast<size_t>(YieldType::Science)], Fixed::fromInt(1));
}

// The uniques replacing each building and unit are listed when the rules load, as a search of them all finds them. A
// unique added after (the lists not made again) is found by that search, and counts as its base.
TEST(uniques_are_listed_under_what_they_replace) {
    const Rules& r = rules();
    const auto unlisted = [](const auto& all, auto replacing) {
        int wrong = 0;
        for (size_t base = 0; base < all.size(); ++base) {
            std::vector<TypeIndex> found;
            for (size_t u = 0; u < all.size(); ++u) {
                if (all[u].replaces == static_cast<TypeIndex>(base)) found.push_back(static_cast<TypeIndex>(u));
            }
            const std::vector<TypeIndex>* listed = replacing(static_cast<TypeIndex>(base));
            wrong += listed && *listed == found ? 0 : 1;
        }
        return wrong;
    };
    CHECK_EQ(unlisted(r.buildings, [&](TypeIndex b) { return r.buildingsReplacing(b); }), 0);
    CHECK_EQ(unlisted(r.units, [&](TypeIndex u) { return r.unitsReplacing(u); }), 0);
    REQUIRE(r.buildingsReplacing(r.building("BUILDING_TEMPLE")) != nullptr);
    CHECK_EQ(r.buildingsReplacing(r.building("BUILDING_TEMPLE"))->size(), 2u);  // the Royal Abbey and the Sahel Mosque
    CHECK(r.buildingsReplacing(kNone) == nullptr);
    CHECK(r.buildingsReplacing(static_cast<TypeIndex>(r.buildings.size())) == nullptr);
    CHECK(r.unitsReplacing(static_cast<TypeIndex>(r.units.size())) == nullptr);
    CHECK_EQ(r.uniqueBuildingFor(civ("CIVILIZATION_MALI"), r.building("BUILDING_TEMPLE")), r.building("BUILDING_SAHEL_MOSQUE"));
    CHECK_EQ(r.uniqueBuildingFor(civ("CIVILIZATION_INCA"), r.building("BUILDING_TEMPLE")), kNone);
    // A Monument for France and a Warrior for England, added to a copy of the rules.
    Rules more = r;
    const TypeIndex monument = r.building("BUILDING_MONUMENT"), warrior = r.unit("UNIT_WARRIOR");
    REQUIRE(r.buildingsReplacing(monument) != nullptr && r.buildingsReplacing(monument)->empty());
    BuildingType building = r.buildings[at(monument)];
    building.id = "BUILDING_TEST_MONUMENT";
    building.uniqueTo = civ("CIVILIZATION_FRANCE");
    building.replaces = monument;
    more.buildings.push_back(building);
    UnitType unitType = r.units[at(warrior)];
    unitType.id = "UNIT_TEST_WARRIOR";
    unitType.uniqueTo = civ("CIVILIZATION_ENGLAND");
    unitType.replaces = warrior;
    more.units.push_back(unitType);
    const TypeIndex newBuilding = static_cast<TypeIndex>(more.buildings.size() - 1), newUnit = static_cast<TypeIndex>(more.units.size() - 1);
    City c;
    c.buildings = {newBuilding};
    for (const bool indexed : {false, true}) {
        if (indexed) more.indexUniques();
        CHECK_EQ(more.buildingsReplacing(monument) != nullptr, indexed);  // until then the lists do not fit the rules
        CHECK_EQ(more.uniqueBuildingFor(civ("CIVILIZATION_FRANCE"), monument), newBuilding);
        CHECK_EQ(more.uniqueBuildingFor(civ("CIVILIZATION_ENGLAND"), monument), kNone);
        CHECK_EQ(more.uniqueUnitFor(civ("CIVILIZATION_ENGLAND"), warrior), newUnit);
        CHECK_EQ(more.uniqueUnitFor(civ("CIVILIZATION_FRANCE"), warrior), kNone);
        CHECK(cityHasBuilding(c, more, monument));
    }
}

// A civ's unique building stands in for the one it replaces in a district with every building: an Aztec Campus with
// a Calmecac (their Library), a University and a Research Lab is a Splendid Campus; another civ's needs the Library.
TEST(a_unique_building_completes_a_splendid_district) {
    for (const bool aztec : {true, false}) {
        GameState s = pair(aztec ? "CIVILIZATION_AZTEC" : "CIVILIZATION_INCA", "CIVILIZATION_GREECE", {});
        City& c = s.cities[0];
        c.districts.push_back({rules().district("DISTRICT_CAMPUS"), {5, 6}, true});
        c.buildings.push_back(rules().building("BUILDING_CALMECAC"));
        c.buildings.push_back(rules().building("BUILDING_UNIVERSITY"));
        std::sort(c.buildings.begin(), c.buildings.end());
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->completeItem(g->stateMutForTests().cities[0], {ProductionKind::Building, rules().building("BUILDING_RESEARCH_LAB")}));
        CHECK_EQ(sovtest::hasMoment(*g, 0, "MOMENT_SPLENDID_CAMPUS_COMPLETED"), aztec);
    }
}

TEST(qullqa_odeon_and_forum_effects) {
    GameState s = pair("CIVILIZATION_INCA", "CIVILIZATION_GREECE", {});
    s.plot({5, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    s.plot({3, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    s.plot({4, 5}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    auto before = Game::fromScenario(rules(), s);
    const Fixed food = before->cityReport(before->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)];
    s.cities[0].buildings.push_back(rules().building("BUILDING_QULLQA"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), s);
    // The Granary's +1 Food, and +1 per adjacent mountain up to 2.
    CHECK(g->cityReport(g->state().cities[0].id).yields[static_cast<size_t>(YieldType::Food)] == food + Fixed::fromInt(3));
    // The Odeon sends an envoy when built.
    const int envoys = g->state().players[1].envoyTokens;
    g->stateMutForTests().cities[1].districts.push_back({rules().district("DISTRICT_THEATER_SQUARE"), {17, 6}, true});
    REQUIRE(g->completeItem(g->stateMutForTests().cities[1], {ProductionKind::Building, rules().building("BUILDING_ODEON")}));
    CHECK_EQ(g->state().players[1].envoyTokens, envoys + 1);
}

TEST(unique_improvements) {
    GameState s = pair("CIVILIZATION_PERSIA", "CIVILIZATION_CHINA", {"TECH_CONSTRUCTION"});
    for (Player& p : s.players) p.civics.done[at(rules().civic("CIVIC_EARLY_EMPIRE"))] = 1;
    for (const Hex& h : s.grid.within({4, 6}, 2)) {
        s.plot(h).owner = 0;
        s.plot(h).city = s.cities[0].id;
    }
    for (const Hex& h : s.grid.within({16, 6}, 2)) {
        s.plot(h).owner = 1;
        s.plot(h).city = s.cities[1].id;
    }
    const TypeIndex garden = rules().improvement("IMPROVEMENT_PARADISE_GARDEN");
    const TypeIndex tower = rules().improvement("IMPROVEMENT_BEACON_TOWER");
    auto g = Game::fromScenario(rules(), s);
    CHECK(g->canImproveAt(0, {5, 7}, garden));
    CHECK(!g->canImproveAt(1, {17, 7}, garden));  // Persia's only
    CHECK(g->canImproveAt(1, {18, 6}, tower));     // at the edge of China's land
    CHECK(!g->canImproveAt(1, {17, 6}, tower));    // inland
    const int amenities = g->cityReport(g->state().cities[0].id).amenities;
    s.plot({5, 7}).improvement = garden;
    s.plot({18, 6}).improvement = tower;
    const UnitId guard = addUnit(s, "UNIT_SPEARMAN", 1, {18, 6});
    const UnitId foe = addUnit(s, "UNIT_SPEARMAN", 0, {19, 6});
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g2->cityReport(g2->state().cities[0].id).amenities, amenities + 1);
    const int defended = g2->combatStrength(*g2->state().unit(guard), *g2->state().unit(foe), false, false);
    const int attacking = g2->combatStrength(*g2->state().unit(guard), *g2->state().unit(foe), true, false);
    CHECK_EQ(defended, attacking + 4);
}

TEST(civ_abilities_colonia_and_craft_guilds) {
    // Rome: new cities start one larger with a Monument.
    GameState s = pair("CIVILIZATION_ROME", "CIVILIZATION_JAPAN", {});
    s.cities.clear();
    const UnitId settler = addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->submit(Command::foundCity(0, settler)) == CommandError::Ok);
    const City& rome = g->state().cities.back();
    CHECK_EQ(rome.population, 2);
    CHECK(rome.has(rules().building("BUILDING_MONUMENT")));
    // Japan: an Industrial Zone next to a Theater Square gains production adjacency.
    GameState j = pair("CIVILIZATION_JAPAN", "CIVILIZATION_ROME", {});
    j.cities[0].districts.push_back({rules().district("DISTRICT_THEATER_SQUARE"), {5, 7}, true});
    claimFor(j, j.cities[0], {5, 7});
    auto gj = Game::fromScenario(rules(), j);
    const TypeIndex iz = rules().district("DISTRICT_INDUSTRIAL_ZONE");
    const Fixed withGuild = gj->districtAdjacency(0, iz, {6, 7})[static_cast<size_t>(YieldType::Production)];
    const Fixed plain = gj->districtAdjacency(1, iz, {6, 7})[static_cast<size_t>(YieldType::Production)];
    CHECK(withGuild == plain + Fixed::fromInt(1));
}

TEST(civ_ability_saharan_riches) {
    // Mali: a Commercial Hub earns +1 Gold for every two Desert plots beside it; its own plot does not count.
    GameState s = pair("CIVILIZATION_MALI", "CIVILIZATION_EGYPT", {});
    const Hex hub{10, 10}, onDesert{18, 10};
    int desert = 0;
    for (const Hex& h : s.grid.within(hub, 1)) {
        if (h == hub || desert == 4) continue;  // four of its six neighbours
        s.plot(h).terrain = rules().terrain("TERRAIN_DESERT");
        ++desert;
    }
    s.plot(onDesert).terrain = rules().terrain("TERRAIN_DESERT");  // with one Desert neighbour
    s.plot({onDesert.x + 1, onDesert.y}).terrain = rules().terrain("TERRAIN_DESERT");
    auto g = Game::fromScenario(rules(), std::move(s));
    const TypeIndex hubType = rules().district("DISTRICT_COMMERCIAL_HUB");
    const auto gold = [&](PlayerId p, Hex at) { return g->districtAdjacency(p, hubType, at)[static_cast<size_t>(YieldType::Gold)]; };
    CHECK(gold(0, hub) == gold(1, hub) + Fixed::fromInt(2));
    CHECK(gold(0, onDesert) == gold(1, onDesert));
}

TEST(civ_abilities_satrapies_and_mita_labor) {
    GameState s = pair("CIVILIZATION_PERSIA", "CIVILIZATION_INCA", {});
    auto g = Game::fromScenario(rules(), s);
    const int titles = g->governorTitles(0);
    s.players[0].civics.done[at(rules().civic("CIVIC_POLITICAL_PHILOSOPHY"))] = 1;
    s.players[1].civics.done[at(rules().civic("CIVIC_POLITICAL_PHILOSOPHY"))] = 1;
    // Inca: the mountain next to their city can be worked for production.
    s.plot({17, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    for (const Hex& h : s.grid.within({16, 6}, 1)) s.plot(h).city = s.cities[1].id;
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK(g2->governorTitles(0) > g2->governorTitles(1));  // Persia's extra title
    CHECK(g2->governorTitles(0) > titles);
    const City& inca = g2->state().cities[1];
    const std::vector<Hex> plots = g2->workablePlots(inca);
    CHECK(std::find(plots.begin(), plots.end(), Hex{17, 6}) != plots.end());
    CHECK(g2->plotYields({17, 6}, inca)[static_cast<size_t>(YieldType::Production)] >= Fixed::fromInt(2));
}

TEST(civ_ability_modifiers_mills_and_mines) {
    GameState s = pair("CIVILIZATION_ENGLAND", "CIVILIZATION_FRANCE", {});
    for (const Hex& h : {Hex{5, 6}, Hex{17, 6}}) {
        s.plot(h).resource = rules().resource("RESOURCE_IRON");
        s.plot(h).resourceAmount = 1;
        s.plot(h).improvement = rules().improvement("IMPROVEMENT_MINE");
    }
    for (Player& p : s.players) p.techs.done[at(rules().tech("TECH_BRONZE_WORKING"))] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    const Fixed english = g->plotYields({5, 6}, g->state().cities[0])[static_cast<size_t>(YieldType::Production)];
    const Fixed french = g->plotYields({17, 6}, g->state().cities[1])[static_cast<size_t>(YieldType::Production)];
    CHECK(english == french + Fixed::fromInt(1));
}

TEST(leader_abilities_edo_peace_and_flower_wars) {
    // Tokugawa: +10% Science while at peace with every major civ.
    GameState s = pair("CIVILIZATION_JAPAN", "CIVILIZATION_AZTEC", {});
    auto peace = Game::fromScenario(rules(), s);
    const Fixed calm = peace->cityReport(peace->state().cities[0].id).yields[static_cast<size_t>(YieldType::Science)];
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    auto war = Game::fromScenario(rules(), s);
    const Fixed tense = war->cityReport(war->state().cities[0].id).yields[static_cast<size_t>(YieldType::Science)];
    CHECK(calm > tense);
    // Moctezuma: a kill pays Faith worth half the victim's strength and counts toward the capital's mood.
    GameState a = pair("CIVILIZATION_AZTEC", "CIVILIZATION_JAPAN", {});
    a.players[0].relations[1].war = a.players[1].relations[0].war = true;
    const UnitId jaguar = addUnit(a, "UNIT_JAGUAR_WARRIOR", 0, {8, 6});
    const UnitId prey = addUnit(a, "UNIT_WARRIOR", 1, {9, 6});
    a.unit(prey)->hp = 5;
    auto g = Game::fromScenario(rules(), std::move(a));
    const Fixed faith = g->state().players[0].faith;
    REQUIRE(g->submit(Command::attack(0, jaguar, {9, 6})) == CommandError::Ok);
    CHECK(g->state().players[0].faith == faith + Fixed::fromInt(rules().units[at(rules().unit("UNIT_WARRIOR"))].combat / 2));
    CHECK_EQ(g->state().players[0].killsThisEra, 1);
}

TEST(leader_abilities_hold_the_pass_and_earthshaker) {
    GameState s = pair("CIVILIZATION_GREECE", "CIVILIZATION_INCA", {"TECH_BRONZE_WORKING"});
    s.plot({8, 6}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    s.plot({9, 6}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    const UnitId greek = addUnit(s, "UNIT_SPEARMAN", 0, {8, 6});
    const UnitId inca = addUnit(s, "UNIT_SPEARMAN", 1, {9, 6});
    auto g = Game::fromScenario(rules(), s);
    // The same unit on the same ground: Leonidas's Spearman defends 5 stronger.
    CHECK_EQ(g->combatStrength(*g->state().unit(greek), *g->state().unit(inca), false, false),
             g->combatStrength(*g->state().unit(inca), *g->state().unit(greek), false, false) + 5);
    // Pachacuti: farms on hills +1 Food; a city next to a mountain +1 Housing.
    for (const Hex& h : {Hex{5, 7}, Hex{17, 7}}) {
        s.plot(h).terrain = rules().terrain("TERRAIN_PLAINS_HILLS");
        s.plot(h).improvement = rules().improvement("IMPROVEMENT_FARM");
    }
    auto g2 = Game::fromScenario(rules(), std::move(s));
    const Fixed greekFood = g2->plotYields({5, 7}, g2->state().cities[0])[static_cast<size_t>(YieldType::Food)];
    const Fixed incaFood = g2->plotYields({17, 7}, g2->state().cities[1])[static_cast<size_t>(YieldType::Food)];
    CHECK(incaFood == greekFood + Fixed::fromInt(1));
}

TEST(leader_abilities_golden_pilgrimage_faith_purchase) {
    GameState s = pair("CIVILIZATION_MALI", "CIVILIZATION_EGYPT", {"TECH_CURRENCY"});
    for (City& c : s.cities) c.districts.push_back({rules().district("DISTRICT_COMMERCIAL_HUB"), {c.pos.x + 1, c.pos.y}, true});
    auto g = Game::fromScenario(rules(), std::move(s));
    const ProductionItem market{ProductionKind::Building, rules().building("BUILDING_MARKET")};
    CHECK(g->faithPurchaseCost(0, g->state().cities[0], market) > 0);
    CHECK(g->faithPurchaseCost(1, g->state().cities[1], market) < 0);
    // At the Market's Gold price, less Theocracy's 15% off Faith purchases; Democracy's off Gold purchases leaves it (04).
    const int gold = g->purchaseCost(0, market);
    CHECK_EQ(g->faithPurchaseCost(0, g->state().cities[0], market), gold);
    const auto under = [&](const char* government) {
        GameState t = g->state();
        t.players[0].government = rules().government(government);
        t.players[0].policies.assign(static_cast<size_t>(rules().governments[static_cast<size_t>(t.players[0].government)].totalSlots()), kNone);
        auto u = Game::fromScenario(rules(), std::move(t));
        return u->faithPurchaseCost(0, u->state().cities[0], market);
    };
    CHECK(under("GOVERNMENT_THEOCRACY") < gold);
    CHECK_EQ(under("GOVERNMENT_DEMOCRACY"), gold);
}

TEST(heirs_bring_their_traits_to_the_throne) {
    const Dynasty* d = rules().dynastyOf(civ("CIVILIZATION_ENGLAND"));
    REQUIRE(d != nullptr);
    REQUIRE(d->traits.size() == 3u);
    CHECK_EQ(d->traits[1].name, std::string("King James Bible"));
    GameState s = pair("CIVILIZATION_ENGLAND", "CIVILIZATION_FRANCE", {});
    auto elizabeth = Game::fromScenario(rules(), s);
    const Fixed faith = elizabeth->cityReport(elizabeth->state().cities[0].id).yields[static_cast<size_t>(YieldType::Faith)];
    s.players[0].rulingHeir = 1;  // James I
    auto james = Game::fromScenario(rules(), s);
    CHECK(james->cityReport(james->state().cities[0].id).yields[static_cast<size_t>(YieldType::Faith)] == faith + Fixed::fromInt(2));
    s.players[0].rulingHeir = -1;  // a regent: no dynasty trait
    auto regent = Game::fromScenario(rules(), std::move(s));
    CHECK(regent->cityReport(regent->state().cities[0].id).yields[static_cast<size_t>(YieldType::Faith)] == faith);
}

TEST(war_chariots_and_mandinka_lancers_move_farther_on_their_ground) {
    GameState s = pair("CIVILIZATION_EGYPT", "CIVILIZATION_MALI", {});
    s.plot({8, 2}).feature = rules().feature("FEATURE_FOREST");
    s.plot({10, 2}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    s.plot({8, 10}).terrain = rules().terrain("TERRAIN_DESERT");
    s.plot({10, 10}).terrain = rules().terrain("TERRAIN_DESERT_HILLS");
    const UnitId open = addUnit(s, "UNIT_WAR_CHARIOT", 0, {6, 2});
    const UnitId woods = addUnit(s, "UNIT_WAR_CHARIOT", 0, {8, 2});
    const UnitId hills = addUnit(s, "UNIT_WAR_CHARIOT", 0, {10, 2});
    const UnitId heavy = addUnit(s, "UNIT_HEAVY_CHARIOT", 1, {12, 2});
    const UnitId grass = addUnit(s, "UNIT_MANDINKA_LANCER", 1, {6, 10});
    const UnitId desert = addUnit(s, "UNIT_MANDINKA_LANCER", 1, {8, 10});
    const UnitId duneHills = addUnit(s, "UNIT_MANDINKA_LANCER", 1, {10, 10});
    for (Unit& u : s.units) u.activity = Activity::Sleep;
    auto g = Game::fromScenario(rules(), std::move(s));
    const int chariot = rules().units[at(rules().unit("UNIT_HEAVY_CHARIOT"))].moves;
    const int knight = rules().units[at(rules().unit("UNIT_KNIGHT"))].moves;
    auto moves = [&](UnitId id) { return g->maxMoves(*g->state().unit(id)); };
    // The War Chariot keeps the Heavy Chariot's +1 on open ground and adds +1 on flat land.
    CHECK_EQ(moves(open), chariot + 2);
    CHECK_EQ(moves(woods), chariot + 1);
    CHECK_EQ(moves(hills), chariot);
    CHECK_EQ(moves(heavy), chariot + 1);
    // The Mandinka Lancer: +1 in desert, flat or hills.
    CHECK_EQ(moves(grass), knight);
    CHECK_EQ(moves(desert), knight + 1);
    CHECK_EQ(moves(duneHills), knight + 1);
    // Counted where the turn starts.
    sovtest::endTurns(*g, 2);
    CHECK(g->state().unit(open)->movesLeft == Fixed::fromInt(chariot + 2));
    CHECK(g->state().unit(desert)->movesLeft == Fixed::fromInt(knight + 1));
}

TEST(chasqui_royal_road_and_earthshaker_move_farther_on_roads) {
    GameState s = pair("CIVILIZATION_PERSIA", "CIVILIZATION_INCA", {});
    const int8_t road = 0;  // the Ancient Road
    // Persia: a road from its land (x 3..7) out past its border.
    for (int x = 3; x <= 10; ++x) s.plot({x, 2}).route = road;
    for (int x = 3; x <= 7; ++x) s.plot({x, 2}).owner = 0;
    s.plot({5, 3}).owner = 0;
    s.plot({7, 2}).routePillaged = true;
    s.plot({4, 6}).route = road;  // the capital's own road
    const UnitId onRoad = addUnit(s, "UNIT_WARRIOR", 0, {5, 2});
    const UnitId abroad = addUnit(s, "UNIT_WARRIOR", 0, {9, 2});
    const UnitId offRoad = addUnit(s, "UNIT_WARRIOR", 0, {5, 3});
    const UnitId pillaged = addUnit(s, "UNIT_WARRIOR", 0, {7, 2});
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {4, 2});
    const UnitId admiral = addUnit(s, "UNIT_GREAT_ADMIRAL", 0, {4, 6});
    // Inca: roads on hills and next to mountains, and a Chasqui on a plain road.
    for (int x = 12; x <= 20; ++x) s.plot({x, 11}).route = road;
    s.plot({13, 11}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    s.plot({16, 12}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    s.plot({13, 9}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    const UnitId hillRoad = addUnit(s, "UNIT_WARRIOR", 1, {13, 11});
    const UnitId mountainRoad = addUnit(s, "UNIT_WARRIOR", 1, {16, 11});
    const UnitId plainRoad = addUnit(s, "UNIT_WARRIOR", 1, {19, 11});
    const UnitId hillsOffRoad = addUnit(s, "UNIT_WARRIOR", 1, {13, 9});
    const UnitId chasqui = addUnit(s, "UNIT_CHASQUI", 1, {20, 11});
    const UnitId chasquiOff = addUnit(s, "UNIT_CHASQUI", 1, {20, 9});
    const UnitId chasquiHills = addUnit(s, "UNIT_CHASQUI", 1, {12, 11});
    s.plot({12, 11}).terrain = rules().terrain("TERRAIN_GRASS_HILLS");
    for (Unit& u : s.units) u.activity = Activity::Sleep;
    auto g = Game::fromScenario(rules(), std::move(s));
    const int warrior = rules().units[at(rules().unit("UNIT_WARRIOR"))].moves;
    const int civilian = rules().units[at(rules().unit("UNIT_BUILDER"))].moves;
    const int scout = rules().units[at(rules().unit("UNIT_CHASQUI"))].moves;
    auto moves = [&](UnitId id) { return g->maxMoves(*g->state().unit(id)); };
    // Cyrus's Royal Road: +1 on a road inside his own territory, for every land unit.
    CHECK_EQ(moves(onRoad), warrior + 1);
    CHECK_EQ(moves(abroad), warrior);
    CHECK_EQ(moves(offRoad), warrior);
    CHECK_EQ(moves(pillaged), warrior);
    CHECK_EQ(moves(builder), civilian + 1);
    CHECK_EQ(moves(admiral), rules().units[at(rules().unit("UNIT_GREAT_ADMIRAL"))].moves);  // ships are not on roads
    // Pachacuti's Earthshaker: +1 on a road on hills or next to a mountain.
    CHECK_EQ(moves(hillRoad), warrior + 1);
    CHECK_EQ(moves(mountainRoad), warrior + 1);
    CHECK_EQ(moves(plainRoad), warrior);
    CHECK_EQ(moves(hillsOffRoad), warrior);
    // The Chasqui: +1 on any road, and Earthshaker's on top.
    CHECK_EQ(moves(chasqui), scout + 1);
    CHECK_EQ(moves(chasquiOff), scout);
    CHECK_EQ(moves(chasquiHills), scout + 2);
    // Counted where the turn starts.
    sovtest::endTurns(*g, 2);
    CHECK(g->state().unit(onRoad)->movesLeft == Fixed::fromInt(warrior + 1));
    CHECK(g->state().unit(chasqui)->movesLeft == Fixed::fromInt(scout + 1));
}

TEST(calmecac_trains_units_that_learn_faster) {
    GameState s = pair("CIVILIZATION_AZTEC", "CIVILIZATION_MALI", {});
    s.cities[0].buildings.push_back(rules().building("BUILDING_CALMECAC"));
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->completeItem(g->stateMutForTests().cities[0], unit("UNIT_SLINGER")));
    REQUIRE(g->completeItem(g->stateMutForTests().cities[1], unit("UNIT_SLINGER")));
    const Unit* aztec = nullptr;
    const Unit* malian = nullptr;
    for (const Unit& u : g->state().units) {
        if (u.type != rules().unit("UNIT_SLINGER")) continue;
        (u.owner == 0 ? aztec : malian) = &u;
    }
    REQUIRE(aztec && malian);
    // +25% combat XP for good, not a head start.
    CHECK_EQ(aztec->xpBonus, 25);
    CHECK_EQ(aztec->xp, 0);
    CHECK_EQ(malian->xpBonus, 0);
}

TEST(legionaries_lay_a_road_or_a_fort_and_march_on) {
    GameState s = pair("CIVILIZATION_ROME", "CIVILIZATION_GREECE", {});
    for (const Hex& h : {Hex{6, 6}, Hex{6, 8}, Hex{6, 4}}) {
        s.plot(h).owner = 0;
        s.plot(h).city = s.cities[0].id;
    }
    const UnitId roadMaker = addUnit(s, "UNIT_LEGIONARY", 0, {6, 6});
    const UnitId fortMaker = addUnit(s, "UNIT_LEGIONARY", 0, {6, 8});
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {6, 4});
    const TypeIndex fort = rules().improvement("IMPROVEMENT_FORT");
    const TypeIndex farm = rules().improvement("IMPROVEMENT_FARM");
    auto g = Game::fromScenario(rules(), std::move(s));
    // Its charge builds a Fort, without Siege Tactics, but no farm; a Builder the other way round.
    CHECK(g->validate(Command::buildImprovement(0, fortMaker, farm)) == CommandError::CannotImprove);
    CHECK(g->validate(Command::buildImprovement(0, builder, fort)) == CommandError::CannotImprove);
    CHECK(g->validate(Command::buildImprovement(0, builder, farm)) == CommandError::Ok);
    REQUIRE(g->submit(Command::buildImprovement(0, fortMaker, fort)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 8}).improvement, fort);
    // Or a road; either way the Legionary marches on with its charge spent.
    REQUIRE(g->submit(Command::buildRoad(0, roadMaker)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).route, g->roadFor(0));
    for (UnitId id : {roadMaker, fortMaker}) {
        REQUIRE(g->state().unit(id) != nullptr);
        CHECK_EQ(g->state().unit(id)->charges, 0);
    }
}

TEST(qins_builders_lay_roads_for_no_charge) {
    GameState s = pair("CIVILIZATION_CHINA", "CIVILIZATION_EGYPT", {});
    for (const Hex& h : {Hex{6, 6}, Hex{6, 8}}) {
        s.plot(h).owner = 0;
        s.plot(h).city = s.cities[0].id;
    }
    s.plot({14, 6}).owner = 1;
    s.plot({14, 6}).city = s.cities[1].id;
    const UnitId builder = addUnit(s, "UNIT_BUILDER", 0, {6, 6});
    s.units.back().charges = 1;  // its last charge
    const UnitId warrior = addUnit(s, "UNIT_WARRIOR", 0, {6, 8});
    const UnitId egyptian = addUnit(s, "UNIT_BUILDER", 1, {14, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    // Only Qin's Builders: not his other units, nor another civ's Builders.
    CHECK(g->freeRoad(0, builder));
    CHECK(!g->freeRoad(0, warrior));
    CHECK(!g->freeRoad(1, egyptian));
    CHECK(!g->freeRoad(1, builder));
    CHECK(!g->freeRoad(0, egyptian));
    CHECK(g->validate(Command::buildRoad(0, warrior)) == CommandError::CannotImprove);
    CHECK(g->roadProblem(1, egyptian) == CommandError::CannotImprove);
    // The road takes the Builder's turn but not its last charge.
    REQUIRE(g->submit(Command::buildRoad(0, builder)) == CommandError::Ok);
    CHECK_EQ(g->state().plot({6, 6}).route, g->roadFor(0));
    REQUIRE(g->state().unit(builder) != nullptr);
    CHECK_EQ(g->state().unit(builder)->charges, 1);
    CHECK(g->validate(Command::buildRoad(0, builder)) == CommandError::CannotImprove);
}

TEST(chinampas_farms_beside_lakes) {
    // Farms next to a lake or river: +1 Food and +0.5 Housing (Aztec Chinampas).
    auto scene = [](bool lake) {
        GameState s = pair("CIVILIZATION_AZTEC", "CIVILIZATION_GREECE", {});
        for (const Hex& h : s.grid.within({4, 6}, 3)) {
            s.plot(h).owner = 0;
            s.plot(h).city = s.cities[0].id;
        }
        s.plot({6, 6}).improvement = rules().improvement("IMPROVEMENT_FARM");
        s.plot({2, 6}).improvement = rules().improvement("IMPROVEMENT_FARM");
        if (lake) s.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
        return Game::fromScenario(rules(), std::move(s));
    };
    auto dry = scene(false), wet = scene(true);
    const size_t food = static_cast<size_t>(YieldType::Food);
    const City& c = wet->state().cities[0];
    CHECK_EQ(wet->plotYields({6, 6}, c)[food], dry->plotYields({6, 6}, dry->state().cities[0])[food] + Fixed::fromInt(1));
    CHECK_EQ(wet->plotYields({2, 6}, c)[food], dry->plotYields({2, 6}, dry->state().cities[0])[food]);
    CHECK_EQ(wet->improvementHousing(c), dry->improvementHousing(dry->state().cities[0]) + Fixed::fromInt(1) / 2);
}

// Ramesses' Builder of Monuments: light cavalry and chariots +5 on flat desert and floodplains, not heavy cavalry.
TEST(builder_of_monuments_favours_light_cavalry_and_chariots) {
    GameState s = pair("CIVILIZATION_EGYPT", "CIVILIZATION_MALI", {});
    const TypeIndex desert = rules().terrain("TERRAIN_DESERT");
    for (int x = 6; x <= 12; x += 2) s.plot({x, 2}).terrain = desert;
    const UnitId foe = addUnit(s, "UNIT_WARRIOR", 1, {8, 10});
    const UnitId horse = addUnit(s, "UNIT_HORSEMAN", 0, {6, 2}), chariot = addUnit(s, "UNIT_WAR_CHARIOT", 0, {8, 2});
    const UnitId knight = addUnit(s, "UNIT_KNIGHT", 0, {10, 2}), maliHorse = addUnit(s, "UNIT_HORSEMAN", 1, {12, 2});
    const UnitId grassChariot = addUnit(s, "UNIT_WAR_CHARIOT", 0, {8, 4}), maliKnight = addUnit(s, "UNIT_KNIGHT", 1, {12, 4});
    s.plot({12, 4}).terrain = desert;
    auto g = Game::fromScenario(rules(), std::move(s));
    auto str = [&](UnitId u) { return g->combatStrength(*g->state().unit(u), *g->state().unit(foe), true, false); };
    CHECK_EQ(str(horse), str(maliHorse) + 5);
    CHECK_EQ(str(chariot), str(grassChariot) + 5);
    CHECK_EQ(str(knight), str(maliKnight));  // a Knight is heavy cavalry, not a chariot
}

// Gift of the Nile: Egypt's floodplain districts and buildings take no flood damage; a storm still wrecks them.
TEST(gift_of_the_nile_spares_floodplain_districts) {
    const auto struck = [](const char* civ0, const char* disasterId) {
        GameState s = pair(civ0, "CIVILIZATION_MALI", {});
        s.plot({5, 6}).feature = rules().feature("FEATURE_FLOODPLAINS_GRASSLAND");
        CityDistrict campus;
        campus.type = rules().district("DISTRICT_CAMPUS");
        campus.pos = {5, 6};
        campus.complete = true;
        s.cities[0].districts.push_back(campus);
        s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        sovtest::claimFor(s, s.cities[0], {5, 6});
        auto g = Game::fromScenario(rules(), std::move(s));
        TypeIndex d = kNone;
        for (size_t i = 0; i < rules().disasters.size(); ++i) d = rules().disasters[i].id == disasterId ? static_cast<TypeIndex>(i) : d;
        g->strikeDisaster(d, {5, 6});
        return g->state().cities[0].districts.back().pillagedTurns > 0;
    };
    CHECK(struck("CIVILIZATION_MALI", "DISASTER_MODERATE_FLOOD"));
    CHECK(!struck("CIVILIZATION_EGYPT", "DISASTER_MODERATE_FLOOD"));
    CHECK(!struck("CIVILIZATION_EGYPT", "DISASTER_1000_YEAR_FLOOD"));
    CHECK(struck("CIVILIZATION_EGYPT", "DISASTER_TORNADO_OUTBREAK"));
}

// Arabia's +2 Gold for a route crossing desert looks at the plots the route takes; the straight line is only the estimate
// used before a route has its way.
TEST(arabias_desert_routes_follow_the_way_taken) {
    GameState s = pair("CIVILIZATION_ARABIA", "CIVILIZATION_MALI", {});
    addCity(s, 0, {12, 6}, false, 3);
    s.plot({8, 2}).terrain = rules().terrain("TERRAIN_DESERT");  // off the straight line
    std::vector<int32_t> straight, around;
    for (int x = 4; x <= 12; ++x) straight.push_back(s.grid.index({x, 6}));
    for (int x = 4; x <= 12; ++x) around.push_back(s.grid.index({x, x == 8 ? 2 : 6}));
    const auto routeGold = [&](const std::vector<int32_t>& path) {
        GameState t = s;
        TradeRoute tr;
        tr.id = 1;
        tr.owner = 0;
        tr.origin = t.cities[0].id;
        tr.destination = t.cities[2].id;
        tr.traderType = rules().unit("UNIT_TRADER");
        tr.path = path;
        tr.turnsLeft = 10;
        t.tradeRoutes.push_back(tr);
        auto g = Game::fromScenario(rules(), std::move(t));
        return g->cityReport(g->state().cities[0].id).yields[static_cast<size_t>(YieldType::Gold)];
    };
    CHECK_EQ(routeGold(around), routeGold(straight) + Fixed::fromInt(2));
    auto g = Game::fromScenario(rules(), s);
    const City& a = g->state().cities[0];
    const City& b = g->state().cities[2];
    const size_t gold = static_cast<size_t>(YieldType::Gold);
    CHECK_EQ(g->tradeRouteYields(a, b, &around)[gold], g->tradeRouteYields(a, b, &straight)[gold] + Fixed::fromInt(2));
    CHECK_EQ(g->tradeRouteYields(a, b)[gold], g->tradeRouteYields(a, b, &straight)[gold]);  // the line misses (8,2)
}
