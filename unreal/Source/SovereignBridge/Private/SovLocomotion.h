// Idle, walk and run blended by speed (plan G step 4), without an Animation Blueprint: a scene sets Speed each frame and
// the figure eases from standing into a walk and from a walk into a run, the walk and run kept in step (one shared
// stride phase) and played at the rate that keeps the feet from sliding. The clips are a rigged figure's Idle, Walk and
// (when it has one) Run, from tools/art/blender/rig_figures.py and rig_riders.py.

// The clips' own ground speeds at figure scale 1, cm/s: the motion-captured Walk and Run (tools/art/README.md), the
// procedural walk of a figure rigged before them (no Run), and rig_riders.py's horse.
constexpr float kSovMocapWalkSpeed = 152.5f;
constexpr float kSovMocapRunSpeed = 295.2f;
constexpr float kSovProceduralWalkSpeed = 108.f;
constexpr float kSovHorseWalkSpeed = 154.f;
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
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
	const UAnimSequence* Walk = nullptr;
	const UAnimSequence* Run = nullptr;
	float WalkSpeed = kSovMocapWalkSpeed;  // cm/s the walk and run were made at
	float RunSpeed = kSovMocapRunSpeed;
	float Speed = 0.f;        // asked by the scene
	float Eased = 0.f;        // eased toward it, so a start or a stop blends
	float IdleTime = 0.f;
	float Phase = 0.f;        // of the stride, 0..1, shared by the walk and the run
	float WalkWeight = 0.f, RunWeight = 0.f;  // the idle has the rest
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
	float WalkSpeed = kSovMocapWalkSpeed, RunSpeed = kSovMocapRunSpeed;
	float StartPhase = 0.f;  // not every figure in step

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> RunClip;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSovLocomotionProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override { delete static_cast<FSovLocomotionProxy*>(Proxy); }
};
