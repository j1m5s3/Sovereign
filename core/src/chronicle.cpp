// The chronicle of a reign and the Hall of Sovereigns (specs/sovereign/player-retention.md §2): the
// game keeps its key events for the whole game (wars and peace, assassinations, rulers captured or
// slain, successions, rebellions, historic moments, new ages) and reads them out as plain lines,
// which the dialogue layer's model (or its scripted stand-in) writes up in a historical voice.
#include <algorithm>
#include <cctype>

#include "sovereign/game.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
constexpr size_t kChronicleCap = 1000;

std::string playerName(const Rules& r, const GameState& s, PlayerId id) {
    if (id < 0 || at(id) >= s.players.size()) return "someone";
    const Player& p = s.players[at(id)];
    if (p.barbarian) return "the barbarians";
    if (p.freeCity) return "the Free Cities";
    if (p.cityState != kNone) return r.cityStates[at(p.cityState)].name;
    return p.civ == kNone ? std::string("someone") : r.civs[at(p.civ)].name;
}

const char* victoryWord(Victory v) {
    switch (v) {
        case Victory::Domination: return "Domination";
        case Victory::Score: return "Score";
        case Victory::LastStanding: return "Last civ standing";
        case Victory::Religious: return "Religious";
        case Victory::Culture: return "Culture";
        case Victory::Diplomatic: return "Diplomatic";
        case Victory::Science: return "Science";
        default: return "";
    }
}
}  // namespace

bool Game::chronicleWorthy(EventKind kind) {
    switch (kind) {
        case EventKind::WarDeclared:
        case EventKind::PeaceMade:
        case EventKind::FriendshipDeclared:
        case EventKind::AssassinKilledLeader:
        case EventKind::AssassinWoundedLeader:
        case EventKind::AssassinCaptured:
        case EventKind::AssassinKilledDouble:
        case EventKind::Rebellion:
        case EventKind::HistoricMoment:
        case EventKind::NewAge:
        case EventKind::LeaderLost:
        case EventKind::Succession: return true;
        default: return false;
    }
}

void Game::recordChronicle(const GameEvent& e) {
    state_.chronicle.push_back(e);
    if (state_.chronicle.size() > kChronicleCap) state_.chronicle.erase(state_.chronicle.begin());
}

std::vector<std::string> Game::chronicleLines(PlayerId viewer) const {
    std::vector<std::string> out;
    if (viewer < 0 || at(viewer) >= state_.players.size()) return out;
    const auto name = [&](PlayerId id) { return playerName(*rules_, state_, id); };

    static const char* const kAges[] = {"a Normal Age", "a Golden Age", "a Dark Age", "a Heroic Age"};
    for (const GameEvent& e : state_.chronicle) {
        if (e.actor != viewer && e.target != viewer) continue;
        const std::string a = name(e.actor), t = name(e.target);
        std::string text;
        switch (e.kind) {
            case EventKind::WarDeclared: text = a + (e.value == 1 ? " declared a surprise war on " : " declared war on ") + t; break;
            case EventKind::PeaceMade: text = a + " made peace with " + t; break;
            case EventKind::FriendshipDeclared: text = a + " and " + t + " declared their friendship"; break;
            case EventKind::AssassinKilledLeader: text = "an assassin from " + a + " killed the ruler of " + t; break;
            case EventKind::AssassinWoundedLeader: text = "an assassin from " + a + " wounded the ruler of " + t; break;
            case EventKind::AssassinCaptured: text = t + " caught an assassin sent by " + a; break;
            case EventKind::AssassinKilledDouble: text = "an assassin from " + a + " killed a stand-in for the ruler of " + t; break;
            case EventKind::Rebellion: text = "rebels rose against " + t; break;
            case EventKind::HistoricMoment:
                if (e.value >= 0 && at(e.value) < rules_->moments.size()) text = a + ": " + rules_->moments[at(e.value)].name;
                break;
            case EventKind::NewAge: text = a + " entered " + kAges[at(e.value) % 4]; break;
            case EventKind::LeaderLost: text = a + (e.value == 1 ? " captured the ruler of " : " slew the ruler of ") + t + " in battle"; break;
            case EventKind::Succession: {
                const Succession kind = static_cast<Succession>(e.value % 16);
                const Dynasty* d = state_.players[at(e.actor)].civ == kNone ? nullptr : rules_->dynastyOf(state_.players[at(e.actor)].civ);
                const int heir = e.value / 16;
                if (kind == Succession::Heir && d && heir >= 0 && at(heir) < d->names.size()) text = d->names[at(heir)] + " took the throne of " + a;
                else if (kind == Succession::Unit) text = "a general took the throne of " + a;
                else if (kind == Succession::Governor) text = "a governor took the throne of " + a;
                else if (kind == Succession::GreatPerson) text = "a great commander took the throne of " + a;
                else text = "a regent took the throne of " + a;
                break;
            }
            default: break;
        }
        if (!text.empty()) {
            text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            out.push_back("Turn " + std::to_string(e.turn) + ": " + text + ".");
        }
    }
    // How it ended for them.
    const Player& v = state_.players[at(viewer)];
    if (state_.winner != kNoPlayer) {
        out.push_back("Turn " + std::to_string(state_.turn) + ": " + name(state_.winner) + " won a " + victoryWord(state_.victory) + " victory.");
    } else if (!v.alive) {
        out.push_back(name(viewer) + " fell.");
    }
    return out;
}

std::string Game::hallEntry(PlayerId player) const {
    if (player < 0 || at(player) >= state_.players.size()) return {};
    const Player& p = state_.players[at(player)];
    const std::string civ = p.civ == kNone ? std::string("?") : rules_->civs[at(p.civ)].name;
    std::string s = (p.leaderName.empty() ? civ : p.leaderName) + " of " + civ + ", turn " + std::to_string(state_.turn) + ": ";
    if (state_.winner == player) s += std::string(victoryWord(state_.victory)) + " victory";
    else if (state_.winner != kNoPlayer) s += "outlasted by " + rules_->civs[at(state_.players[at(state_.winner)].civ)].name;
    else if (!p.alive) s += "fallen";
    else s += "the reign goes on";
    if (const Unit* l = leaderOf(player)) {
        s += ". Ruler level " + std::to_string(l->level());
        std::string gear;
        for (const TypeIndex g : l->gear) {
            if (g != kNone) gear += (gear.empty() ? "" : ", ") + rules_->gear[at(g)].name;
        }
        if (!gear.empty()) s += ", " + gear;
        std::string promotions;
        for (const TypeIndex pr : l->promotions) promotions += (promotions.empty() ? "" : ", ") + rules_->promotions[at(pr)].name;
        if (!promotions.empty()) s += "; " + promotions;
    } else if (p.captor != kNoPlayer) {
        s += ". The ruler is a captive";
    }
    // Rivals made: the AI civs it fought, most wars first.
    std::vector<std::pair<int, std::string>> fought;
    for (const RivalTally& t : state_.rivalTally) {
        if (t.human == player && t.memory.wars > 0) fought.push_back({-t.memory.wars, playerName(*rules_, state_, t.ai)});
    }
    std::sort(fought.begin(), fought.end());
    for (size_t i = 0; i < fought.size() && i < 3; ++i) {
        const int wars = -fought[i].first;
        s += (i == 0 ? ". Rivals made: " : ", ") + fought[i].second + " (" + std::to_string(wars) + (wars == 1 ? " war)" : " wars)");
    }
    return s + ".";
}

std::vector<std::string> Game::achievementsEarned(PlayerId player) const {
    std::vector<std::string> out;
    if (player < 0 || at(player) >= state_.players.size()) return out;
    static const std::pair<const char*, Victory> kVictories[] = {{"DOMINATION", Victory::Domination}, {"SCORE", Victory::Score},
                                                                 {"LAST_STANDING", Victory::LastStanding}, {"RELIGIOUS", Victory::Religious},
                                                                 {"CULTURE", Victory::Culture}, {"DIPLOMATIC", Victory::Diplomatic},
                                                                 {"SCIENCE", Victory::Science}};
    int rulers = 0;
    for (const GameEvent& e : state_.chronicle) rulers += e.kind == EventKind::LeaderLost && e.actor == player ? 1 : 0;
    int wonders = 0;
    for (const City& c : state_.cities) {
        if (c.owner != player) continue;
        for (const TypeIndex b : c.buildings) wonders += rules_->buildings[at(b)].wonder ? 1 : 0;
    }
    const Unit* l = leaderOf(player);
    for (const AchievementType& a : rules_->achievements) {
        bool earned = false;
        switch (a.kind) {
            case AchievementKind::Victory: {
                earned = state_.winner == player && (a.speed.empty() || a.speed == state_.setup.speed);
                if (earned && !a.victory.empty()) {
                    earned = std::any_of(std::begin(kVictories), std::end(kVictories),
                                         [&](const std::pair<const char*, Victory>& v) { return a.victory == v.first && state_.victory == v.second; });
                }
                break;
            }
            case AchievementKind::RulersTaken: earned = rulers >= a.value; break;
            case AchievementKind::LeaderLevel: earned = l && l->level() >= a.value; break;
            case AchievementKind::Wonders: earned = wonders >= a.value; break;
        }
        if (earned) out.push_back(a.id);
    }
    return out;
}

}  // namespace sov
