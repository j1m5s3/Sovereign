// Reading a model's structured output into deal items, and the small JSON writing helpers the
// requests need. A reading never reaches the game as-is: the rules core validates the deal.
#include <algorithm>
#include <cctype>
#include <cstdio>

#include "sovereign/json.h"
#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Models sometimes wrap JSON in prose or code fences: take the outermost object.
std::string objectIn(const std::string& text) {
    const size_t a = text.find('{'), b = text.rfind('}');
    if (a == std::string::npos || b == std::string::npos || b < a) return {};
    return text.substr(a, b - a + 1);
}

TypeIndex resourceNamed(const Rules& r, const std::string& name) {
    const std::string want = lower(trim(name));
    if (want.empty()) return kNone;
    for (size_t i = 0; i < r.resources.size(); ++i) {
        const ResourceType& rt = r.resources[i];
        if (rt.cls == ResourceClass::Bonus) continue;
        if (lower(rt.name) == want || lower(rt.id) == want || lower(rt.id) == "resource_" + want) return static_cast<TypeIndex>(i);
    }
    // Plurals and near misses ("silks", "the wine").
    for (size_t i = 0; i < r.resources.size(); ++i) {
        const ResourceType& rt = r.resources[i];
        if (rt.cls != ResourceClass::Bonus && want.find(lower(rt.name)) != std::string::npos) return static_cast<TypeIndex>(i);
    }
    return kNone;
}
}  // namespace

bool parseInterpretation(const std::string& json, const Game& game, const Persona& persona, Interpretation& out) {
    out = Interpretation{};
    std::string err;
    const Json j = Json::parse(objectIn(json), &err);
    if (!err.empty() || !j.isObject()) return false;
    const std::string intent = lower(j["intent"].str());
    if (intent == "propose") out.intent = Intent::Propose;
    else if (intent == "denounce") out.intent = Intent::Denounce;
    else if (intent == "leave") out.intent = Intent::Leave;
    else if (intent == "chat" || intent.empty()) out.intent = Intent::Chat;
    else return false;
    if (out.intent != Intent::Propose) return true;
    const Rules& r = game.rules();
    for (const Json& it : j["items"].items()) {
        if (out.items.size() >= 10) break;
        if (!it.isObject()) continue;
        const std::string kind = lower(it["kind"].str()), from = lower(it["from"].str());
        DealItem d;
        d.from = from == "leader" ? persona.leader : from == "player" ? persona.player : kNoPlayer;
        if (d.from == kNoPlayer) continue;
        const int64_t amount = it["amount"].integer(0);
        d.amount = static_cast<int32_t>(std::clamp<int64_t>(amount, 0, 1000000));
        if (kind == "gold") d.kind = DealItemKind::Gold;
        else if (kind == "gold_per_turn") d.kind = DealItemKind::GoldPerTurn;
        else if (kind == "resource") {
            d.kind = DealItemKind::Resource;
            d.resource = resourceNamed(r, it["resource"].str());
            if (d.resource == kNone) continue;
            // A luxury trades one copy; a strategic resource at least one per turn.
            const bool luxury = r.resources[static_cast<size_t>(d.resource)].cls == ResourceClass::Luxury;
            d.amount = luxury ? 1 : std::max<int32_t>(1, d.amount);
        } else if (kind == "open_borders") d.kind = DealItemKind::OpenBorders;
        else if (kind == "friendship") d.kind = DealItemKind::Friendship;
        else if (kind == "alliance") d.kind = DealItemKind::Alliance;
        else if (kind == "peace") d.kind = DealItemKind::Peace;
        else if (kind == "favor") d.kind = DealItemKind::Favor;
        else if (kind == "ruler") {
            // A captured ruler goes home (leader doc §5): whichever side holds the other's captive gives it.
            d.kind = DealItemKind::Ruler;
            const std::vector<Player>& players = game.state().players;
            if (players[static_cast<size_t>(persona.player)].captor == persona.leader) d.from = persona.leader;
            else if (players[static_cast<size_t>(persona.leader)].captor == persona.player) d.from = persona.player;
            d.amount = d.from == persona.leader ? persona.player : persona.leader;
        }
        else if (kind == "city") {
            // A city of the giver's, by name (ceded with peace; 08).
            d.kind = DealItemKind::City;
            const std::string name = lower(it["city"].str());
            const City* named = nullptr;
            for (const City& c : game.state().cities) {
                if (c.owner == d.from && lower(c.name) == name) named = &c;
            }
            if (!named) continue;
            d.amount = named->id;
        } else continue;
        const bool counted = d.kind == DealItemKind::Gold || d.kind == DealItemKind::GoldPerTurn || d.kind == DealItemKind::Favor;
        if (counted && d.amount <= 0) continue;
        if (!counted && d.kind != DealItemKind::Resource && d.kind != DealItemKind::City && d.kind != DealItemKind::Ruler) d.amount = 0;
        // An alliance's type rides in its amount (08: Alliance); Economic when none is named.
        if (d.kind == DealItemKind::Alliance) {
            static const char* const kTypes[] = {"research", "military", "economic", "cultural", "religious"};
            const std::string type = lower(it["type"].str());
            d.amount = static_cast<int32_t>(AllianceType::Economic);
            for (int t = 0; t < kNumAllianceTypes; ++t) {
                if (type == kTypes[t]) d.amount = t;
            }
        }
        // One friendship, alliance or peace for both sides; keep the first.
        const bool mutual = d.kind == DealItemKind::Friendship || d.kind == DealItemKind::Peace || d.kind == DealItemKind::Alliance;
        if (mutual && std::any_of(out.items.begin(), out.items.end(), [&](const DealItem& x) { return x.kind == d.kind; })) continue;
        out.items.push_back(d);
    }
    if (out.items.empty()) out.intent = Intent::Chat;  // nothing tradeable was named
    return true;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string jsonString(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

}  // namespace sov::diplomacy
