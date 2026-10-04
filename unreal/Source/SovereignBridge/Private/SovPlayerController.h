// Seat 0's hands: camera, selection and orders. Every order leaves here as a
// sov::Command through USovGameSubsystem::Submit; nothing else changes the game.
// Keys are polled each tick, so no input assets are needed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "sovereign/commands.h"

#include "SovPlayerController.generated.h"

class ASovCameraPawn;
class ASovHUD;
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

	enum class EChooser : uint8
	{
		None,
		Production,
		Research,
		Civic,
		Improvement
	};

	struct FChoice
	{
		FString Label;
		sov::Command Command;
	};

	USovGameSubsystem* Subsystem() const;
	ASovCameraPawn* CameraPawn() const;
	bool MyTurn() const;
	sov::PlayerId Me() const;

	void UpdateCamera(float DeltaTime);
	void HandleOrders();
	bool HexUnderCursor(int32& OutX, int32& OutY) const;
	void ClickSelect(int32 X, int32 Y);
	void ClickOrder(int32 X, int32 Y);
	void SelectUnit(int32 Id, bool bCenter);
	void SelectCity(int32 Id, bool bCenter);
	void SelectNextUnit();
	void EndTurn();
	// Sends the command; true when the core accepted it.
	bool Send(const sov::Command& Command);
	void AfterUnitOrder();

	void OpenChooser(EChooser Kind);
	void Pick(int32 Index);
	void UpdatePanel();

	UPROPERTY()
	TObjectPtr<ASovMapActor> Map;

	int32 SelectedUnit = -1;
	int32 SelectedCity = -1;
	EChooser Chooser = EChooser::None;
	TArray<FChoice> Choices;
	int32 ChooserPage = 0;
	FString ChooserTitle;
	bool bWasMyTurn = false;

	float PanSpeed = 1.4f;  // fraction of camera height per second
};
