#include "SovBoard.h"

#include "Async/Async.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#include "sovereign_board/board.h"

namespace
{
const TCHAR* const kSection = TEXT("Sovereign");

// board/'s Link over an engine socket: each message framed by its 4-byte little-endian length, as tcp.cpp does.
class FSocketLink : public sov::board::Link
{
public:
	explicit FSocketLink(FSocket* InSocket) : Socket(InSocket) { Socket->SetNonBlocking(true); Socket->SetNoDelay(true); }
	~FSocketLink() override { close(); }

	bool send(const std::vector<uint8_t>& M) override
	{
		if (!Socket) return false;
		const uint32 N = static_cast<uint32>(M.size());
		for (int32 i = 0; i < 4; ++i) Out.Add(static_cast<uint8>(N >> (8 * i)));
		Out.Append(M.data(), static_cast<int32>(M.size()));
		Flush();
		return Socket != nullptr;
	}

	bool receive(std::vector<uint8_t>& M) override
	{
		Flush();
		Pump();
		if (In.Num() < 4) return false;
		const uint32 N = static_cast<uint32>(In[0]) | static_cast<uint32>(In[1]) << 8 | static_cast<uint32>(In[2]) << 16 | static_cast<uint32>(In[3]) << 24;
		if (N > (64u << 20))
		{
			close();
			return false;
		}
		if (In.Num() < 4 + static_cast<int32>(N)) return false;
		M.assign(In.GetData() + 4, In.GetData() + 4 + N);
		In.RemoveAt(0, 4 + static_cast<int32>(N));
		return true;
	}

	bool connected() const override { return Socket != nullptr; }

	void close() override
	{
		if (!Socket) return;
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

private:
	void Flush()
	{
		while (Socket && Out.Num() > 0)
		{
			int32 Sent = 0;
			if (!Socket->Send(Out.GetData(), Out.Num(), Sent))
			{
				if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() == SE_EWOULDBLOCK) return;
				close();
				return;
			}
			if (Sent <= 0) return;
			Out.RemoveAt(0, Sent);
		}
	}
	void Pump()
	{
		uint8 Buf[16384];
		while (Socket)
		{
			int32 Read = 0;
			if (!Socket->Recv(Buf, sizeof Buf, Read))
			{
				if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() == SE_EWOULDBLOCK) return;
				close();
				return;
			}
			if (Read <= 0)
			{
				// A non-blocking read with nothing waiting returns true and 0 bytes; a closed peer shows as no longer connected.
				if (Socket->GetConnectionState() != SCS_Connected) close();
				return;
			}
			In.Append(Buf, Read);
		}
	}

	FSocket* Socket = nullptr;
	TArray<uint8> Out, In;
};

// A connected link to the board, or null (no address, unknown host, or nobody answering).
std::unique_ptr<sov::board::Link> Connect(const FString& HostPort, FString& Error)
{
	FString Host = HostPort, PortText = TEXT("7788");
	HostPort.Split(TEXT(":"), &Host, &PortText, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	ISocketSubsystem* Subsys = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedPtr<FInternetAddr> Addr = Subsys->GetAddressFromString(Host);
	if (!Addr.IsValid() || !Addr->IsValid())
	{
		FAddressInfoResult Info = Subsys->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None);
		if (Info.ReturnCode != SE_NO_ERROR || Info.Results.Num() == 0)
		{
			Error = FString::Printf(TEXT("the board's address (%s) is unknown"), *Host);
			return nullptr;
		}
		Addr = Info.Results[0].Address;
	}
	Addr->SetPort(FCString::Atoi(*PortText));
	FSocket* S = Subsys->CreateSocket(NAME_Stream, TEXT("SovBoard"), Addr->GetProtocolType());
	if (!S || !S->Connect(*Addr))
	{
		if (S) Subsys->DestroySocket(S);
		Error = FString::Printf(TEXT("the board at %s did not answer"), *HostPort);
		return nullptr;
	}
	return std::make_unique<FSocketLink>(S);
}
}  // namespace

FString SovBoard::Address()
{
	FString A;
	if (FParse::Value(FCommandLine::Get(), TEXT("SovBoard="), A)) return A;
	if (GConfig) GConfig->GetString(kSection, TEXT("Board"), A, GGameUserSettingsIni);
	return A.TrimStartAndEnd();
}

void SovBoard::SetAddress(const FString& HostPort)
{
	if (!GConfig) return;
	GConfig->SetString(kSection, TEXT("Board"), *HostPort.TrimStartAndEnd(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString SovBoard::CleanName(const FString& Name)
{
	FString Out;
	for (const TCHAR C : Name)
	{
		if (FChar::IsAlnum(C) && C < 128) Out.AppendChar(C);
		else if (C == '_' || C == '-' || C == '.') Out.AppendChar(C);
		else if (C == ' ') Out.AppendChar('_');
		if (Out.Len() >= static_cast<int32>(sov::board::kMaxName)) break;
	}
	return Out.IsEmpty() ? FString(TEXT("Player")) : Out;
}

bool SovBoard::Submit(const FString& Board, int32 Week, const FString& Name, const TArray<uint8>& Save, FString& Line)
{
	const FString Who = CleanName(Name);
	FString Error;
	std::unique_ptr<sov::board::Link> L = Connect(Board, Error);
	if (!L)
	{
		Line = Error;
		return false;
	}
	sov::board::Client C(std::move(L), 120);
	const sov::board::SubmitReply R = C.submit(Week, TCHAR_TO_UTF8(*Who), std::vector<uint8_t>(Save.GetData(), Save.GetData() + Save.Num()));
	Line = R.accepted ? FString::Printf(TEXT("the board took your game as %s: rank %d"), *Who, R.rank)
					  : FString::Printf(TEXT("the board refused it: %s"), UTF8_TO_TCHAR(R.error.c_str()));
	return R.accepted;
}

bool SovBoard::Fetch(const FString& Board, int32 Week, TArray<FSovBoardEntry>& Entries)
{
	FString Error;
	std::unique_ptr<sov::board::Link> L = Connect(Board, Error);
	if (!L) return false;
	sov::board::Client C(std::move(L), 10);
	for (const sov::board::Entry& E : C.ranking(Week)) Entries.Add({UTF8_TO_TCHAR(E.name.c_str()), E.goalMet, E.turn, E.score});
	return true;
}

void SovBoard::SubmitAsync(int32 Week, const FString& Name, TArray<uint8> Save, TFunction<void(bool, const FString&)> Done)
{
	const FString Board = Address();
	if (Board.IsEmpty())
	{
		Done(false, TEXT("no board is set (Settings: Challenge board)"));
		return;
	}
	Async(EAsyncExecution::Thread, [Board, Name, Week, Save = MoveTemp(Save), Done = MoveTemp(Done)]() mutable {
		FString Line;
		const bool bOk = Submit(Board, Week, Name, Save, Line);
		AsyncTask(ENamedThreads::GameThread, [bOk, Line, Done = MoveTemp(Done)]() { Done(bOk, Line); });
	});
}

void SovBoard::FetchAsync(int32 Week, TFunction<void(bool, const TArray<FSovBoardEntry>&)> Done)
{
	const FString Board = Address();
	if (Board.IsEmpty())
	{
		Done(false, {});
		return;
	}
	Async(EAsyncExecution::Thread, [Board, Week, Done = MoveTemp(Done)]() mutable {
		TArray<FSovBoardEntry> Out;
		const bool bOk = Fetch(Board, Week, Out);
		AsyncTask(ENamedThreads::GameThread, [bOk, Out = MoveTemp(Out), Done = MoveTemp(Done)]() { Done(bOk, Out); });
	});
}
