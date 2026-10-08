// The dialogue layer for language-model diplomacy (leader doc §10, layer 4: "the model talks,
// the game decides"). Plain C++17 over the rules core, like battle/: no Unreal, no network
// library. A player's words go through an input filter to a Model, which turns them into a
// proposal (structured output); the rules core validates it and decides the leader's answer
// (Game::wouldAccept); the Model then writes the leader's reply in character, knowing that
// answer; an output filter guards the reply. The model never changes game state: the screen
// submits the proposal (Command::proposeDeal) and a summary (Command::recordTalk) as commands
// on the speaking player's machine.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sovereign/commands.h"
#include "sovereign/game.h"

namespace sov::diplomacy {

// Everything a model is told about the leader it plays, built from the rules and game state.
struct Persona {
    PlayerId leader = kNoPlayer, player = kNoPlayer;  // the AI civ, and the human speaking to it
    std::string leaderName, civName, playerCivName, playerLeaderName;
    std::string leaning, voice, agendaName, agendaText;
    Relationship relationship = Relationship::Neutral;
    int opinion = 0;
    std::vector<std::string> reasons;  // "Gave us gifts (+10)"
    std::vector<std::string> pastTalks;  // "Turn 40: ..."
    // What it remembers of this ruler from earlier games (player-retention §1), and the crowns taken each way.
    std::vector<std::string> earlierGames;
    int crownsTaken = 0, crownsLost = 0;
    std::vector<std::string> leaderOffers, playerOffers;  // what each side could put into a deal now
    std::vector<std::string> resourceNames;  // every tradeable resource (the scripted reader matches these)
    bool atWar = false;
    int turn = 0;
};
SOV_API Persona buildPersona(const Game& game, PlayerId leader, PlayerId player);

// What the player's words amount to.
enum class Intent : uint8_t { Chat = 0, Propose, Denounce, Leave };
struct Interpretation {
    Intent intent = Intent::Chat;
    std::vector<DealItem> items;  // the proposal, when intent is Propose
};

// The rules' answer to a proposal, which the reply must follow.
enum class Verdict : uint8_t { None = 0, Accept, Reject, Invalid };

struct ChatMessage {
    std::string role;  // "user" (the player) or "assistant" (the leader)
    std::string content;
};

// A language model, or the scripted stand-in. Both calls must return quickly; a model that
// fails returns false and the conversation falls back to the scripted model.
class SOV_API Model {
public:
    virtual ~Model() = default;
    // Fills `out` with the model's raw JSON reading of the player's latest words.
    virtual bool interpret(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words,
                           std::string& json) = 0;
    // The leader's reply in character, given the rules' verdict on what was proposed.
    virtual bool reply(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words,
                       const std::string& proposal, Verdict verdict, std::string& text) = 0;
    // A one-line summary of the conversation for the leader's memory; `facts` lists what the rules decided.
    virtual bool summarize(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& facts, std::string& text) = 0;
};

// Keyword intents and template replies by relationship and agenda: the fallback when no model
// server answers, and the test double.
class SOV_API ScriptedModel : public Model {
public:
    bool interpret(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words, std::string& json) override;
    bool reply(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words, const std::string& proposal,
               Verdict verdict, std::string& text) override;
    bool summarize(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& facts, std::string& text) override;
};

// How requests reach a model server. Tests stub it; the game uses SocketTransport (a separate
// library, http.h) or the engine's HTTP client.
class SOV_API Transport {
public:
    virtual ~Transport() = default;
    // POSTs a JSON body; false when nothing answered or the status was not 2xx.
    virtual bool postJson(const std::string& path, const std::string& body, std::string& response) = 0;
};

// A llama.cpp server (llama-server, OpenAI-compatible /v1/chat/completions) the player runs.
class SOV_API LlamaModel : public Model {
public:
    explicit LlamaModel(Transport& transport, int maxTokens = 200) : transport_(transport), maxTokens_(maxTokens) {}
    bool interpret(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words, std::string& json) override;
    bool reply(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& words, const std::string& proposal,
               Verdict verdict, std::string& text) override;
    bool summarize(const Persona& persona, const std::vector<ChatMessage>& history, const std::string& facts, std::string& text) override;

private:
    bool complete(const std::vector<ChatMessage>& messages, bool jsonSchema, std::string& content);
    Transport& transport_;
    int maxTokens_;
};

// ---- prompts (persona.cpp)
SOV_API std::string systemPrompt(const Persona& p);
SOV_API std::string interpretInstructions(const Persona& p);
SOV_API std::string replyInstructions(const Persona& p, const std::string& proposal, Verdict verdict);
SOV_API std::string summaryInstructions(const Persona& p);
SOV_API const char* interpretSchema();  // JSON schema for the structured reading

// ---- parsing (parse.cpp)
// Reads a model's JSON into an interpretation; names of resources and sides ("leader",
// "player") become rules indices and player ids. False when the JSON is unusable.
SOV_API bool parseInterpretation(const std::string& json, const Game& game, const Persona& persona, Interpretation& out);

// ---- safety (safety.cpp)
constexpr size_t kMaxInputChars = 500;
constexpr size_t kMaxReplyChars = 700;
constexpr size_t kMaxSummaryChars = 300;
// Printable, trimmed, capped player text; attempts to rewrite the leader's instructions are
// neutralised. `flagged` is set when anything had to be removed.
SOV_API std::string filterInput(const std::string& words, bool* flagged = nullptr);
// A reply that stays in character and in the game: capped, no links, no talk of being an AI or
// of the world outside the game, nothing from the blocked list. False: use a scripted line.
SOV_API bool filterOutput(std::string& text, size_t cap = kMaxReplyChars);

// ---- JSON helpers (parse.cpp)
SOV_API std::string jsonEscape(const std::string& s);
SOV_API std::string jsonString(const std::string& s);  // quoted and escaped

// One exchange on the diplomacy screen.
struct Exchange {
    std::string words;     // the filtered player text
    std::string reply;     // the leader's words
    Interpretation reading;
    Deal proposal;         // valid deal from the player to the leader (items empty: none)
    Verdict verdict = Verdict::None;
    bool scripted = false;  // the fallback answered
    bool flagged = false;   // the input filter removed something
};

// A conversation between the human and one AI leader. It holds the model and the history,
// and asks the game only const questions; the screen turns results into commands.
class SOV_API Conversation {
public:
    Conversation(const Game& game, PlayerId leader, PlayerId player, Model* model);
    Exchange say(const std::string& words);
    // The summary command for the leader's memory (empty text: nothing worth recording).
    Command summaryCommand();
    // The proposal as a command, ready for the player to confirm.
    static Command proposalCommand(const Exchange& e) { return Command::proposeDeal(e.proposal.from, e.proposal.to, e.proposal.items); }
    const std::vector<ChatMessage>& history() const { return history_; }
    const Persona& persona() const { return persona_; }

private:
    const Game& game_;
    Persona persona_;
    Model* model_;
    ScriptedModel scripted_;
    std::vector<ChatMessage> history_;
    std::vector<std::string> facts_;  // what the rules decided, for the summary
};

}  // namespace sov::diplomacy
