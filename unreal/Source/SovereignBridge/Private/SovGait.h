// The walking blend's arithmetic and the map's walking pace (plan G step 4), kept apart from the engine's animation
// classes so the tests can check them: USovLocomotion plays the blend, ASovMapActor paces its rigged walkers. Defined in
// SovLocomotion.cpp.
#pragma once

#include "CoreMinimal.h"

// The clips' own ground speeds at figure scale 1, cm/s: the motion-captured slow walk, walk and run (tools/art/README.md),
// the procedural walk of a figure rigged before them (no Run or WalkSlow), and rig_riders.py's horse.
constexpr float kSovMocapWalkSlowSpeed = 100.4f;
constexpr float kSovMocapWalkSpeed = 152.5f;
constexpr float kSovMocapRunSpeed = 295.2f;
constexpr float kSovProceduralWalkSpeed = 108.f;
constexpr float kSovHorseWalkSpeed = 154.f;

namespace SovGait
{
// The most times its own pace a gait is played: past it the feet slide rather than blur.
constexpr float kMaxRate = 3.f;

// One gait loop: its ground speed (cm/s) and length (s); Speed 0 when the figure has no such clip.
struct FGait
{
	float Speed = 0.f;
	float Time = 0.f;
};

// The blend at an (eased) speed: each clip's weight (they sum to 1), the stride the two clips blending make between them
// (its speed and its loop's length) and the times its own pace that stride is played at.
struct FBlend
{
	float Idle = 1.f, Slow = 0.f, Walk = 0.f, Run = 0.f;
	float StrideSpeed = 0.f, StrideTime = 0.f;
	float Rate = 0.f;
};
FBlend Blend(float Speed, const FGait& Slow, const FGait& Walk, const FGait& Run);

// A rigged unit's walk on the map: its length (s) and the share of it spent speeding up (and again slowing down).
struct FGlidePace
{
	float Duration = 0.f;
	float Ramp = 0.f;
};
// For Dist (world cm) by a figure of world Scale whose clips walk at WalkSpeed and run at RunSpeed (cm/s at scale 1;
// RunSpeed 0 without a Run).
FGlidePace RiggedGlide(float Dist, float Scale, float WalkSpeed, float RunSpeed);
// The share of the way covered at A (0..1) of a glide: with Ramp, speeding up over that share of it, a steady pace,
// then slowing over the same share; with Ramp 0, the smoothstep (the still figures' glide). EaseRate is its slope: the
// speed as a multiple of the glide's average.
float Ease(float A, float Ramp);
float EaseRate(float A, float Ramp);
}  // namespace SovGait
