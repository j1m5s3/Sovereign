// Grievances, Diplomatic Favor and the World Congress (08 [GS]): where grievances come from and
// how they fade, favor per turn, convening, voting with favor, resolutions in force, the
// Diplomatic Victory, saves.
#include <algorithm>

#include "helpers.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::flatState;
using sovtest::rules;

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }

GameState wcState() {
    GameState s = flatState(30, 14, 2);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(2, 1);
        p.grievances.assign(2, 0);
    }
    s.players[0].human = true;
    addCity(s, 0, {4, 6}, true, 5);
    addCity(s, 1, {16, 6}, true, 5);
    s.majorsAtStart = 2;
    return s;
}

// A session with one resolution and votes already cast.
GameState inSession(const char* resolution, uint8_t option, int32_t candidate) {
    GameState s = wcState();
    CongressItem item;
    item.resolution = rules().resolution(resolution);
    item.candidates = {0, 1};
    item.votes.push_back({0, option, candidate, 2});
    item.votes.push_back({1, option, candidate, 1});
    s.congress.push_back(item);
    s.congressOpenedTurn = s.turn;
    s.nextCongressTurn = s.turn + 30;
    return s;
}
}  // namespace

TEST(congress_rules_data) {
    const Rules& r = rules();
    int supported = 0;
    for (const ResolutionType& res : r.resolutions) supported += res.kind != ResolutionKind::Unsupported ? 1 : 0;
    CHECK_EQ(supported, static_cast<int>(r.resolutions.size()));  // every one, Mercenary Companies and Arms Control included
    CHECK(r.resolutions[at(r.resolution("RESOLUTION_MERCENARY_COMPANIES"))].target == ResolutionTarget::Yield);
    CHECK_EQ(r.resolutions[at(r.resolution("RESOLUTION_DIPLOMATIC_VICTORY"))].minEra, r.era("ERA_MODERN"));
    CHECK_EQ(r.eras[0].grievanceDecay, 10);
    CHECK_EQ(r.governments[at(r.government("GOVERNMENT_MONARCHY"))].favor, 2);
    CHECK(!r.promotionClasses.empty());
}

TEST(wars_and_denunciations_breed_grievances_that_fade) {
    auto g = Game::fromScenario(rules(), wcState());
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK_EQ(g->grievances(1, 0), 150);  // a surprise war
    sovtest::endTurns(*g, 2);            // one world turn: Ancient decay 10
    CHECK_EQ(g->grievances(1, 0), 140);
    bool weighs = false;
    for (const OpinionReason& r : g->opinionReasons(1, 0)) weighs |= r.kind == OpinionReasonKind::Grievances && r.value == -14;
    CHECK(weighs);
    auto g2 = Game::fromScenario(rules(), wcState());
    REQUIRE(g2->submit(Command::denounce(0, 1)) == CommandError::Ok);
    CHECK_EQ(g2->grievances(1, 0), 25);
}

TEST(holding_a_capital_breeds_grievances) {
    GameState s = wcState();
    addCity(s, 1, {10, 10}, false, 3);
    s.cities.back().originalOwner = 0;
    s.cities.back().originalCapital = true;  // once 0's capital
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK_EQ(g->grievances(0, 1), 0);  // +3, then the Ancient era's decay of 10
    GameState s2 = g->state();
    s2.gameEra = 8;  // the Information era decays 3 a turn
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    sovtest::endTurns(*g2, 2);
    CHECK_EQ(g2->grievances(0, 1), 0);
}

TEST(favor_comes_from_government_and_suffers_from_grievances) {
    GameState s = wcState();
    s.players[0].government = rules().government("GOVERNMENT_MONARCHY");
    s.players[1].grievances[0] = 450;  // 250 over the start: -5
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->favorPerTurn(0), 2 - 5);
    CHECK_EQ(g->favorPerTurn(1), 0);
}

TEST(the_congress_convenes_in_the_medieval_era) {
    GameState s = wcState();
    // A Medieval tech makes player 1 a Medieval civ.
    for (size_t i = 0; i < rules().techs.size(); ++i) {
        if (rules().techs[i].era == 2) {
            s.players[1].techs.done[i] = 1;
            break;
        }
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(!g->congressInSession());
    sovtest::endTurns(*g, 2);
    REQUIRE(g->congressInSession());
    CHECK_EQ(g->state().congress.size(), 2u);
    CHECK(g->hasVoted(1, 0));   // the AI votes as the session opens
    CHECK(!g->hasVoted(0, 0));  // the human votes on their turn
    REQUIRE(g->submit(Command::congressVote(0, 0, 0, 0)) == CommandError::Ok);
    CHECK(g->submit(Command::congressVote(0, 0, 0, 0)) == CommandError::CannotVote);  // once
    CHECK(g->submit(Command::congressVote(0, 1, 0, 0, 3)) == CommandError::CannotVote);  // 60 favor it lacks
    sovtest::endTurns(*g, 2);
    CHECK(!g->congressInSession());
    CHECK(g->state().nextCongressTurn > g->state().turn);
}

TEST(trade_policy_and_patronage_take_effect) {
    auto g = Game::fromScenario(rules(), inSession("RESOLUTION_TRADE_POLICY", 0, 0));
    const int before = g->tradeRouteCapacity(0);
    sovtest::endTurns(*g, 2);
    REQUIRE(g->passed(ResolutionKind::TradePolicy));
    CHECK_EQ(g->tradeRouteCapacity(0), before + 1);
    auto g2 = Game::fromScenario(rules(), inSession("RESOLUTION_TRADE_POLICY", 1, 1));
    sovtest::endTurns(*g2, 2);
    CHECK_EQ(g2->tradeRouteCapacity(1), 0);
}

TEST(diplomatic_victory_points_win_the_game) {
    GameState s = inSession("RESOLUTION_DIPLOMATIC_VICTORY", 0, 0);
    s.players[0].diplomaticVictoryPoints = 18;
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 2);
    CHECK_EQ(g->state().players[0].diplomaticVictoryPoints, 20);
    CHECK(g->state().victory == Victory::Diplomatic);
    CHECK_EQ(g->state().winner, 0);
}

TEST(congress_survives_a_save) {
    GameState s = inSession("RESOLUTION_PATRONAGE", 0, 1);
    s.players[0].favor = 40;
    s.players[1].grievances[0] = 77;
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->congressInSession());
    CHECK_EQ(loaded->state().congress[0].votes.size(), 2u);
    CHECK_EQ(loaded->state().players[0].favor, 40);
    CHECK_EQ(loaded->grievances(1, 0), 77);
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

namespace {
// wcState() with one resolution in force.
std::unique_ptr<Game> withResolution(const char* resolution, uint8_t option, int32_t target, GameState s = wcState()) {
    s.passedResolutions.push_back({rules().resolution(resolution), option, target});
    return Game::fromScenario(rules(), std::move(s));
}
}  // namespace

TEST(more_resolutions_take_effect) {
    // Global Energy Treaty: A bans the chosen power building; Public Works: +100% toward a project.
    TypeIndex plant = kNone;
    for (size_t i = 0; i < rules().buildings.size() && plant == kNone; ++i) {
        if (rules().buildings[i].powerPerResource > 0 && !rules().buildings[i].wonder) plant = static_cast<TypeIndex>(i);
    }
    REQUIRE(plant != kNone);
    GameState s = wcState();
    for (size_t i = 0; i < s.players[0].techs.done.size(); ++i) s.players[0].techs.done[i] = 1;
    for (size_t i = 0; i < s.players[0].civics.done.size(); ++i) s.players[0].civics.done[i] = 1;
    auto open = Game::fromScenario(rules(), s);
    auto banned = withResolution("RESOLUTION_GLOBAL_ENERGY_TREATY", 0, plant, s);
    const ProductionItem item{ProductionKind::Building, plant};
    CommandError why = CommandError::Ok;
    CHECK(!banned->canProduce(banned->state().cities[0], item, &why));
    CHECK(why == CommandError::CannotBuild);
    // Espionage Pact B: the operation is closed; A: two levels more.
    auto closed = withResolution("RESOLUTION_ESPIONAGE_PACT", 1, static_cast<int32_t>(SpyMission::SiphonFunds));
    Agent spy;
    spy.owner = 0;
    CHECK_EQ(closed->spyOperationLevels(spy, SpyMission::StealTechBoost), open->spyOperationLevels(spy, SpyMission::StealTechBoost));
    auto pact = withResolution("RESOLUTION_ESPIONAGE_PACT", 0, static_cast<int32_t>(SpyMission::SiphonFunds));
    CHECK_EQ(pact->spyOperationLevels(spy, SpyMission::SiphonFunds), open->spyOperationLevels(spy, SpyMission::SiphonFunds) + 2);
    // World Ideology A: a Wildcard slot more under the chosen government.
    GameState gov = wcState();
    const TypeIndex monarchy = rules().government("GOVERNMENT_MONARCHY");
    gov.players[0].government = monarchy;
    gov.players[0].policies.assign(static_cast<size_t>(rules().governments[at(monarchy)].totalSlots()), kNone);
    auto ideology = withResolution("RESOLUTION_WORLD_IDEOLOGY", 0, monarchy, gov);
    ideology->syncPolicySlots(0);
    CHECK_EQ(ideology->state().players[0].policies.size(), static_cast<size_t>(rules().governments[at(monarchy)].totalSlots() + 1));
    // Border Control Treaty B: borders stop growing.
    GameState grow = wcState();
    grow.cities[0].borderCulture = Fixed::fromInt(10000);
    auto stuck = withResolution("RESOLUTION_BORDER_CONTROL_TREATY", 1, 0, grow);
    auto free = Game::fromScenario(rules(), grow);
    sovtest::endTurns(*stuck, 2);
    sovtest::endTurns(*free, 2);
    CHECK(stuck->state().cities[0].plotsByCulture < free->state().cities[0].plotsByCulture);
}

TEST(luxury_policy_and_deforestation_treaty) {
    GameState s = wcState();
    TypeIndex lux = kNone;
    for (size_t i = 0; i < rules().resources.size() && lux == kNone; ++i) {
        if (rules().resources[i].cls == ResourceClass::Luxury && rules().resources[i].reveal.none()) lux = static_cast<TypeIndex>(i);
    }
    REQUIRE(lux != kNone);
    s.plot({4, 6}).resource = lux;  // the capital's center counts as improved
    auto plain = Game::fromScenario(rules(), s);
    auto banned = withResolution("RESOLUTION_LUXURY_POLICY", 1, lux, s);
    CHECK_EQ(banned->luxuryAmenities(banned->state().cities[0]), plain->luxuryAmenities(plain->state().cities[0]) - 1);
    // Option A: the luxury reaches eight cities instead of four.
    for (Hex h : {Hex{4, 10}, Hex{4, 2}, Hex{10, 10}, Hex{10, 2}, Hex{24, 10}}) addCity(s, 0, h, false, 3);
    const auto reached = [](const Game& g) {
        int n = 0;
        for (const City& c : g.state().cities) n += c.owner == 0 ? g.luxuryAmenities(c) : 0;
        return n;
    };
    CHECK_EQ(reached(*Game::fromScenario(rules(), s)), 4);
    CHECK_EQ(reached(*withResolution("RESOLUTION_LUXURY_POLICY", 0, lux, s)), 6);
    // Deforestation Treaty A: woods may not be chopped.
    GameState w = wcState();
    const TypeIndex forest = rules().feature("FEATURE_FOREST");
    w.plot({5, 6}).feature = forest;
    for (size_t i = 0; i < w.players[0].techs.done.size(); ++i) w.players[0].techs.done[i] = 1;
    auto chop = Game::fromScenario(rules(), w);
    auto noChop = withResolution("RESOLUTION_DEFORESTATION_TREATY", 0, forest, w);
    CHECK(chop->canHarvestAt(0, {5, 6}));
    CHECK(!noChop->canHarvestAt(0, {5, 6}));
}

// Mercenary Companies (Civilopedia): "+100% cost when producing or purchasing military units using this currency type"
// (A), "-50% cost" (B); the target is Production, Gold or Faith.
TEST(mercenary_companies_change_what_military_units_cost) {
    const ProductionItem warrior{ProductionKind::Unit, rules().unit("UNIT_WARRIOR")};
    const ProductionItem builder{ProductionKind::Unit, rules().unit("UNIT_BUILDER")};
    const int32_t production = static_cast<int32_t>(YieldType::Production), gold = static_cast<int32_t>(YieldType::Gold),
                  faith = static_cast<int32_t>(YieldType::Faith);
    GameState s = wcState();
    s.cities[0].queue = {warrior};
    s.players[0].government = rules().government("GOVERNMENT_THEOCRACY");  // land combat units for Faith
    auto plain = Game::fromScenario(rules(), s);
    // Gold.
    const int price = plain->purchaseCost(0, warrior);
    REQUIRE(price > 0 && price % 10 == 0);
    CHECK_EQ(withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, gold, s)->purchaseCost(0, warrior), 2 * price);
    CHECK_EQ(withResolution("RESOLUTION_MERCENARY_COMPANIES", 1, gold, s)->purchaseCost(0, warrior), price / 2 / 5 * 5);
    CHECK_EQ(withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, gold, s)->purchaseCost(0, builder), plain->purchaseCost(0, builder));
    CHECK_EQ(withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, production, s)->purchaseCost(0, warrior), price);
    // Faith, at Theocracy's price: twice as dear, the Gold price untouched.
    auto devout = withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, faith, s);
    const int faithPrice = plain->faithPurchaseCost(0, plain->state().cities[0], warrior);
    REQUIRE(faithPrice > 0);
    CHECK_EQ(devout->faithPurchaseCost(0, devout->state().cities[0], warrior), 2 * faithPrice);
    CHECK_EQ(devout->purchaseCost(0, warrior), price);
    // Grand Master's Chapel: land combat units for Faith at their Gold price, which only Faith's resolution changes.
    GameState chapel = s;
    chapel.cities[0].buildings.push_back(rules().building("BUILDING_GRAND_MASTER_S_CHAPEL"));
    std::sort(chapel.cities[0].buildings.begin(), chapel.cities[0].buildings.end());
    auto chapelFaith = withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, faith, chapel);
    auto chapelGold = withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, gold, chapel);
    CHECK_EQ(chapelFaith->faithPurchaseCost(0, chapelFaith->state().cities[0], warrior), 2 * price);
    CHECK_EQ(chapelGold->faithPurchaseCost(0, chapelGold->state().cities[0], warrior), price);
    // Production: the Warrior comes at half speed (A) or twice (B); Gold chosen leaves it alone.
    const auto progress = [&](Game& g) {
        sovtest::endTurns(g, 2);
        for (const ProductionProgress& p : g.state().cities[0].progress) {
            if (p.item == warrior) return p.amount;
        }
        return Fixed();
    };
    const Fixed made = progress(*plain);
    REQUIRE(made > Fixed());
    CHECK_EQ(progress(*withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, production, s)), made / 2);
    CHECK_EQ(progress(*withResolution("RESOLUTION_MERCENARY_COMPANIES", 1, production, s)), made * 2);
    CHECK_EQ(progress(*withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, gold, s)), made);
    // Every major civ's military units, support units and aircraft included; not religious units, Rock Bands or spies
    // (all in the military layer), and not city-states'.
    auto dear = withResolution("RESOLUTION_MERCENARY_COMPANIES", 0, gold, s);
    CHECK_EQ(dear->mercenaryPercent(0, rules().unit("UNIT_BATTERING_RAM"), YieldType::Gold), 200);
    CHECK_EQ(dear->mercenaryPercent(1, rules().unit("UNIT_BIPLANE"), YieldType::Gold), 200);
    CHECK_EQ(dear->mercenaryPercent(0, rules().unit("UNIT_WARRIOR_MONK"), YieldType::Gold), 200);
    for (const char* id : {"UNIT_SPY", "UNIT_APOSTLE", "UNIT_INQUISITOR", "UNIT_ROCK_BAND", "UNIT_SETTLER", "UNIT_SOVEREIGN"})
        CHECK_EQ(dear->mercenaryPercent(0, rules().unit(id), YieldType::Gold), 100);
    dear->stateMutForTests().players[1].cityState = 0;
    CHECK_EQ(dear->mercenaryPercent(1, rules().unit("UNIT_WARRIOR"), YieldType::Gold), 100);
}

// The AI's votes: Mercenary Companies on Production, -50% while at war with a major civ and +100% in peace; Arms Control
// B on the rival holding the most devices, else A on itself.
TEST(ai_votes_on_mercenary_companies_and_arms_control) {
    // A session that can only draw this resolution; returns the AI's (player 1's) vote on it.
    const auto aiVote = [](const char* resolution, GameState s) {
        Rules only = rules();
        for (ResolutionType& res : only.resolutions) {
            if (res.id != resolution) res.kind = ResolutionKind::Unsupported;
        }
        s.gameEra = rules().era("ERA_ATOMIC");
        s.nextCongressTurn = s.turn;
        auto g = Game::fromScenario(only, std::move(s));
        sovtest::endTurns(*g, 2);
        CongressVote out;
        out.player = kNoPlayer;
        for (const CongressItem& item : g->state().congress) {
            for (const CongressVote& v : item.votes) {
                if (v.player == 1) {
                    out = v;
                    out.target = item.candidates[static_cast<size_t>(v.target)];  // the candidate itself
                }
            }
        }
        return out;
    };
    const int32_t production = static_cast<int32_t>(YieldType::Production);
    GameState s = wcState();
    const CongressVote peace = aiVote("RESOLUTION_MERCENARY_COMPANIES", s);
    REQUIRE(peace.player == 1);
    CHECK_EQ(static_cast<int>(peace.option), 0);
    CHECK_EQ(peace.target, production);
    s.players[0].relations.resize(2);
    s.players[1].relations.resize(2);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    const CongressVote war = aiVote("RESOLUTION_MERCENARY_COMPANIES", s);
    CHECK_EQ(static_cast<int>(war.option), 1);
    CHECK_EQ(war.target, production);

    GameState n = wcState();
    const CongressVote unarmed = aiVote("RESOLUTION_ARMS_CONTROL", n);
    REQUIRE(unarmed.player == 1);
    CHECK_EQ(static_cast<int>(unarmed.option), 0);
    CHECK_EQ(unarmed.target, 1);
    n.players[0].wmds[at(rules().wmd("WMD_NUCLEAR_DEVICE"))] = 2;
    const CongressVote armed = aiVote("RESOLUTION_ARMS_CONTROL", n);
    CHECK_EQ(static_cast<int>(armed.option), 1);
    CHECK_EQ(armed.target, 0);
}
