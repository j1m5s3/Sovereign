// A street-level scene: builds an FSovStreetLayout out of engine primitives far from
// the map, with wandering citizens, the herald and the captain of the guard.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

#include "SovStreetLayout.h"

#include "SovStreetScene.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;

// The leader on foot: a capsule with a pillar body and a gold crown, third-person camera.
UCLASS()
class ASovWalker : public ACharacter
{
	GENERATED_BODY()

public:
	ASovWalker();
	void SetColor(const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Crown;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	bool bFigure = false;  // drawn by the kit's leader figure (no primitive crown)
};

UCLASS()
class ASovStreetScene : public AActor
{
	GENERATED_BODY()

public:
	ASovStreetScene();
	virtual void Tick(float DeltaSeconds) override;

	void Build(const FSovStreetLayout& Layout);
	const FSovStreetLayout& GetLayout() const { return Layout; }
	// World positions of the scene's people and landmarks (scene origin is the actor's location).
	FVector ToWorld(const FVector& Local) const { return GetActorLocation() + Local; }

	// Scenes sit 10 km west of the map so both can exist at once (and the sky stays above them).
	static FVector Origin() { return FVector(1000000.0, -1000000.0, 0.0); }

private:
	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, float Yaw, const FLinearColor& Color);
	// A kit mesh at true size, pivot on the ground; a non-white tint makes an owner-coloured instance.
	UStaticMeshComponent* AddKitPiece(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FLinearColor& Tint);

	struct FCitizen
	{
		TObjectPtr<UStaticMeshComponent> Body;
		FVector Target = FVector::ZeroVector;
		float Speed = 120.f;
		bool bFigure = false;
	};

	FSovStreetLayout Layout;
	TArray<FCitizen> Citizens;
	FRandomStream Rng;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;
};
