// Diplomacy between major civs (08-diplomacy-city-states-governors.md: relationship states,
// diplomatic actions, agendas; leaders-and-art-style.md: the twelve Sovereign agendas; leader
// doc §10: the model talks, the game decides). Each civ holds an opinion of each other civ:
// live reasons (war, friendship, religion, trade, its leader's agenda) plus memories that fade
// (wars declared on it, denunciations, gifts, deals kept and broken). Deals trade gold now or
// per turn, luxuries and strategic resources per turn, open borders, a declaration of
// friendship and peace; an AI answers by the deal's gold value against a bar its opinion sets.
#include <algorithm>

#include "sovereign/ai.h"
#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(PlayerId p) { return static_cast<size_t>(p); }
size_t ti(TypeIndex i) { return static_cast<size_t>(i); }
constexpr int kItemInts = 4;
constexpr int kMaxItems = 10;
constexpr int kFriendlyOpinion = 12;    // at or above: Friendly (and friendship is on the table)
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

bool Game::grantsOpenBorders(PlayerId owner, PlayerId to) const {
    if (owner < 0 || to < 0 || owner == to) return false;
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
            OpinionReasonKind::PlunderedTrader, OpinionReasonKind::Warmonger,
        };
        add(kinds[static_cast<size_t>(m.kind)], v);
    }
    add(OpinionReasonKind::Agenda, agendaOpinion(holder, about));
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

CommandError Game::dealProblem(const Deal& d) const {
    if (d.from == d.to || !isMajorCiv(d.from) || !isMajorCiv(d.to) || !hasMet(d.from, d.to)) return CommandError::CannotDeal;
    if (d.items.empty() || d.items.size() > static_cast<size_t>(kMaxItems)) return CommandError::CannotDeal;
    const bool war = atWar(d.from, d.to);
    bool peace = false, friendship = false;
    std::vector<int> openBorders(2, 0);
    std::vector<TypeIndex> luxuries;
    int gold[2] = {0, 0}, perTurn[2] = {0, 0};
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
            case DealItemKind::Peace: {
                const int mine = ai::militaryStrength(*this, judge), theirs = ai::militaryStrength(*this, other);
                const int turns = state_.turn - state_.players[at(judge)].relations[at(other)].since;
                int worth = 0;
                if (mine * 100 < theirs * 90) worth = 200;            // losing
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
    tryItem({DealItemKind::Peace, from, 0, kNone});
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
            case DealItemKind::Peace: onPeace(d.from, d.to); break;
        }
    }
    // Both remember a deal kept; a deal that only gives is a gift.
    for (PlayerId judge : {d.from, d.to}) {
        const PlayerId other = judge == d.from ? d.to : d.from;
        const bool gift = std::all_of(d.items.begin(), d.items.end(), [&](const DealItem& i) { return i.from == other; });
        if (gift) remember(judge, other, MemoryKind::Gift, std::min(15, std::max(1, dealValue(judge, d) / 10)), kDealTurns);
        else remember(judge, other, MemoryKind::Deal, 3, kDealTurns);
    }
    pushEvent(EventKind::DealAccepted, d.from, d.to, d.id);
}

void Game::onWarDeclared(PlayerId by, PlayerId target) {
    Player& p = state_.players[at(by)];
    Relation& mine = p.relations[at(target)];
    // A formal war follows DIPLOMACY_DENOUNCE_WAR_DELAY turns of denunciation; anything else is a surprise.
    const bool formal = denouncing(by, target) && state_.turn - mine.denouncedOn >= rules_->globalInt("DIPLOMACY_DENOUNCE_WAR_DELAY");
    if (isMajorCiv(target)) {
        ++p.warsDeclared;
        if (!formal) ++p.surpriseWars;
    }
    remember(target, by, formal ? MemoryKind::DeclaredWar : MemoryKind::SurpriseWar, formal ? -12 : -24, formal ? 60 : 80);
    for (const Player& o : state_.players) {
        if (o.id != by && o.id != target && hasMet(o.id, by)) remember(o.id, by, MemoryKind::Warmonger, formal ? -4 : -6, 40);
    }
    for (PlayerId a : {by, target}) {
        const PlayerId b = a == by ? target : by;
        Relation& r = state_.players[at(a)].relations[at(b)];
        r.friendsUntil = 0;
        r.openBordersUntil = 0;
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

void Game::onPeace(PlayerId a, PlayerId b) {
    for (PlayerId x : {a, b}) {
        Relation& r = state_.players[at(x)].relations[at(x == a ? b : a)];
        r.war = false;
        r.since = state_.turn;
        r.peaceOffered = false;
    }
    remember(a, b, MemoryKind::MadePeace, 4, kDealTurns);
    remember(b, a, MemoryKind::MadePeace, 4, kDealTurns);
    pushEvent(EventKind::PeaceMade, a, b, 0);
}

void Game::processDiplomacy(PlayerId pid) {
    // Proposals this player made last turn and nobody answered lapse.
    state_.deals.erase(std::remove_if(state_.deals.begin(), state_.deals.end(),
                                      [&](const Deal& d) { return d.from == pid && d.turn < state_.turn; }),
                       state_.deals.end());
    Player& p = state_.players[at(pid)];
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
        case DealItemKind::Peace: return "peace";
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
    }
    return "?";
}

}  // namespace sov
