#include "SovNetLink.h"

#include "Common/TcpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
constexpr uint32 kMaxFrame = 64u << 20;  // a whole save fits (net/src/tcp.cpp)

ISocketSubsystem* Subsystem() { return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM); }
}  // namespace

FSovTcpLink::FSovTcpLink(FSocket* InSocket) : Socket(InSocket)
{
	if (Socket)
	{
		Socket->SetNonBlocking(true);
		Socket->SetNoDelay(true);
	}
}

FSovTcpLink::~FSovTcpLink() { close(); }

void FSovTcpLink::close()
{
	if (Socket)
	{
		Socket->Close();
		Subsystem()->DestroySocket(Socket);
		Socket = nullptr;
	}
}

bool FSovTcpLink::send(const std::vector<uint8_t>& Message)
{
	if (!Socket)
	{
		return false;
	}
	const uint32 N = static_cast<uint32>(Message.size());
	for (int32 i = 0; i < 4; ++i)
	{
		Out.Add(static_cast<uint8>(N >> (8 * i)));
	}
	Out.Append(Message.data(), static_cast<int32>(Message.size()));
	Flush();
	return Socket != nullptr;
}

void FSovTcpLink::Flush()
{
	while (Socket && Out.Num() > 0)
	{
		int32 Sent = 0;
		if (!Socket->Send(Out.GetData(), Out.Num(), Sent))
		{
			const ESocketErrors Err = Subsystem()->GetLastErrorCode();
			if (Err == SE_EWOULDBLOCK || Err == SE_EINPROGRESS)
			{
				return;
			}
			close();
			return;
		}
		if (Sent <= 0)
		{
			return;
		}
		Out.RemoveAt(0, Sent, EAllowShrinking::No);
	}
}

void FSovTcpLink::Pump()
{
	uint8 Buf[16384];
	while (Socket)
	{
		int32 Read = 0;
		if (!Socket->Recv(Buf, sizeof(Buf), Read, ESocketReceiveFlags::None))
		{
			// Nothing waiting fails with EWOULDBLOCK; any other failure (a graceful close included) ends the link.
			if (Subsystem()->GetLastErrorCode() == SE_EWOULDBLOCK)
			{
				return;
			}
			close();
			return;
		}
		if (Read <= 0)
		{
			// A clean close reads 0 bytes with success; a socket with nothing waiting fails with EWOULDBLOCK.
			if (Socket->GetConnectionState() != SCS_Connected)
			{
				close();
			}
			return;
		}
		In.Append(Buf, Read);
	}
}

bool FSovTcpLink::receive(std::vector<uint8_t>& Message)
{
	Flush();
	Pump();
	if (In.Num() < 4)
	{
		return false;
	}
	const uint32 N = static_cast<uint32>(In[0]) | static_cast<uint32>(In[1]) << 8 | static_cast<uint32>(In[2]) << 16 | static_cast<uint32>(In[3]) << 24;
	if (N > kMaxFrame)
	{
		close();
		return false;
	}
	if (static_cast<uint32>(In.Num()) < 4 + N)
	{
		return false;
	}
	Message.assign(In.GetData() + 4, In.GetData() + 4 + N);
	In.RemoveAt(0, 4 + static_cast<int32>(N), EAllowShrinking::No);
	return true;
}

std::unique_ptr<sov::net::Link> FSovTcpLink::Connect(const FString& Host, int32 Port, float TimeoutSeconds)
{
	ISocketSubsystem* Sockets = Subsystem();
	TSharedPtr<FInternetAddr> Addr = Sockets->CreateInternetAddr();
	bool bValid = false;
	Addr->SetIp(*Host, bValid);
	if (!bValid)
	{
		// A host name: resolve it.
		FAddressInfoResult Found = Sockets->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None);
		if (Found.ReturnCode != SE_NO_ERROR || Found.Results.Num() == 0)
		{
			return nullptr;
		}
		Addr = Found.Results[0].Address;
	}
	Addr->SetPort(Port);
	FSocket* S = Sockets->CreateSocket(NAME_Stream, TEXT("SovereignClient"), Addr->GetProtocolType());
	if (!S)
	{
		return nullptr;
	}
	S->SetNonBlocking(true);
	S->Connect(*Addr);
	// Wait for the connection without hanging forever on a dead address.
	const double Until = FPlatformTime::Seconds() + TimeoutSeconds;
	while (FPlatformTime::Seconds() < Until)
	{
		if (S->GetConnectionState() == SCS_Connected)
		{
			return std::make_unique<FSovTcpLink>(S);
		}
		if (S->GetConnectionState() == SCS_ConnectionError)
		{
			break;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	S->Close();
	Sockets->DestroySocket(S);
	return nullptr;
}

FSovTcpListener::FSovTcpListener(int32 Port)
{
	Socket = FTcpSocketBuilder(TEXT("SovereignHost")).AsReusable().AsNonBlocking().BoundToPort(Port).Listening(8).Build();
}

FSovTcpListener::~FSovTcpListener()
{
	if (Socket)
	{
		Socket->Close();
		Subsystem()->DestroySocket(Socket);
	}
}

std::unique_ptr<sov::net::Link> FSovTcpListener::accept()
{
	if (!Socket)
	{
		return nullptr;
	}
	bool bPending = false;
	if (!Socket->HasPendingConnection(bPending) || !bPending)
	{
		return nullptr;
	}
	FSocket* C = Socket->Accept(TEXT("SovereignPeer"));
	return C ? std::make_unique<FSovTcpLink>(C) : nullptr;
}
