#include "SovSteam.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#include <deque>
#include <map>
#include <vector>

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingmessages.h"
THIRD_PARTY_INCLUDES_END

namespace
{
constexpr int32 kChannel = 0;
constexpr size_t kChunk = 400 * 1024;  // under Steam's 512 KB reliable message limit
constexpr const TCHAR* kDevAppId = TEXT("480");  // Spacewar, Valve's development App ID

std::unique_ptr<FSovSteam>& Instance()
{
	static std::unique_ptr<FSovSteam> P;
	return P;
}

SteamNetworkingIdentity Identity(uint64 Id)
{
	SteamNetworkingIdentity I;
	I.SetSteamID64(Id);
	return I;
}

// What arrived from one peer: whole messages, and the chunks of the one in progress.
struct FPeerQueue
{
	std::deque<std::vector<uint8_t>> Messages;
	std::vector<uint8_t> Partial;
	bool bClosed = false;
};

class FSteamLink : public sov::net::Link
{
public:
	FSteamLink(uint64 InPeer, std::shared_ptr<FPeerQueue> InQueue) : Peer(InPeer), Queue(std::move(InQueue)) {}
	virtual ~FSteamLink() override { close(); }
	virtual bool send(const std::vector<uint8_t>& M) override
	{
		if (Queue->bClosed) return false;
		const SteamNetworkingIdentity To = Identity(Peer);
		size_t At = 0;
		do
		{
			const size_t N = std::min(kChunk, M.size() - At);
			std::vector<uint8_t> Buf(1 + N);
			Buf[0] = At + N < M.size() ? 1 : 0;  // 1: more chunks follow
			if (N) memcpy(Buf.data() + 1, M.data() + At, N);
			const EResult R = SteamNetworkingMessages()->SendMessageToUser(To, Buf.data(), static_cast<uint32>(Buf.size()),
				k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_AutoRestartBrokenSession, kChannel);
			if (R != k_EResultOK)
			{
				Queue->bClosed = true;
				return false;
			}
			At += N;
		} while (At < M.size());
		return true;
	}
	virtual bool receive(std::vector<uint8_t>& M) override
	{
		if (Queue->Messages.empty()) return false;
		M = std::move(Queue->Messages.front());
		Queue->Messages.pop_front();
		return true;
	}
	virtual bool connected() const override { return !Queue->bClosed; }
	virtual void close() override
	{
		if (!Queue->bClosed)
		{
			Queue->bClosed = true;
			SteamNetworkingMessages()->CloseSessionWithUser(Identity(Peer));
		}
	}

private:
	uint64 Peer;
	std::shared_ptr<FPeerQueue> Queue;
};
}  // namespace

struct FSovSteam::FImpl
{
	explicit FImpl(FSovSteam& InOwner) : Owner(InOwner) {}

	FSovSteam& Owner;
	std::map<uint64, std::shared_ptr<FPeerQueue>> Peers;
	std::deque<uint64> NewPeers;  // lobby members who messaged the host, waiting for accept()
	bool bListening = false;
	CCallResult<FImpl, LobbyCreated_t> CreateResult;
	CCallResult<FImpl, LobbyEnter_t> EnterResult;

	bool IsMember(uint64 Id) const
	{
		const CSteamID Room(Owner.LobbyIdValue);
		const int32 N = SteamMatchmaking()->GetNumLobbyMembers(Room);
		for (int32 i = 0; i < N; ++i)
		{
			if (SteamMatchmaking()->GetLobbyMemberByIndex(Room, i).ConvertToUint64() == Id) return true;
		}
		return false;
	}

	std::shared_ptr<FPeerQueue> QueueFor(uint64 Id)
	{
		auto& Q = Peers[Id];
		if (!Q || Q->bClosed) Q = std::make_shared<FPeerQueue>();
		return Q;
	}

	void OnCreated(LobbyCreated_t* R, bool bIOFailure)
	{
		if (bIOFailure || R->m_eResult != k_EResultOK)
		{
			Owner.Lobby = ELobby::Failed;
			Owner.Error = FString::Printf(TEXT("Steam could not create a lobby (result %d)"), bIOFailure ? -1 : static_cast<int32>(R->m_eResult));
			return;
		}
		Owner.LobbyIdValue = R->m_ulSteamIDLobby;
		Owner.Lobby = ELobby::In;
		SteamMatchmaking()->SetLobbyData(CSteamID(Owner.LobbyIdValue), "sovereign", "1");
	}

	void OnEntered(LobbyEnter_t* R, bool bIOFailure)
	{
		if (bIOFailure || R->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
		{
			Owner.Lobby = ELobby::Failed;
			Owner.Error = TEXT("Steam could not join the lobby");
			return;
		}
		Owner.LobbyIdValue = R->m_ulSteamIDLobby;
		Owner.Lobby = ELobby::In;
	}

	STEAM_CALLBACK(FImpl, OnJoinRequested, GameLobbyJoinRequested_t)
	{
		Owner.PendingInvite = pParam->m_steamIDLobby.ConvertToUint64();
	}

	STEAM_CALLBACK(FImpl, OnSessionRequest, SteamNetworkingMessagesSessionRequest_t)
	{
		const uint64 From = pParam->m_identityRemote.GetSteamID64();
		if (Owner.Lobby == ELobby::In && IsMember(From))
		{
			SteamNetworkingMessages()->AcceptSessionWithUser(pParam->m_identityRemote);
		}
	}

	STEAM_CALLBACK(FImpl, OnSessionFailed, SteamNetworkingMessagesSessionFailed_t)
	{
		const uint64 Who = pParam->m_info.m_identityRemote.GetSteamID64();
		auto It = Peers.find(Who);
		if (It != Peers.end() && It->second) It->second->bClosed = true;
	}

	STEAM_CALLBACK(FImpl, OnLobbyChange, LobbyChatUpdate_t)
	{
		// A member who leaves or drops loses their link.
		const uint32 Gone = k_EChatMemberStateChangeLeft | k_EChatMemberStateChangeDisconnected | k_EChatMemberStateChangeKicked |
			k_EChatMemberStateChangeBanned;
		if ((pParam->m_rgfChatMemberStateChange & Gone) == 0) return;
		auto It = Peers.find(pParam->m_ulSteamIDUserChanged);
		if (It != Peers.end() && It->second) It->second->bClosed = true;
	}

	void Pump()
	{
		SteamNetworkingMessage_t* Msgs[32];
		for (;;)
		{
			const int32 N = SteamNetworkingMessages()->ReceiveMessagesOnChannel(kChannel, Msgs, 32);
			if (N <= 0) break;
			for (int32 i = 0; i < N; ++i)
			{
				SteamNetworkingMessage_t* M = Msgs[i];
				const uint64 From = M->m_identityPeer.GetSteamID64();
				auto It = Peers.find(From);
				std::shared_ptr<FPeerQueue> Q = It != Peers.end() ? It->second : nullptr;
				if ((!Q || Q->bClosed) && bListening && IsMember(From))
				{
					Q = QueueFor(From);
					NewPeers.push_back(From);
				}
				const uint8_t* Data = static_cast<const uint8_t*>(M->m_pData);
				if (Q && !Q->bClosed && M->m_cbSize >= 1)
				{
					Q->Partial.insert(Q->Partial.end(), Data + 1, Data + M->m_cbSize);
					if (Data[0] == 0)
					{
						Q->Messages.push_back(std::move(Q->Partial));
						Q->Partial.clear();
					}
				}
				M->Release();
			}
		}
	}
};

namespace
{
class FSteamListener : public sov::net::Listener
{
public:
	explicit FSteamListener(FSovSteam::FImpl& InImpl) : Impl(InImpl) { Impl.bListening = true; }
	virtual ~FSteamListener() override { Impl.bListening = false; }
	virtual std::unique_ptr<sov::net::Link> accept() override
	{
		if (Impl.NewPeers.empty()) return nullptr;
		const uint64 Id = Impl.NewPeers.front();
		Impl.NewPeers.pop_front();
		return std::make_unique<FSteamLink>(Id, Impl.Peers[Id]);
	}

private:
	FSovSteam::FImpl& Impl;
};
}  // namespace

FSovSteam::FSovSteam() = default;
FSovSteam::~FSovSteam() = default;

FSovSteam* FSovSteam::Get() { return Instance().get(); }

bool FSovSteam::Start(FString& OutError)
{
	if (Instance())
	{
		return true;
	}
	// The API is delay-loaded by the engine's Steamworks module; load it from the engine's copy.
	const FString Dll = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Steamworks"), STEAM_SDK_VER_PATH, TEXT("Win64/steam_api64.dll"));
	if (!FPlatformProcess::GetDllHandle(*Dll))
	{
		OutError = FString::Printf(TEXT("cannot load %s"), *Dll);
		return false;
	}
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("SteamAppId")).IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("SteamAppId"), kDevAppId);
		FPlatformMisc::SetEnvironmentVar(TEXT("SteamGameId"), kDevAppId);
	}
	SteamErrMsg Err = {};
	if (SteamAPI_InitEx(&Err) != k_ESteamAPIInitResult_OK)
	{
		OutError = FString::Printf(TEXT("Steam is not available (%s). Is the Steam client running and signed in?"), UTF8_TO_TCHAR(Err));
		return false;
	}
	Instance().reset(new FSovSteam());
	Instance()->Impl = std::make_unique<FImpl>(*Instance());
	return true;
}

void FSovSteam::Tick()
{
	if (FSovSteam* S = Get())
	{
		SteamAPI_RunCallbacks();
		S->Impl->Pump();
	}
}

void FSovSteam::CreateLobby(int32 MaxMembers)
{
	Lobby = ELobby::Creating;
	const SteamAPICall_t Call = SteamMatchmaking()->CreateLobby(k_ELobbyTypeFriendsOnly, FMath::Clamp(MaxMembers, 2, 12));
	Impl->CreateResult.Set(Call, Impl.get(), &FImpl::OnCreated);
}

void FSovSteam::JoinLobby(uint64 Id)
{
	Lobby = ELobby::Joining;
	const SteamAPICall_t Call = SteamMatchmaking()->JoinLobby(CSteamID(Id));
	Impl->EnterResult.Set(Call, Impl.get(), &FImpl::OnEntered);
}

void FSovSteam::LeaveLobby()
{
	if (Lobby == ELobby::In)
	{
		SteamMatchmaking()->LeaveLobby(CSteamID(LobbyIdValue));
	}
	Lobby = ELobby::None;
	LobbyIdValue = 0;
}

bool FSovSteam::OwnsLobby() const { return Lobby == ELobby::In && LobbyOwner() == SteamUser()->GetSteamID().ConvertToUint64(); }

uint64 FSovSteam::LobbyOwner() const
{
	return Lobby == ELobby::In ? SteamMatchmaking()->GetLobbyOwner(CSteamID(LobbyIdValue)).ConvertToUint64() : 0;
}

void FSovSteam::InviteFriends()
{
	if (Lobby == ELobby::In)
	{
		SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(LobbyIdValue));
	}
}

FString FSovSteam::PersonaName() const { return UTF8_TO_TCHAR(SteamFriends()->GetPersonaName()); }

uint64 FSovSteam::TakeInvite()
{
	const uint64 Id = PendingInvite;
	PendingInvite = 0;
	return Id;
}

std::unique_ptr<sov::net::Listener> FSovSteam::MakeListener() { return std::make_unique<FSteamListener>(*Impl); }

std::unique_ptr<sov::net::Link> FSovSteam::MakeLink(uint64 HostSteamId)
{
	return std::make_unique<FSteamLink>(HostSteamId, Impl->QueueFor(HostSteamId));
}
