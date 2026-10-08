// Player modelling (leader doc §10, AI layer 2): a profile of how each major civ plays, built from
// public facts every world turn and kept in the game state (saved, replayed, identical on every
// machine). Shares and indexes are x1000 and fade toward the latest sample over kProfileFade turns,
// so recent play weighs most; counts only grow. The AI reads it to counter what it sees.
#include <algorithm>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }
constexpr int kProfileFade = 20;  // a sample moves the value 1/20 of the way

void fade(int32_t& value, int64_t sample) {
    value = static_cast<int32_t>((static_cast<int64_t>(value) * (kProfileFade - 1) + sample) / kProfileFade);
}
}  // namespace

ProfileClass profileClassOf(const std::string& promotionClass) {
    if (promotionClass == "PROMOTION_CLASS_MELEE") return ProfileClass::Melee;
    if (promotionClass == "PROMOTION_CLASS_RANGED") return ProfileClass::Ranged;
    if (promotionClass == "PROMOTION_CLASS_ANTI_CAVALRY") return ProfileClass::AntiCavalry;
    if (promotionClass == "PROMOTION_CLASS_LIGHT_CAVALRY") return ProfileClass::LightCavalry;
    if (promotionClass == "PROMOTION_CLASS_HEAVY_CAVALRY") return ProfileClass::HeavyCavalry;
    if (promotionClass == "PROMOTION_CLASS_SIEGE") return ProfileClass::Siege;
    if (promotionClass.rfind("PROMOTION_CLASS_NAVAL", 0) == 0) return ProfileClass::Naval;
    return ProfileClass::Other;
}

const PlayerProfile* Game::profile(PlayerId player) const {
    return player >= 0 && at(player) < state_.profiles.size() ? &state_.profiles[at(player)] : nullptr;
}

void Game::processProfiles() {
    const size_t n = state_.players.size();
    if (state_.profiles.size() < n) state_.profiles.resize(n);
    // World averages for the relative measures.
    int majors = 0;
    int64_t citiesAll = 0, strengthAll = 0;
    std::vector<int64_t> strength(n, 0), cities(n, 0);
    std::vector<std::array<int64_t, kNumProfileClasses>> byClass(n);
    for (const Unit& u : state_.units) {
        const UnitType& t = rules_->units[at(u.type)];
        if (u.owner < 0 || at(u.owner) >= n || t.layer != UnitLayer::Military || std::max(t.combat, t.ranged) <= 0) continue;
        const int64_t s = static_cast<int64_t>(std::max(t.combat, t.ranged)) * u.hp / 100;
        strength[at(u.owner)] += s;
        byClass[at(u.owner)][static_cast<size_t>(profileClassOf(t.promotionClass))] += s;
    }
    for (const City& c : state_.cities) cities[at(c.owner)] += 1;
    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        ++majors;
        citiesAll += cities[at(p.id)];
        strengthAll += strength[at(p.id)];
    }
    if (majors == 0) return;

    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        PlayerProfile& pr = state_.profiles[at(p.id)];
        const size_t me = at(p.id);
        ++pr.turnsObserved;
        // Army composition by class, as a share of its strength.
        for (size_t k = 0; k < kNumProfileClasses; ++k) fade(pr.army[k], strength[me] > 0 ? byClass[me][k] * 1000 / strength[me] : 0);
        // Military weight: its strength per city against the world's (1000 = average).
        const int64_t mine = cities[me] > 0 ? strength[me] * 1000 / cities[me] : strength[me] * 1000;
        const int64_t world = citiesAll > 0 ? strengthAll * 1000 / citiesAll : 1;
        fade(pr.militarism, world > 0 ? std::min<int64_t>(5000, mine * 1000 / world) : 1000);
        // Expansion: its cities against the average major (1000 = average).
        fade(pr.expansion, citiesAll > 0 ? std::min<int64_t>(5000, cities[me] * majors * 1000 / citiesAll) : 1000);
        // What it puts its economy into: shares of science, culture and faith in those three.
        {
            const Output output = outputPerTurn(p.id);
            const Fixed sci = output.science, cul = output.culture, faith = output.faith;
            const int64_t total = (sci + cul + faith).toInt();
            fade(pr.science, total > 0 ? sci.toInt() * 1000 / total : 0);
            fade(pr.culture, total > 0 ? cul.toInt() * 1000 / total : 0);
            fade(pr.faith, total > 0 ? faith.toInt() * 1000 / total : 0);
        }
        // Aggression: wars it declared last turn (a spike) and the share of its army camped near
        // other majors' cities while at peace with them.
        int64_t declared = 0;
        for (const GameEvent& e : state_.events) {
            if (e.turn != state_.turn - 1 || e.kind != EventKind::WarDeclared || e.actor != p.id) continue;
            ++declared;
            ++pr.warsDeclared;
            if (e.value == 1) ++pr.surpriseWars;
        }
        int64_t near = 0, army = 0;
        for (const Unit& u : state_.units) {
            const UnitType& t = rules_->units[at(u.type)];
            if (u.owner != p.id || t.layer != UnitLayer::Military || std::max(t.combat, t.ranged) <= 0) continue;
            ++army;
            for (const City& c : state_.cities) {
                if (c.owner == p.id || !isMajor(state_.players[at(c.owner)]) || atWar(p.id, c.owner)) continue;
                if (state_.grid.distance(u.pos, c.pos) <= 4) {
                    ++near;
                    break;
                }
            }
        }
        fade(pr.aggression, std::min<int64_t>(5000, declared * 5000 + (army > 0 ? near * 1000 / army : 0)));
        // Cities it holds that another major founded.
        int taken = 0;
        for (const City& c : state_.cities) {
            if (c.owner == p.id && c.originalOwner != kNoPlayer && c.originalOwner != p.id && at(c.originalOwner) < n &&
                state_.players[at(c.originalOwner)].cityState == kNone && !state_.players[at(c.originalOwner)].barbarian)
                ++taken;
        }
        pr.citiesHeld = taken;
        // The leader: outside its cities, unescorted, with enemies near (leader doc §6-§7).
        if (const Unit* l = leaderOf(p.id)) {
            const City* in = state_.cityAt(l->pos);
            const bool outside = !in || in->owner != p.id;
            fade(pr.leaderOutside, outside ? 1000 : 0);
            fade(pr.leaderExposed, leaderExposed(*l) ? 1000 : 0);
        }
    }
}

// ---- rivals who remember you (player-retention §1) -------------------------------------------
// Each AI civ's memory of a human from earlier games enters through the setup; this game's part is
// tallied in the state each world turn, and the two are merged when the human's file is written.

const RivalMemory* Game::rivalMemory(PlayerId ai, PlayerId human) const {
    if (!state_.setup.rivalMemory || ai < 0 || human < 0 || at(ai) >= state_.players.size() || at(human) >= state_.setup.players.size()) return nullptr;
    const Player& a = state_.players[at(ai)];
    if (a.human || a.civ == kNone || !state_.players[at(human)].human) return nullptr;
    const std::string& civ = rules_->civs[at(a.civ)].id;
    for (const RivalMemory& m : state_.setup.players[at(human)].rivals) {
        if (m.civ == civ) return &m;
    }
    return nullptr;
}

int Game::rivalGrudge(PlayerId ai, PlayerId human) const {
    const RivalMemory* m = rivalMemory(ai, human);
    if (!m) return 0;
    // Its rulers taken and its cities lost weigh most, then betrayals, then plain wars.
    return std::min(30, 6 * m->leadersLost + 3 * m->citiesLost + 5 * m->betrayals + 2 * m->wars);
}

int Game::rivalRespect(PlayerId ai, PlayerId human) const {
    const RivalMemory* m = rivalMemory(ai, human);
    return m ? std::min(15, m->friendTurns / 10) : 0;
}

void Game::processRivals() {
    if (!state_.setup.rivalMemory) return;
    for (const Player& h : state_.players) {
        if (!h.human || h.barbarian) continue;
        for (const Player& a : state_.players) {
            if (a.human || !isMajor(a) || a.civ == kNone || !hasMet(h.id, a.id)) continue;
            auto it = std::find_if(state_.rivalTally.begin(), state_.rivalTally.end(),
                                   [&](const RivalTally& t) { return t.human == h.id && t.ai == a.id; });
            if (it == state_.rivalTally.end()) {
                RivalTally t;
                t.human = h.id;
                t.ai = a.id;
                t.memory.civ = rules_->civs[at(a.civ)].id;
                t.memory.games = 1;
                state_.rivalTally.push_back(t);
                it = state_.rivalTally.end() - 1;
            }
            RivalMemory& m = it->memory;
            for (const GameEvent& e : state_.events) {
                if (e.turn != state_.turn - 1) continue;
                const bool between = (e.actor == h.id && e.target == a.id) || (e.actor == a.id && e.target == h.id);
                if (e.kind == EventKind::WarDeclared && between) {
                    ++m.wars;
                    if (e.actor == h.id && e.value == 1) ++m.betrayals;
                }
                if (e.kind == EventKind::LeaderLost && between) ++(e.actor == a.id ? m.leadersTaken : m.leadersLost);
            }
            int taken = 0;
            for (const City& c : state_.cities) taken += c.owner == h.id && c.originalOwner == a.id ? 1 : 0;
            m.citiesLost = std::max(m.citiesLost, taken);
            if (friends(h.id, a.id)) ++m.friendTurns;
        }
    }
}

std::vector<RivalMemory> Game::rivalMemories(PlayerId human) const {
    std::vector<RivalMemory> out;
    if (human < 0 || at(human) >= state_.setup.players.size()) return out;
    out = state_.setup.players[at(human)].rivals;
    if (!state_.setup.rivalMemory) return out;
    for (const RivalTally& t : state_.rivalTally) {
        if (t.human != human) continue;
        auto it = std::find_if(out.begin(), out.end(), [&](const RivalMemory& m) { return m.civ == t.memory.civ; });
        if (it == out.end()) {
            out.push_back(t.memory);
            continue;
        }
        it->games += t.memory.games;
        it->wars += t.memory.wars;
        it->betrayals += t.memory.betrayals;
        it->leadersTaken += t.memory.leadersTaken;
        it->leadersLost += t.memory.leadersLost;
        it->citiesLost += t.memory.citiesLost;
        it->friendTurns += t.memory.friendTurns;
    }
    return out;
}

}  // namespace sov
