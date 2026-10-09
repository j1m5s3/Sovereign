// The scripted model: keyword intents and template replies by relationship, leaning and
// agenda. It is the fallback on weak hardware or when no model server answers (leader doc §10,
// Fallback), and the deterministic stand-in the tests use.
#include <cctype>
#include <cstdint>

#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool has(const std::string& text, const char* what) { return text.find(what) != std::string::npos; }

template <size_t N>
bool hasAny(const std::string& text, const char* const (&words)[N]) {
    for (const char* w : words) if (has(text, w)) return true;
    return false;
}

// A stable pick among lines, so the same words get the same answer.
size_t pick(const std::string& seed, size_t n) {
    uint32_t h = 2166136261u;
    for (char c : seed) h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return n == 0 ? 0 : h % n;
}

// The first whole number in the text, or 0.
int firstNumber(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) continue;
        long v = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) && v < 1000000) v = v * 10 + (s[i++] - '0');
        return static_cast<int>(v);
    }
    return 0;
}

// The agenda text is written about the leader ("Dislikes civs with more wonders than he has");
// the leader says it of itself ("I dislike civs with more wonders than I have").
std::string firstPerson(const std::string& text) {
    static const char* const swaps[][2] = {
        {"Likes ", "I like "}, {"likes ", "like "}, {"Dislikes ", "I dislike "}, {"dislikes ", "dislike "},
        {"Respects ", "I respect "}, {"respects ", "respect "}, {"Hates ", "I hate "}, {"hates ", "hate "},
        {"scorns ", "scorn "}, {" he has", " I have"}, {" she has", " I have"}, {" him", " me"}, {" his ", " my "},
        {" her ", " my "}, {" hers", " mine"}, {" he ", " I "}, {" she ", " I "},
    };
    std::string out = " " + text;
    for (const auto& s : swaps) {
        const std::string from = s[0], to = s[1];
        const std::string a = from.front() == ' ' ? from : " " + from, b = from.front() == ' ' ? to : " " + to;
        for (size_t at = out.find(a); at != std::string::npos; at = out.find(a, at + b.size())) out.replace(at, a.size(), b);
    }
    // After a semicolon the clause starts again: "...; dislike civs" becomes "...; I dislike civs".
    for (size_t at = out.find("; "); at != std::string::npos; at = out.find("; ", at + 2)) {
        static const char* const verbs[] = {"like ", "dislike ", "respect ", "hate ", "scorn "};
        for (const char* v : verbs) {
            if (out.compare(at + 2, std::char_traits<char>::length(v), v) == 0) {
                out.insert(at + 2, "I ");
                break;
            }
        }
    }
    return out.substr(1);
}

const char* const kAsking[] = {"give me", "send me", "i want", "i need", "i'd like", "i would like", "can i have", "may i have",
                               "your ", "you give", "pay me", "grant me", "let me"};

// One side's part of an offer ("50 gold", "your wine", "open borders").
void readPart(const std::string& part, const char* from, const Persona& p, std::string& items) {
    auto add = [&](const std::string& item) { items += (items.empty() ? "" : ",") + item; };
    const std::string who = std::string("\"from\":\"") + from + "\"";
    const int n = firstNumber(part);
    if (has(part, "gold") && n > 0) {
        const bool perTurn = has(part, "per turn") || has(part, "a turn") || has(part, "each turn") || has(part, "every turn");
        add(std::string("{\"kind\":\"") + (perTurn ? "gold_per_turn" : "gold") + "\"," + who + ",\"amount\":" + std::to_string(n) + "}");
    }
    for (const std::string& name : p.resourceNames) {
        if (has(part, lower(name).c_str())) add("{\"kind\":\"resource\"," + who + ",\"resource\":" + jsonString(name) + ",\"amount\":" + std::to_string(n > 0 ? n : 1) + "}");
    }
    if (has(part, "border") || has(part, "passage")) add("{\"kind\":\"open_borders\"," + who + "}");
    if (has(part, "ruler") || has(part, "captive")) add("{\"kind\":\"ruler\"," + who + "}");  // its giver is settled on reading
}
}  // namespace

bool ScriptedModel::interpret(const Persona& p, const std::vector<ChatMessage>&, const std::string& words, std::string& json) {
    const std::string w = lower(words);
    static const char* const kLeave[] = {"goodbye", "farewell", "good bye", "bye", "i must go", "leave you"};
    if (has(w, "denounce")) {
        json = R"({"intent":"denounce","items":[]})";
        return true;
    }
    if (hasAny(w, kLeave)) {
        json = R"({"intent":"leave","items":[]})";
        return true;
    }
    std::string items;
    auto add = [&](const std::string& item) { items += (items.empty() ? "" : ",") + item; };
    if (has(w, "peace")) add(R"({"kind":"peace","from":"player"})");
    if (has(w, "friend")) add(R"({"kind":"friendship","from":"player"})");
    if (has(w, "allian")) {
        // The alliance's type from the words around it (08: Alliance).
        const char* type = has(w, "research") || has(w, "science") ? "research"
                           : has(w, "military") || has(w, "war")   ? "military"
                           : has(w, "cultur")                      ? "cultural"
                           : has(w, "relig") || has(w, "faith")    ? "religious"
                                                                    : "economic";
        add(std::string(R"({"kind":"alliance","from":"player","type":")") + type + R"("})");
    }
    // "X for Y": the speaker gives X for Y, unless X is what they ask for ("give me X for Y").
    const size_t forAt = w.find(" for ");
    std::string tradeItems;
    if (forAt != std::string::npos) {
        const std::string left = w.substr(0, forAt), right = w.substr(forAt + 5);
        const bool asks = hasAny(left, kAsking);
        readPart(left, asks ? "leader" : "player", p, tradeItems);
        readPart(right, asks ? "player" : "leader", p, tradeItems);
    } else if (has(w, "border") && !has(w, "my border") && !has(w, "your border")) {
        // "Shall we open our borders?" means both sides.
        tradeItems = R"({"kind":"open_borders","from":"player"},{"kind":"open_borders","from":"leader"})";
        std::string rest;
        readPart(w, hasAny(w, kAsking) ? "leader" : "player", p, rest);
        for (size_t at = rest.find(R"({"kind":"open_borders")"); at != std::string::npos; at = rest.find(R"({"kind":"open_borders")")) {
            const size_t end = rest.find('}', at);
            rest.erase(at, end - at + 1 + (end + 1 < rest.size() && rest[end + 1] == ',' ? 1 : 0));
        }
        while (!rest.empty() && rest.back() == ',') rest.pop_back();
        if (!rest.empty()) tradeItems += "," + rest;
    } else {
        readPart(w, hasAny(w, kAsking) ? "leader" : "player", p, tradeItems);
    }
    if (!tradeItems.empty()) add(tradeItems);
    json = std::string("{\"intent\":\"") + (items.empty() ? "chat" : "propose") + "\",\"items\":[" + items + "]}";
    return true;
}

bool ScriptedModel::reply(const Persona& p, const std::vector<ChatMessage>& history, const std::string& words,
                          const std::string& proposal, Verdict verdict, std::string& text) {
    const std::string w = lower(words);
    const std::string seed = words + std::to_string(history.size());
    const bool warlord = p.leaning == "Warlord", builder = p.leaning == "Builder-King";
    const bool hostile = p.relationship == Relationship::AtWar || p.relationship == Relationship::Denounced ||
                         p.relationship == Relationship::Unfriendly;
    const bool warm = p.relationship == Relationship::Friendly || p.relationship == Relationship::DeclaredFriend;
    switch (verdict) {
        case Verdict::Accept: {
            static const char* const lines[] = {
                "That is acceptable. Put it before me formally and it is done.",
                "A fair offer. Set it down and I will seal it.",
                "Agreed, if you mean it. Make the offer and we have a bargain.",
            };
            text = std::string(warlord ? "Hm. " : builder ? "Very well. " : "") + lines[pick(seed, 3)];
            if (warm) text += " It is good to deal with a friend.";
            return true;
        }
        case Verdict::Reject: {
            if (proposal.find("friendship") != std::string::npos) {
                text = hostile ? "Friends? You have given me no reason even to trust you."
                               : "Friendship is earned, not asked for. Show me your goodwill first.";
                return true;
            }
            if (hostile) {
                static const char* const lines[] = {"You insult me with such terms.", "No. I have no reason to favour you.",
                                                    "Do you take me for a fool? Never."};
                text = lines[pick(seed, 3)];
            } else if (warm) {
                static const char* const lines[] = {"I value our friendship, but I cannot accept that.",
                                                    "Not on those terms, friend. Sweeten it and ask again."};
                text = lines[pick(seed, 2)];
            } else {
                static const char* const lines[] = {"That is not worth my while. Offer more.", "I must decline. Those terms favour you alone.",
                                                    "No. Come back with something better."};
                text = lines[pick(seed, 3)];
            }
            return true;
        }
        case Verdict::Invalid:
            text = proposal.empty() ? "I do not understand what you ask of me." : "That cannot be done, not now.";
            return true;
        case Verdict::None: break;
    }
    if (has(w, "denounce")) {
        text = warlord ? "Then words are finished between us." : "So be it. The world will remember who spoke first.";
        return true;
    }
    static const char* const kLeave[] = {"goodbye", "farewell", "good bye", "bye", "i must go"};
    if (hasAny(w, kLeave)) {
        text = hostile ? "Go, then." : warm ? "Farewell, my friend." : "Farewell.";
        return true;
    }
    static const char* const kWants[] = {"what do you want", "what would you", "what do you like", "agenda", "what pleases", "what do you desire"};
    if (hasAny(w, kWants) && !p.agendaText.empty()) {
        text = "You wish to know my mind? " + firstPerson(p.agendaText);
        return true;
    }
    static const char* const kOutside[] = {"computer", "internet", "president", "phone", "ai ", "robot", "real world"};
    if (hasAny(w, kOutside)) {
        text = "I do not follow such riddles. Speak of our realms, or not at all.";
        return true;
    }
    std::string greet;
    if (p.atWar) greet = warlord ? "You come to talk while our armies bleed? Speak quickly." : "We are at war. What do you want?";
    else if (hostile) greet = "I have little patience for you. Say what you came to say.";
    else if (warm) greet = "Welcome, friend. What can " + p.civName + " do for you?";
    else greet = "Greetings from " + p.civName + ". What brings you here?";
    if (!p.pastTalks.empty() && history.empty()) greet += " I remember our last conversation.";
    // Trophies and grudges from earlier games (player-retention §1).
    if (history.empty() && p.crownsTaken > 0) greet += " I have held your crown before. Do not make me take it again.";
    else if (history.empty() && p.crownsLost > 0) greet += " You took my crown once. I have not forgotten.";
    text = greet;
    return true;
}

bool ScriptedModel::summarize(const Persona& p, const std::vector<ChatMessage>& history, const std::string& facts, std::string& text) {
    if (history.empty()) return false;
    text = facts.empty() ? p.playerCivName + " spoke with " + p.leaderName + "; nothing was agreed." : facts;
    return true;
}

}  // namespace sov::diplomacy
