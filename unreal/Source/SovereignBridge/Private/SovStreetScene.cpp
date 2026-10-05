#include "SovStreetScene.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "SovArt.h"

namespace
{
uint32 ColorKey(const FLinearColor& C)
{
	return C.ToFColor(false).ToPackedRGBA();
}
}  // namespace

// ------------------------------------------------------------------ the walker

ASovWalker::ASovWalker()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(40.f, 90.f);
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.6, 0.6, 1.7));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
	Crown->SetupAttachment(GetCapsuleComponent());
	Crown->SetStaticMesh(Sphere.Object);
	Crown->SetRelativeLocation(FVector(0, 0, 105));
	Crown->SetRelativeScale3D(FVector(0.35));
	Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->TargetArmLength = 650.f;
	Boom->SocketOffset = FVector(0, 0, 150);
	Boom->bUsePawnControlRotation = true;
	Boom->bDoCollisionTest = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom);
	Camera->SetFieldOfView(70.f);
}

void ASovWalker::SetColor(const FLinearColor& Color)
{
	static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Base, this);
	BodyMat->SetVectorParameterValue(TEXT("Color"), Color);
	Body->SetMaterial(0, BodyMat);
	UMaterialInstanceDynamic* Gold = UMaterialInstanceDynamic::Create(Base, this);
	Gold->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.75f, 0.1f));
	Crown->SetMaterial(0, Gold);
}

// ------------------------------------------------------------------ the scene

ASovStreetScene::ASovStreetScene()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	BaseMaterial = Material.Object;
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	ConeMesh = Cone.Object;
}

UMaterialInstanceDynamic* ASovStreetScene::MaterialFor(const FLinearColor& Color)
{
	const uint32 Key = ColorKey(Color);
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Materials.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Materials.Add(Key, Mid);
	return Mid;
}

UStaticMeshComponent* ASovStreetScene::AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, float Yaw, const FLinearColor& Color)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Location);
	C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	C->SetRelativeScale3D(Scale);
	C->SetMaterial(0, MaterialFor(Color));
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

UStaticMeshComponent* ASovStreetScene::AddKitPiece(UStaticMesh* Mesh, const FVector& Location, float Yaw, const FLinearColor& Tint)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Location);
	C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	if (!Tint.Equals(FLinearColor::White))
	{
		if (UMaterialInstanceDynamic* Mid = SovArt::Tinted(this, Tint))
		{
			C->SetMaterial(0, Mid);
		}
	}
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

void ASovStreetScene::Build(const FSovStreetLayout& InLayout)
{
	Layout = InLayout;
	Rng.Initialize(Layout.CityId * 7919 + 17);
	// Ground: a large slab under the hex (BasicShapes are 100 cm cubes).
	AddPiece(CubeMesh, FVector(0, 0, -50), FVector(Layout.Radius * 2.2 / 100.0, Layout.Radius * 2.2 / 100.0, 1.0), 0.f, FLinearColor(0.32f, 0.45f, 0.24f));
	for (const FSovStreetPiece& P : Layout.Pieces)
	{
		const FVector Scale = P.Size / 100.0;
		// Kit meshes when the art has been built (tools/art); primitives otherwise.
		const TCHAR* Kit = P.Kind == ESovStreetPiece::Tree ? TEXT("Nature") : TEXT("Classical");
		if (UStaticMesh* Mesh = P.Recipe.IsEmpty() ? nullptr : SovArt::Mesh(Kit, P.Recipe))
		{
			if (P.Kind == ESovStreetPiece::Wall)
			{
				// A run of wall pieces (10 m each) along the edge.
				const int32 N = FMath::Max(1, FMath::RoundToInt(P.Size.X / 1000.0));
				const FVector Dir = FRotator(0.f, P.Yaw, 0.f).Vector();
				for (int32 k = 0; k < N; ++k)
				{
					const FVector At = FVector(P.Location.X, P.Location.Y, 0) + Dir * (P.Size.X * ((k + 0.5) / N - 0.5));
					AddKitPiece(Mesh, At, P.Yaw, FLinearColor::White);
				}
				continue;
			}
			const bool bTint = P.Kind == ESovStreetPiece::Banner;
			AddKitPiece(Mesh, FVector(P.Location.X, P.Location.Y, 0), P.Yaw, bTint ? P.Color : FLinearColor::White);
			continue;
		}
		switch (P.Kind)
		{
			case ESovStreetPiece::Plaza:
				AddPiece(CylinderMesh, P.Location, FVector(Scale.X, Scale.Y, Scale.Z), 0.f, P.Color);
				break;
			case ESovStreetPiece::Banner:
			case ESovStreetPiece::Guard:
				AddPiece(CylinderMesh, P.Location, Scale, P.Yaw, P.Color);
				break;
			case ESovStreetPiece::Landmark:
			{
				AddPiece(CubeMesh, P.Location, Scale, P.Yaw, P.Color);
				// A roof marks each landmark (stage 4: "landmark model").
				AddPiece(ConeMesh, P.Location + FVector(0, 0, P.Size.Z * 0.5 + 150), FVector(Scale.X * 0.75, Scale.Y * 0.75, 3.0), P.Yaw,
					FLinearColor(0.45f, 0.2f, 0.15f));
				break;
			}
			default:
				AddPiece(CubeMesh, P.Location, Scale, P.Yaw, P.Color);
				break;
		}
	}
	// The two people the leader can talk to.
	AddPiece(CylinderMesh, Layout.Herald + FVector(0, 0, 90), FVector(0.6, 0.6, 1.8), 0.f, FLinearColor(0.9f, 0.8f, 0.2f));
	AddPiece(SphereMesh, Layout.Herald + FVector(0, 0, 205), FVector(0.4), 0.f, FLinearColor(0.95f, 0.8f, 0.65f));
	AddPiece(CylinderMesh, Layout.Captain + FVector(0, 0, 90), FVector(0.7, 0.7, 1.9), 0.f, FLinearColor(0.35f, 0.08f, 0.08f));
	AddPiece(ConeMesh, Layout.Captain + FVector(0, 0, 215), FVector(0.5, 0.5, 0.6), 0.f, FLinearColor(0.6f, 0.6f, 0.62f));
	// Citizens wander between points on the plaza and streets (stage 5: crowd by population).
	for (int32 i = 0; i < Layout.Crowd; ++i)
	{
		FCitizen C;
		const FVector Start(Rng.FRandRange(-2500.f, 2500.f), Rng.FRandRange(-2500.f, 2500.f), 80.f);
		const float Shade = Rng.FRandRange(0.35f, 0.85f);
		C.Body = AddPiece(CylinderMesh, Start, FVector(0.45, 0.45, 1.6), 0.f, FLinearColor(Shade, Shade * 0.8f, Shade * 0.6f));
		C.Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C.Target = FVector(Rng.FRandRange(-3000.f, 3000.f), Rng.FRandRange(-3000.f, 3000.f), 80.f);
		C.Speed = Rng.FRandRange(80.f, 160.f) * (Layout.bFear ? 0.6f : 1.f);  // a cowed city moves quietly
		Citizens.Add(C);
	}
}

void ASovStreetScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (FCitizen& C : Citizens)
	{
		const FVector Here = C.Body->GetRelativeLocation();
		const FVector To = C.Target - Here;
		if (To.Size2D() < 50.f)
		{
			C.Target = FVector(Rng.FRandRange(-3000.f, 3000.f), Rng.FRandRange(-3000.f, 3000.f), 80.f);
			continue;
		}
		C.Body->SetRelativeLocation(Here + To.GetSafeNormal2D() * C.Speed * DeltaSeconds);
	}
}
