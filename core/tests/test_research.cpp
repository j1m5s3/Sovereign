// Research and government (specs/civ6/04-tech-civics-government.md).
#include <algorithm>

#include "helpers.h"

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
    CHECK(r.techs[at(tech("TECH_WRITING"))].boost.kind == BoostKind::NotTracked);  // meet a civ: diplomacy
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
    CHECK_EQ(q.techs.progress[at(tech("TECH_CONSTRUCTION"))], Fixed::fromInt(80));
    CHECK_EQ(q.techs.boosted[at(tech("TECH_SAILING"))], 0);  // inland
    // A boost is earned once.
    endTurns(*g, 1);
    CHECK_EQ(g->state().players[0].techs.progress[at(tech("TECH_CONSTRUCTION"))], Fixed::fromInt(80));
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
    CHECK_EQ(g2->plotPurchaseCost(city, {8, 6}), 50);
    const Fixed gold = g2->goldPerTurn(0);
    REQUIRE(g2->submit(Command::setPolicy(0, 1, policy("POLICY_LAND_SURVEYORS"))) == CommandError::Ok);
    CHECK_EQ(g2->plotPurchaseCost(city, {8, 6}), 40);
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
