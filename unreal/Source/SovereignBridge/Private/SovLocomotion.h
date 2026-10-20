// Idle, walk and run blended by speed (plan G step 4), without an Animation Blueprint: a scene sets Speed each frame and
// the figure eases from standing into a slow walk, from that into a walk and from a walk into a run, played at the rate
// that keeps the feet from sliding. Every gait loop starts as the left foot plants (tools/art/mocap/README.md), so one
// shared stride phase keeps the feet in step through each cross-fade. The clips are a rigged figure's Idle, WalkSlow,
// Walk and Run (WalkSlow and Run when it has them), from tools/art/blender/rig_figures.py and rig_riders.py.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "SovGait.h"
#include "SovLocomotion.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

struct FSovLocomotionProxy : public FAnimInstanceProxy
{
	FSovLocomotionProxy() = default;
	explicit FSovLocomotionProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	const UAnimSequence* Idle = nullptr;
	const UAnimSequence* Slow = nullptr;
	const UAnimSequence* Walk = nullptr;
	const UAnimSequence* Run = nullptr;
	float SlowSpeed = kSovMocapWalkSlowSpeed;  // cm/s the gaits were made at
	float WalkSpeed = kSovMocapWalkSpeed;
	float RunSpeed = kSovMocapRunSpeed;
	float Speed = 0.f;        // asked by the scene
	float Eased = 0.f;        // eased toward it, so a start or a stop blends
	float Easing = 8.f;       // per second
	float IdleTime = 0.f;
	float Phase = 0.f;        // of the stride, 0..1, shared by every gait
	float SlowWeight = 0.f, WalkWeight = 0.f, RunWeight = 0.f;  // the idle has the rest
};

UCLASS(Transient, NotBlueprintable)
class USovLocomotion : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Puts the blend on the component (in place of a single clip) with the rigged figure `Name`'s clips, at their own
	// speeds for a figure of this scale (a child strides shorter). Null when the figure has no Walk.
	static USovLocomotion* Attach(USkeletalMeshComponent* Component, const FString& Name, float Scale = 1.f);

	float Speed = 0.f;  // cm/s, set by the scene each frame
	// How quickly the blend takes up a new speed, per second: a scene whose speed is already smooth (the map's glide)
	// can ask for more, so the legs keep up with the ground.
	float SpeedEasing = 8.f;
	float SlowSpeed = kSovMocapWalkSlowSpeed, WalkSpeed = kSovMocapWalkSpeed, RunSpeed = kSovMocapRunSpeed;
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

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSovLocomotionProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override { delete static_cast<FSovLocomotionProxy*>(Proxy); }
};
