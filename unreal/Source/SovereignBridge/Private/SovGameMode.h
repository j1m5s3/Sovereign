// Starts a rules-core game, lights the empty map and keeps the map actor in step
// with the core after every change.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SovGameMode.generated.h"

class ASovMapActor;

UCLASS()
class ASovGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASovGameMode();

	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void SpawnLighting();
	void OnStateChanged();

	UPROPERTY()
	TObjectPtr<ASovMapActor> Map;

	FDelegateHandle StateChangedHandle;
};
