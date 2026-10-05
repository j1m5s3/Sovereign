#include "SovGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "SovCameraPawn.h"
#include "SovGameSubsystem.h"
#include "SovHUD.h"
#include "SovMapActor.h"
#include "SovPlayerController.h"

ASovGameMode::ASovGameMode()
{
	DefaultPawnClass = ASovCameraPawn::StaticClass();
	PlayerControllerClass = ASovPlayerController::StaticClass();
	HUDClass = ASovHUD::StaticClass();
}

void ASovGameMode::SpawnLighting()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.f, -150.f, 0.f), Params);  // from behind the default views (cameras look north, +X)
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(2.6f);
		L->SetAtmosphereSunLight(true);
	}

	AActor* Sky = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky);
	Sky->SetRootComponent(Atmosphere);
	Atmosphere->RegisterComponent();

	ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (USkyLightComponent* L = SkyLight->GetLightComponent())
	{
		// Ambient light from the engine's daylight cubemap, so faces turned from the sun stay readable.
		L->SetMobility(EComponentMobility::Movable);
		L->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
		L->SetCubemap(LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")));
		L->SetIntensity(1.0f);
		L->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));  // warm the cubemap so shaded faces are not blue
		L->RecaptureSky();
	}
}

void ASovGameMode::StartPlay()
{
	Super::StartPlay();
	SpawnLighting();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Map = GetWorld()->SpawnActor<ASovMapActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	USovGameSubsystem* Sub = GetGameInstance()->GetSubsystem<USovGameSubsystem>();
	StateChangedHandle = Sub->OnStateChanged.AddUObject(this, &ASovGameMode::OnStateChanged);
	if (!Sub->IsRunning())
	{
		Sub->StartGame(FSovSetup::FromCommandLine());
	}
	else
	{
		OnStateChanged();
	}

	if (ASovPlayerController* PC = Cast<ASovPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SetMap(Map);
		PC->CenterOnHome();
	}
}

void ASovGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USovGameSubsystem* Sub = GI->GetSubsystem<USovGameSubsystem>())
		{
			Sub->OnStateChanged.Remove(StateChangedHandle);
		}
	}
	Super::EndPlay(Reason);
}

void ASovGameMode::OnStateChanged()
{
	USovGameSubsystem* Sub = GetGameInstance()->GetSubsystem<USovGameSubsystem>();
	if (Map && Sub->IsRunning())
	{
		Map->Sync(BuildMirror(Sub->GetGame(), Sub->GetSession().ViewPlayer()));
	}
}
