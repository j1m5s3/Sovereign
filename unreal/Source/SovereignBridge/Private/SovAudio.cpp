#include "SovAudio.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"

namespace
{
const TCHAR* const kSection = TEXT("Sovereign.Audio");

const TCHAR* Key(ESovVolume Which)
{
	switch (Which)
	{
		case ESovVolume::Music: return TEXT("Music");
		case ESovVolume::Ambience: return TEXT("Ambience");
		case ESovVolume::Effects: return TEXT("Effects");
		default: return TEXT("Master");
	}
}

float Default(ESovVolume Which)
{
	switch (Which)
	{
		case ESovVolume::Music: return 0.55f;
		case ESovVolume::Ambience: return 0.5f;
		case ESovVolume::Effects: return 0.85f;
		default: return 1.f;
	}
}

USoundBase* Load(const FString& Name)
{
	static TMap<FString, TStrongObjectPtr<USoundBase>> Cache;
	if (const TStrongObjectPtr<USoundBase>* Found = Cache.Find(Name)) return Found->Get();
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Cache.Add(Name, TStrongObjectPtr<USoundBase>(Sound));  // a missing sound is remembered as null
	return Sound;
}
}  // namespace

float FSovAudio::Volume(ESovVolume Which)
{
	float V = Default(Which);
	GConfig->GetFloat(kSection, Key(Which), V, GGameUserSettingsIni);
	return FMath::Clamp(V, 0.f, 1.f);
}

void FSovAudio::SetVolume(ESovVolume Which, float Value)
{
	GConfig->SetFloat(kSection, Key(Which), FMath::Clamp(Value, 0.f, 1.f), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FSovAudio::Play(UObject* WorldContext, const TCHAR* Name)
{
	if (USoundBase* Sound = Load(Name))
	{
		const float V = Volume(ESovVolume::Master) * Volume(ESovVolume::Effects);
		if (V > 0.f) UGameplayStatics::PlaySound2D(WorldContext, Sound, V);
	}
}

void FSovAudio::Switch(UObject* WorldContext, FLoop& Loop, const FString& Name, ESovVolume Channel)
{
	if (Loop.Name == Name && (Name.IsEmpty() || (Loop.Component.IsValid() && Loop.Component->IsPlaying()))) return;
	if (Loop.Component.IsValid())
	{
		Loop.Component->FadeOut(2.5f, 0.f);  // auto-destroys once silent
		Loop.Component.Reset();
	}
	Loop.Name = Name;
	USoundBase* Sound = Name.IsEmpty() ? nullptr : Load(Name);
	if (!Sound) return;
	UAudioComponent* C = UGameplayStatics::CreateSound2D(WorldContext, Sound, Volume(ESovVolume::Master) * Volume(Channel), 1.f, 0.f, nullptr, true, true);
	if (!C) return;
	C->FadeIn(2.5f, Volume(ESovVolume::Master) * Volume(Channel));
	Loop.Component.Reset(C);
}

void FSovAudio::SetMusic(UObject* WorldContext, const FString& Name)
{
	Switch(WorldContext, Music, Name, ESovVolume::Music);
}

void FSovAudio::SetAmbience(UObject* WorldContext, const FString& Name)
{
	Switch(WorldContext, Ambience, Name, ESovVolume::Ambience);
}

void FSovAudio::ApplyVolumes()
{
	if (Music.Component.IsValid()) Music.Component->SetVolumeMultiplier(Volume(ESovVolume::Master) * Volume(ESovVolume::Music));
	if (Ambience.Component.IsValid()) Ambience.Component->SetVolumeMultiplier(Volume(ESovVolume::Master) * Volume(ESovVolume::Ambience));
}

void FSovAudio::Stop()
{
	for (FLoop* L : {&Music, &Ambience})
	{
		if (L->Component.IsValid()) L->Component->Stop();
		L->Component.Reset();
		L->Name.Reset();
	}
}
