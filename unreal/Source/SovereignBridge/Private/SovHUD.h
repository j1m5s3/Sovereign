// Canvas HUD: turn and empire numbers, city labels, unit health, messages.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "SovHUD.generated.h"

class ASovPlayerController;
class USovGameSubsystem;

UCLASS()
class ASovHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// Extra lines the player controller wants shown (selection, chooser), drawn bottom-left.
	TArray<FString> PanelLines;

protected:
	void DrawStatus(const USovGameSubsystem& Sub, float& Y);
	void DrawLabels(const USovGameSubsystem& Sub);
	void DrawStreet(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void DrawBattle(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void Line(const FString& Text, float X, float& Y, const FLinearColor& Color = FLinearColor::White);
};
