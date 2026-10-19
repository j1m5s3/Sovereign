// Draws an FSovBattleSim on a patch of the actual terrain (step 5: one medieval battle).
// Primitives stand in for soldiers until art exists.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SovBattleSim.h"

#include "SovBattleScene.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class ASovBattleScene : public AActor
{
	GENERATED_BODY()

public:
	ASovBattleScene();

	// Ground colour from the fought-over plot; woods scatter trees (cover is decorative here).
	// bCity: a city assault, fought before the city with the defenders backed by its walls (bWalls)
	// or its outer houses.
	void Build(const FSovBattleSpec& Spec, const FLinearColor& Ground, bool bWoods, const FLinearColor& AttackerColor,
		const FLinearColor& DefenderColor, bool bCity = false, bool bWalls = false);
	void Sync(const FSovBattleSim& Sim);
	FVector ToWorld(const FVector2D& P, double Z = 0.0) const { return GetActorLocation() + FVector(P.X, P.Y, Z); }

	// Battles are fought 10 km east of the map (street scenes sit to the west).
	static FVector Origin() { return FVector(-1000000.0, 1000000.0, 0.0); }

private:
	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UStaticMeshComponent* Add(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color);

	FLinearColor SideColor[2];
	// Each side's soldier figure: its unit's when that is a man on foot or a horseman, else the spearman.
	FString SideFigure[2] = {TEXT("Soldier"), TEXT("Soldier")};
	// A siege or armoured side's machine (Siege, Tank), drawn for every third man of a squad, the rest its crew
	// (SideFigure); empty for the others.
	FString SideVehicle[2];
	// Turn added to a side's figures: horsemen are modelled side-on (along X), the people facing the viewer.
	float SideYaw[2] = {0.f, 0.f};
	int32 SquadMen[2][3] = {{0, 0, 0}, {0, 0, 0}};  // soldiers drawn so far per squad, for picking the machines
	TArray<bool> Figure;   // per soldier: drawn by a kit figure
	TArray<bool> Vehicle;  // per soldier: drawn as its side's machine (side-on, left standing as a dark wreck)

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WreckMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Bodies;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Heads;

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

	// The field: flat where the armies fight, rising to wooded hills all round, under a haze and a warm grade.
	bool BuildField(const FLinearColor& Ground, int32 Seed);
	UPROPERTY()
	TObjectPtr<class UProceduralMeshComponent> Field;
	UPROPERTY()
	TObjectPtr<class UExponentialHeightFogComponent> Haze;
	UPROPERTY()
	TObjectPtr<class UPostProcessComponent> Grade;
};
