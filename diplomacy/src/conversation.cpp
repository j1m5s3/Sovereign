// One talk on the diplomacy screen: filter the words, let the model read them, let the rules
// judge the proposal, let the model answer in character, filter the answer. Any model failure
// (no server, unusable JSON, a reply the filter refuses) falls back to the scripted model, so the
// screen always answers.
#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

Conversation::Conversation(const Game& game, PlayerId leader, PlayerId player, Model* model)
    : game_(game), persona_(buildPersona(game, leader, player)), model_(model) {}

Exchange Conversation::say(const std::string& raw) {
    Exchange e;
    e.words = filterInput(raw, &e.flagged);
    // The state may have moved on since the last line (a deal was made): refresh the persona.
    persona_ = buildPersona(game_, persona_.leader, persona_.player);
    if (e.words.empty()) {
        e.reply = "...";
        return e;
    }
    std::string json;
    bool fromModel = model_ && model_->interpret(persona_, history_, e.words, json) && parseInterpretation(json, game_, persona_, e.reading);
    if (!fromModel) {
        e.scripted = true;
        scripted_.interpret(persona_, history_, e.words, json);
        parseInterpretation(json, game_, persona_, e.reading);
    }
    // The rules decide; the model only learns the verdict.
    std::string proposal;
    if (e.reading.intent == Intent::Propose) {
        const Deal d{0, persona_.player, persona_.leader, game_.state().turn, e.reading.items};
        proposal = describeDeal(game_.rules(), game_.state(), d);
        if (game_.dealProblem(d) != CommandError::Ok) {
            e.verdict = Verdict::Invalid;
        } else {
            e.proposal = d;
            e.verdict = game_.wouldAccept(persona_.leader, d) ? Verdict::Accept : Verdict::Reject;
        }
        facts_.push_back(persona_.playerCivName + " proposed (" + proposal + "); " + persona_.leaderName +
                         (e.verdict == Verdict::Accept ? " was willing" : e.verdict == Verdict::Reject ? " refused" : " said it could not be done"));
    } else if (e.reading.intent == Intent::Denounce) {
        facts_.push_back(persona_.playerCivName + " threatened to denounce " + persona_.civName);
    }
    const bool replied = model_ && model_->reply(persona_, history_, e.words, proposal, e.verdict, e.reply) && filterOutput(e.reply);
    if (!replied) {
        e.scripted = true;
        scripted_.reply(persona_, history_, e.words, proposal, e.verdict, e.reply);
        filterOutput(e.reply);
    }
    history_.push_back({"user", e.words});
    history_.push_back({"assistant", e.reply});
    return e;
}

Command Conversation::summaryCommand() {
    std::string facts;
    for (const std::string& f : facts_) facts += (facts.empty() ? "" : "; ") + f;
    std::string text;
    const bool fromModel = model_ && model_->summarize(persona_, history_, facts, text) && filterOutput(text, kMaxSummaryChars);
    if (!fromModel && (!scripted_.summarize(persona_, history_, facts, text) || !filterOutput(text, kMaxSummaryChars))) text.clear();
    if (text.size() > kMaxTalkText) text.resize(kMaxTalkText);
    return Command::recordTalk(persona_.player, persona_.leader, text);
}

}  // namespace sov::diplomacy
