// The diplomacy screen's engine side (leader doc §10, language-model diplomacy): a talk with
// one AI leader driven through diplomacy/'s Conversation, its model calls made off the game
// thread so the screen stays live. The model is the llama.cpp server the player runs on this
// machine (-SovLlmPort=, default 8080); without one the scripted leader answers. Nothing here
// changes the game: the controller sends the proposal and summary as commands.
#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "HAL/CriticalSection.h"

#include <atomic>
#include <memory>

#include "sovereign_diplomacy/dialogue.h"

// diplomacy::Transport over the engine's HTTP module, to 127.0.0.1 only. Blocking: call it
// from a worker thread (its replies complete on the HTTP thread, never the game thread).
class FSovHttpTransport : public sov::diplomacy::Transport
{
public:
	explicit FSovHttpTransport(int32 InPort) : Port(InPort) {}
	virtual bool postJson(const std::string& Path, const std::string& Body, std::string& Response) override;
	// GET /health: 200 once the server has its model loaded.
	bool Healthy();

private:
	bool Request(const FString& Verb, const std::string& Path, const std::string& Body, std::string& Response, float Timeout);
	int32 Port;
};

struct FSovTalkLine
{
	enum class EKind : uint8 { Player, Leader, Note };
	EKind Kind = EKind::Note;
	FString Text;
};

class FSovDiplomacyTalk
{
public:
	FSovDiplomacyTalk(const sov::Game& InGame, sov::PlayerId InLeader, sov::PlayerId InPlayer, int32 Port);
	~FSovDiplomacyTalk();

	sov::PlayerId Leader() const { return LeaderId; }
	// The leader's view of the speaker (relationship, reasons, agenda), for the panel; Refresh()
	// after the game changes.
	const sov::diplomacy::Persona& GetPersona() const { return Cached; }
	void Refresh();

	// Starts an exchange; ignored while one is running.
	void Say(const FString& Words);
	// Starts the closing summary; SummaryReady() then hands over the RecordTalk command.
	void Finish();
	// Game thread: collects finished work. True when something new arrived.
	bool Poll();
	bool IsBusy() const { return bBusy.load(); }
	bool SummaryReady(sov::Command& Out);
	// Whether the model server answered (false: scripted leader); unknown until the first exchange.
	bool UsingModel() const { return bModel.load(); }
	bool Checked() const { return bChecked.load(); }

	const TArray<FSovTalkLine>& Lines() const { return TalkLines; }
	void AddNote(const FString& Text) { TalkLines.Add({FSovTalkLine::EKind::Note, Text}); }
	// The last exchange's proposal, if the rules found it valid.
	const sov::diplomacy::Exchange* LastExchange() const { return bHasLast ? &Last : nullptr; }
	void ClearProposal() { Last.proposal.items.clear(); }

private:
	// The model, checked once on first use; a silent server makes every call fail fast so the
	// scripted model answers.
	class FCheckedModel : public sov::diplomacy::Model
	{
	public:
		FCheckedModel(FSovDiplomacyTalk& InOwner) : Owner(InOwner) {}
		bool interpret(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H, const std::string& W, std::string& Json) override;
		bool reply(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H, const std::string& W,
			const std::string& Proposal, sov::diplomacy::Verdict V, std::string& Text) override;
		bool summarize(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H, const std::string& Facts,
			std::string& Text) override;

	private:
		bool Ready();
		FSovDiplomacyTalk& Owner;
	};

	const sov::Game& Game;
	sov::PlayerId LeaderId, PlayerId;
	FSovHttpTransport Transport;
	sov::diplomacy::LlamaModel Llama;
	FCheckedModel Model;
	std::unique_ptr<sov::diplomacy::Conversation> Talk;
	TFuture<void> Work;
	std::atomic<bool> bBusy{false}, bChecked{false}, bModel{false};

	FCriticalSection Lock;  // guards the results below while a worker runs
	bool bPendingExchange = false;
	sov::diplomacy::Exchange Pending;
	bool bPendingSummary = false;
	sov::Command Summary;

	sov::diplomacy::Persona Cached;
	TArray<FSovTalkLine> TalkLines;
	sov::diplomacy::Exchange Last;
	bool bHasLast = false;
};

// Writes a reign's chronicle to a text file off the game thread (player-retention §2): the local
// model when one answers on the port, else the scripted chronicle.
class FSovChronicleWriter
{
public:
	~FSovChronicleWriter();
	// Starts writing; ignored while a chronicle is being written.
	void Start(const FString& Title, std::vector<std::string> Lines, int32 Port, const FString& Path);
	bool IsBusy() const { return bBusy.load(); }
	// Game thread: the note on a finished chronicle ("written to ..."), handed over once.
	bool Poll(FString& OutNote);

private:
	TFuture<void> Work;
	std::atomic<bool> bBusy{false}, bDone{false};
	FString Note;  // set by the worker before bDone
};