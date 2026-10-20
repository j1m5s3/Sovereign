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
	// The ruler's figure in the owner's colour: Figure (Leader_ENGLAND...) when its art exists, else the generic ruler.
	void SetColor(const FLinearColor& Color, const FString& Figure = TEXT("Leader"));
	// In the streets the ruler walks, and runs while Shift is held.
	void SetRunning(bool bRun);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Crown;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	bool bFigure = false;  // drawn by the kit's leader figure (no primitive crown)
	bool bRigged = false;  // drawn by the rigged leader (the character's mesh), walking when it moves

	virtual void Tick(float DeltaSeconds) override;

private:
	bool bWalking = false;
	FVector LastSpot = FVector::ZeroVector;  // where it stood last frame
	FString RigName = TEXT("Leader");      // the rigged figure it is drawn by
	float Pace = 0.f;                        // its eased speed, cm/s
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
	FVector ToWorld(const FVector& Local) const { return GetActorTransform().TransformPosition(Local); }
	// The scene is turned so the leader, walking in from the entry toward the square and the Palace, has the map's sun
	// (ASovGameMode::SpawnLighting) at the back: the buildings ahead are lit, not black against it.
	static FRotator Facing() { return FRotator(0.f, 0.f, 0.f); }

	// Scenes sit 10 km west of the map so both can exist at once (and the sky stays above them).
	static FVector Origin() { return FVector(1000000.0, -1000000.0, 0.0); }

private:
	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, float Yaw, const FLinearColor& Color);
	// A kit mesh at true size, pivot on the ground; a non-white tint makes an owner-coloured instance.
	UStaticMeshComponent* AddKitPiece(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FLinearColor& Tint);
	// The ground as one painted mesh (the art kit's material): paved plaza, cobbled streets, grass, and hills
	// rising beyond the town to close the horizon. False without the art (the slab and primitives stand in).
	bool BuildGround();
	// The ground's height at a point (0 in the town, rising to the hills beyond it).
	double GroundZ(double X, double Y) const;

	struct FCitizen
	{
		TObjectPtr<UStaticMeshComponent> Body;
		TObjectPtr<class USkeletalMeshComponent> Rigged;  // a walking figure (rig_figures.py); Body is hidden then
		FString Kind;                                     // which rigged figure (Townsman, Traveller, Townswoman, Elder... Citizen)
		FVector Target = FVector::ZeroVector;
		float Speed = 120.f;
		float Pause = 0.f;  // seconds left standing (idling) before walking on
		bool bFigure = false;
	};
	// A rigged person standing at a place, facing Yaw, playing its Idle; null without the art.
	class USkeletalMeshComponent* AddRigged(const FString& Name, const FVector& Location, float Yaw, const FLinearColor& Tint);

	FSovStreetLayout Layout;
	TArray<FCitizen> Citizens;
	// What the walkers step round (they move without collision): the well, carts, stalls and the like, as circles (centre, radius).
	TArray<FVector> Obstacles;
	FRandomStream Rng;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY()
	TObjectPtr<class UProceduralMeshComponent> Ground;

	UPROPERTY()
	TObjectPtr<class UExponentialHeightFogComponent> Fog;

	UPROPERTY()
	TArray<TObjectPtr<class USkeletalMeshComponent>> RiggedParts;

	UPROPERTY()
	TObjectPtr<class UPostProcessComponent> Grade;

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
