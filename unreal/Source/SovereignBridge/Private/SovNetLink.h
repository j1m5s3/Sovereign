// sov::net links over the engine's sockets (LAN and direct IP): non-blocking TCP with each
// message framed by its 4-byte length, the same framing as net/'s own TCP links, so an Unreal
// game and the sovnet tool can share a session.
#pragma once

#include "CoreMinimal.h"

#include <memory>

#include "sovereign_net/session.h"

class FSocket;

class FSovTcpLink : public sov::net::Link
{
public:
	explicit FSovTcpLink(FSocket* InSocket);
	virtual ~FSovTcpLink() override;
	virtual bool send(const std::vector<uint8_t>& Message) override;
	virtual bool receive(std::vector<uint8_t>& Message) override;
	virtual bool connected() const override { return Socket != nullptr; }
	virtual void close() override;

	// Connects to host:port (blocking up to TimeoutSeconds); null when nobody answered.
	static std::unique_ptr<sov::net::Link> Connect(const FString& Host, int32 Port, float TimeoutSeconds = 5.f);

private:
	void Flush();
	void Pump();
	FSocket* Socket = nullptr;
	TArray<uint8> In, Out;
};

class FSovTcpListener : public sov::net::Listener
{
public:
	explicit FSovTcpListener(int32 Port);
	virtual ~FSovTcpListener() override;
	bool IsListening() const { return Socket != nullptr; }
	virtual std::unique_ptr<sov::net::Link> accept() override;

private:
	FSocket* Socket = nullptr;
};
