#include "sovereign/rules.h"

#include <fstream>
#include <functional>
#include <sstream>

#include "sovereign/json.h"

namespace sov {

namespace {
constexpr const char* kYieldNames[kNumYields] = {"FOOD", "PRODUCTION", "GOLD", "SCIENCE", "CULTURE", "FAITH"};

using Table = std::vector<std::pair<std::string, Json>>;

struct Merged {
    std::map<std::string, Table> tables;
    std::map<std::string, Fixed> globals;
};

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool mergeDocument(const Json& doc, const std::string& file, Merged& m, std::string* error) {
    if (!doc.isObject()) {
        *error = file + ": top level must be an object";
        return false;
    }
    for (const auto& [name, value] : doc.members()) {
        if (name == "globals") {
            for (const auto& [key, v] : value.members()) {
                if (!v.isNumber()) {
                    *error = file + ": global " + key + " must be a number";
                    return false;
                }
                m.globals[key] = v.fixed();
            }
            continue;
        }
        if (name.size() > 0 && name[0] == '_') continue;  // "_comment" and similar
        if (!value.isArray()) {
            *error = file + ": table " + name + " must be an array";
            return false;
        }
        Table& table = m.tables[name];
        for (const Json& row : value.items()) {
            const std::string& id = row["id"].str();
            if (id.empty()) {
                *error = file + ": a row in " + name + " has no id";
                return false;
            }
            bool del = row["delete"].boolean(false);
            bool replaced = false;
            for (size_t i = 0; i < table.size(); ++i) {
                if (table[i].first == id) {
                    if (del) table.erase(table.begin() + static_cast<long>(i));
                    else table[i].second = row;
                    replaced = true;
                    break;
                }
            }
            if (!replaced && !del) table.emplace_back(id, row);
        }
    }
    return true;
}

Yields readYields(const Json& j) {
    Yields y{};
    for (const auto& [name, v] : j.members()) {
        YieldType t;
        if (parseYieldName(name, t)) y[static_cast<size_t>(t)] = v.fixed();
    }
    return y;
}

template <typename T>
TypeIndex findIn(const std::vector<T>& v, const std::string& id) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i].id == id) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

bool resolveList(const Json& list, const std::function<TypeIndex(const std::string&)>& find,
                 std::vector<TypeIndex>& out, const std::string& where, std::string* error) {
    for (const Json& item : list.items()) {
        TypeIndex t = find(item.str());
        if (t == kNone) {
            *error = where + ": unknown reference " + item.str();
            return false;
        }
        out.push_back(t);
    }
    return true;
}
}  // namespace

const char* yieldName(YieldType y) { return kYieldNames[static_cast<size_t>(y)]; }

bool parseYieldName(const std::string& s, YieldType& out) {
    for (size_t i = 0; i < kNumYields; ++i) {
        if (s == kYieldNames[i]) {
            out = static_cast<YieldType>(i);
            return true;
        }
    }
    return false;
}

uint64_t fnv1a(const void* data, size_t size, uint64_t h) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

const std::vector<std::string>& Rules::fileNames() {
    static const std::vector<std::string> names = {
        "globals.json", "terrain.json", "resources.json", "units.json", "civilizations.json", "setup.json",
    };
    return names;
}

bool Rules::load(const std::vector<std::string>& dirs, std::string* error) {
    std::vector<std::map<std::string, std::string>> layers;
    for (const std::string& dir : dirs) {
        std::map<std::string, std::string> files;
        for (const std::string& name : fileNames()) {
            std::string text;
            if (readFile(dir + "/" + name, text)) files[name] = std::move(text);
        }
        layers.push_back(std::move(files));
    }
    return loadFromText(layers, error);
}

bool Rules::loadFromText(const std::vector<std::map<std::string, std::string>>& layers, std::string* error) {
    std::string localError;
    if (!error) error = &localError;
    *this = Rules();
    Merged m;
    uint64_t sum = 0xCBF29CE484222325ull;
    for (const auto& files : layers) {
        // Load in the fixed file order so the checksum is stable.
        for (const std::string& name : fileNames()) {
            auto it = files.find(name);
            if (it == files.end()) continue;
            sum = fnv1a(name.data(), name.size(), sum);
            // Hash with line endings normalised so a Windows checkout (CRLF)
            // and a Linux one agree on the checksum.
            for (char ch : it->second) {
                if (ch != '\r') sum = fnv1a(&ch, 1, sum);
            }
            std::string perr;
            Json doc = Json::parse(it->second, &perr);
            if (!perr.empty()) {
                *error = name + ": " + perr;
                return false;
            }
            if (!mergeDocument(doc, name, m, error)) return false;
        }
    }
    checksum_ = sum;
    globals_ = m.globals;

    for (const auto& [id, j] : m.tables["terrains"]) {
        TerrainType t;
        t.id = id;
        t.name = j["name"].str(id);
        t.base = j["base"].str(id);
        const std::string& relief = j["relief"].str("FLAT");
        t.relief = relief == "HILLS" ? Relief::Hills : relief == "MOUNTAIN" ? Relief::Mountain : Relief::Flat;
        t.yields = readYields(j["yields"]);
        t.moveCost = static_cast<int>(j["moveCost"].integer(1));
        t.defense = static_cast<int>(j["defense"].integer(0));
        t.appeal = static_cast<int>(j["appeal"].integer(0));
        t.sightModifier = static_cast<int>(j["sightModifier"].integer(0));
        t.sightThrough = static_cast<int>(j["sightThrough"].integer(0));
        t.impassable = j["impassable"].boolean(false);
        t.water = j["water"].boolean(false);
        t.shallowWater = j["shallowWater"].boolean(false);
        terrains.push_back(std::move(t));
    }
    auto findTerrain = [this](const std::string& id) { return terrain(id); };
    for (const auto& [id, j] : m.tables["features"]) {
        FeatureType f;
        f.id = id;
        f.name = j["name"].str(id);
        f.yields = readYields(j["yields"]);
        f.moveChange = static_cast<int>(j["moveChange"].integer(0));
        f.defense = static_cast<int>(j["defense"].integer(0));
        f.appeal = static_cast<int>(j["appeal"].integer(0));
        f.sightThrough = static_cast<int>(j["sightThrough"].integer(0));
        f.impassable = j["impassable"].boolean(false);
        f.freshWater = j["freshWater"].boolean(false);
        f.removable = j["removable"].boolean(false);
        if (!resolveList(j["validTerrains"], findTerrain, f.validTerrains, "feature " + id, error)) return false;
        features.push_back(std::move(f));
    }
    auto findFeature = [this](const std::string& id) { return feature(id); };
    for (const auto& [id, j] : m.tables["resources"]) {
        ResourceType r;
        r.id = id;
        r.name = j["name"].str(id);
        const std::string& cls = j["class"].str("BONUS");
        r.cls = cls == "LUXURY" ? ResourceClass::Luxury : cls == "STRATEGIC" ? ResourceClass::Strategic : ResourceClass::Bonus;
        r.yields = readYields(j["yields"]);
        r.revealTech = j["revealTech"].str();
        r.frequency = static_cast<int>(j["frequency"].integer(0));
        r.seaFrequency = static_cast<int>(j["seaFrequency"].integer(0));
        if (!resolveList(j["validTerrains"], findTerrain, r.validTerrains, "resource " + id, error)) return false;
        if (!resolveList(j["validFeatures"], findFeature, r.validFeatures, "resource " + id, error)) return false;
        resources.push_back(std::move(r));
    }
    for (const auto& [id, j] : m.tables["units"]) {
        UnitType u;
        u.id = id;
        u.name = j["name"].str(id);
        u.unitClass = j["class"].str();
        const std::string& domain = j["domain"].str("LAND");
        u.domain = domain == "SEA" ? Domain::Sea : domain == "AIR" ? Domain::Air : Domain::Land;
        const std::string& layer = j["layer"].str("MILITARY");
        u.layer = layer == "CIVILIAN" ? UnitLayer::Civilian : layer == "SUPPORT" ? UnitLayer::Support : UnitLayer::Military;
        u.cost = static_cast<int>(j["cost"].integer(0));
        u.maintenance = static_cast<int>(j["maintenance"].integer(0));
        u.combat = static_cast<int>(j["combat"].integer(0));
        u.ranged = static_cast<int>(j["ranged"].integer(0));
        u.range = static_cast<int>(j["range"].integer(0));
        u.moves = static_cast<int>(j["moves"].integer(2));
        u.sight = static_cast<int>(j["sight"].integer(2));
        u.zoneOfControl = j["zoneOfControl"].boolean(false);
        u.foundCity = j["foundCity"].boolean(false);
        u.buildCharges = static_cast<int>(j["buildCharges"].integer(0));
        units.push_back(std::move(u));
    }
    for (const auto& [id, j] : m.tables["civilizations"]) {
        CivType c;
        c.id = id;
        c.name = j["name"].str(id);
        c.leader = j["leader"].str();
        for (const Json& n : j["cityNames"].items()) c.cityNames.push_back(n.str());
        civs.push_back(std::move(c));
    }
    for (const auto& [id, j] : m.tables["mapSizes"]) {
        MapSizeType s;
        s.id = id;
        s.width = static_cast<int>(j["width"].integer(0));
        s.height = static_cast<int>(j["height"].integer(0));
        s.defaultPlayers = static_cast<int>(j["defaultPlayers"].integer(2));
        if (s.width < 8 || s.height < 8) {
            *error = "map size " + id + " is too small";
            return false;
        }
        mapSizes.push_back(std::move(s));
    }
    for (const auto& [id, j] : m.tables["gameSpeeds"]) {
        GameSpeedType s;
        s.id = id;
        s.costPercent = static_cast<int>(j["costPercent"].integer(100));
        s.turns = static_cast<int>(j["turns"].integer(500));
        speeds.push_back(std::move(s));
    }
    for (const auto& [id, j] : m.tables["startingUnits"]) {
        const std::string& u = j["unit"].str();
        if (unit(u) == kNone) {
            *error = "starting unit " + id + ": unknown unit " + u;
            return false;
        }
        startingUnits.push_back(u);
    }

    static const char* required[] = {"CITY_MIN_RANGE", "START_DISTANCE_MAJOR_CIVILIZATION", "MOVEMENT_RIVER_COST",
                                     "CITY_SIGHT_RANGE", "COMBAT_MAX_HIT_POINTS"};
    for (const char* name : required) {
        if (!hasGlobal(name)) {
            *error = std::string("missing required global ") + name;
            return false;
        }
    }
    if (terrains.empty() || units.empty() || civs.empty() || mapSizes.empty() || speeds.empty()) {
        *error = "rules are missing a required table";
        return false;
    }
    return true;
}

TypeIndex Rules::terrain(const std::string& id) const { return findIn(terrains, id); }
TypeIndex Rules::feature(const std::string& id) const { return findIn(features, id); }
TypeIndex Rules::resource(const std::string& id) const { return findIn(resources, id); }
TypeIndex Rules::unit(const std::string& id) const { return findIn(units, id); }
TypeIndex Rules::civ(const std::string& id) const { return findIn(civs, id); }
TypeIndex Rules::mapSize(const std::string& id) const { return findIn(mapSizes, id); }
TypeIndex Rules::speed(const std::string& id) const { return findIn(speeds, id); }

TypeIndex Rules::terrainFor(const std::string& base, Relief relief) const {
    for (size_t i = 0; i < terrains.size(); ++i) {
        if (terrains[i].base == base && terrains[i].relief == relief) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

Fixed Rules::global(const std::string& name) const {
    auto it = globals_.find(name);
    return it == globals_.end() ? Fixed() : it->second;
}

}  // namespace sov
