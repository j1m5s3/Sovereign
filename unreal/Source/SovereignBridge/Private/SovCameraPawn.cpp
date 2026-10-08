#include "SovCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

#include "SovHexLayout.h"

ASovCameraPawn::ASovCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetFieldOfView(50.f);
}

FVector ASovCameraPawn::FocusPoint() const
{
	// Where the view axis meets the ground plane.
	const double Back = Height / FMath::Tan(FMath::DegreesToRadians(-Pitch));
	const FVector P = GetActorLocation();
	return FVector(P.X + Back, P.Y, 0.0);
}

void ASovCameraPawn::Place(const FVector& Focus)
{
	const double Back = Height / FMath::Tan(FMath::DegreesToRadians(-Pitch));
	// Panning off one side of a wrapping map comes back in from the other; the copies drawn on each side look the same.
	const double Y = WrapWidth > 0.0 ? Focus.Y - WrapWidth * FMath::Floor(Focus.Y / WrapWidth) : Focus.Y;
	SetActorLocation(FVector(Focus.X - Back, Y, Height));
	SetActorRotation(FRotator(Pitch, 0.f, 0.f));
}

void ASovCameraPawn::LookAt(const FVector& Target)
{
	const FVector Near = SovHex::NearestCopy(Target, FocusPoint().Y, WrapWidth);
	Place(FVector(Near.X, Near.Y, 0.0));
}

void ASovCameraPawn::Pan(const FVector2D& MapDelta)
{
	const FVector F = FocusPoint();
	Place(FVector(F.X + MapDelta.Y, F.Y + MapDelta.X, 0.0));
}

void ASovCameraPawn::Zoom(float Steps)
{
	const FVector F = FocusPoint();
	Height = FMath::Clamp(Height * FMath::Pow(0.85f, Steps), MinHeight, MaxHeight);
	Place(F);
}
