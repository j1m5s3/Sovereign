// Idle, walk and run blended by speed (plan G step 4), without an Animation Blueprint: a scene sets Speed each frame and
// the figure eases from standing into a slow walk, from that into a walk and from a walk into a run, played at the rate
// that keeps the feet from sliding. Every gait loop starts as the left foot plants (tools/art/mocap/README.md), so one
// shared stride phase keeps the feet in step through each cross-fade. A horse's walk, trot and gallop (in the slow
// walk's, walk's and run's places) share only that first footfall (the walk and the trot the right hind too, not the
// fores), so a horse plays each alone, slowed or hurried, and hands it to the next only in a narrow band of speed
// between the two (kSovHorseHandoverBand). The clips are a rigged figure's Idle, WalkSlow, Walk and Run (WalkSlow and
// Run when it has them), from tools/art/blender/rig_figures.py, or a horseman's Idle, Walk, Trot and Run (Trot and Run
// when it has them) from rig_riders.py.
//
// Over the blend it plays one-shots (plan G step 5) for battles and the battle's ruler: a Strike over the upper body (the
// bone named spine, or a horseman's rider, and everything below it), so the legs keep walking or standing and no foot
// slides; a Hit added over whatever plays (its change from its own first frame), a flinch for each blow, which add up
// rather than cut each other off; and a Death (or DeathBack) over the whole body, held at its last frame. Each waits its
// own delay, counted in the animation, so a blow can land after the scene stops asking. A figure without a clip simply
// plays nothing for it.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "SovGait.h"
#include "SovLocomotion.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

// The one-shots, in the order of USovLocomotion::Asks.
enum class ESovAction : uint8 { Strike, Hit, Death };

// A one-shot asked for on the game thread (USovLocomotion::Play): the proxy starts it when its Serial changes, if it is
// still asked (a Clear since withdraws it).
struct FSovActionAsk
{
	int32 Serial = 0;
	float Delay = 0.f;
	float Weight = 1.f;
	bool bBackward = false;
	bool bAtEnd = false;
	bool bAsked = false;
};

struct FSovLocomotionProxy : public FAnimInstanceProxy
{
	FSovLocomotionProxy() = default;
	explicit FSovLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	// The one-shots over the walking blend in Output.
	void EvaluateActions(FPoseContext& Output);
	// A flinch for a blow landing Delay seconds from now (SovLocomotion.cpp).
	void AddFlinch(float Delay, float Weight);

	const UAnimSequence* Idle = nullptr;
	const UAnimSequence* Slow = nullptr;
	const UAnimSequence* Walk = nullptr;
	const UAnimSequence* Run = nullptr;
	float SlowSpeed = kSovMocapWalkSlowSpeed;  // cm/s the gaits were made at
	float WalkSpeed = kSovMocapWalkSpeed;
	float RunSpeed = kSovMocapRunSpeed;
	float Band = 0.f;         // a horse: the band its gaits hand over in (SovGait::Blend)
	float Speed = 0.f;        // asked by the scene
	float Eased = 0.f;        // eased toward it, so a start or a stop blends
	float Easing = 8.f;       // per second
	float IdleTime = 0.f;
	float Phase = 0.f;        // of the stride, 0..1, shared by every gait
	float SlowWeight = 0.f, WalkWeight = 0.f, RunWeight = 0.f;  // the idle has the rest

	// The one-shots (ESovAction order): each one's clip, the ask it last took up, the seconds before the next starts
	// (below 0: none waiting) and into this one (below 0: not playing); the one waiting and its clip. A hit's times are
	// its flinches' instead.
	struct FAction
	{
		const UAnimSequence* Clip = nullptr;
		int32 Seen = 0;
		float Wait = -1.f;
		float Time = -1.f;
		bool bAtEnd = false;  // a death that lies already (no fall)
		const UAnimSequence* Next = nullptr;
	};
	FAction Actions[3];
	// The flinches, one for each blow (a few at once): the seconds before each starts (below 0: started) and into it
	// (below 0: not playing), and how hard.
	struct FFlinch
	{
		float Wait = -1.f;
		float Time = -1.f;
		float Weight = 1.f;
	};
	static constexpr int32 MaxFlinches = 3;
	FFlinch Flinches[MaxFlinches];
	int32 ClearSeen = 0;
	bool bFallen = false;  // the death's last frame posed in full (EvaluateActions): the bones hold him lying
	// Per compact bone, 1 where a strike plays (the upper body), for the bone container it was built from.
	TArray<float> Upper;
	uint16 UpperSerial = 0;
	bool bUpperBuilt = false;
};

UCLASS(Transient, NotBlueprintable)
class USovLocomotion : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Puts the blend on the component (in place of a single clip) with the rigged figure `Name`'s clips, at their own
	// speeds for a figure of this scale (a child strides shorter). Null when the figure has no Walk. bActions: also load
	// the one-shots (Strike, Hit, Death, DeathBack), for battles and the battle's ruler; the map and the street's
	// townsfolk leave them out.
	static USovLocomotion* Attach(USkeletalMeshComponent* Component, const FString& Name, float Scale = 1.f, bool bActions = false);

	// Plays a one-shot after Delay seconds: a strike on the upper body (the legs keep walking or standing), a hit added
	// over whatever plays (Weight: how hard, 0..1), a death over all of it, held at its last frame (bBackward: the
	// DeathBack when there is one; bAtEnd: lying already). A strike or a death asked again starts over, but one asked with
	// a delay waits and takes the place only of another still waiting; a hit asked again flinches him again over any
	// flinch still playing (at most three at once), and blows that land together flinch him once. Nothing without its
	// clip.
	void Play(ESovAction Kind, float Delay = 0.f, float Weight = 1.f, bool bBackward = false, bool bAtEnd = false);
	// Stops them all (a man up again).
	void Clear();
	// Its clip exists (a death: either fall).
	bool Has(ESovAction Kind) const;
	// Asked since the last Clear.
	bool Asked(ESovAction Kind) const { return Asks[static_cast<int32>(Kind)].bAsked; }
	// A death's last frame has been posed in full (read back each update, as StridePhase): he lies still in his bones,
	// which need no more refreshing. A rig whose bones are not refreshed (off camera) is not Fallen until it is seen.
	bool Fallen() const { return bFallen; }

	float Speed = 0.f;  // cm/s, set by the scene each frame
	// How quickly the blend takes up a new speed, per second: a scene whose speed is already smooth (the map's glide)
	// can ask for more, so the legs keep up with the ground.
	float SpeedEasing = 8.f;
	float SlowSpeed = kSovMocapWalkSlowSpeed, WalkSpeed = kSovMocapWalkSpeed, RunSpeed = kSovMocapRunSpeed;
	float Band = 0.f;         // a horse: its gaits hand over only within this band (SovGait::Blend); 0 for people
	float StartPhase = 0.f;   // not every figure in step
	float StridePhase = 0.f;  // where the stride had got to (read back each update, so a rebuilt figure can carry on)

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> SlowClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> RunClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> StrikeClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> HitClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> DeathClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> DeathBackClip;

	// The game thread's asks, one per ESovAction, and its Clears: the proxy takes each up on its next update.
	FSovActionAsk Asks[3];
	int32 ClearSerial = 0;
	bool bFallen = false;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSovLocomotionProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override { delete static_cast<FSovLocomotionProxy*>(Proxy); }
};
