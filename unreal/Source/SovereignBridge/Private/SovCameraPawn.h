// A strategy camera looking down at the map; the player controller moves it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "SovCameraPawn.generated.h"

class UCameraComponent;

UCLASS()
class ASovCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ASovCameraPawn();

	// Puts the camera over a world point at the current height.
	void LookAt(const FVector& Target);
	void Pan(const FVector2D& MapDelta);  // x: east, y: north, in centimetres
	void Zoom(float Steps);               // positive zooms in
	FVector FocusPoint() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	float Height = 2200.f;
	float MinHeight = 500.f;
	float MaxHeight = 9000.f;
	float Pitch = -58.f;

private:
	void Place(const FVector& Focus);
};
