// Draws a FSovMirror: one procedural mesh for the revealed terrain (a section per
// colour) and pooled engine-shape markers for units and cities.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SovMirror.h"

#include "SovMapActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class ASovMapActor : public AActor
{
	GENERATED_BODY()

public:
	ASovMapActor();

	void Sync(const FSovMirror& Mirror);
	// Highlights a plot (selection); X < 0 hides the highlight.
	void SetHighlight(int32 X, int32 Y);

	// Height of a plot's top surface (0 when it is not drawn).
	double SurfaceZ(int32 X, int32 Y) const;

	static double ReliefHeight(ESovRelief Relief);

private:
	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UStaticMeshComponent* Marker(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32 Index, UStaticMesh* Mesh);
	void BuildTerrain(const FSovMirror& Mirror);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Terrain;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Highlight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> UnitMarkers;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CityMarkers;

	// Gold crowns on top of leader markers.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Crowns;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	int32 Width = 0;
	TArray<double> Heights;  // per plot index, top surface
};
