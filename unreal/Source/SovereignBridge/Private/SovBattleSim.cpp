#include "SovBattleSim.h"

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
	Enemy.update(Sim, 1 - Human, Dt);
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
	return Out;
}
