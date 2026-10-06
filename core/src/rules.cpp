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
        else if (type == "PLOT_HAS_FEATURE") { q.type = ReqType::PlotHasFeature; q.ref = ref.empty() ? kNone : rules.feature(ref); }
        else if (type == "PLOT_HAS_TERRAIN") { q.type = ReqType::PlotHasTerrain; q.ref = rules.terrain(ref); }
        else if (type == "PLOT_HAS_IMPROVEMENT") { q.type = ReqType::PlotHasImprovement; q.ref = ref.empty() ? kNone : rules.improvement(ref); }
        else if (type == "PLOT_NEXT_TO_RIVER") q.type = ReqType::PlotNextToRiver;
        else if (type == "CITY_HAS_BUILDING") { q.type = ReqType::CityHasBuilding; q.ref = rules.building(ref); }
        else if (type == "CITY_IS_CAPITAL") { q.type = ReqType::CityIsCapital; }
        else if (type == "CITY_MIN_POPULATION") { q.type = ReqType::CityMinPopulation; }
        else if (type == "PLAYER_IS_HUMAN") { q.type = ReqType::PlayerIsHuman; }
        else {
            *error = "unknown requirement type " + type;
            return false;
        }
        bool needsRef = q.type == ReqType::PlotHasResource || (q.type == ReqType::PlotHasFeature && !ref.empty()) ||
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
        {"ADJUST_WAR_WEARINESS_PERCENT", ModEffect::WarWearinessPercent},
        {"GRANT_ABILITY", ModEffect::GrantAbility},
        {"ADJUST_UNIT_XP_PERCENT", ModEffect::UnitXpPercent},
        {"ADJUST_UNIT_STRENGTH", ModEffect::UnitStrength},
        {"ADJUST_DISTRICT_ADJACENCY_PERCENT", ModEffect::DistrictAdjacencyPercent},
        {"ADJUST_CITY_LOYALTY", ModEffect::CityLoyalty},
        {"ADJUST_CITY_YIELD_PER_POP", ModEffect::CityYieldPerPop},
        {"ADJUST_CITY_YIELD_PER_DISTRICT", ModEffect::CityYieldPerDistrict},
        {"ADJUST_CITY_GREAT_PERSON_PERCENT", ModEffect::CityGreatPersonPercent},
        {"ADJUST_CITY_HARVEST_PERCENT", ModEffect::CityHarvestPercent},
        {"ADJUST_CITY_BORDER_GROWTH_PERCENT", ModEffect::CityBorderGrowthPercent},
        {"ADJUST_CITY_DISTRICT_PRODUCTION_PERCENT", ModEffect::CityDistrictProductionPercent},
        {"ADJUST_CITY_RELIGION_PRESSURE_PERCENT", ModEffect::CityReligionPressurePercent},
        {"SETTLERS_COST_NO_POPULATION", ModEffect::SettlerNoPopCost},
        {"ADJUST_BUILDER_CHARGES", ModEffect::BuilderExtraCharges},
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
    const bool playerEffect = mod.effect == ModEffect::UnitMaintenanceDiscount || mod.effect == ModEffect::WarWearinessPercent ||
                              mod.effect == ModEffect::GrantAbility || mod.effect == ModEffect::UnitXpPercent ||
                              mod.effect == ModEffect::UnitStrength || mod.effect == ModEffect::DistrictAdjacencyPercent ||
                              (mod.effect >= ModEffect::FounderYieldPerCity && mod.effect <= ModEffect::ReligionColonizes);
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
        "policies.json",    "improvements.json", "greatpeople.json", "religion.json", "wonders.json", "citystates.json", "moments.json", "governors.json", "espionage.json", "worldcongress.json", "disasters.json", "projects.json", "civilizations.json", "leader.json", "setup.json", "modifiers.json",
    };
    return names;
}

namespace {
// A civ ability and its leader's applied together: amounts add, lists join, a set reference wins.
CivAbility combineAbilities(const CivAbility& a, const CivAbility& b) {
    CivAbility c = a;
    c.name = a.name.empty() ? b.name : b.name.empty() ? a.name : a.name + " / " + b.name;
    c.extraAdjacency.insert(c.extraAdjacency.end(), b.extraAdjacency.begin(), b.extraAdjacency.end());
    if (c.wonderProductionPercent == 0) {
        c.wonderEraMin = b.wonderEraMin;
        c.wonderEraMax = b.wonderEraMax;
    }
    c.wonderProductionPercent += b.wonderProductionPercent;
    c.amenityPerWonder += b.amenityPerWonder;
    c.foundPopulation += b.foundPopulation;
    if (c.foundBuilding == kNone) c.foundBuilding = b.foundBuilding;
    c.culturePerSuzerainty += b.culturePerSuzerainty;
    c.governorLoyalty += b.governorLoyalty;
    c.governorGold += b.governorGold;
    if (c.extraGovernorTitleCivic == kNone) c.extraGovernorTitleCivic = b.extraGovernorTitleCivic;
    c.desertRouteGold += b.desertRouteGold;
    for (size_t i = 0; i < kNumYields; ++i) {
        c.capitalYieldsPerGovernorTitle[i] += b.capitalYieldsPerGovernorTitle[i];
        c.internationalRouteYields[i] += b.internationalRouteYields[i];
        c.peaceYieldPercent[i] += b.peaceYieldPercent[i];
    }
    c.freshWaterFarmHousing += b.freshWaterFarmHousing;
    c.mountainDistrictProductionPercent += b.mountainDistrictProductionPercent;
    c.mountainProduction += b.mountainProduction;
    for (size_t i = 0; i < c.domainProductionPercent.size(); ++i) c.domainProductionPercent[i] += b.domainProductionPercent[i];
    c.greatPersonPercent.insert(c.greatPersonPercent.end(), b.greatPersonPercent.begin(), b.greatPersonPercent.end());
    c.intercontinentalRouteGold += b.intercontinentalRouteGold;
    c.districtBuildingYields.insert(c.districtBuildingYields.end(), b.districtBuildingYields.begin(), b.districtBuildingYields.end());
    c.districtBuildingAmenities.insert(c.districtBuildingAmenities.end(), b.districtBuildingAmenities.begin(), b.districtBuildingAmenities.end());
    c.strengthNearLeader.insert(c.strengthNearLeader.end(), b.strengthNearLeader.begin(), b.strengthNearLeader.end());
    c.cityCenterBuildingProductionPercent += b.cityCenterBuildingProductionPercent;
    c.wallProductionPercent += b.wallProductionPercent;
    c.governorAmenity += b.governorAmenity;
    c.grantAbilities.insert(c.grantAbilities.end(), b.grantAbilities.begin(), b.grantAbilities.end());
    c.capturedCityLoyalty += b.capturedCityLoyalty;
    c.foreignReligionAmenity += b.foreignReligionAmenity;
    c.nearFollowingCityStrength += b.nearFollowingCityStrength;
    c.nearFollowingCityRange = std::max(c.nearFollowingCityRange, b.nearFollowingCityRange);
    c.extraBuilderCharges += b.extraBuilderCharges;
    c.wonderCulture += b.wonderCulture;
    if (c.faithPurchaseDistrict == kNone) c.faithPurchaseDistrict = b.faithPurchaseDistrict;
    c.killFaithPercent += b.killFaithPercent;
    if (c.capitalAmenityPerKills == 0) {
        c.capitalAmenityPerKills = b.capitalAmenityPerKills;
        c.capitalAmenityMax = b.capitalAmenityMax;
    }
    c.mountainCityHousing += b.mountainCityHousing;
    for (size_t i = 0; i < kNumYields; ++i) c.capitalYields[i] += b.capitalYields[i];
    c.cityLoyalty += b.cityLoyalty;
    c.unitXpPercent += b.unitXpPercent;
    c.classStrength.insert(c.classStrength.end(), b.classStrength.begin(), b.classStrength.end());
    return c;
}
}  // namespace

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

namespace {
Plunder readPlunder(const Json& j) {
    Plunder p;
    if (!j.isObject()) return p;
    static const std::pair<const char*, PlunderKind> kinds[] = {{"GOLD", PlunderKind::Gold}, {"FAITH", PlunderKind::Faith}, {"SCIENCE", PlunderKind::Science},
                                                                {"CULTURE", PlunderKind::Culture}, {"HEAL", PlunderKind::Heal}};
    for (const auto& [name, kind] : kinds) {
        if (j["kind"].str() == name) p.kind = kind;
    }
    p.amount = static_cast<int>(j["amount"].integer(0));
    return p;
}
}  // namespace

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
    // A row with a `base` is that row with its own fields laid over (civ uniques: only what differs).
    for (const char* name : {"units", "buildings", "improvements"}) {
        Table& table = m.tables[name];
        for (auto& [id, row] : table) {
            const std::string baseId = row["base"].str();
            if (baseId.empty()) continue;
            const Json* base = nullptr;
            for (const auto& [bid, brow] : table) {
                if (bid == baseId) base = &brow;
            }
            if (!base || base == &row) {
                *error = std::string(name) + " " + id + ": unknown base " + baseId;
                return false;
            }
            row = Json::overlay(*base, row);
        }
    }
    globals_ = m.globals;

    // Eras and research trees first: everything else may be unlocked by them.
    for (const auto& [id, j] : m.tables["eras"]) {
        EraType e;
        e.id = id;
        e.name = j["name"].str(id);
        e.embarkedStrength = static_cast<int>(j["embarkedStrength"].integer(10));
        e.greatPersonBaseCost = static_cast<int>(j["greatPersonBaseCost"].integer(0));
        e.tradeRouteExtraTurns = static_cast<int>(j["tradeRouteExtraTurns"].integer(0));
        e.grievanceDecay = static_cast<int>(j["grievanceDecay"].integer(0));
        e.minTurns = static_cast<int>(j["minTurns"].integer(0));
        e.maxTurns = static_cast<int>(j["maxTurns"].integer(0));
        e.eraScoreShift = static_cast<int>(j["eraScoreShift"].integer(0));
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
            n.spies = static_cast<int>(j["spies"].integer(0));
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
        const std::string relief = j["relief"].str("FLAT");
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
        f.naturalWonder = j["naturalWonder"].boolean(false);
        f.tiles = static_cast<int>(j["tiles"].integer(1));
        f.adjacentYields = readYields(j["adjacentYields"]);
        f.doublesAdjacentTerrain = j["doublesAdjacentTerrain"].boolean(false);
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
        rt.unitOnly = j["unitOnly"].boolean(false);
        if (j.has("tech") && (rt.tech = tech(j["tech"].str())) == kNone) {
            *error = "route " + id + ": unknown tech";
            return false;
        }
        for (const auto& [res, n] : j["resourceCost"].members()) rt.resourceCostIds.push_back({res, static_cast<int>(n.integer(0))});
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
        const std::string cls = j["class"].str("BONUS");
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
            {"HEAL_ON_KILL", UnitEffectKind::HealOnKill},        {"MELEE_AND_RANGED", UnitEffectKind::MeleeAndRanged},
            {"CAPTURE_AS_BUILDER", UnitEffectKind::CaptureAsBuilder},
            {"BAND_LEVEL", UnitEffectKind::BandLevel},          {"BAND_BURST", UnitEffectKind::BandBurst},
        };
        static const std::pair<const char*, CombatAtom> atoms[] = {
            {"UNTRACKED", CombatAtom::Untracked},       {"ATTACKING", CombatAtom::Attacking},
            {"VS_CLASS", CombatAtom::VsClass},          {"VS_DOMAIN", CombatAtom::VsDomain},
            {"VS_DISTRICT", CombatAtom::VsDistrict},    {"COMBAT_TYPE", CombatAtom::CombatType},
            {"TILE_HILLS", CombatAtom::TileHills},      {"TILE_FEATURE", CombatAtom::TileFeature},
            {"TILE_TERRAIN", CombatAtom::TileTerrain},  {"OPPONENT_FORTIFIED", CombatAtom::OpponentFortified},
            {"OPPONENT_WOUNDED", CombatAtom::OpponentWounded}, {"DISTRICT_TILE", CombatAtom::DistrictTile},
            {"OWN_TERRITORY", CombatAtom::OwnTerritory},
            {"ADJACENT_SAME_UNIT", CombatAtom::AdjacentSameUnit}, {"OPPONENT_TILE_BASE", CombatAtom::OpponentTileBase},
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
            fx.at = e["at"].str();
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
        const std::string domain = j["domain"].str("LAND");
        u.domain = domain == "SEA" ? Domain::Sea : domain == "AIR" ? Domain::Air : Domain::Land;
        const std::string layer = j["layer"].str("MILITARY");
        u.layer = layer == "CIVILIAN" ? UnitLayer::Civilian
                  : layer == "SUPPORT" ? UnitLayer::Support
                  : layer == "LEADER"  ? UnitLayer::Leader
                                       : UnitLayer::Military;
        if (u.domain == Domain::Air) u.layer = UnitLayer::Air;  // aircraft never share a plot's layers
        u.antiAir = static_cast<int>(j["antiAir"].integer(0));
        u.airSlots = static_cast<int>(j["airSlots"].integer(0));
        u.deliversWmd = j["deliversWmd"].boolean(false);
        u.excavations = static_cast<int>(j["excavations"].integer(0));
        u.wmdImmune = j["wmdImmune"].boolean(false);
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
        u.spy = j["spy"].boolean(false);
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
        u.uniqueToId = j["uniqueTo"].str();
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
            const std::string baseId = j["base"].str();
            if (!units[i].uniqueToId.empty() && (units[i].replaces = unit(baseId)) == kNone) {
                *error = "unit " + id + ": a unique unit needs the base it replaces";
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
            im.appeal = static_cast<int>(j["appeal"].integer(0));
            im.uniqueToId = j["uniqueTo"].str();
            im.amenities = static_cast<int>(j["amenities"].integer(0));
            im.defense = static_cast<int>(j["defense"].integer(0));
            im.sight = static_cast<int>(j["sight"].integer(0));
            im.borderOnly = j["borderOnly"].boolean(false);
            im.needsRiver = j["needsRiver"].boolean(false);
            im.halvesFloods = j["halvesFloods"].boolean(false);
            im.powerProvided = static_cast<int>(j["powerProvided"].integer(0));
            if (j.has("tourism")) {
                im.tourismSource = j["tourism"]["source"].str();
                im.tourismPercent = static_cast<int>(j["tourism"]["percent"].integer(100));
                if (j["tourism"].has("after") && !readUnlock(j["tourism"]["after"], im.tourismAfter, "improvement " + id)) return false;
            }
            im.minAppeal = j.has("minAppeal") ? static_cast<int>(j["minAppeal"].integer(0)) : -100;
            im.coastal = j["coastal"].boolean(false);
            im.airSlots = static_cast<int>(j["airSlots"].integer(0));
            im.tunnel = j["tunnel"].boolean(false);
            im.plunder = readPlunder(j["plunder"]);
            im.builtById = j["builtBy"].str();
            const Json& adj = j["adjacentImprovementYield"];
            if (adj.isObject()) {
                im.adjacentImprovementId = adj["improvement"].str();
                parseYieldName(adj["yield"].str(), im.adjacentYield);
                im.adjacentAmount = static_cast<int>(adj["amount"].integer(0));
            }
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
            b.citizenSlots = static_cast<int>(j["citizenSlots"].integer(0));
            b.specialistYields = readYields(j["specialistYields"]);
            b.amenities = static_cast<int>(j["amenities"].integer(0));
            b.outerDefenseHp = static_cast<int>(j["outerDefenseHp"].integer(0));
            b.airSlots = static_cast<int>(j["airSlots"].integer(0));
            b.uniqueToId = j["uniqueTo"].str();
            b.replacesId = j["base"].str();
            const Json& adj = j["adjacentImprovementYield"];
            if (adj.isObject()) {
                b.adjacentImprovementId = adj["improvement"].str();
                parseYieldName(adj["yield"].str(), b.adjacentYield);
                b.adjacentAmount = static_cast<int>(adj["amount"].integer(0));
            }
            b.goldPerTradeRoute = static_cast<int>(j["goldPerTradeRoute"].integer(0));
            b.envoysOnBuild = static_cast<int>(j["envoysOnBuild"].integer(0));
            b.trainedXpPercent = static_cast<int>(j["trainedXpPercent"].integer(0));
            b.foodPerAdjacentMountain = static_cast<int>(j["foodPerAdjacentMountain"].integer(0));
            b.requiredPower = static_cast<int>(j["requiredPower"].integer(0));
            b.poweredYields = readYields(j["poweredYields"]);
            b.poweredAmenities = static_cast<int>(j["poweredAmenities"].integer(0));
            if (j["burns"].isObject()) {
                b.burnsResource = resource(j["burns"]["resource"].str());
                b.powerPerResource = static_cast<int>(j["burns"]["power"].integer(0));
            }
            b.powerProvided = static_cast<int>(j["powerProvided"].integer(0));
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
        const std::string progression = j["costProgression"].str("NO_COST_PROGRESSION");
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
        d.housing = static_cast<int>(j["housing"].integer(0));
        d.amenities = static_cast<int>(j["amenities"].integer(0));
        d.airSlots = static_cast<int>(j["airSlots"].integer(0));
        d.plunder = readPlunder(j["plunder"]);
        d.specialistYields = readYields(j["specialistYields"]);
        d.canal = j["canal"].boolean(false);
        d.appeal = static_cast<int>(j["appeal"].integer(0));
        for (const Json& band : j["appealHousing"].items()) {
            if (band.items().size() == 2) d.appealHousing.push_back({static_cast<int>(band.items()[0].integer(0)), static_cast<int>(band.items()[1].integer(0))});
        }
        d.aqueduct = j["aqueduct"].boolean(false);
        d.onePerPlayer = j["onePerPlayer"].boolean(false);
        d.floodplainsRiver = j["floodplainsRiver"].boolean(false);
        d.preventsDrought = j["preventsDrought"].boolean(false);
        d.preventsFloods = j["preventsFloods"].boolean(false);
        for (const Json& x : j["exclusiveWith"].items()) d.exclusiveIds.push_back(x.str());
        for (const Json& x : j["validTerrains"].items()) {
            const TypeIndex t = terrain(x.str());
            if (t == kNone) {
                if (error) *error = "district " + id + ": unknown terrain " + x.str();
                return false;
            }
            d.validTerrains.push_back(t);
        }
        districts.push_back(std::move(d));
    }
    for (DistrictType& d : districts) {
        for (const std::string& x : d.exclusiveIds) {
            const TypeIndex o = district(x);
            if (o == kNone) {
                if (error) *error = "district " + d.id + ": unknown exclusiveWith " + x;
                return false;
            }
            d.exclusiveWith.push_back(o);
        }
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
        g.favor = static_cast<int>(j["favor"].integer(0));
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
    // Civ, leader and heir abilities share one reader (leaders-and-art-style).
    auto readAbility = [&](const Json& a, CivAbility& ab, const std::string& where) -> bool {
        ab.name = a["name"].str();
        for (const Json& x : a["extraAdjacency"].items()) {
            CivAdjacency adj;
            adj.district = district(x["district"].str());
            adj.from = x["from"].str().empty() ? kNone : district(x["from"].str());
            adj.fromTerrainBase = x["fromTerrainBase"].str();
            adj.per = std::max(1, static_cast<int>(x["per"].integer(1)));
            parseYieldName(x["yield"].str(), adj.yield);
            adj.amount = static_cast<int>(x["amount"].integer(0));
            if (adj.district == kNone || (adj.from == kNone && adj.fromTerrainBase.empty())) {
                *error = where + ": bad extra adjacency";
                return false;
            }
            ab.extraAdjacency.push_back(adj);
        }
        ab.wonderProductionPercent = static_cast<int>(a["wonderProductionPercent"].integer(0));
        if (a["wonderEras"].items().size() == 2) {
            ab.wonderEraMin = static_cast<int>(a["wonderEras"].items()[0].integer(0));
            ab.wonderEraMax = static_cast<int>(a["wonderEras"].items()[1].integer(0));
        }
        ab.amenityPerWonder = static_cast<int>(a["amenityPerWonder"].integer(0));
        ab.foundPopulation = static_cast<int>(a["foundPopulation"].integer(0));
        if (!a["foundBuilding"].str().empty()) ab.foundBuilding = building(a["foundBuilding"].str());
        ab.culturePerSuzerainty = static_cast<int>(a["culturePerSuzerainty"].integer(0));
        ab.governorLoyalty = static_cast<int>(a["governorLoyalty"].integer(0));
        ab.governorGold = static_cast<int>(a["governorGold"].integer(0));
        if (!a["extraGovernorTitleCivic"].str().empty()) ab.extraGovernorTitleCivic = civic(a["extraGovernorTitleCivic"].str());
        ab.desertRouteGold = static_cast<int>(a["desertRouteGold"].integer(0));
        ab.capitalYieldsPerGovernorTitle = readYields(a["capitalYieldsPerGovernorTitle"]);
        ab.freshWaterFarmHousing = a["freshWaterFarmHousing"].fixed();
        ab.mountainDistrictProductionPercent = static_cast<int>(a["mountainDistrictProductionPercent"].integer(0));
        ab.mountainProduction = static_cast<int>(a["mountainProduction"].integer(0));
        for (const auto& [dom, pct] : a["domainProductionPercent"].members()) {
            const size_t k = dom == "SEA" ? 1 : dom == "AIR" ? 2 : 0;
            ab.domainProductionPercent[k] = static_cast<int>(pct.integer(0));
        }
        for (const auto& [cls, pct] : a["greatPersonPercent"].members()) ab.greatPersonPercent.push_back({greatPersonClass(cls), static_cast<int>(pct.integer(0))});
        ab.intercontinentalRouteGold = static_cast<int>(a["intercontinentalRouteGold"].integer(0));
        ab.internationalRouteYields = readYields(a["internationalRouteYields"]);
        for (const Json& x : a["districtBuildingYields"].items()) ab.districtBuildingYields.push_back({district(x["district"].str()), readYields(x["yields"])});
        for (const Json& x : a["districtBuildingAmenities"].items()) ab.districtBuildingAmenities.push_back({district(x["district"].str()), static_cast<int>(x["amount"].integer(0))});
        for (const Json& x : a["strengthNearLeader"].items())
            ab.strengthNearLeader.push_back({x["class"].str(), static_cast<int>(x["amount"].integer(0)), static_cast<int>(x["range"].integer(0))});
        ab.cityCenterBuildingProductionPercent = static_cast<int>(a["cityCenterBuildingProductionPercent"].integer(0));
        ab.wallProductionPercent = static_cast<int>(a["wallProductionPercent"].integer(0));
        ab.governorAmenity = static_cast<int>(a["governorAmenity"].integer(0));
        for (const Json& x : a["grantAbilities"].items()) {
            const TypeIndex g = ability(x.str());
            if (g == kNone) {
                *error = where + ": unknown ability " + x.str();
                return false;
            }
            ab.grantAbilities.push_back(g);
        }
        ab.capturedCityLoyalty = static_cast<int>(a["capturedCityLoyalty"].integer(0));
        ab.foreignReligionAmenity = static_cast<int>(a["foreignReligionAmenity"].integer(0));
        ab.nearFollowingCityStrength = static_cast<int>(a["strengthNearFollowingCity"]["amount"].integer(0));
        ab.nearFollowingCityRange = static_cast<int>(a["strengthNearFollowingCity"]["range"].integer(0));
        ab.extraBuilderCharges = static_cast<int>(a["extraBuilderCharges"].integer(0));
        ab.peaceYieldPercent = readYields(a["peaceYieldPercent"]);
        ab.wonderCulture = static_cast<int>(a["wonderCulture"].integer(0));
        if (!a["faithPurchaseDistrict"].str().empty()) ab.faithPurchaseDistrict = district(a["faithPurchaseDistrict"].str());
        ab.killFaithPercent = static_cast<int>(a["killFaithPercent"].integer(0));
        ab.capitalAmenityPerKills = static_cast<int>(a["capitalAmenityPerKills"].integer(0));
        ab.capitalAmenityMax = static_cast<int>(a["capitalAmenityMax"].integer(0));
        ab.mountainCityHousing = static_cast<int>(a["mountainCityHousing"].integer(0));
        ab.capitalYields = readYields(a["capitalYields"]);
        ab.cityLoyalty = static_cast<int>(a["cityLoyalty"].integer(0));
        ab.unitXpPercent = static_cast<int>(a["unitXpPercent"].integer(0));
        for (const Json& x : a["classStrength"].items()) ab.classStrength.push_back({x["class"].str(), static_cast<int>(x["amount"].integer(0))});
        return true;
    };
    for (const auto& [id, j] : m.tables["civilizations"]) {
        CivType c;
        c.id = id;
        c.name = j["name"].str(id);
        c.leader = j["leader"].str();
        for (const Json& n : j["cityNames"].items()) c.cityNames.push_back(n.str());
        c.agendaId = j["agenda"].str();
        c.agendaName = j["agendaName"].str();
        c.agendaText = j["agendaText"].str();
        c.leaning = j["leaning"].str();
        c.voice = j["voice"].str();
        if (!readAbility(j["ability"], c.ability, "civilization " + id) || !readAbility(j["leaderAbility"], c.leaderAbility, "civilization " + id))
            return false;
        c.combined = combineAbilities(c.ability, c.leaderAbility);
        static const char* const agendas[] = {"", "AGENDA_QUEEN_OF_THE_SEAS", "AGENDA_DEFENDER_OF_THE_FAITH", "AGENDA_PAX_ROMANA",
                                              "AGENDA_SPARTAN_PRIDE", "AGENDA_TOLERANT_CONQUEROR", "AGENDA_MAGNANIMOUS",
                                              "AGENDA_FIRST_EMPEROR", "AGENDA_CLOSED_COUNTRY", "AGENDA_ETERNAL_NAME",
                                              "AGENDA_PATRON_OF_TRADE", "AGENDA_HONOURABLE_WAR", "AGENDA_SAPA_INCA"};
        for (size_t a = 1; a < sizeof(agendas) / sizeof(agendas[0]); ++a) {
            if (c.agendaId == agendas[a]) c.agenda = static_cast<Agenda>(a);
        }
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
        // Heirs' traits: one per heir after the starting leader.
        d.traits.assign(d.names.size(), CivAbility{});
        const std::vector<Json>& traits = j["traits"].items();
        for (size_t i = 0; i < traits.size() && i + 1 < d.names.size(); ++i) {
            if (!readAbility(traits[i], d.traits[i + 1], "dynasty " + id)) return false;
        }
        for (const CivAbility& t : d.traits) d.combined.push_back(combineAbilities(civs[static_cast<size_t>(d.civ)].combined, t));
        dynasties.push_back(std::move(d));
    }
    // Historic moments (09).
    for (const auto& [id, j] : m.tables["moments"]) {
        MomentType mo;
        mo.id = id;
        mo.name = j["name"].str(id);
        mo.eraScore = static_cast<int>(j["eraScore"].integer(0));
        mo.obsoleteEra = j.has("obsoleteEra") ? era(j["obsoleteEra"].str()) : -1;
        moments.push_back(std::move(mo));
    }
    for (const auto& [id, j] : m.tables["rockBandResults"]) {
        RockBandResult rb;
        rb.id = id;
        rb.name = j["name"].str(id);
        rb.albumSales = static_cast<int>(j["albumSales"].integer(0));
        rb.tourismBomb = static_cast<int>(j["tourismBomb"].integer(0));
        rb.probability = static_cast<int>(j["probability"].integer(0));
        rb.dies = j["dies"].boolean(false);
        rb.gainsLevel = j["gainsLevel"].boolean(false);
        rb.extraPromotion = j["extraPromotion"].boolean(false);
        rockBandResults.push_back(std::move(rb));
    }
    for (const auto& [id, j] : m.tables["dedications"]) {
        DedicationType d;
        d.id = id;
        d.name = j["name"].str(id);
        d.eraMin = j.has("eraMin") ? era(j["eraMin"].str()) : 0;
        d.eraMax = j.has("eraMax") ? era(j["eraMax"].str()) : -1;
        dedications.push_back(std::move(d));
    }
    // Governors and their promotion trees (08: Governors).
    for (const auto& [id, j] : m.tables["governors"]) {
        GovernorType g;
        g.id = id;
        g.name = j["name"].str(id);
        g.title = j["title"].str();
        g.establishPercent = std::max(1, static_cast<int>(j["establishPercent"].integer(100)));
        g.loyalty = static_cast<int>(j["loyalty"].integer(8));
        g.cityStates = j["cityStates"].boolean(false);
        const TypeIndex gi = static_cast<TypeIndex>(governors.size());
        std::vector<std::vector<std::string>> needs;
        for (const Json& pj : j["promotions"].items()) {
            GovernorPromotionType p;
            p.id = pj["id"].str();
            p.name = pj["name"].str(p.id);
            p.effects = pj["effects"].str();
            p.governor = gi;
            p.tier = static_cast<int>(pj["tier"].integer(0));
            p.column = static_cast<int>(pj["column"].integer(0));
            p.base = pj["base"].boolean(false);
            std::vector<std::string> req;
            for (const Json& r : pj["requires"].items()) req.push_back(r.str());
            needs.push_back(req);
            g.promotions.push_back(static_cast<TypeIndex>(governorPromotions.size()));
            governorPromotions.push_back(std::move(p));
        }
        // The base ability first.
        std::stable_sort(g.promotions.begin(), g.promotions.end(),
                         [&](TypeIndex a, TypeIndex b) { return governorPromotions[static_cast<size_t>(a)].base && !governorPromotions[static_cast<size_t>(b)].base; });
        for (size_t k = 0; k < needs.size(); ++k) {
            GovernorPromotionType& p = governorPromotions[governorPromotions.size() - needs.size() + k];
            for (const std::string& r : needs[k]) {
                const TypeIndex ri = governorPromotion(r);
                if (ri == kNone) {
                    *error = "governor promotion " + p.id + ": unknown requirement " + r;
                    return false;
                }
                p.prerequisites.push_back(ri);
            }
        }
        governors.push_back(std::move(g));
    }
    for (const auto& [id, j] : m.tables["disasters"]) {
        DisasterType d;
        d.id = id;
        d.name = j["name"].str(id);
        static const std::pair<const char*, DisasterKind> kinds[] = {
            {"FLOOD", DisasterKind::Flood},   {"ERUPTION", DisasterKind::Eruption}, {"BLIZZARD", DisasterKind::Blizzard}, {"DUST_STORM", DisasterKind::DustStorm},
            {"TORNADO", DisasterKind::Tornado}, {"HURRICANE", DisasterKind::Hurricane}, {"DROUGHT", DisasterKind::Drought}, {"FIRE", DisasterKind::Fire}};
        for (const auto& [k, v] : kinds) {
            if (j["kind"].str() == k) d.kind = v;
        }
        d.severity = static_cast<int>(j["severity"].integer(0));
        d.hexes = static_cast<int>(j["hexes"].integer(0));
        d.duration = static_cast<int>(j["duration"].integer(0));
        d.chancePerDegree = static_cast<int>(j["chancePerDegree"].integer(0));
        static const char* const levels[] = {"MINIMAL", "LIGHT", "MODERATE", "HEAVY", "HYPERREAL"};
        for (int i = 0; i < kNumDisasterIntensities; ++i) d.frequencyTenths[static_cast<size_t>(i)] = static_cast<int>((j["frequency"][levels[i]].fixed() * 10).toInt());
        static const std::pair<const char*, DisasterDamageType> damages[] = {
            {"IMPROVEMENT_DESTROYED", DisasterDamageType::ImprovementDestroyed}, {"IMPROVEMENT_PILLAGED", DisasterDamageType::ImprovementPillaged},
            {"POPULATION_LOSS", DisasterDamageType::PopulationLoss},             {"UNIT_KILLED_CIVILIAN", DisasterDamageType::CivilianKilled},
            {"UNIT_DAMAGE_LAND", DisasterDamageType::UnitDamageLand},           {"UNIT_DAMAGE_NAVAL", DisasterDamageType::UnitDamageNaval},
            {"SPECIFIC_IMPROVEMENT_DESTROYED", DisasterDamageType::ImprovementDestroyed}, {"SPECIFIC_IMPROVEMENT_PILLAGED", DisasterDamageType::ImprovementPillaged},
            {"CITY_GARRISON", DisasterDamageType::CityGarrison},                 {"CITY_WALLS", DisasterDamageType::CityWalls}};
        for (const Json& dj : j["damage"].items()) {
            DisasterDamage dd;
            for (const auto& [k, v] : damages) {
                if (dj["type"].str() == k) dd.type = v;
            }
            dd.percent = static_cast<int>(dj["percent"].integer(0));
            dd.minHp = static_cast<int>(dj["minHp"].integer(0));
            dd.maxHp = static_cast<int>(dj["maxHp"].integer(0));
            if (dd.type != DisasterDamageType::Other) d.damage.push_back(dd);
        }
        for (const Json& fj : j["fertility"].items()) {
            DisasterFertility f;
            if (!parseYieldName(fj["yield"].str(), f.yield)) continue;
            const std::string& feat = fj["feature"].str();
            f.feature = feat.empty() ? kNone : feature(feat);
            if (!feat.empty() && f.feature == kNone) continue;
            f.percent = static_cast<int>(fj["percent"].integer(0));
            f.amount = static_cast<int>(fj["amount"].integer(0));
            f.replaceFeature = fj["replaceFeature"].boolean(false);
            d.fertility.push_back(f);
        }
        disasters.push_back(std::move(d));
    }
    for (const auto& [id, j] : m.tables["climatePhases"]) {
        climatePhases.push_back({id, j["name"].str(id), static_cast<int>(j["points"].integer(0)), static_cast<int>(j["iceLoss"].integer(0)),
                                 static_cast<int>(j["fertilityRemoval"].integer(0))});
    }
    for (const auto& [id, j] : m.tables["disasterIntensities"]) {
        disasterIntensities.push_back({id, j["name"].str(id), static_cast<int>(j["activeVolcanoes"].integer(70)), static_cast<int>(j["extraRange"].integer(0))});
    }
    for (const auto& [id, j] : m.tables["resolutions"]) {
        ResolutionType rs;
        rs.id = id;
        rs.name = j["name"].str(id);
        rs.optionA = j["optionA"].str();
        rs.optionB = j["optionB"].str();
        static const std::pair<const char*, ResolutionKind> kinds[] = {
            {"RESOLUTION_DIPLOMATIC_VICTORY", ResolutionKind::DiplomaticVictory}, {"RESOLUTION_TRADE_POLICY", ResolutionKind::TradePolicy},
            {"RESOLUTION_PATRONAGE", ResolutionKind::Patronage},                   {"RESOLUTION_MIGRATION_TREATY", ResolutionKind::MigrationTreaty},
            {"RESOLUTION_PUBLIC_RELATIONS", ResolutionKind::PublicRelations},     {"RESOLUTION_MILITARY_ADVISORY", ResolutionKind::MilitaryAdvisory},
            {"RESOLUTION_URBAN_DEVELOPMENT_TREATY", ResolutionKind::UrbanDevelopment},
        };
        for (const auto& [rid, k] : kinds) {
            if (id == rid) rs.kind = k;
        }
        const std::string& t = j["target"].str();
        rs.target = t == "PLAYER" ? ResolutionTarget::Player : t == "GREATPERSONCLASS" ? ResolutionTarget::GreatPersonClass
                  : t == "DISTRICT" ? ResolutionTarget::District : t == "UNITPROMOTIONCLASS" ? ResolutionTarget::PromotionClass
                  : ResolutionTarget::Other;
        rs.minEra = j.has("minEra") ? era(j["minEra"].str()) : -1;
        rs.maxEra = j.has("maxEra") ? era(j["maxEra"].str()) : -1;
        resolutions.push_back(std::move(rs));
    }
    for (const UnitType& u : units) {
        if (!u.promotionClass.empty() && std::find(promotionClasses.begin(), promotionClasses.end(), u.promotionClass) == promotionClasses.end())
            promotionClasses.push_back(u.promotionClass);
    }
    for (const auto& [id, j] : m.tables["spyOperations"]) {
        SpyOperationType op;
        op.id = id;
        op.name = j["name"].str(id);
        op.turns = static_cast<int>(j["turns"].integer(8));
        op.base = static_cast<int>(j["base"].integer(0));
        op.levelChange = static_cast<int>(j["levelChange"].integer(1));
        op.enemyChange = static_cast<int>(j["enemyChange"].integer(0));
        op.enemyLevelChange = static_cast<int>(j["enemyLevelChange"].integer(0));
        op.needsDistrict = j.has("district");
        if (op.needsDistrict) op.district = district(j["district"].str());
        spyOperations.push_back(std::move(op));
    }
    for (const auto& [id, j] : m.tables["goodies"]) {
        static const std::pair<const char*, GoodyKind> kinds[] = {
            {"RELIC", GoodyKind::Relic}, {"INSPIRATION", GoodyKind::Inspiration}, {"EUREKA", GoodyKind::Eureka}, {"GOVERNOR_TITLE", GoodyKind::GovernorTitle},
            {"ENVOY", GoodyKind::Envoy}, {"FAVOR", GoodyKind::Favor}, {"FAITH", GoodyKind::Faith}, {"GOLD", GoodyKind::Gold}, {"XP", GoodyKind::Xp},
            {"HEAL", GoodyKind::Heal}, {"STRATEGIC", GoodyKind::Strategic}, {"TECH", GoodyKind::Tech}, {"POPULATION", GoodyKind::Population},
            {"UNIT", GoodyKind::Unit}};
        GoodyType g;
        g.id = id;
        g.category = j["category"].str();
        g.weight = static_cast<int>(j["weight"].integer(0));
        bool known = false;
        for (const auto& [name, kind] : kinds) {
            if (j["kind"].str() == name) {
                g.kind = kind;
                known = true;
            }
        }
        g.amount = static_cast<int>(j["amount"].integer(0));
        g.unit = j.has("unit") ? unit(j["unit"].str()) : kNone;
        g.minTurn = static_cast<int>(j["minTurn"].integer(0));
        g.needsCity = j["needsCity"].boolean(false);
        if (known && (g.kind != GoodyKind::Unit || g.unit != kNone)) goodies.push_back(std::move(g));
    }
    for (const auto& [id, j] : m.tables["spyPromotions"]) {
        SpyPromotionType sp;
        sp.id = id;
        sp.name = j["name"].str(id);
        sp.levels.assign(spyOperations.size(), 0);
        sp.faster.assign(spyOperations.size(), 0);
        for (const auto& [op, n] : j["levels"].members()) {
            const TypeIndex k = spyOperation(op);
            if (k != kNone) sp.levels[static_cast<size_t>(k)] = static_cast<int>(n.integer(0));
        }
        for (const auto& [op, n] : j["faster"].members()) {
            const TypeIndex k = spyOperation(op);
            if (k != kNone) sp.faster[static_cast<size_t>(k)] = static_cast<int>(n.integer(0));
        }
        sp.allLevels = static_cast<int>(j["allLevels"].integer(0));
        sp.escape = static_cast<int>(j["escape"].integer(0));
        sp.travelFaster = static_cast<int>(j["travelFaster"].integer(0));
        sp.counterspyLevels = static_cast<int>(j["counterspyLevels"].integer(0));
        spyPromotions.push_back(std::move(sp));
    }
    for (const auto& [id, j] : m.tables["governorTitles"]) {
        const TypeIndex c = civic(j["civic"].str());
        if (c == kNone) {
            *error = "governor titles " + id + ": unknown civic";
            return false;
        }
        governorTitleCivics.push_back({c, static_cast<int>(j["titles"].integer(1))});
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
        const std::string toward = j["toward"].str("UNITS");
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
        b.stockpileCap = static_cast<int>(j["stockpileCap"].integer(0));
        if (j.has("theming")) {
            const auto& t = j["theming"];
            BuildingType::Theming th;
            th.uniquePerson = t["uniquePerson"].boolean(false);
            th.sameObject = t["sameObject"].boolean(false);
            th.uniqueCivs = t["uniqueCivs"].boolean(false);
            th.sameEra = t["sameEra"].boolean(false);
            th.yieldPercent = static_cast<int>(t["yieldPercent"].integer(0));
            th.tourismPercent = static_cast<int>(t["tourismPercent"].integer(0));
            b.theming = th;
        }
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
        } else if ((mod.sourceIndex = governorPromotion(mod.source)) != kNone) {
            mod.sourceKind = ModSource::Governor;
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
        s.co2PerDegree = std::max<int64_t>(1, j["co2PerDegree"].integer(2000000));
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
    for (const auto& [id, row] : m.tables["difficulties"]) {
        const auto& j = row;
        DifficultyType d;
        d.id = id;
        d.name = j["name"].str(id);
        const auto num = [&](const char* k) { return static_cast<int>(j[k].integer(0)); };
        d.aiSkill = static_cast<int>(j["aiSkill"].integer(3));
        d.aiYieldPercent = num("aiYieldPercent");
        d.aiProductionGoldPercent = num("aiProductionGoldPercent");
        d.aiCombat = num("aiCombat");
        d.aiXpPercent = num("aiXpPercent");
        d.aiFreeBoosts = num("aiFreeBoosts");
        d.aiExtraWarriors = num("aiExtraWarriors");
        d.aiExtraBuilders = num("aiExtraBuilders");
        d.aiExtraSettlers = num("aiExtraSettlers");
        d.humanCombat = num("humanCombat");
        d.humanXpPercent = num("humanXpPercent");
        d.humanCampGoldPercent = num("humanCampGoldPercent");
        difficulties.push_back(std::move(d));
    }
    for (const auto& [id, j] : m.tables["startingUnits"]) {
        const std::string& u = j["unit"].str();
        if (unit(u) == kNone) {
            *error = "starting unit " + id + ": unknown unit " + u;
            return false;
        }
        startingUnits.push_back(u);
    }

    // The weapons the projects build (05: Nuclear weapons), before the projects.
    for (const auto& [id, j] : m.tables["wmds"]) {
        wmds.push_back({id, j["name"].str(id), static_cast<int>(j["blastRadius"].integer(1)), static_cast<int>(j["falloutTurns"].integer(10)),
                        static_cast<int>(j["icbmRange"].integer(12)), static_cast<int>(j["maintenance"].integer(0))});
    }
    for (const auto& [id, row] : m.tables["projects"]) {
        const Json& j = row;
        ProjectType pj;
        pj.id = id;
        pj.name = j["name"].str(id);
        pj.districtId = j["district"].str();
        if (!pj.districtId.empty()) pj.district = district(pj.districtId);
        if (!readUnlock(j["unlock"], pj.unlock, "project " + id)) return false;
        pj.cost = static_cast<int>(j["cost"].integer(0));
        const std::string model = j["costProgression"].str("NO_PROGRESSION_MODEL");
        pj.costProgression = model == "GAME_PROGRESS" ? DistrictCostProgression::GameProgress : DistrictCostProgression::None;
        pj.costProgressionParam = static_cast<int>(j["costProgressionParam"].integer(0));
        pj.maxPerPlayer = static_cast<int>(j["maxPerPlayer"].integer(0));
        pj.spaceRace = j["spaceRace"].boolean(false);
        if (j["conversion"].isObject() && parseYieldName(j["conversion"]["yield"].str(), pj.conversionYield)) {
            pj.converts = true;
            pj.conversionPercent = static_cast<int>(j["conversion"]["percent"].integer(0));
        }
        for (const auto& [cls, points] : j["greatPersonPoints"].members()) {
            const TypeIndex c = greatPersonClass(cls);
            if (c != kNone) pj.greatPersonPoints.push_back({c, static_cast<int>(points.integer(0))});
        }
        if (j["resourceCost"].isObject()) {
            pj.resource = resource(j["resourceCost"]["resource"].str());
            pj.resourceAmount = static_cast<int>(j["resourceCost"]["amount"].integer(0));
        }
        static const std::pair<const char*, ProjectEffectKind> kinds[] = {
            {"REPAIR_WALLS", ProjectEffectKind::RepairWalls}, {"LOYALTY", ProjectEffectKind::Loyalty}, {"FAVOR", ProjectEffectKind::Favor},
            {"REMOVE_CO2", ProjectEffectKind::RemoveCo2}, {"REVEAL_MAP", ProjectEffectKind::RevealMap},
            {"CULTURE_FROM_SCIENCE", ProjectEffectKind::CultureFromScience}, {"EXPEDITION_SPEED", ProjectEffectKind::ExpeditionSpeed},
            {"WMD", ProjectEffectKind::Wmd}, {"AID", ProjectEffectKind::Aid}, {"COMPETITION", ProjectEffectKind::Competition},
            {"DECOMMISSION", ProjectEffectKind::Decommission}, {"FESTIVAL", ProjectEffectKind::Festival}};
        bool known = true;
        for (const Json& e : j["effects"].items()) {
            bool found = false;
            for (const auto& [name, kind] : kinds) {
                if (e["kind"].str() == name) {
                    TypeIndex weapon = e.has("weapon") ? wmd(e["weapon"].str()) : kNone;
                    if (kind == ProjectEffectKind::Decommission) weapon = building(e["building"].str());
                    // CompetitionKind::WorldGames (1) and SpaceStation (6); state.h is out of reach here (checked in competitions.cpp).
                    if (kind == ProjectEffectKind::Competition) weapon = e["competition"].str() == "WORLD_GAMES" ? 1 : e["competition"].str() == "SPACE_STATION" ? 6 : kNone;
                    found = (kind != ProjectEffectKind::Wmd && kind != ProjectEffectKind::Decommission && kind != ProjectEffectKind::Competition) || weapon != kNone;
                    if (found) pj.effects.push_back({kind, static_cast<int>(e["amount"].integer(0)), weapon});
                }
            }
            known = known && found;
        }
        pj.modelled = j["modelled"].boolean(false) && known;
        projects.push_back(std::move(pj));
    }
    {
        size_t k = 0;
        for (const auto& [id, j] : m.tables["projects"]) {
            const std::string req = j["requires"].str();
            if (!req.empty() && (projects[k].prerequisite = project(req)) == kNone) {
                if (error) *error = "project " + id + ": unknown requirement " + req;
                return false;
            }
            ++k;
        }
    }
    // Civ uniques: buildings and improvements name their civ, base and neighbours by id.
    for (BuildingType& b : buildings) {
        if (!b.uniqueToId.empty() && ((b.uniqueTo = civ(b.uniqueToId)) == kNone || (b.replaces = building(b.replacesId)) == kNone)) {
            *error = "building " + b.id + ": unknown civilization or base";
            return false;
        }
        if (!b.adjacentImprovementId.empty() && (b.adjacentImprovement = improvement(b.adjacentImprovementId)) == kNone) {
            *error = "building " + b.id + ": unknown improvement " + b.adjacentImprovementId;
            return false;
        }
    }
    for (RouteType& rt : routes) {
        for (const auto& [res, n] : rt.resourceCostIds) {
            const TypeIndex r = resource(res);
            if (r == kNone) {
                *error = "route " + rt.id + ": unknown resource " + res;
                return false;
            }
            rt.resourceCost.push_back({r, n});
        }
    }
    for (ImprovementType& im : improvements) {
        if (!im.builtById.empty() && (im.builtBy = unit(im.builtById)) == kNone) {
            *error = "improvement " + im.id + ": unknown builder unit " + im.builtById;
            return false;
        }
        if (!im.uniqueToId.empty() && (im.uniqueTo = civ(im.uniqueToId)) == kNone) {
            *error = "improvement " + im.id + ": unknown civilization " + im.uniqueToId;
            return false;
        }
        if (!im.adjacentImprovementId.empty() && (im.adjacentImprovement = improvement(im.adjacentImprovementId)) == kNone) {
            *error = "improvement " + im.id + ": unknown improvement " + im.adjacentImprovementId;
            return false;
        }
    }
    for (UnitType& u : units) {
        if (u.uniqueToId.empty()) continue;
        u.uniqueTo = civ(u.uniqueToId);
        if (u.uniqueTo == kNone) {
            *error = "unit " + u.id + ": unknown civilization " + u.uniqueToId;
            return false;
        }
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
TypeIndex Rules::moment(const std::string& id) const { return findIn(moments, id); }
TypeIndex Rules::governor(const std::string& id) const { return findIn(governors, id); }
TypeIndex Rules::dedication(const std::string& id) const { return findIn(dedications, id); }
TypeIndex Rules::spyOperation(const std::string& id) const { return findIn(spyOperations, id); }
TypeIndex Rules::resolution(const std::string& id) const { return findIn(resolutions, id); }
TypeIndex Rules::project(const std::string& id) const { return findIn(projects, id); }
TypeIndex Rules::wmd(const std::string& id) const { return findIn(wmds, id); }
TypeIndex Rules::uniqueUnitFor(TypeIndex civ, TypeIndex base) const {
    if (civ == kNone || base == kNone) return kNone;
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i].uniqueTo == civ && units[i].replaces == base) return static_cast<TypeIndex>(i);
    }
    return kNone;
}
TypeIndex Rules::governorPromotion(const std::string& id) const { return findIn(governorPromotions, id); }

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
