// Seat 0's hands: camera movement, and (from milestone 3) selection and orders.
// Keys are polled each tick, so no input assets are needed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "SovPlayerController.generated.h"

class ASovCameraPawn;
class ASovMapActor;
class USovGameSubsystem;

UCLASS()
class ASovPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASovPlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	void SetMap(ASovMapActor* InMap) { Map = InMap; }
	// Centres the camera on the viewer's capital, else their first unit.
	void CenterOnHome();

protected:
	virtual void BeginPlay() override;

	USovGameSubsystem* Subsystem() const;
	ASovCameraPawn* CameraPawn() const;
	void UpdateCamera(float DeltaTime);

	UPROPERTY()
	TObjectPtr<ASovMapActor> Map;

	float PanSpeed = 1.4f;  // fraction of camera height per second
};
