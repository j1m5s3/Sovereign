// The game's sound (plan F): effects, ambience and music synthesised by tools/audio/synth.py and imported to
// /Game/Audio/. Effects play once; one music track and one ambience loop at a time, cross-faded when they change.
// Volumes live in GameUserSettings.ini, [Sovereign.Audio]: Master, Music, Ambience, Effects (0..1).
#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "UObject/StrongObjectPtr.h"

class USoundBase;

enum class ESovVolume : uint8 { Master, Music, Ambience, Effects };

class FSovAudio
{
public:
	// Plays an effect (SFX_Click, SFX_FoundCity...) once; a missing sound is skipped.
	static void Play(UObject* WorldContext, const TCHAR* Name);

	// The music track (MUS_Ancient...) and ambience loop (AMB_Country...) to hear now; empty stops it. Changing one
	// fades the old out and the new in; asking for the one already playing does nothing.
	void SetMusic(UObject* WorldContext, const FString& Name);
	void SetAmbience(UObject* WorldContext, const FString& Name);
	// After a volume change: the loops follow at once.
	void ApplyVolumes();
	void Stop();

	static float Volume(ESovVolume Which);
	static void SetVolume(ESovVolume Which, float Value);

private:
	struct FLoop
	{
		FString Name;
		TStrongObjectPtr<UAudioComponent> Component;
	};
	void Switch(UObject* WorldContext, FLoop& Loop, const FString& Name, ESovVolume Channel);
	FLoop Music, Ambience;
};
