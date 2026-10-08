#include "SovBattleSim.h"

#include "sovereign/serialize.h"

#include "Misc/Paths.h"

std::shared_ptr<const sov::battle::Policy> FSovBattleSim::TrainedPolicy()
{
	static std::shared_ptr<const sov::battle::Policy> Cached;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/battle_ai/commander.txt")));
		std::shared_ptr<sov::battle::Policy> P = std::make_shared<sov::battle::Policy>();
		std::string Error;
		if (P->load(std::string(TCHAR_TO_UTF8(*Path)), &Error) && P->valid())
		{
			Cached = P;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Battle AI: %s; the enemy uses the scripted baseline"), UTF8_TO_TCHAR(Error.c_str()));
		}
	}
	return Cached;
}

void FSovBattleSim::Start(const FSovBattleSpec& InSpec, std::shared_ptr<const sov::battle::Policy> Policy)
{
	Spec = InSpec;
	sov::battle::Spec S;
	auto Unit = [](const FSovBattleUnitSpec& U) {
		sov::battle::UnitSpec Out;
		Out.name = TCHAR_TO_UTF8(*U.Name);
		Out.owner = U.Owner;
		Out.strength = U.Strength;
		Out.hp = U.Hp;
		Out.leaderIsUnit = U.bLeaderIsUnit;
		return Out;
	};
	S.attacker = Unit(Spec.Attacker);
	S.defender = Unit(Spec.Defender);
	S.humanSide = Spec.HumanSide;
	S.leaderSide = Spec.bLeaderPresent ? Spec.HumanSide : -1;
	S.leaderStrength = Spec.LeaderStrength;
	S.leaderHp = Spec.LeaderHp;
	S.seed = static_cast<uint32_t>(Spec.Seed);
	S.timeLimit = Spec.TimeLimit;
	Sim.start(S);
	bEnemyTrained = Policy && Policy->valid();
	if (bEnemyTrained)
	{
		Enemy = sov::battle::Commander::trained(Policy, 0.f, static_cast<uint64_t>(Spec.Seed));
	}
	else
	{
		Enemy = sov::battle::Commander::fixed(sov::battle::Order::Advance);
	}
	Enemy.setCounter(Spec.Counter);
	Idle = sov::battle::Commander::fixed(sov::battle::Order::Advance);
	Mirror();
}

void FSovBattleSim::SetOrder(int32 Side, sov::battle::Order Order, int32 Squad)
{
	if (Squad < 0)
	{
		Sim.setAllOrders(Side, Order);
	}
	else
	{
		Sim.setOrder(Side, Squad, Order);
	}
}

void FSovBattleSim::Step(float Dt, TOptional<FVector2D> LeaderMove, bool bStrike)
{
	if (Sim.finished())
	{
		return;
	}
	const int32 Human = Spec.HumanSide;
	if (!bRemoteEnemy)
	{
		Enemy.update(Sim, 1 - Human, Dt);
	}
	if (LeaderMove.IsSet())
	{
		sov::battle::LeaderInput In;
		In.move = sov::battle::Vec2(static_cast<float>(LeaderMove->X), static_cast<float>(LeaderMove->Y));
		In.strike = bStrike;
		Sim.step(Dt, &In);
	}
	else
	{
		Idle.update(Sim, Human, Dt);
		Sim.step(Dt);
	}
	Mirror();
}

FSovBattleSnapshot FSovBattleSim::Snapshot() const
{
	FSovBattleSnapshot S;
	S.TimeLeft = Sim.timeLeft();
	S.bFinished = Sim.finished();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		S.Alive[Side] = Sim.alive(Side);
		S.Started[Side] = Sim.started(Side);
		for (int32 Squad = 0; Squad < 3; ++Squad) S.Orders[Side][Squad] = static_cast<uint8>(Sim.order(Side, Squad));
	}
	S.Men = Men;
	return S;
}

void FSovBattleSim::StartRemoteView(const FSovBattleSpec& InSpec)
{
	Spec = InSpec;
	bRemoteView = true;
	View = FSovBattleSnapshot();
	View.TimeLeft = Spec.TimeLimit;
	Men.Reset();
}

void FSovBattleSim::ApplySnapshot(const FSovBattleSnapshot& S)
{
	View = S;
	Men = S.Men;
}

namespace
{
void PutI32(std::vector<uint8_t>& B, int32 V)
{
	for (int32 i = 0; i < 4; ++i) B.push_back(static_cast<uint8_t>(static_cast<uint32>(V) >> (8 * i)));
}
bool GetI32(const std::vector<uint8_t>& B, size_t& At, int32& V)
{
	if (At + 4 > B.size()) return false;
	uint32 U = 0;
	for (int32 i = 0; i < 4; ++i) U |= static_cast<uint32>(B[At + i]) << (8 * i);
	V = static_cast<int32>(U);
	At += 4;
	return true;
}
}  // namespace

std::vector<uint8_t> FSovBattleSnapshot::Encode() const
{
	// Positions and hit points in hundredths: plenty for drawing, and no float on the wire.
	std::vector<uint8_t> B;
	PutI32(B, FMath::RoundToInt(TimeLeft * 100.f));
	for (int32 Side = 0; Side < 2; ++Side)
	{
		PutI32(B, Alive[Side]);
		PutI32(B, Started[Side]);
		for (int32 Squad = 0; Squad < 3; ++Squad) B.push_back(Orders[Side][Squad]);
	}
	B.push_back(bFinished ? 1 : 0);
	PutI32(B, Men.Num());
	for (const FSovSoldier& M : Men)
	{
		PutI32(B, FMath::RoundToInt(M.Pos.X * 100.f));
		PutI32(B, FMath::RoundToInt(M.Pos.Y * 100.f));
		PutI32(B, FMath::RoundToInt(M.Hp * 100.f));
		PutI32(B, FMath::RoundToInt(M.MaxHp * 100.f));
		B.push_back(static_cast<uint8_t>(M.Side));
		B.push_back(static_cast<uint8_t>(M.Squad + 1));
		B.push_back(static_cast<uint8_t>((M.bLeader ? 1 : 0) | (M.bAlive ? 2 : 0)));
	}
	return B;
}

bool FSovBattleSnapshot::Decode(const std::vector<uint8_t>& B)
{
	size_t At = 0;
	int32 V = 0;
	if (!GetI32(B, At, V)) return false;
	TimeLeft = V / 100.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!GetI32(B, At, Alive[Side]) || !GetI32(B, At, Started[Side]) || At + 3 > B.size()) return false;
		for (int32 Squad = 0; Squad < 3; ++Squad) Orders[Side][Squad] = FMath::Min<uint8>(B[At++], sov::battle::kOrders - 1);
	}
	if (At + 1 > B.size()) return false;
	bFinished = B[At++] != 0;
	int32 N = 0;
	if (!GetI32(B, At, N) || N < 0 || N > 4096) return false;
	Men.SetNum(N);
	for (FSovSoldier& M : Men)
	{
		int32 X = 0, Y = 0, Hp = 0, MaxHp = 0;
		if (!GetI32(B, At, X) || !GetI32(B, At, Y) || !GetI32(B, At, Hp) || !GetI32(B, At, MaxHp) || At + 3 > B.size()) return false;
		M.Pos = FVector2D(X / 100.f, Y / 100.f);
		M.Hp = Hp / 100.f;
		M.MaxHp = MaxHp / 100.f;
		M.Side = B[At++] & 1;
		M.Squad = static_cast<int32>(B[At++]) - 1;
		const uint8 Flags = B[At++];
		M.bLeader = (Flags & 1) != 0;
		M.bAlive = (Flags & 2) != 0;
	}
	return At == B.size();
}

void FSovBattleSim::Mirror()
{
	const std::vector<sov::battle::Soldier>& From = Sim.soldiers();
	Men.SetNum(static_cast<int32>(From.size()));
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		const sov::battle::Soldier& S = From[static_cast<size_t>(i)];
		FSovSoldier& M = Men[i];
		M.Pos = FVector2D(S.pos.x, S.pos.y);
		M.Hp = S.hp;
		M.MaxHp = S.maxHp;
		M.Side = S.side;
		M.Squad = S.squad;
		M.bLeader = S.leader;
		M.bAlive = S.alive;
	}
}

FSovBattleResult FSovBattleSim::Result() const
{
	const sov::battle::Result R = Sim.result();
	FSovBattleResult Out;
	Out.ToAttacker = R.toAttacker;
	Out.ToDefender = R.toDefender;
	Out.LeaderWound = R.leaderWound;
	Out.bTimedOut = R.timedOut;
	Out.Winner = R.winner;
	if (!bRemoteView && Sim.spec().humanSide >= 0)
	{
		const sov::battle::Habits H = Sim.habits(Sim.spec().humanSide);
		Out.Habits = {H.flank, H.fallBack, H.huntLeader, H.leaderFront};
	}
	return Out;
}

namespace
{
constexpr uint32 kRecordingMagic = 0x42564F53;  // "SOVB"
constexpr uint32 kRecordingVersion = 1;

void WriteColor(sov::ByteWriter& W, const FLinearColor& C)
{
	const FColor B = C.ToFColor(true);
	W.u8(B.R);
	W.u8(B.G);
	W.u8(B.B);
}
FLinearColor ReadColor(sov::ByteReader& R)
{
	const uint8 Red = R.u8(), Green = R.u8(), Blue = R.u8();
	return FLinearColor(FColor(Red, Green, Blue));
}
void WriteUnit(sov::ByteWriter& W, const FSovBattleUnitSpec& U)
{
	W.str(TCHAR_TO_UTF8(*U.Name));
	W.i32(U.Owner);
	W.i32(U.Strength);
	W.i32(U.Hp);
	W.boolean(U.bLeaderIsUnit);
}
void ReadUnit(sov::ByteReader& R, FSovBattleUnitSpec& U)
{
	U.Name = UTF8_TO_TCHAR(R.str().c_str());
	U.Owner = R.i32();
	U.Strength = R.i32();
	U.Hp = R.i32();
	U.bLeaderIsUnit = R.boolean();
}
}  // namespace

std::vector<uint8_t> FSovBattleRecording::Encode() const
{
	sov::ByteWriter W;
	W.u32(kRecordingMagic);
	W.u32(kRecordingVersion);
	W.str(TCHAR_TO_UTF8(*Title));
	WriteUnit(W, Spec.Attacker);
	WriteUnit(W, Spec.Defender);
	W.i32(Spec.HumanSide);
	W.boolean(Spec.bLeaderPresent);
	W.i32(Spec.LeaderStrength);
	W.i32(Spec.LeaderHp);
	W.i32(Spec.Seed);
	W.i32(FMath::RoundToInt(Spec.TimeLimit * 1000.f));
	WriteColor(W, Ground);
	WriteColor(W, AttackerColor);
	WriteColor(W, DefenderColor);
	W.boolean(bWoods);
	W.boolean(bCity);
	W.boolean(bWalls);
	W.u32(static_cast<uint32>(Frames.size()));
	for (const std::vector<uint8_t>& F : Frames) W.bytes(F);
	return W.take();
}

bool FSovBattleRecording::Decode(const std::vector<uint8_t>& Bytes)
{
	sov::ByteReader R(Bytes);
	if (R.u32() != kRecordingMagic || R.u32() != kRecordingVersion) return false;
	FSovBattleRecording Out;
	Out.Title = UTF8_TO_TCHAR(R.str().c_str());
	ReadUnit(R, Out.Spec.Attacker);
	ReadUnit(R, Out.Spec.Defender);
	Out.Spec.HumanSide = R.i32();
	Out.Spec.bLeaderPresent = R.boolean();
	Out.Spec.LeaderStrength = R.i32();
	Out.Spec.LeaderHp = R.i32();
	Out.Spec.Seed = R.i32();
	Out.Spec.TimeLimit = R.i32() / 1000.f;
	Out.Ground = ReadColor(R);
	Out.AttackerColor = ReadColor(R);
	Out.DefenderColor = ReadColor(R);
	Out.bWoods = R.boolean();
	Out.bCity = R.boolean();
	Out.bWalls = R.boolean();
	const uint32 Count = R.u32();
	if (!R.checkCount(Count, 4)) return false;
	Out.Frames.resize(Count);
	for (std::vector<uint8_t>& F : Out.Frames) F = R.bytes();
	if (!R.ok() || !R.atEnd()) return false;
	*this = std::move(Out);
	return true;
}