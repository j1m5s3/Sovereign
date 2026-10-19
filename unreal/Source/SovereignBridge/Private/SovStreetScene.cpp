#include "SovStreetScene.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PostProcessComponent.h"

#include "SovArt.h"

namespace
{
uint32 ColorKey(const FLinearColor& C)
{
	return C.ToFColor(false).ToPackedRGBA();
}

// As the map's terrain (SovMapActor KitPaint): a linear colour as the bytes M_SovKit squares back, and the art kit's
// detail cell in alpha (SovArt::Cell).
FColor KitPaint(const FLinearColor& Linear, int32 Tile)
{
	auto Byte = [](float V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Sqrt(FMath::Max(V, 0.f)) * 255.f), 0, 255)); };
	return FColor(Byte(Linear.R), Byte(Linear.G), Byte(Linear.B), static_cast<uint8>(FMath::RoundToInt(Tile * 255.f / (SovArt::KitCells - 1))));
}

// Smooth value noise in about 0..1, from a few sines (deterministic, no state).
double Noise(double X, double Y, double Scale)
{
	const double U = X / Scale, V = Y / Scale;
	return 0.5 + 0.25 * FMath::Sin(U * 1.7 + FMath::Cos(V * 0.9)) * FMath::Cos(V * 1.3 - 0.7) + 0.15 * FMath::Sin(U * 3.1 + V * 2.3)
		+ 0.1 * FMath::Cos(U * 5.3 - V * 4.1);
}

double SegmentDistance2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
	const FVector2D AB = B - A;
	const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
	return FVector2D::Distance(P, A + AB * T);
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
	// The rigged leader when the art exists, walking as the character moves; else the kit's still figure.
	if (SovArt::SetRiggedFigure(GetMesh(), TEXT("Leader"), Color, TEXT("Idle")))
	{
		GetMesh()->SetRelativeLocation(FVector(0, 0, -90));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));  // as the kit figures: built facing +Y in Unreal
		Body->SetVisibility(false);
		Crown->SetVisibility(false);
		bFigure = bRigged = true;
		PrimaryActorTick.bCanEverTick = true;
		SetActorTickEnabled(true);
		return;
	}
	// The leader's own figure when the art exists: robe and cloak in the owner's colour.
	if (SovArt::SetKitMesh(Body, TEXT("Figures"), TEXT("Leader"), Color))
	{
		Body->SetRelativeLocation(FVector(0, 0, -90));
		Body->SetRelativeScale3D(FVector(1.0));
		Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));  // kit figures face +Y in Unreal; the character faces +X
		Crown->SetVisibility(false);
		bFigure = true;
		return;
	}
	static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Base, this);
	BodyMat->SetVectorParameterValue(TEXT("Color"), Color);
	Body->SetMaterial(0, BodyMat);
	UMaterialInstanceDynamic* Gold = UMaterialInstanceDynamic::Create(Base, this);
	Gold->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.75f, 0.1f));
	Crown->SetMaterial(0, Gold);
}

void ASovWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRigged) return;
	// Walk while moving, at a pace that keeps the feet from sliding (the cycle is one stride a second at 140 cm/s).
	const float Speed = GetVelocity().Size2D();
	const bool bMoving = Speed > 20.f;
	if (bMoving != bWalking)
	{
		bWalking = bMoving;
		if (UAnimSequence* A = SovArt::RiggedAnim(TEXT("Leader"), bMoving ? TEXT("Walk") : TEXT("Idle"))) GetMesh()->PlayAnimation(A, true);
	}
	if (bWalking) GetMesh()->SetPlayRate(FMath::Clamp(Speed / 140.f, 0.6f, 3.5f));
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

double ASovStreetScene::GroundZ(double X, double Y) const
{
	const double R = Layout.Radius;
	const double T = FMath::Clamp((FMath::Sqrt(X * X + Y * Y) - R * 0.97) / (R * 1.5), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T) * (1500.0 + 2200.0 * Noise(X, Y, 4200.0));
}

bool ASovStreetScene::BuildGround()
{
	UMaterialInterface* Kit = SovArt::KitMaterial();
	if (!Kit) return false;
	double PlazaR = 0.0;
	TArray<TPair<FVector2D, FVector2D>> Streets;
	for (const FSovStreetPiece& P : Layout.Pieces)
	{
		if (P.Kind == ESovStreetPiece::Plaza) PlazaR = P.Size.X * 0.5;
		if (P.Kind == ESovStreetPiece::Street)
		{
			const FVector2D Dir(FRotator(0.f, P.Yaw, 0.f).Vector());
			const FVector2D Mid(P.Location);
			Streets.Add({Mid - Dir * P.Size.X * 0.5, Mid + Dir * P.Size.X * 0.5});
		}
	}
	// A grid finer in the town than in the hills: x = Extent * sign(u) * |u|^1.7 for u evenly spaced in -1..1.
	const int32 N = 230;
	const double Extent = Layout.Radius * 3.2;
	TArray<double> Axis;
	for (int32 i = 0; i <= N; ++i)
	{
		const double U = -1.0 + 2.0 * i / N;
		Axis.Add(Extent * FMath::Sign(U) * FMath::Pow(FMath::Abs(U), 1.7));
	}
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<int32> Triangles;
	for (int32 j = 0; j <= N; ++j)
	{
		for (int32 i = 0; i <= N; ++i)
		{
			const double X = Axis[i], Y = Axis[j];
			const double Z = GroundZ(X, Y);
			Vertices.Add(FVector(X, Y, Z));
			const double E = 50.0;
			Normals.Add(FVector(GroundZ(X - E, Y) - GroundZ(X + E, Y), GroundZ(X, Y - E) - GroundZ(X, Y + E), 2.0 * E).GetSafeNormal());
			UVs.Add(FVector2D(X, Y) / 160.0);
			// What the ground is here: paving in the square, cobbles down the streets with dirt at their edges, grass
			// that dries toward the hills.
			const double R = FMath::Sqrt(X * X + Y * Y);
			double DStreet = 1e9;
			for (const TPair<FVector2D, FVector2D>& S : Streets) DStreet = FMath::Min(DStreet, SegmentDistance2D(FVector2D(X, Y), S.Key, S.Value));
			const float Var = static_cast<float>(0.88 + 0.24 * Noise(X, Y, 700.0));
			FColor C;
			if (R < PlazaR - 90.0) C = KitPaint(FLinearColor(0.36f, 0.31f, 0.25f) * Var, SovArt::Cell::Flagstones);
			else if (R < PlazaR + 30.0) C = KitPaint(FLinearColor(0.2f, 0.19f, 0.17f), SovArt::Cell::Masonry);  // the kerb round the square
			else if (DStreet < 240.0) C = KitPaint(FLinearColor(0.27f, 0.24f, 0.2f) * Var, SovArt::Cell::Cobbles);
			else if (DStreet < 340.0) C = KitPaint(FLinearColor(0.25f, 0.18f, 0.11f) * Var, SovArt::Cell::Sand);
			else
			{
				const float Dry = static_cast<float>(FMath::Clamp(Z / 2500.0, 0.0, 1.0));
				const FLinearColor Lush(0.1f, 0.2f, 0.05f), Sere(0.26f, 0.25f, 0.11f);
				C = KitPaint(FMath::Lerp(Lush, Sere, Dry * 0.7f + 0.3f * static_cast<float>(Noise(Y, X, 1500.0))) * Var, SovArt::Cell::Grass);
			}
			Colors.Add(C);
		}
	}
	for (int32 j = 0; j < N; ++j)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const int32 A = j * (N + 1) + i, B = A + 1, Cn = A + N + 1, D = Cn + 1;
			Triangles.Append({A, Cn, B, B, Cn, D});
		}
	}
	Ground = NewObject<UProceduralMeshComponent>(this);
	Ground->SetupAttachment(RootComponent);
	Ground->RegisterComponent();
	Ground->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
	Ground->SetMaterial(0, Kit);
	Ground->SetCollisionProfileName(TEXT("BlockAll"));
	// Woods on the hills round the town, and a haze that softens the distance.
	for (int32 k = 0; k < 140; ++k)
	{
		const double A = Rng.FRandRange(0.f, 2.f * PI);
		const double R = Layout.Radius * Rng.FRandRange(1.05f, 2.6f);
		const FVector At(R * FMath::Cos(A), R * FMath::Sin(A), 0.0);
		if (UStaticMesh* Tree = SovArt::Mesh(TEXT("Nature"), Rng.FRand() < 0.6f ? TEXT("Tree_Broadleaf") : TEXT("Tree_Conifer")))
		{
			UStaticMeshComponent* T = AddKitPiece(Tree, FVector(At.X, At.Y, GroundZ(At.X, At.Y) - 20.0), Rng.FRandRange(0.f, 360.f), FLinearColor::White);
			T->SetRelativeScale3D(FVector(Rng.FRandRange(1.3f, 2.2f)));
			T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(RootComponent);
	Fog->SetFogDensity(0.008f);
	Fog->SetFogHeightFalloff(0.08f);
	Fog->SetStartDistance(2500.f);
	Fog->SetFogInscatteringColor(FLinearColor(0.62f, 0.66f, 0.72f));
	Fog->RegisterComponent();
	// A warm, fuller grade for the street (the map keeps its own look): more contrast and colour, a late-afternoon
	// white balance, a touch brighter for the darker paving and grass.
	Grade = NewObject<UPostProcessComponent>(this);
	Grade->SetupAttachment(RootComponent);
	Grade->bUnbound = true;
	Grade->Priority = 1.f;
	FPostProcessSettings& S = Grade->Settings;
	S.bOverride_ColorContrast = S.bOverride_ColorSaturation = S.bOverride_WhiteTemp = S.bOverride_AutoExposureBias = true;
	S.bOverride_VignetteIntensity = true;
	S.ColorContrast = FVector4(1.12, 1.12, 1.12, 1.0);
	S.ColorSaturation = FVector4(1.18, 1.18, 1.18, 1.0);
	S.WhiteTemp = 7600.f;  // above 6500 warms the image
	S.AutoExposureBias = 0.3f;
	S.VignetteIntensity = 0.35f;
	Grade->RegisterComponent();
	return true;
}

USkeletalMeshComponent* ASovStreetScene::AddRigged(const FString& Name, const FVector& Location, float Yaw, const FLinearColor& Tint)
{
	if (!SovArt::RiggedMesh(Name)) return nullptr;
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Location);
	C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	SovArt::SetRiggedFigure(C, Name, Tint, TEXT("Idle"));
	C->SetPosition(Rng.FRandRange(0.f, 3.f));  // not all breathing in step
	RiggedParts.Add(C);
	return C;
}

void ASovStreetScene::Build(const FSovStreetLayout& InLayout)
{
	Layout = InLayout;
	Rng.Initialize(Layout.CityId * 7919 + 17);
	// Ground: one painted mesh with the art kit; without it, a large slab under the hex (BasicShapes are 100 cm cubes).
	const bool bGround = BuildGround();
	if (!bGround) AddPiece(CubeMesh, FVector(0, 0, -50), FVector(Layout.Radius * 2.2 / 100.0, Layout.Radius * 2.2 / 100.0, 1.0), 0.f, FLinearColor(0.32f, 0.45f, 0.24f));
	for (const FSovStreetPiece& P : Layout.Pieces)
	{
		if (bGround && (P.Kind == ESovStreetPiece::Plaza || P.Kind == ESovStreetPiece::Street)) continue;  // painted into the ground
		const FVector Scale = P.Size / 100.0;
		// Kit meshes when the art has been built (tools/art); primitives otherwise.
		// Houses and the Palace in the owner's era's style (SovArt::EraPiece); here the Landmark stands for other
		// buildings, so it stays.
		const TPair<const TCHAR*, FString> Piece = P.Kind == ESovStreetPiece::Tree ? TPair<const TCHAR*, FString>(TEXT("Nature"), P.Recipe)
			: P.Recipe == TEXT("Landmark") ? TPair<const TCHAR*, FString>(TEXT("Classical"), P.Recipe) : SovArt::EraPiece(Layout.Era, P.Recipe, Layout.Civ);
		const TCHAR* Kit = Piece.Key;
		if (UStaticMesh* Mesh = P.Recipe.IsEmpty() ? nullptr : SovArt::Mesh(Kit, Piece.Value))
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
			UStaticMeshComponent* C = AddKitPiece(Mesh, FVector(P.Location.X, P.Location.Y, 0), P.Yaw, FLinearColor::White);
			if (P.Kind == ESovStreetPiece::Banner)
			{
				SovArt::SetKitMesh(C, Kit, Piece.Value, P.Color);  // the cloth takes the owner's colour
			}
			continue;
		}
		switch (P.Kind)
		{
			case ESovStreetPiece::Plaza:
				AddPiece(CylinderMesh, P.Location, FVector(Scale.X, Scale.Y, Scale.Z), 0.f, P.Color);
				break;
			case ESovStreetPiece::Guard:
			{
				// Soldiers of the guard stand at the street mouths under Fear.
				const float Out = FMath::RadiansToDegrees(FMath::Atan2(P.Location.Y, P.Location.X)) - 90.f;
				if (AddRigged(TEXT("Soldier"), FVector(P.Location.X, P.Location.Y, 0), Out, Layout.CivColor)) break;
				UStaticMeshComponent* G = AddPiece(CylinderMesh, P.Location, Scale, P.Yaw + 180.f, P.Color);
				if (SovArt::SetKitMesh(G, TEXT("Figures"), TEXT("Soldier"), Layout.CivColor))
				{
					// Facing out of the plaza, down their street.
					G->SetRelativeLocation(FVector(P.Location.X, P.Location.Y, 0));
					G->SetRelativeScale3D(FVector(1.0));
					G->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(P.Location.Y, P.Location.X)) - 90.f, 0.f));
				}
				break;
			}
			case ESovStreetPiece::Banner:
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
	// The two people the leader can talk to (kit figures, primitives without the art).
	UStaticMeshComponent* Herald = AddRigged(TEXT("Herald"), Layout.Herald, 90.f, Layout.CivColor) ? nullptr
		: AddPiece(CylinderMesh, Layout.Herald + FVector(0, 0, 90), FVector(0.6, 0.6, 1.8), 0.f, FLinearColor(0.9f, 0.8f, 0.2f));
	if (!Herald) {}
	else if (SovArt::SetKitMesh(Herald, TEXT("Figures"), TEXT("Herald"), Layout.CivColor))
	{
		Herald->SetRelativeLocation(Layout.Herald);
		Herald->SetRelativeScale3D(FVector(1.0));
		Herald->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));  // facing the street the leader walks in from (-X)
	}
	else
	{
		AddPiece(SphereMesh, Layout.Herald + FVector(0, 0, 205), FVector(0.4), 0.f, FLinearColor(0.95f, 0.8f, 0.65f));
	}
	UStaticMeshComponent* Captain = AddRigged(TEXT("Captain"), Layout.Captain, 90.f, Layout.CivColor) ? nullptr
		: AddPiece(CylinderMesh, Layout.Captain + FVector(0, 0, 90), FVector(0.7, 0.7, 1.9), 0.f, FLinearColor(0.35f, 0.08f, 0.08f));
	if (!Captain) {}
	else if (SovArt::SetKitMesh(Captain, TEXT("Figures"), TEXT("Captain"), Layout.CivColor))
	{
		Captain->SetRelativeLocation(Layout.Captain);
		Captain->SetRelativeScale3D(FVector(1.0));
		Captain->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	}
	else
	{
		AddPiece(ConeMesh, Layout.Captain + FVector(0, 0, 215), FVector(0.5, 0.5, 0.6), 0.f, FLinearColor(0.6f, 0.6f, 0.62f));
	}
	// Citizens wander between points on the plaza and streets (stage 5: crowd by population).
	for (int32 i = 0; i < Layout.Crowd; ++i)
	{
		FCitizen C;
		const FVector Start(Rng.FRandRange(-2500.f, 2500.f), Rng.FRandRange(-2500.f, 2500.f), 80.f);
		const float Shade = Rng.FRandRange(0.35f, 0.85f);
		C.Body = AddPiece(CylinderMesh, Start, FVector(0.45, 0.45, 1.6), 0.f, FLinearColor(Shade, Shade * 0.8f, Shade * 0.6f));
		C.Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Townsfolk in clothes of many colours (the citizen's team cloth), walking on their own legs.
		const FLinearColor Cloth = FLinearColor::MakeFromHSV8(static_cast<uint8>(Rng.RandRange(0, 255)), static_cast<uint8>(Rng.RandRange(60, 150)),
			static_cast<uint8>(Rng.RandRange(150, 235)));
		// One of the townsfolk (the dyed variants of rig_figures.py), else the plain citizen.
		static const TCHAR* const Folk[] = {TEXT("Townsman"), TEXT("Traveller"), TEXT("Townswoman")};
		C.Kind = Folk[Rng.RandRange(0, UE_ARRAY_COUNT(Folk) - 1)];
		if (!SovArt::RiggedMesh(C.Kind)) C.Kind = TEXT("Citizen");
		if ((C.Rigged = AddRigged(C.Kind, FVector(Start.X, Start.Y, 0), 0.f, Cloth)) != nullptr)
		{
			C.Body->SetVisibility(false);
			C.Rigged->SetRelativeScale3D(FVector(Rng.FRandRange(0.92f, 1.06f)));
			if (UAnimSequence* Walk = SovArt::RiggedAnim(C.Kind, TEXT("Walk"))) C.Rigged->PlayAnimation(Walk, true);
			C.bFigure = true;
		}
		else if (SovArt::SetKitMesh(C.Body, TEXT("Figures"), TEXT("Citizen"), FLinearColor::White))
		{
			C.Body->SetRelativeScale3D(FVector(Rng.FRandRange(0.92f, 1.06f)));
			C.bFigure = true;
		}
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
		USceneComponent* Who = C.Rigged ? static_cast<USceneComponent*>(C.Rigged) : static_cast<USceneComponent*>(C.Body);
		if (C.Pause > 0.f)
		{
			// Standing a while, then walking on.
			C.Pause -= DeltaSeconds;
			if (C.Pause <= 0.f && C.Rigged)
				if (UAnimSequence* Walk = SovArt::RiggedAnim(C.Kind, TEXT("Walk"))) C.Rigged->PlayAnimation(Walk, true);
			continue;
		}
		const FVector Here = Who->GetRelativeLocation();
		const FVector To = C.Target - Here;
		if (To.Size2D() < 50.f)
		{
			C.Target = FVector(Rng.FRandRange(-3000.f, 3000.f), Rng.FRandRange(-3000.f, 3000.f), 80.f);
			if (C.Rigged && Rng.FRand() < 0.5f)
			{
				C.Pause = Rng.FRandRange(2.f, 7.f);  // stop to look about, or to talk
				if (UAnimSequence* Idle = SovArt::RiggedAnim(C.Kind, TEXT("Idle"))) C.Rigged->PlayAnimation(Idle, true);
			}
			continue;
		}
		const FVector Step = To.GetSafeNormal2D() * C.Speed * DeltaSeconds;
		// Figures stand on the ground and face where they walk (kit figures face +Y in Unreal).
		const FVector Next = Here + Step;
		Who->SetRelativeLocation(C.bFigure ? FVector(Next.X, Next.Y, 0.0) : Next);
		if (C.bFigure)
		{
			Who->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Step.Y, Step.X)) - 90.f, 0.f));
		}
		if (C.Rigged) C.Rigged->SetPlayRate(C.Speed / 140.f);  // one stride a second at 140 cm/s: the feet keep their footing
	}
}
