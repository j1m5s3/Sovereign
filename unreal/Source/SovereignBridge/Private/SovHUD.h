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
	// F3: what each plot of the viewer's territory yields its city (* worked). F1: the keys.
	bool bShowYields = false;
	bool bShowHelp = false;
	// F4: the chronicle of the viewer's reign so far (player-retention §2).
	bool bShowChronicle = false;

protected:
	void DrawStatus(const USovGameSubsystem& Sub, float& Y);
	void DrawLabels(const USovGameSubsystem& Sub);
	void DrawYields(const USovGameSubsystem& Sub);
	void DrawHelp();
	void DrawChronicle(const USovGameSubsystem& Sub);
	void DrawStreet(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void DrawBattle(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void Line(const FString& Text, float X, float& Y, const FLinearColor& Color = FLinearColor::White);
};
