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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "SovCameraPawn.h"
#include "SovGameSubsystem.h"
#include "SovHUD.h"
#include "SovLens.h"
#include "SovMinimap.h"
#include "SovMapActor.h"
#include "SovPlayerController.h"

#include "sovereign/game.h"

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
	const bool bMenu = !Sub->IsActive() && !FSovSetup::HasStartOptions();
	if (!Sub->IsActive() && !bMenu)
	{
		Sub->StartGame(FSovSetup::FromCommandLine());
	}
	else if (Sub->IsRunning())
	{
		OnStateChanged();
	}

	if (ASovPlayerController* PC = Cast<ASovPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SetMap(Map);
		PC->CenterOnHome();
		// -SovReplay=<file>: watch a recorded battle first (player-retention §2: replays can be shared).
		FString Replay;
		if (FParse::Value(FCommandLine::Get(), TEXT("SovReplay="), Replay))
		{
			PC->StartReplay(Replay);  // the menu opens when it ends
		}
		else if (bMenu && FParse::Param(FCommandLine::Get(), TEXT("SovSettings")))
		{
			PC->OpenSettings();
		}
		else if (bMenu && FParse::Param(FCommandLine::Get(), TEXT("SovSetup")))
		{
			PC->OpenSetup(FSovSetup());  // straight to the new-game screen
		}
		else if (bMenu)
		{
			PC->OpenMenu();  // no start options: the player chooses
		}
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
		FSovMirror Mirror = BuildMirror(Sub->GetGame(), Sub->GetSession().ViewPlayer());
		// The cosmetic chosen on this machine tints the viewer's own ruler (player-retention §7; not part of the game).
		const FString CosmeticId = USovGameSubsystem::Cosmetic();
		for (const sov::CosmeticType& C : Sub->GetGame().rules().cosmetics)
		{
			if (CosmeticId != UTF8_TO_TCHAR(C.id.c_str())) continue;
			for (FSovUnitMarker& U : Mirror.Units)
			{
				if (U.bLeader && U.Owner == Mirror.Viewer) U.Color = FLinearColor(FColor(C.color[0], C.color[1], C.color[2]));
			}
		}
		Sub->LensLegend.Reset();
		SovApplyLens(Mirror, Sub->GetGame(), Mirror.Viewer, Sub->Lens, &Sub->LensLegend);
		// The minimap: owners' colours over the ground (or the lens's), fogged plots darker.
		TSharedPtr<FSovMinimapData> Mini = MakeShared<FSovMinimapData>();
		Mini->Width = Mirror.Width;
		Mini->Height = Mirror.Height;
		Mini->Colors.Init(FLinearColor::Transparent, Mirror.Width * Mirror.Height);
		for (const FSovTile& Tile : Mirror.Tiles)
		{
			FLinearColor C = Sub->Lens == ESovLens::None && Tile.Owner >= 0 ? FMath::Lerp(Tile.Color, SovPlayerColor(Sub->GetGame(), Tile.Owner), 0.6f) : Tile.Color;
			if (!Tile.bVisible) C *= 0.55f;
			C.A = 1.f;
			if (Mini->Colors.IsValidIndex(Tile.Y * Mirror.Width + Tile.X)) Mini->Colors[Tile.Y * Mirror.Width + Tile.X] = C;
		}
		Sub->Minimap = Mini;
		Map->Sync(Mirror);
	}
}
