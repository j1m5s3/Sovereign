// Diplomacy: opinions and agendas, deals, denouncing, friendship, open borders and peace
// (08-diplomacy-city-states-governors.md; leaders-and-art-style.md, the twelve agendas).
#include <algorithm>

#include "helpers.h"
#include "sovereign/ai.h"
#include "sovereign/serialize.h"

using namespace sov;
using sovtest::addCity;
using sovtest::addUnit;
using sovtest::flatState;
using sovtest::rules;

namespace {
TypeIndex civIndex(const char* id) {
    for (size_t i = 0; i < rules().civs.size(); ++i) {
        if (rules().civs[i].id == id) return static_cast<TypeIndex>(i);
    }
    return kNone;
}

// Two (or more) civs who have met, each with a capital; player 0 is human, the rest AI.
GameState diploState(int players = 2) {
    GameState s = flatState(30, 14, players);
    for (Player& p : s.players) {
        Game::fitPlayerToRules(p, rules());
        p.met.assign(static_cast<size_t>(players), 1);
        p.gold = Fixed::fromInt(300);
    }
    s.players[0].human = true;
    for (int i = 0; i < players; ++i) addCity(s, static_cast<PlayerId>(i), {4 + 10 * i, 6}, true, 3);
    s.majorsAtStart = players;
    return s;
}

int reason(const Game& g, PlayerId holder, PlayerId about, OpinionReasonKind k) {
    for (const OpinionReason& r : g.opinionReasons(holder, about)) {
        if (r.kind == k) return r.value;
    }
    return 0;
}
}  // namespace

TEST(deal_items_travel_in_the_command) {
    const std::vector<DealItem> items = {{DealItemKind::Gold, 0, 50, kNone}, {DealItemKind::Resource, 1, 1, rules().resource("RESOURCE_WINE")}};
    const Command c = Command::proposeDeal(0, 1, items);
    const std::vector<DealItem> back = dealItems(c);
    REQUIRE(back.size() == 2u);
    CHECK(back[1].kind == DealItemKind::Resource);
    CHECK_EQ(back[1].from, 1);
    CHECK_EQ(back[1].resource, rules().resource("RESOURCE_WINE"));
    Command bad = c;
    bad.data.pop_back();
    CHECK(dealItems(bad).empty());
    bad = c;
    bad.data[0] = 99;
    CHECK(dealItems(bad).empty());
}

TEST(every_leader_has_an_agenda) {
    for (const CivType& c : rules().civs) CHECK(c.agenda != Agenda::None && !c.agendaName.empty());
}

TEST(an_ai_takes_a_gift_and_remembers_it) {
    auto g = Game::fromScenario(rules(), diploState());
    const int before = g->opinionOf(1, 0);
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 100, kNone}})) == CommandError::Ok);
    CHECK(g->state().players[0].gold == Fixed::fromInt(200));
    CHECK(g->state().players[1].gold == Fixed::fromInt(400));
    CHECK_EQ(reason(*g, 1, 0, OpinionReasonKind::Gifts), 10);
    CHECK(g->opinionOf(1, 0) > before);
    CHECK(g->state().events.back().kind == EventKind::DealAccepted);
    CHECK_EQ(describeDeal(rules(), g->state(), Deal{0, 0, 1, 1, {{DealItemKind::Gold, 0, 100, kNone}}}), std::string("England gives 100 Gold"));
}

TEST(an_ai_turns_down_a_lopsided_deal) {
    auto g = Game::fromScenario(rules(), diploState());
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 1, 100, kNone}})) == CommandError::Ok);
    CHECK(g->state().players[0].gold == Fixed::fromInt(300));
    CHECK(g->state().events.back().kind == EventKind::DealRejected);
    // More gold than the giver has is no deal at all.
    CHECK(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 1000, kNone}})) == CommandError::CannotDeal);
    // Nor are deals with civs not yet met.
    GameState s = diploState();
    s.players[0].met.assign(2, 0);
    auto g2 = Game::fromScenario(rules(), std::move(s));
    CHECK(g2->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 10, kNone}})) == CommandError::CannotDeal);
}

TEST(a_human_answers_on_their_own_turn) {
    GameState s = diploState();
    s.players[1].human = true;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 50, kNone}})) == CommandError::Ok);
    REQUIRE(g->state().deals.size() == 1u);
    const int32_t id = g->state().deals[0].id;
    CHECK(g->state().events.back().kind == EventKind::DealProposed);
    // One waiting proposal per pair.
    CHECK(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 10, kNone}})) == CommandError::CannotDeal);
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    REQUIRE(g->submit(Command::answerDeal(1, id, true)) == CommandError::Ok);
    CHECK(g->state().deals.empty());
    CHECK(g->state().players[1].gold >= Fixed::fromInt(350));
    // An unanswered proposal lapses when its maker's next turn begins.
    REQUIRE(g->submit(Command::proposeDeal(1, 0, {{DealItemKind::Gold, 1, 10, kNone}})) == CommandError::Ok);
    sovtest::endTurns(*g, 1);
    CHECK_EQ(g->state().deals.size(), 1u);
    sovtest::endTurns(*g, 1);
    CHECK(g->state().deals.empty());
}

TEST(gold_per_turn_flows_for_thirty_turns) {
    auto g = Game::fromScenario(rules(), diploState());
    const Fixed mine = g->goldPerTurn(0), theirs = g->goldPerTurn(1);
    REQUIRE(mine >= Fixed::fromInt(1));
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::GoldPerTurn, 0, 1, kNone}})) == CommandError::Ok);
    REQUIRE(g->state().agreements.size() == 1u);
    CHECK(g->goldPerTurn(0) == mine - Fixed::fromInt(1));
    CHECK(g->goldPerTurn(1) == theirs + Fixed::fromInt(1));
    // More than the giver earns cannot be promised.
    CHECK(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::GoldPerTurn, 0, 1000, kNone}})) == CommandError::CannotDeal);
}

TEST(a_traded_luxury_moves_its_amenity) {
    GameState s = diploState();
    const TypeIndex wine = rules().resource("RESOURCE_WINE");
    s.plot({4, 6}).resource = wine;  // on player 0's city center: improved
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->hasLuxury(0, wine));
    REQUIRE(!g->hasLuxury(1, wine));
    const int amenities = g->luxuryAmenities(g->state().cities[1]);
    // Player 1 pays a little gold for wine it lacks: worth it to the AI.
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Resource, 0, 1, wine}, {DealItemKind::Gold, 1, 30, kNone}})) == CommandError::Ok);
    CHECK(g->hasLuxury(1, wine));
    CHECK(!g->hasLuxury(0, wine));  // its only copy
    CHECK_EQ(g->luxuryAmenities(g->state().cities[1]), amenities + 1);
    // The copy is spoken for: it cannot be traded again.
    CHECK(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Resource, 0, 1, wine}})) == CommandError::CannotDeal);
}

TEST(denouncing_sours_and_makes_a_war_formal) {
    auto g = Game::fromScenario(rules(), diploState());
    REQUIRE(g->submit(Command::denounce(0, 1)) == CommandError::Ok);
    CHECK(g->denouncing(0, 1));
    CHECK(g->relationship(1, 0) == Relationship::Denounced);
    CHECK(reason(*g, 1, 0, OpinionReasonKind::DenouncedUs) < 0);
    CHECK(g->submit(Command::denounce(0, 1)) == CommandError::CannotDenounce);
    sovtest::endTurns(*g, 2 * 5);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].warsDeclared, 1);
    CHECK_EQ(g->state().players[0].surpriseWars, 0);
    CHECK(reason(*g, 1, 0, OpinionReasonKind::DeclaredWar) < 0);
    CHECK(g->relationship(1, 0) == Relationship::AtWar);
}

TEST(a_surprise_war_is_remembered_by_everyone) {
    auto g = Game::fromScenario(rules(), diploState(3));
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].surpriseWars, 1);
    CHECK_EQ(reason(*g, 1, 0, OpinionReasonKind::SurpriseWar), -24);
    CHECK_EQ(reason(*g, 2, 0, OpinionReasonKind::Warmonger), -6);
    // Rome's Pax Romana weighs it too.
    CHECK(g->agendaOpinion(2, 0) < 0);
}

TEST(friendship_needs_liking_and_rules_out_denouncing) {
    auto g = Game::fromScenario(rules(), diploState());
    const std::vector<DealItem> friendship = {{DealItemKind::Friendship, 0, 0, kNone}};
    REQUIRE(g->submit(Command::proposeDeal(0, 1, friendship)) == CommandError::Ok);
    CHECK(!g->friends(0, 1));
    // A generous gift wins the AI over.
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 200, kNone}})) == CommandError::Ok);
    REQUIRE(g->opinionOf(1, 0) >= 12);
    REQUIRE(g->submit(Command::proposeDeal(0, 1, friendship)) == CommandError::Ok);
    CHECK(g->friends(0, 1));
    CHECK(g->friends(1, 0));
    CHECK(g->relationship(1, 0) == Relationship::DeclaredFriend);
    CHECK(!g->canDenounce(0, 1));
}

TEST(open_borders_let_units_through) {
    GameState s = diploState();
    const TypeIndex early = rules().civic("CIVIC_EARLY_EMPIRE");
    s.players[1].civics.done[static_cast<size_t>(early)] = 1;
    const UnitId scout = addUnit(s, "UNIT_SCOUT", 0, {11, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->submit(Command::move(0, scout, {13, 6})) == CommandError::NoPath);
    // Player 1 sells passage for gold.
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::OpenBorders, 1, 0, kNone}, {DealItemKind::Gold, 0, 20, kNone}})) == CommandError::Ok);
    CHECK(g->grantsOpenBorders(1, 0));
    CHECK(!g->grantsOpenBorders(0, 1));
    CHECK(g->submit(Command::move(0, scout, {13, 6})) == CommandError::Ok);
}

TEST(a_losing_ai_takes_peace) {
    GameState s = diploState();
    s.turn = 20;
    for (int a = 0; a < 2; ++a) {
        s.players[static_cast<size_t>(a)].relations.resize(2);
        Relation& r = s.players[static_cast<size_t>(a)].relations[static_cast<size_t>(1 - a)];
        r.war = true;
        r.since = 5;
    }
    addUnit(s, "UNIT_WARRIOR", 0, {6, 6});
    auto g = Game::fromScenario(rules(), std::move(s));
    // At war, only a peace deal can be struck.
    CHECK(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 0, 10, kNone}})) == CommandError::CannotDeal);
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Peace, 0, 0, kNone}})) == CommandError::Ok);
    CHECK(!g->atWar(0, 1));
    CHECK(reason(*g, 1, 0, OpinionReasonKind::MadePeace) > 0);
}

TEST(agendas_judge_deeds) {
    GameState s = diploState(2);
    s.players[1].civ = civIndex("CIVILIZATION_PERSIA");
    s.players[0].citiesRazed = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->agendaOpinion(1, 0), -8);  // Tolerant Conqueror
    GameState s2 = diploState(2);
    s2.players[1].civ = civIndex("CIVILIZATION_AZTEC");
    s2.players[0].warsDeclared = 2;
    auto g2 = Game::fromScenario(rules(), std::move(s2));
    CHECK_EQ(g2->agendaOpinion(1, 0), 4);  // Honourable War respects formal wars
    GameState s3 = diploState(2);
    s3.players[1].civ = civIndex("CIVILIZATION_ARABIA");
    s3.players[0].assassinsSent = 1;
    auto g3 = Game::fromScenario(rules(), std::move(s3));
    CHECK_EQ(g3->agendaOpinion(1, 0), -6);  // Magnanimous
    CHECK(g3->relationship(1, 0) == Relationship::Neutral);
}

TEST(an_ai_offers_friendship_to_a_civ_it_likes) {
    GameState s = diploState();
    OpinionMemory m;
    m.about = 0;
    m.kind = MemoryKind::Gift;
    m.amount = 30;
    m.duration = 30;
    m.turn = 1;
    s.players[1].memories.push_back(m);
    auto g = Game::fromScenario(rules(), std::move(s));
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    ai::playTurn(*g);
    REQUIRE(g->state().deals.size() == 1u);
    const Deal& d = g->state().deals[0];
    CHECK_EQ(d.from, 1);
    CHECK(d.items[0].kind == DealItemKind::Friendship);
    REQUIRE(g->submit(Command::answerDeal(0, d.id, true)) == CommandError::Ok);
    CHECK(g->friends(0, 1));
}

TEST(conversation_summaries_are_recorded_and_capped) {
    auto g = Game::fromScenario(rules(), diploState());
    for (int i = 0; i < kTalksKept + 2; ++i) REQUIRE(g->submit(Command::recordTalk(0, 1, "Talk " + std::to_string(i))) == CommandError::Ok);
    const auto talks = g->talksBetween(1, 0);
    REQUIRE(talks.size() == static_cast<size_t>(kTalksKept));
    CHECK_EQ(talks.back()->text, std::string("Talk ") + std::to_string(kTalksKept + 1));
    CHECK_EQ(talks.front()->text, std::string("Talk 2"));
    CHECK(g->submit(Command::recordTalk(0, 1, std::string(kMaxTalkText + 1, 'x'))) == CommandError::CannotDeal);
    CHECK(g->submit(Command::recordTalk(0, 1, "bad\x01")) == CommandError::CannotDeal);
    CHECK(g->submit(Command::recordTalk(0, 1, "")) == CommandError::CannotDeal);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->talksBetween(0, 1).size(), talks.size());
    CHECK_EQ(loaded->log().back().text, g->log().back().text);
}

TEST(diplomacy_survives_a_save) {
    auto g = Game::fromScenario(rules(), diploState());
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::GoldPerTurn, 0, 1, kNone}})) == CommandError::Ok);
    REQUIRE(g->submit(Command::denounce(0, 1)) == CommandError::Ok);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().agreements.size(), 1u);
    CHECK(loaded->denouncing(0, 1));
    CHECK_EQ(loaded->state().players[1].memories.size(), g->state().players[1].memories.size());
    CHECK_EQ(loaded->log().size(), g->log().size());
    CHECK(loaded->log()[0].data == g->log()[0].data);
    CHECK_EQ(loaded->opinionOf(1, 0), g->opinionOf(1, 0));
    CHECK_EQ(loaded->stateHash(), g->stateHash());
}

// ---- war weariness (08: War weariness)

TEST(fighting_abroad_and_losses_bring_war_weariness) {
    GameState s = diploState();
    s.players[0].relations.resize(2);
    s.players[1].relations.resize(2);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    const UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {13, 6});  // next to player 1's capital at (14,6), on its land
    const UnitId d = addUnit(s, "UNIT_SCOUT", 1, {13, 7});
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->state().plot({13, 7}).owner == 1);
    REQUIRE(g->submit(Command::attack(0, a, {13, 7})) == CommandError::Ok);
    // The attacker fought on foreign ground (+2); the defender at home (0), but lost its Scout (+3).
    CHECK_EQ(g->state().players[0].warWeariness[1], 2);
    const bool killed = g->state().unit(d) == nullptr;
    CHECK_EQ(g->state().players[1].warWeariness.empty() ? 0 : g->state().players[1].warWeariness[0], killed ? 3 : 0);
}

TEST(war_weariness_costs_amenities_and_fades) {
    GameState s = diploState();
    s.players[0].relations.resize(2);
    s.players[1].relations.resize(2);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    auto calm = Game::fromScenario(rules(), s);
    const int before = calm->cityReport(calm->state().cities[0].id).amenities;
    s.players[0].warWeariness = {0, 850};
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->warWearinessAmenities(0), 2);
    CHECK_EQ(g->cityReport(g->state().cities[0].id).amenities, before - 2);
    g->processWarWeariness(0);
    CHECK_EQ(g->state().players[0].warWeariness[1], 800);  // -50 a turn at war
    GameState st = g->state();
    st.players[0].relations[1].war = st.players[1].relations[0].war = false;
    auto peace = Game::fromScenario(rules(), std::move(st));
    peace->processWarWeariness(0);
    CHECK_EQ(peace->state().players[0].warWeariness[1], 600);  // -200 at peace
}

TEST(policies_and_grievances_scale_war_weariness) {
    GameState s = diploState();
    s.players[0].government = rules().government("GOVERNMENT_FASCISM");
    auto g = Game::fromScenario(rules(), s);
    g->addWarWeariness(0, 1, 100);
    CHECK_EQ(g->state().players[0].warWeariness[1], 120);  // Fascism +20%
    s.players[0].government = kNone;
    s.players[0].grievances.assign(2, 0);
    s.players[0].grievances[1] = 300;  // held against the enemy: -30%
    auto h = Game::fromScenario(rules(), std::move(s));
    h->addWarWeariness(0, 1, 100);
    CHECK_EQ(h->state().players[0].warWeariness[1], 70);
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*h), &err);
    REQUIRE(loaded);
    CHECK_EQ(loaded->state().players[0].warWeariness[1], 70);
}

// ---- alliances [R&F] (08: Alliance)

namespace {
// Two declared friends with Civil Service who like each other.
GameState friendsState() {
    GameState s = diploState();
    const TypeIndex civil = rules().civic("CIVIC_CIVIL_SERVICE");
    for (Player& p : s.players) {
        p.civics.done[static_cast<size_t>(civil)] = 1;
        p.relations.resize(2);
    }
    s.players[0].relations[1].friendsUntil = s.players[1].relations[0].friendsUntil = 30;
    for (PlayerId x : {0, 1}) s.players[static_cast<size_t>(x)].memories.push_back({static_cast<PlayerId>(1 - x), MemoryKind::Gift, 40, 100, 0});
    return s;
}
}  // namespace

TEST(friends_with_civil_service_form_an_alliance) {
    GameState s = friendsState();
    auto g = Game::fromScenario(rules(), s);
    const std::vector<DealItem> alliance{{DealItemKind::Alliance, 0, static_cast<int32_t>(AllianceType::Research), kNone}};
    REQUIRE(g->submit(Command::proposeDeal(0, 1, alliance)) == CommandError::Ok);  // the AI answers at once
    CHECK(g->alliance(0, 1) == AllianceType::Research);
    CHECK(g->alliance(1, 0) == AllianceType::Research);
    CHECK_EQ(g->allianceLevel(0, 1), 1);
    // Not twice, and not without friendship.
    CHECK(g->submit(Command::proposeDeal(0, 1, alliance)) != CommandError::Ok);
    s.players[0].relations[1].friendsUntil = s.players[1].relations[0].friendsUntil = 0;
    auto h = Game::fromScenario(rules(), std::move(s));
    CHECK(h->submit(Command::proposeDeal(0, 1, alliance)) != CommandError::Ok);
}

TEST(alliance_points_raise_the_level_and_the_alliance_lapses) {
    GameState s = friendsState();
    for (PlayerId x : {0, 1}) {
        Relation& r = s.players[static_cast<size_t>(x)].relations[static_cast<size_t>(1 - x)];
        r.alliance = AllianceType::Military;
        r.allianceUntil = s.turn + 30;
        r.alliancePoints = rules().globalInt("ALLIANCE_LEVEL_TWO_XP");
    }
    auto g = Game::fromScenario(rules(), s);
    CHECK_EQ(g->allianceLevel(0, 1), 2);
    // Level 2 of a Military alliance: player 0 sees what its ally's units see.
    GameState st = g->state();
    const UnitId scout = addUnit(st, "UNIT_SCOUT", 1, {24, 10});
    const UnitId mine = addUnit(st, "UNIT_SCOUT", 0, {4, 7});
    auto seen = Game::fromScenario(rules(), std::move(st));
    REQUIRE(seen->submit(Command::setActivity(0, mine, Activity::Fortify)) == CommandError::Ok);
    REQUIRE(seen->submit(Command::move(0, mine, {5, 7})) == CommandError::Ok);  // moving refreshes player 0's sight
    CHECK(seen->visibility(0, seen->state().unit(scout)->pos) == Visibility::Visible);
    s.players[0].relations[1].allianceUntil = s.players[1].relations[0].allianceUntil = s.turn - 1;
    auto lapsed = Game::fromScenario(rules(), std::move(s));
    CHECK(lapsed->alliance(0, 1) == AllianceType::None);
}

TEST(allies_strike_harder_at_a_common_foe_and_route_yields) {
    GameState s = friendsState();
    s.players.resize(3);
    Player& foe = s.players[2];
    foe.id = 2;
    Game::fitPlayerToRules(foe, rules());
    for (Player& p : s.players) {
        p.met.assign(3, 1);
        p.relations.resize(3);
    }
    addCity(s, 2, {24, 10}, true, 3);
    for (PlayerId x : {0, 1}) {
        Relation& r = s.players[static_cast<size_t>(x)].relations[static_cast<size_t>(1 - x)];
        r.alliance = AllianceType::Military;
        r.allianceUntil = s.turn + 30;
    }
    for (PlayerId x : {0, 1}) s.players[static_cast<size_t>(x)].relations[2].war = s.players[2].relations[static_cast<size_t>(x)].war = true;
    const UnitId a = addUnit(s, "UNIT_WARRIOR", 0, {20, 10});
    const UnitId d = addUnit(s, "UNIT_WARRIOR", 2, {21, 10});
    s.majorsAtStart = 3;
    auto g = Game::fromScenario(rules(), s);
    const int allied = g->combatStrength(*g->state().unit(a), *g->state().unit(d), true, false);
    s.players[1].relations[2].war = s.players[2].relations[1].war = false;  // the ally is at peace with the foe
    auto alone = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(allied, alone->combatStrength(*alone->state().unit(a), *alone->state().unit(d), true, false) + 5);
    // A Research ally's route carries Science.
    GameState r = friendsState();
    for (PlayerId x : {0, 1}) {
        Relation& rel = r.players[static_cast<size_t>(x)].relations[static_cast<size_t>(1 - x)];
        rel.alliance = AllianceType::Research;
        rel.allianceUntil = r.turn + 30;
    }
    auto withAlly = Game::fromScenario(rules(), r);
    auto plain = Game::fromScenario(rules(), friendsState());
    const Yields y1 = withAlly->tradeRouteYields(withAlly->state().cities[0], withAlly->state().cities[1]);
    const Yields y0 = plain->tradeRouteYields(plain->state().cities[0], plain->state().cities[1]);
    CHECK(y1[static_cast<size_t>(YieldType::Science)] == y0[static_cast<size_t>(YieldType::Science)] + Fixed::fromInt(2));
}

TEST(alliances_survive_a_save) {
    GameState s = friendsState();
    for (PlayerId x : {0, 1}) {
        Relation& r = s.players[static_cast<size_t>(x)].relations[static_cast<size_t>(1 - x)];
        r.alliance = AllianceType::Cultural;
        r.allianceUntil = s.turn + 30;
        r.alliancePoints = 44;
        r.sharedBoostTurns = 7;
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->alliance(0, 1) == AllianceType::Cultural);
    CHECK_EQ(loaded->state().players[0].relations[1].alliancePoints, 44);
    CHECK_EQ(loaded->state().players[0].relations[1].sharedBoostTurns, 7);
}

// ---- casus belli (08: War types)

TEST(a_reconquest_war_costs_no_grievances) {
    GameState s = diploState();
    s.turn = 40;
    for (Player& p : s.players) p.relations.resize(2);
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_DEFENSIVE_TACTICS"))] = 1;
    s.cities[1].originalOwner = 0;  // player 1 holds a city player 0 founded
    auto noDenounce = Game::fromScenario(rules(), s);
    CHECK(!noDenounce->hasCasusBelli(0, 1, CasusBelli::Reconquest));  // denouncement first
    s.players[0].relations[1].denouncedOn = 30;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->denouncing(0, 1));
    REQUIRE(g->hasCasusBelli(0, 1, CasusBelli::Reconquest));
    CHECK(g->bestCasusBelli(0, 1) == CasusBelli::Reconquest);
    CHECK(!g->hasCasusBelli(0, 1, CasusBelli::Colonial));
    const int before = g->grievances(1, 0);
    REQUIRE(g->submit(Command::declareWarFor(0, 1, CasusBelli::Reconquest)) == CommandError::Ok);
    CHECK_EQ(g->grievances(1, 0), before);  // 0%
    CHECK(g->atWar(0, 1));
    // A casus belli it does not hold is refused.
    GameState t = diploState();
    t.turn = 40;
    for (Player& p : t.players) p.relations.resize(2);
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK(h->submit(Command::declareWarFor(0, 1, CasusBelli::Reconquest)) == CommandError::CannotDeclareWar);
}

// ---- promises [GS] (08: Ask Promise)

TEST(a_broken_promise_brings_grievances_and_a_war_of_retribution) {
    GameState s = diploState();
    s.turn = 40;
    for (Player& p : s.players) p.relations.resize(2);
    s.players[0].favor = 100;
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_EARLY_EMPIRE"))] = 1;
    s.players[0].memories.push_back({1, MemoryKind::Gift, 30, 100, 40});   // player 0 likes 1 (irrelevant)
    s.players[1].memories.push_back({0, MemoryKind::Gift, 30, 100, 40});   // player 1 likes 0: it will promise
    const UnitId settler = sovtest::addUnit(s, "UNIT_SETTLER", 1, {9, 6});  // within 6 of player 0's city at (4,6)
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->submit(Command::askPromise(0, 1, PromiseKind::NoSettling)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].favor, 70);
    REQUIRE(g->promised(1, 0, PromiseKind::NoSettling));
    CHECK(g->submit(Command::askPromise(0, 1, PromiseKind::NoSettling)) != CommandError::Ok);  // already promised
    // Player 1 settles near anyway.
    sovtest::endTurns(*g, 1);
    REQUIRE(g->state().currentPlayer == 1);
    const int before = g->grievances(0, 1);
    REQUIRE(g->submit(Command::foundCity(1, settler)) == CommandError::Ok);
    CHECK(!g->promised(1, 0, PromiseKind::NoSettling));
    CHECK_EQ(g->grievances(0, 1), before + 200);
    // A War of Retribution once denounced long enough.
    GameState t = g->state();
    t.players[0].relations[1].denouncedOn = t.turn - 10;
    t.currentPlayer = 0;
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK(h->hasCasusBelli(0, 1, CasusBelli::Retribution));
}

// ---- delegations, embassies and access (08: Access level)

TEST(delegations_and_spies_raise_access_and_bring_gossip) {
    GameState s = diploState(3);
    for (Player& p : s.players) p.relations.resize(3);
    s.players[0].met[2] = 0;
    s.players[2].met[0] = 0;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK_EQ(g->accessLevel(0, 1), 1);  // met: Limited
    CHECK_EQ(g->accessLevel(0, 2), 0);  // not met
    CHECK(g->submit(Command::sendDelegation(0, 1, true)) != CommandError::Ok);  // no embassy before Diplomatic Service
    REQUIRE(g->submit(Command::sendDelegation(0, 1, false)) == CommandError::Ok);
    CHECK(g->state().players[0].gold == Fixed::fromInt(275));
    CHECK_EQ(g->accessLevel(0, 1), 2);  // Open
    CHECK(g->submit(Command::sendDelegation(0, 1, false)) != CommandError::Ok);  // one is enough
    // Gossip: a war between 1 and 2 is heard at Limited; a great person of 1's needs Secret.
    const GameEvent war{1, EventKind::WarDeclared, 1, 2, 0};
    const GameEvent person{1, EventKind::GreatPersonRecruited, 1, kNoPlayer, 0};
    CHECK(g->hearsOf(0, war));
    CHECK(!g->hearsOf(0, person));
    // A level-3 spy in 1's city adds two levels: Top Secret, which shows all its cities.
    GameState t = g->state();
    Agent spy;
    spy.id = 1;
    spy.owner = 0;
    spy.spy = true;
    spy.level = 3;
    spy.city = t.cities[1].id;
    t.agents.push_back(spy);
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->accessLevel(0, 1), 4);
    CHECK(h->hearsOf(0, person));
    CHECK(Game::gossipLevel(EventKind::SpyOperation) == 4);
    // War sends the delegation home.
    GameState u = h->state();
    u.agents.clear();
    auto k = Game::fromScenario(rules(), std::move(u));
    REQUIRE(k->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK_EQ(k->state().players[0].relations[1].delegation, 0);
    CHECK_EQ(k->accessLevel(0, 1), 1);
}

TEST(an_embassy_follows_diplomatic_service_and_brings_favor_with_a_diplomatic_quarter) {
    GameState s = diploState();
    for (Player& p : s.players) p.relations.resize(2);
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_DIPLOMATIC_SERVICE"))] = 1;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->submit(Command::sendDelegation(0, 1, false)) != CommandError::Ok);  // delegations are obsolete
    const int before = g->favorPerTurn(0);
    REQUIRE(g->submit(Command::sendDelegation(0, 1, true)) == CommandError::Ok);
    CHECK_EQ(g->state().players[0].relations[1].delegation, 2);
    CHECK_EQ(g->favorPerTurn(0), before);  // no Diplomatic Quarter yet
    // A Diplomatic Quarter earns 1 Favor a turn for each delegation or embassy it receives, not for one it sends.
    const int theirs = g->favorPerTurn(1);
    CityDistrict dq;
    dq.type = rules().district("DISTRICT_DIPLOMATIC_QUARTER");
    dq.pos = {5, 7};
    dq.complete = true;
    GameState t = g->state();
    t.cities[0].districts.push_back(dq);
    auto h = Game::fromScenario(rules(), std::move(t));
    CHECK_EQ(h->favorPerTurn(0), before);
    dq.pos = {14, 7};
    GameState u = g->state();
    u.cities[1].districts.push_back(dq);
    auto k = Game::fromScenario(rules(), std::move(u));
    CHECK_EQ(k->favorPerTurn(1), theirs + 1);
}

TEST(a_joint_war_is_agreed_and_declared_together) {
    GameState s = diploState(3);
    for (Player& p : s.players) p.relations.resize(3);
    s.turn = 40;
    // Player 1 has an army and holds a grudge against player 2.
    for (int i = 0; i < 4; ++i) addUnit(s, "UNIT_SWORDSMAN", 1, {14, 4 + i});
    OpinionMemory grudge;
    grudge.about = 2;
    grudge.kind = MemoryKind::SurpriseWar;
    grudge.amount = -60;
    grudge.duration = 100;
    grudge.turn = s.turn;
    s.players[1].memories.push_back(grudge);
    const std::vector<DealItem> terms = {{DealItemKind::JointWar, 0, 2, kNone}};
    {
        auto g = Game::fromScenario(rules(), s);
        CHECK(g->dealProblem({0, 0, 1, 0, terms}) != CommandError::Ok);  // the proposer needs Foreign Trade
    }
    s.players[0].civics.done[static_cast<size_t>(rules().civic("CIVIC_FOREIGN_TRADE"))] = 1;
    auto g = Game::fromScenario(rules(), s);
    REQUIRE(g->dealProblem({0, 0, 1, 0, terms}) == CommandError::Ok);
    CHECK(g->dealProblem({0, 0, 1, 0, {{DealItemKind::JointWar, 0, 1, kNone}}}) != CommandError::Ok);  // not on one of them
    const std::vector<DealItem> offer = g->offerableItems(0, 1);
    CHECK(std::any_of(offer.begin(), offer.end(), [](const DealItem& i) { return i.kind == DealItemKind::JointWar && i.amount == 2; }));
    REQUIRE(g->wouldAccept(1, {0, 0, 1, 0, terms}));
    REQUIRE(g->submit(Command::proposeDeal(0, 1, terms)) == CommandError::Ok);
    CHECK(g->atWar(0, 2));
    CHECK(g->atWar(1, 2));
    CHECK(!g->atWar(0, 1));
    CHECK_EQ(g->grievances(2, 0), 100);  // a formal war's grievances, with no denouncement
    // Joining a war already fought: only the newcomer declares.
    s.players[1].relations[2].war = s.players[2].relations[1].war = true;
    auto h = Game::fromScenario(rules(), std::move(s));
    REQUIRE(h->submit(Command::proposeDeal(0, 1, terms)) == CommandError::Ok);
    CHECK(h->atWar(0, 2));
}

TEST(alliance_levels_bring_their_effects) {
    auto allied = [](AllianceType type, int points) {
        GameState s = diploState(3);
        for (Player& p : s.players) p.relations.resize(3);
        for (auto [a, b] : {std::pair<int, int>{0, 1}, {1, 0}}) {
            Relation& r = s.players[static_cast<size_t>(a)].relations[static_cast<size_t>(b)];
            r.alliance = type;
            r.allianceUntil = 1000;
            r.alliancePoints = points;
            r.friendsUntil = 1000;
        }
        return s;
    };
    // Military, level 2 with a war on: +15% toward military units; level 3: trained units have a promotion's XP.
    {
        GameState s = allied(AllianceType::Military, 960);
        s.players[1].relations[2].war = s.players[2].relations[1].war = true;
        auto g = Game::fromScenario(rules(), std::move(s));
        CHECK(g->militaryAllianceAtWar(0));
        CHECK_EQ(g->bestAllianceLevel(0, AllianceType::Military), 3);
    }
    // Religious, level 2: +10 religious strength.
    {
        GameState s = allied(AllianceType::Religious, 320);
        const UnitId m = addUnit(s, "UNIT_MISSIONARY", 0, {4, 7});
        GameState plain = diploState(3);
        const UnitId m2 = addUnit(plain, "UNIT_MISSIONARY", 0, {4, 7});
        auto g = Game::fromScenario(rules(), std::move(s));
        auto h = Game::fromScenario(rules(), std::move(plain));
        CHECK_EQ(g->religiousStrength(*g->state().unit(m), false), h->religiousStrength(*h->state().unit(m2), false) + 10);
    }
    // Cultural, level 3: a fifth of the ally's tourism.
    {
        GameState s = allied(AllianceType::Cultural, 960);
        s.players[1].religion = -1;
        GreatWork w;
        w.type = rules().greatWorkType("SCULPTURE");
        s.cities[1].buildings.push_back(rules().building("BUILDING_AMPHITHEATER"));
        std::sort(s.cities[1].buildings.begin(), s.cities[1].buildings.end());
        w.building = rules().building("BUILDING_AMPHITHEATER");
        for (int i = 0; i < 5; ++i) s.cities[1].greatWorks.push_back(w);
        auto g = Game::fromScenario(rules(), std::move(s));
        REQUIRE(g->tourismBase(1) >= 5);
        CHECK_EQ(g->tourismPerTurn(0), g->tourismBase(0) + g->tourismBase(1) / 5);
    }
}

// A Research alliance at level 2 (08; data: alliance research agreement, Amount=30): every 30 turns at standard
// speed, a Eureka toward a tech the ally has researched or boosted and this civ has neither.
TEST(a_research_alliance_shares_a_eureka_every_30_turns) {
    const size_t flight = static_cast<size_t>(rules().tech("TECH_FLIGHT"));
    const int two = rules().globalInt("ALLIANCE_LEVEL_TWO_XP");
    auto allied = [&](AllianceType type, int points, int16_t turns, bool done, const char* speed) {
        GameState s = diploState(2);
        s.setup.speed = speed;
        for (Player& p : s.players) p.relations.resize(2);
        for (auto [a, b] : {std::pair<int, int>{0, 1}, {1, 0}}) {
            Relation& r = s.players[static_cast<size_t>(a)].relations[static_cast<size_t>(b)];
            r.alliance = type;
            r.allianceUntil = 1000;
            r.alliancePoints = points;
            r.friendsUntil = 1000;
        }
        s.players[0].relations[1].sharedBoostTurns = turns;
        // Flight is the only tech the ally could share: player 0 has researched or boosted every other, and the
        // ally has researched two in three of them.
        for (size_t t = 0; t < rules().techs.size(); ++t) {
            if (t == flight) continue;
            (t % 2 ? s.players[0].techs.boosted : s.players[0].techs.done)[t] = 1;
            if (t % 3 != 2) s.players[1].techs.done[t] = 1;
        }
        (done ? s.players[1].techs.done : s.players[1].techs.boosted)[flight] = 1;
        return Game::fromScenario(rules(), std::move(s));
    };
    auto g = allied(AllianceType::Research, two, 28, true, "GAMESPEED_STANDARD");
    sovtest::endTurns(*g, 2);  // player 0's turn again: its 29th at level 2
    CHECK(!g->state().players[0].techs.boosted[flight]);
    CHECK_EQ(g->state().players[0].relations[1].sharedBoostTurns, 29);
    sovtest::endTurns(*g, 2);  // the 30th
    CHECK(g->state().players[0].techs.boosted[flight]);
    CHECK_EQ(g->state().players[0].relations[1].sharedBoostTurns, 0);
    // A tech the ally only boosted counts too; faster speeds share sooner (Online: every 15 turns).
    auto quick = allied(AllianceType::Research, two, 14, false, "GAMESPEED_ONLINE");
    sovtest::endTurns(*quick, 2);
    CHECK(quick->state().players[0].techs.boosted[flight]);
    // Level 1, or another kind of alliance, shares nothing and keeps no count.
    for (auto [type, points] : {std::pair<AllianceType, int>{AllianceType::Research, 0}, {AllianceType::Cultural, two}}) {
        auto h = allied(type, points, 29, true, "GAMESPEED_STANDARD");
        sovtest::endTurns(*h, 2);
        CHECK(!h->state().players[0].techs.boosted[flight]);
        CHECK_EQ(h->state().players[0].relations[1].sharedBoostTurns, 0);
    }
}

TEST(a_route_between_allies_pays_the_destination_too) {
    GameState s = diploState(2);
    for (Player& p : s.players) p.relations.resize(2);
    for (auto [a, b] : {std::pair<int, int>{0, 1}, {1, 0}}) {
        Relation& r = s.players[static_cast<size_t>(a)].relations[static_cast<size_t>(b)];
        r.alliance = AllianceType::Economic;
        r.allianceUntil = 1000;
        r.friendsUntil = 1000;
    }
    GameState plain = s;
    TradeRoute route;
    route.owner = 1;
    route.origin = s.cities[1].id;
    route.destination = s.cities[0].id;
    route.turnsLeft = 10;
    s.tradeRoutes.push_back(route);
    auto g = Game::fromScenario(rules(), std::move(s));
    auto h = Game::fromScenario(rules(), std::move(plain));
    constexpr size_t G = static_cast<size_t>(YieldType::Gold);
    const CityId mine = g->state().cities[0].id;
    CHECK_EQ(g->cityReport(mine).yields[G], h->cityReport(mine).yields[G] + Fixed::fromInt(2));
}

namespace {
// Player 0 (human) at war with player 1 since turn 0, now turn 30; player 1 holds a second city; player 0 has an army.
GameState warState(bool army) {
    GameState s = diploState(2);
    addCity(s, 1, {20, 11}, false, 2);
    for (PlayerId x : {0, 1}) s.players[static_cast<size_t>(x)].relations.resize(2);
    s.players[0].relations[1].war = s.players[1].relations[0].war = true;
    s.turn = 30;
    if (army) {
        for (int i = 0; i < 8; ++i) addUnit(s, "UNIT_SWORDSMAN", 0, {2 + i, 1});
    }
    return s;
}
}  // namespace

TEST(a_peace_deal_can_cede_a_city) {
    auto g = Game::fromScenario(rules(), warState(true));
    const CityId capital = g->state().cities[1].id, town = g->state().cities[2].id;
    // A capital is never ceded; a city only with peace.
    CHECK(g->dealProblem({0, 0, 1, 30, {{DealItemKind::Peace, 0, 0, kNone}, {DealItemKind::City, 1, capital, kNone}}}) == CommandError::CannotDeal);
    CHECK(g->dealProblem({0, 0, 1, 30, {{DealItemKind::City, 1, town, kNone}}}) == CommandError::CannotDeal);
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Peace, 0, 0, kNone}, {DealItemKind::City, 1, town, kNone}})) == CommandError::Ok);
    CHECK(!g->atWar(0, 1));
    CHECK_EQ(g->state().city(town)->owner, 0);
}

TEST(favor_trades_in_deals) {
    GameState s = diploState(2);
    s.players[1].favor = 100;
    auto g = Game::fromScenario(rules(), std::move(s));
    CHECK(g->dealProblem({0, 0, 1, 0, {{DealItemKind::Favor, 1, 150, kNone}}}) == CommandError::CannotDeal);  // more than it has
    REQUIRE(g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Favor, 1, 50, kNone}, {DealItemKind::Gold, 0, 150, kNone}})) == CommandError::Ok);
    CHECK_EQ(g->state().players[1].favor, 50);
    CHECK_EQ(g->state().players[0].favor, 50);
}

TEST(a_strong_civ_can_make_demands) {
    const auto demand = [](bool army) {
        GameState s = warState(army);
        s.players[0].relations[1].war = s.players[1].relations[0].war = false;
        auto g = Game::fromScenario(rules(), std::move(s));
        g->submit(Command::proposeDeal(0, 1, {{DealItemKind::Gold, 1, 100, kNone}}));
        return std::make_pair(g->state().players[0].gold.toInt(), reason(*g, 1, 0, OpinionReasonKind::Demanded));
    };
    const auto weak = demand(false), strong = demand(true);
    CHECK_EQ(weak.first, 300);
    CHECK_EQ(strong.first, 400);  // it gave in
    CHECK(strong.second < 0);     // and resents it
}

TEST(city_states_join_their_suzerains_wars) {
    GameState s = diploState(3);
    s.players[2].civ = kNone;
    s.players[2].cityState = 0;
    for (Player& p : s.players) {
        p.envoys.assign(3, 0);
        p.relations.resize(3);
    }
    s.players[1].envoys[2] = 3;
    s.majorsAtStart = 2;
    for (int i = 0; i < 8; ++i) addUnit(s, "UNIT_SWORDSMAN", 0, {2 + i, 1});  // player 1 will want peace
    s.turn = 30;
    auto g = Game::fromScenario(rules(), std::move(s));
    REQUIRE(g->suzerainOf(2) == 1);
    REQUIRE(g->submit(Command::declareWar(0, 1)) == CommandError::Ok);
    CHECK(g->atWar(2, 0));  // the suzerain's city-state joins
    GameState later = g->state();
    later.turn += 20;
    auto h = Game::fromScenario(rules(), std::move(later));
    REQUIRE(h->submit(Command::proposeDeal(0, 1, {{DealItemKind::Peace, 0, 0, kNone}, {DealItemKind::Gold, 0, 300, kNone}})) == CommandError::Ok);
    REQUIRE(!h->atWar(0, 1));
    CHECK(!h->atWar(2, 0));  // and makes peace with it
}
