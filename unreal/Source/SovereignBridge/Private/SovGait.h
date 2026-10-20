// The walking blend's arithmetic and the map's walking pace (plan G step 4), kept apart from the engine's animation
// classes so the tests can check them: USovLocomotion plays the blend, ASovMapActor paces its rigged walkers. Defined in
// SovLocomotion.cpp.
#pragma once

#include "CoreMinimal.h"

// The clips' own ground speeds at figure scale 1, cm/s: the motion-captured slow walk, walk and run (tools/art/README.md),
// the procedural walk of a figure rigged before them (no Run or WalkSlow), and rig_riders.py's horse: its walk, its
// trot and its gallop, which is a horseman's Run (one rigged before the trot has the walk and the gallop, one rigged
// before the gallop only the walk). rig_riders.py prints the last three.
constexpr float kSovMocapWalkSlowSpeed = 100.4f;
constexpr float kSovMocapWalkSpeed = 152.5f;
constexpr float kSovMocapRunSpeed = 295.2f;
constexpr float kSovProceduralWalkSpeed = 108.f;
constexpr float kSovHorseWalkSpeed = 92.3f;
constexpr float kSovHorseTrotSpeed = 264.f;
constexpr float kSovHorseRunSpeed = 457.1f;
// A horse's walk, trot and gallop land only the left hind together (the walk and the trot the right hind too, not the
// fores; rig_riders.py), so two of them mixed at one phase slide the hooves: each plays alone, slowed or hurried to the
// speed asked (a single clip holds its hooves at any rate), and hands over to the next only within this share either
// side of the speed between theirs (SovGait::Blend's Band). A battle's 208 to 299 cm/s is the trot alone, at 0.79 to
// 1.13 times its own pace.
constexpr float kSovHorseHandoverBand = 0.1f;

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
// With Band 0 each gait hands over to the next across the whole way from its own speed to the next one's (people,
// whose gaits keep in step at one phase). With a Band, for gaits whose footfalls differ (a horse:
// kSovHorseHandoverBand), each gait plays alone, at the speed asked over its own, but for a cross-fade to the next
// within Band (a share) either side of the speed between the two (their geometric mean), through which the stride's
// speed, its loop's length and the rate run on without a jump. Either way, below the slowest gait's speed it is that
// gait alone, the idle fading in to standing.
FBlend Blend(float Speed, const FGait& Slow, const FGait& Walk, const FGait& Run, float Band = 0.f);

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
