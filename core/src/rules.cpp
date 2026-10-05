#include "sovereign/rules.h"

#include <algorithm>
#include <climits>
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
bool parseRequirements(const Json& j, RequirementSet& set, const Rules& rules, std::string* error) {
    if (j.isNull()) return true;
    set.any = j.has("any");
    const Json& list = set.any ? j["any"] : j["all"];
    for (const Json& r : list.items()) {
        Requirement q;
        const std::string& type = r["type"].str();
        q.negate = r["negate"].boolean(false);
        q.value = static_cast<int>(r["value"].integer(0));
        const std::string& ref = r["ref"].str();
        if (type == "PLOT_HAS_RESOURCE") { q.type = ReqType::PlotHasResource; q.ref = rules.resource(ref); }
        else if (type == "PLOT_HAS_FEATURE") { q.type = ReqType::PlotHasFeature; q.ref = rules.feature(ref); }
        else if (type == "PLOT_HAS_TERRAIN") { q.type = ReqType::PlotHasTerrain; q.ref = rules.terrain(ref); }
        else if (type == "PLOT_HAS_IMPROVEMENT") { q.type = ReqType::PlotHasImprovement; q.ref = ref.empty() ? kNone : rules.improvement(ref); }
        else if (type == "CITY_HAS_BUILDING") { q.type = ReqType::CityHasBuilding; q.ref = rules.building(ref); }
        else if (type == "CITY_IS_CAPITAL") { q.type = ReqType::CityIsCapital; }
        else if (type == "CITY_MIN_POPULATION") { q.type = ReqType::CityMinPopulation; }
        else if (type == "PLAYER_IS_HUMAN") { q.type = ReqType::PlayerIsHuman; }
        else {
            *error = "unknown requirement type " + type;
            return false;
        }
        bool needsRef = q.type == ReqType::PlotHasResource || q.type == ReqType::PlotHasFeature ||
                        q.type == ReqType::PlotHasTerrain || q.type == ReqType::CityHasBuilding;
        if (needsRef && q.ref == kNone) {
            *error = "requirement " + type + " refers to unknown " + ref;
            return false;
        }
        set.reqs.push_back(q);
    }
    return true;
}

bool parseModifier(const Json& j, Modifier& mod, const Rules& rules, std::string* error) {
    static const std::pair<const char*, ModCollection> collections[] = {
        {"OWNER_CITY", ModCollection::OwnerCity},         {"OWNER_CITY_PLOTS", ModCollection::OwnerCityPlots},
        {"PLAYER_CITIES", ModCollection::PlayerCities},   {"PLAYER_CAPITAL", ModCollection::PlayerCapital},
        {"PLAYER_CITY_PLOTS", ModCollection::PlayerCityPlots}, {"PLAYER", ModCollection::Player},
    };
    static const std::pair<const char*, ModEffect> effects[] = {
        {"ADJUST_CITY_YIELD", ModEffect::CityYield},           {"ADJUST_CITY_YIELD_PERCENT", ModEffect::CityYieldPercent},
        {"ADJUST_PLOT_YIELD", ModEffect::PlotYield},           {"ADJUST_CITY_HOUSING", ModEffect::CityHousing},
        {"ADJUST_CITY_AMENITIES", ModEffect::CityAmenities},   {"ADJUST_CITY_GROWTH_PERCENT", ModEffect::CityGrowthPercent},
        {"ADJUST_CITY_DEFENSE", ModEffect::CityDefense},
        {"ADJUST_UNIT_PRODUCTION_PERCENT", ModEffect::UnitProductionPercent},
        {"ADJUST_PLOT_PURCHASE_COST_PERCENT", ModEffect::PlotPurchaseCostPercent},
        {"ADJUST_UNIT_MAINTENANCE_DISCOUNT", ModEffect::UnitMaintenanceDiscount},
        {"GRANT_ABILITY", ModEffect::GrantAbility},
        {"ADJUST_UNIT_XP_PERCENT", ModEffect::UnitXpPercent},
        {"ADJUST_UNIT_STRENGTH", ModEffect::UnitStrength},
        {"ADJUST_DISTRICT_ADJACENCY_PERCENT", ModEffect::DistrictAdjacencyPercent},
        {"ADJUST_CITY_LOYALTY", ModEffect::CityLoyalty},
        {"FOUNDER_YIELD_PER_CITY", ModEffect::FounderYieldPerCity},
        {"FOUNDER_YIELD_PER_FOLLOWERS", ModEffect::FounderYieldPerFollowers},
        {"FOUNDER_YIELD_PER_DISTRICT", ModEffect::FounderYieldPerDistrict},
        {"ADJUST_RELIGION_PRESSURE_RANGE", ModEffect::ReligionPressureRange},
        {"ADJUST_RELIGION_PRESSURE_PERCENT", ModEffect::ReligionPressurePercent},
        {"ADJUST_RELIGIOUS_UNIT_DISCOUNT_PERCENT", ModEffect::ReligiousUnitDiscountPercent},
        {"ADJUST_STRENGTH_NEAR_FOLLOWING_CITY", ModEffect::UnitStrengthNearFollowingCity},
        {"RELIGIOUS_UNITS_IGNORE_TERRAIN", ModEffect::ReligiousUnitsIgnoreTerrain},
        {"NO_COMBAT_PRESSURE_LOSS", ModEffect::NoCombatPressureLoss},
        {"RELIGION_COLONIZES", ModEffect::ReligionColonizes},
    };
    const std::string& c = j["collection"].str();
    const std::string& e = j["effect"].str();
    bool foundC = false, foundE = false;
    for (const auto& [name, v] : collections) if (c == name) { mod.collection = v; foundC = true; }
    for (const auto& [name, v] : effects) if (e == name) { mod.effect = v; foundE = true; }
    if (!foundC) { *error = "unknown collection " + c; return false; }
    if (!foundE) { *error = "unknown effect " + e; return false; }
    if (mod.source.empty()) { *error = "no source"; return false; }
    const Json& args = j["arguments"];
    if (args.has("yield") && !parseYieldName(args["yield"].str(), mod.yield)) {
        *error = "unknown yield " + args["yield"].str();
        return false;
    }
    mod.amount = args["amount"].fixed();
    mod.unitClass = args["unitClass"].str();
    if (args.has("unit") && (mod.unit = rules.unit(args["unit"].str())) == kNone) {
        *error = "unknown unit " + args["unit"].str();
        return false;
    }
    if (args.has("maxEra") && (mod.maxEra = rules.era(args["maxEra"].str())) == kNone) {
        *error = "unknown era " + args["maxEra"].str();
        return false;
    }
    if (args.has("ability") && (mod.ability = rules.ability(args["ability"].str())) == kNone) {
        *error = "unknown ability " + args["ability"].str();
        return false;
    }
    if (args.has("district") && (mod.district = rules.district(args["district"].str())) == kNone) {
        *error = "unknown district " + args["district"].str();
        return false;
    }
    if (mod.effect == ModEffect::DistrictAdjacencyPercent && mod.district == kNone) {
        *error = "ADJUST_DISTRICT_ADJACENCY_PERCENT needs a district";
        return false;
    }
    if (mod.effect == ModEffect::GrantAbility && mod.ability == kNone) {
        *error = "GRANT_ABILITY needs an ability";
        return false;
    }
    // Player-wide effects and the player collection go together.
    const bool playerEffect = mod.effect == ModEffect::UnitMaintenanceDiscount ||
                              mod.effect == ModEffect::GrantAbility || mod.effect == ModEffect::UnitXpPercent ||
                              mod.effect == ModEffect::UnitStrength || mod.effect == ModEffect::DistrictAdjacencyPercent ||
                              mod.effect >= ModEffect::FounderYieldPerCity;
    mod.vsBarbarians = args["vsBarbarians"].boolean(false);
    mod.per = std::max(1, static_cast<int>(args["per"].integer(1)));
    mod.foreign = args["foreign"].boolean(false);
    if (mod.effect == ModEffect::FounderYieldPerDistrict && mod.district == kNone) {
        *error = "FOUNDER_YIELD_PER_DISTRICT needs a district";
        return false;
    }
    if (playerEffect != (mod.collection == ModCollection::Player)) {
        *error = "effect " + e + " does not fit collection " + c;
        return false;
    }
    return parseRequirements(j["ownerRequirements"], mod.ownerReqs, rules, error) &&
           parseRequirements(j["subjectRequirements"], mod.subjectReqs, rules, error);
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
        "globals.json",     "terrain.json",  "resources.json",     "promotions.json", "units.json",
        "buildings.json",   "districts.json", "barbarians.json", "techs.json",    "civics.json",        "governments.json",
        "policies.json",    "improvements.json", "greatpeople.json", "religion.json", "wonders.json", "citystates.json", "civilizations.json", "leader.json", "setup.json", "modifiers.json",
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
    // World wonders load as buildings (flagged by their placement), so their yields, slots and
    // points use the building paths.
    for (const auto& row : m.tables["wonders"]) m.tables["buildings"].push_back(row);
    globals_ = m.globals;

    // Eras and research trees first: everything else may be unlocked by them.
    for (const auto& [id, j] : m.tables["eras"]) {
        EraType e;
        e.id = id;
        e.name = j["name"].str(id);
        e.embarkedStrength = static_cast<int>(j["embarkedStrength"].integer(10));
        e.greatPersonBaseCost = static_cast<int>(j["greatPersonBaseCost"].integer(0));
        e.tradeRouteExtraTurns = static_cast<int>(j["tradeRouteExtraTurns"].integer(0));
        eras.push_back(std::move(e));
    }
    auto readNodes = [&](const char* name, std::vector<TreeNode>& out) {
        for (const auto& [id, j] : m.tables[name]) {
            TreeNode n;
            n.id = id;
            n.name = j["name"].str(id);
            n.era = era(j["era"].str());
            n.cost = static_cast<int>(j["cost"].integer(0));
            n.embarkedMoves = static_cast<int>(j["embarkedMoves"].integer(0));
            n.envoys = static_cast<int>(j["envoys"].integer(0));
            for (const Json& e : j["effects"].items()) {
                if (e.str() == "COMBAT_ADJACENCY") n.combatAdjacency = true;
                if (e.str() == "ENFORCE_BORDERS") n.enforceBorders = true;
                if (e.str() == "EMBARK_ALL") n.embarkAll = true;
                if (e.str() == "OCEAN") n.ocean = true;
                if (e.str() == "TRADE_ROUTE_CAPACITY") n.tradeCapacity = true;
            }
            if (n.era == kNone || n.cost <= 0) {
                *error = std::string(name) + " " + id + ": bad era or cost";
                return false;
            }
            out.push_back(std::move(n));
        }
        return true;
    };
    if (!readNodes("techs", techs) || !readNodes("civics", civics)) return false;
    auto findTech = [this](const std::string& id) { return tech(id); };
    auto findCivic = [this](const std::string& id) { return civic(id); };
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["techs"]) {
            if (!resolveList(j["prereqs"], findTech, techs[i++].prereqs, "tech " + id, error)) return false;
        }
        i = 0;
        for (const auto& [id, j] : m.tables["civics"]) {
            if (!resolveList(j["prereqs"], findCivic, civics[i++].prereqs, "civic " + id, error)) return false;
        }
    }
    auto readUnlock = [&](const Json& j, Unlock& u, const std::string& where) {
        const std::string& id = j.str();
        u = Unlock{};
        if (id.empty()) return true;
        if ((u.index = tech(id)) != kNone) return true;
        if ((u.index = civic(id)) != kNone) {
            u.civic = true;
            return true;
        }
        *error = where + ": unknown tech or civic " + id;
        return false;
    };

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
        if (!readUnlock(j["removeTech"], f.removeTech, "feature " + id)) return false;
        f.harvest = readYields(j["harvestYields"]);
        if (!resolveList(j["validTerrains"], findTerrain, f.validTerrains, "feature " + id, error)) return false;
        features.push_back(std::move(f));
    }
    for (const auto& [id, j] : m.tables["routes"]) {
        RouteType rt;
        rt.id = id;
        rt.name = j["name"].str(id);
        rt.moveCost = j["moveCost"].fixed();
        rt.bridges = j["bridges"].boolean(false);
        rt.era = j.has("era") ? era(j["era"].str()) : 0;
        if (rt.era == kNone || rt.moveCost <= Fixed()) {
            *error = "route " + id + ": bad era or cost";
            return false;
        }
        routes.push_back(std::move(rt));
    }
    auto findFeature = [this](const std::string& id) { return feature(id); };
    for (const auto& [id, j] : m.tables["resources"]) {
        ResourceType r;
        r.id = id;
        r.name = j["name"].str(id);
        const std::string& cls = j["class"].str("BONUS");
        r.cls = cls == "LUXURY" ? ResourceClass::Luxury : cls == "STRATEGIC" ? ResourceClass::Strategic : ResourceClass::Bonus;
        r.yields = readYields(j["yields"]);
        if (!readUnlock(j["revealTech"], r.reveal, "resource " + id)) return false;
        r.frequency = static_cast<int>(j["frequency"].integer(0));
        r.seaFrequency = static_cast<int>(j["seaFrequency"].integer(0));
        if (!resolveList(j["validTerrains"], findTerrain, r.validTerrains, "resource " + id, error)) return false;
        if (!resolveList(j["validFeatures"], findFeature, r.validFeatures, "resource " + id, error)) return false;
        r.amenityCities = static_cast<int>(j["amenityCities"].integer(0));
        r.harvest = readYields(j["harvestYields"]);
        if (!readUnlock(j["harvestTech"], r.harvestTech, "resource " + id)) return false;
        r.accumulation = static_cast<int>(j["accumulation"].integer(0));
        r.stockpileCap = static_cast<int>(j["stockpileCap"].integer(0));
        resources.push_back(std::move(r));
    }
    // Promotions and abilities: effects with data-driven combat conditions.
    auto readEffects = [&](const Json& list, std::vector<UnitEffect>& out, const std::string& where) {
        static const std::pair<const char*, UnitEffectKind> kinds[] = {
            {"UNTRACKED", UnitEffectKind::Untracked},           {"STRENGTH", UnitEffectKind::Strength},
            {"MOVES", UnitEffectKind::Moves},                   {"RANGE", UnitEffectKind::Range},
            {"SIGHT", UnitEffectKind::Sight},                   {"ATTACKS", UnitEffectKind::Attacks},
            {"XP_PERCENT", UnitEffectKind::XpPercent},          {"FLANKING_PERCENT", UnitEffectKind::FlankingPercent},
            {"SUPPORT_PERCENT", UnitEffectKind::SupportPercent}, {"MOVE_AFTER_ATTACK", UnitEffectKind::MoveAfterAttack},
            {"ATTACK_AFTER_MOVE", UnitEffectKind::AttackAfterMove},
            {"NO_ATTACK_AFTER_MOVE", UnitEffectKind::NoAttackAfterMove},
            {"IGNORE_ZOC", UnitEffectKind::IgnoreZoc},          {"EXERT_ZOC", UnitEffectKind::ExertZoc},
            {"NO_RIVER_PENALTY", UnitEffectKind::NoRiverPenalty},
            {"NO_WOUNDED_PENALTY", UnitEffectKind::NoWoundedPenalty},
            {"HEAL_AFTER_ACTION", UnitEffectKind::HealAfterAction}, {"IGNORE_BORDERS", UnitEffectKind::IgnoreBorders},
            {"RANGED_VS_DISTRICT", UnitEffectKind::RangedVsDistrict},
            {"BOMBARD_VS_UNIT", UnitEffectKind::BombardVsUnit},
            {"WALL_FULL_DAMAGE", UnitEffectKind::WallFullDamage}, {"BYPASS_WALLS", UnitEffectKind::BypassWalls},
            {"AURA_STRENGTH", UnitEffectKind::AuraStrength},     {"ASSASSIN_DEFENSE", UnitEffectKind::AssassinDefense},
            {"CITY_PRODUCTION", UnitEffectKind::CityProduction}, {"CITY_AMENITIES", UnitEffectKind::CityAmenities},
        };
        static const std::pair<const char*, CombatAtom> atoms[] = {
            {"UNTRACKED", CombatAtom::Untracked},       {"ATTACKING", CombatAtom::Attacking},
            {"VS_CLASS", CombatAtom::VsClass},          {"VS_DOMAIN", CombatAtom::VsDomain},
            {"VS_DISTRICT", CombatAtom::VsDistrict},    {"COMBAT_TYPE", CombatAtom::CombatType},
            {"TILE_HILLS", CombatAtom::TileHills},      {"TILE_FEATURE", CombatAtom::TileFeature},
            {"TILE_TERRAIN", CombatAtom::TileTerrain},  {"OPPONENT_FORTIFIED", CombatAtom::OpponentFortified},
            {"OPPONENT_WOUNDED", CombatAtom::OpponentWounded}, {"DISTRICT_TILE", CombatAtom::DistrictTile},
            {"OWN_TERRITORY", CombatAtom::OwnTerritory},
        };
        for (const Json& e : list.items()) {
            UnitEffect fx;
            bool found = false;
            for (const auto& [name, k] : kinds) {
                if (e["kind"].str() == name) { fx.kind = k; found = true; }
            }
            if (!found) {
                *error = where + ": unknown effect kind " + e["kind"].str();
                return false;
            }
            fx.amount = static_cast<int>(e["amount"].integer(0));
            for (const Json& group : e["when"].items()) {
                std::vector<CombatCondition> any;
                for (const Json& a : group.items()) {
                    CombatCondition c;
                    found = false;
                    for (const auto& [name, k] : atoms) {
                        if (a["atom"].str() == name) { c.atom = k; found = true; }
                    }
                    if (!found) {
                        *error = where + ": unknown condition " + a["atom"].str();
                        return false;
                    }
                    c.negate = a["not"].boolean(false);
                    c.value = a["value"].str();
                    if (c.atom == CombatAtom::VsDomain) c.arg = c.value == "SEA" ? 1 : c.value == "AIR" ? 2 : 0;
                    if (c.atom == CombatAtom::CombatType) c.arg = c.value == "RANGED" ? 1 : 0;
                    if (c.atom == CombatAtom::TileFeature) c.ref = feature(c.value);
                    if (c.atom == CombatAtom::TileTerrain) c.ref = terrain(c.value);
                    if ((c.atom == CombatAtom::TileFeature || c.atom == CombatAtom::TileTerrain) && c.ref == kNone) {
                        *error = where + ": unknown plot type " + c.value;
                        return false;
                    }
                    any.push_back(std::move(c));
                }
                fx.when.push_back(std::move(any));
            }
            out.push_back(std::move(fx));
        }
        return true;
    };
    for (const auto& [id, j] : m.tables["abilities"]) {
        AbilityType a;
        a.id = id;
        a.name = j["name"].str(id);
        for (const Json& c : j["classes"].items()) a.classes.push_back(c.str());
        a.inactive = j["inactive"].boolean(false);
        if (!readEffects(j["effects"], a.effects, "ability " + id)) return false;
        abilities.push_back(std::move(a));
    }
    for (const auto& [id, j] : m.tables["promotions"]) {
        PromotionType pr;
        pr.id = id;
        pr.name = j["name"].str(id);
        pr.promotionClass = j["class"].str();
        pr.tier = static_cast<int>(j["tier"].integer(1));
        pr.branch = j["branch"].str();
        if (!readEffects(j["effects"], pr.effects, "promotion " + id)) return false;
        promotions.push_back(std::move(pr));
    }
    {
        size_t i = 0;
        auto findPromotion = [this](const std::string& pid) { return promotion(pid); };
        for (const auto& [id, j] : m.tables["promotions"]) {
            if (!resolveList(j["requires"], findPromotion, promotions[i++].prereqs, "promotion " + id, error)) return false;
        }
    }
    for (const auto& [id, j] : m.tables["units"]) {
        UnitType u;
        u.id = id;
        u.name = j["name"].str(id);
        u.unitClass = j["class"].str();
        const std::string& domain = j["domain"].str("LAND");
        u.domain = domain == "SEA" ? Domain::Sea : domain == "AIR" ? Domain::Air : Domain::Land;
        const std::string& layer = j["layer"].str("MILITARY");
        u.layer = layer == "CIVILIAN" ? UnitLayer::Civilian
                  : layer == "SUPPORT" ? UnitLayer::Support
                  : layer == "LEADER"  ? UnitLayer::Leader
                                       : UnitLayer::Military;
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
        u.costProgression = static_cast<int>(j["costProgression"].integer(0));
        u.popCost = static_cast<int>(j["popCost"].integer(0));
        u.minPopulation = static_cast<int>(j["minPopulation"].integer(0));
        u.mustPurchase = j["mustPurchase"].boolean(false);
        u.trainable = j["trainable"].boolean(true);
        u.agent = j["agent"].boolean(false);
        u.purchaseYield = j["purchaseYield"].str();
        u.religiousStrength = static_cast<int>(j["religiousStrength"].integer(0));
        u.spreadCharges = static_cast<int>(j["spreadCharges"].integer(0));
        u.evictPercent = static_cast<int>(j["evictPercent"].integer(0));
        u.healCharges = static_cast<int>(j["healCharges"].integer(0));
        u.foundReligion = j["foundReligion"].boolean(false);
        if (!readUnlock(j["unlock"], u.unlock, "unit " + id)) return false;
        if (!u.unlock.none()) u.era = (u.unlock.civic ? civics : techs)[static_cast<size_t>(u.unlock.index)].era;
        const Json& sc = j["strategicCost"];
        if (!sc.isNull()) {
            u.strategicResource = resource(sc["resource"].str());
            u.strategicCost = static_cast<int>(sc["amount"].integer(0));
            if (u.strategicResource == kNone) {
                *error = "unit " + id + ": unknown strategic resource " + sc["resource"].str();
                return false;
            }
        }
        if (!readUnlock(j["obsoleteWith"], u.obsoleteWith, "unit " + id)) return false;
        u.bombard = static_cast<int>(j["bombard"].integer(0));
        const Json& rm = j["resourceMaintenance"];
        if (!rm.isNull()) {
            u.resourceMaintenance = static_cast<int>(rm["amount"].integer(0));
            if (u.strategicResource == kNone) u.strategicResource = resource(rm["resource"].str());
            if (u.strategicResource != resource(rm["resource"].str())) {
                *error = "unit " + id + ": maintenance resource differs from its strategic cost";
                return false;
            }
        }
        u.promotionClass = j["promotionClass"].str();
        auto findAbility = [this](const std::string& aid) { return ability(aid); };
        if (!resolveList(j["abilities"], findAbility, u.abilities, "unit " + id, error)) return false;
        units.push_back(std::move(u));
    }
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["units"]) {
            const std::string& up = j["upgradesTo"].str();
            if (!up.empty() && (units[i].upgradesTo = unit(up)) == kNone) {
                *error = "unit " + id + ": unknown upgrade " + up;
                return false;
            }
            const std::string& cap = j["capturedAs"].str();
            if (!cap.empty() && (units[i].capturedAs = unit(cap)) == kNone) {
                *error = "unit " + id + ": unknown capture result " + cap;
                return false;
            }
            ++i;
        }
    }
    {
        const Table& rows = m.tables["improvements"];
        for (const auto& [id, j] : rows) {
            ImprovementType im;
            im.id = id;
            im.name = j["name"].str(id);
            if (!readUnlock(j["unlock"], im.unlock, "improvement " + id)) return false;
            im.yields = readYields(j["yields"]);
            const std::string where = "improvement " + id;
            if (!resolveList(j["validTerrains"], findTerrain, im.validTerrains, where, error) ||
                !resolveList(j["validFeatures"], findFeature, im.validFeatures, where, error) ||
                !resolveList(j["validResources"], [this](const std::string& r) { return resource(r); },
                             im.validResources, where, error))
                return false;
            for (const Json& b : j["bonusYields"].items()) {
                ImprovementBonus bonus;
                if (!parseYieldName(b["yield"].str(), bonus.yield)) {
                    *error = where + ": bad bonus yield";
                    return false;
                }
                bonus.amount = b["amount"].fixed();
                if (!readUnlock(b["unlock"], bonus.unlock, where)) return false;
                im.bonuses.push_back(bonus);
            }
            im.housing = j["housing"].fixed();
            improvements.push_back(std::move(im));
        }
        // Second pass: adjacency refers to other improvements.
        for (size_t i = 0; i < rows.size(); ++i) {
            const std::string where = "improvement " + rows[i].first;
            for (const Json& a : rows[i].second["adjacency"].items()) {
                ImprovementAdjacency adj;
                if (!parseYieldName(a["yield"].str(), adj.yield)) {
                    *error = where + ": bad adjacency yield";
                    return false;
                }
                adj.amount = a["amount"].fixed();
                adj.per = std::max(1, static_cast<int>(a["per"].integer(1)));
                adj.improvement = improvement(a["improvement"].str());
                if (adj.improvement == kNone) {
                    *error = where + ": unknown adjacent improvement " + a["improvement"].str();
                    return false;
                }
                if (!readUnlock(a["needs"], adj.needs, where) || !readUnlock(a["obsoleteWith"], adj.obsoleteWith, where))
                    return false;
                improvements[i].adjacency.push_back(adj);
            }
        }
    }
    {
        const Table& rows = m.tables["buildings"];
        for (const auto& [id, j] : rows) {
            BuildingType b;
            b.id = id;
            b.name = j["name"].str(id);
            b.district = j["district"].str("DISTRICT_CITY_CENTER");
            if (!readUnlock(j["unlock"], b.unlock, "building " + id)) return false;
            b.cost = static_cast<int>(j["cost"].integer(0));
            b.maintenance = static_cast<int>(j["maintenance"].integer(0));
            b.yields = readYields(j["yields"]);
            b.housing = j["housing"].fixed();
            b.amenities = static_cast<int>(j["amenities"].integer(0));
            b.outerDefenseHp = static_cast<int>(j["outerDefenseHp"].integer(0));
            b.defense = static_cast<int>(j["defense"].integer(0));
            b.needsRiver = j["needsRiver"].boolean(false);
            b.purchasable = j["purchasable"].boolean(false);
            b.faithOnly = j["faithOnly"].boolean(false);
            b.tradeCapacity = static_cast<int>(j["tradeCapacity"].integer(0));
            b.meleeCannotDamageWalls = j["meleeCannotDamageWalls"].boolean(false);
            b.wallsCannotBeBypassed = j["wallsCannotBeBypassed"].boolean(false);
            b.wonder = j.has("placement");
            b.text = j["text"].str();
            if (b.wonder) b.district = "DISTRICT_WONDER";
            buildings.push_back(std::move(b));
        }
        // Second pass: building references may point forward.
        auto findBuilding = [this](const std::string& bid) { return building(bid); };
        for (size_t i = 0; i < rows.size(); ++i) {
            if (!resolveList(rows[i].second["requires"], findBuilding, buildings[i].prereqs, "building " + rows[i].first, error))
                return false;
        }
        size_t k = 0;
        for (const auto& [uid, j] : m.tables["units"]) {
            if (!resolveList(j["needsBuilding"], findBuilding, units[k++].needsBuilding, "unit " + uid, error)) return false;
        }
    }
    for (const auto& [id, j] : m.tables["districts"]) {
        DistrictType d;
        d.id = id;
        d.name = j["name"].str(id);
        d.hp = static_cast<int>(j["hp"].integer(0));
        d.attackRange = static_cast<int>(j["attackRange"].integer(0));
        const std::string where = "district " + id;
        if (!readUnlock(j["unlock"], d.unlock, where)) return false;
        d.cost = static_cast<int>(j["cost"].integer(0));
        const std::string& progression = j["costProgression"].str("NO_COST_PROGRESSION");
        d.costProgression = progression == "NUM_UNDER_AVG_PLUS_TECH" ? DistrictCostProgression::NumUnderAvgPlusTech
                            : progression == "GAME_PROGRESS"         ? DistrictCostProgression::GameProgress
                                                                     : DistrictCostProgression::None;
        d.costDiscountPercent = static_cast<int>(j["costDiscountPercent"].integer(0));
        d.needsPopulation = j["needsPopulation"].boolean(false);
        d.maintenance = static_cast<int>(j["maintenance"].integer(0));
        d.notAdjacentToCityCenter = j["notAdjacentToCityCenter"].boolean(false);
        d.water = j["water"].boolean(false);
        d.tradeDomestic = readYields(j["tradeYields"]["domestic"]);
        d.tradeInternational = readYields(j["tradeYields"]["international"]);
        districts.push_back(std::move(d));
    }
    for (BuildingType& b : buildings) b.districtType = district(b.district);
    // Wonder placement and one-time effects name terrains, features, resources, districts and units.
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["buildings"]) {
            BuildingType& b = buildings[i++];
            if (!b.wonder) continue;
            const std::string where = "wonder " + id;
            const Json& p = j["placement"];
            WonderPlacement& w = b.placement;
            for (const Json& t : p["terrains"].items()) {
                if (t.str() == "MOUNTAIN") {
                    w.mountain = true;
                } else if (TypeIndex ti = terrain(t.str()); ti != kNone) {
                    w.terrains.push_back(ti);
                } else {
                    *error = where + ": unknown terrain " + t.str();
                    return false;
                }
            }
            auto readFeatures = [&](const Json& list, std::vector<TypeIndex>& out) {
                for (const Json& f : list.items()) {
                    const TypeIndex fi = feature(f.str());
                    if (fi == kNone) {
                        *error = where + ": unknown feature " + f.str();
                        return false;
                    }
                    out.push_back(fi);
                }
                return true;
            };
            if (!readFeatures(p["features"], w.features) || !readFeatures(p["needsFeature"], w.needsFeature)) return false;
            w.river = p["river"].boolean(false);
            w.coastal = p["coastal"].boolean(false);
            w.lake = p["lake"].boolean(false);
            w.notLake = p["notLake"].boolean(false);
            w.nextToLand = p["nextToLand"].boolean(false);
            w.nextToCapital = p["nextToCapital"].boolean(false);
            w.nextToMountain = p["nextToMountain"].boolean(false);
            w.nextToCityCenter = p["nextToCityCenter"].boolean(false);
            if (p.has("nextToDistrict") && (w.nextToDistrict = district(p["nextToDistrict"].str())) == kNone) {
                *error = where + ": unknown district";
                return false;
            }
            if (p.has("nextToResource") && (w.nextToResource = resource(p["nextToResource"].str())) == kNone) {
                *error = where + ": unknown resource";
                return false;
            }
            if (p.has("nextToImprovement") && (w.nextToImprovement = improvement(p["nextToImprovement"].str())) == kNone) {
                *error = where + ": unknown improvement";
                return false;
            }
            for (const Json& ej : j["effects"].items()) {
                GreatPersonEffect fx;
                const std::string& kind = ej["kind"].str();
                if (kind == "UNIT") {
                    fx.kind = GreatPersonEffectKind::Unit;
                    fx.ref = unit(ej["ref"].str());
                } else if (kind == "RANDOM_BOOST") {
                    fx.kind = GreatPersonEffectKind::RandomBoost;
                    fx.civic = ej["tree"].str() == "CIVIC";
                    fx.count = static_cast<int>(ej["count"].integer(0));
                    fx.minEra = era(ej["minEra"].str());
                    fx.maxEra = era(ej["maxEra"].str());
                    fx.ref = fx.minEra == kNone || fx.maxEra == kNone ? kNone : 0;
                } else {
                    *error = where + ": unknown effect " + kind;
                    return false;
                }
                if (fx.ref == kNone) {
                    *error = where + ": bad effect";
                    return false;
                }
                b.wonderEffects.push_back(fx);
            }
        }
    }
    // Second pass: adjacency rows may name districts later in the table.
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["districts"]) {
            const std::string where = "district " + id;
            for (const Json& a : j["adjacency"].items()) {
                DistrictAdjacency adj;
                if (!parseYieldName(a["yield"].str(), adj.yield)) {
                    *error = where + ": bad adjacency yield";
                    return false;
                }
                adj.amount = static_cast<int>(a["amount"].integer(0));
                adj.tilesRequired = std::max(1, static_cast<int>(a["tilesRequired"].integer(1)));
                const std::string& kind = a["kind"].str();
                const std::string& ref = a["ref"].str();
                if (kind == "MOUNTAIN") adj.kind = DistrictAdjacencyKind::Mountain;
                else if (kind == "RIVER") adj.kind = DistrictAdjacencyKind::River;
                else if (kind == "ANY_DISTRICT") adj.kind = DistrictAdjacencyKind::AnyDistrict;
                else if (kind == "STRATEGIC_RESOURCE") adj.kind = DistrictAdjacencyKind::StrategicResource;
                else if (kind == "SEA_RESOURCE") adj.kind = DistrictAdjacencyKind::SeaResource;
                else if (kind == "DISTRICT") {
                    adj.kind = DistrictAdjacencyKind::District;
                    adj.ref = district(ref);
                } else if (kind == "FEATURE") {
                    adj.kind = DistrictAdjacencyKind::Feature;
                    adj.ref = feature(ref);
                } else if (kind == "IMPROVEMENT") {
                    adj.kind = DistrictAdjacencyKind::Improvement;
                    adj.ref = improvement(ref);
                } else {
                    *error = where + ": unknown adjacency kind " + kind;
                    return false;
                }
                const bool needsRef = adj.kind == DistrictAdjacencyKind::District || adj.kind == DistrictAdjacencyKind::Feature ||
                                      adj.kind == DistrictAdjacencyKind::Improvement;
                if (needsRef && adj.ref == kNone) {
                    *error = where + ": unknown adjacency reference " + ref;
                    return false;
                }
                districts[i].adjacency.push_back(adj);
            }
            ++i;
        }
    }
    for (const auto& [id, j] : m.tables["barbarianTribes"]) {
        BarbarianTribe t;
        t.id = id;
        t.coastal = j["coastal"].boolean(false);
        if (j.has("resource") && (t.resource = resource(j["resource"].str())) == kNone) {
            *error = "barbarian tribe " + id + ": unknown resource";
            return false;
        }
        t.resourceRange = static_cast<int>(j["resourceRange"].integer(0));
        t.rangedPercent = static_cast<int>(j["rangedPercent"].integer(0));
        t.spawnTurns = static_cast<int>(j["spawnTurns"].integer(10));
        t.raidBoldness = static_cast<int>(j["raidBoldness"].integer(0));
        t.attackBoldness = static_cast<int>(j["attackBoldness"].integer(0));
        t.unitClass = j["unitClass"].str();
        barbarianTribes.push_back(std::move(t));
    }
    for (const auto& [id, j] : m.tables["grantedBuildings"]) {
        TypeIndex b = building(j["building"].str());
        if (b == kNone) {
            *error = "granted building " + id + ": unknown building";
            return false;
        }
        buildings[static_cast<size_t>(b)].granted = true;
    }
    // A tech may let one unit type embark early.
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["techs"]) {
            TreeNode& n = techs[i++];
            if (!j.has("embarkUnit")) continue;
            n.embarkUnit = unit(j["embarkUnit"].str());
            if (n.embarkUnit == kNone) {
                *error = "tech " + id + ": unknown embark unit " + j["embarkUnit"].str();
                return false;
            }
        }
    }
    // Boost conditions refer to units, buildings and the trees themselves.
    auto readBoosts = [&](const char* name, std::vector<TreeNode>& nodes) {
        size_t i = 0;
        for (const auto& [id, j] : m.tables[name]) {
            Boost& b = nodes[i++].boost;
            const Json& bj = j["boost"];
            if (bj.isNull()) continue;
            b.percent = static_cast<int>(bj["percent"].integer(0));
            b.type = bj["type"].str("NONE");
            b.text = bj["text"].str();
            b.count = static_cast<int>(bj["count"].integer(1));
            const std::string& ref = bj["ref"].str();
            static const std::pair<const char*, BoostKind> kinds[] = {
                {"NONE", BoostKind::None},
                {"COASTAL_CITY", BoostKind::CoastalCity},
                {"BUILDING", BoostKind::Building},
                {"OWN_UNITS", BoostKind::OwnUnits},
                {"TECH", BoostKind::Tech},
                {"CIVIC", BoostKind::Civic},
                {"GOVERNMENT_TIER", BoostKind::GovernmentTier},
                {"TOTAL_POPULATION", BoostKind::TotalPopulation},
                {"CITY_POPULATION", BoostKind::CityPopulation},
                {"LAND_COMBAT_UNITS", BoostKind::LandCombatUnits},
                {"IMPROVEMENT", BoostKind::Improvement},
                {"IMPROVEMENT_ON_RESOURCE", BoostKind::ImprovementOnResource},
                {"IMPROVE_RESOURCE", BoostKind::ImproveResource},
                {"IMPROVED_TILES", BoostKind::ImprovedTiles},
            };
            b.kind = BoostKind::NotTracked;
            for (const auto& [k, v] : kinds) if (b.type == k) b.kind = v;
            switch (b.kind) {
                case BoostKind::Building: b.ref = building(ref); break;
                case BoostKind::OwnUnits: b.ref = unit(ref); break;
                case BoostKind::Tech: b.ref = tech(ref); break;
                case BoostKind::Civic: b.ref = civic(ref); break;
                case BoostKind::Improvement:
                case BoostKind::ImprovementOnResource: b.ref = improvement(ref); break;
                case BoostKind::ImproveResource: b.ref = resource(ref); break;
                default: break;
            }
            const bool needsRef = b.kind == BoostKind::Building || b.kind == BoostKind::OwnUnits ||
                                  b.kind == BoostKind::Tech || b.kind == BoostKind::Civic ||
                                  b.kind == BoostKind::Improvement || b.kind == BoostKind::ImprovementOnResource ||
                                  b.kind == BoostKind::ImproveResource;
            if (needsRef && b.ref == kNone) {
                // A reference the rules do not define (e.g. a Great Person unit) cannot fire yet.
                b.kind = BoostKind::NotTracked;
            }
        }
    };
    readBoosts("techs", techs);
    readBoosts("civics", civics);
    for (const auto& [id, j] : m.tables["governments"]) {
        GovernmentType g;
        g.id = id;
        g.name = j["name"].str(id);
        g.tier = static_cast<int>(j["tier"].integer(0));
        g.influencePerTurn = static_cast<int>(j["influencePerTurn"].integer(0));
        g.influenceThreshold = static_cast<int>(j["influenceThreshold"].integer(0));
        g.envoysPerThreshold = static_cast<int>(j["envoysPerThreshold"].integer(0));
        if (!readUnlock(j["unlock"], g.unlock, "government " + id)) return false;
        static const char* slotNames[kNumGovernmentSlotTypes] = {"MILITARY", "ECONOMIC", "DIPLOMATIC", "WILDCARD"};
        for (size_t k = 0; k < kNumGovernmentSlotTypes; ++k) {
            g.slots[k] = static_cast<int>(j["slots"][slotNames[k]].integer(0));
            if (g.slots[k] < 0 || g.slots[k] > 16) {
                *error = "government " + id + ": bad slot count";
                return false;
            }
        }
        governments.push_back(std::move(g));
    }
    for (const auto& [id, j] : m.tables["policies"]) {
        PolicyType p;
        p.id = id;
        p.name = j["name"].str(id);
        static const std::pair<const char*, PolicySlot> slots[] = {
            {"MILITARY", PolicySlot::Military},     {"ECONOMIC", PolicySlot::Economic},
            {"DIPLOMATIC", PolicySlot::Diplomatic}, {"WILDCARD", PolicySlot::Wildcard},
            {"GREAT_PERSON", PolicySlot::GreatPerson},
        };
        const std::string& slot = j["slot"].str();
        bool found = false;
        for (const auto& [k, v] : slots) if (slot == k) { p.slot = v; found = true; }
        if (!found) {
            *error = "policy " + id + ": unknown slot " + slot;
            return false;
        }
        if (!readUnlock(j["unlock"], p.unlock, "policy " + id)) return false;
        const std::string& gov = j["government"].str();
        if (!gov.empty() && (p.government = government(gov)) == kNone) {
            *error = "policy " + id + ": unknown government " + gov;
            return false;
        }
        policies.push_back(std::move(p));
    }
    {
        auto findPolicy = [this](const std::string& pid) { return policy(pid); };
        size_t i = 0;
        for (const auto& [id, j] : m.tables["policies"]) {
            if (!resolveList(j["obsoletedBy"], findPolicy, policies[i++].obsoletedBy, "policy " + id, error)) return false;
        }
    }
    for (const auto& [id, j] : m.tables["happinessLevels"]) {
        HappinessLevel h;
        h.id = id;
        h.minBalance = j.has("minBalance") ? static_cast<int>(j["minBalance"].integer(0)) : INT_MIN;
        h.growthPercent = static_cast<int>(j["growthPercent"].integer(0));
        h.yieldPercent = static_cast<int>(j["yieldPercent"].integer(0));
        h.loyaltyPerTurn = static_cast<int>(j["loyaltyPerTurn"].integer(0));
        happiness.push_back(std::move(h));
    }
    std::sort(happiness.begin(), happiness.end(),
              [](const HappinessLevel& a, const HappinessLevel& b) { return a.minBalance < b.minBalance; });
    for (const auto& [id, j] : m.tables["loyaltyLevels"]) {
        LoyaltyLevel l;
        l.id = id;
        l.minLoyalty = static_cast<int>(j["minLoyalty"].integer(0));
        l.yieldPercent = static_cast<int>(j["yieldPercent"].integer(0));
        l.growthPercent = static_cast<int>(j["growthPercent"].integer(100));
        loyaltyLevels.push_back(std::move(l));
    }
    std::sort(loyaltyLevels.begin(), loyaltyLevels.end(),
              [](const LoyaltyLevel& a, const LoyaltyLevel& b) { return a.minLoyalty < b.minLoyalty; });
    for (const auto& [id, j] : m.tables["civilizations"]) {
        CivType c;
        c.id = id;
        c.name = j["name"].str(id);
        c.leader = j["leader"].str();
        for (const Json& n : j["cityNames"].items()) c.cityNames.push_back(n.str());
        civs.push_back(std::move(c));
    }
    {
        size_t k = 0;  // districts load after units
        for (const auto& [uid, j] : m.tables["units"]) {
            const std::string& nd = j["needsDistrict"].str();
            if (!nd.empty() && (units[k].needsDistrict = district(nd)) == kNone) {
                *error = "unit " + uid + ": unknown district " + nd;
                return false;
            }
            ++k;
        }
    }
    for (const auto& [id, j] : m.tables["dynasties"]) {
        Dynasty d;
        d.id = id;
        d.civ = civ(j["civ"].str());
        for (const Json& n : j["names"].items()) d.names.push_back(n.str());
        if (d.civ == kNone || d.names.empty()) {
            *error = "dynasty " + id + ": unknown civ or no names";
            return false;
        }
        dynasties.push_back(std::move(d));
    }
    // City-states (08) and what envoys to them give.
    auto kindOf = [](const std::string& k, CityStateKind& out) {
        static const std::pair<const char*, CityStateKind> kinds[] = {
            {"SCIENTIFIC", CityStateKind::Scientific}, {"CULTURAL", CityStateKind::Cultural}, {"RELIGIOUS", CityStateKind::Religious},
            {"TRADE", CityStateKind::Trade}, {"INDUSTRIAL", CityStateKind::Industrial}, {"MILITARISTIC", CityStateKind::Militaristic}};
        for (const auto& [name, v] : kinds) {
            if (k == name) {
                out = v;
                return true;
            }
        }
        return false;
    };
    for (const auto& [id, j] : m.tables["cityStates"]) {
        CityStateType c;
        c.id = id;
        c.name = j["name"].str(id);
        c.suzerainText = j["suzerainText"].str();
        if (!kindOf(j["type"].str(), c.kind)) {
            *error = "city-state " + id + ": unknown type";
            return false;
        }
        cityStates.push_back(std::move(c));
    }
    for (const auto& [id, j] : m.tables["envoyBonuses"]) {
        EnvoyBonus e;
        const std::string where = "envoy bonus " + id;
        if (!kindOf(j["type"].str(), e.kind)) {
            *error = where + ": unknown type";
            return false;
        }
        e.envoys = static_cast<int>(j["envoys"].integer(1));
        if (j.has("yield") && !parseYieldName(j["yield"].str(), e.yield)) {
            *error = where + ": bad yield";
            return false;
        }
        e.amount = static_cast<int>(j["amount"].integer(0));
        e.capital = j["capital"].boolean(false);
        if (j.has("building") && (e.building = building(j["building"].str())) == kNone) continue;  // a building not modelled
        e.production = static_cast<int>(j["production"].integer(0));
        const std::string& toward = j["toward"].str("UNITS");
        e.toward = toward == "BUILDINGS" ? EnvoyToward::Buildings : toward == "DISTRICTS" ? EnvoyToward::Districts : EnvoyToward::Units;
        for (const Json& b : j["buildings"].items()) {
            const TypeIndex bi = building(b.str());
            if (bi != kNone) e.buildings.push_back(bi);
        }
        if (e.production > 0 && !e.capital && e.buildings.empty()) continue;
        envoyBonuses.push_back(std::move(e));
    }
    // Religion (06): beliefs and the religions that can be founded.
    for (const auto& [id, j] : m.tables["beliefs"]) {
        BeliefType b;
        b.id = id;
        b.name = j["name"].str(id);
        b.text = j["text"].str();
        const std::string& cls = j["class"].str();
        if (cls == "PANTHEON") b.cls = BeliefClass::Pantheon;
        else if (cls == "FOLLOWER") b.cls = BeliefClass::Follower;
        else if (cls == "WORSHIP") b.cls = BeliefClass::Worship;
        else if (cls == "FOUNDER") b.cls = BeliefClass::Founder;
        else if (cls == "ENHANCER") b.cls = BeliefClass::Enhancer;
        else {
            *error = "belief " + id + ": unknown class " + cls;
            return false;
        }
        if (j.has("worshipBuilding") && (b.worshipBuilding = building(j["worshipBuilding"].str())) == kNone) {
            *error = "belief " + id + ": unknown building";
            return false;
        }
        if (j.has("grantUnit") && (b.grantUnit = unit(j["grantUnit"].str())) == kNone) {
            *error = "belief " + id + ": unknown unit";
            return false;
        }
        beliefs.push_back(std::move(b));
    }
    for (const auto& [id, j] : m.tables["religions"]) religions.push_back({id, j["name"].str(id)});
    // Great people (07): classes, the points buildings and districts earn, individuals, Great Works.
    for (const auto& [id, j] : m.tables["greatPersonClasses"]) {
        GreatPersonClass c;
        c.id = id;
        c.name = j["name"].str(id);
        c.unit = unit(j["unit"].str());
        c.district = district(j["district"].str());
        c.maxPerPlayer = static_cast<int>(j["maxPerPlayer"].integer(0));
        if (c.unit == kNone) {
            *error = "great person class " + id + ": unknown unit";
            return false;
        }
        greatPersonClasses.push_back(std::move(c));
    }
    auto readPoints = [&](const Json& j, std::vector<std::pair<TypeIndex, int>>& out, const std::string& where) {
        for (const auto& [cls, v] : j.members()) {
            const TypeIndex c = greatPersonClass(cls);
            if (c == kNone) {
                *error = where + ": unknown great person class " + cls;
                return false;
            }
            out.emplace_back(c, static_cast<int>(v.integer(0)));
        }
        return true;
    };
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["buildings"]) {
            BuildingType& b = buildings[i++];
            if (!readPoints(j["greatPersonPoints"], b.greatPersonPoints, "building " + id)) return false;
            for (const auto& [slot, v] : j["greatWorkSlots"].members()) b.greatWorkSlots.emplace_back(slot, static_cast<int>(v.integer(0)));
        }
        i = 0;
        for (const auto& [id, j] : m.tables["districts"]) {
            if (!readPoints(j["greatPersonPoints"], districts[i++].greatPersonPoints, "district " + id)) return false;
        }
    }
    {
        size_t i = 0;
        for (const auto& [id, j] : m.tables["buildings"]) {
            BuildingType& bt = buildings[i++];
            if (j.has("tradeCapacityUnless") && (bt.tradeCapacityUnless = building(j["tradeCapacityUnless"].str())) == kNone) {
                *error = "building " + id + ": unknown building";
                return false;
            }
        }
    }
    for (const auto& [id, j] : m.tables["greatWorkTypes"]) {
        GreatWorkType w;
        w.id = id;
        if (!parseYieldName(j["yield"].str(), w.yield)) {
            *error = "great work type " + id + ": bad yield";
            return false;
        }
        w.amount = static_cast<int>(j["amount"].integer(0));
        w.tourism = static_cast<int>(j["tourism"].integer(0));
        for (const Json& s : j["slots"].items()) w.slots.push_back(s.str());
        greatWorkTypes.push_back(std::move(w));
    }
    for (const auto& [id, j] : m.tables["greatPeople"]) {
        const std::string where = "great person " + id;
        GreatPersonType g;
        g.id = id;
        g.name = j["name"].str(id);
        g.cls = greatPersonClass(j["class"].str());
        const TypeIndex e = era(j["era"].str());
        if (g.cls == kNone || e == kNone) {
            *error = where + ": unknown class or era";
            return false;
        }
        g.era = e;
        g.charges = static_cast<int>(j["charges"].integer(0));
        const Json& rq = j["requires"];
        g.ownedTile = rq["ownedTile"].boolean(false);
        if (rq.has("district") && (g.district = district(rq["district"].str())) == kNone) {
            *error = where + ": unknown district " + rq["district"].str();
            return false;
        }
        g.noMilitaryUnit = rq["noMilitaryUnit"].boolean(false);
        if (rq.has("unitDomain")) {
            const std::string& d = rq["unitDomain"].str();
            g.unitDomain = static_cast<int>(d == "SEA" ? Domain::Sea : d == "AIR" ? Domain::Air : Domain::Land);
        }
        if (rq.has("missingBuilding")) g.missingBuilding = building(rq["missingBuilding"].str());
        for (const Json& ej : j["effects"].items()) {
            GreatPersonEffect fx;
            const std::string& kind = ej["kind"].str();
            const std::string& ref = ej["ref"].str();
            fx.amount = static_cast<int>(ej["amount"].integer(0));
            fx.count = static_cast<int>(ej["count"].integer(0));
            fx.scaled = ej["scaled"].boolean(false);
            fx.orComplete = ej["orComplete"].boolean(false);
            if (kind == "YIELD") {
                fx.kind = GreatPersonEffectKind::Yield;
                if (!parseYieldName(ej["yield"].str(), fx.yield)) {
                    *error = where + ": bad effect yield";
                    return false;
                }
            } else if (kind == "PRODUCTION") {
                fx.kind = GreatPersonEffectKind::Production;
            } else if (kind == "BOOST") {
                fx.kind = GreatPersonEffectKind::Boost;
                fx.civic = ref.rfind("CIVIC_", 0) == 0;
                fx.ref = fx.civic ? civic(ref) : tech(ref);
            } else if (kind == "RANDOM_BOOST") {
                fx.kind = GreatPersonEffectKind::RandomBoost;
                fx.civic = ej["tree"].str() == "CIVIC";
                fx.minEra = era(ej["minEra"].str());
                fx.maxEra = era(ej["maxEra"].str());
                if (fx.minEra == kNone || fx.maxEra == kNone) {
                    *error = where + ": bad boost eras";
                    return false;
                }
                fx.ref = 0;
            } else if (kind == "PROMOTION_XP") {
                fx.kind = GreatPersonEffectKind::PromotionXp;
                fx.ref = 0;
            } else if (kind == "BUILDING") {
                fx.kind = GreatPersonEffectKind::Building;
                fx.ref = building(ref);
            } else if (kind == "UNIT") {
                fx.kind = GreatPersonEffectKind::Unit;
                fx.ref = unit(ref);
            } else if (kind == "BUILDING_YIELD") {
                fx.kind = GreatPersonEffectKind::BuildingYield;
                fx.ref = building(ej["building"].str());
                if (!parseYieldName(ej["yield"].str(), fx.yield)) {
                    *error = where + ": bad effect yield";
                    return false;
                }
            } else if (kind == "GREAT_PERSON_POINTS") {
                fx.kind = GreatPersonEffectKind::GreatPersonPoints;
                fx.ref = 0;
            } else {
                *error = where + ": unknown effect kind " + kind;
                return false;
            }
            if (fx.ref == kNone && fx.kind != GreatPersonEffectKind::Yield && fx.kind != GreatPersonEffectKind::Production) {
                *error = where + ": effect " + kind + " refers to unknown " + ref;
                return false;
            }
            g.effects.push_back(fx);
        }
        for (const Json& t : j["untrackedEffects"].items()) g.untrackedEffects.push_back(t.str());
        if (j.has("greatWorks")) {
            g.greatWorkType = greatWorkType(j["greatWorks"]["type"].str());
            g.greatWorkCount = static_cast<int>(j["greatWorks"]["count"].integer(0));
            if (g.greatWorkType == kNone) {
                *error = where + ": unknown great work type";
                return false;
            }
        }
        if (j.has("aura")) {
            const Json& a = j["aura"];
            g.hasAura = true;
            g.aura.domain = a["domain"].str() == "SEA" ? Domain::Sea : Domain::Land;
            g.aura.strength = static_cast<int>(a["strength"].integer(0));
            g.aura.moves = static_cast<int>(a["moves"].integer(0));
            g.aura.range = static_cast<int>(a["range"].integer(0));
            for (const Json& er : a["eras"].items()) g.aura.eras.push_back(era(er.str()));
        }
        greatPeople.push_back(std::move(g));
    }
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i].layer != UnitLayer::Leader) continue;
        if (leaderUnit != kNone) {
            *error = "more than one leader unit (" + units[i].id + ")";
            return false;
        }
        leaderUnit = static_cast<TypeIndex>(i);
    }
    for (const auto& [id, j] : m.tables["gear"]) {
        GearType g;
        g.id = id;
        g.name = j["name"].str(id);
        const std::string& slot = j["slot"].str();
        if (slot == "WEAPON") g.slot = GearSlot::Weapon;
        else if (slot == "ARMOR") g.slot = GearSlot::Armor;
        else if (slot == "MOUNT") g.slot = GearSlot::Mount;
        else {
            *error = "gear " + id + ": unknown slot " + slot;
            return false;
        }
        if (!readUnlock(j["unlock"], g.unlock, "gear " + id)) return false;
        g.combat = static_cast<int>(j["combat"].integer(0));
        g.ranged = static_cast<int>(j["ranged"].integer(0));
        g.range = static_cast<int>(j["range"].integer(0));
        g.defense = static_cast<int>(j["defense"].integer(0));
        g.moves = static_cast<int>(j["moves"].integer(0));
        g.goldCost = static_cast<int>(j["goldCost"].integer(0));
        const Json& sc = j["strategicCost"];
        if (!sc.isNull()) {
            g.strategicResource = resource(sc["resource"].str());
            g.strategicCost = static_cast<int>(sc["amount"].integer(0));
            if (g.strategicResource == kNone) {
                *error = "gear " + id + ": unknown strategic resource " + sc["resource"].str();
                return false;
            }
        }
        const std::string& upkeep = j["upkeepAs"].str();
        if (!upkeep.empty() && (g.upkeepAs = unit(upkeep)) == kNone) {
            *error = "gear " + id + ": unknown unit " + upkeep;
            return false;
        }
        gear.push_back(std::move(g));
    }
    for (const auto& [id, j] : m.tables["modifiers"]) {
        Modifier mod;
        mod.id = id;
        mod.source = j["source"].str();
        if (!parseModifier(j, mod, *this, error)) {
            *error = "modifier " + id + ": " + *error;
            return false;
        }
        if (mod.source == "EVERYONE") {
            mod.sourceKind = ModSource::Everyone;
        } else if ((mod.sourceIndex = building(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Building;
        } else if ((mod.sourceIndex = civ(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Civ;
        } else if ((mod.sourceIndex = policy(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Policy;
        } else if ((mod.sourceIndex = government(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Government;
        } else if ((mod.sourceIndex = belief(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Belief;
        } else {
            *error = "modifier " + id + ": unknown source " + mod.source;
            return false;
        }
        modifiers.push_back(std::move(mod));
    }
    for (const auto& [id, j] : m.tables["mapSizes"]) {
        MapSizeType s;
        s.id = id;
        s.width = static_cast<int>(j["width"].integer(0));
        s.height = static_cast<int>(j["height"].integer(0));
        s.defaultPlayers = static_cast<int>(j["defaultPlayers"].integer(2));
        s.maxReligions = static_cast<int>(j["maxReligions"].integer(0));
        s.defaultCityStates = static_cast<int>(j["defaultCityStates"].integer(0));
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
                                     "CITY_SIGHT_RANGE", "COMBAT_MAX_HIT_POINTS",
                                     "CITY_FOOD_CONSUMPTION_PER_POPULATION", "CITY_GROWTH_THRESHOLD",
                                     "CITY_GROWTH_MULTIPLIER", "CITY_GROWTH_EXPONENT", "CULTURE_COST_FIRST_PLOT",
                                     "CULTURE_COST_LATER_PLOT_MULTIPLIER", "CULTURE_COST_LATER_PLOT_EXPONENT",
                                     "CITY_POP_PER_AMENITY", "PLOT_BUY_BASE_COST", "GOLD_PURCHASE_MULTIPLIER"};
    for (const char* name : required) {
        if (!hasGlobal(name)) {
            *error = std::string("missing required global ") + name;
            return false;
        }
    }
    if (district("DISTRICT_CITY_CENTER") == kNone) {
        *error = "rules have no DISTRICT_CITY_CENTER";
        return false;
    }
    if (terrains.empty() || units.empty() || civs.empty() || mapSizes.empty() || speeds.empty() || techs.empty() ||
        civics.empty() || governments.empty()) {
        *error = "rules are missing a required table";
        return false;
    }
    return true;
}

TypeIndex Rules::terrain(const std::string& id) const { return findIn(terrains, id); }
TypeIndex Rules::feature(const std::string& id) const { return findIn(features, id); }
TypeIndex Rules::resource(const std::string& id) const { return findIn(resources, id); }
TypeIndex Rules::ability(const std::string& id) const { return findIn(abilities, id); }
TypeIndex Rules::promotion(const std::string& id) const { return findIn(promotions, id); }
TypeIndex Rules::unit(const std::string& id) const { return findIn(units, id); }
TypeIndex Rules::building(const std::string& id) const { return findIn(buildings, id); }
TypeIndex Rules::district(const std::string& id) const { return findIn(districts, id); }
TypeIndex Rules::improvement(const std::string& id) const { return findIn(improvements, id); }
TypeIndex Rules::era(const std::string& id) const { return findIn(eras, id); }
TypeIndex Rules::tech(const std::string& id) const { return findIn(techs, id); }
TypeIndex Rules::civic(const std::string& id) const { return findIn(civics, id); }
TypeIndex Rules::government(const std::string& id) const { return findIn(governments, id); }
TypeIndex Rules::policy(const std::string& id) const { return findIn(policies, id); }

std::vector<const Modifier*> Rules::modifiersFrom(const std::string& source) const {
    std::vector<const Modifier*> out;
    for (const Modifier& m : modifiers) {
        if (m.source == source) out.push_back(&m);
    }
    return out;
}
TypeIndex Rules::civ(const std::string& id) const { return findIn(civs, id); }
TypeIndex Rules::gearType(const std::string& id) const { return findIn(gear, id); }
TypeIndex Rules::greatPersonClass(const std::string& id) const { return findIn(greatPersonClasses, id); }
TypeIndex Rules::greatPerson(const std::string& id) const { return findIn(greatPeople, id); }
TypeIndex Rules::greatWorkType(const std::string& id) const { return findIn(greatWorkTypes, id); }
TypeIndex Rules::belief(const std::string& id) const { return findIn(beliefs, id); }
TypeIndex Rules::religion(const std::string& id) const { return findIn(religions, id); }

const Dynasty* Rules::dynastyOf(TypeIndex c) const {
    for (const Dynasty& d : dynasties) {
        if (d.civ == c) return &d;
    }
    return nullptr;
}
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
