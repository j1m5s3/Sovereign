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

FBlend Blend(float Speed, const FGait& Slow, const FGait& Walk, const FGait& Run, float Band)
{
	FBlend B;
	const float E = FMath::Max(Speed, 0.f);
	const bool bSlow = Slow.Speed > 0.f;
	const bool bRun = Run.Speed > 0.f;
	const float W = FMath::Max(Walk.Speed, 1.f);
	const float Lowest = bSlow ? FMath::Min(Slow.Speed, W) : W;
	// Standing to moving over the first 40% of the slowest gait's speed.
	const float Moving = FMath::Clamp(E / (0.4f * Lowest), 0.f, 1.f);
	B.Idle = 1.f - Moving;
	if (Band > 0.f)
	{
		// Gaits whose footfalls differ: the ones there are, slowest first (each one's weight, speed and loop's length).
		struct FStep
		{
			float* Weight;
			float Speed, Time;
		};
		FStep Steps[3];
		int32 Count = 0;
		if (bSlow) Steps[Count++] = {&B.Slow, Lowest, Slow.Time};
		Steps[Count++] = {&B.Walk, W, Walk.Time};
		if (bRun) Steps[Count++] = {&B.Run, Run.Speed, Run.Time};
		// The first gait whose handover the speed has not passed plays; within that handover's band it shares the
		// figure with the next, whose share rises straight across the band.
		int32 At = 0;
		float Share = 0.f, Low = 0.f, High = 0.f;
		for (; At + 1 < Count; ++At)
		{
			const float Mid = FMath::Sqrt(Steps[At].Speed * Steps[At + 1].Speed);
			Low = Mid * (1.f - Band);
			High = Mid * (1.f + Band);
			if (E < High)
			{
				Share = FMath::Clamp((E - Low) / (High - Low), 0.f, 1.f);
				break;
			}
		}
		const FStep& From = Steps[At];
		if (Share <= 0.f)
		{
			// One gait alone: its own stride, played at the speed asked over its own (floor and ceiling as below).
			*From.Weight = Moving;
			B.StrideSpeed = From.Speed;
			B.StrideTime = From.Time;
			B.Rate = FMath::Clamp(FMath::Max(E, 0.4f * Lowest) / B.StrideSpeed, 0.f, kMaxRate);
			return B;
		}
		// In a band: the rate runs straight from the one gait's at the band's foot to the next one's at its head, and
		// the stride is what that rate covers at the speed asked, so neither jumps where a gait comes in or goes.
		const FStep& To = Steps[At + 1];
		*From.Weight = Moving * (1.f - Share);
		*To.Weight = Moving * Share;
		B.Rate = FMath::Clamp(FMath::Lerp(Low / From.Speed, High / To.Speed, Share), 0.f, kMaxRate);
		B.StrideSpeed = E / FMath::Max(B.Rate, KINDA_SMALL_NUMBER);
		B.StrideTime = FMath::Lerp(From.Time, To.Time, Share);
		return B;
	}
	// Gaits that keep in step: each into the next between the two clips' speeds.
	const float WalkShare = bSlow ? FMath::Clamp((E - Lowest) / FMath::Max(W - Lowest, 1.f), 0.f, 1.f) : 1.f;
	const float RunShare = bRun ? FMath::Clamp((E - W) / FMath::Max(Run.Speed - W, 1.f), 0.f, 1.f) : 0.f;
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

float ActionWeight(float Time, float Length, float In, float Out)
{
	if (Time < 0.f || (Out > 0.f && Time >= Length)) return 0.f;
	// Smoothstep at each end, so a one-shot neither pops in nor out.
	auto Smooth = [](float A) {
		A = FMath::Clamp(A, 0.f, 1.f);
		return A * A * (3.f - 2.f * A);
	};
	const float Rise = In > 0.f ? Smooth(Time / In) : 1.f;
	const float Fall = Out > 0.f ? Smooth((Length - Time) / Out) : 1.f;
	return FMath::Min(Rise, Fall);
}
}  // namespace SovGait

USovLocomotion* USovLocomotion::Attach(USkeletalMeshComponent* Component, const FString& Name, float Scale, bool bActions)
{
	UAnimSequence* Walk = SovArt::RiggedAnim(Name, TEXT("Walk"));
	if (!Component || !Walk) return nullptr;
	Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Component->SetAnimInstanceClass(USovLocomotion::StaticClass());
	USovLocomotion* Loco = Cast<USovLocomotion>(Component->GetAnimInstance());
	if (!Loco) return nullptr;
	Loco->IdleClip = SovArt::RiggedAnim(Name, TEXT("Idle"));
	Loco->RunClip = SovArt::RiggedAnim(Name, TEXT("Run"));
	if (Name.StartsWith(TEXT("Rider")))
	{
		// A horse fills the same slots with its own gaits, each played alone but for a narrow band where the next
		// takes over (kSovHorseHandoverBand), as their footfalls differ: with a Trot, its walk is the slow walk, its
		// trot the walk and its gallop the Run; rigged before the trot (no Trot), its walk and its gallop; rigged
		// before the gallop too (no Run), its walk alone. A gait's speed counts only where its clip was found.
		UAnimSequence* Trot = SovArt::RiggedAnim(Name, TEXT("Trot"));
		Loco->SlowClip = Trot ? Walk : nullptr;
		Loco->WalkClip = Trot ? Trot : Walk;
		Loco->SlowSpeed = kSovHorseWalkSpeed * Scale;
		Loco->WalkSpeed = (Trot ? kSovHorseTrotSpeed : kSovHorseWalkSpeed) * Scale;
		Loco->RunSpeed = kSovHorseRunSpeed * Scale;
		Loco->Band = kSovHorseHandoverBand;
	}
	else
	{
		// A person with a Run (or a WalkSlow) has the motion-captured clips; one without has the procedural walk.
		Loco->SlowClip = SovArt::RiggedAnim(Name, TEXT("WalkSlow"));
		Loco->WalkClip = Walk;
		const bool bMocap = Loco->RunClip || Loco->SlowClip;
		const float Walking = bMocap ? kSovMocapWalkSpeed : kSovProceduralWalkSpeed;
		Loco->SlowSpeed = kSovMocapWalkSlowSpeed * Scale;
		Loco->WalkSpeed = Walking * Scale;
		Loco->RunSpeed = FMath::Max(kSovMocapRunSpeed, Walking * 1.5f) * Scale;
		// No band: a person's gaits keep in step (and a walker that carried a horse before keeps none of its band).
		Loco->Band = 0.f;
	}
	// The one-shots (rig_figures.py, rig_actions.py): only where they are asked for, and each only when it was imported.
	Loco->StrikeClip = bActions ? SovArt::RiggedAnim(Name, TEXT("Strike")) : nullptr;
	Loco->HitClip = bActions ? SovArt::RiggedAnim(Name, TEXT("Hit")) : nullptr;
	Loco->DeathClip = bActions ? SovArt::RiggedAnim(Name, TEXT("Death")) : nullptr;
	Loco->DeathBackClip = bActions ? SovArt::RiggedAnim(Name, TEXT("DeathBack")) : nullptr;
	Loco->StartPhase = FMath::FRand();
	return Loco;
}

void USovLocomotion::Play(ESovAction Kind, float Delay, float Weight, bool bBackward, bool bAtEnd)
{
	if (!Has(Kind)) return;
	FSovActionAsk& Ask = Asks[static_cast<int32>(Kind)];
	++Ask.Serial;
	Ask.Delay = FMath::Max(Delay, 0.f);
	Ask.Weight = FMath::Clamp(Weight, 0.f, 1.f);
	Ask.bBackward = bBackward;
	Ask.bAtEnd = bAtEnd;
	Ask.bAsked = true;
}

void USovLocomotion::Clear()
{
	++ClearSerial;
	for (FSovActionAsk& Ask : Asks) Ask.bAsked = false;
	bFallen = false;
}

bool USovLocomotion::Has(ESovAction Kind) const
{
	switch (Kind)
	{
		case ESovAction::Strike: return StrikeClip != nullptr;
		case ESovAction::Hit: return HitClip != nullptr;
		default: return DeathClip != nullptr || DeathBackClip != nullptr;
	}
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
	Band = Loco->Band;
	Speed = FMath::Max(Loco->Speed, 0.f);
	Easing = Loco->SpeedEasing;
	// The one-shots: a Clear drops them all; then each new ask starts (a death on its fall, backward or forward, as
	// there is a clip for it).
	if (Loco->ClearSerial != ClearSeen)
	{
		ClearSeen = Loco->ClearSerial;
		for (FAction& A : Actions)
		{
			A.Wait = -1.f;
			A.Time = -1.f;
		}
		for (FFlinch& F : Flinches) F = FFlinch();
		bFallen = false;
	}
	for (int32 k = 0; k < 3; ++k)
	{
		const FSovActionAsk& Ask = Loco->Asks[k];
		FAction& A = Actions[k];
		if (Ask.Serial == A.Seen) continue;
		A.Seen = Ask.Serial;
		if (!Ask.bAsked) continue;
		const ESovAction Kind = static_cast<ESovAction>(k);
		const UAnimSequence* Fall = Ask.bBackward && Loco->DeathBackClip ? Loco->DeathBackClip.Get() : Loco->DeathClip ? Loco->DeathClip.Get() : Loco->DeathBackClip.Get();
		const UAnimSequence* Clip = Kind == ESovAction::Strike ? Loco->StrikeClip.Get() : Kind == ESovAction::Hit ? Loco->HitClip.Get() : Fall;
		if (!Clip) continue;
		if (Kind == ESovAction::Hit)
		{
			A.Clip = Clip;
			AddFlinch(Ask.Delay, Ask.Weight);
			continue;
		}
		if (Ask.Delay > 0.f && !Ask.bAtEnd)
		{
			// It starts when its delay is out (Update); until then whatever of it plays plays on, so it never vanishes in
			// a frame. A later ask before then takes this one's place (a strike is asked at once, and a man falls once).
			A.Next = Clip;
			A.Wait = Ask.Delay;
			continue;
		}
		A.Clip = Clip;
		A.bAtEnd = Ask.bAtEnd;
		A.Wait = -1.f;
		A.Time = Ask.bAtEnd ? static_cast<float>(Clip->GetPlayLength()) : 0.f;
	}
	Loco->bFallen = bFallen;
}

void FSovLocomotionProxy::AddFlinch(float Delay, float Weight)
{
	// Each blow flinches him as it lands, added over any flinch still playing, so none is cut off. A blow landing within
	// a jolt's time (kSovHitJolt) of another's joins that one's flinch, as does a blow that comes while MaxFlinches are
	// under way (the nearest in time takes it): one still waiting then starts at the sooner blow, as hard as the harder;
	// one playing plays on as it is.
	int32 Free = INDEX_NONE, Near = INDEX_NONE;
	float NearGap = 0.f;
	for (int32 f = 0; f < MaxFlinches; ++f)
	{
		const FFlinch& F = Flinches[f];
		if (F.Wait < 0.f && F.Time < 0.f)
		{
			if (Free == INDEX_NONE) Free = f;
			continue;
		}
		// How far apart the two blows land (a flinch playing started Time ago).
		const float Gap = FMath::Abs((F.Wait >= 0.f ? F.Wait : -F.Time) - Delay);
		if (Near == INDEX_NONE || Gap < NearGap)
		{
			Near = f;
			NearGap = Gap;
		}
	}
	if (Near != INDEX_NONE && (NearGap < kSovHitJolt || Free == INDEX_NONE))
	{
		FFlinch& F = Flinches[Near];
		if (F.Wait >= 0.f)
		{
			F.Wait = FMath::Min(F.Wait, Delay);
			F.Weight = FMath::Max(F.Weight, Weight);
		}
		return;
	}
	FFlinch& F = Flinches[Free];
	F.Wait = Delay;
	F.Time = -1.f;
	F.Weight = Weight;
}

void FSovLocomotionProxy::Update(float DeltaSeconds)
{
	Eased = FMath::FInterpTo(Eased, Speed, DeltaSeconds, Easing);
	IdleTime += DeltaSeconds;
	auto Gait = [](const UAnimSequence* Clip, float ClipSpeed) {
		return Clip ? SovGait::FGait{ClipSpeed, static_cast<float>(Clip->GetPlayLength())} : SovGait::FGait();
	};
	const SovGait::FBlend B =
		SovGait::Blend(Eased, Gait(Slow, SlowSpeed), Gait(Walk, WalkSpeed), Gait(Run, RunSpeed), Band);
	SlowWeight = B.Slow;
	WalkWeight = B.Walk;
	RunWeight = B.Run;
	// The one-shots: a strike and a death each wait out their delay, then run from the first frame (one already playing
	// runs on while it waits); a strike ends with its clip, a death holds its last frame. Each flinch the same, to its end.
	for (int32 k = 0; k < 3; ++k)
	{
		FAction& A = Actions[k];
		float Ran = DeltaSeconds;
		if (A.Wait >= 0.f)
		{
			A.Wait -= DeltaSeconds;
			if (A.Wait < 0.f)
			{
				Ran = -A.Wait;  // the part of this update after it started
				A.Wait = -1.f;
				A.Clip = A.Next;
				A.bAtEnd = false;
				A.Time = 0.f;
			}
		}
		if (!A.Clip || A.Time < 0.f) continue;
		const float Length = static_cast<float>(A.Clip->GetPlayLength());
		A.Time += Ran;
		if (A.Time > Length)
		{
			A.Time = static_cast<ESovAction>(k) == ESovAction::Death ? Length : -1.f;
		}
	}
	const FAction& Hit = Actions[static_cast<int32>(ESovAction::Hit)];
	const float HitLength = Hit.Clip ? static_cast<float>(Hit.Clip->GetPlayLength()) : 0.f;
	for (FFlinch& F : Flinches)
	{
		float Ran = DeltaSeconds;
		if (F.Wait >= 0.f)
		{
			F.Wait -= DeltaSeconds;
			if (F.Wait >= 0.f) continue;
			Ran = -F.Wait;
			F.Wait = -1.f;
			F.Time = 0.f;
		}
		if (F.Time < 0.f) continue;
		F.Time += Ran;
		if (F.Time > HitLength) F.Time = -1.f;
	}
	if (B.Idle >= 1.f || !Walk) return;
	Phase = FMath::Fmod(Phase + DeltaSeconds * B.Rate / FMath::Max(B.StrideTime, 0.05f), 1.f);
}

namespace
{
// bLoop: a gait wraps round; a one-shot is sampled within its own length.
void SamplePose(const UAnimSequence* Clip, float Time, FPoseContext& Out, bool bLoop = true)
{
	FAnimationPoseData Data(Out);
	Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, {}, bLoop));
}

// Moves each bone of Pose toward Other's by Alpha times its weight (all bones without weights).
void BlendBones(FCompactPose& Pose, const FCompactPose& Other, float Alpha, const TArray<float>* Weights = nullptr)
{
	if (Alpha <= 0.f) return;
	for (FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
	{
		const float W = Weights ? Alpha * (*Weights)[Bone.GetInt()] : Alpha;
		if (W > 0.f) Pose[Bone].BlendWith(Other[Bone], FMath::Min(W, 1.f));
	}
	Pose.NormalizeRotations();
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
		bFallen = false;
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
	EvaluateActions(Output);
	return true;
}

void FSovLocomotionProxy::EvaluateActions(FPoseContext& Output)
{
	auto Length = [](const FAction& A) { return static_cast<float>(A.Clip->GetPlayLength()); };
	// A death goes over everything, eased in and then held; until it is all there, a strike or a flinch it cuts off
	// plays on under it, so the fall takes over from the blow rather than from a stand.
	const FAction& Death = Actions[static_cast<int32>(ESovAction::Death)];
	const bool bDying = Death.Clip && Death.Time >= 0.f;
	const float DeathWeight = !bDying ? 0.f : Death.bAtEnd ? 1.f : SovGait::ActionWeight(Death.Time, Length(Death), kSovDeathIn, 0.f);
	// Fallen once his death's last frame is posed in full: set here, where the pose is made, not in Update, as a rig not
	// rendered may tick its pose without refreshing its bones (a battle rig off camera), so a man killed there lies down
	// in them only once he is seen. PreUpdate reads it back on the game thread, as it does the stride's phase.
	bFallen = bDying && DeathWeight >= 1.f && Death.Time >= Length(Death);
	// A strike over the upper body: the pelvis and the legs are the blend's, so it reads the same standing or walking
	// and no foot slides. The upper body is the bone named spine (people) or rider (horsemen) and every bone below it,
	// found again whenever the bones asked for change (a level of detail); a skeleton with neither plays it on every bone.
	const FAction& Strike = Actions[static_cast<int32>(ESovAction::Strike)];
	if (DeathWeight < 1.f && Strike.Clip && Strike.Time >= 0.f)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		if (!bUpperBuilt || UpperSerial != Bones.GetSerialNumber() || Upper.Num() != Output.Pose.GetNumBones())
		{
			bUpperBuilt = true;
			UpperSerial = Bones.GetSerialNumber();
			Upper.Init(0.f, Output.Pose.GetNumBones());
			const FReferenceSkeleton& Reference = Bones.GetReferenceSkeleton();
			const FName Spine(TEXT("spine")), Rider(TEXT("rider"));
			bool bAny = false;
			for (FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				// Compact bones come parents first, so a parent is marked before its children.
				const FName Name = Reference.GetBoneName(Bones.MakeMeshPoseIndex(Bone).GetInt());
				const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
				const bool bUpper = Name == Spine || Name == Rider || (Parent.IsValid() && Upper[Parent.GetInt()] > 0.f);
				Upper[Bone.GetInt()] = bUpper ? 1.f : 0.f;
				bAny |= bUpper;
			}
			if (!bAny) Upper.Init(1.f, Output.Pose.GetNumBones());
		}
		FPoseContext Swing(this);
		SamplePose(Strike.Clip, FMath::Min(Strike.Time, Length(Strike)), Swing, false);
		BlendBones(Output.Pose, Swing.Pose, SovGait::ActionWeight(Strike.Time, Length(Strike), kSovStrikeIn, kSovStrikeOut), &Upper);
	}
	// The flinches added on top, each the hit's change from its own first frame (the rest pose), so it flinches whatever
	// plays; blows in quick succession add up.
	const FAction& Hit = Actions[static_cast<int32>(ESovAction::Hit)];
	if (DeathWeight < 1.f && Hit.Clip)
	{
		float Weights[MaxFlinches];
		bool bAny = false;
		for (int32 f = 0; f < MaxFlinches; ++f)
		{
			const FFlinch& F = Flinches[f];
			Weights[f] = F.Time >= 0.f ? F.Weight * SovGait::ActionWeight(F.Time, Length(Hit), kSovHitIn, kSovHitOut) : 0.f;
			bAny |= Weights[f] > 0.f;
		}
		if (bAny)
		{
			FPoseContext Rest(this);
			SamplePose(Hit.Clip, 0.f, Rest, false);
			FAnimationPoseData OutData(Output);
			for (int32 f = 0; f < MaxFlinches; ++f)
			{
				if (Weights[f] <= 0.f) continue;
				FPoseContext Flinch(this);
				SamplePose(Hit.Clip, FMath::Min(Flinches[f].Time, Length(Hit)), Flinch, false);
				FAnimationRuntime::ConvertPoseToAdditive(Flinch.Pose, Rest.Pose);
				const FAnimationPoseData FlinchData(Flinch);
				FAnimationRuntime::AccumulateAdditivePose(OutData, FlinchData, Weights[f], AAT_LocalSpaceBase);
			}
			Output.Pose.NormalizeRotations();
		}
	}
	if (bDying)
	{
		FPoseContext Fall(this);
		SamplePose(Death.Clip, FMath::Min(Death.Time, Length(Death)), Fall, false);
		BlendBones(Output.Pose, Fall.Pose, DeathWeight);
	}
}
