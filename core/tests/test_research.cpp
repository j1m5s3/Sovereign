// Research and government (specs/civ6/04-tech-civics-government.md).
#include <algorithm>

#include "helpers.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::capitalScenario;
using sovtest::endTurns;
using sovtest::flatState;
using sovtest::rules;

namespace {
constexpr size_t P = static_cast<size_t>(YieldType::Production);
constexpr size_t S = static_cast<size_t>(YieldType::Science);

TypeIndex tech(const char* id) { return rules().tech(id); }
TypeIndex civic(const char* id) { return rules().civic(id); }
TypeIndex gov(const char* id) { return rules().government(id); }
TypeIndex policy(const char* id) { return rules().policy(id); }
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
ProductionItem warrior() { return {ProductionKind::Unit, rules().unit("UNIT_WARRIOR")}; }

// A capital with a Warrior in production and Pottery and Code of Laws in
// progress, so turns can end. `edit` adjusts the state before play resumes.
template <typename Edit>
std::unique_ptr<Game> capitalWith(Edit edit, GameState base = flatState(20, 14, 1)) {
    auto sc = capitalScenario(std::move(base));
    GameState s = sc.game->state();
    s.cities[0].queue = {warrior()};
    s.players[0].techs.current = tech("TECH_POTTERY");
    s.players[0].civics.current = civic("CIVIC_CODE_OF_LAWS");
    edit(s);
    return Game::fromScenario(rules(), std::move(s));
}

// Chiefdom adopted after Code of Laws, with the free change window open.
void chiefdom(GameState& s) {
    Player& p = s.players[0];
    p.civics.done[at(civic("CIVIC_CODE_OF_LAWS"))] = 1;
    p.civics.current = civic("CIVIC_CRAFTSMANSHIP");
    p.government = gov("GOVERNMENT_CHIEFDOM");
    p.governmentUses[at(p.government)] = 1;
    p.policies.assign(2, kNone);
    p.freeChanges = true;
}
}  // namespace

TEST(research_data_from_civ_tables) {
    const Rules& r = rules();
    CHECK_EQ(r.eras.size(), 9u);
    CHECK_EQ(r.techs.size(), 77u);
    CHECK_EQ(r.civics.size(), 61u);
    const TreeNode& pottery = r.techs[at(tech("TECH_POTTERY"))];
    CHECK_EQ(pottery.cost, 25);
    CHECK_EQ(pottery.era, 0);
    CHECK_EQ(pottery.boost.percent, 0);
    const TreeNode& bronze = r.techs[at(tech("TECH_BRONZE_WORKING"))];
    REQUIRE(bronze.prereqs.size() == 1u);
    CHECK_EQ(bronze.prereqs[0], tech("TECH_MINING"));
    const Boost& sailing = r.techs[at(tech("TECH_SAILING"))].boost;
    CHECK_EQ(sailing.percent, 40);
    CHECK(sailing.kind == BoostKind::CoastalCity);
    const Boost& construction = r.techs[at(tech("TECH_CONSTRUCTION"))].boost;
    CHECK(construction.kind == BoostKind::Building);
    CHECK_EQ(construction.ref, r.building("BUILDING_WATER_MILL"));
    CHECK(r.techs[at(tech("TECH_WRITING"))].boost.kind == BoostKind::MetCivs);
    const Boost& buttress = r.techs[at(tech("TECH_BUTTRESS"))].boost;  // a wonder of the era before its own or later
    CHECK(buttress.kind == BoostKind::WonderFromEra);
    CHECK_EQ(buttress.count, static_cast<int>(r.era("ERA_CLASSICAL")));
    const Boost& steel = r.techs[at(tech("TECH_STEEL"))].boost;  // an Ironclad and a Coal Mine
    CHECK(steel.kind == BoostKind::UnitAndImprovement);
    CHECK_EQ(steel.ref, r.unit("UNIT_IRONCLAD"));
    CHECK_EQ(steel.improvement, r.improvement("IMPROVEMENT_MINE"));
    CHECK_EQ(steel.resource, r.resource("RESOURCE_COAL"));
    const Boost& archery = r.techs[at(tech("TECH_ARCHERY"))].boost;  // events: a kill with a Slinger
    CHECK(archery.kind == BoostKind::KillWith);
    CHECK_EQ(archery.ref, r.unit("UNIT_SLINGER"));
    const Boost& guidance = r.techs[at(tech("TECH_GUIDANCE_SYSTEMS"))].boost;  // a Fighter killed
    CHECK(guidance.kind == BoostKind::KillUnit);
    CHECK_EQ(guidance.ref, r.unit("UNIT_FIGHTER"));
    // Every boost with a condition can fire.
    std::vector<std::string> untracked;
    for (const auto* tree : {&r.techs, &r.civics}) {
        for (const TreeNode& n : *tree) {
            if (n.boost.percent > 0 && n.boost.kind == BoostKind::NotTracked) untracked.push_back(n.id);
        }
    }
    CHECK(untracked.empty());
    const Boost& empire = r.civics[at(civic("CIVIC_EARLY_EMPIRE"))].boost;
    CHECK(empire.kind == BoostKind::TotalPopulation);
    CHECK_EQ(empire.count, 6);
    const Boost& nfg = r.civics[at(civic("CIVIC_NEAR_FUTURE_GOVERNANCE"))].boost;
    CHECK_EQ(nfg.percent, 90);
    CHECK(nfg.kind == BoostKind::GovernmentTier);
    CHECK(!r.techs[at(tech("TECH_FUTURE_TECH"))].prereqs.empty());  // Sovereign: needs the Information era

    const Unlock archer = r.units[at(r.unit("UNIT_ARCHER"))].unlock;
    CHECK(!archer.civic);
    CHECK_EQ(archer.index, tech("TECH_ARCHERY"));
    const Unlock trader = r.units[at(r.unit("UNIT_TRADER"))].unlock;
    CHECK(trader.civic);
    CHECK_EQ(trader.index, civic("CIVIC_FOREIGN_TRADE"));
    CHECK(r.units[at(r.unit("UNIT_WARRIOR"))].unlock.none());

    const GovernmentType& chief = r.governments[at(gov("GOVERNMENT_CHIEFDOM"))];
    CHECK(chief.unlock.none());
    CHECK_EQ(chief.totalSlots(), 2);
    CHECK_EQ(Game::slotType(chief, 0) == PolicySlot::Military, true);
    CHECK_EQ(Game::slotType(chief, 1) == PolicySlot::Economic, true);
    const GovernmentType& autocracy = r.governments[at(gov("GOVERNMENT_AUTOCRACY"))];
    CHECK(autocracy.unlock.civic);
    CHECK_EQ(autocracy.unlock.index, civic("CIVIC_POLITICAL_PHILOSOPHY"));
    CHECK_EQ(autocracy.tier, 1);
    CHECK_EQ(Game::slotType(autocracy, 3) == PolicySlot::Wildcard, true);
    const PolicyType& agoge = r.policies[at(policy("POLICY_AGOGE"))];
    CHECK(agoge.slot == PolicySlot::Military);
    CHECK_EQ(agoge.unlock.index, civic("CIVIC_CRAFTSMANSHIP"));
    REQUIRE(agoge.obsoletedBy.size() == 1u);
    CHECK_EQ(agoge.obsoletedBy[0], policy("POLICY_FEUDAL_CONTRACT"));
    CHECK(r.policies[at(policy("POLICY_AUTOCRATIC_LEGACY"))].unlock.none());

    // Costs scale with game speed.
    GameState s = flatState(10, 10, 1);
    s.setup.speed = "GAMESPEED_ONLINE";
    auto g = Game::fromScenario(r, s);
    CHECK_EQ(g->techCost(tech("TECH_POTTERY")), 12);
    CHECK_EQ(g->civicCost(civic("CIVIC_CODE_OF_LAWS")), 10);
}

TEST(research_needs_choices_and_carries_overflow) {
    auto sc = capitalScenario();
    Game& g = *sc.game;
    REQUIRE(g.submit(Command::setProduction(0, sc.city, warrior())) == CommandError::Ok);
    CHECK_EQ(g.submit(Command::endTurn(0)), CommandError::ResearchNeeded);
    CHECK_EQ(g.submit(Command::chooseResearch(0, tech("TECH_IRRIGATION"))), CommandError::CannotResearch);  // needs Pottery
    CHECK_EQ(g.submit(Command::chooseCivic(0, civic("CIVIC_CRAFTSMANSHIP"))), CommandError::CannotResearch);
    REQUIRE(g.submit(Command::chooseResearch(0, tech("TECH_POTTERY"))) == CommandError::Ok);
    CHECK_EQ(g.submit(Command::endTurn(0)), CommandError::CivicNeeded);
    REQUIRE(g.submit(Command::chooseCivic(0, civic("CIVIC_CODE_OF_LAWS"))) == CommandError::Ok);

    GameState s = g.state();
    s.players[0].techs.progress[at(tech("TECH_POTTERY"))] = Fixed::fromInt(24);
    auto g2 = Game::fromScenario(rules(), s);
    const Fixed science = g2->sciencePerTurn(0);
    CHECK(science > Fixed());
    REQUIRE(g2->submit(Command::endTurn(0)) == CommandError::Ok);
    const Player& p = g2->state().players[0];
    CHECK(p.techs.has(tech("TECH_POTTERY")));
    CHECK_EQ(p.techs.current, kNone);
    const Fixed overflow = Fixed::fromInt(24) + science - Fixed::fromInt(25);
    CHECK_EQ(p.techs.overflow, overflow);
    CHECK_EQ(g2->submit(Command::endTurn(0)), CommandError::ResearchNeeded);
    REQUIRE(g2->submit(Command::chooseResearch(0, tech("TECH_IRRIGATION"))) == CommandError::Ok);  // Pottery done now
    const Fixed science2 = g2->sciencePerTurn(0);
    REQUIRE(g2->submit(Command::endTurn(0)) == CommandError::Ok);
    const Player& p2 = g2->state().players[0];
    CHECK_EQ(p2.techs.progress[at(tech("TECH_IRRIGATION"))], overflow + science2);
    CHECK_EQ(p2.techs.overflow, Fixed());
    // Switching keeps what was put in.
    REQUIRE(g2->submit(Command::chooseResearch(0, tech("TECH_MINING"))) == CommandError::Ok);
    CHECK_EQ(g2->state().players[0].techs.progress[at(tech("TECH_IRRIGATION"))], overflow + science2);
}

TEST(research_boosts_fire_from_state) {
    // A coastal capital boosts Sailing by 40% of 50 the moment it is founded.
    GameState coast = flatState(20, 14, 1);
    coast.plot({7, 6}).terrain = rules().terrain("TERRAIN_COAST");
    auto sc = capitalScenario(coast);
    const Player& p = sc.game->state().players[0];
    CHECK_EQ(p.techs.boosted[at(tech("TECH_SAILING"))], 1);
    CHECK_EQ(p.techs.progress[at(tech("TECH_SAILING"))], Fixed::fromInt(20));
    CHECK_EQ(p.techs.boosted[at(tech("TECH_ASTROLOGY"))], 0);

    // Six citizens boost Early Empire; a Water Mill boosts Construction.
    auto g = capitalWith([](GameState& s) {
        s.cities[0].population = 6;
        s.cities[0].buildings.push_back(rules().building("BUILDING_WATER_MILL"));
        std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
    });
    const Player& q = g->state().players[0];
    CHECK_EQ(q.civics.boosted[at(civic("CIVIC_EARLY_EMPIRE"))], 1);
    CHECK_EQ(q.civics.progress[at(civic("CIVIC_EARLY_EMPIRE"))], Fixed::fromInt(28));
    // 40% of its cost, a Classical tech 20% dearer while the world is Ancient (04: [GS] world era).
    CHECK_EQ(q.techs.progress[at(tech("TECH_CONSTRUCTION"))], Fixed::fromInt(g->techCost(tech("TECH_CONSTRUCTION"))) * 40 / 100);
    CHECK_EQ(q.techs.boosted[at(tech("TECH_SAILING"))], 0);  // inland
    // A boost is earned once.
    endTurns(*g, 1);
    CHECK_EQ(g->state().players[0].techs.progress[at(tech("TECH_CONSTRUCTION"))], Fixed::fromInt(g->techCost(tech("TECH_CONSTRUCTION"))) * 40 / 100);
}

TEST(a_civ_s_unique_building_counts_toward_boosts) {
    // Guilds: two Markets; Rome's Forum is its Market.
    const auto met = [](int forums) {
        GameState s = flatState(20, 14, 1);
        Game::fitPlayerToRules(s.players[0], rules());
        sovtest::addCity(s, 0, {4, 5}, true, 3);
        sovtest::addCity(s, 0, {12, 5}, false, 3);
        for (int i = 0; i < forums; ++i) s.cities[static_cast<size_t>(i)].buildings.push_back(rules().building("BUILDING_FORUM"));
        auto g = Game::fromScenario(rules(), std::move(s));
        return g->boostMet(0, rules().civics[at(civic("CIVIC_GUILDS"))].boost);
    };
    CHECK(!met(1));
    CHECK(met(2));
}

// Two cities of player 0, one of player 1, and three city-states (players 2-4) that own nothing.
GameState boostState() {
    GameState s = flatState(30, 14, 5);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(s.players.size(), 0);
        p.relations.resize(s.players.size());
    }
    for (PlayerId cs = 2; cs < 5; ++cs) s.players[at(cs)].cityState = static_cast<TypeIndex>(cs - 2);
    for (Plot& p : s.plots) p.continent = 0;
    sovtest::addCity(s, 0, {4, 5}, true, 3);
    sovtest::addCity(s, 0, {12, 5}, false, 3);
    sovtest::addCity(s, 1, {22, 5}, true, 3);
    return s;
}

bool boostIn(const GameState& s, const Boost& b) { return Game::fromScenario(rules(), s)->boostMet(0, b); }
const Boost& techBoost(const char* id) { return rules().techs[at(tech(id))].boost; }
const Boost& civicBoost(const char* id) { return rules().civics[at(civic(id))].boost; }

TEST(a_civ_s_unique_unit_counts_toward_boosts) {
    // Metal Casting: two Crossbowmen; China's Repeating Crossbow is its Crossbowman.
    GameState s = boostState();
    sovtest::addUnit(s, "UNIT_CROSSBOWMAN", 0, {6, 8});
    CHECK(!boostIn(s, techBoost("TECH_METAL_CASTING")));
    sovtest::addUnit(s, "UNIT_REPEATING_CROSSBOW", 0, {7, 8});
    CHECK(boostIn(s, techBoost("TECH_METAL_CASTING")));
}

TEST(boosts_from_improved_plots) {
    // 04: Apprenticeship (3 Mines), Craftsmanship (3 improved tiles), the Wheel (a Mine on a resource it works) and Iron
    // Working (an improved Iron). A rival's plots count for nothing.
    GameState s = boostState();
    const TypeIndex mine = rules().improvement("IMPROVEMENT_MINE"), iron = rules().resource("RESOURCE_IRON");
    s.plot({3, 5}).improvement = mine;
    s.plot({5, 5}).improvement = mine;
    s.plot({21, 5}).improvement = mine;  // player 1's
    CHECK(!boostIn(s, techBoost("TECH_APPRENTICESHIP")));
    CHECK(!boostIn(s, civicBoost("CIVIC_CRAFTSMANSHIP")));
    s.plot({11, 5}).improvement = rules().improvement("IMPROVEMENT_FARM");
    CHECK(!boostIn(s, techBoost("TECH_APPRENTICESHIP")));  // two Mines and a Farm
    CHECK(boostIn(s, civicBoost("CIVIC_CRAFTSMANSHIP")));
    s.plot({13, 5}).improvement = mine;
    CHECK(boostIn(s, techBoost("TECH_APPRENTICESHIP")));

    s.plot({11, 5}).resource = iron;                               // under the Farm, which does not work it
    s.plot({3, 5}).resource = rules().resource("RESOURCE_WHEAT");  // under a Mine, which does not work it
    CHECK(!boostIn(s, techBoost("TECH_WHEEL")));
    CHECK(!boostIn(s, techBoost("TECH_IRON_WORKING")));
    s.plot({5, 5}).resource = iron;
    CHECK(boostIn(s, techBoost("TECH_WHEEL")));
    CHECK(boostIn(s, techBoost("TECH_IRON_WORKING")));
}

TEST(boosts_from_districts_trade_routes_and_meetings) {
    // 04: Cartography (2 Harbors), Mathematics (3 different specialty districts), Currency and Medieval Faires (1 and 4
    // trade routes), Writing (another civ met), Political Philosophy (3 city-states met).
    GameState s = boostState();
    const TypeIndex harbor = rules().district("DISTRICT_HARBOR");
    s.cities[0].districts.push_back({harbor, {5, 6}, true});
    s.cities[1].districts.push_back({harbor, {13, 6}, false});
    CHECK(!boostIn(s, techBoost("TECH_CARTOGRAPHY")));  // the second is still being built
    s.cities[1].districts.back().complete = true;
    CHECK(boostIn(s, techBoost("TECH_CARTOGRAPHY")));
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), {3, 6}, true});
    s.cities[1].districts.push_back({rules().district("DISTRICT_AQUEDUCT"), {11, 6}, true});  // not a specialty district
    CHECK(!boostIn(s, techBoost("TECH_MATHEMATICS")));  // two Harbors and a Campus: two types
    s.cities[1].districts.push_back({rules().district("DISTRICT_HOLY_SITE"), {12, 6}, true});
    CHECK(boostIn(s, techBoost("TECH_MATHEMATICS")));
    CHECK(boostIn(s, techBoost("TECH_MILITARY_ENGINEERING")));  // the Aqueduct

    TradeRoute route;
    route.turnsLeft = 10;
    route.owner = 1;
    route.origin = s.cities[2].id;
    route.destination = s.cities[0].id;
    s.tradeRoutes.push_back(route);
    CHECK(!boostIn(s, techBoost("TECH_CURRENCY")));  // a rival's
    route.origin = s.cities[0].id;
    route.destination = s.cities[1].id;
    for (int32_t id = 2; id <= 4; ++id) {
        route.id = id;
        route.owner = 0;
        s.tradeRoutes.push_back(route);
    }
    CHECK(boostIn(s, techBoost("TECH_CURRENCY")));
    CHECK(!boostIn(s, civicBoost("CIVIC_MEDIEVAL_FAIRES")));
    route.id = 5;
    s.tradeRoutes.push_back(route);
    CHECK(boostIn(s, civicBoost("CIVIC_MEDIEVAL_FAIRES")));

    Player& p = s.players[0];
    p.met[2] = p.met[3] = 1;
    CHECK(!boostIn(s, techBoost("TECH_WRITING")));  // city-states are not civilizations
    CHECK(!boostIn(s, civicBoost("CIVIC_POLITICAL_PHILOSOPHY")));
    p.met[1] = p.met[4] = 1;
    CHECK(boostIn(s, techBoost("TECH_WRITING")));
    CHECK(boostIn(s, civicBoost("CIVIC_POLITICAL_PHILOSOPHY")));
}

TEST(boosts_from_religion_alliances_great_people_and_formations) {
    // 04: Mysticism (a pantheon), Theology (a religion), Reformed Church (6 cities anywhere follow it), Diplomatic
    // Service and Chemistry (an alliance, one of level 2), The Enlightenment (3 Great People), Mobilization and
    // Combined Arms (3 Corps, 3 Armies).
    GameState s = boostState();
    CHECK(!boostIn(s, civicBoost("CIVIC_MYSTICISM")));
    s.players[0].pantheon = 0;
    CHECK(boostIn(s, civicBoost("CIVIC_MYSTICISM")));
    CHECK(!boostIn(s, civicBoost("CIVIC_THEOLOGY")));
    FoundedReligion faith;
    faith.type = 0;
    faith.founder = 0;
    faith.holyCity = s.cities[0].id;
    s.religions.push_back(faith);
    s.players[0].religion = 0;
    CHECK(boostIn(s, civicBoost("CIVIC_THEOLOGY")));
    sovtest::addCity(s, 1, {28, 5}, false, 3);
    sovtest::addCity(s, 0, {4, 11}, false, 3);
    for (City& c : s.cities) c.pressure = {100000};  // five cities, two of them player 1's, follow it
    CHECK(!boostIn(s, civicBoost("CIVIC_REFORMED_CHURCH")));
    sovtest::addCity(s, 1, {12, 11}, false, 3);
    s.cities.back().pressure = {100000};
    CHECK(boostIn(s, civicBoost("CIVIC_REFORMED_CHURCH")));

    Relation& ally = s.players[0].relations[1];
    ally.alliance = AllianceType::Research;
    ally.allianceUntil = 50;
    CHECK(boostIn(s, civicBoost("CIVIC_DIPLOMATIC_SERVICE")));
    CHECK(!boostIn(s, techBoost("TECH_CHEMISTRY")));
    ally.alliancePoints = rules().globalInt("ALLIANCE_LEVEL_TWO_XP");
    CHECK(boostIn(s, techBoost("TECH_CHEMISTRY")));

    std::vector<int>& recruited = s.players[0].greatPeopleRecruited;
    recruited.assign(rules().greatPersonClasses.size(), 0);
    recruited[0] = 2;
    CHECK(!boostIn(s, civicBoost("CIVIC_THE_ENLIGHTENMENT")));
    recruited[1] = 1;
    CHECK(boostIn(s, civicBoost("CIVIC_THE_ENLIGHTENMENT")));

    for (int i = 0; i < 3; ++i) {
        s.unit(sovtest::addUnit(s, "UNIT_SWORDSMAN", 0, {6 + i, 8}))->formation = 1;
        s.unit(sovtest::addUnit(s, "UNIT_SWORDSMAN", 0, {6 + i, 9}))->formation = i < 2 ? 2 : 1;
    }
    CHECK(boostIn(s, civicBoost("CIVIC_MOBILIZATION")));  // four Corps
    CHECK(!boostIn(s, techBoost("TECH_COMBINED_ARMS")));  // two Armies
    s.unit(sovtest::addUnit(s, "UNIT_SWORDSMAN", 0, {9, 8}))->formation = 2;
    CHECK(boostIn(s, techBoost("TECH_COMBINED_ARMS")));
}

TEST(boosts_from_wonders_places_and_continents) {
    // 04: Drama and Poetry (a wonder), Buttress and Flight (a wonder of the era before theirs or later), Astronomy (a
    // University whose Campus touches a Mountain), Conservation (a Neighborhood of appeal 4+), Cultural Heritage (a
    // themed building), Steel (an Ironclad and a Coal Mine), Rapid Deployment (an Aerodrome or Airstrip off the
    // capital's continent), Foreign Trade (land of a second continent revealed).
    GameState s = boostState();
    const auto addBuilding = [&](size_t city, const char* id) {
        s.cities[city].buildings.push_back(rules().building(id));
        std::sort(s.cities[city].buildings.begin(), s.cities[city].buildings.end());
    };
    CHECK(!boostIn(s, civicBoost("CIVIC_DRAMA_AND_POETRY")));
    addBuilding(1, "BUILDING_PYRAMIDS");  // Ancient
    CHECK(boostIn(s, civicBoost("CIVIC_DRAMA_AND_POETRY")));
    CHECK(!boostIn(s, techBoost("TECH_BUTTRESS")));
    addBuilding(0, "BUILDING_COLOSSEUM");  // Classical
    CHECK(boostIn(s, techBoost("TECH_BUTTRESS")));
    CHECK(!boostIn(s, techBoost("TECH_FLIGHT")));
    addBuilding(0, "BUILDING_BIG_BEN");  // Industrial
    CHECK(boostIn(s, techBoost("TECH_FLIGHT")));

    const Hex campus = {5, 7};
    s.cities[0].districts.push_back({rules().district("DISTRICT_CAMPUS"), campus, true});
    addBuilding(0, "BUILDING_UNIVERSITY");
    CHECK(!boostIn(s, techBoost("TECH_ASTRONOMY")));
    Hex peak = campus;  // beside the Campus, away from the city
    for (const Hex& h : s.grid.within(campus, 1)) peak = s.grid.distance(h, s.cities[0].pos) > s.grid.distance(peak, s.cities[0].pos) ? h : peak;
    s.plot(peak).terrain = rules().terrain("TERRAIN_GRASS_MOUNTAIN");
    CHECK(boostIn(s, techBoost("TECH_ASTRONOMY")));

    const Hex home = {13, 7};
    s.cities[1].districts.push_back({rules().district("DISTRICT_NEIGHBORHOOD"), home, true});
    CHECK(!boostIn(s, civicBoost("CIVIC_CONSERVATION")));
    for (const Hex& h : s.grid.within(home, 1)) {
        if (h != home && h != s.cities[1].pos) s.plot(h).feature = rules().feature("FEATURE_FOREST");
    }
    CHECK(boostIn(s, civicBoost("CIVIC_CONSERVATION")));  // Woods all round: Breathtaking

    CHECK(!boostIn(s, civicBoost("CIVIC_CULTURAL_HERITAGE")));
    const TypeIndex dig = rules().building("BUILDING_ARCHAEOLOGICAL_MUSEUM");
    addBuilding(0, "BUILDING_ARCHAEOLOGICAL_MUSEUM");
    TypeIndex artifact = kNone;
    for (size_t w = 0; w < rules().greatWorkTypes.size(); ++w) artifact = rules().greatWorkTypes[w].id == "ARTIFACT" ? static_cast<TypeIndex>(w) : artifact;
    for (size_t k = 0; k < 3; ++k) s.cities[0].greatWorks.push_back({artifact, dig, kNone, 1, s.players[k].civ});
    s.cities[0].greatWorks[1].civ = s.cities[0].greatWorks[0].civ;
    CHECK(!boostIn(s, civicBoost("CIVIC_CULTURAL_HERITAGE")));  // two artifacts of one civilization: no theme
    s.cities[0].greatWorks[1].civ = s.players[1].civ;
    CHECK(boostIn(s, civicBoost("CIVIC_CULTURAL_HERITAGE")));

    Plot& coal = s.plot({3, 4});
    coal.owner = 0;
    coal.resource = rules().resource("RESOURCE_IRON");
    coal.improvement = rules().improvement("IMPROVEMENT_MINE");
    sovtest::addUnit(s, "UNIT_IRONCLAD", 0, {9, 9});
    CHECK(!boostIn(s, techBoost("TECH_STEEL")));  // an Iron Mine
    coal.resource = rules().resource("RESOURCE_COAL");
    CHECK(boostIn(s, techBoost("TECH_STEEL")));
    s.units.clear();
    CHECK(!boostIn(s, techBoost("TECH_STEEL")));  // no Ironclad

    const Hex field = {13, 4};
    s.cities[1].districts.push_back({rules().district("DISTRICT_AERODROME"), field, true});
    CHECK(!boostIn(s, civicBoost("CIVIC_RAPID_DEPLOYMENT")));  // on the capital's continent
    s.plot(field).continent = 1;
    CHECK(boostIn(s, civicBoost("CIVIC_RAPID_DEPLOYMENT")));
    s.plot(field).continent = 0;
    Plot& strip = s.plot({11, 3});
    strip.owner = 0;
    strip.improvement = rules().improvement("IMPROVEMENT_AIRSTRIP");
    CHECK(!boostIn(s, civicBoost("CIVIC_RAPID_DEPLOYMENT")));
    strip.continent = 1;
    CHECK(boostIn(s, civicBoost("CIVIC_RAPID_DEPLOYMENT")));

    GameState t = boostState();
    const Hex island = {29, 13};
    t.plot(island).continent = 1;
    CHECK(!boostIn(t, civicBoost("CIVIC_FOREIGN_TRADE")));  // player 0 sees only its own land
    t.players[0].visibility.assign(t.plots.size(), static_cast<uint8_t>(Visibility::Unrevealed));
    t.players[0].visibility[static_cast<size_t>(t.grid.index(island))] = static_cast<uint8_t>(Visibility::Revealed);
    CHECK(boostIn(t, civicBoost("CIVIC_FOREIGN_TRADE")));
}

TEST(boosts_from_kills_and_cleared_camps) {
    // 04: Archery (a kill with a Slinger), Military Tactics (with a Spearman; Greece's Hoplite stands in), Bronze Working
    // (3 barbarians killed), Military Tradition (a barbarian camp cleared).
    GameState s = flatState(16, 12, 3);
    s.players[2].barbarian = true;
    s.players[2].civ = kNone;
    const auto weak = [&](PlayerId owner, Hex at) { sovtest::addUnit(s, "UNIT_WARRIOR", owner, at), s.units.back().hp = 1; };
    const UnitId slinger = sovtest::addUnit(s, "UNIT_SLINGER", 0, {4, 5});
    weak(1, {5, 5});  // a rival's
    const UnitId warrior = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {8, 2});
    weak(2, {9, 2});
    const UnitId hoplite = sovtest::addUnit(s, "UNIT_HOPLITE", 0, {4, 8});
    weak(2, {5, 8});
    const UnitId third = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {8, 10});
    weak(2, {9, 10});
    const UnitId raider = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {12, 8});
    Camp camp;
    camp.id = s.nextCampId++;
    camp.pos = {13, 8};
    camp.tribe = static_cast<TypeIndex>(rules().barbarianTribes.size() - 1);
    camp.spawnTimer = 99;
    s.camps.push_back(camp);
    auto g = Game::fromScenario(rules(), std::move(s));
    const auto boosted = [&](const char* id) {
        const Player& p = g->state().players[0];
        return id[0] == 'T' ? p.techs.boosted[at(tech(id))] != 0 : p.civics.boosted[at(civic(id))] != 0;
    };
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    REQUIRE(g->submit(Command::rangedAttack(0, slinger, {5, 5})) == CommandError::Ok);
    CHECK(boosted("TECH_ARCHERY"));
    CHECK_EQ(g->state().players[0].barbarianKills, 0);
    REQUIRE(g->submit(Command::attack(0, warrior, {9, 2})) == CommandError::Ok);
    CHECK(!boosted("TECH_MILITARY_TACTICS"));  // a Warrior is no Spearman
    REQUIRE(g->submit(Command::attack(0, hoplite, {5, 8})) == CommandError::Ok);
    CHECK(boosted("TECH_MILITARY_TACTICS"));
    CHECK(!boosted("TECH_BRONZE_WORKING"));  // two barbarians
    REQUIRE(g->submit(Command::attack(0, third, {9, 10})) == CommandError::Ok);
    CHECK(boosted("TECH_BRONZE_WORKING"));
    CHECK(!boosted("CIVIC_MILITARY_TRADITION"));
    REQUIRE(g->submit(Command::move(0, raider, {13, 8})) == CommandError::Ok);
    REQUIRE(g->state().camps.empty());
    CHECK(boosted("CIVIC_MILITARY_TRADITION"));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].barbarianKills, 3);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

TEST(boosts_from_declarations_of_war) {
    // 04: Defensive Tactics (the target of a declaration of war), Nationalism (war declared with a casus belli).
    GameState s = flatState(24, 12, 3);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.relations.resize(s.players.size());
    }
    s.turn = 40;
    sovtest::addCity(s, 0, {3, 5}, true, 3);
    sovtest::addCity(s, 1, {12, 5}, true, 3);
    sovtest::addCity(s, 1, {20, 5}, false, 3);
    s.cities.back().originalOwner = 0;  // founded by player 0: Reconquest
    sovtest::addCity(s, 2, {12, 10}, true, 3);
    Player& p = s.players[0];
    p.civics.done[at(civic("CIVIC_DEFENSIVE_TACTICS"))] = 1;
    p.relations[1].denouncedOn = s.turn - rules().globalInt("DIPLOMACY_DENOUNCE_WAR_DELAY");
    const size_t defensive = at(civic("CIVIC_DEFENSIVE_TACTICS")), nationalism = at(civic("CIVIC_NATIONALISM"));
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->submit(Command::declareWarFor(0, 1, CasusBelli::Reconquest)) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].civics.boosted[defensive], 1);
    CHECK_EQ(g->state().players[0].civics.boosted[nationalism], 1);
    CHECK_EQ(g->state().players[2].civics.boosted[defensive], 0);
    // A surprise war: the target earns its Inspiration, the aggressor nothing.
    auto h = Game::fromScenario(rules(), s);
    REQUIRE(h->submit(Command::declareWar(0, 2)) == CommandError::Ok);
    CHECK_EQ(h->state().players[2].civics.boosted[defensive], 1);
    CHECK_EQ(h->state().players[0].civics.boosted[nationalism], 0);
}

TEST(research_unlocks_units_buildings_and_resources) {
    GameState base = flatState(20, 14, 1);
    base.plot({7, 6}).resource = rules().resource("RESOURCE_HORSES");
    auto g = capitalWith([](GameState&) {}, base);
    const City& c = g->state().cities[0];
    const ProductionItem archer{ProductionKind::Unit, rules().unit("UNIT_ARCHER")};
    const ProductionItem granary{ProductionKind::Building, rules().building("BUILDING_GRANARY")};
    CHECK(!g->canProduce(c, archer));
    CHECK(!g->canProduce(c, granary));
    const Yields hidden = g->plotYields({7, 6}, c);

    auto g2 = capitalWith([](GameState& s) {
        s.players[0].techs.done[at(tech("TECH_ARCHERY"))] = 1;
        s.players[0].techs.done[at(tech("TECH_POTTERY"))] = 1;
        s.players[0].techs.done[at(tech("TECH_ANIMAL_HUSBANDRY"))] = 1;
        s.players[0].techs.current = tech("TECH_MINING");
    }, base);
    const City& c2 = g2->state().cities[0];
    CHECK(g2->canProduce(c2, archer));
    CHECK(g2->canProduce(c2, granary));
    const Yields shown = g2->plotYields({7, 6}, c2);
    CHECK_EQ(shown[P], hidden[P] + Fixed::fromInt(1));  // Horses: +1 Food, +1 Production
}

TEST(government_adoption_and_policy_slots) {
    auto g = capitalWith([](GameState& s) {
        s.players[0].civics.progress[at(civic("CIVIC_CODE_OF_LAWS"))] = Fixed::fromInt(20);  // completes next turn
    });
    CHECK_EQ(g->submit(Command::changeGovernment(0, gov("GOVERNMENT_CHIEFDOM"))), CommandError::CannotAdoptGovernment);
    endTurns(*g, 1);
    const Player& p = g->state().players[0];
    REQUIRE(p.civics.has(civic("CIVIC_CODE_OF_LAWS")));
    CHECK(p.freeChanges);
    CHECK_EQ(g->submit(Command::changeGovernment(0, gov("GOVERNMENT_AUTOCRACY"))), CommandError::CannotAdoptGovernment);
    REQUIRE(g->submit(Command::changeGovernment(0, gov("GOVERNMENT_CHIEFDOM"))) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].policies.size(), 2u);

    const CityId city = g->state().cities[0].id;
    const Fixed before = g->cityReport(city).yields[P];
    CHECK_EQ(g->submit(Command::setPolicy(0, 0, policy("POLICY_URBAN_PLANNING"))), CommandError::CannotSetPolicy);  // military slot
    CHECK_EQ(g->submit(Command::setPolicy(0, 1, policy("POLICY_AGOGE"))), CommandError::CannotSetPolicy);  // not unlocked
    REQUIRE(g->submit(Command::setPolicy(0, 1, policy("POLICY_URBAN_PLANNING"))) == CommandError::Ok);
    CHECK_EQ(g->submit(Command::setPolicy(0, 0, policy("POLICY_URBAN_PLANNING"))), CommandError::CannotSetPolicy);
    REQUIRE(g->submit(Command::setPolicy(0, 0, policy("POLICY_SURVEY"))) == CommandError::Ok);
    REQUIRE(g->submit(Command::setPolicy(0, 0, policy("POLICY_DISCIPLINE"))) == CommandError::Ok);
    CHECK_EQ(g->cityReport(city).yields[P], before + Fixed::fromInt(1));  // Urban Planning

    // The window closes when the turn ends.
    endTurns(*g, 1);
    CHECK(!g->state().players[0].freeChanges);
    CHECK_EQ(g->submit(Command::setPolicy(0, 1, policy("POLICY_GOD_KING"))), CommandError::ChangesLocked);
    CHECK_EQ(g->submit(Command::setPolicy(0, 1, kNone)), CommandError::ChangesLocked);
}

TEST(government_bonus_and_anarchy) {
    auto g = capitalWith([](GameState& s) {
        chiefdom(s);
        s.players[0].civics.done[at(civic("CIVIC_POLITICAL_PHILOSOPHY"))] = 1;
    });
    const CityId city = g->state().cities[0].id;
    const Fixed science = g->cityReport(city).yields[S];
    REQUIRE(g->submit(Command::changeGovernment(0, gov("GOVERNMENT_AUTOCRACY"))) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].anarchyTurns, 0);  // first time under Autocracy
    CHECK_EQ(g->state().players[0].policies.size(), 4u);
    CHECK_EQ(g->cityReport(city).yields[S], science + Fixed::fromInt(1));  // +1 all yields in the Palace city

    // Going back to Chiefdom, used before, costs 2 + 1 turns of anarchy.
    GameState s = g->state();
    s.players[0].freeChanges = true;
    auto g2 = Game::fromScenario(rules(), s);
    REQUIRE(g2->submit(Command::changeGovernment(0, gov("GOVERNMENT_CHIEFDOM"))) == CommandError::Ok);
    CHECK_EQ(g2->state().players[0].anarchyTurns, 3);
    CHECK_EQ(g2->sciencePerTurn(0), Fixed());
    CHECK_EQ(g2->culturePerTurn(0), Fixed());
    CHECK_EQ(g2->submit(Command::setPolicy(0, 0, policy("POLICY_SURVEY"))), CommandError::CannotSetPolicy);
    CHECK_EQ(g2->submit(Command::changeGovernment(0, gov("GOVERNMENT_AUTOCRACY"))), CommandError::CannotAdoptGovernment);
    const Fixed progress = g2->state().players[0].techs.progress[at(tech("TECH_POTTERY"))];
    const Fixed gold = g2->state().players[0].gold;
    endTurns(*g2, 2);
    CHECK_EQ(g2->state().players[0].techs.progress[at(tech("TECH_POTTERY"))], progress);
    CHECK_EQ(g2->state().players[0].gold, gold);  // no income; nothing to maintain either
    CHECK_EQ(g2->state().players[0].anarchyTurns, 1);
    endTurns(*g2, 1);
    CHECK_EQ(g2->state().players[0].anarchyTurns, 0);
    CHECK(g2->state().players[0].freeChanges);  // set up the new government's policies
    REQUIRE(g2->submit(Command::setPolicy(0, 0, policy("POLICY_SURVEY"))) == CommandError::Ok);
}

TEST(policy_effects_and_obsolescence) {
    // Agoge: +50% production toward Ancient and Classical melee units.
    auto g = capitalWith([](GameState& s) {
        chiefdom(s);
        s.players[0].civics.done[at(civic("CIVIC_CRAFTSMANSHIP"))] = 1;
        s.players[0].civics.done[at(civic("CIVIC_EARLY_EMPIRE"))] = 1;
        s.players[0].civics.done[at(civic("CIVIC_STATE_WORKFORCE"))] = 1;
        s.players[0].civics.current = civic("CIVIC_FEUDALISM");
    });
    const CityId city = g->state().cities[0].id;
    const Fixed prod = g->cityReport(city).yields[P];
    REQUIRE(g->submit(Command::setPolicy(0, 0, policy("POLICY_AGOGE"))) == CommandError::Ok);
    endTurns(*g, 1);
    const City& c = *g->state().city(city);
    REQUIRE(c.progress.size() == 1u);
    CHECK_EQ(c.progress[0].amount, prod * 3 / 2);

    // Land Surveyors: -20% plot cost. Conscription: 1 gold off each unit's upkeep.
    GameState s = g->state();
    s.players[0].freeChanges = true;
    s.players[0].gold = Fixed::fromInt(100);
    sovtest::addUnit(s, "UNIT_SPEARMAN", 0, {9, 9});
    auto g2 = Game::fromScenario(rules(), s);
    const int plotCost = g2->plotPurchaseCost(city, {8, 6});  // 50, raised a little by the research done
    CHECK(plotCost >= 50);
    const Fixed gold = g2->goldPerTurn(0);
    REQUIRE(g2->submit(Command::setPolicy(0, 1, policy("POLICY_LAND_SURVEYORS"))) == CommandError::Ok);
    CHECK_EQ(g2->plotPurchaseCost(city, {8, 6}), plotCost * 80 / 100);
    REQUIRE(g2->submit(Command::setPolicy(0, 0, policy("POLICY_CONSCRIPTION"))) == CommandError::Ok);
    CHECK_EQ(g2->goldPerTurn(0), gold + Fixed::fromInt(1));

    // Feudalism unlocks Feudal Contract, which retires a slotted Agoge.
    GameState s3 = g->state();
    s3.players[0].civics.progress[at(civic("CIVIC_FEUDALISM"))] = Fixed::fromInt(g->civicCost(civic("CIVIC_FEUDALISM")));
    auto g3 = Game::fromScenario(rules(), s3);
    REQUIRE(g3->state().players[0].policies[0] == policy("POLICY_AGOGE"));
    endTurns(*g3, 1);
    CHECK(g3->state().players[0].civics.has(civic("CIVIC_FEUDALISM")));
    CHECK_EQ(g3->state().players[0].policies[0], kNone);
    CHECK(!g3->policyAvailable(0, policy("POLICY_AGOGE")));
}

TEST(dark_age_cards_come_with_a_dark_age_and_go_with_it) {
    // Classical Republic (one Wildcard slot), the world in the Classical era, a Holy Site in the capital.
    auto setup = [](GameState& s) {
        chiefdom(s);
        Player& p = s.players[0];
        p.government = gov("GOVERNMENT_CLASSICAL_REPUBLIC");
        p.governmentUses[at(p.government)] = 1;
        p.policies.assign(static_cast<size_t>(rules().governments[at(p.government)].totalSlots()), kNone);
        p.age = Age::Dark;
        s.gameEra = 1;
        CityDistrict holy;
        holy.type = rules().district("DISTRICT_HOLY_SITE");
        holy.pos = {9, 7};
        holy.complete = true;
        s.cities[0].districts.push_back(holy);
    };
    auto g = capitalWith(setup);
    const TypeIndex monasticism = policy("POLICY_MONASTICISM");
    REQUIRE(monasticism != kNone);
    CHECK(rules().policies[at(monasticism)].darkAge);
    CHECK(g->policyAvailable(0, monasticism));
    CHECK(!g->policyAvailable(0, policy("POLICY_ROBBER_BARONS")));  // Industrial to Atomic
    int wild = -1;
    const GovernmentType& republic = rules().governments[at(gov("GOVERNMENT_CLASSICAL_REPUBLIC"))];
    for (int slot = 0; slot < static_cast<int>(g->state().players[0].policies.size()); ++slot) {
        if (Game::slotType(republic, slot) == PolicySlot::Wildcard) wild = slot;
    }
    REQUIRE(wild >= 0);
    const CityId city = g->state().cities[0].id;
    const Fixed science = g->cityReport(city).yields[S];
    REQUIRE(g->submit(Command::setPolicy(0, wild, monasticism)) == CommandError::Ok);
    CHECK(g->cityReport(city).yields[S] > science);  // +75% Science with a Holy Site
    // Not outside a Dark Age.
    {
        GameState s = g->state();
        s.players[0].age = Age::Normal;
        auto normal = Game::fromScenario(rules(), std::move(s));
        CHECK(!normal->policyAvailable(0, monasticism));
    }
    // The world moves on with a Golden Age for us: the card leaves its slot.
    GameState s = g->state();
    s.players[0].eraScore = 200;
    s.gameEraStart = s.turn - 1000;
    auto later = Game::fromScenario(rules(), std::move(s));
    endTurns(*later, 1);
    REQUIRE(later->state().gameEra == 2);
    CHECK(later->state().players[0].age != Age::Dark);
    CHECK(later->state().players[0].policies[static_cast<size_t>(wild)] == kNone);
}

TEST(dark_age_cards_do_what_their_text_says) {
    // Each card slotted straight into the first slot (its effect, not its slot rules, is tested here).
    auto with = [](const char* card, auto&& edit) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            s.players[0].policies[0] = policy(card);
            edit(s);
        });
    };
    // Isolationism: no new cities and no Settlers; domestic routes +2 Food and Production.
    {
        auto g = with("POLICY_ISOLATIONISM", [](GameState&) {});
        const City& c = g->state().cities[0];
        CHECK(!g->canFoundCityAt(0, {12, 6}));
        CHECK(!g->canProduce(c, {ProductionKind::Unit, rules().unit("UNIT_SETTLER")}));
        CHECK(g->canProduce(c, warrior()));
        const Yields y = g->tradeRouteYields(c, c);
        CHECK(y[static_cast<size_t>(YieldType::Food)] >= Fixed::fromInt(2));
        CHECK(y[P] >= Fixed::fromInt(2));
    }
    // Twilight Valor: wounded units stay wounded.
    {
        UnitId hurt = kNoUnit;
        auto g = with("POLICY_TWILIGHT_VALOR", [&](GameState& s) {
            hurt = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {7, 7});
            s.units.back().hp = 50;
            s.units.back().activity = Activity::Fortify;
        });
        endTurns(*g, 1);
        CHECK_EQ(g->state().unit(hurt)->hp, 50);
    }
    // Letters of Marque: raiders +2 Movement, +100% plunder, route yields halved.
    {
        UnitId raider = kNoUnit, warrior2 = kNoUnit;
        auto g = with("POLICY_LETTERS_OF_MARQUE", [&](GameState& s) {
            raider = sovtest::addUnit(s, "UNIT_PRIVATEER", 0, {7, 7});
            warrior2 = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {7, 8});
        });
        CHECK_EQ(g->maxMoves(*g->state().unit(raider)), rules().units[at(rules().unit("UNIT_PRIVATEER"))].moves + 2);
        CHECK_EQ(g->plunderPercent(*g->state().unit(warrior2)), 100);
    }
    // Flower Power: units are bought, at twice the price, not trained; Rock Bands keep their price.
    {
        auto g = with("POLICY_FLOWER_POWER", [](GameState&) {});
        auto plain = capitalWith([](GameState& s) { chiefdom(s); });
        const City& c = g->state().cities[0];
        CHECK(!g->canProduce(c, warrior()));
        CHECK(g->canProduce(c, warrior(), nullptr, true));
        CHECK_EQ(g->purchaseCost(0, warrior()), plain->purchaseCost(0, warrior()) * 2);
    }
    // Rogue State: no envoys from civics.
    {
        auto g = with("POLICY_ROGUE_STATE", [](GameState& s) {
            s.players[0].civics.current = civic("CIVIC_MYSTICISM");
            s.players[0].civics.done[at(civic("CIVIC_FOREIGN_TRADE"))] = 1;
            s.players[0].civics.done[at(civic("CIVIC_CRAFTSMANSHIP"))] = 1;
            s.players[0].civics.progress[at(civic("CIVIC_MYSTICISM"))] = Fixed::fromInt(1000);
        });
        const int tokens = g->state().players[0].envoyTokens;
        endTurns(*g, 1);
        REQUIRE(g->state().players[0].civics.has(civic("CIVIC_MYSTICISM")));
        CHECK_EQ(g->state().players[0].envoyTokens, tokens);
    }
    // Cyber Warfare: grievances against us do not fade (held here by the only civ, against itself).
    {
        auto keep = [](GameState& s) { s.players[0].grievances.assign(1, 100); };
        auto g = with("POLICY_CYBER_WARFARE", keep);
        auto plain = capitalWith([&](GameState& s) {
            chiefdom(s);
            keep(s);
        });
        endTurns(*g, 2);
        endTurns(*plain, 2);
        CHECK_EQ(g->state().players[0].grievances[0], 100);
        CHECK(plain->state().players[0].grievances[0] < 100);
    }
}

TEST(generated_policy_cards_take_effect) {
    const Rules& r = rules();
    auto with = [](const char* card, auto&& edit) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            s.players[0].policies[0] = policy(card);
            edit(s);
        });
    };
    auto none = [](GameState&) {};
    // The generator turned the text into modifiers; hand-written cards keep theirs only.
    size_t generated = 0;
    for (const Modifier& m : r.modifiers) generated += m.source == "POLICY_FEUDAL_CONTRACT" ? 1 : 0;
    CHECK(generated >= 3u);
    for (const Modifier& m : r.modifiers) CHECK(m.source != "POLICY_AGOGE" || m.id.rfind("POLICY_", 0) != 0);
    // Feudal Contract: +50% toward melee units of the Ancient to Renaissance eras, not later ones.
    {
        auto g = with("POLICY_FEUDAL_CONTRACT", none);
        const City& c = g->state().cities[0];
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_MAN_AT_ARMS")), Fixed::fromInt(50));
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_INFANTRY")), Fixed());
        // Ranged units have one line for the Ancient era and one from the Medieval: each unit gets one of them.
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_ARCHER")), Fixed::fromInt(50));
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_CROSSBOWMAN")), Fixed::fromInt(50));
    }
    // Ilkum: +30% toward Builders, not other units.
    {
        auto g = with("POLICY_ILKUM", none);
        const City& c = g->state().cities[0];
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_BUILDER")), Fixed::fromInt(30));
        CHECK_EQ(sumUnitProductionPercent(g->state(), r, c, r.unit("UNIT_SETTLER")), Fixed());
    }
    // Retainers: +1 Amenity with a garrison.
    {
        auto bare = with("POLICY_RETAINERS", none);
        auto guarded = with("POLICY_RETAINERS", [](GameState& s) { sovtest::addUnit(s, "UNIT_WARRIOR", 0, s.cities[0].pos); });
        CHECK_EQ(guarded->cityReport(guarded->state().cities[0].id).amenities, bare->cityReport(bare->state().cities[0].id).amenities + 1);
    }
    // Insulae: +1 Housing with two specialty districts.
    {
        auto districts = [](GameState& s) {
            for (const char* id : {"DISTRICT_CAMPUS", "DISTRICT_HOLY_SITE"}) {
                CityDistrict d;
                d.type = rules().district(id);
                d.pos = {static_cast<int>(s.cities[0].districts.size()) == 0 ? 7 : 5, 7};
                d.complete = true;
                s.cities[0].districts.push_back(d);
            }
        };
        auto g = with("POLICY_INSULAE", districts);
        auto plain = capitalWith([&](GameState& s) {
            chiefdom(s);
            districts(s);
        });
        CHECK_EQ(g->cityReport(g->state().cities[0].id).housing, plain->cityReport(plain->state().cities[0].id).housing + Fixed::fromInt(1));
    }
    // After Action Reports: +50% XP.
    {
        auto g = with("POLICY_AFTER_ACTION_REPORTS", none);
        CHECK_EQ(sumUnitXpPercent(g->state(), r, g->state().players[0], "MELEE"), Fixed::fromInt(50));
    }
    // Survey: +100% XP for recon units only.
    {
        auto g = with("POLICY_SURVEY", none);
        CHECK_EQ(sumUnitXpPercent(g->state(), r, g->state().players[0], "RECON"), Fixed::fromInt(100));
        CHECK_EQ(sumUnitXpPercent(g->state(), r, g->state().players[0], "MELEE"), Fixed());
    }
    // Invention: Great Engineer points, +2 in a city with a Workshop and +4 for the civ, and none for other classes.
    {
        auto g = with("POLICY_INVENTION", [](GameState& s) {
            std::vector<TypeIndex>& b = s.cities[0].buildings;
            b.push_back(rules().building("BUILDING_WORKSHOP"));
            std::sort(b.begin(), b.end());
        });
        const TypeIndex engineer = r.greatPersonClass("GREAT_PERSON_CLASS_ENGINEER"), scientist = r.greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
        const City& c = g->state().cities[0];
        const Player& p = g->state().players[0];
        CHECK_EQ(sumCityGreatPersonPoints(g->state(), r, c, engineer), Fixed::fromInt(2));
        CHECK_EQ(sumCityGreatPersonPoints(g->state(), r, c, scientist), Fixed());
        CHECK_EQ(sumPlayerGreatPersonPoints(g->state(), r, p, engineer), Fixed::fromInt(4));
        CHECK_EQ(sumPlayerGreatPersonPoints(g->state(), r, p, scientist), Fixed());
    }
    // Wisselbanken: a route to an ally gives +2 Food and +2 Production to its origin, and as much again to its destination.
    {
        auto g = with("POLICY_WISSELBANKEN", none);
        for (const bool toDestination : {false, true}) {
            const Yields y = tradeRouteModifierYields(g->state(), r, g->state().players[0], false, true, false, false, toDestination);
            CHECK_EQ(y[static_cast<size_t>(YieldType::Food)], Fixed::fromInt(2));
            CHECK_EQ(y[static_cast<size_t>(YieldType::Production)], Fixed::fromInt(2));
        }
    }
}

TEST(policy_cards_reach_routes_production_great_people_and_favor) {
    const Rules& r = rules();
    auto with = [](const char* card) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            s.players[0].policies[0] = policy(card);
        });
    };
    auto plain = capitalWith([](GameState& s) { chiefdom(s); });
    const City& pc = plain->state().cities[0];
    constexpr size_t G = static_cast<size_t>(YieldType::Gold);
    // Caravansaries: +2 Gold on every route.
    {
        auto g = with("POLICY_CARAVANSARIES");
        const City& c = g->state().cities[0];
        CHECK_EQ(g->tradeRouteYields(c, c)[G], plain->tradeRouteYields(pc, pc)[G] + Fixed::fromInt(2));
    }
    // Corvée: +15% toward Ancient and Classical wonders; Veterancy: +30% toward Encampment buildings.
    {
        auto g = with("POLICY_CORV_E");
        const City& c = g->state().cities[0];
        CHECK_EQ(sumItemProductionPercent(g->state(), r, c, {ProductionKind::Building, r.building("BUILDING_PYRAMIDS")}), Fixed::fromInt(15));
        CHECK_EQ(sumItemProductionPercent(g->state(), r, c, {ProductionKind::Building, r.building("BUILDING_MONUMENT")}), Fixed());
        auto v = with("POLICY_VETERANCY");
        CHECK_EQ(sumItemProductionPercent(v->state(), r, v->state().cities[0], {ProductionKind::Building, r.building("BUILDING_BARRACKS")}), Fixed::fromInt(30));
        CHECK_EQ(sumItemProductionPercent(v->state(), r, v->state().cities[0], {ProductionKind::District, r.district("DISTRICT_ENCAMPMENT")}), Fixed::fromInt(30));
    }
    // Inspiration: +2 Great Scientist points a turn.
    {
        auto g = with("POLICY_INSPIRATION");
        const TypeIndex sci = r.greatPersonClass("GREAT_PERSON_CLASS_SCIENTIST");
        CHECK_EQ(g->greatPersonPointsPerTurn(0, sci), plain->greatPersonPointsPerTurn(0, sci) + 2);
    }
    // Diplomatic Capital: +4 Favor a turn. Charismatic Leader: +2 influence a turn.
    {
        auto g = with("POLICY_DIPLOMATIC_CAPITAL");
        CHECK_EQ(g->favorPerTurn(0), plain->favorPerTurn(0) + 4);
        auto h = with("POLICY_CHARISMATIC_LEADER");
        const int before = h->state().players[0].influence;
        auto p = plain->state().players[0].influence;
        endTurns(*h, 1);
        endTurns(*plain, 1);
        CHECK_EQ(h->state().players[0].influence - before, plain->state().players[0].influence - p + 2);
    }
}

TEST(policy_cards_in_code_military_and_economy) {
    auto with = [](const char* card, auto&& edit) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            if (card) s.players[0].policies[0] = policy(card);
            edit(s);
        });
    };
    auto none = [](GameState&) {};
    // Bastions: +6 city strength.
    {
        auto g = with("POLICY_BASTIONS", none);
        auto plain = with(nullptr, none);
        CHECK_EQ(g->cityStrength(g->state().cities[0]), plain->cityStrength(plain->state().cities[0]) + 6);
    }
    // Professional Army: upgrades at half the gold.
    {
        UnitId w = kNoUnit;
        auto warrior2 = [&](GameState& s) {
            s.players[0].techs.done[at(tech("TECH_MINING"))] = 1;
            s.players[0].techs.done[at(tech("TECH_BRONZE_WORKING"))] = 1;
            s.players[0].techs.done[at(tech("TECH_IRON_WORKING"))] = 1;
            w = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {7, 7});
        };
        auto g = with("POLICY_PROFESSIONAL_ARMY", warrior2);
        auto plain = with(nullptr, warrior2);
        REQUIRE(plain->upgradeCost(*plain->state().unit(w)) > 0);
        CHECK_EQ(g->upgradeCost(*g->state().unit(w)), plain->upgradeCost(*plain->state().unit(w)) / 2);
    }
    // Logistics: +1 Movement starting the turn in its own territory.
    {
        UnitId w = kNoUnit;
        auto g = with("POLICY_LOGISTICS", [&](GameState& s) {
            w = sovtest::addUnit(s, "UNIT_WARRIOR", 0, {7, 6});
            s.units.back().activity = Activity::Fortify;
        });
        endTurns(*g, 1);
        CHECK(g->state().unit(w)->movesLeft == Fixed::fromInt(g->maxMoves(*g->state().unit(w)) + 1));
    }
    // Rationalism: +50% of a Campus's building science in a city of 15.
    {
        auto campus = [](GameState& s) {
            CityDistrict d;
            d.type = rules().district("DISTRICT_CAMPUS");
            d.pos = {7, 7};
            d.complete = true;
            s.cities[0].districts.push_back(d);
            s.cities[0].buildings.push_back(rules().building("BUILDING_LIBRARY"));
            std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
            s.cities[0].population = 15;
        };
        auto g = with("POLICY_RATIONALISM", campus);
        auto plain = with(nullptr, campus);
        CHECK(g->cityReport(g->state().cities[0].id).yields[S] > plain->cityReport(plain->state().cities[0].id).yields[S]);
    }
}

TEST(policy_cards_in_code_tourism_and_diplomacy) {
    auto with = [](const char* card, auto&& edit) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            if (card) s.players[0].policies[0] = policy(card);
            edit(s);
        });
    };
    // Heritage Tourism: art doubles its tourism.
    {
        auto art = [](GameState& s) {
            City& c = s.cities[0];
            c.buildings.push_back(rules().building("BUILDING_AMPHITHEATER"));
            std::sort(c.buildings.begin(), c.buildings.end());
            GreatWork w;
            w.type = rules().greatWorkType("SCULPTURE");
            w.building = rules().building("BUILDING_AMPHITHEATER");
            c.greatWorks.push_back(w);
        };
        auto g = with("POLICY_HERITAGE_TOURISM", art);
        auto plain = with(nullptr, art);
        const int work = rules().greatWorkTypes[at(rules().greatWorkType("SCULPTURE"))].tourism;
        CHECK_EQ(g->tourismPerTurn(0), plain->tourismPerTurn(0) + work);
    }
    // Merchant Confederation: +1 Gold per envoy placed.
    {
        auto envoys = [](GameState& s) { s.players[0].envoys.assign(1, 3); };
        auto g = with("POLICY_MERCHANT_CONFEDERATION", envoys);
        auto plain = with(nullptr, envoys);
        CHECK_EQ(g->goldPerTurn(0), plain->goldPerTurn(0) + Fixed::fromInt(3));
    }
}

TEST(left_out_building_effects) {
    auto with = [](std::vector<const char*> buildings, auto&& edit) {
        return capitalWith([&](GameState& s) {
            chiefdom(s);
            for (const char* b : buildings) s.cities[0].buildings.push_back(rules().building(b));
            std::sort(s.cities[0].buildings.begin(), s.cities[0].buildings.end());
            edit(s);
        });
    };
    auto none = [](GameState&) {};
    auto plain = with({}, none);
    const CityId pc = plain->state().cities[0].id;
    // Pagoda: +1 Favor. Shopping Mall: +4 Tourism. Audience Chamber: -2 Loyalty in a city without a governor.
    {
        auto g = with({"BUILDING_PAGODA"}, none);
        CHECK_EQ(g->favorPerTurn(0), plain->favorPerTurn(0) + 1);
        auto m = with({"BUILDING_SHOPPING_MALL"}, none);
        CHECK_EQ(m->tourismPerTurn(0), plain->tourismPerTurn(0) + 4);
        auto a = with({"BUILDING_AUDIENCE_CHAMBER"}, none);
        CHECK_EQ(a->loyaltyPerTurn(a->state().cities[0].id), plain->loyaltyPerTurn(pc) - Fixed::fromInt(2));
    }
    // Zoo: +1 Science on its rainforest.
    {
        auto jungle = [](GameState& s) { s.plot({7, 6}).feature = rules().feature("FEATURE_JUNGLE"); };
        auto g = with({"BUILDING_ZOO"}, jungle);
        auto bare = with({}, jungle);
        CHECK_EQ(g->plotYields({7, 6}, g->state().cities[0])[S], bare->plotYields({7, 6}, bare->state().cities[0])[S] + Fixed::fromInt(1));
    }
}
