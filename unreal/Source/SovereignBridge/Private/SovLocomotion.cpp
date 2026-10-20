#include "SovLocomotion.h"
#include "SovArt.h"

#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"

USovLocomotion* USovLocomotion::Attach(USkeletalMeshComponent* Component, const FString& Name, float Scale)
{
	UAnimSequence* Walk = SovArt::RiggedAnim(Name, TEXT("Walk"));
	if (!Component || !Walk) return nullptr;
	Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Component->SetAnimInstanceClass(USovLocomotion::StaticClass());
	USovLocomotion* Loco = Cast<USovLocomotion>(Component->GetAnimInstance());
	if (!Loco) return nullptr;
	Loco->IdleClip = SovArt::RiggedAnim(Name, TEXT("Idle"));
	Loco->WalkClip = Walk;
	Loco->RunClip = SovArt::RiggedAnim(Name, TEXT("Run"));
	// A figure with a Run has the motion-captured clips; one without has the procedural walk (or is a horse).
	const float Walking = Name.StartsWith(TEXT("Rider")) ? kSovHorseWalkSpeed : Loco->RunClip ? kSovMocapWalkSpeed : kSovProceduralWalkSpeed;
	Loco->WalkSpeed = Walking * Scale;
	Loco->RunSpeed = FMath::Max(kSovMocapRunSpeed, Walking * 1.5f) * Scale;
	Loco->StartPhase = FMath::FRand();
	return Loco;
}

void FSovLocomotionProxy::PreUpdate(UAnimInstance* Instance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
	const USovLocomotion* Loco = CastChecked<USovLocomotion>(Instance);
	if (!Walk && Loco->WalkClip)
	{
		// First update: start each figure at its own place in the clips.
		Phase = Loco->StartPhase;
		IdleTime = Loco->StartPhase * 3.f;
		Eased = Loco->Speed;
	}
	Idle = Loco->IdleClip;
	Walk = Loco->WalkClip;
	Run = Loco->RunClip;
	WalkSpeed = FMath::Max(Loco->WalkSpeed, 1.f);
	RunSpeed = Loco->RunSpeed;
	Speed = FMath::Max(Loco->Speed, 0.f);
}

void FSovLocomotionProxy::Update(float DeltaSeconds)
{
	Eased = FMath::FInterpTo(Eased, Speed, DeltaSeconds, 8.f);
	IdleTime += DeltaSeconds;
	// Standing to moving over the first 40% of the walk's speed; walking to running between the two clips' speeds.
	const float Moving = FMath::Clamp(Eased / (0.4f * WalkSpeed), 0.f, 1.f);
	const float RunShare = Run ? FMath::Clamp((Eased - WalkSpeed) / FMath::Max(RunSpeed - WalkSpeed, 1.f), 0.f, 1.f) : 0.f;
	WalkWeight = Moving * (1.f - RunShare);
	RunWeight = Moving * RunShare;
	if (Moving <= 0.f || !Walk) return;
	// One stride covers the ground its clip was made for: the phase runs at the speed asked over the blended stride's
	// speed, so the planted foot holds (with a floor, so a shuffle still steps, and a ceiling past a full run).
	const float StrideSpeed = FMath::Lerp(WalkSpeed, RunSpeed, RunShare);
	const float StrideTime = FMath::Lerp(Walk->GetPlayLength(), Run ? Run->GetPlayLength() : Walk->GetPlayLength(), RunShare);
	const float Rate = FMath::Clamp(FMath::Max(Eased, 0.4f * WalkSpeed) / StrideSpeed, 0.f, 2.5f);
	Phase = FMath::Fmod(Phase + DeltaSeconds * Rate / FMath::Max(StrideTime, 0.05f), 1.f);
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
	TArray<FPart, TInlineAllocator<3>> Parts;
	const float Moving = Walk ? WalkWeight + RunWeight : 0.f;
	if (Idle && Moving < 0.999f) Parts.Add({Idle, FMath::Fmod(IdleTime, FMath::Max(Idle->GetPlayLength(), 0.05f)), 1.f - Moving});
	if (Walk && (WalkWeight > 0.001f || !Idle)) Parts.Add({Walk, Phase * Walk->GetPlayLength(), Idle ? WalkWeight : 1.f - RunWeight});
	if (Run && RunWeight > 0.001f) Parts.Add({Run, Phase * Run->GetPlayLength(), RunWeight});
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
