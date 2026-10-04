#include "helpers.h"

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
    const TerrainType& hills = r.terrains[static_cast<size_t>(r.terrain("TERRAIN_PLAINS_HILLS"))];
    CHECK_EQ(hills.moveCost, 2);
    CHECK_EQ(hills.defense, 3);
    CHECK_EQ(hills.yields[static_cast<size_t>(YieldType::Production)].toInt(), 2);
    CHECK(r.terrains[static_cast<size_t>(r.terrain("TERRAIN_GRASS_MOUNTAIN"))].impassable);
    CHECK_EQ(r.features[static_cast<size_t>(r.feature("FEATURE_FOREST"))].moveChange, 1);
    CHECK_EQ(r.civs.size(), 12u);
    CHECK_EQ(r.civs[static_cast<size_t>(r.civ("CIVILIZATION_INCA"))].leader, std::string("LEADER_PACHACUTI"));
    REQUIRE(r.resource("RESOURCE_IRON") != kNone);
    CHECK_EQ(r.resources[static_cast<size_t>(r.resource("RESOURCE_IRON"))].revealTech, std::string("TECH_BRONZE_WORKING"));
    CHECK_EQ(r.startingUnits.size(), 2u);
}

TEST(rules_mod_layers_override_by_id) {
    std::map<std::string, std::string> base = {
        {"globals.json", R"({"globals": {"CITY_MIN_RANGE": 3, "START_DISTANCE_MAJOR_CIVILIZATION": 12,
            "MOVEMENT_RIVER_COST": 2, "CITY_SIGHT_RANGE": 2, "COMBAT_MAX_HIT_POINTS": 100}})"},
        {"terrain.json", R"({"terrains": [{"id": "TERRAIN_GRASS", "base": "GRASSLAND"}], "features": []})"},
        {"units.json", R"({"units": [{"id": "UNIT_WARRIOR", "combat": 20}, {"id": "UNIT_SCOUT", "combat": 10}]})"},
        {"civilizations.json", R"({"civilizations": [{"id": "CIV_A"}]})"},
        {"setup.json", R"({"mapSizes": [{"id": "M", "width": 10, "height": 10}],
            "gameSpeeds": [{"id": "S"}], "startingUnits": []})"},
    };
    std::map<std::string, std::string> mod = {
        {"units.json", R"({"units": [{"id": "UNIT_WARRIOR", "combat": 25}, {"id": "UNIT_SCOUT", "delete": true},
                                     {"id": "UNIT_LEADER", "combat": 15}]})"},
        {"globals.json", R"({"globals": {"CITY_MIN_RANGE": 4}})"},
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
