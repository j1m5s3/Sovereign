// Specialty districts (03-districts-buildings-wonders.md, Districts: general rules).
#include <algorithm>

#include "helpers.h"
#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
TypeIndex district(const char* id) { return rules().district(id); }
ProductionItem item(const char* id) { return {ProductionKind::District, district(id)}; }
const Hex kCenter{6, 6};

void learn(GameState& s, PlayerId p, std::initializer_list<const char*> techs) {
    Player& pl = s.players[static_cast<size_t>(p)];
    pl.techs.resize(rules().techs.size());
    for (const char* t : techs) pl.techs.done[at(rules().tech(t))] = 1;
}

// One city at (6,6) owning every plot within 3, with Writing, Astrology, Bronze
// Working, Currency and Apprenticeship known. `edit` sets the scene.
template <typename Edit>
std::unique_ptr<Game> town(int population, Edit edit) {
    GameState s = flatState(20, 14, 1);
    const CityId id = addCity(s, 0, kCenter, true, population);
    for (const Hex& h : s.grid.within(kCenter, 3)) {
        s.plot(h).owner = 0;
        s.plot(h).city = id;
    }
    learn(s, 0, {"TECH_WRITING", "TECH_ASTROLOGY", "TECH_BRONZE_WORKING", "TECH_CURRENCY", "TECH_APPRENTICESHIP"});
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}
}  // namespace

TEST(district_data_from_civ_tables) {
    const Rules& r = rules();
    const DistrictType& campus = r.districts[at(district("DISTRICT_CAMPUS"))];
    CHECK_EQ(campus.cost, 54);
    CHECK(!campus.unlock.civic && campus.unlock.index == r.tech("TECH_WRITING"));
    CHECK(campus.needsPopulation);
    CHECK_EQ(campus.costDiscountPercent, 40);
    CHECK_EQ(campus.adjacency.size(), 8u);
    CHECK(r.districts[at(district("DISTRICT_THEATER_SQUARE"))].unlock.civic);
    CHECK(r.districts[at(district("DISTRICT_ENCAMPMENT"))].notAdjacentToCityCenter);
    CHECK_EQ(r.buildings[at(r.building("BUILDING_LIBRARY"))].districtType, district("DISTRICT_CAMPUS"));
    CHECK_EQ(r.buildings[at(r.building("BUILDING_MONUMENT"))].districtType, district("DISTRICT_CITY_CENTER"));
}

TEST(district_limit_follows_population) {
    auto g = town(1, [](GameState&) {});
    City c = *g->state().city(1);
    CHECK_EQ(g->districtLimit(c), 1);
    c.population = 4;
    CHECK_EQ(g->districtLimit(c), 2);
    c.population = 7;
    CHECK_EQ(g->districtLimit(c), 3);
}

TEST(district_placement_rules) {
    const Hex woods{8, 6};
    auto g = town(1, [&](GameState& s) { s.plot(woods).feature = rules().feature("FEATURE_FOREST"); });
    const City& c = *g->state().city(1);
    CHECK(!g->canPlaceDistrict(c, district("DISTRICT_CAMPUS"), kCenter));      // the city center
    CHECK(!g->canPlaceDistrict(c, district("DISTRICT_CAMPUS"), {10, 6}));      // four plots out
    CHECK(!g->canPlaceDistrict(c, district("DISTRICT_ENCAMPMENT"), {7, 6}));   // next to the center
    CHECK(g->canPlaceDistrict(c, district("DISTRICT_ENCAMPMENT"), woods));
    CHECK(!g->canPlaceDistrict(c, district("DISTRICT_THEATER_SQUARE"), woods));  // Drama and Poetry unknown
    CHECK_EQ(g->submit(Command::setProduction(0, 1, item("DISTRICT_CAMPUS"))), CommandError::BadTarget);
    CHECK_EQ(g->submit(Command::setProduction(0, 1, item("DISTRICT_CAMPUS"), woods)), CommandError::Ok);
    const City& placed = *g->state().city(1);
    REQUIRE(placed.districts.size() == 1u);
    CHECK_EQ(placed.districts[0].pos, woods);
    CHECK(!placed.districts[0].complete);
    CHECK_EQ(g->state().plot(woods).feature, kNone);  // placing clears woods
    // The plot is reserved: no citizen, no improvement, no second district.
    std::vector<Hex> workable = g->workablePlots(placed);
    CHECK(std::find(workable.begin(), workable.end(), woods) == workable.end());
    CHECK(!g->canImproveAt(0, woods, rules().improvement("IMPROVEMENT_FARM")));
    // Population 1 allows one district; re-selecting the placed one needs no plot.
    CHECK_EQ(g->submit(Command::queueProduction(0, 1, item("DISTRICT_HOLY_SITE"), {5, 4})), CommandError::CannotBuild);
    CHECK_EQ(g->submit(Command::setProduction(0, 1, {ProductionKind::Unit, rules().unit("UNIT_WARRIOR")})), CommandError::Ok);
    CHECK_EQ(g->submit(Command::setProduction(0, 1, item("DISTRICT_CAMPUS"))), CommandError::Ok);
    CHECK_EQ(g->state().city(1)->districts.size(), 1u);
    // Districts are never bought.
    CHECK_EQ(g->purchaseCost(0, item("DISTRICT_CAMPUS")), -1);
}

// A Mountain Tunnel opens its mountain to units, not to building: no district goes on a Mountain (03), and a wonder
// only when it is built on one (Machu Picchu).
TEST(a_tunnelled_mountain_holds_no_district_and_only_a_mountain_wonder) {
    const Hex peak{7, 6}, flat{6, 7};  // both beside the City Center
    auto g = town(1, [&](GameState& s) {
        s.plot(peak).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        s.plot(peak).improvement = rules().improvement("IMPROVEMENT_MOUNTAIN_TUNNEL");
    });
    const City& c = *g->state().city(1);
    REQUIRE(isLandPassable(g->state(), rules(), peak));
    CHECK(!g->canPlaceDistrict(c, district("DISTRICT_CAMPUS"), peak));
    CHECK(g->canPlaceDistrict(c, district("DISTRICT_CAMPUS"), flat));
    const TypeIndex basil = rules().building("BUILDING_ST_BASIL_S_CATHEDRAL");  // anywhere beside the City Center
    CHECK(!g->canPlaceWonder(c, basil, peak));
    CHECK(g->canPlaceWonder(c, basil, flat));
    CHECK(g->canPlaceWonder(c, rules().building("BUILDING_MACHU_PICCHU"), peak));
}

// A district or wonder may go over a strategic resource still hidden (03), and once the resource is revealed its
// owner gathers it as from an improved plot ("it is granted once revealed").
TEST(a_hidden_strategic_resource_under_a_district_or_wonder_is_granted_once_revealed) {
    const Hex spot{7, 6};
    const TypeIndex coal = rules().resource("RESOURCE_COAL");
    enum class Over { Nothing, District, Wonder };
    const auto gathered = [&](bool revealed, Over over) {
        auto g = town(3, [&](GameState& s) {
            s.plot(spot).resource = coal;
            const Unlock& reveal = rules().resources[at(coal)].reveal;
            if (revealed && reveal.civic) s.players[0].civics.done[at(reveal.index)] = 1;
            if (revealed && !reveal.civic) s.players[0].techs.done[at(reveal.index)] = 1;
            City& c = *s.city(1);
            if (over == Over::District) c.districts.push_back({district("DISTRICT_CAMPUS"), spot, true});
            if (over == Over::Wonder) c.wonders.push_back({rules().building("BUILDING_ST_BASIL_S_CATHEDRAL"), spot});
            c.queue = {{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")}};
        });
        // Placed only while it is hidden.
        if (over == Over::Nothing) CHECK_EQ(g->canPlaceDistrict(*g->state().city(1), district("DISTRICT_CAMPUS"), spot), !revealed);
        const int before = g->state().players[0].stockpile[at(coal)];
        endTurns(*g, 1);
        return g->state().players[0].stockpile[at(coal)] - before;
    };
    const int perTurn = rules().resources[at(coal)].accumulation;
    REQUIRE(perTurn > 0);
    CHECK_EQ(gathered(true, Over::District), perTurn);
    CHECK_EQ(gathered(true, Over::Wonder), perTurn);
    CHECK_EQ(gathered(true, Over::Nothing), 0);
    CHECK_EQ(gathered(false, Over::District), 0);
    CHECK_EQ(gathered(false, Over::Nothing), 0);
}

TEST(district_adjacency_yields) {
    const Hex spot{8, 6};
    // Neighbours of (8,6): (7,5) (8,5) (7,6) (9,6) (7,7) (8,7); the center (6,6) is two away.
    auto g = town(4, [&](GameState& s) {
        for (Hex h : {Hex{8, 5}, Hex{9, 6}}) s.plot(h).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        for (Hex h : {Hex{7, 7}, Hex{8, 7}}) s.plot(h).feature = rules().feature("FEATURE_JUNGLE");
    });
    // 2 mountains (+1 each), 2 rainforest (+1 per 2).
    Yields y = g->districtAdjacency(0, district("DISTRICT_CAMPUS"), spot);
    CHECK_EQ(y[static_cast<size_t>(YieldType::Science)], Fixed::fromInt(3));
    CHECK_EQ(g->districtAdjacency(0, district("DISTRICT_HOLY_SITE"), spot)[static_cast<size_t>(YieldType::Faith)],
             Fixed::fromInt(2));
    // One neighbouring district gives nothing, two give +1 (placed districts count before they finish).
    CHECK_EQ(g->submit(Command::setProduction(0, 1, item("DISTRICT_HOLY_SITE"), {7, 6})), CommandError::Ok);
    CHECK_EQ(g->districtAdjacency(0, district("DISTRICT_CAMPUS"), spot)[static_cast<size_t>(YieldType::Science)],
             Fixed::fromInt(3));
    CHECK_EQ(g->submit(Command::queueProduction(0, 1, item("DISTRICT_COMMERCIAL_HUB"), {7, 5})), CommandError::Ok);
    y = g->districtAdjacency(0, district("DISTRICT_CAMPUS"), spot);
    CHECK_EQ(y[static_cast<size_t>(YieldType::Science)], Fixed::fromInt(4));

    // Commercial Hub on a river: +2 gold; Industrial Zone: quarry +1, two mines +1.
    auto river = town(1, [&](GameState& s) {
        s.plot(spot).riverEdges = kRiverE;
        s.plot({8, 5}).improvement = rules().improvement("IMPROVEMENT_QUARRY");
        s.plot({7, 7}).improvement = rules().improvement("IMPROVEMENT_MINE");
        s.plot({8, 7}).improvement = rules().improvement("IMPROVEMENT_MINE");
    });
    CHECK_EQ(river->districtAdjacency(0, district("DISTRICT_COMMERCIAL_HUB"), spot)[static_cast<size_t>(YieldType::Gold)],
             Fixed::fromInt(2));
    CHECK_EQ(river->districtAdjacency(0, district("DISTRICT_INDUSTRIAL_ZONE"), spot)[static_cast<size_t>(YieldType::Production)],
             Fixed::fromInt(2));

    // Commercial Hub beside a Harbor: +2 gold (one district alone is too few for the +1 per two districts).
    auto port = town(1, [&](GameState& s) {
        s.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
        s.cities[0].districts.push_back({district("DISTRICT_HARBOR"), {7, 6}, true});
    });
    CHECK_EQ(port->districtAdjacency(0, district("DISTRICT_COMMERCIAL_HUB"), spot)[static_cast<size_t>(YieldType::Gold)],
             Fixed::fromInt(2));

    // Natural Philosophy doubles Campus adjacency.
    auto wise = town(1, [&](GameState& s) {
        s.plot({8, 5}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        s.players[0].government = rules().government("GOVERNMENT_CHIEFDOM");
        s.players[0].policies = {kNone, rules().policy("POLICY_NATURAL_PHILOSOPHY")};
    });
    CHECK_EQ(wise->districtAdjacency(0, district("DISTRICT_CAMPUS"), spot)[static_cast<size_t>(YieldType::Science)],
             Fixed::fromInt(2));
    // Only the Campus's: the Holy Site keeps +1 from the mountain.
    CHECK_EQ(wise->districtAdjacency(0, district("DISTRICT_HOLY_SITE"), spot)[static_cast<size_t>(YieldType::Faith)],
             Fixed::fromInt(1));
}

TEST(wonders_and_natural_wonders_raise_adjacency) {
    // Holy Site +2 Faith per natural wonder plot; Theater Square +2 Culture per finished world wonder; the Great
    // Barrier Reef +2 Science to a Campus; Pamukkale +1 Faith, +2 Science, +2 Gold, +2 Culture (data: districts).
    const Hex spot{8, 6};
    auto g = town(1, [&](GameState& s) {
        s.plot({8, 5}).feature = rules().feature("FEATURE_GREAT_BARRIER_REEF");
        s.plot({9, 6}).feature = rules().feature("FEATURE_PAMUKKALE");
        City& c = s.cities[0];
        const TypeIndex stonehenge = rules().building("BUILDING_STONEHENGE"), pyramids = rules().building("BUILDING_PYRAMIDS");
        c.wonders.push_back({stonehenge, {7, 7}});  // finished
        c.buildings.push_back(stonehenge);
        std::sort(c.buildings.begin(), c.buildings.end());
        c.wonders.push_back({pyramids, {8, 7}});  // still being built
    });
    const auto adj = [&](const char* d, YieldType y) { return g->districtAdjacency(0, district(d), spot)[static_cast<size_t>(y)]; };
    CHECK_EQ(adj("DISTRICT_HOLY_SITE", YieldType::Faith), Fixed::fromInt(5));
    CHECK_EQ(adj("DISTRICT_CAMPUS", YieldType::Science), Fixed::fromInt(4));
    CHECK_EQ(adj("DISTRICT_COMMERCIAL_HUB", YieldType::Gold), Fixed::fromInt(2));
    CHECK_EQ(adj("DISTRICT_THEATER_SQUARE", YieldType::Culture), Fixed::fromInt(4));
}

TEST(district_cost_grows_with_progress_and_discounts) {
    auto fresh = town(1, [](GameState& s) { s.players[0].techs.done.assign(rules().techs.size(), 0); });
    CHECK_EQ(fresh->districtCost(0, district("DISTRICT_CAMPUS")), 54);
    // x (1 + 9 x share of the tech tree known): 5 techs known here.
    auto g = town(1, [](GameState&) {});
    const int n = static_cast<int>(rules().techs.size());
    const int expect = 54 * (n + 9 * 5) / n;
    const int campus = g->districtCost(0, district("DISTRICT_CAMPUS"));
    CHECK(campus >= expect - 1 && campus <= expect);
    // With only Writing and Astrology known (A = 2) and two Campuses done (B = 2),
    // a Holy Site (none yet) costs 40% less; a third Campus does not.
    GameState s = flatState(20, 14, 1);
    addCity(s, 0, kCenter, true, 4);
    addCity(s, 0, {14, 6}, false, 4);
    learn(s, 0, {"TECH_WRITING", "TECH_ASTROLOGY"});
    s.city(1)->districts.push_back({district("DISTRICT_CAMPUS"), {8, 6}, true});
    s.city(2)->districts.push_back({district("DISTRICT_CAMPUS"), {16, 6}, true});
    auto d = Game::fromScenario(rules(), std::move(s));
    const int full = d->districtCost(0, district("DISTRICT_CAMPUS"));
    const int holy = d->districtCost(0, district("DISTRICT_HOLY_SITE"));
    CHECK(holy >= full * 60 / 100 - 1 && holy <= full * 60 / 100 + 1);
}

TEST(finished_district_yields_and_unlocks_buildings) {
    const Hex spot{8, 6};
    auto g = town(1, [&](GameState& s) {
        s.plot({8, 5}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
        City& c = *s.city(1);
        c.districts.push_back({district("DISTRICT_CAMPUS"), spot, false});
        c.queue = {item("DISTRICT_CAMPUS")};
        c.progress.push_back({item("DISTRICT_CAMPUS"), Fixed::fromInt(5000)});
    });
    const ProductionItem library{ProductionKind::Building, rules().building("BUILDING_LIBRARY")};
    CHECK(!g->canProduce(*g->state().city(1), library));
    const Fixed science = g->cityReport(1).yields[static_cast<size_t>(YieldType::Science)];
    const Fixed gold = g->goldPerTurn(0);
    endTurns(*g, 1);
    const City& c = *g->state().city(1);
    REQUIRE(c.districts.size() == 1u);
    CHECK(c.districts[0].complete);
    CHECK(g->canProduce(c, library));
    // +1 science from the mountain, and 1 gold of district maintenance.
    CHECK_EQ(g->cityReport(1).yields[static_cast<size_t>(YieldType::Science)], science + Fixed::fromInt(1));
    CHECK_EQ(g->goldPerTurn(0), gold - Fixed::fromInt(1));
    CHECK(!g->canProduce(c, item("DISTRICT_CAMPUS")));  // one per city

    // Districts survive a save round trip.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
    CHECK(loaded->state().city(1)->districts[0].complete);
}

TEST(aqueduct_and_neighborhood_add_housing) {
    // A dry inland city: the Aqueduct needs a Mountain (or river, lake, oasis) beside its plot.
    auto g = town(3, [](GameState& s) {
        learn(s, 0, {"TECH_ENGINEERING"});
        s.plot({8, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    });
    const City& c = g->state().cities[0];
    const TypeIndex aqueduct = district("DISTRICT_AQUEDUCT");
    CHECK(!g->canPlaceDistrict(c, aqueduct, {5, 6}));  // nothing watery beside it
    REQUIRE(g->canPlaceDistrict(c, aqueduct, {7, 6}));  // next to the center and the Mountain
    CHECK(!g->canPlaceDistrict(c, aqueduct, {9, 6}));  // not next to the City Center
    const Fixed before = g->cityReport(c.id).housing;
    GameState s = g->state();
    s.cities[0].districts.push_back({aqueduct, {7, 6}, true});
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK(g2->cityReport(g2->state().cities[0].id).housing == before + Fixed::fromInt(4));  // 2 (no water) up to 6
    // A Neighborhood on Average ground: 4 housing; Woods all round make it Breathtaking: 6.
    GameState s3 = g2->state();
    s3.cities[0].districts.push_back({district("DISTRICT_NEIGHBORHOOD"), {6, 8}, true});
    auto g3 = Game::fromScenario(rules(), s3);
    CHECK_EQ(g3->plotAppeal({6, 8}), 0);
    CHECK(g3->cityReport(g3->state().cities[0].id).housing == before + Fixed::fromInt(8));
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = s3.grid.neighbor({6, 8}, static_cast<Dir>(d));
        if (n && *n != kCenter) s3.plot(*n).feature = rules().feature("FEATURE_FOREST");
    }
    auto g4 = Game::fromScenario(rules(), std::move(s3));
    CHECK(g4->plotAppeal({6, 8}) >= 4);
    CHECK(g4->cityReport(g4->state().cities[0].id).housing == before + Fixed::fromInt(10));
}

TEST(lakes_feed_aqueducts_and_raise_appeal) {
    // A lake beside the Aqueduct's plot is fresh water; the sea is not (01: Lake, 03: Aqueduct).
    const TypeIndex coast = rules().terrain("TERRAIN_COAST");
    auto dry = town(3, [](GameState& s) { learn(s, 0, {"TECH_ENGINEERING"}); });
    auto lake = town(3, [&](GameState& s) {
        learn(s, 0, {"TECH_ENGINEERING"});
        s.plot({8, 6}).terrain = coast;
    });
    auto sea = town(3, [&](GameState& s) {
        learn(s, 0, {"TECH_ENGINEERING"});
        for (int x = 8; x <= 17; ++x) s.plot({x, 6}).terrain = coast;  // ten plots: a sea
    });
    const TypeIndex aqueduct = district("DISTRICT_AQUEDUCT");
    CHECK(!dry->canPlaceDistrict(dry->state().cities[0], aqueduct, {7, 6}));
    CHECK(lake->canPlaceDistrict(lake->state().cities[0], aqueduct, {7, 6}));
    CHECK(!sea->canPlaceDistrict(sea->state().cities[0], aqueduct, {7, 6}));
    // Appeal: +1 for the water beside it, and +1 once beside a lake, as beside a river.
    CHECK_EQ(lake->plotAppeal({7, 6}), dry->plotAppeal({7, 6}) + 2);
    CHECK_EQ(sea->plotAppeal({7, 6}), dry->plotAppeal({7, 6}) + 1);
    // A city beside a lake already has fresh water, so its Aqueduct adds CITY_POPULATION_AQUEDUCT_BOOST (+2).
    auto lakeside = town(3, [&](GameState& s) {
        learn(s, 0, {"TECH_ENGINEERING"});
        s.plot({7, 6}).terrain = coast;
    });
    REQUIRE(lakeside->canPlaceDistrict(lakeside->state().cities[0], aqueduct, {6, 7}));
    GameState built = lakeside->state();
    built.cities[0].districts.push_back({aqueduct, {6, 7}, true});
    auto watered = Game::fromScenario(rules(), std::move(built));
    CHECK(watered->cityReport(watered->state().cities[0].id).housing == lakeside->cityReport(lakeside->state().cities[0].id).housing + Fixed::fromInt(2));
    // A city on the sea has 3 Housing from it, so its Aqueduct adds 3 (up to CITY_POPULATION_AQUEDUCT_MIN).
    auto shore = town(3, [&](GameState& s) {
        learn(s, 0, {"TECH_ENGINEERING"});
        for (int x = 2; x <= 11; ++x) s.plot({x, 7}).terrain = coast;  // ten plots: a sea beside the City Center
        s.plot({8, 6}).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    });
    REQUIRE(shore->canPlaceDistrict(shore->state().cities[0], aqueduct, {7, 6}));
    GameState piped = shore->state();
    piped.cities[0].districts.push_back({aqueduct, {7, 6}, true});
    auto seaside = Game::fromScenario(rules(), std::move(piped));
    CHECK(seaside->cityReport(seaside->state().cities[0].id).housing == shore->cityReport(shore->state().cities[0].id).housing + Fixed::fromInt(3));
}

// Appeal (03): each district beside a plot adds its own Appeal (Holy Site and Theater Square +1, Industrial Zone -1)
// and each wonder +1, on the rows beside the plot as on its own; the Biosphère's holder gets +1 from a Marsh beside it,
// and a barbarian camp beside it takes 1.
TEST(districts_wonders_and_the_biosphere_beside_a_plot_change_its_appeal) {
    const Hex plot{6, 8};  // beside it: (6, 7) and (5, 7) above, (5, 8) and (7, 8) on its row, (6, 9) and (5, 9) below
    auto plain = town(3, [](GameState&) {});
    auto built = town(3, [](GameState& s) {
        City& c = s.cities[0];
        c.districts.push_back({district("DISTRICT_HOLY_SITE"), {6, 7}, true});
        c.districts.push_back({district("DISTRICT_THEATER_SQUARE"), {5, 9}, true});
        c.districts.push_back({district("DISTRICT_INDUSTRIAL_ZONE"), {5, 8}, true});
        c.wonders.push_back({rules().building("BUILDING_STONEHENGE"), {6, 9}});
    });
    CHECK_EQ(built->plotAppeal(plot), plain->plotAppeal(plot) + 2);
    auto marsh = [](bool biosphere) {
        return town(3, [&](GameState& s) {
            s.plot({7, 8}).feature = rules().feature("FEATURE_MARSH");
            if (!biosphere) return;
            s.cities[0].buildings.push_back(rules().building("BUILDING_BIOSPH_RE"));
            std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
        });
    };
    CHECK_EQ(marsh(false)->plotAppeal(plot), plain->plotAppeal(plot) - 1);
    CHECK_EQ(marsh(true)->plotAppeal(plot), plain->plotAppeal(plot));
    // A barbarian camp beside it: -1, the Barbarian Outpost's Appeal (data/improvements.md).
    auto camped = town(3, [](GameState& s) {
        Camp c;
        c.id = s.nextCampId++;
        c.pos = {7, 8};
        s.camps.push_back(c);
    });
    CHECK_EQ(camped->plotAppeal(plot), plain->plotAppeal(plot) - 1);
    // A pillaged district beside it: -1, whatever its own Appeal (01: "pillaged tiles").
    auto pillaged = town(3, [](GameState& s) {
        CityDistrict d{district("DISTRICT_HOLY_SITE"), {6, 7}, true};
        d.pillagedTurns = 1;
        s.cities[0].districts.push_back(d);
    });
    CHECK_EQ(pillaged->plotAppeal(plot), plain->plotAppeal(plot) - 1);
}

// A pillaged district stops working until it is repaired (03: Pillage), and so do its buildings: its Housing, Amenities,
// great person points, trade route capacity, Great Works' yields and Tourism, its buildings' modifiers and a Dam's
// flood protection all go, as if the district were not there.
TEST(a_pillaged_district_and_its_buildings_stop_working) {
    struct Site {
        const char* district;
        Hex pos;
        std::vector<const char*> buildings;
    };
    // No two beside each other, so a pillaged one changes no other's Appeal or adjacency.
    const std::vector<Site> sites = {
        {"DISTRICT_CAMPUS", {6, 8}, {"BUILDING_LIBRARY", "BUILDING_UNIVERSITY"}},
        {"DISTRICT_COMMERCIAL_HUB", {8, 6}, {"BUILDING_MARKET"}},
        {"DISTRICT_ENTERTAINMENT_COMPLEX", {4, 6}, {"BUILDING_ZOO"}},
        {"DISTRICT_NEIGHBORHOOD", {6, 3}, {}},
        {"DISTRICT_THEATER_SQUARE", {4, 8}, {"BUILDING_AMPHITHEATER"}},
        {"DISTRICT_GOVERNMENT_PLAZA", {8, 4}, {"BUILDING_AUDIENCE_CHAMBER"}},
        {"DISTRICT_DAM", {4, 4}, {}},
    };
    const Hex jungle{7, 5};  // the Zoo: +1 Science on its city's rainforest
    // Every site built but `drop` (left out with its buildings), with `pillage` pillaged.
    const auto scene = [&](const std::string& pillage, const std::string& drop) {
        return town(10, [&](GameState& s) {
            City& c = s.cities[0];
            for (const Site& site : sites) {
                REQUIRE(s.grid.distance(kCenter, site.pos) <= 3);
                if (drop == site.district) continue;
                CityDistrict d{district(site.district), site.pos, true};
                d.pillagedTurns = pillage == site.district ? 3 : 0;
                c.districts.push_back(d);
                for (const char* b : site.buildings) c.buildings.push_back(rules().building(b));
            }
            std::sort(c.buildings.begin(), c.buildings.end());
            if (drop != "DISTRICT_THEATER_SQUARE") {
                GreatWork w;
                w.type = rules().greatWorkType("WRITING");
                w.building = rules().building("BUILDING_AMPHITHEATER");
                c.greatWorks.push_back(w);
            }
            s.plot(jungle).feature = rules().feature("FEATURE_JUNGLE");
        });
    };
    const auto measure = [&](const Game& g) {
        const City& c = g.state().cities[0];
        const CityReport r = g.cityReport(c.id);
        std::vector<std::pair<std::string, int64_t>> m = {
            {"housing", r.housing.raw()},
            {"amenities", r.amenities},
            {"culture", r.yields[static_cast<size_t>(YieldType::Culture)].raw()},
            {"trade capacity", g.tradeRouteCapacity(0)},
            {"tourism", g.tourismBase(0)},
            {"loyalty", g.loyaltyPerTurn(c.id).raw()},
            {"zoo science", g.plotYields(jungle, c)[static_cast<size_t>(YieldType::Science)].raw()},
            {"flood protection", g.cityPrevents(c.id, true) ? 1 : 0},
        };
        for (size_t k = 0; k < rules().greatPersonClasses.size(); ++k)
            m.emplace_back(rules().greatPersonClasses[k].id, g.greatPersonPointsPerTurn(0, static_cast<TypeIndex>(k)));
        return m;
    };
    const auto all = measure(*scene("", ""));
    for (const Site& site : sites) {
        const auto pillaged = measure(*scene(site.district, "")), gone = measure(*scene("", site.district));
        CHECK(gone != all);  // the site does something while it works
        for (size_t k = 0; k < gone.size(); ++k) {
            if (pillaged[k].second != gone[k].second)
                sovtest::fail(__FILE__, __LINE__, std::string(site.district) + " pillaged: " + gone[k].first + " " +
                                                      sovtest::showPair(pillaged[k].second, gone[k].second), false);
        }
    }
}

// The rest of a pillaged district's buildings' bonuses go too: their yields and Amenities while the city is powered,
// great people's yields on them (Hypatia's Libraries, James Watt's Factories), Magnus's Industrialist on a power plant,
// and civ abilities on district buildings (Charlemagne's Holy Site buildings, Japan's Encampment buildings).
TEST(a_pillaged_districts_buildings_lose_their_bonuses) {
    struct Site {
        const char* district;
        Hex pos;
        std::vector<const char*> buildings;
    };
    const std::vector<Site> sites = {
        {"DISTRICT_INDUSTRIAL_ZONE", {8, 6}, {"BUILDING_WORKSHOP", "BUILDING_FACTORY", "BUILDING_COAL_POWER_PLANT"}},
        {"DISTRICT_ENTERTAINMENT_COMPLEX", {4, 6}, {"BUILDING_ARENA", "BUILDING_STADIUM"}},
        {"DISTRICT_CAMPUS", {6, 3}, {"BUILDING_LIBRARY"}},
        {"DISTRICT_HOLY_SITE", {4, 8}, {"BUILDING_SHRINE"}},
        {"DISTRICT_ENCAMPMENT", {8, 4}, {"BUILDING_BARRACKS"}},
    };
    const auto scene = [&](const char* civ, const std::string& pillage, const std::string& drop) {
        return town(10, [&](GameState& s) {
            Player& p = s.players[0];
            p.civ = rules().civ(civ);
            for (const char* gp : {"GREAT_PERSON_HYPATIA", "GREAT_PERSON_JAMES_WATT"}) p.greatPeopleActivated.push_back(rules().greatPerson(gp));
            City& c = s.cities[0];
            for (const Site& site : sites) {
                REQUIRE(s.grid.distance(kCenter, site.pos) <= 3);
                if (drop == site.district) continue;
                CityDistrict d{district(site.district), site.pos, true};
                d.pillagedTurns = pillage == site.district ? 3 : 0;
                c.districts.push_back(d);
                for (const char* b : site.buildings) c.buildings.push_back(rules().building(b));
            }
            std::sort(c.buildings.begin(), c.buildings.end());
            c.powerDemand = 1;  // powered (a turn's end works these out)
            c.powerSupply = 20;
            Governor magnus;
            magnus.type = rules().governor("GOVERNOR_MAGNUS");
            magnus.city = c.id;
            magnus.promotions = {rules().governors[at(magnus.type)].promotions.front(), rules().governorPromotion("GOVERNOR_PROMOTION_INDUSTRIALIST")};
            p.governors.push_back(magnus);
        });
    };
    const auto measure = [](const Game& g) {
        const CityReport r = g.cityReport(g.state().cities[0].id);
        std::vector<int64_t> m{r.amenities};
        for (const Fixed& y : r.yields) m.push_back(y.raw());
        return m;
    };
    for (const char* civ : {"CIVILIZATION_FRANCE", "CIVILIZATION_JAPAN"}) {
        const auto all = measure(*scene(civ, "", ""));
        for (const Site& site : sites) {
            const auto pillaged = measure(*scene(civ, site.district, "")), gone = measure(*scene(civ, "", site.district));
            CHECK(gone != all);
            for (size_t k = 0; k < gone.size(); ++k) {
                if (pillaged[k] != gone[k])
                    sovtest::fail(__FILE__, __LINE__, std::string(civ) + " " + site.district + " pillaged: " + (k ? "yield " + std::to_string(k - 1) : "amenities") +
                                                          " " + sovtest::showPair(pillaged[k], gone[k]), false);
            }
        }
    }
}

TEST(entertainment_districts_are_exclusive_and_bring_amenities) {
    auto g = town(7, [](GameState& s) { learn(s, 0, {"TECH_ENGINEERING"}); });
    GameState s = g->state();
    Player& p = s.players[0];
    p.civics.resize(rules().civics.size());
    p.civics.done[at(rules().civic("CIVIC_GAMES_AND_RECREATION"))] = 1;
    p.civics.done[at(rules().civic("CIVIC_NATURAL_HISTORY"))] = 1;
    s.plot({6, 9}).terrain = rules().terrain("TERRAIN_COAST");
    auto g2 = Game::fromScenario(rules(), s);
    const int before = g2->cityReport(g2->state().cities[0].id).amenities;
    s.cities[0].districts.push_back({district("DISTRICT_ENTERTAINMENT_COMPLEX"), {7, 7}, true});
    auto g3 = Game::fromScenario(rules(), std::move(s));
    const City& c = g3->state().cities[0];
    CHECK_EQ(g3->cityReport(c.id).amenities, before + 1);
    CHECK(!g3->canPlaceDistrict(c, district("DISTRICT_WATER_PARK"), {6, 9}));  // exclusive with the Entertainment Complex
}

TEST(no_district_goes_on_a_plot_already_built_on) {
    // A district or a wonder on a plot three rows from the city's takes the plot, as one on any plot within 3 does.
    const Hex holy{6, 9}, wonder{5, 9}, open{7, 9};
    auto g = town(7, [&](GameState& s) {
        s.cities[0].districts.push_back({district("DISTRICT_HOLY_SITE"), holy, true});
        s.cities[0].wonders.push_back({rules().building("BUILDING_PYRAMIDS"), wonder});
    });
    REQUIRE(g->state().grid.distance(kCenter, holy) == 3);
    REQUIRE(g->state().grid.distance(kCenter, wonder) == 3);
    const std::vector<Hex> plots = g->districtPlots(1, district("DISTRICT_CAMPUS"));
    CHECK(std::find(plots.begin(), plots.end(), open) != plots.end());
    CHECK(std::find(plots.begin(), plots.end(), holy) == plots.end());
    CHECK(std::find(plots.begin(), plots.end(), wonder) == plots.end());
    // Nor does a barbarian camp's, in the city's land.
    auto camped = town(7, [&](GameState& s) {
        Camp c;
        c.id = s.nextCampId++;
        c.pos = open;
        s.camps.push_back(c);
    });
    const std::vector<Hex> left = camped->districtPlots(1, district("DISTRICT_CAMPUS"));
    CHECK(std::find(left.begin(), left.end(), open) == left.end());
}

TEST(one_per_civ_and_exclusive_districts_leave_the_build_list) {
    // A Government Plaza is one per civ; a Water Park keeps an Entertainment Complex out of its city (03).
    const auto lists = [](bool plaza, const char* districtId) {
        auto g = town(7, [&](GameState& s) {
            const CityId second = addCity(s, 0, {14, 6}, false, 7);
            Player& p = s.players[0];
            p.civics.resize(rules().civics.size());
            for (const char* c : {"CIVIC_STATE_WORKFORCE", "CIVIC_GAMES_AND_RECREATION", "CIVIC_NATURAL_HISTORY"}) p.civics.done[at(rules().civic(c))] = 1;
            if (plaza) s.cities[0].districts.push_back({district("DISTRICT_GOVERNMENT_PLAZA"), {5, 4}, true});
            s.city(second)->districts.push_back({district("DISTRICT_WATER_PARK"), {14, 7}, true});
        });
        const std::vector<ProductionItem> items = g->buildableItems(g->state().cities[1].id);
        return std::find(items.begin(), items.end(), item(districtId)) != items.end();
    };
    CHECK(lists(false, "DISTRICT_GOVERNMENT_PLAZA"));
    CHECK(!lists(true, "DISTRICT_GOVERNMENT_PLAZA"));
    CHECK(!lists(false, "DISTRICT_ENTERTAINMENT_COMPLEX"));
    CHECK(lists(false, "DISTRICT_CAMPUS"));
}

TEST(a_canal_links_two_waters_and_lets_ships_through) {
    // Water west at (3,8) and east at (5,8) around a land plot (4,8) next to the city's ring.
    auto g = town(6, [](GameState& s) {
        learn(s, 0, {"TECH_STEAM_POWER"});
        for (const Hex& h : {Hex{3, 8}, Hex{5, 8}}) s.plot(h).terrain = rules().terrain("TERRAIN_COAST");
    });
    const City& c = g->state().cities[0];
    const TypeIndex canal = district("DISTRICT_CANAL");
    CHECK(g->canPlaceDistrict(c, canal, {4, 8}));   // between two bodies of water
    CHECK(!g->canPlaceDistrict(c, canal, {8, 6}));  // dry land
    // Finished, it carries a ship across the land.
    GameState s = g->state();
    s.cities[0].districts.push_back({canal, {4, 8}, true});
    for (Player& p : s.players) p.techs.done[at(rules().tech("TECH_SAILING"))] = 1;
    const UnitId ship = sovtest::addUnit(s, "UNIT_GALLEY", 0, {3, 8});
    auto g2 = Game::fromScenario(rules(), std::move(s));
    auto path = g2->findPath(ship, {5, 8}, false);
    REQUIRE(path.has_value());
    CHECK(std::any_of(path->begin(), path->end(), [](const PathStep& st) { return st.pos == Hex{4, 8}; }));
    // Without it, no way across.
    GameState dry = g2->state();
    dry.cities[0].districts.pop_back();
    auto g3 = Game::fromScenario(rules(), std::move(dry));
    CHECK(!g3->findPath(ship, {5, 8}, false).has_value());
}

// ---- specialists (02: Citizens and specialists)

TEST(specialists_work_district_slots) {
    // A Campus with a Library (one slot); every plot around is desert, worth less than a specialist.
    auto g = town(3, [](GameState& s) {
        for (const Hex& h : s.grid.within(kCenter, 3)) {
            if (h != kCenter) s.plot(h).terrain = rules().terrain("TERRAIN_DESERT");
        }
        City& c = s.cities[0];
        c.districts.push_back({district("DISTRICT_CAMPUS"), {7, 6}, true});
        c.buildings.push_back(rules().building("BUILDING_LIBRARY"));
        std::sort(c.buildings.begin(), c.buildings.end());
    });
    City& c = g->stateMutForTests().cities[0];
    const CityDistrict& campus = c.districts[0];
    CHECK_EQ(g->specialistSlots(c, campus), 1);
    CHECK(g->specialistYield(c, campus)[static_cast<size_t>(YieldType::Science)] >= Fixed::fromInt(2));
    g->assignCitizens(c);
    CHECK_EQ(c.districts[0].specialists, 1);
    CHECK_EQ(static_cast<int>(c.worked.size()) + c.districts[0].specialists, c.population);
    // The specialist's Science shows in the city.
    const Fixed science = g->cityReport(c.id).yields[static_cast<size_t>(YieldType::Science)];
    c.districts[0].specialists = 0;
    CHECK(g->cityReport(c.id).yields[static_cast<size_t>(YieldType::Science)] < science);
}

TEST(city_focus_steers_its_citizens) {
    // One citizen; grassland (2 Food) all around but one wooded plains plot (1 Food, 2 Production), and four districts each
    // with one building's specialist slot (Science, Gold, Culture, Faith). Balanced, it works the woods; a Food focus takes
    // grassland, a yield focus that yield's specialist slot, a Production focus the woods again (02: Citizens).
    const Hex woods{8, 6};
    auto g = town(1, [&](GameState& s) {
        s.plot(woods).terrain = rules().terrain("TERRAIN_PLAINS");
        s.plot(woods).feature = rules().feature("FEATURE_FOREST");
        City& c = s.cities[0];
        c.districts.push_back({district("DISTRICT_CAMPUS"), {7, 6}, true});
        c.districts.push_back({district("DISTRICT_COMMERCIAL_HUB"), {5, 6}, true});
        c.districts.push_back({district("DISTRICT_THEATER_SQUARE"), {6, 8}, true});
        c.districts.push_back({district("DISTRICT_HOLY_SITE"), {6, 4}, true});
        for (const char* b : {"BUILDING_LIBRARY", "BUILDING_MARKET", "BUILDING_AMPHITHEATER", "BUILDING_SHRINE"}) c.buildings.push_back(rules().building(b));
        std::sort(c.buildings.begin(), c.buildings.end());
    });
    const CityId id = g->state().cities[0].id;
    const int32_t woodsIndex = g->state().grid.index(woods);
    // The plot its one citizen works, or -1 - the district whose specialist slot it fills.
    const auto focus = [&](CityFocus f) {
        REQUIRE(g->submit(Command::setCityFocus(0, id, f)) == CommandError::Ok);
        const City& c = *g->state().city(id);
        CHECK(c.focus == f);
        int specialists = 0, slot = 0;
        for (size_t k = 0; k < c.districts.size(); ++k) {
            specialists += c.districts[k].specialists;
            if (c.districts[k].specialists > 0) slot = -1 - static_cast<int>(k);
        }
        REQUIRE(static_cast<int>(c.worked.size()) + specialists == 1);
        return specialists > 0 ? slot : c.worked[0];
    };
    CHECK_EQ(focus(CityFocus::Balanced), woodsIndex);
    const int32_t food = focus(CityFocus::Food);
    CHECK(food >= 0 && food != woodsIndex);
    CHECK_EQ(focus(CityFocus::Science), -1);
    CHECK_EQ(focus(CityFocus::Gold), -2);
    CHECK_EQ(focus(CityFocus::Culture), -3);
    CHECK_EQ(focus(CityFocus::Faith), -4);
    CHECK_EQ(focus(CityFocus::Production), woodsIndex);
    CHECK_EQ(g->validate({CommandType::SetCityFocus, 0, id, {}, kNumCityFocuses, 0}), CommandError::BadTarget);
    CHECK_EQ(g->validate({CommandType::SetCityFocus, 0, id, {}, -1, 0}), CommandError::BadTarget);
    // Saved with the city.
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->state().city(id)->focus == CityFocus::Production);
    // A save naming no focus is refused.
    g->stateMutForTests().cities[0].focus = static_cast<CityFocus>(kNumCityFocuses);
    CHECK(!loadGame(rules(), saveGame(*g), &err));
}

TEST(an_aqueduct_by_a_geothermal_fissure_brings_an_amenity) {
    // [GS] An Aqueduct at (7,6) beside a Geothermal Fissure gives its city +1 Amenity; a fissure two plots away doesn't.
    auto g = town(3, [](GameState& s) { learn(s, 0, {"TECH_ENGINEERING"}); });
    const auto amenities = [&](Hex fissure, bool complete) {
        GameState s = g->state();
        s.plot(fissure).feature = rules().feature("FEATURE_GEOTHERMAL_FISSURE");
        s.cities[0].districts.push_back({district("DISTRICT_AQUEDUCT"), {7, 6}, complete});
        auto h = Game::fromScenario(rules(), std::move(s));
        return h->cityReport(h->state().cities[0].id).amenities;
    };
    CHECK_EQ(amenities({8, 6}, true), amenities({9, 6}, true) + 1);
    CHECK_EQ(amenities({7, 7}, true), amenities({9, 6}, true) + 1);
    CHECK_EQ(amenities({8, 6}, false), amenities({9, 6}, false));  // not until it is built
}

TEST(a_diplomatic_quarter_beside_the_city_center_brings_an_envoy) {
    // [GS] +1 Envoy once a Diplomatic Quarter is built beside the City Center at (6,6); none two plots away.
    const auto envoys = [](Hex quarter) {
        auto g = town(4, [&](GameState& s) {
            CityDistrict dq;
            dq.type = district("DISTRICT_DIPLOMATIC_QUARTER");
            dq.pos = quarter;
            s.cities[0].districts.push_back(dq);
        });
        const int before = g->state().players[0].envoyTokens;
        REQUIRE(g->completeItem(g->stateMutForTests().cities[0], item("DISTRICT_DIPLOMATIC_QUARTER")));
        REQUIRE(g->state().cities[0].district(district("DISTRICT_DIPLOMATIC_QUARTER"), true) != nullptr);
        return g->state().players[0].envoyTokens - before;
    };
    CHECK_EQ(envoys({7, 6}), 1);
    CHECK_EQ(envoys({8, 6}), 0);
}
