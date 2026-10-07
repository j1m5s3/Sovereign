// Research and government (specs/civ6/04-tech-civics-government.md).
#include <algorithm>

#include "helpers.h"
#include "sovereign/modifiers.h"

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
