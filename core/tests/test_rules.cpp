#include <algorithm>
#include <fstream>
#include <sstream>

#include "helpers.h"
#include "sovereign/json.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::rules;

TEST(rules_load_known_values) {
    const Rules& r = rules();
    // Data tests (10-ai-ui-implementation.md, Testing strategy).
    REQUIRE(r.speed("GAMESPEED_STANDARD") != kNone);
    CHECK_EQ(r.speeds[static_cast<size_t>(r.speed("GAMESPEED_STANDARD"))].turns, 500);
    CHECK_EQ(r.speeds[static_cast<size_t>(r.speed("GAMESPEED_MARATHON"))].costPercent, 300);
    const UnitType& settler = r.units[static_cast<size_t>(r.unit("UNIT_SETTLER"))];
    CHECK(settler.foundCity);
    CHECK_EQ(settler.sight, 3);
    CHECK(settler.layer == UnitLayer::Civilian);
    const UnitType& warrior = r.units[static_cast<size_t>(r.unit("UNIT_WARRIOR"))];
    CHECK_EQ(warrior.combat, 20);
    CHECK_EQ(warrior.moves, 2);
    CHECK(warrior.zoneOfControl);
    CHECK_EQ(r.units[static_cast<size_t>(r.unit("UNIT_SCOUT"))].moves, 3);
    CHECK(!r.units[static_cast<size_t>(r.unit("UNIT_SLINGER"))].zoneOfControl);
    CHECK_EQ(r.globalInt("CITY_MIN_RANGE"), 3);
    CHECK_EQ(r.globalInt("MOVEMENT_RIVER_COST"), 2);
    CHECK_EQ(r.globalInt("START_DISTANCE_MAJOR_CIVILIZATION"), 12);
    CHECK(r.hasGlobal("CITY_MIN_RANGE"));
    CHECK(!r.hasGlobal("NO_SUCH_GLOBAL"));  // a name the rules lack: absent, and zero
    CHECK_EQ(r.globalInt("NO_SUCH_GLOBAL"), 0);
    const TerrainType& hills = r.terrains[static_cast<size_t>(r.terrain("TERRAIN_PLAINS_HILLS"))];
    CHECK_EQ(hills.moveCost, 2);
    CHECK_EQ(hills.defense, 3);
    CHECK_EQ(hills.yields[static_cast<size_t>(YieldType::Production)].toInt(), 2);
    CHECK(r.terrains[static_cast<size_t>(r.terrain("TERRAIN_GRASS_MOUNTAIN"))].impassable);
    CHECK_EQ(r.features[static_cast<size_t>(r.feature("FEATURE_FOREST"))].moveChange, 1);
    CHECK_EQ(r.civs.size(), 12u);
    CHECK_EQ(r.civs[static_cast<size_t>(r.civ("CIVILIZATION_INCA"))].leader, std::string("LEADER_PACHACUTI"));
    REQUIRE(r.resource("RESOURCE_IRON") != kNone);
    const Unlock ironReveal = r.resources[static_cast<size_t>(r.resource("RESOURCE_IRON"))].reveal;
    CHECK(!ironReveal.civic);
    CHECK_EQ(ironReveal.index, r.tech("TECH_BRONZE_WORKING"));
    CHECK_EQ(r.startingUnits.size(), 2u);
}

TEST(rules_index_every_modifier_by_effect) {
    // Each modifier is listed once, under its own effect and collection, in load order; indexing again rebuilds the lists.
    auto misplaced = [](const Rules& r) {
        int wrong = 0;
        std::vector<int> listed(r.modifiers.size(), 0);
        for (int e = 0; e < 256; ++e) {
            const ModEffect effect = static_cast<ModEffect>(e);
            for (bool player : {true, false}) {
                const std::vector<uint32_t>& list = player ? r.playerModifiers(effect) : r.cityModifiers(effect);
                for (size_t k = 0; k < list.size(); ++k) {
                    if (list[k] >= r.modifiers.size() || (k > 0 && list[k - 1] >= list[k])) {
                        ++wrong;
                        continue;
                    }
                    const Modifier& m = r.modifiers[list[k]];
                    if (m.effect != effect || player != (m.collection == ModCollection::Player)) ++wrong;
                    ++listed[list[k]];
                }
            }
        }
        for (int n : listed) wrong += n == 1 ? 0 : 1;
        return wrong;
    };
    REQUIRE(!rules().modifiers.empty());
    CHECK_EQ(misplaced(rules()), 0);
    Rules again = rules();
    again.indexModifiers();
    CHECK_EQ(misplaced(again), 0);
}

// The city modifiers no policy brings, by effect, and each policy's own: between them each city modifier once, in
// load order, a policy's under the policy that brings it.
TEST(rules_index_city_modifiers_by_policy) {
    auto misplaced = [](const Rules& r) {
        int wrong = 0;
        std::vector<int> listed(r.modifiers.size(), 0);
        const auto ofPolicy = [&](const Modifier& m) {
            return m.sourceKind == ModSource::Policy && m.sourceIndex >= 0 && static_cast<size_t>(m.sourceIndex) < r.policies.size();
        };
        const auto inOrder = [&](const std::vector<uint32_t>& list) {
            for (size_t k = 1; k < list.size(); ++k) wrong += list[k - 1] < list[k] ? 0 : 1;
        };
        for (int e = 0; e < 256; ++e) {
            const ModEffect effect = static_cast<ModEffect>(e);
            const std::vector<uint32_t>& list = r.cityModifiersBesidePolicies(effect);
            inOrder(list);
            for (uint32_t i : list) {
                const Modifier& m = r.modifiers[i];
                wrong += m.effect == effect && m.collection != ModCollection::Player && !ofPolicy(m) ? 0 : 1;
                ++listed[i];
            }
        }
        for (size_t p = 0; p < r.policies.size(); ++p) {
            const std::vector<uint32_t>* list = r.policyCityModifiers(static_cast<TypeIndex>(p));
            if (!list) {
                ++wrong;
                continue;
            }
            inOrder(*list);
            for (uint32_t i : *list) {
                const Modifier& m = r.modifiers[i];
                wrong += m.collection != ModCollection::Player && ofPolicy(m) && m.sourceIndex == static_cast<TypeIndex>(p) ? 0 : 1;
                ++listed[i];
            }
        }
        for (size_t i = 0; i < r.modifiers.size(); ++i) wrong += listed[i] == (r.modifiers[i].collection == ModCollection::Player ? 0 : 1) ? 0 : 1;
        return wrong;
    };
    const Rules& r = rules();
    const TypeIndex feudal = r.policy("POLICY_FEUDAL_CONTRACT");
    REQUIRE(feudal != kNone);
    CHECK_EQ(misplaced(r), 0);
    REQUIRE(r.policyCityModifiers(feudal) != nullptr);
    CHECK(!r.policyCityModifiers(feudal)->empty());
    CHECK(r.policyCityModifiers(kNone) == nullptr);
    CHECK(r.policyCityModifiers(static_cast<TypeIndex>(r.policies.size())) == nullptr);
    // A policy's modifier added later is listed under it once the modifiers are indexed again.
    Rules again = r;
    Modifier m;
    m.collection = ModCollection::PlayerCities;
    m.effect = ModEffect::CityYield;
    m.sourceKind = ModSource::Policy;
    m.sourceIndex = feudal;
    again.modifiers.push_back(m);
    again.indexModifiers();
    CHECK_EQ(misplaced(again), 0);
    const std::vector<uint32_t>* listed = again.policyCityModifiers(feudal);
    REQUIRE(listed && !listed->empty());
    CHECK_EQ(listed->back(), static_cast<uint32_t>(again.modifiers.size() - 1));
}

// A pass over a city's modifiers looks once for the city holding modifiers of one source in a row; a modifier of
// another collection, kind of source or source looks again.
TEST(modifiers_of_each_source_find_their_own_holder) {
    const Rules& r = rules();
    const TypeIndex granary = r.building("BUILDING_GRANARY"), mill = r.building("BUILDING_WATER_MILL");
    Rules more = r;
    const auto add = [&](ModSource kind, TypeIndex source, ModCollection collection, int gold) {
        Modifier m;
        m.sourceKind = kind;
        m.sourceIndex = source;
        m.collection = collection;
        m.effect = ModEffect::CityYield;
        m.yield = YieldType::Gold;
        m.amount = Fixed::fromInt(gold);
        more.modifiers.push_back(m);
    };
    add(ModSource::Building, granary, ModCollection::OwnerCity, 1);        // the Granary's own city
    add(ModSource::Building, granary, ModCollection::PlayerCities, 10);    // each city of a civ with a Granary
    add(ModSource::Building, mill, ModCollection::PlayerCities, 100);      // ... with a Water Mill, which none has
    add(ModSource::GreatPerson, mill, ModCollection::PlayerCities, 1000);  // a great person of that index, used by the civ
    more.indexModifiers();
    GameState s = sovtest::flatState(16, 10, 1);
    sovtest::addCity(s, 0, {3, 4}, true);
    sovtest::addCity(s, 0, {10, 4}, false);
    s.cities[0].buildings.push_back(granary);
    std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    s.players[0].greatPeopleActivated.push_back(mill);
    const auto gold = [&](const Rules& with, const City& c) {
        return sumCityModifiersByYield(s, with, c, ModEffect::CityYield)[static_cast<size_t>(YieldType::Gold)];
    };
    CHECK_EQ(gold(more, s.cities[0]) - gold(r, s.cities[0]), Fixed::fromInt(1011));
    CHECK_EQ(gold(more, s.cities[1]) - gold(r, s.cities[1]), Fixed::fromInt(1010));
}

// Every named constant in the data is found by its name, with its value.
TEST(rules_find_every_named_constant) {
    std::ifstream in(std::string(SOVEREIGN_RULES_DIR) + "/globals.json", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string err;
    const Json j = Json::parse(ss.str(), &err);
    REQUIRE(err.empty());
    const Rules& r = rules();
    int missing = 0, wrong = 0;
    for (const auto& [name, value] : j["globals"].members()) {
        if (!r.hasGlobal(name)) ++missing;
        else if (r.global(name) != value.fixed()) ++wrong;
    }
    CHECK(j["globals"].size() > 400u);
    CHECK_EQ(missing, 0);
    CHECK_EQ(wrong, 0);
}

// Each constant read on a hot path is the one of its name, and that name is in the data.
TEST(rules_read_hot_constants_as_their_names) {
    const Rules& r = rules();
    for (size_t i = 0; i < static_cast<size_t>(HotGlobal::Count); ++i) {
        const HotGlobal g = static_cast<HotGlobal>(i);
        CHECK(r.hasGlobal(Rules::hotGlobalName(g)));
        CHECK(r.global(g) == r.global(Rules::hotGlobalName(g)));
    }
    CHECK_EQ(r.globalInt(HotGlobal::CityMinRange), 3);
    CHECK_EQ(r.globalInt(HotGlobal::MovementRiverCost), 2);
}

// Each plot yield modifier is listed once in the plot index, and one listed under a plot property needs it.
TEST(rules_index_plot_yield_modifiers_by_what_they_need) {
    auto misplaced = [](const Rules& r) {
        const Rules::PlotModifiers& mods = r.plotYieldModifiers();
        int wrong = 0;
        std::vector<int> listed(r.modifiers.size(), 0);
        for (uint32_t i : mods.unkeyed) ++listed[i];
        auto check = [&](const std::vector<std::vector<uint32_t>>& by, ReqType type) {
            for (size_t k = 0; k < by.size(); ++k) {
                for (uint32_t i : by[k]) {
                    ++listed[i];
                    const RequirementSet& reqs = r.modifiers[i].subjectReqs;
                    const bool needs = !reqs.any && std::any_of(reqs.reqs.begin(), reqs.reqs.end(), [&](const Requirement& q) {
                        return q.type == type && !q.negate && q.ref == static_cast<TypeIndex>(k);
                    });
                    wrong += needs ? 0 : 1;
                }
            }
        };
        check(mods.byImprovement, ReqType::PlotHasImprovement);
        check(mods.byResource, ReqType::PlotHasResource);
        check(mods.byFeature, ReqType::PlotHasFeature);
        check(mods.byTerrain, ReqType::PlotHasTerrain);
        for (size_t i = 0; i < r.modifiers.size(); ++i) {
            const bool plotYield = r.modifiers[i].effect == ModEffect::PlotYield && r.modifiers[i].collection != ModCollection::Player;
            wrong += listed[i] == (plotYield ? 1 : 0) ? 0 : 1;
        }
        return wrong;
    };
    const Rules& r = rules();
    CHECK_EQ(misplaced(r), 0);
    CHECK(r.plotYieldModifiers().unkeyed.size() < r.cityModifiers(ModEffect::PlotYield).size() / 4);  // most are keyed
    // An any-of set, or a requirement that the plot lacks something, names nothing the plot must have: both
    // stay unkeyed.
    Rules again = r;
    for (const bool anyOf : {true, false}) {
        Modifier m;
        m.collection = ModCollection::OwnerCityPlots;
        m.effect = ModEffect::PlotYield;
        m.subjectReqs.any = anyOf;
        m.subjectReqs.reqs = {{ReqType::PlotHasFeature, 0, 0, !anyOf}, {ReqType::PlotHasTerrain, 0, 0, !anyOf}};
        again.modifiers.push_back(m);
    }
    again.indexModifiers();
    CHECK_EQ(misplaced(again), 0);
    CHECK_EQ(again.plotYieldModifiers().unkeyed.size(), r.plotYieldModifiers().unkeyed.size() + 2);
}

namespace {
// The smallest set of rules files that loads.
std::map<std::string, std::string> minimalRules() {
    return {
        {"globals.json", R"({"globals": {"CITY_MIN_RANGE": 3, "START_DISTANCE_MAJOR_CIVILIZATION": 12,
            "MOVEMENT_RIVER_COST": 2, "CITY_SIGHT_RANGE": 2, "COMBAT_MAX_HIT_POINTS": 100,
            "CITY_FOOD_CONSUMPTION_PER_POPULATION": 2, "CITY_GROWTH_THRESHOLD": 15, "CITY_GROWTH_MULTIPLIER": 8,
            "CITY_GROWTH_EXPONENT": 1.5, "CULTURE_COST_FIRST_PLOT": 10, "CULTURE_COST_LATER_PLOT_MULTIPLIER": 6,
            "CULTURE_COST_LATER_PLOT_EXPONENT": 1.3, "CITY_POP_PER_AMENITY": 2, "PLOT_BUY_BASE_COST": 50,
            "GOLD_PURCHASE_MULTIPLIER": 2}})"},
        {"terrain.json", R"({"terrains": [{"id": "TERRAIN_GRASS", "base": "GRASSLAND"}], "features": []})"},
        {"units.json", R"({"units": [{"id": "UNIT_WARRIOR", "combat": 20}, {"id": "UNIT_SCOUT", "combat": 10}]})"},
        {"civilizations.json", R"({"civilizations": [{"id": "CIV_A"}]})"},
        {"techs.json", R"({"eras": [{"id": "ERA_A"}], "techs": [{"id": "TECH_A", "era": "ERA_A", "cost": 10}]})"},
        {"civics.json", R"({"civics": [{"id": "CIVIC_A", "era": "ERA_A", "cost": 10}]})"},
        {"governments.json", R"({"governments": [{"id": "GOVERNMENT_A", "slots": {"WILDCARD": 1}}]})"},
        {"districts.json", R"({"districts": [{"id": "DISTRICT_CITY_CENTER", "hp": 200}]})"},
        {"setup.json", R"({"mapSizes": [{"id": "M", "width": 10, "height": 10}],
            "gameSpeeds": [{"id": "S"}], "startingUnits": []})"},
    };
}
}  // namespace

TEST(rules_mod_layers_override_by_id) {
    const std::map<std::string, std::string> base = minimalRules();
    std::map<std::string, std::string> mod = {
        {"units.json", R"({"units": [{"id": "UNIT_WARRIOR", "combat": 25}, {"id": "UNIT_SCOUT", "delete": true},
                                     {"id": "UNIT_LEADER", "combat": 15}]})"},
        {"globals.json", R"({"globals": {"CITY_MIN_RANGE": 4, "A": 1, "SEVEN_7": 7, "EIGHT_88": 8, "NINE_9999": 9}})"},
    };
    Rules plain, modded;
    std::string err;
    REQUIRE(plain.loadFromText({base}, &err));
    REQUIRE(modded.loadFromText({base, mod}, &err));
    CHECK_EQ(modded.units.size(), 2u);
    CHECK_EQ(modded.units[static_cast<size_t>(modded.unit("UNIT_WARRIOR"))].combat, 25);
    CHECK(modded.unit("UNIT_SCOUT") == kNone);
    CHECK(modded.unit("UNIT_LEADER") != kNone);
    CHECK_EQ(modded.globalInt("CITY_MIN_RANGE"), 4);
    CHECK_EQ(modded.globalInt(HotGlobal::CityMinRange), 4);
    CHECK_EQ(plain.globalInt(HotGlobal::CityMinRange), 3);
    // Names shorter than, as long as and longer than eight bytes.
    CHECK_EQ(modded.globalInt("A"), 1);
    CHECK_EQ(modded.globalInt("SEVEN_7"), 7);
    CHECK_EQ(modded.globalInt("EIGHT_88"), 8);
    CHECK_EQ(modded.globalInt("NINE_9999"), 9);
    CHECK(!modded.hasGlobal("NINE_999") && !modded.hasGlobal("") && !plain.hasGlobal("A"));
    CHECK(plain.checksum() != modded.checksum());
}

TEST(rules_reject_bad_data) {
    Rules r;
    std::string err;
    CHECK(!r.loadFromText({{{"units.json", "{not json"}}}, &err));
    CHECK(!err.empty());
    CHECK(!r.loadFromText({{{"terrain.json", R"({"terrains": [{"name": "no id"}]})"}}}, &err));
    CHECK(!r.loadFromText({{{"resources.json", R"({"resources": [{"id": "R", "validTerrains": ["NOPE"]}]})"}}}, &err));
    // Missing required globals.
    CHECK(!r.loadFromText({{{"terrain.json", R"({"terrains": [{"id": "T"}]})"}}}, &err));
}

// A governor's promotion is found by its id alone (Game::governorHasPromotion), so two may not share one.
TEST(rules_reject_a_governor_promotion_listed_twice) {
    const auto withPromotions = [](const std::string& second) {
        std::map<std::string, std::string> docs = minimalRules();
        docs["governors.json"] = R"({"governors": [{"id": "GOVERNOR_A", "promotions": [{"id": "PROMOTION_A", "base": true}]},
            {"id": "GOVERNOR_B", "promotions": [{"id": ")" + second + R"(", "base": true}]}]})";
        return docs;
    };
    Rules apart, twice;
    std::string err;
    REQUIRE(apart.loadFromText({withPromotions("PROMOTION_B")}, &err));
    CHECK_EQ(apart.governorPromotions.size(), 2u);
    CHECK(!twice.loadFromText({withPromotions("PROMOTION_A")}, &err));
    CHECK(err.find("PROMOTION_A") != std::string::npos);
}

TEST(rules_checksum_ignores_line_endings) {
    std::map<std::string, std::string> lf, crlf;
    for (const std::string& name : Rules::fileNames()) {
        std::ifstream in(std::string(SOVEREIGN_RULES_DIR) + "/" + name, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        lf[name] = ss.str();
        std::string withCr;
        for (char ch : lf[name]) {
            if (ch == '\n') withCr += '\r';
            withCr += ch;
        }
        crlf[name] = withCr;
    }
    Rules a, b;
    std::string err;
    REQUIRE(a.loadFromText({lf}, &err));
    REQUIRE(b.loadFromText({crlf}, &err));
    CHECK_EQ(a.checksum(), b.checksum());
}

TEST(a_mod_patches_rows_over_the_rules) {
    // The example mod lays a Settler cost over the rules; the rest of the row stays, and the checksum notices.
    Rules modded;
    std::string err;
    REQUIRE(modded.load({SOVEREIGN_RULES_DIR, std::string(SOVEREIGN_RULES_DIR) + "/../../mods/swift-settlers"}, &err));
    const UnitType& settler = modded.units[static_cast<size_t>(modded.unit("UNIT_SETTLER"))];
    CHECK_EQ(settler.cost, 60);
    CHECK(settler.foundCity);
    CHECK_EQ(settler.name, std::string("Settler"));
    CHECK(modded.checksum() != rules().checksum());
    // A save names its mods, and they can be read before the rules are loaded.
    GameSetup setup = sovtest::duelSetup(4);
    setup.mods = {"swift-settlers"};
    auto g = Game::create(modded, setup, &err);
    REQUIRE(g);
    const std::vector<uint8_t> bytes = saveGame(*g);
    GameSetup peeked;
    REQUIRE(peekSaveSetup(bytes, peeked));
    CHECK(peeked.mods == setup.mods);
    CHECK(loadGame(modded, bytes, &err) != nullptr);
    CHECK(loadGame(rules(), bytes, &err) == nullptr);  // without the mod the rules differ
    CHECK(!peekSaveSetup({1, 2, 3}, peeked));
}