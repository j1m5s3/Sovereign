// The settings screen (plan D, step 5): graphics quality, window mode, resolution, vsync, frame limit and
// interface scale, kept in GameUserSettings, and the controls for reference. There is no sound yet, and the
// keys are fixed (no rebinding yet).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SSovSettingsScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSovSettingsScreen) {}
	SLATE_EVENT(TDelegate<void()>, OnBack)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	// The interface scale saved last time, put back at start-up.
	static void ApplySavedInterfaceScale();

private:
	TSharedRef<SWidget> Option(const FString& Label, TFunction<FString()> Value, TFunction<void(int32)> Step);
	void Apply();

	int32 Quality = 3;      // 0 Low .. 4 Cinematic
	int32 WindowMode = 0;   // EWindowMode: fullscreen, windowed fullscreen, windowed
	TArray<FIntPoint> Resolutions;
	int32 Resolution = 0;
	bool bVSync = false;
	int32 FrameLimit = 0;   // index into the limits list
	int32 Scale = 10;       // interface scale in tenths
	FString Status;
	TDelegate<void()> OnBack;
};
