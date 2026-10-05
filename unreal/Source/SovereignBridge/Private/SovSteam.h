// Steam lobbies and invites, and sov::net links over Steam's networking (engine doc, Core
// foundations: Steamworks first). Steamworks is called directly rather than through the
// engine's online subsystem, which in editor builds writes steam_appid.txt beside the engine
// executable; the development App ID (480, Spacewar) goes in through the SteamAppId variable.
// The host opens a friends-only lobby and invites friends from the Steam overlay; a friend
// running Sovereign who accepts joins the lobby, and the session runs over
// ISteamNetworkingMessages with the lobby's owner as host.
#pragma once

#include "CoreMinimal.h"

#include <cstdint>
#include <memory>

#include "sovereign_net/session.h"

class FSovSteam
{
public:
	// The one Steam connection for the process; null until Start succeeds.
	static FSovSteam* Get();
	// Loads the Steam API and connects to the running Steam client. Idempotent.
	static bool Start(FString& OutError);
	static void Tick();  // runs Steam callbacks and routes incoming messages

	enum class ELobby : uint8 { None, Creating, Joining, In, Failed };

	void CreateLobby(int32 MaxMembers);
	void JoinLobby(uint64 LobbyId);
	void LeaveLobby();
	ELobby LobbyState() const { return Lobby; }
	uint64 LobbyId() const { return Lobby == ELobby::In ? LobbyIdValue : 0; }
	bool OwnsLobby() const;
	uint64 LobbyOwner() const;
	// Opens the overlay's invite dialog for the lobby.
	void InviteFriends();
	FString PersonaName() const;
	// A lobby a friend invited us to (accepted in the overlay while Sovereign runs); 0 when none.
	uint64 TakeInvite();
	FString LastError() const { return Error; }

	// The host's listener: links for lobby members who message it.
	std::unique_ptr<sov::net::Listener> MakeListener();
	// A client's link to the lobby's owner.
	std::unique_ptr<sov::net::Link> MakeLink(uint64 HostSteamId);

	struct FImpl;
	~FSovSteam();

private:
	FSovSteam();
	std::unique_ptr<FImpl> Impl;
	ELobby Lobby = ELobby::None;
	uint64 LobbyIdValue = 0;
	uint64 PendingInvite = 0;
	FString Error;
	friend struct FImpl;
};
