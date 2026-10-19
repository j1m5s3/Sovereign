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

	// Where a unit's figure is drawn now, less where it is going: non-zero while it walks from plot to plot (the
	// HUD moves its flag with it).
	FVector UnitDrift(int32 Id) const;

	virtual void Tick(float DeltaSeconds) override;

private:
	// Puts a unit's figure at its plot; when the unit was drawn elsewhere a moment ago (a move, not a jump across
	// the map), it walks there instead, facing the way it goes, then turns back to Rot.
	void PlaceUnit(UStaticMeshComponent* C, int32 Id, const FVector& At, const FRotator& Rot);
	// The figures of a unit's squad beside its lead figure (C), one fewer for each third of its health lost.
	int32 AddSquad(UStaticMeshComponent* C, int32 Index, const FSovUnitMarker& Unit, const FString& Figure);

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
	TArray<TObjectPtr<UStaticMeshComponent>> Trees;  // (unused since the forests are instanced)
	// Woods, as instances: conifers and broadleaves on seen plots, then the same on fogged ones (browned).
	UPROPERTY()
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Forest;

	// Kit rocks on hills and mountains.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Rocks;

	// Gold crowns on top of leader markers.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Crowns;

	// Wonders: the Wonders kit's model for its kind, or a temple (a monument while building).
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WonderPieces;
	// Districts (the Districts kit) and the houses round a city centre, more as it grows.
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DistrictPieces;
	// Terrain features and natural wonders (the Nature kit).
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FeaturePieces;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CityHouses;
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
	// A unit's other men, attached to its lead figure so they move with it.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SquadPieces;

	struct FGlide
	{
		TWeakObjectPtr<UStaticMeshComponent> Figure;
		int32 Id = 0;
		FVector From, To;
		FRotator Rot;  // facing when it arrives
		float T = 0.f, Duration = 0.5f;
	};
	TArray<FGlide> Glides;
	TMap<int32, FVector> UnitNow;     // each unit's figure where it is drawn now
	TMap<int32, FVector> UnitTarget;  // and where it is going

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

	UPROPERTY()
	TArray<TObjectPtr<ASovMapActor>> Ghosts;
	bool bGhost = false;     // a copy beside the real map: it spawns no copies of its own
	double WrapWidth = 0.0;  // world width of one copy on a wrapping map, else 0

	int32 Width = 0;
	TArray<double> Heights;  // per plot index, top surface
};
