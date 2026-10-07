// Diplomacy between major civs (08-diplomacy-city-states-governors.md: relationship states,
// diplomatic actions, agendas; leaders-and-art-style.md: the twelve Sovereign agendas; leader
// doc §10: the model talks, the game decides). Each civ holds an opinion of each other civ:
// live reasons (war, friendship, religion, trade, its leader's agenda) plus memories that fade
// (wars declared on it, denunciations, gifts, deals kept and broken). Deals trade gold now or
// per turn, luxuries and strategic resources per turn, open borders, a declaration of
// friendship and peace; an AI answers by the deal's gold value against a bar its opinion sets.
#include <algorithm>
#include <cctype>

#include "sovereign/ai.h"
#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
size_t at(PlayerId p) { return static_cast<size_t>(p); }
size_t ti(TypeIndex i) { return static_cast<size_t>(i); }
constexpr int kItemInts = 4;
constexpr int kMaxItems = 10;
constexpr int kFriendlyOpinion = 12;    // at or above: Friendly (and friendship is on the table)
constexpr int kAllyOpinion = 15;       // at or above: an alliance is on the table (the AI's friendship bar)
constexpr int kUnfriendlyOpinion = -12; // at or below: Unfriendly
constexpr int kDealTurns = 30;          // per-turn deal terms (DIPLOMACY_*_TIME_LIMIT)
}  // namespace

Command Command::proposeDeal(PlayerId p, PlayerId to, const std::vector<DealItem>& items) {
    Command c{CommandType::ProposeDeal, p, -1, {}, to, 0};
    for (const DealItem& i : items) {
        c.data.push_back(static_cast<int32_t>(i.kind));
        c.data.push_back(i.from);
        c.data.push_back(i.amount);
        c.data.push_back(i.resource);
    }
    return c;
}

std::vector<DealItem> dealItems(const Command& c) {
    std::vector<DealItem> items;
    if (c.data.empty() || c.data.size() % kItemInts != 0 || c.data.size() > static_cast<size_t>(kItemInts * kMaxItems)) return items;
    for (size_t i = 0; i < c.data.size(); i += kItemInts) {
        if (c.data[i] < 0 || c.data[i] >= kNumDealItemKinds) return {};
        DealItem d;
        d.kind = static_cast<DealItemKind>(c.data[i]);
        d.from = static_cast<PlayerId>(c.data[i + 1]);
        d.amount = c.data[i + 2];
        d.resource = static_cast<TypeIndex>(c.data[i + 3]);
        if (d.from != c.data[i + 1] || d.resource != c.data[i + 3]) return {};
        items.push_back(d);
    }
    return items;
}

// ------------------------------------------------------------------ standing

bool Game::isMajorCiv(PlayerId player) const {
    if (player < 0 || at(player) >= state_.players.size()) return false;
    const Player& p = state_.players[at(player)];
    return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone;
}

bool Game::hasMet(PlayerId a, PlayerId b) const {
    if (a == b || a < 0 || b < 0 || at(a) >= state_.players.size() || at(b) >= state_.players.size()) return false;
    const auto& m = state_.players[at(a)].met;
    return at(b) < m.size() && m[at(b)];
}

bool Game::denouncing(PlayerId by, PlayerId target) const {
    if (by < 0 || target < 0 || by == target) return false;
    const Relation& r = state_.players[at(by)].relations[at(target)];
    return r.denouncedOn > 0 && state_.turn - r.denouncedOn < rules_->globalInt("DIPLOMACY_DENOUNCE_TIME_LIMIT");
}

bool Game::friends(PlayerId a, PlayerId b) const {
    if (a < 0 || b < 0 || a == b) return false;
    return state_.players[at(a)].relations[at(b)].friendsUntil >= state_.turn;
}

AllianceType Game::alliance(PlayerId a, PlayerId b) const {
    if (a < 0 || b < 0 || a == b || at(a) >= state_.players.size() || at(b) >= state_.players.size()) return AllianceType::None;
    const auto& rels = state_.players[at(a)].relations;
    if (at(b) >= rels.size()) return AllianceType::None;
    const Relation& r = rels[at(b)];
    return r.allianceUntil >= state_.turn ? r.alliance : AllianceType::None;
}

int Game::allianceLevel(PlayerId a, PlayerId b) const {
    if (alliance(a, b) == AllianceType::None) return 0;
    const int points = state_.players[at(a)].relations[at(b)].alliancePoints;
    if (points >= rules_->globalInt("ALLIANCE_LEVEL_THREE_XP")) return 3;
    return points >= rules_->globalInt("ALLIANCE_LEVEL_TWO_XP") ? 2 : 1;
}

bool Game::militaryAllianceAtWar(PlayerId player) const {
    // A Military alliance at level 2 (08): +15% military production while it or the ally is at war.
    for (const Player& ally : state_.players) {
        if (alliance(player, ally.id) != AllianceType::Military || allianceLevel(player, ally.id) < 2) continue;
        for (const Player& o : state_.players) {
            if (isMajorCiv(o.id) && (atWar(player, o.id) || atWar(ally.id, o.id))) return true;
        }
    }
    return false;
}

int Game::bestAllianceLevel(PlayerId player, AllianceType type) const {
    int best = 0;
    for (const Player& o : state_.players) {
        if (alliance(player, o.id) == type) best = std::max(best, allianceLevel(player, o.id));
    }
    return best;
}

bool Game::grantsOpenBorders(PlayerId owner, PlayerId to) const {
    if (owner < 0 || to < 0 || owner == to) return false;
    // Gunboat Diplomacy (04): city-states the player has an envoy with open their borders.
    if (isCityState(owner) && at(owner) < state_.players[at(to)].envoys.size() && state_.players[at(to)].envoys[at(owner)] > 0 &&
        policyIs(to, "POLICY_GUNBOAT_DIPLOMACY"))
        return true;
    return state_.players[at(owner)].relations[at(to)].openBordersUntil >= state_.turn;
}

std::vector<const TalkRecord*> Game::talksBetween(PlayerId a, PlayerId b) const {
    std::vector<const TalkRecord*> out;
    for (const TalkRecord& t : state_.talks) {
        if ((t.speaker == a && t.leader == b) || (t.speaker == b && t.leader == a)) out.push_back(&t);
    }
    return out;
}

const Deal* Game::deal(int32_t id) const {
    for (const Deal& d : state_.deals) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

// ------------------------------------------------------------------ luxuries

int Game::luxuryCopies(PlayerId player, TypeIndex resource) const {
    int n = 0;
    // Luxury corporations (07): John Spilsbury's Toys (1), Helena Rubinstein's Cosmetics, Levi Strauss's Jeans, Estée Lauder's Perfume (2 each).
    static const std::pair<Gp, int> kCorporations[] = {{Gp::Spilsbury, 1}, {Gp::Rubinstein, 2}, {Gp::Strauss, 2}, {Gp::Lauder, 2}};
    for (size_t i = 0; i < 4; ++i) {
        if (resource != products_[i] || resource == kNone) continue;
        for (const City& c : state_.cities) {
            if (c.owner == player && !c.greatPeopleHere.empty()) n += kCorporations[i].second * usedHere(c, kCorporations[i].first);
        }
    }
    // Magellan, Colaeus (07): a copy of the luxury each stood on, for good.
    for (TypeIndex lux : state_.players[at(player)].luxuryGrants) n += lux == resource ? 1 : 0;
    // Zanzibar (08: suzerain): Cinnamon and Cloves, found nowhere else.
    if (resource != kNone) {
        const std::string& id = rules_->resources[ti(resource)].id;
        if ((id == "RESOURCE_CINNAMON" || id == "RESOURCE_CLOVES") && suzerainBonus(player, "CITYSTATE_ZANZIBAR")) ++n;
    }
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        const Plot& p = state_.plots[i];
        if (p.owner != player || p.resource != resource) continue;
        const Hex h = state_.grid.at(static_cast<int>(i));
        if (resourceVisible(player, h) && resourceImproved(h)) ++n;
    }
    return n;
}

int Game::luxuryCopiesTraded(PlayerId player, TypeIndex resource) const {
    int n = 0;
    for (const Agreement& a : state_.agreements) {
        if (a.kind == DealItemKind::Resource && a.from == player && a.resource == resource && a.until >= state_.turn) ++n;
    }
    return n;
}

bool Game::hasLuxury(PlayerId player, TypeIndex resource) const {
    if (luxuryCopies(player, resource) - luxuryCopiesTraded(player, resource) > 0) return true;
    // The suzerain gets its city-states' luxuries (08: Suzerain).
    for (const Player& cs : state_.players) {
        if (cs.cityState != kNone && cs.alive && suzerainOf(cs.id) == player && luxuryCopies(cs.id, resource) > 0) return true;
    }
    // Affluence: Amani in a city-state we are suzerain of copies its luxuries (08: Governors).
    for (const Governor& g : state_.players[at(player)].governors) {
        const City* c = state_.city(g.city);
        if (!c || g.establishTurns > 0 || !governorHasPromotion(g, "GOVERNOR_PROMOTION_AFFLUENCE")) continue;
        if (state_.players[at(c->owner)].cityState != kNone && suzerainOf(c->owner) == player && luxuryCopies(c->owner, resource) > 0) return true;
    }
    return std::any_of(state_.agreements.begin(), state_.agreements.end(), [&](const Agreement& a) {
        return a.kind == DealItemKind::Resource && a.to == player && a.resource == resource && a.until >= state_.turn;
    });
}

// ------------------------------------------------------------------ opinion

namespace {
int clampTo(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

bool isNavy(const Rules& r, const Unit& u) {
    const UnitType& t = r.units[ti(u.type)];
    return t.domain == Domain::Sea && t.layer == UnitLayer::Military && (t.combat > 0 || t.ranged > 0);
}
}  // namespace

int Game::agendaOpinion(PlayerId holder, PlayerId about) const {
    const Player& h = state_.players[at(holder)];
    const Player& x = state_.players[at(about)];
    if (h.civ == kNone || ti(h.civ) >= rules_->civs.size()) return 0;
    auto citiesOf = [&](PlayerId p) {
        std::vector<const City*> out;
        for (const City& c : state_.cities) if (c.owner == p) out.push_back(&c);
        return out;
    };
    auto nearMine = [&](Hex pos, int range) {
        for (const City& c : state_.cities) {
            if (c.owner == holder && state_.grid.distance(c.pos, pos) <= range) return true;
        }
        return false;
    };
    auto wonders = [&](PlayerId p) {
        int n = 0;
        for (const City& c : state_.cities) n += c.owner == p ? static_cast<int>(c.wonders.size()) : 0;
        return n;
    };
    auto routesTo = [&](PlayerId from, PlayerId to) {
        int n = 0;
        for (const TradeRoute& r : state_.tradeRoutes) {
            const City* d = state_.city(r.destination);
            n += r.owner == from && d && d->owner == to ? 1 : 0;
        }
        return n;
    };
    switch (rules_->civs[ti(h.civ)].agenda) {
        case Agenda::None: return 0;
        case Agenda::QueenOfTheSeas: {
            int mine = 0, theirs = 0, near = 0;
            for (const Unit& u : state_.units) {
                if (!isNavy(*rules_, u)) continue;
                if (u.owner == holder) ++mine;
                if (u.owner == about) {
                    ++theirs;
                    if (nearMine(u.pos, 6)) ++near;
                }
            }
            if (near == 0) return 3;
            return theirs > mine ? -8 : -2 * std::min(near, 3);
        }
        case Agenda::DefenderOfTheFaith: {
            int value = 0;
            const int faith = h.religion;
            if (faith >= 0) {
                int follow = 0;
                const auto theirs = citiesOf(about);
                for (const City* c : theirs) follow += cityMajorityReligion(*c) == faith ? 1 : 0;
                if (!theirs.empty() && follow * 2 > static_cast<int>(theirs.size())) value += 6;
            }
            if (x.religion >= 0 && x.religion != faith) {
                int converted = 0;
                for (const City* c : citiesOf(holder)) converted += cityMajorityReligion(*c) == x.religion ? 1 : 0;
                value -= std::min(12, 4 * converted);
            }
            return value;
        }
        case Agenda::PaxRomana: {
            bool war = false;
            for (const Player& o : state_.players) war = war || (isMajorCiv(o.id) && atWar(about, o.id));
            if (!war && x.warsDeclared == 0) return 4;
            return -std::min(10, (war ? 4 : 0) + 2 * x.warsDeclared);
        }
        case Agenda::SpartanPride: {
            const int mine = ai::militaryStrength(*this, holder), theirs = ai::militaryStrength(*this, about);
            if (theirs * 100 >= mine * 80) return 5;
            if (theirs * 100 < mine * 50) return -5;
            return 0;
        }
        case Agenda::TolerantConqueror:
            if (x.citiesRazed > 0) return -std::min(24, 8 * x.citiesRazed);
            return std::min(6, 2 * x.citiesCaptured);
        case Agenda::Magnanimous:
            if (x.assassinsSent > 0) return -std::min(18, 6 * x.assassinsSent);
            return x.surpriseWars == 0 ? 3 : -3;
        case Agenda::FirstEmperor: {
            const int theirs = wonders(about);
            return theirs == 0 ? 2 : -std::min(12, 3 * theirs);
        }
        case Agenda::ClosedCountry: {
            int intruders = routesTo(about, holder);
            for (const Unit& u : state_.units) {
                if (u.owner != about || u.religion < 0) continue;
                if (state_.plot(u.pos).owner == holder) ++intruders;
            }
            return intruders == 0 ? 2 : -std::min(12, 3 * intruders);
        }
        case Agenda::EternalName: {
            const int mine = wonders(holder), theirs = wonders(about);
            if (theirs > mine) return -std::min(12, 3 * (theirs - mine));
            return theirs < mine ? 3 : 0;
        }
        case Agenda::PatronOfTrade:
            return std::min(9, 3 * routesTo(about, holder)) - std::min(12, 6 * x.tradersPlundered);
        case Agenda::HonourableWar: {
            if (x.surpriseWars > 0) return -std::min(24, 8 * x.surpriseWars);
            return std::min(4, 2 * x.warsDeclared);
        }
        case Agenda::SapaInca: {
            int mountains = 0;
            for (const City* c : citiesOf(about)) {
                if (!nearMine(c->pos, 6)) continue;
                bool mountain = false;
                for (const Hex& n : state_.grid.within(c->pos, 1)) {
                    mountain = mountain || rules_->terrains[ti(state_.plot(n).terrain)].relief == Relief::Mountain;
                }
                mountains += mountain ? 1 : 0;
            }
            return -std::min(12, 4 * mountains);
        }
    }
    return 0;
}

std::vector<OpinionReason> Game::opinionReasons(PlayerId holder, PlayerId about) const {
    std::vector<OpinionReason> out;
    if (!isMajorCiv(holder) || !isMajorCiv(about) || holder == about) return out;
    auto add = [&](OpinionReasonKind k, int v) {
        if (v == 0) return;
        for (OpinionReason& r : out) {
            if (r.kind == k) {
                r.value += v;
                return;
            }
        }
        out.push_back({k, v});
    };
    if (atWar(holder, about)) add(OpinionReasonKind::AtWar, -10);
    if (denouncing(about, holder)) add(OpinionReasonKind::DenouncedUs, -6);  // on top of the memory
    if (denouncing(holder, about)) add(OpinionReasonKind::WeDenounced, -6);
    if (friends(holder, about)) add(OpinionReasonKind::Friends, 12);
    if (grantsOpenBorders(about, holder)) add(OpinionReasonKind::OpenBorders, 3);
    // Religion: the same majority faith draws civs together; theirs taking our cities does not.
    {
        const Player& x = state_.players[at(about)];
        std::vector<int> mine(state_.religions.size(), 0), theirs(state_.religions.size(), 0);
        int converted = 0;
        for (const City& c : state_.cities) {
            const int m = cityMajorityReligion(c);
            if (m < 0) continue;
            if (c.owner == holder) ++mine[static_cast<size_t>(m)];
            if (c.owner == about) ++theirs[static_cast<size_t>(m)];
            if (c.owner == holder && m == x.religion && x.religion != state_.players[at(holder)].religion) ++converted;
        }
        const auto top = [](const std::vector<int>& v) {
            int best = -1, n = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (v[i] > n) {
                    n = v[i];
                    best = static_cast<int>(i);
                }
            }
            return best;
        };
        const int a = top(mine), b = top(theirs);
        if (a >= 0 && a == b) add(OpinionReasonKind::SameReligion, 5);
        add(OpinionReasonKind::ConvertingUs, -std::min(9, 3 * converted));
    }
    {
        int routes = 0;
        for (const TradeRoute& r : state_.tradeRoutes) {
            const City* d = state_.city(r.destination);
            routes += r.owner == about && d && d->owner == holder ? 1 : 0;
        }
        add(OpinionReasonKind::TradeRoutes, std::min(6, 2 * routes));
    }
    for (const OpinionMemory& m : state_.players[at(holder)].memories) {
        if (m.about != about) continue;
        const int left = m.duration - (state_.turn - m.turn);
        if (left <= 0 || m.duration <= 0) continue;
        const int v = m.amount * left / m.duration;
        static const OpinionReasonKind kinds[] = {
            OpinionReasonKind::DeclaredWar, OpinionReasonKind::SurpriseWar, OpinionReasonKind::DenouncedUs,
            OpinionReasonKind::MadePeace,   OpinionReasonKind::Gifts,       OpinionReasonKind::Deals,
            OpinionReasonKind::BrokeDeal,   OpinionReasonKind::CapturedCity, OpinionReasonKind::Assassin,
            OpinionReasonKind::PlunderedTrader, OpinionReasonKind::Warmonger, OpinionReasonKind::SpyCaught,
            OpinionReasonKind::UsedWmd, OpinionReasonKind::Demanded,
        };
        add(kinds[static_cast<size_t>(m.kind)], v);
    }
    add(OpinionReasonKind::Agenda, agendaOpinion(holder, about));
    add(OpinionReasonKind::Grievances, -std::min(30, grievances(holder, about) / 10));
    return out;
}

int Game::opinionOf(PlayerId holder, PlayerId about) const {
    int total = 0;
    for (const OpinionReason& r : opinionReasons(holder, about)) total += r.value;
    return clampTo(total, -100, 100);
}

Relationship Game::relationship(PlayerId holder, PlayerId about) const {
    if (atWar(holder, about)) return Relationship::AtWar;
    if (denouncing(holder, about) || denouncing(about, holder)) return Relationship::Denounced;
    if (friends(holder, about)) return Relationship::DeclaredFriend;
    const int o = opinionOf(holder, about);
    if (o >= kFriendlyOpinion) return Relationship::Friendly;
    if (o <= kUnfriendlyOpinion) return Relationship::Unfriendly;
    return Relationship::Neutral;
}

void Game::remember(PlayerId holder, PlayerId about, MemoryKind kind, int amount, int duration) {
    if (!isMajorCiv(holder) || !isMajorCiv(about) || holder == about || amount == 0) return;
    OpinionMemory m;
    m.about = about;
    m.kind = kind;
    m.amount = static_cast<int16_t>(clampTo(amount, -100, 100));
    m.duration = static_cast<int16_t>(std::max(1, duration));
    m.turn = state_.turn;
    state_.players[at(holder)].memories.push_back(m);
}

// ------------------------------------------------------------------ actions

bool Game::canDenounce(PlayerId by, PlayerId target) const {
    return isMajorCiv(by) && isMajorCiv(target) && by != target && hasMet(by, target) && !denouncing(by, target) &&
           !friends(by, target) && !atWar(by, target);
}

const CapturedSpy* Game::dealCaptive(const DealItem& item) const {
    for (const CapturedSpy& c : state_.capturedSpies) {
        if (c.spy.id == item.amount && c.captor == item.from) return &c;
    }
    return nullptr;
}

CommandError Game::dealProblem(const Deal& d) const {
    if (d.from == d.to || !isMajorCiv(d.from) || !isMajorCiv(d.to) || !hasMet(d.from, d.to)) return CommandError::CannotDeal;
    if (d.items.empty() || d.items.size() > static_cast<size_t>(kMaxItems)) return CommandError::CannotDeal;
    const bool war = atWar(d.from, d.to);
    bool peace = false, friendship = false, alliance = false, jointWar = false;
    std::vector<int> openBorders(2, 0);
    std::vector<TypeIndex> luxuries;
    int gold[2] = {0, 0}, perTurn[2] = {0, 0}, favor[2] = {0, 0}, ceded[2] = {0, 0};
    for (const DealItem& i : d.items) {
        if (i.from != d.from && i.from != d.to) return CommandError::CannotDeal;
        const PlayerId other = i.from == d.from ? d.to : d.from;
        const size_t side = i.from == d.from ? 0 : 1;
        const Player& giver = state_.players[at(i.from)];
        switch (i.kind) {
            case DealItemKind::Gold:
                gold[side] += i.amount;
                if (i.amount <= 0 || Fixed::fromInt(gold[side]) > giver.gold) return CommandError::CannotDeal;
                break;
            case DealItemKind::GoldPerTurn:
                perTurn[side] += i.amount;
                if (i.amount <= 0 || Fixed::fromInt(perTurn[side]) > goldPerTurn(i.from)) return CommandError::CannotDeal;
                break;
            case DealItemKind::Resource: {
                if (i.resource < 0 || ti(i.resource) >= rules_->resources.size()) return CommandError::CannotDeal;
                const ResourceType& r = rules_->resources[ti(i.resource)];
                if (r.cls == ResourceClass::Luxury) {
                    // One copy per deal; the giver must still hold a copy it has not traded away.
                    if (std::find(luxuries.begin(), luxuries.end(), i.resource) != luxuries.end()) return CommandError::CannotDeal;
                    luxuries.push_back(i.resource);
                    if (luxuryCopies(i.from, i.resource) - luxuryCopiesTraded(i.from, i.resource) < 1) return CommandError::CannotDeal;
                    const bool already = std::any_of(state_.agreements.begin(), state_.agreements.end(), [&](const Agreement& a) {
                        return a.kind == DealItemKind::Resource && a.from == i.from && a.to == other && a.resource == i.resource;
                    });
                    if (already) return CommandError::CannotDeal;
                } else if (r.cls == ResourceClass::Strategic) {
                    if (i.amount <= 0 || ti(i.resource) >= giver.stockpile.size() || giver.stockpile[ti(i.resource)] < i.amount)
                        return CommandError::CannotDeal;
                } else {
                    return CommandError::CannotDeal;
                }
                break;
            }
            case DealItemKind::OpenBorders: {
                if (war || grantsOpenBorders(i.from, other) || openBorders[side]++) return CommandError::CannotDeal;
                // Only borders that are closed (Early Empire) can be opened.
                bool closed = false;
                for (size_t c = 0; c < rules_->civics.size(); ++c) {
                    closed = closed || (rules_->civics[c].enforceBorders && giver.civics.has(static_cast<TypeIndex>(c)));
                }
                if (!closed) return CommandError::CannotDeal;
                break;
            }
            case DealItemKind::Friendship:
                if (friendship || war || friends(d.from, d.to) || denouncing(d.from, d.to) || denouncing(d.to, d.from))
                    return CommandError::CannotDeal;
                friendship = true;
                break;
            case DealItemKind::Alliance: {
                // Declared friends who both have Civil Service, not allied already (08: Alliance).
                const TypeIndex civil = rules_->civic("CIVIC_CIVIL_SERVICE");
                const auto has = [&](PlayerId x) { return civil != kNone && state_.players[at(x)].civics.has(civil); };
                if (alliance || war || i.amount < 0 || i.amount >= kNumAllianceTypes || !friends(d.from, d.to) || !has(d.from) || !has(d.to) ||
                    this->alliance(d.from, d.to) != AllianceType::None)
                    return CommandError::CannotDeal;
                alliance = true;
                break;
            }
            case DealItemKind::GreatWork: {
                // A work the giver holds, once per deal, with a free slot for it in one of the taker's cities (07).
                const GreatWork* w = dealWork(i);
                if (!w || war) return CommandError::CannotDeal;
                for (const DealItem& j : d.items) {
                    if (&j != &i && j.kind == DealItemKind::GreatWork && j.amount == i.amount && j.resource == i.resource) return CommandError::CannotDeal;
                }
                bool room = false;
                for (const City& c : state_.cities) room = room || (c.owner == other && freeGreatWorkSlot(c, w->type) != kNone);
                if (!room) return CommandError::CannotDeal;
                break;
            }
            case DealItemKind::JointWar: {
                // Both go to war on a third major civ they have met (08: Joint War; the proposer needs Foreign
                // Trade); one may already be at war with it ("join an ongoing war"), not both. Once per deal.
                const PlayerId t = static_cast<PlayerId>(i.amount);
                const TypeIndex trade = rules_->civic("CIVIC_FOREIGN_TRADE");
                if (jointWar || war || i.amount < 0 || static_cast<size_t>(i.amount) >= state_.players.size() || t == d.from || t == d.to || !isMajorCiv(t))
                    return CommandError::CannotDeal;
                if (trade == kNone || !state_.players[at(d.from)].civics.has(trade)) return CommandError::CannotDeal;
                int joining = 0;
                for (PlayerId x : {d.from, d.to}) {
                    if (!hasMet(x, t)) return CommandError::CannotDeal;
                    if (atWar(x, t)) continue;
                    if (!canDeclareWar(x, t)) return CommandError::CannotDeal;
                    ++joining;
                }
                if (joining == 0) return CommandError::CannotDeal;
                jointWar = true;
                break;
            }
            case DealItemKind::Captive: {
                // A spy of the other side's that the giver holds, once per deal.
                const CapturedSpy* c = dealCaptive(i);
                if (!c || c->spy.owner != other) return CommandError::CannotDeal;
                for (const DealItem& j : d.items) {
                    if (&j != &i && j.kind == DealItemKind::Captive && j.amount == i.amount) return CommandError::CannotDeal;
                }
                break;
            }
            case DealItemKind::City: {
                // A city ceded in a peace deal (08: Make Peace): not the giver's capital, nor its last city, once per deal.
                const City* c = state_.city(static_cast<CityId>(i.amount));
                if (!war || !c || c->owner != i.from || c->capital) return CommandError::CannotDeal;
                for (const DealItem& j : d.items) {
                    if (&j != &i && j.kind == DealItemKind::City && j.amount == i.amount) return CommandError::CannotDeal;
                }
                int owned = 0;
                for (const City& o : state_.cities) owned += o.owner == i.from ? 1 : 0;
                if (owned - ++ceded[side] < 1) return CommandError::CannotDeal;
                break;
            }
            case DealItemKind::Favor:
                favor[side] += i.amount;
                if (i.amount <= 0 || favor[side] > giver.favor) return CommandError::CannotDeal;
                break;
            case DealItemKind::Peace: {
                if (peace || !war) return CommandError::CannotDeal;
                const Relation& rel = state_.players[at(d.from)].relations[at(d.to)];
                if (state_.turn - rel.since < rules_->globalInt("DIPLOMACY_WAR_MIN_TURNS")) return CommandError::CannotDeal;
                peace = true;
                break;
            }
        }
    }
    // At war, only a peace deal (with whatever terms) can be struck.
    if (war && !peace) return CommandError::CannotDeal;
    return CommandError::Ok;
}

int Game::dealValue(PlayerId judge, const Deal& d) const {
    const PlayerId other = judge == d.from ? d.to : d.from;
    int cities = 0;
    for (const City& c : state_.cities) cities += c.owner == judge ? 1 : 0;
    const int opinion = opinionOf(judge, other);
    int value = 0;
    for (const DealItem& i : d.items) {
        const bool gives = i.from == judge;
        switch (i.kind) {
            case DealItemKind::Gold: value += gives ? -i.amount : i.amount; break;
            case DealItemKind::GoldPerTurn: {
                const int worth = i.amount * kDealTurns * 2 / 3;  // promised gold is worth less than gold in hand
                value += gives ? -worth : worth;
                break;
            }
            case DealItemKind::Resource: {
                const ResourceType& r = rules_->resources[ti(i.resource)];
                if (r.cls == ResourceClass::Luxury) {
                    const int need = 40 + 10 * std::min(cities, 6);
                    if (gives) {
                        const bool spare = luxuryCopies(judge, i.resource) - luxuryCopiesTraded(judge, i.resource) > 1;
                        value -= spare ? 25 : need;
                    } else {
                        value += hasLuxury(judge, i.resource) ? 10 : need;
                    }
                } else {
                    const int worth = 60 * i.amount;
                    value += gives ? -worth : worth;
                }
                break;
            }
            case DealItemKind::OpenBorders:
                value += gives ? (opinion >= 0 ? -15 : -40) : 15;
                break;
            case DealItemKind::Friendship:
                // Friendship is not bought: a civ declares it only with someone it already likes.
                if (opinion < kFriendlyOpinion) value -= 1000;
                break;
            case DealItemKind::Alliance:
                // Nor is an alliance: only with a friend it likes well.
                if (opinion < kAllyOpinion) value -= 1000;
                break;
            case DealItemKind::GreatWork: {
                // By its tourism; far more when it completes (or would break) a museum's theme (07: Theming).
                const GreatWork* w = dealWork(i);
                if (!w) break;
                const int worth = 60 + 20 * rules_->greatWorkTypes[ti(w->type)].tourism;
                if (gives) {
                    const City* holder = state_.city(i.amount);
                    value -= holder && themed(*holder, w->building) ? 1000 : worth * 3 / 2;
                } else {
                    value += worth + (workCompletesTheme(judge, *w) ? 300 : 0);
                }
                break;
            }
            case DealItemKind::JointWar: {
                // Help in a war it already fights is welcome; a new war only on a civ it dislikes and can match.
                const PlayerId t = static_cast<PlayerId>(i.amount);
                if (atWar(judge, t)) {
                    value += 60;
                } else {
                    const int mine = ai::militaryStrength(*this, judge), theirs = ai::militaryStrength(*this, t);
                    value += opinionOf(judge, t) <= -20 && mine * 100 >= theirs * 80 ? 30 : -1000;
                }
                break;
            }
            case DealItemKind::Captive: {
                // Its own spy back is worth more to it than a caught one is to the catcher (Sovereign's values).
                const CapturedSpy* c = dealCaptive(i);
                const int level = c ? c->spy.level : 1;
                value += gives ? -(20 + 20 * level) : 40 + 40 * level;
                break;
            }
            case DealItemKind::City: {
                // By its size and districts; the giver parts with it dearly (Sovereign's values).
                const City* c = state_.city(static_cast<CityId>(i.amount));
                if (!c) break;
                const int worth = 100 + 30 * c->population + 20 * static_cast<int>(c->districts.size());
                value += gives ? -worth * 3 / 2 : worth;
                break;
            }
            case DealItemKind::Favor: value += gives ? -2 * i.amount : 2 * i.amount; break;  // a point of favor is worth 2 Gold
            case DealItemKind::Peace: {
                const int mine = ai::militaryStrength(*this, judge), theirs = ai::militaryStrength(*this, other);
                const int turns = state_.turn - state_.players[at(judge)].relations[at(other)].since;
                int worth = 0;
                if (mine * 2 < theirs) worth = 400;                   // losing badly
                else if (mine * 100 < theirs * 90) worth = 200;       // losing
                else if (mine * 100 >= theirs * 150) worth = -200;    // winning
                else worth = 25;
                worth += std::min(100, std::max(0, turns - 20) * 5);  // the war drags on
                value += worth;
                break;
            }
        }
    }
    return value;
}

bool Game::wouldAccept(PlayerId judge, const Deal& d) const {
    if (dealProblem(d) != CommandError::Ok) return false;
    const PlayerId other = judge == d.from ? d.to : d.from;
    // Make Demand (08): a deal in which only the judge gives. It yields to a civ at least twice as strong when the
    // price is bearable (Sovereign's values: 200 Gold of worth, +100 per further multiple of strength).
    if (!atWar(judge, other) && std::all_of(d.items.begin(), d.items.end(), [&](const DealItem& i) { return i.from == judge; }) &&
        std::none_of(d.items.begin(), d.items.end(), [](const DealItem& i) { return i.kind == DealItemKind::Friendship || i.kind == DealItemKind::Alliance; })) {
        const int mine = std::max(1, ai::militaryStrength(*this, judge)), theirs = ai::militaryStrength(*this, other);
        if (theirs >= 2 * mine && -dealValue(judge, d) <= 200 + 100 * (theirs / mine - 2)) return true;
    }
    const int opinion = opinionOf(judge, other);
    // Liked civs get a little leeway; disliked ones must overpay.
    const int bar = opinion >= 0 ? -std::min(25, opinion) : std::min(150, -3 * opinion);
    return dealValue(judge, d) >= bar;
}

std::vector<DealItem> Game::offerableItems(PlayerId from, PlayerId to) const {
    std::vector<DealItem> out;
    if (!isMajorCiv(from) || !isMajorCiv(to) || !hasMet(from, to)) return out;
    auto tryItem = [&](DealItem i) {
        Deal d{0, from, to, state_.turn, {i}};
        if (atWar(from, to) && i.kind != DealItemKind::Peace) d.items.push_back({DealItemKind::Peace, from, 0, kNone});
        if (dealProblem(d) == CommandError::Ok) out.push_back(i);
    };
    const Player& p = state_.players[at(from)];
    if (p.gold >= Fixed::fromInt(1)) tryItem({DealItemKind::Gold, from, static_cast<int32_t>(p.gold.toInt()), kNone});
    const Fixed gpt = goldPerTurn(from);
    if (gpt >= Fixed::fromInt(1)) tryItem({DealItemKind::GoldPerTurn, from, static_cast<int32_t>(gpt.toInt()), kNone});
    for (size_t r = 0; r < rules_->resources.size(); ++r) {
        const ResourceType& rt = rules_->resources[r];
        if (rt.cls == ResourceClass::Luxury) tryItem({DealItemKind::Resource, from, 1, static_cast<TypeIndex>(r)});
        else if (rt.cls == ResourceClass::Strategic && r < p.stockpile.size() && p.stockpile[r] > 0)
            tryItem({DealItemKind::Resource, from, p.stockpile[r], static_cast<TypeIndex>(r)});
    }
    tryItem({DealItemKind::OpenBorders, from, 0, kNone});
    tryItem({DealItemKind::Friendship, from, 0, kNone});
    for (int t = 0; t < kNumAllianceTypes; ++t) tryItem({DealItemKind::Alliance, from, t, kNone});
    tryItem({DealItemKind::Peace, from, 0, kNone});
    for (const CapturedSpy& c : state_.capturedSpies) {
        if (c.captor == from && c.spy.owner == to) tryItem({DealItemKind::Captive, from, c.spy.id, kNone});
    }
    for (const Player& t : state_.players) tryItem({DealItemKind::JointWar, from, t.id, kNone});
    if (p.favor > 0) tryItem({DealItemKind::Favor, from, p.favor, kNone});
    for (const City& c : state_.cities) {
        if (c.owner == from) tryItem({DealItemKind::City, from, c.id, kNone});
    }
    int works = 0;
    for (const City& c : state_.cities) {
        if (c.owner != from) continue;
        for (size_t w = 0; w < c.greatWorks.size() && works < 6; ++w) {
            if (themed(c, c.greatWorks[w].building)) continue;
            const size_t before = out.size();
            tryItem({DealItemKind::GreatWork, from, c.id, static_cast<TypeIndex>(w)});
            works += out.size() > before ? 1 : 0;
        }
    }
    return out;
}

// ------------------------------------------------------------------ commands

CommandError Game::validateDiplomacy(const Command& c) const {
    switch (c.type) {
        case CommandType::ProposeDeal: {
            if (c.text.size() > 2000) return CommandError::CannotDeal;
            if (c.arg < 0 || static_cast<size_t>(c.arg) >= state_.players.size()) return CommandError::CannotDeal;
            const std::vector<DealItem> items = dealItems(c);
            if (items.empty()) return CommandError::CannotDeal;
            // One waiting proposal per pair at a time.
            for (const Deal& w : state_.deals) {
                if (w.from == c.player && w.to == c.arg) return CommandError::CannotDeal;
            }
            return dealProblem(Deal{0, c.player, static_cast<PlayerId>(c.arg), state_.turn, items});
        }
        case CommandType::AnswerDeal: {
            const Deal* d = deal(c.id);
            if (!d) return CommandError::NoDeal;
            if (c.player == d->from) return c.arg == 0 ? CommandError::Ok : CommandError::CannotDeal;  // withdraw only
            if (c.player != d->to) return CommandError::NoDeal;
            return c.arg == 0 ? CommandError::Ok : dealProblem(*d);
        }
        case CommandType::RecordTalk: {
            if (c.text.empty() || c.text.size() > kMaxTalkText) return CommandError::CannotDeal;
            for (char ch : c.text) {
                if (static_cast<unsigned char>(ch) < 0x20 && ch != '\n') return CommandError::CannotDeal;
            }
            return c.arg >= 0 && static_cast<size_t>(c.arg) < state_.players.size() && isMajorCiv(static_cast<PlayerId>(c.arg)) &&
                           isMajorCiv(c.player) && hasMet(c.player, static_cast<PlayerId>(c.arg))
                       ? CommandError::Ok
                       : CommandError::CannotDeal;
        }
        case CommandType::Denounce:
            return c.arg >= 0 && static_cast<size_t>(c.arg) < state_.players.size() && canDenounce(c.player, static_cast<PlayerId>(c.arg))
                       ? CommandError::Ok
                       : CommandError::CannotDenounce;
        default: return CommandError::BadTarget;
    }
}

void Game::applyDiplomacy(const Command& c) {
    switch (c.type) {
        case CommandType::ProposeDeal: {
            Deal d{state_.nextDealId++, c.player, static_cast<PlayerId>(c.arg), state_.turn, dealItems(c)};
            state_.players[at(d.from)].relations[at(d.to)].lastProposal = state_.turn;
            if (state_.players[at(d.to)].human) {
                state_.deals.push_back(d);
                pushEvent(EventKind::DealProposed, d.from, d.to, d.id);
            } else if (wouldAccept(d.to, d)) {
                executeDeal(d);
            } else {
                pushEvent(EventKind::DealRejected, d.from, d.to, d.id);
            }
            return;
        }
        case CommandType::AnswerDeal: {
            const auto it = std::find_if(state_.deals.begin(), state_.deals.end(), [&](const Deal& d) { return d.id == c.id; });
            const Deal d = *it;
            state_.deals.erase(it);
            if (c.arg == 1) executeDeal(d);
            else if (c.player == d.to) pushEvent(EventKind::DealRejected, d.from, d.to, d.id);
            return;
        }
        case CommandType::RecordTalk: {
            const PlayerId other = static_cast<PlayerId>(c.arg);
            state_.talks.push_back({state_.turn, c.player, other, c.text});
            // Keep the latest kTalksKept for this pair.
            int seen = 0;
            for (size_t i = state_.talks.size(); i-- > 0;) {
                const TalkRecord& t = state_.talks[i];
                const bool pair = (t.speaker == c.player && t.leader == other) || (t.speaker == other && t.leader == c.player);
                if (pair && ++seen > kTalksKept) state_.talks.erase(state_.talks.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return;
        }
        case CommandType::Denounce: {
            const PlayerId target = static_cast<PlayerId>(c.arg);
            state_.players[at(c.player)].relations[at(target)].denouncedOn = state_.turn;
            remember(target, c.player, MemoryKind::Denounced, -12, rules_->globalInt("DIPLOMACY_DENOUNCE_TIME_LIMIT"));
            addGrievance(target, c.player, rules_->globalInt("GRIEVANCES_FOR_DENOUNCEMENT"));
            // A denunciation also cancels the deals still waiting between them.
            state_.deals.erase(std::remove_if(state_.deals.begin(), state_.deals.end(), [&](const Deal& d) {
                                   return (d.from == c.player && d.to == target) || (d.from == target && d.to == c.player);
                               }),
                               state_.deals.end());
            pushEvent(EventKind::Denounced, c.player, target, 0);
            return;
        }
        default: return;
    }
}

void Game::executeDeal(const Deal& d) {
    const int until = state_.turn + kDealTurns;
    // Great Works change hands first, highest index first in each city so the others keep their places.
    std::vector<DealItem> works;
    for (const DealItem& i : d.items) {
        if (i.kind == DealItemKind::GreatWork && dealWork(i)) works.push_back(i);
    }
    std::sort(works.begin(), works.end(), [](const DealItem& a, const DealItem& b) { return a.amount != b.amount ? a.amount < b.amount : a.resource > b.resource; });
    for (const DealItem& i : works) {
        const PlayerId other = i.from == d.from ? d.to : d.from;
        City& from = *state_.city(i.amount);
        GreatWork w = from.greatWorks[ti(i.resource)];
        for (City& c : state_.cities) {
            if (c.owner != other) continue;
            const TypeIndex slot = freeGreatWorkSlot(c, w.type);
            if (slot == kNone) continue;
            from.greatWorks.erase(from.greatWorks.begin() + i.resource);
            w.building = slot;
            lockArt(w);
            c.greatWorks.push_back(w);
            break;
        }
    }
    for (const DealItem& i : d.items) {
        const PlayerId other = i.from == d.from ? d.to : d.from;
        Player& giver = state_.players[at(i.from)];
        Player& taker = state_.players[at(other)];
        switch (i.kind) {
            case DealItemKind::Gold:
                giver.gold -= Fixed::fromInt(i.amount);
                taker.gold += Fixed::fromInt(i.amount);
                break;
            case DealItemKind::GoldPerTurn:
            case DealItemKind::Resource: {
                Agreement a;
                a.kind = i.kind;
                a.from = i.from;
                a.to = other;
                a.amount = i.amount;
                a.resource = i.resource;
                a.until = until;
                state_.agreements.push_back(a);
                break;
            }
            case DealItemKind::OpenBorders: giver.relations[at(other)].openBordersUntil = until; break;
            case DealItemKind::Friendship:
                state_.players[at(d.from)].relations[at(d.to)].friendsUntil = state_.turn + rules_->globalInt("DIPLOMACY_DECLARED_FRIENDSHIP_TIME_LIMIT");
                state_.players[at(d.to)].relations[at(d.from)].friendsUntil = state_.turn + rules_->globalInt("DIPLOMACY_DECLARED_FRIENDSHIP_TIME_LIMIT");
                pushEvent(EventKind::FriendshipDeclared, d.from, d.to, 0);
                break;
            case DealItemKind::Alliance:
                for (PlayerId x : {d.from, d.to}) {
                    Relation& rel = state_.players[at(x)].relations[at(x == d.from ? d.to : d.from)];
                    if (rel.alliance != static_cast<AllianceType>(i.amount)) rel.alliancePoints = 0;  // a new type starts over
                    rel.alliance = static_cast<AllianceType>(i.amount);
                    rel.allianceUntil = state_.turn + rules_->globalInt("DIPLOMACY_ALLIANCE_TIME_LIMIT");
                }
                break;
            case DealItemKind::Peace: onPeace(d.from, d.to); break;
            case DealItemKind::GreatWork: break;  // moved above
            case DealItemKind::JointWar: break;  // declared below, once the rest of the deal is done
            case DealItemKind::City: break;      // ceded below
            case DealItemKind::Favor:
                giver.favor -= i.amount;
                taker.favor += i.amount;
                break;
            case DealItemKind::Captive: {
                // The spy comes home, idle, keeping its level and promotions.
                auto it = std::find_if(state_.capturedSpies.begin(), state_.capturedSpies.end(),
                                       [&](const CapturedSpy& c) { return c.spy.id == i.amount && c.captor == i.from; });
                if (it == state_.capturedSpies.end()) break;
                const Agent back = it->spy;
                state_.capturedSpies.erase(it);
                const auto slot = std::lower_bound(state_.agents.begin(), state_.agents.end(), back.id,
                                                 [](const Agent& a, int32_t id) { return a.id < id; });
                state_.agents.insert(slot, back);
                break;
            }
        }
    }
    // Cities ceded change hands once peace is made (08: Make Peace).
    for (const DealItem& i : d.items) {
        if (i.kind != DealItemKind::City) continue;
        const City* c = state_.city(static_cast<CityId>(i.amount));
        if (c && c->owner == i.from) transferCity(c->id, i.from == d.from ? d.to : d.from, rules_->globalInt("LOYALTY_AFTER_TRANSFERRED_BY_COMBAT"));
    }
    // A Joint War: whichever of them is not yet at war with the target declares it, as a formal war.
    for (const DealItem& i : d.items) {
        if (i.kind != DealItemKind::JointWar) continue;
        for (PlayerId x : {d.from, d.to}) {
            if (!atWar(x, static_cast<PlayerId>(i.amount))) declareWarOn(x, static_cast<PlayerId>(i.amount), CasusBelli::JointWar);
        }
    }
    // Both remember a deal kept; a deal that only gives is a gift.
    for (PlayerId judge : {d.from, d.to}) {
        const PlayerId other = judge == d.from ? d.to : d.from;
        const bool gift = std::all_of(d.items.begin(), d.items.end(), [&](const DealItem& i) { return i.from == other; });
        const bool demand = std::all_of(d.items.begin(), d.items.end(), [&](const DealItem& i) { return i.from == judge; }) && dealValue(judge, d) < 0;
        if (demand && d.to == judge) remember(judge, other, MemoryKind::Demanded, -10, kDealTurns);  // gave in to a demand (08)
        else if (gift) remember(judge, other, MemoryKind::Gift, std::min(15, std::max(1, dealValue(judge, d) / 10)), kDealTurns);
        else remember(judge, other, MemoryKind::Deal, 3, kDealTurns);
    }
    pushEvent(EventKind::DealAccepted, d.from, d.to, d.id);
}

// Promises [GS] (08: Ask Promise): a civ asks another not to settle near it, convert its cities, spy
// on it or dig in its land, for 30 turns and DIPLOMACY_PROMISE_FAVOR_COST (30) Diplomatic Favor. An AI
// asked answers at once: it promises when it does not dislike the asker or the asker is much stronger;
// a refusal gives the asker 25 grievances against it. (A human asked is taken to promise; Sovereign
// reading until the diplomacy screen asks them.) Doing the deed while a promise holds breaks it: the
// asker gains GRIEVANCE_MULTIPLIER_FOR_BROKEN_PROMISE (200%) of a formal war's 100 grievances, remembers
// it as a broken deal, and holds a War of Retribution casus belli for 30 turns.
CommandError Game::askPromiseProblem(PlayerId asker, PlayerId of, PromiseKind kind) const {
    if (asker == of || !isMajorCiv(asker) || !isMajorCiv(of) || !hasMet(asker, of) || atWar(asker, of)) return CommandError::CannotDeal;
    if (state_.players[at(asker)].favor < 30 || promised(of, asker, kind)) return CommandError::CannotDeal;
    return CommandError::Ok;
}

bool Game::promised(PlayerId by, PlayerId to, PromiseKind kind) const {
    return std::any_of(state_.promises.begin(), state_.promises.end(), [&](const Promise& pr) {
        return pr.by == by && pr.to == to && pr.kind == kind && pr.brokenOn == 0 && pr.until >= state_.turn;
    });
}

bool Game::wouldPromise(PlayerId of, PlayerId asker) const {
    if (state_.players[at(of)].human) return true;
    return opinionOf(of, asker) >= 0 || ai::militaryStrength(*this, asker) >= 2 * ai::militaryStrength(*this, of);
}

void Game::askPromise(PlayerId asker, PlayerId of, PromiseKind kind) {
    state_.players[at(asker)].favor -= 30;
    if (wouldPromise(of, asker)) {
        state_.promises.push_back({of, asker, kind, state_.turn + 30, 0});
    } else {
        addGrievance(asker, of, 25);
    }
}

void Game::breakPromises(PlayerId by, PlayerId to, PromiseKind kind) {
    for (Promise& pr : state_.promises) {
        if (pr.by != by || pr.to != to || pr.kind != kind || pr.brokenOn != 0 || pr.until < state_.turn) continue;
        pr.brokenOn = state_.turn;
        addGrievance(to, by, 100 * rules_->globalInt("GRIEVANCE_MULTIPLIER_FOR_BROKEN_PROMISE") / 100);
        remember(to, by, MemoryKind::BrokeDeal, -15, 60);
    }
}

// Delegations and resident embassies (08: Diplomatic actions; data: DiplomaticActions). A Send Delegation
// (25 Gold, until Diplomatic Service) or a Resident Embassy (50 Gold, from Diplomatic Service; it replaces
// the delegation) goes to a met major at peace; an AI turns it away while it denounces or dislikes the
// sender. It stays until war between them. Each is a Delegate source of access; a receiver with a Diplomatic
// Quarter [GS] gains 1 Favor a turn for each (favorPerTurn). The land path a delegation needs in Civ VI is
// not checked (Sovereign reading).
CommandError Game::delegationProblem(PlayerId from, PlayerId to, bool embassy) const {
    if (from == to || !isMajorCiv(from) || !isMajorCiv(to) || !hasMet(from, to) || atWar(from, to)) return CommandError::CannotDeal;
    const Player& p = state_.players[at(from)];
    const TypeIndex service = rules_->civic("CIVIC_DIPLOMATIC_SERVICE");
    const bool served = service != kNone && p.civics.has(service);
    const uint8_t have = p.relations[at(to)].delegation;
    if (embassy ? (!served || have >= 2) : (served || have >= 1)) return CommandError::CannotDeal;
    if (p.relations[at(to)].lastProposal == state_.turn) return CommandError::CannotDeal;  // turned away this turn
    if (p.gold < Fixed::fromInt(embassy ? 50 : 25)) return CommandError::NotEnoughGold;
    return CommandError::Ok;
}

bool Game::wouldReceive(PlayerId to, PlayerId from) const {
    if (state_.players[at(to)].human) return true;  // a human's leader receives every delegation (Sovereign reading)
    return !denouncing(to, from) && opinionOf(to, from) > -20;
}

void Game::sendDelegation(PlayerId from, PlayerId to, bool embassy) {
    Player& p = state_.players[at(from)];
    if (!wouldReceive(to, from)) {
        p.relations[at(to)].lastProposal = state_.turn;
        pushEvent(EventKind::DealRejected, from, to, -1);
        return;
    }
    p.gold -= Fixed::fromInt(embassy ? 50 : 25);
    p.relations[at(to)].delegation = static_cast<uint8_t>(embassy ? 2 : 1);
    if (!embassy) remember(to, from, MemoryKind::Gift, 3, 30);  // a delegation flatters (+opinion)
}

// Access level (08; data: Visibilities, DiplomaticVisibilitySources): met is Limited; each source adds one,
// up to Top Secret: the Printing tech, a trade route to the civ, a delegation or embassy with it, an
// alliance, and a spy in one of its cities (one more if the spy is level 3 or better).
int Game::accessLevel(PlayerId viewer, PlayerId target) const {
    if (viewer == target) return 4;
    if (viewer < 0 || target < 0 || !hasMet(viewer, target)) return 0;
    const Player& p = state_.players[at(viewer)];
    int level = 1;
    const TypeIndex printing = rules_->tech("TECH_PRINTING");
    level += printing != kNone && p.techs.has(printing) ? 1 : 0;
    level += std::any_of(state_.tradeRoutes.begin(), state_.tradeRoutes.end(), [&](const TradeRoute& t) {
        const City* d = state_.city(t.destination);
        return t.owner == viewer && d && d->owner == target;
    }) ? 1 : 0;
    level += p.relations[at(target)].delegation > 0 ? 1 : 0;
    level += alliance(viewer, target) != AllianceType::None ? 1 : 0;
    level += usedBy(viewer, Gp::Goddard) ? 1 : 0;  // Mary Katherine Goddard (07)
    int spy = 0;
    for (const Agent& a : state_.agents) {
        const City* c = a.spy && a.owner == viewer && a.travel == 0 ? state_.city(a.city) : nullptr;
        if (c && c->owner == target) spy = std::max(spy, a.level >= 3 ? 2 : 1);
    }
    return std::min(4, level + spy);
}

const char* Game::accessName(int level) {
    static const char* const names[] = {"None", "Limited", "Open", "Secret", "Top Secret"};
    return names[std::clamp(level, 0, 4)];
}

// Gossip (08: Access level): what outsiders hear of a civ's doings. Wars, peace, denunciations and
// friendships are heard by all who have met it; deals and new ages from Open; great people and
// historic moments from Secret; its spies' work only at Top Secret (Sovereign reading of the gossip tiers).
int Game::gossipLevel(EventKind kind) {
    switch (kind) {
        case EventKind::WarDeclared:
        case EventKind::PeaceMade:
        case EventKind::Denounced:
        case EventKind::FriendshipDeclared:
        case EventKind::AssassinKilledLeader:
        case EventKind::Rebellion: return 1;
        case EventKind::DealAccepted:
        case EventKind::DealBroken:
        case EventKind::NewAge: return 2;
        case EventKind::GreatPersonRecruited:
        case EventKind::HistoricMoment: return 3;
        case EventKind::SpyOperation: return 4;
        default: return 99;  // private, or world news shown to all anyway
    }
}

bool Game::hearsOf(PlayerId viewer, const GameEvent& e) const {
    if (e.actor == viewer || e.target == viewer) return true;
    const int need = gossipLevel(e.kind);
    if (need > 4) return false;
    const PlayerId about = e.actor != kNoPlayer ? e.actor : e.target;
    if (about == kNoPlayer || !isMajorCiv(about)) return false;
    // A spy running a Listening Post in one of its cities hears all its gossip (08: Espionage).
    for (const Agent& a : state_.agents) {
        const City* c = a.spy && a.owner == viewer && a.travel == 0 && a.mission == SpyMission::ListeningPost ? state_.city(a.city) : nullptr;
        if (c && c->owner == about) return true;
    }
    return accessLevel(viewer, about) >= need;
}

// Casus belli (08: War types). Each needs its civic and its condition, and, except Protectorate,
// DIPLOMACY_DENOUNCE_WAR_DELAY turns of denouncement first; the declaration's grievances are the
// formal war's times the war type's percent.
bool Game::hasCasusBelli(PlayerId player, PlayerId target, CasusBelli why) const {
    if (why == CasusBelli::None || !isMajorCiv(player) || !isMajorCiv(target)) return false;
    const Player& p = state_.players[at(player)];
    const auto has = [&](const char* civic) { const TypeIndex c = rules_->civic(civic); return c != kNone && p.civics.has(c); };
    const Relation& rel = p.relations[at(target)];
    const bool denounced = denouncing(player, target) && state_.turn - rel.denouncedOn >= rules_->globalInt("DIPLOMACY_DENOUNCE_WAR_DELAY");
    switch (why) {
        case CasusBelli::HolyWar: {
            // They converted one of our cities.
            if (!has("CIVIC_DIPLOMATIC_SERVICE") || !denounced) return false;
            for (const City& c : state_.cities) {
                const int maj = c.owner == player ? cityMajorityReligion(c) : -1;
                if (maj >= 0 && static_cast<size_t>(maj) < state_.religions.size() && state_.religions[static_cast<size_t>(maj)].founder == target && p.religion != maj) return true;
            }
            return false;
        }
        case CasusBelli::Liberation:
            // They hold a city of a friend or ally of ours.
            if (!has("CIVIC_DIPLOMATIC_SERVICE") || !denounced) return false;
            for (const City& c : state_.cities) {
                if (c.owner == target && c.originalOwner != target && c.originalOwner != player &&
                    (friends(player, c.originalOwner) || alliance(player, c.originalOwner) != AllianceType::None))
                    return true;
            }
            return false;
        case CasusBelli::Reconquest:
            // They hold a city we used to own.
            if (!has("CIVIC_DEFENSIVE_TACTICS") || !denounced) return false;
            for (const City& c : state_.cities) {
                if (c.owner == target && c.originalOwner == player) return true;
            }
            return false;
        case CasusBelli::Protectorate:
            // They are at war with a city-state we are suzerain of (no denouncement needed).
            if (!has("CIVIC_DEFENSIVE_TACTICS")) return false;
            for (const Player& cs : state_.players) {
                if (isCityState(cs.id) && cs.alive && suzerainOf(cs.id) == player && atWar(target, cs.id)) return true;
            }
            return false;
        case CasusBelli::Colonial:
            return has("CIVIC_NATIONALISM") && denounced && playerEra(player) - playerEra(target) >= 2;
        case CasusBelli::TerritorialExpansion: {
            // Two of our cities within range of two of theirs (DIPLOMACY_ADJACENT_EMPIRE_*).
            if (!has("CIVIC_MOBILIZATION") || !denounced) return false;
            const int range = rules_->globalInt("DIPLOMACY_ADJACENT_EMPIRE_RANGE"), need = rules_->globalInt("DIPLOMACY_ADJACENT_EMPIRE_CITIES_REQUIRED");
            int mine = 0, theirs = 0;
            for (const City& a : state_.cities) {
                if (a.owner != player && a.owner != target) continue;
                bool near = false;
                for (const City& b : state_.cities) {
                    if (b.owner == (a.owner == player ? target : player) && state_.grid.distance(a.pos, b.pos) <= range) near = true;
                }
                if (near) (a.owner == player ? mine : theirs)++;
            }
            return mine >= need && theirs >= need;
        }
        case CasusBelli::Ideological: {
            // Both in tier-3-or-later governments, and different ones.
            if (!has("CIVIC_IDEOLOGY") || !denounced) return false;
            const Player& t = state_.players[at(target)];
            if (p.government == kNone || t.government == kNone || p.government == t.government) return false;
            return rules_->governments[static_cast<size_t>(p.government)].tier >= 3 && rules_->governments[static_cast<size_t>(t.government)].tier >= 3;
        }
        case CasusBelli::GoldenAge:
            // In a Golden Age with the To Arms! dedication, right after denouncing them (09: Dedications).
            return goldenDedication(player, "DEDICATION_TO_ARMS") && denouncing(player, target);
        case CasusBelli::Retribution:
            // They broke a promise to us within the last 30 turns (Early Empire).
            if (!has("CIVIC_EARLY_EMPIRE") || !denounced) return false;
            return std::any_of(state_.promises.begin(), state_.promises.end(), [&](const Promise& pr) {
                return pr.by == target && pr.to == player && pr.brokenOn > 0 && state_.turn - pr.brokenOn <= 30;
            });
        case CasusBelli::JointWar:  // agreed in a deal, never held on its own
        case CasusBelli::None: break;
    }
    return false;
}

int Game::casusBelliGrievancePercent(CasusBelli why) const {
    switch (why) {
        case CasusBelli::HolyWar: return 50;
        case CasusBelli::Liberation: return 0;
        case CasusBelli::Reconquest: return 0;
        case CasusBelli::Protectorate: return 0;
        case CasusBelli::Colonial: return 50;
        case CasusBelli::TerritorialExpansion: return 75;
        case CasusBelli::Ideological: return 50;
        case CasusBelli::Retribution: return 50;
        case CasusBelli::GoldenAge: return 25;
        case CasusBelli::JointWar: return 100;  // Joint War [100%] (08: War types)
        case CasusBelli::None: break;
    }
    return 100;
}

CasusBelli Game::bestCasusBelli(PlayerId player, PlayerId target) const {
    CasusBelli best = CasusBelli::None;
    for (int w = 1; w < kNumCasusBelli; ++w) {
        const CasusBelli why = static_cast<CasusBelli>(w);
        if (hasCasusBelli(player, target, why) && (best == CasusBelli::None || casusBelliGrievancePercent(why) < casusBelliGrievancePercent(best))) best = why;
    }
    return best;
}

void Game::onWarDeclared(PlayerId by, PlayerId target, CasusBelli why) {
    Player& p = state_.players[at(by)];
    Relation& mine = p.relations[at(target)];
    // A formal war follows DIPLOMACY_DENOUNCE_WAR_DELAY turns of denunciation, or has a casus belli; anything else is a surprise.
    const bool justified = why != CasusBelli::None;
    const bool formal = justified || (denouncing(by, target) && state_.turn - mine.denouncedOn >= rules_->globalInt("DIPLOMACY_DENOUNCE_WAR_DELAY"));
    // Betrayal [GS]: war on a declared friend or an ally (08: Emergencies).
    if (isMajorCiv(by) && isMajorCiv(target) && (friends(by, target) || alliance(by, target) != AllianceType::None))
        triggerEmergency(EmergencyKind::Betrayal, by, kNoCity, target);
    // Joining an emergency against the target is an Emergency War: no grievances.
    const bool emergencyWar = inEmergencyAgainst(by, target);
    if (isMajorCiv(target)) {
        ++p.warsDeclared;
        if (!formal) ++p.surpriseWars;
    }
    remember(target, by, formal ? MemoryKind::DeclaredWar : MemoryKind::SurpriseWar, formal ? -12 : -24, formal ? 60 : 80);
    // Grievances [GS]: 100 for a formal war (Sovereign's base; the engine value is unverified), 150%
    // for a surprise; declared friends of the target share 25% (SHARE_WAR_GRIEVANCES_DECLARED_FRIENDS).
    // A casus belli scales the grievances (08: War types).
    const int base = emergencyWar ? 0 : justified ? casusBelliGrievancePercent(why) : formal ? 100 : 150;
    addGrievance(target, by, base);
    if (justified && isMajorCiv(target)) awardMoment(by, "MOMENT_CAUSE_FOR_WAR");  // 09
    // War on a city-state angers the civs with envoys there, and its suzerain more (08: GRIEVANCES_*_CITY_STATE_DOW).
    if (isCityState(target)) {
        const PlayerId suzerain = suzerainOf(target);
        for (const Player& o : state_.players) {
            if (o.id == by || !isMajorCiv(o.id)) continue;
            if (o.id == suzerain) addGrievance(o.id, by, rules_->globalInt("GRIEVANCES_SUZERAIN_CITY_STATE_DOW"));
            else if (envoysAt(o.id, target) > 0) addGrievance(o.id, by, rules_->globalInt("GRIEVANCES_HAVE_ENVOYS_CITY_STATE_DOW"));
        }
    }
    for (const Player& o : state_.players) {
        if (o.id == by || o.id == target) continue;
        if (alliance(o.id, target) != AllianceType::None) addGrievance(o.id, by, base * rules_->globalInt("SHARE_WAR_GRIEVANCES_ALLY") / 100);
        else if (friends(o.id, target)) addGrievance(o.id, by, base * rules_->globalInt("SHARE_WAR_GRIEVANCES_DECLARED_FRIENDS") / 100);
    }
    for (const Player& o : state_.players) {
        if (o.id != by && o.id != target && hasMet(o.id, by)) remember(o.id, by, MemoryKind::Warmonger, formal ? -4 : -6, 40);
    }
    for (PlayerId a : {by, target}) {
        const PlayerId b = a == by ? target : by;
        Relation& r = state_.players[at(a)].relations[at(b)];
        r.friendsUntil = 0;
        r.openBordersUntil = 0;
        r.alliance = AllianceType::None;
        r.allianceUntil = 0;
        r.alliancePoints = 0;
    }
    state_.agreements.erase(std::remove_if(state_.agreements.begin(), state_.agreements.end(), [&](const Agreement& a) {
                                return (a.from == by && a.to == target) || (a.from == target && a.to == by);
                            }),
                            state_.agreements.end());
    state_.deals.erase(std::remove_if(state_.deals.begin(), state_.deals.end(), [&](const Deal& d) {
                           return (d.from == by && d.to == target) || (d.from == target && d.to == by);
                       }),
                       state_.deals.end());
    pushEvent(EventKind::WarDeclared, by, target, formal ? 0 : 1);
}

// War weariness (08: War weariness): points against each opponent from fighting away from home
// (2 a combat on foreign ground), units lost (3 each) and nuclear weapons launched (10). Policies
// and the government scale it (Propaganda -25%, Fascism +20%), and grievances held against the
// enemy soften it (Sovereign: -1% per 10, at most -50%). Each city loses an amenity per 400 points.
// It decays as the player's turn begins: 50 a turn at war with that player, 200 at peace.
int Game::warWeariness(PlayerId player) const {
    int total = 0;
    for (int32_t w : state_.players[at(player)].warWeariness) total += w;
    return total;
}

int Game::warWearinessAmenities(PlayerId player) const {
    return warWeariness(player) / std::max(1, rules_->globalInt("WAR_WEARINESS_POINTS_FOR_AMENITY_LOSS"));
}

void Game::addWarWeariness(PlayerId player, PlayerId against, int points) {
    if (player == against || player < 0 || against < 0 || at(player) >= state_.players.size() || at(against) >= state_.players.size()) return;
    Player& p = state_.players[at(player)];
    if (p.barbarian || state_.players[at(against)].barbarian || points <= 0) return;
    const int policy = static_cast<int>(sumPlayerModifiers(state_, *rules_, p, ModEffect::WarWearinessPercent).toInt());
    const int softened = std::min(50, grievances(player, against) / 10);
    int pct = 100 + policy;
    for (const Emergency& e : state_.emergencies) {
        if (e.outcome == 0 && e.kind == EmergencyKind::Betrayal && e.target == player && at(against) < e.members.size() && e.members[at(against)]) pct += 50;
    }
    const int gained = points * std::max(0, pct) * (100 - softened) / 10000;
    if (gained <= 0) return;
    if (p.warWeariness.size() < state_.players.size()) p.warWeariness.resize(state_.players.size(), 0);
    p.warWeariness[at(against)] += gained;
}

void Game::processWarWeariness(PlayerId player) {
    Player& p = state_.players[at(player)];
    for (size_t o = 0; o < p.warWeariness.size(); ++o) {
        if (p.warWeariness[o] <= 0) continue;
        const int decay = atWar(player, static_cast<PlayerId>(o)) ? rules_->globalInt("WAR_WEARINESS_DECAY_TURN_AT_WAR")
                                                                   : rules_->globalInt("WAR_WEARINESS_DECAY_TURN_AT_PEACE");
        p.warWeariness[o] = std::max(0, p.warWeariness[o] - decay);
    }
}

void Game::onPeace(PlayerId a, PlayerId b) {
    for (PlayerId x : {a, b}) {
        Relation& r = state_.players[at(x)].relations[at(x == a ? b : a)];
        r.war = false;
        r.since = state_.turn;
        r.peaceOffered = false;
    }
    // City-states in a Suzerain War make peace with their suzerain (08).
    if (isMajorCiv(a) && isMajorCiv(b)) {
        for (const Player& cs : state_.players) {
            if (cs.cityState == kNone || !cs.alive) continue;
            const PlayerId suzerain = suzerainOf(cs.id);
            const PlayerId foe = suzerain == a ? b : suzerain == b ? a : kNoPlayer;
            if (foe == kNoPlayer || !atWar(cs.id, foe)) continue;
            for (PlayerId x : {cs.id, foe}) {
                Relation& r = state_.players[at(x)].relations[at(x == cs.id ? foe : cs.id)];
                r.war = false;
                r.since = state_.turn;
            }
        }
    }
    remember(a, b, MemoryKind::MadePeace, 4, kDealTurns);
    remember(b, a, MemoryKind::MadePeace, 4, kDealTurns);
    // Peace lifts most of the weariness at once (WAR_WEARINESS_DECAY_PEACE_DECLARED).
    for (PlayerId x : {a, b}) {
        std::vector<int32_t>& w = state_.players[at(x)].warWeariness;
        const PlayerId other = x == a ? b : a;
        if (at(other) < w.size()) w[at(other)] = std::max(0, w[at(other)] - rules_->globalInt("WAR_WEARINESS_DECAY_PEACE_DECLARED"));
    }
    pushEvent(EventKind::PeaceMade, a, b, 0);
}

void Game::processDiplomacy(PlayerId pid) {
    // Proposals this player made last turn and nobody answered lapse.
    state_.deals.erase(std::remove_if(state_.deals.begin(), state_.deals.end(),
                                      [&](const Deal& d) { return d.from == pid && d.turn < state_.turn; }),
                       state_.deals.end());
    Player& p = state_.players[at(pid)];
    p.favor = std::max(0, p.favor + favorPerTurn(pid));  // Diplomatic Favor [GS]
    // Alliances [R&F]: points each turn, more with trade routes either way; a lapsed one ends.
    for (size_t o = 0; o < p.relations.size(); ++o) {
        Relation& rel = p.relations[o];
        if (rel.alliance == AllianceType::None) continue;
        if (rel.allianceUntil < state_.turn) {
            rel.alliance = AllianceType::None;
            rel.alliancePoints = 0;
            continue;
        }
        int points = rules_->globalInt("ALLIANCE_POINTS_MULTIPLIER");
        bool out = false, in = false;
        for (const TradeRoute& tr : state_.tradeRoutes) {
            const City* dest = state_.city(tr.destination);
            if (!dest) continue;
            out = out || (tr.owner == pid && dest->owner == static_cast<PlayerId>(o));
            in = in || (tr.owner == static_cast<PlayerId>(o) && dest->owner == pid);
        }
        points += (out ? rules_->globalInt("ALLIANCE_POINTS_FOR_TRADE") : 0) + (in ? rules_->globalInt("ALLIANCE_POINTS_FOR_TRADE") : 0);
        // Wisselbanken, Democratic Legacy (04): +1 a turn each.
        points += (policyIs(pid, "POLICY_WISSELBANKEN") ? 1 : 0) + (policyIs(pid, "POLICY_DEMOCRATIC_LEGACY") ? 1 : 0);
        rel.alliancePoints += points;
    }
    allianceEurekas(pid);
    // Strategic resources flow; a giver in debt or out of stock breaks its deals.
    std::vector<size_t> broken;
    for (size_t i = 0; i < state_.agreements.size(); ++i) {
        Agreement& a = state_.agreements[i];
        if (a.from != pid || a.until < state_.turn) continue;
        if (a.kind == DealItemKind::GoldPerTurn && p.gold < Fixed()) broken.push_back(i);
        if (a.kind == DealItemKind::Resource && rules_->resources[ti(a.resource)].cls == ResourceClass::Strategic) {
            if (ti(a.resource) >= p.stockpile.size() || p.stockpile[ti(a.resource)] < a.amount) {
                broken.push_back(i);
                continue;
            }
            Player& to = state_.players[at(a.to)];
            if (to.stockpile.size() <= ti(a.resource)) to.stockpile.resize(rules_->resources.size(), 0);
            p.stockpile[ti(a.resource)] -= a.amount;
            to.stockpile[ti(a.resource)] += a.amount;
        }
    }
    for (size_t k = broken.size(); k-- > 0;) {
        const Agreement a = state_.agreements[broken[k]];
        state_.agreements.erase(state_.agreements.begin() + static_cast<std::ptrdiff_t>(broken[k]));
        remember(a.to, a.from, MemoryKind::BrokeDeal, -10, 40);
        pushEvent(EventKind::DealBroken, a.from, a.to, 0);
    }
    state_.agreements.erase(std::remove_if(state_.agreements.begin(), state_.agreements.end(),
                                           [&](const Agreement& a) { return a.until < state_.turn; }),
                            state_.agreements.end());
    p.memories.erase(std::remove_if(p.memories.begin(), p.memories.end(),
                                    [&](const OpinionMemory& m) { return state_.turn - m.turn >= m.duration; }),
                     p.memories.end());
}

// ------------------------------------------------------------------ text

namespace {
std::string civName(const Rules& r, const GameState& s, PlayerId p) {
    if (p < 0 || at(p) >= s.players.size()) return "?";
    const Player& x = s.players[at(p)];
    if (x.cityState != kNone) return r.cityStates[ti(x.cityState)].name;
    return x.civ == kNone ? std::string("?") : r.civs[ti(x.civ)].name;
}
}  // namespace

std::string describeDealItem(const Rules& r, const GameState& s, const DealItem& i) {
    const std::string who = civName(r, s, i.from);
    switch (i.kind) {
        case DealItemKind::Gold: return who + " gives " + std::to_string(i.amount) + " Gold";
        case DealItemKind::GoldPerTurn: return who + " gives " + std::to_string(i.amount) + " Gold per turn for " + std::to_string(kDealTurns) + " turns";
        case DealItemKind::Resource: {
            const std::string name = i.resource >= 0 && ti(i.resource) < r.resources.size() ? r.resources[ti(i.resource)].name : "?";
            const bool luxury = i.resource >= 0 && ti(i.resource) < r.resources.size() && r.resources[ti(i.resource)].cls == ResourceClass::Luxury;
            return who + " gives " + (luxury ? name : std::to_string(i.amount) + " " + name + " per turn") + " for " + std::to_string(kDealTurns) + " turns";
        }
        case DealItemKind::OpenBorders: return who + " opens its borders for " + std::to_string(kDealTurns) + " turns";
        case DealItemKind::Friendship: return "a declaration of friendship";
        case DealItemKind::Alliance: {
            static const char* const kTypes[] = {"Research", "Military", "Economic", "Cultural", "Religious"};
            return std::string("a ") + (i.amount >= 0 && i.amount < kNumAllianceTypes ? kTypes[i.amount] : "?") + " Alliance for " +
                   std::to_string(r.globalInt("DIPLOMACY_ALLIANCE_TIME_LIMIT")) + " turns";
        }
        case DealItemKind::Peace: return "peace";
        case DealItemKind::City: {
            const City* c = s.city(i.amount);
            return who + " cedes " + (c ? c->name : std::string("a city"));
        }
        case DealItemKind::Favor: return who + " gives " + std::to_string(i.amount) + " Diplomatic Favor";
        case DealItemKind::GreatWork: {
            const City* c = s.city(i.amount);
            if (!c || i.resource < 0 || ti(i.resource) >= c->greatWorks.size()) return who + " gives a Great Work";
            const GreatWork& w = c->greatWorks[ti(i.resource)];
            std::string kind = r.greatWorkTypes[ti(w.type)].id;
            std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const std::string by = w.creator >= 0 && ti(w.creator) < r.greatPeople.size() ? " by " + r.greatPeople[ti(w.creator)].name : "";
            return who + " gives a Great Work (" + kind + by + ")";
        }
        case DealItemKind::JointWar: {
            const bool known = i.amount >= 0 && static_cast<size_t>(i.amount) < s.players.size() && s.players[static_cast<size_t>(i.amount)].civ >= 0 &&
                               ti(s.players[static_cast<size_t>(i.amount)].civ) < r.civs.size();
            return "a joint war on " + (known ? r.civs[ti(s.players[static_cast<size_t>(i.amount)].civ)].name : std::string("?"));
        }
        case DealItemKind::Captive: {
            for (const CapturedSpy& c : s.capturedSpies) {
                if (c.spy.id == i.amount) return who + " returns a captured spy (level " + std::to_string(c.spy.level) + ")";
            }
            return who + " returns a captured spy";
        }
    }
    return "?";
}

std::string describeDeal(const Rules& r, const GameState& s, const Deal& d) {
    std::string out;
    for (const DealItem& i : d.items) out += (out.empty() ? "" : "; ") + describeDealItem(r, s, i);
    return out;
}

const char* relationshipName(Relationship r) {
    switch (r) {
        case Relationship::AtWar: return "At War";
        case Relationship::Denounced: return "Denounced";
        case Relationship::Unfriendly: return "Unfriendly";
        case Relationship::Neutral: return "Neutral";
        case Relationship::Friendly: return "Friendly";
        case Relationship::DeclaredFriend: return "Declared Friend";
    }
    return "?";
}

const char* opinionReasonName(OpinionReasonKind k) {
    switch (k) {
        case OpinionReasonKind::AtWar: return "We are at war";
        case OpinionReasonKind::DeclaredWar: return "Declared war on us";
        case OpinionReasonKind::SurpriseWar: return "Attacked us without warning";
        case OpinionReasonKind::DenouncedUs: return "Denounced us";
        case OpinionReasonKind::WeDenounced: return "We denounced them";
        case OpinionReasonKind::Friends: return "Declared friends";
        case OpinionReasonKind::OpenBorders: return "Opened their borders to us";
        case OpinionReasonKind::SameReligion: return "Shares our faith";
        case OpinionReasonKind::ConvertingUs: return "Their faith is taking our cities";
        case OpinionReasonKind::TradeRoutes: return "Trades with us";
        case OpinionReasonKind::MadePeace: return "Made peace with us";
        case OpinionReasonKind::Gifts: return "Gave us gifts";
        case OpinionReasonKind::Deals: return "Kept a deal with us";
        case OpinionReasonKind::BrokeDeal: return "Broke a deal with us";
        case OpinionReasonKind::CapturedCity: return "Took one of our cities";
        case OpinionReasonKind::Assassin: return "Sent an assassin against us";
        case OpinionReasonKind::PlunderedTrader: return "Plundered our trader";
        case OpinionReasonKind::Warmonger: return "Warmonger";
        case OpinionReasonKind::Agenda: return "Agenda";
        case OpinionReasonKind::SpyCaught: return "Caught spying on us";
        case OpinionReasonKind::Grievances: return "Grievances";
        case OpinionReasonKind::UsedWmd: return "Used nuclear weapons";
        case OpinionReasonKind::Demanded: return "Made demands of us";
    }
    return "?";
}

}  // namespace sov
