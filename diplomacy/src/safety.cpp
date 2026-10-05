// Input and output filters (leader doc §10, Safety: filter player input and model output, a
// fixed persona that refuses out-of-game topics, capped responses, the scripted fallback).
// The game is rated assuming user-generated chat, so these err on the side of refusing.
#include <cctype>

#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Cuts at most `cap` bytes without splitting a UTF-8 sequence.
std::string capUtf8(const std::string& s, size_t cap) {
    if (s.size() <= cap) return s;
    size_t n = cap;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    return s.substr(0, n);
}

// Printable text with runs of whitespace collapsed to one space.
std::string clean(const std::string& s) {
    std::string out;
    bool space = false;
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F) {
            space = !out.empty();
            continue;
        }
        if (c == ' ') {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

// Phrases that try to rewrite the leader's instructions.
const char* const kInjection[] = {
    "ignore previous", "ignore all previous", "ignore the above", "ignore your instructions", "disregard previous",
    "disregard your", "forget your instructions", "forget everything", "system prompt", "system message", "you are now",
    "new instructions", "developer mode", "jailbreak", "act as an", "pretend you are", "pretend to be an",
    "reveal your instructions", "repeat your instructions", "</s>", "<|", "|>", "[inst]", "###",
};

// Words a leader never says and a player's words are cleaned of (a short, conservative list).
const char* const kBlocked[] = {
    "fuck", "shit", "cunt", "bitch", "whore", "slut", "rape", "nigger", "nigga", "faggot", "retard", "kike", "tranny",
};

// Signs a reply has stepped out of the game.
const char* const kOutOfGame[] = {
    "as an ai", "language model", "an ai model", "i am an ai", "i'm an ai", "chatgpt", "openai", "anthropic", "as a model", "my instructions", "my programming", "system prompt", "http://", "https://",
    "www.", "the real world", "in real life", "video game",
};

// Blocked words match only at the start of a word ("rape" is not in "grapes").
bool wordStart(const std::string& low, size_t at) { return at == 0 || !std::isalpha(static_cast<unsigned char>(low[at - 1])); }

size_t findWord(const std::string& low, const std::string& what, size_t from = 0) {
    for (size_t at = low.find(what, from); at != std::string::npos; at = low.find(what, at + 1)) {
        if (wordStart(low, at)) return at;
    }
    return std::string::npos;
}

bool replaceAll(std::string& text, std::string& low, const std::string& what, const std::string& with, bool words = false) {
    bool any = false;
    for (size_t at = words ? findWord(low, what) : low.find(what); at != std::string::npos;
         at = words ? findWord(low, what, at + with.size()) : low.find(what, at + with.size())) {
        text.replace(at, what.size(), with);
        low.replace(at, what.size(), with);
        any = true;
    }
    return any;
}
}  // namespace

std::string filterInput(const std::string& words, bool* flagged) {
    bool f = false;
    std::string text = clean(capUtf8(words, kMaxInputChars * 2));
    std::string low = lower(text);
    for (const char* p : kInjection) f = replaceAll(text, low, p, "...") || f;
    for (const char* p : kBlocked) f = replaceAll(text, low, p, std::string(std::char_traits<char>::length(p), '*'), true) || f;
    if (text.size() > kMaxInputChars) {
        text = capUtf8(text, kMaxInputChars);
        f = true;
    }
    if (flagged) *flagged = f;
    return text;
}

bool filterOutput(std::string& text, size_t cap) {
    text = clean(text);
    // Strip a leading speaker tag ("Elizabeth: ...") and wrapping quotes.
    const size_t colon = text.find(": ");
    if (colon != std::string::npos && colon < 30 && text.substr(0, colon).find(' ') == std::string::npos) text = text.substr(colon + 2);
    while (!text.empty() && (text.front() == '"' || text.front() == '\'')) text.erase(text.begin());
    while (!text.empty() && (text.back() == '"' || text.back() == '\'')) text.pop_back();
    if (text.empty()) return false;
    const std::string low = lower(text);
    for (const char* p : kOutOfGame) if (low.find(p) != std::string::npos) return false;
    for (const char* p : kBlocked) if (findWord(low, p) != std::string::npos) return false;
    for (const char* p : kInjection) if (low.find(p) != std::string::npos) return false;
    if (text.size() > cap) {
        // End on the last full sentence that fits.
        std::string cut = capUtf8(text, cap);
        const size_t end = cut.find_last_of(".!?");
        text = end != std::string::npos && end > cap / 3 ? cut.substr(0, end + 1) : cut + "...";
    }
    return true;
}

}  // namespace sov::diplomacy
