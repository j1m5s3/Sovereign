// Diplomacy: opinions and agendas, deals, denouncing, friendship, open borders and peace
// (08-diplomacy-city-states-governors.md; leaders-and-art-style.md, the twelve agendas).
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
    }
    auto g = Game::fromScenario(rules(), std::move(s));
    std::string err;
    auto loaded = loadGame(rules(), saveGame(*g), &err);
    REQUIRE(loaded);
    CHECK(loaded->alliance(0, 1) == AllianceType::Cultural);
    CHECK_EQ(loaded->state().players[0].relations[1].alliancePoints, 44);
}
