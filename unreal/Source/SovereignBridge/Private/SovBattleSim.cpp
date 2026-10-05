#include "SovBattleSim.h"

void FSovBattleSim::Start(const FSovBattleSpec& InSpec)
{
	Spec = InSpec;
	Men.Reset();
	Rng.Initialize(Spec.Seed);
	Elapsed = 0.f;
	bFinished = bTimedOut = false;
	Leader = -1;
	UnitLeader[0] = UnitLeader[1] = -1;
	Charge[0] = Charge[1] = true;
	// Soldiers follow HP: one per 10 HP, at least one (an unescorted leader fights alone).
	auto Field = [&](const FSovBattleUnitSpec& U, int32 Side) {
		const float X = Side == 0 ? -1600.f : 1600.f;
		if (U.bLeaderIsUnit)
		{
			FSovSoldier L;
			L.Side = Side;
			L.bLeader = true;
			L.Hp = L.MaxHp = static_cast<float>(U.Hp) / 3.f;
			L.Pos = FVector2D(X, 0.f);
			UnitLeader[Side] = Men.Add(L);
			if (Side == Spec.HumanSide) Leader = UnitLeader[Side];
			return;
		}
		const int32 N = FMath::Max(1, U.Hp / 10);
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(N / 5.f));
		for (int32 i = 0; i < N; ++i)
		{
			FSovSoldier S;
			S.Side = Side;
			S.Pos = FVector2D(X + (Side == 0 ? -1.f : 1.f) * 160.f * (i / 5), (i % 5 - 2) * 180.f + Rng.FRandRange(-20.f, 20.f));
			Men.Add(S);
		}
		(void)Rows;
	};
	Field(Spec.Attacker, 0);
	Field(Spec.Defender, 1);
	const bool bUnitIsLeader = (Spec.HumanSide == 0 ? Spec.Attacker : Spec.Defender).bLeaderIsUnit;
	if (Spec.bLeaderPresent && !bUnitIsLeader)
	{
		// The escorted leader rides behind its men: a hero worth three soldiers.
		FSovSoldier L;
		L.Side = Spec.HumanSide;
		L.bLeader = true;
		L.Hp = L.MaxHp = static_cast<float>(Spec.LeaderHp) / 3.f;
		L.Pos = FVector2D(Spec.HumanSide == 0 ? -2300.f : 2300.f, 0.f);
		Leader = Men.Add(L);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		Initial[Side] = Alive(Side);
	}
}

int32 FSovBattleSim::Alive(int32 Side) const
{
	int32 N = 0;
	for (const FSovSoldier& S : Men)
	{
		N += S.bAlive && S.Side == Side && !S.bLeader;
	}
	return N;
}

int32 FSovBattleSim::StrengthOf(const FSovSoldier& S) const
{
	if (S.bLeader)
	{
		const FSovBattleUnitSpec& U = S.Side == 0 ? Spec.Attacker : Spec.Defender;
		return U.bLeaderIsUnit ? U.Strength : Spec.LeaderStrength;
	}
	return (S.Side == 0 ? Spec.Attacker : Spec.Defender).Strength;
}

int32 FSovBattleSim::Nearest(int32 From) const
{
	int32 Best = INDEX_NONE;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		if (!Men[i].bAlive || Men[i].Side == Men[From].Side)
		{
			continue;
		}
		const float D = FVector2D::DistSquared(Men[i].Pos, Men[From].Pos);
		if (D < BestDist)
		{
			BestDist = D;
			Best = i;
		}
	}
	return Best;
}

void FSovBattleSim::Swing(int32 From, int32 To)
{
	FSovSoldier& A = Men[From];
	FSovSoldier& D = Men[To];
	A.Cooldown = SwingSeconds;
	// The stronger side lands more and harder blows (05: damage scales with e^(0.04 x diff)).
	const float Diff = static_cast<float>(StrengthOf(A) - StrengthOf(D));
	const float Scale = FMath::Exp(0.04f * Diff);
	const float HitChance = FMath::Clamp(0.55f * Scale, 0.1f, 0.95f);
	if (Rng.FRand() < HitChance)
	{
		D.Hp -= 1.8f * Scale * Rng.FRandRange(0.8f, 1.2f);
		if (D.Hp <= 0.f)
		{
			D.bAlive = false;
		}
	}
}

void FSovBattleSim::Step(float Dt, TOptional<FVector2D> LeaderMove, bool bStrike)
{
	if (bFinished)
	{
		return;
	}
	Elapsed += Dt;
	for (int32 i = 0; i < Men.Num(); ++i)
	{
		FSovSoldier& S = Men[i];
		if (!S.bAlive)
		{
			continue;
		}
		S.Cooldown = FMath::Max(0.f, S.Cooldown - Dt);
		const int32 T = Nearest(i);
		if (T == INDEX_NONE)
		{
			continue;
		}
		const float Dist = FVector2D::Distance(S.Pos, Men[T].Pos);
		if (i == Leader && LeaderMove.IsSet())
		{
			// The human moves and strikes by hand.
			S.Pos += LeaderMove.GetValue() * Speed * 1.15f * Dt;
			if (bStrike && S.Cooldown <= 0.f && Dist <= Reach * 1.2f)
			{
				Swing(i, T);
			}
			continue;
		}
		if (Dist <= Reach)
		{
			if (S.Cooldown <= 0.f)
			{
				Swing(i, T);
			}
			continue;
		}
		// Holding troops wait until the enemy comes close.
		if (!Charge[S.Side] && Dist > 700.f)
		{
			continue;
		}
		S.Pos += (Men[T].Pos - S.Pos).GetSafeNormal() * Speed * Dt;
	}
	// A side that falls below a quarter of its men routs; the time cap settles a stalemate.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Alive(Side) * 4 < Initial[Side] || (UnitLeader[Side] != INDEX_NONE && !Men[UnitLeader[Side]].bAlive))
		{
			bFinished = true;
		}
	}
	if (Elapsed >= Spec.TimeLimit)
	{
		bFinished = bTimedOut = true;
	}
}

FSovBattleResult FSovBattleSim::Result() const
{
	FSovBattleResult R;
	R.bTimedOut = bTimedOut;
	auto Loss = [&](int32 Side) -> int32 {
		const FSovBattleUnitSpec& U = Side == 0 ? Spec.Attacker : Spec.Defender;
		if (U.bLeaderIsUnit)
		{
			const FSovSoldier& L = Men[UnitLeader[Side]];
			return FMath::RoundToInt(U.Hp * (1.f - FMath::Max(0.f, L.Hp) / L.MaxHp));
		}
		return Initial[Side] == 0 ? 0 : FMath::RoundToInt(static_cast<float>(U.Hp) * (Initial[Side] - Alive(Side)) / Initial[Side]);
	};
	R.ToAttacker = Loss(0);
	R.ToDefender = Loss(1);
	if (Leader != INDEX_NONE && !(Spec.HumanSide == 0 ? Spec.Attacker : Spec.Defender).bLeaderIsUnit)
	{
		const FSovSoldier& L = Men[Leader];
		R.LeaderWound = FMath::RoundToInt(Spec.LeaderHp * (1.f - FMath::Max(0.f, L.Hp) / L.MaxHp));
	}
	const int32 A = Alive(0), D = Alive(1);
	R.Winner = A * Initial[1] > D * Initial[0] ? 0 : D * Initial[0] > A * Initial[1] ? 1 : -1;
	return R;
}
