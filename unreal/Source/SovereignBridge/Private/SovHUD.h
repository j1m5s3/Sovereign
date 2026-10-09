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
	// With a city of ours selected, its plots' yields show anyway (and only its own); -1: none.
	int32 YieldCity = -1;
	// The selected unit's path to the plot under the cursor: plots, and the turn each is reached (0: this turn).
	TArray<FIntPoint> PathPlots;
	TArray<int32> PathTurns;
	bool bShowHelp = false;
	// F4: the chronicle of the viewer's reign so far (player-retention §2).
	bool bShowChronicle = false;
	// Room the game screen's widgets take (plan D): the top bar, and the unit panel at the bottom left.
	float TopInset = 0.f, BottomInset = 0.f;

protected:
	void DrawStatus(const USovGameSubsystem& Sub, float& Y);
	void DrawLabels(const USovGameSubsystem& Sub);
	void DrawPath();
	void DrawYields(const USovGameSubsystem& Sub);
	void DrawHelp();
	void DrawChronicle(const USovGameSubsystem& Sub);
	void DrawStreet(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void DrawBattle(const USovGameSubsystem& Sub, const ASovPlayerController& PC);
	void Line(const FString& Text, float X, float& Y, const FLinearColor& Color = FLinearColor::White);
	// A UI icon (unreal/Content/Slate/Icons) as a texture for the canvas, loaded once.
	class UTexture2D* IconTexture(FName Name);

	UPROPERTY()
	TMap<FName, TObjectPtr<class UTexture2D>> Icons;
};
