// Draws an FSovBattleSim on a patch of the actual terrain (step 5: one medieval battle).
// Primitives stand in for soldiers until art exists.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SovBattleSim.h"
#include "SovGait.h"

#include "SovBattleScene.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

// The sounds of a fight: the clash of a blow that lands, a bow loosed and a shot fired.
enum class ESovBattleSound : uint8 { Clash, Bow, Gunshot };

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
	FVector ToWorld(const FVector2D& P, double Z = 0.0) const { return GetActorTransform().TransformPosition(FVector(P.X, P.Y, Z)); }
	// A direction in the world (the camera's) as one on the field.
	FVector2D ToField(const FVector2D& Dir) const { return FVector2D(GetActorTransform().InverseTransformVectorNoScale(FVector(Dir, 0.0))); }
	// The field is turned so the side the player commands looks away from the map's sun (ASovGameMode::SpawnLighting),
	// the armies ahead lit rather than black against the light: side 0 starts at -X looking +X.
	static FRotator FacingFor(int32 HumanSide) { return FRotator(0.f, HumanSide == 0 ? 0.f : 180.f, 0.f); }

	// Battles are fought 10 km east of the map (street scenes sit to the west).
	static FVector Origin() { return FVector(-1000000.0, 1000000.0, 0.0); }

	// The human's ruler in a battle fought here is drawn by its walker: his strikes, hits and fall play on that mesh.
	void SetLeaderRig(class USkeletalMeshComponent* Rig);
	// Whom a man fights: the nearest living enemy within 175 cm (sov::battle::ShowTiming's engage, past the sim's reach of
	// 140) at the last frame the scene read, else INDEX_NONE.
	int32 Target(int32 Man) const;
	// Whom the strike he is shown making is at while it plays, else INDEX_NONE.
	int32 StrikingAt(int32 Man) const;

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

	// Men on their own legs (the rigged figures, rig_figures.py): per soldier, null when drawn still; where each
	// stood when it last moved, when that was, its speed and heading over that move (held a moment between a replay's
	// or the online view's snapshots), and whether it walks, to pace its stride.
	UPROPERTY()
	TArray<TObjectPtr<class USkeletalMeshComponent>> Rigs;
	TArray<FString> RigNames;  // which rigged figure each is drawn by
	TArray<FVector2D> LastPos;
	TArray<double> MovedAt;
	TArray<float> MoveSpeed;
	TArray<float> MoveYaw;
	TArray<bool> Striding;
	TArray<float> Yaws;  // each rigged figure's facing, eased toward where it goes
	double LastSyncTime = 0.0;

	// Strikes, hits and falls (plan G step 5): read from the men each Sync until the battle's last frame and played on
	// their rigs, each hit and fall timed to the blow that lands it. Per soldier, whom the strike he is shown making is at
	// and when it is over.
	sov::battle::Show Shows = sov::battle::Show(sov::battle::ShowTiming{kSovStrikeImpact, kSovStrikeLength});
	std::vector<sov::battle::Seen> SeenMen;
	std::vector<sov::battle::Cue> Cues;
	TArray<int32> StrikeAt;
	TArray<double> StrikeEnds;
	bool bShownOver = false;  // the battle's last frame has been read
	TWeakObjectPtr<class USkeletalMeshComponent> LeaderRig;
	void PlayCue(const FSovBattleSim& Sim, const sov::battle::Cue& Cue, double Now);
	class USovLocomotion* LocomotionOf(const FSovBattleSim& Sim, int32 Man) const;
	// A sound Delay seconds from now, even after the last Sync; each kind at most once in a short while (a crowd's
	// blows as one clash, not a roar). When each kind last played.
	void PlaySound(ESovBattleSound Kind, float Delay);
	void SoundNow(ESovBattleSound Kind);
	double SoundAt[3] = {-1.0e9, -1.0e9, -1.0e9};

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
