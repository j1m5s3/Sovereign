#include "SovLocomotion.h"
#include "SovArt.h"

#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"

namespace SovGait
{
namespace
{
// A map walk's steady pace, at most so many times its fastest gait's own (the play rate that still holds the feet): a
// person's motion-captured run or a horse's gallop, else the walk (the procedural walk, a horse rigged before the
// gallop). A short speed-up and slow-down either side of it, and a long move kept to a few seconds (its feet then hurry).
constexpr float kRunCadence = 1.6f;
constexpr float kWalkCadence = 2.5f;
constexpr float kRampSeconds = 0.25f;
constexpr float kLongestGlide = 3.f;
}  // namespace

FBlend Blend(float Speed, const FGait& Slow, const FGait& Walk, const FGait& Run, float RunBy)
{
	FBlend B;
	const float E = FMath::Max(Speed, 0.f);
	const bool bSlow = Slow.Speed > 0.f;
	const bool bRun = Run.Speed > 0.f;
	const float W = FMath::Max(Walk.Speed, 1.f);
	const float Lowest = bSlow ? FMath::Min(Slow.Speed, W) : W;
	// Standing to moving over the first 40% of the slowest gait's speed; then each gait into the next between the two
	// clips' speeds, or the walk into the run by RunBy (past it the run alone, its stride its own).
	const float Moving = FMath::Clamp(E / (0.4f * Lowest), 0.f, 1.f);
	const float WalkShare = bSlow ? FMath::Clamp((E - Lowest) / FMath::Max(W - Lowest, 1.f), 0.f, 1.f) : 1.f;
	const float RunFull = RunBy > W ? FMath::Min(RunBy, Run.Speed) : Run.Speed;
	const float RunShare = bRun ? FMath::Clamp((E - W) / FMath::Max(RunFull - W, 1.f), 0.f, 1.f) : 0.f;
	B.Idle = 1.f - Moving;
	B.Slow = Moving * (1.f - WalkShare);
	B.Walk = Moving * WalkShare * (1.f - RunShare);
	B.Run = Moving * RunShare;
	// The stride the two clips blending make between them: its ground speed and its loop's length.
	if (RunShare > 0.f)
	{
		B.StrideSpeed = FMath::Lerp(W, Run.Speed, RunShare);
		B.StrideTime = FMath::Lerp(Walk.Time, Run.Time, RunShare);
	}
	else if (bSlow)
	{
		B.StrideSpeed = FMath::Lerp(Lowest, W, WalkShare);
		B.StrideTime = FMath::Lerp(Slow.Time, Walk.Time, WalkShare);
	}
	else
	{
		B.StrideSpeed = W;
		B.StrideTime = Walk.Time;
	}
	// One stride covers the ground its clips were made for: played at the speed asked over the stride's speed, the
	// planted foot holds (with a floor, so a shuffle still steps, and a ceiling past a full run).
	B.Rate = FMath::Clamp(FMath::Max(E, 0.4f * Lowest) / B.StrideSpeed, 0.f, kMaxRate);
	return B;
}

FGlidePace RiggedGlide(float Dist, float Scale, float WalkSpeed, float RunSpeed)
{
	const float Top = (RunSpeed > 0.f ? kRunCadence * RunSpeed : kWalkCadence * WalkSpeed) * FMath::Max(Scale, 0.01f);
	FGlidePace P;
	// The steady pace covers the way less half of each ramp's: Dist = Top * (Duration - kRampSeconds).
	P.Duration = FMath::Min(FMath::Max(Dist, 0.f) / FMath::Max(Top, 1.f) + kRampSeconds, kLongestGlide);
	P.Ramp = FMath::Min(kRampSeconds / P.Duration, 0.5f);
	return P;
}

float Ease(float A, float Ramp)
{
	A = FMath::Clamp(A, 0.f, 1.f);
	if (Ramp <= 0.f) return A * A * (3.f - 2.f * A);
	const float R = FMath::Min(Ramp, 0.5f);
	const float K = 1.f / (2.f * R * (1.f - R));
	if (A < R) return K * A * A;
	if (A > 1.f - R) return 1.f - K * (1.f - A) * (1.f - A);
	return (A - 0.5f * R) / (1.f - R);
}

float EaseRate(float A, float Ramp)
{
	A = FMath::Clamp(A, 0.f, 1.f);
	if (Ramp <= 0.f) return 6.f * A * (1.f - A);
	const float R = FMath::Min(Ramp, 0.5f);
	return FMath::Min(1.f, FMath::Min(A, 1.f - A) / R) / (1.f - R);
}
}  // namespace SovGait

USovLocomotion* USovLocomotion::Attach(USkeletalMeshComponent* Component, const FString& Name, float Scale)
{
	UAnimSequence* Walk = SovArt::RiggedAnim(Name, TEXT("Walk"));
	if (!Component || !Walk) return nullptr;
	Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Component->SetAnimInstanceClass(USovLocomotion::StaticClass());
	USovLocomotion* Loco = Cast<USovLocomotion>(Component->GetAnimInstance());
	if (!Loco) return nullptr;
	Loco->IdleClip = SovArt::RiggedAnim(Name, TEXT("Idle"));
	Loco->SlowClip = SovArt::RiggedAnim(Name, TEXT("WalkSlow"));
	Loco->WalkClip = Walk;
	Loco->RunClip = SovArt::RiggedAnim(Name, TEXT("Run"));
	// A person with a Run (or a WalkSlow) has the motion-captured clips; one without has the procedural walk. A horse
	// has its own walk, and its gallop for a Run, handed over just past the walk (kSovHorseHandover). The run speed
	// counts only where a Run was found, so a horse rigged before the gallop keeps to its walk.
	const bool bHorse = Name.StartsWith(TEXT("Rider"));
	const bool bMocap = Loco->RunClip || Loco->SlowClip;
	const float Walking = bHorse ? kSovHorseWalkSpeed : bMocap ? kSovMocapWalkSpeed : kSovProceduralWalkSpeed;
	Loco->SlowSpeed = kSovMocapWalkSlowSpeed * Scale;
	Loco->WalkSpeed = Walking * Scale;
	Loco->RunSpeed = (bHorse ? kSovHorseRunSpeed : FMath::Max(kSovMocapRunSpeed, Walking * 1.5f)) * Scale;
	Loco->RunBy = bHorse ? kSovHorseHandover * Loco->WalkSpeed : 0.f;
	Loco->StartPhase = FMath::FRand();
	return Loco;
}

void FSovLocomotionProxy::PreUpdate(UAnimInstance* Instance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
	USovLocomotion* Loco = CastChecked<USovLocomotion>(Instance);
	if (!Walk && Loco->WalkClip)
	{
		// First update: start each figure at its own place in the clips.
		Phase = Loco->StartPhase;
		IdleTime = Loco->StartPhase * 3.f;
		Eased = Loco->Speed;
	}
	Loco->StridePhase = Phase;
	Idle = Loco->IdleClip;
	Slow = Loco->SlowClip;
	Walk = Loco->WalkClip;
	Run = Loco->RunClip;
	SlowSpeed = Loco->SlowSpeed;
	WalkSpeed = FMath::Max(Loco->WalkSpeed, 1.f);
	RunSpeed = Loco->RunSpeed;
	RunBy = Loco->RunBy;
	Speed = FMath::Max(Loco->Speed, 0.f);
	Easing = Loco->SpeedEasing;
}

void FSovLocomotionProxy::Update(float DeltaSeconds)
{
	Eased = FMath::FInterpTo(Eased, Speed, DeltaSeconds, Easing);
	IdleTime += DeltaSeconds;
	auto Gait = [](const UAnimSequence* Clip, float ClipSpeed) {
		return Clip ? SovGait::FGait{ClipSpeed, static_cast<float>(Clip->GetPlayLength())} : SovGait::FGait();
	};
	const SovGait::FBlend B = SovGait::Blend(Eased, Gait(Slow, SlowSpeed), Gait(Walk, WalkSpeed), Gait(Run, RunSpeed), RunBy);
	SlowWeight = B.Slow;
	WalkWeight = B.Walk;
	RunWeight = B.Run;
	if (B.Idle >= 1.f || !Walk) return;
	Phase = FMath::Fmod(Phase + DeltaSeconds * B.Rate / FMath::Max(B.StrideTime, 0.05f), 1.f);
}

namespace
{
void SamplePose(const UAnimSequence* Clip, float Time, FPoseContext& Out)
{
	FAnimationPoseData Data(Out);
	Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, {}, true));
}
}  // namespace

bool FSovLocomotionProxy::Evaluate(FPoseContext& Output)
{
	struct FPart
	{
		const UAnimSequence* Clip;
		float Time;
		float Weight;
	};
	TArray<FPart, TInlineAllocator<4>> Parts;
	float SlowShare = Walk && Slow ? SlowWeight : 0.f;
	float WalkShare = Walk ? WalkWeight : 0.f;
	const float RunShare = Walk && Run ? RunWeight : 0.f;
	const float Moving = SlowShare + WalkShare + RunShare;
	// Without an idle the slowest gait stands in for it (held still: the phase only runs while moving).
	if (!Idle && Walk)
	{
		if (Slow) SlowShare += 1.f - Moving;
		else WalkShare += 1.f - Moving;
	}
	if (Idle && Moving < 0.999f) Parts.Add({Idle, FMath::Fmod(IdleTime, FMath::Max(Idle->GetPlayLength(), 0.05f)), 1.f - Moving});
	if (SlowShare > 0.001f) Parts.Add({Slow, Phase * Slow->GetPlayLength(), SlowShare});
	if (WalkShare > 0.001f) Parts.Add({Walk, Phase * Walk->GetPlayLength(), WalkShare});
	if (RunShare > 0.001f) Parts.Add({Run, Phase * Run->GetPlayLength(), RunShare});
	if (Parts.Num() == 0)
	{
		Output.ResetToRefPose();
		return true;
	}
	// The parts summed in turn: each blend keeps what was summed so far at its share of the weight seen.
	SamplePose(Parts[0].Clip, Parts[0].Time, Output);
	float Seen = Parts[0].Weight;
	for (int32 i = 1; i < Parts.Num(); ++i)
	{
		FPoseContext Other(this);
		SamplePose(Parts[i].Clip, Parts[i].Time, Other);
		const float Before = Seen;
		Seen += Parts[i].Weight;
		if (Seen <= KINDA_SMALL_NUMBER) continue;
		FAnimationPoseData OutData(Output);
		const FAnimationPoseData OtherData(Other);
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, OtherData, Before / Seen);
	}
	return true;
}
