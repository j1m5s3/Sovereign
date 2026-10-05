// A llama.cpp server as the model (llama-server, OpenAI-compatible /v1/chat/completions; the
// player starts it with `llama-server -m <model.gguf>`). The reading asks for JSON that matches
// interpretSchema() through response_format; replies are short plain text.
#include "sovereign/json.h"
#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

namespace {
constexpr size_t kHistoryKept = 8;  // messages of the talk sent back to the model

std::vector<ChatMessage> withHistory(const std::string& system, const std::vector<ChatMessage>& history, const std::string& last) {
    std::vector<ChatMessage> m;
    m.push_back({"system", system});
    const size_t from = history.size() > kHistoryKept ? history.size() - kHistoryKept : 0;
    for (size_t i = from; i < history.size(); ++i) m.push_back(history[i]);
    m.push_back({"user", last});
    return m;
}
}  // namespace

bool LlamaModel::complete(const std::vector<ChatMessage>& messages, bool jsonSchema, std::string& content) {
    std::string body = "{\"model\":\"sovereign-leader\",\"stream\":false,\"max_tokens\":" + std::to_string(maxTokens_) +
                       ",\"temperature\":" + (jsonSchema ? "0.1" : "0.7") + ",\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        body += (i ? "," : "") + std::string("{\"role\":") + jsonString(messages[i].role) + ",\"content\":" + jsonString(messages[i].content) + "}";
    }
    body += "]";
    if (jsonSchema) body += std::string(",\"response_format\":{\"type\":\"json_schema\",\"json_schema\":{\"name\":\"reading\",\"schema\":") + interpretSchema() + "}}";
    body += "}";
    std::string response;
    if (!transport_.postJson("/v1/chat/completions", body, response)) return false;
    std::string err;
    const Json j = Json::parse(response, &err);
    if (!err.empty() || !j.isObject() || !j["choices"].isArray() || j["choices"].items().empty()) return false;
    content = j["choices"].items()[0]["message"]["content"].str();
    return !content.empty();
}

bool LlamaModel::interpret(const Persona& persona, const std::vector<ChatMessage>&, const std::string& words, std::string& json) {
    // The reading sees the persona and the latest words only: earlier lines were read already.
    return complete(withHistory(systemPrompt(persona) + "\n\n" + interpretInstructions(persona), {}, words), true, json);
}

bool LlamaModel::reply(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words,
                       const std::string& proposal, Verdict verdict, std::string& text) {
    return complete(withHistory(systemPrompt(persona) + "\n\n" + replyInstructions(persona, proposal, verdict), history, words), false, text);
}

bool LlamaModel::summarize(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& facts, std::string& text) {
    std::string talk;
    for (const ChatMessage& m : history) talk += (m.role == "user" ? persona.playerCivName : persona.leaderName) + ": " + m.content + "\n";
    if (!facts.empty()) talk += "What was decided: " + facts + "\n";
    return complete({{"system", systemPrompt(persona) + "\n\n" + summaryInstructions(persona)}, {"user", talk}}, false, text);
}

}  // namespace sov::diplomacy
