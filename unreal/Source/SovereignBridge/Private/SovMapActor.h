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
	// Outlines where the selected unit can go this turn (empty: none).
	void SetReach(const TArray<FSovEdge>& Edges);

	// Height of a plot's top surface (0 when it is not drawn).
	double SurfaceZ(int32 X, int32 Y) const;

	static double ReliefHeight(ESovRelief Relief);

private:
	UMaterialInstanceDynamic* MaterialFor(const FLinearColor& Color);
	UStaticMeshComponent* Marker(TArray<TObjectPtr<UStaticMeshComponent>>& Pool, int32 Index, UStaticMesh* Mesh);
	void BuildTerrain(const FSovMirror& Mirror);
	// On a wrapping map, the copies a map's width to the west and east draw the same mirror.
	void SyncGhosts(const FSovMirror& Mirror);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Terrain;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Highlight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> UnitMarkers;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CityMarkers;

	// Kit trees on woods tiles.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Trees;

	// Gold crowns on top of leader markers.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Crowns;

	// Wonders: a temple when built, a monument while building.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WonderPieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> VillagePieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> AntiquityPieces;

	// Road segments between plot centres.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RoadPieces;

	// Rivers, territory borders, resource and improvement markers.
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> RiverPieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BorderPieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ResourcePieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ImprovementPieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ReachPieces;

	// A strip along the edge between two neighbouring plots, pulled `Inset` toward A's centre.
	int32 EdgeStrips(const TArray<FSovEdge>& Edges, TArray<TObjectPtr<UStaticMeshComponent>>& Pool, double Inset, double StripWidth, double Lift);

	// Boats under embarked units.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Boats;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Materials;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// The art kit's material (vertex colour x detail tile) for the terrain; null when the art is not imported.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> KitMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TArray<TObjectPtr<ASovMapActor>> Ghosts;
	bool bGhost = false;     // a copy beside the real map: it spawns no copies of its own
	double WrapWidth = 0.0;  // world width of one copy on a wrapping map, else 0

	int32 Width = 0;
	TArray<double> Heights;  // per plot index, top surface
};
