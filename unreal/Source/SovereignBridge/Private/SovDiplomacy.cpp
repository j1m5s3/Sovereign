#include "SovDiplomacy.h"

#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace
{
FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

std::string Utf8(const FString& S)
{
	const FTCHARToUTF8 Conv(*S);
	return std::string(Conv.Get(), static_cast<size_t>(Conv.Length()));
}
}  // namespace

// ---------------------------------------------------------------- HTTP

bool FSovHttpTransport::Request(const FString& Verb, const std::string& Path, const std::string& Body, std::string& Response, float Timeout)
{
	// The result is shared with the completion callback, which may outlive a timed-out wait.
	struct FResult
	{
		FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);
		bool bOk = false;
		std::string Content;
		~FResult() { FPlatformProcess::ReturnSynchEventToPool(Done); }
	};
	const TSharedRef<FResult, ESPMode::ThreadSafe> Result = MakeShared<FResult, ESPMode::ThreadSafe>();
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d%s"), Port, *Str(Path)));
	Req->SetVerb(Verb);
	Req->SetTimeout(Timeout);
	Req->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);
	if (Verb == TEXT("POST"))
	{
		Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Req->SetContent(TArray<uint8>(reinterpret_cast<const uint8*>(Body.data()), static_cast<int32>(Body.size())));
	}
	Req->OnProcessRequestComplete().BindLambda([Result](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected) {
		if (bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
		{
			const TArray<uint8>& Bytes = Resp->GetContent();
			Result->Content.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
			Result->bOk = true;
		}
		Result->Done->Trigger();
	});
	if (!Req->ProcessRequest())
	{
		return false;
	}
	if (!Result->Done->Wait(FTimespan::FromSeconds(Timeout + 1.0)))
	{
		Req->CancelRequest();
		return false;
	}
	Response = Result->Content;
	return Result->bOk;
}

bool FSovHttpTransport::postJson(const std::string& Path, const std::string& Body, std::string& Response)
{
	return Request(TEXT("POST"), Path, Body, Response, 60.f);
}

bool FSovHttpTransport::Healthy()
{
	std::string Ignored;
	return Request(TEXT("GET"), "/health", "", Ignored, 3.f);
}

// ---------------------------------------------------------------- the talk

bool FSovDiplomacyTalk::FCheckedModel::Ready()
{
	if (!Owner.bChecked.load())
	{
		Owner.bModel = Owner.Transport.Healthy();
		Owner.bChecked = true;
	}
	return Owner.bModel.load();
}

bool FSovDiplomacyTalk::FCheckedModel::interpret(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H,
	const std::string& W, std::string& Json)
{
	return Ready() && Owner.Llama.interpret(P, H, W, Json);
}

bool FSovDiplomacyTalk::FCheckedModel::reply(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H,
	const std::string& W, const std::string& Proposal, sov::diplomacy::Verdict V, std::string& Text)
{
	return Ready() && Owner.Llama.reply(P, H, W, Proposal, V, Text);
}

bool FSovDiplomacyTalk::FCheckedModel::summarize(const sov::diplomacy::Persona& P, const std::vector<sov::diplomacy::ChatMessage>& H,
	const std::string& Facts, std::string& Text)
{
	return Ready() && Owner.Llama.summarize(P, H, Facts, Text);
}

FSovDiplomacyTalk::FSovDiplomacyTalk(const sov::Game& InGame, sov::PlayerId InLeader, sov::PlayerId InPlayer, int32 Port)
	: Game(InGame), LeaderId(InLeader), PlayerId(InPlayer), Transport(Port), Llama(Transport), Model(*this),
	  Talk(std::make_unique<sov::diplomacy::Conversation>(InGame, InLeader, InPlayer, &Model))
{
	Refresh();
	const sov::diplomacy::Persona& P = GetPersona();
	TalkLines.Add({FSovTalkLine::EKind::Note, FString::Printf(TEXT("You stand before %s of %s."), *Str(P.leaderName), *Str(P.civName))});
}

FSovDiplomacyTalk::~FSovDiplomacyTalk()
{
	if (Work.IsValid())
	{
		Work.Wait();
	}
}

void FSovDiplomacyTalk::Refresh() { Cached = sov::diplomacy::buildPersona(Game, LeaderId, PlayerId); }

void FSovDiplomacyTalk::Say(const FString& Words)
{
	if (bBusy.load() || Words.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	bBusy = true;
	const std::string Text = Utf8(Words);
	Work = Async(EAsyncExecution::Thread, [this, Text]() {
		sov::diplomacy::Exchange E = Talk->say(Text);
		FScopeLock Guard(&Lock);
		Pending = MoveTemp(E);
		bPendingExchange = true;
		bBusy = false;
	});
}

void FSovDiplomacyTalk::Finish()
{
	if (bBusy.load())
	{
		return;
	}
	bBusy = true;
	Work = Async(EAsyncExecution::Thread, [this]() {
		sov::Command C = Talk->summaryCommand();
		FScopeLock Guard(&Lock);
		Summary = C;
		bPendingSummary = true;
		bBusy = false;
	});
}

bool FSovDiplomacyTalk::Poll()
{
	FScopeLock Guard(&Lock);
	if (!bPendingExchange)
	{
		return false;
	}
	bPendingExchange = false;
	Last = Pending;
	bHasLast = true;
	Refresh();
	TalkLines.Add({FSovTalkLine::EKind::Player, Str(Last.words)});
	TalkLines.Add({FSovTalkLine::EKind::Leader, Str(Last.reply)});
	if (Last.flagged)
	{
		TalkLines.Add({FSovTalkLine::EKind::Note, TEXT("(Some of your words were not heard.)")});
	}
	return true;
}

bool FSovDiplomacyTalk::SummaryReady(sov::Command& Out)
{
	FScopeLock Guard(&Lock);
	if (!bPendingSummary)
	{
		return false;
	}
	bPendingSummary = false;
	Out = Summary;
	return true;
}
