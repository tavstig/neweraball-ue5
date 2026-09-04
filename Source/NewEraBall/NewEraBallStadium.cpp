// NewEraBall - Stadium and pitch environment implementation

#include "NewEraBallStadium.h"
#include "NewEraBall.h"
#include "NewEraBallGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ANewEraBallStadium::ANewEraBallStadium()
{
	PrimaryActorTick.bCanEverTick = false;

	StadiumRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StadiumRoot"));
	SetRootComponent(StadiumRoot);
	// Every generated mesh component is Static; a Static child can only attach under a Static parent.
	StadiumRoot->SetMobility(EComponentMobility::Static);

	// Engine-native BasicShapes: bundled with every UE install, no plugin or import required.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = CubeFinder.Object;
	PlaneMesh = PlaneFinder.Object;
	CylinderMesh = CylinderFinder.Object;

	// A safe grey placeholder so the stadium is never fully unlit-magenta before real materials are assigned.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlaceholderFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* Placeholder = PlaceholderFinder.Object;
	GrassMaterial = Placeholder;
	LineMaterial = Placeholder;
	GoalFrameMaterial = Placeholder;
	StandMaterial = Placeholder;
	StandFasciaMaterial = Placeholder;
	LightRigMaterial = Placeholder;
}

void ANewEraBallStadium::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildStadium();
}

void ANewEraBallStadium::BeginPlay()
{
	Super::BeginPlay();
	// Idempotent safety net: guarantees geometry exists even if OnConstruction did not run for this instance.
	RebuildStadium();
}

#if WITH_EDITOR
void ANewEraBallStadium::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildStadium();
}
#endif

ANewEraBallGameMode* ANewEraBallStadium::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

void ANewEraBallStadium::SyncPitchFromGameMode()
{
	if (const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		Pitch = GameMode->Pitch;
		RebuildStadium();
	}
	else
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("%s: no NewEraBall GameMode in this world to sync pitch dimensions from."), *GetNameSafe(this));
	}
}

// ----------------------------------------------------------------------
// Low level primitive helpers
// ----------------------------------------------------------------------

UStaticMeshComponent* ANewEraBallStadium::AddBox(FName BaseName, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, const FVector& FullSizeCm)
{
	if (!Mesh)
	{
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), BaseName);
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, UniqueName, RF_Transient);
	Comp->SetStaticMesh(Mesh);
	if (Material)
	{
		Comp->SetMaterial(0, Material);
	}
	Comp->SetMobility(EComponentMobility::Static);
	Comp->SetupAttachment(StadiumRoot);
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeRotation(Rotation);
	// Every BasicShapes mesh is authored at a 100 uu unit size, so the scale is simply the target size in cm / 100.
	Comp->SetRelativeScale3D(FullSizeCm / 100.0f);
	Comp->RegisterComponent();

	GeneratedMeshComponents.Add(Comp);
	return Comp;
}

UStaticMeshComponent* ANewEraBallStadium::AddCylinder(FName BaseName, UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, float DiameterCm, float HeightCm)
{
	return AddBox(BaseName, CylinderMesh, Material, Location, Rotation, FVector(DiameterCm, DiameterCm, HeightCm));
}

void ANewEraBallStadium::BuildLineSegment(const FVector& From, const FVector& To, float Width, FName BaseName)
{
	const FVector FromFlat(From.X, From.Y, 0.0f);
	const FVector ToFlat(To.X, To.Y, 0.0f);
	const FVector Delta = ToFlat - FromFlat;
	const float Length = Delta.Size();
	if (Length < 1.0f)
	{
		return;
	}

	FVector Mid = (FromFlat + ToFlat) * 0.5f;
	Mid.Z = LineHeightOffset + LineThickness * 0.5f;

	const FRotator Rotation = FRotationMatrix::MakeFromX(Delta / Length).Rotator();
	UStaticMeshComponent* Comp = AddBox(BaseName, CubeMesh, LineMaterial, Mid, Rotation, FVector(Length, Width, LineThickness));
	if (Comp)
	{
		// Decorative surface marking only; it must not trip up players or deflect the ball.
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ANewEraBallStadium::BuildArc(const FVector& Center, float Radius, float StartDegrees, float EndDegrees, int32 Segments, float Width, FName BaseName)
{
	Segments = FMath::Max(1, Segments);

	auto PointOnArc = [&Center, Radius](float Degrees) -> FVector
	{
		const float Rad = FMath::DegreesToRadians(Degrees);
		return Center + FVector(Radius * FMath::Cos(Rad), Radius * FMath::Sin(Rad), 0.0f);
	};

	FVector Prev = PointOnArc(StartDegrees);
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Segments);
		const FVector Next = PointOnArc(FMath::Lerp(StartDegrees, EndDegrees, Alpha));
		BuildLineSegment(Prev, Next, Width, BaseName);
		Prev = Next;
	}
}

// ----------------------------------------------------------------------
// Build
// ----------------------------------------------------------------------

void ANewEraBallStadium::RebuildStadium()
{
	if (!CubeMesh || !PlaneMesh || !CylinderMesh)
	{
		UE_LOG(LogNewEraBall, Error, TEXT("%s: BasicShapes meshes failed to load; cannot build stadium geometry."), *GetNameSafe(this));
		return;
	}

	ClearGeneratedComponents();

	BuildPitchSurface();
	BuildLineMarkings();
	BuildGoalFrames();
	BuildStands();
	BuildCornerLightRigs();
}

void ANewEraBallStadium::ClearGeneratedComponents()
{
	for (UStaticMeshComponent* Comp : GeneratedMeshComponents)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	GeneratedMeshComponents.Reset();

	for (USpotLightComponent* Light : GeneratedLightComponents)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	GeneratedLightComponents.Reset();
}

void ANewEraBallStadium::BuildPitchSurface()
{
	// The Z size is cosmetic: the Plane mesh is flat, so scaling its negligible thickness is invisible.
	AddBox(TEXT("PitchSurface"), PlaneMesh, GrassMaterial, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Pitch.Length, Pitch.Width, 1.0f));
}

void ANewEraBallStadium::BuildLineMarkings()
{
	const float HalfLength = Pitch.GetHalfLength();
	const float HalfWidth = Pitch.GetHalfWidth();

	// Outer boundary: two touchlines, two goal lines.
	BuildLineSegment(FVector(-HalfLength, -HalfWidth, 0.0f), FVector(HalfLength, -HalfWidth, 0.0f), LineWidth, TEXT("Touchline"));
	BuildLineSegment(FVector(-HalfLength, HalfWidth, 0.0f), FVector(HalfLength, HalfWidth, 0.0f), LineWidth, TEXT("Touchline"));
	BuildLineSegment(FVector(-HalfLength, -HalfWidth, 0.0f), FVector(-HalfLength, HalfWidth, 0.0f), LineWidth, TEXT("GoalLine"));
	BuildLineSegment(FVector(HalfLength, -HalfWidth, 0.0f), FVector(HalfLength, HalfWidth, 0.0f), LineWidth, TEXT("GoalLine"));

	// Halfway (midfield) line.
	BuildLineSegment(FVector(0.0f, -HalfWidth, 0.0f), FVector(0.0f, HalfWidth, 0.0f), LineWidth, TEXT("HalfwayLine"));

	// Centre spot and centre circle.
	UStaticMeshComponent* CentreSpot = AddCylinder(TEXT("CentreSpot"), LineMaterial, FVector(0.0f, 0.0f, LineHeightOffset + LineThickness * 0.5f), FRotator::ZeroRotator, SpotMarkingRadius * 2.0f, LineThickness);
	if (CentreSpot)
	{
		CentreSpot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	BuildArc(FVector::ZeroVector, CentreCircleRadius, 0.0f, 360.0f, CentreCircleSegments, LineWidth, TEXT("CentreCircle"));

	// Goal areas and penalty spots, one set per end.
	for (float EndSign : { -1.0f, 1.0f })
	{
		const float GoalLineX = EndSign * HalfLength;
		const float InnerX = GoalLineX - EndSign * Pitch.GoalAreaDepth;
		const float HalfGoalAreaWidth = Pitch.GoalAreaWidth * 0.5f;

		BuildLineSegment(FVector(GoalLineX, -HalfGoalAreaWidth, 0.0f), FVector(InnerX, -HalfGoalAreaWidth, 0.0f), LineWidth, TEXT("GoalAreaSide"));
		BuildLineSegment(FVector(GoalLineX, HalfGoalAreaWidth, 0.0f), FVector(InnerX, HalfGoalAreaWidth, 0.0f), LineWidth, TEXT("GoalAreaSide"));
		BuildLineSegment(FVector(InnerX, -HalfGoalAreaWidth, 0.0f), FVector(InnerX, HalfGoalAreaWidth, 0.0f), LineWidth, TEXT("GoalAreaBack"));

		const float PenaltyX = GoalLineX - EndSign * Pitch.PenaltySpotDistance;
		UStaticMeshComponent* PenaltySpot = AddCylinder(TEXT("PenaltySpot"), LineMaterial, FVector(PenaltyX, 0.0f, LineHeightOffset + LineThickness * 0.5f), FRotator::ZeroRotator, SpotMarkingRadius * 2.0f, LineThickness);
		if (PenaltySpot)
		{
			PenaltySpot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Corner arcs: a quarter circle at each of the four corners, bulging into the pitch.
	for (float SignX : { -1.0f, 1.0f })
	{
		for (float SignY : { -1.0f, 1.0f })
		{
			float StartDegrees;
			float EndDegrees;
			if (SignX > 0.0f && SignY > 0.0f)		{ StartDegrees = 180.0f; EndDegrees = 270.0f; }
			else if (SignX > 0.0f && SignY < 0.0f)	{ StartDegrees = 90.0f;  EndDegrees = 180.0f; }
			else if (SignX < 0.0f && SignY > 0.0f)	{ StartDegrees = 270.0f; EndDegrees = 360.0f; }
			else									{ StartDegrees = 0.0f;   EndDegrees = 90.0f;  }

			const FVector Corner(SignX * HalfLength, SignY * HalfWidth, 0.0f);
			BuildArc(Corner, CornerArcRadius, StartDegrees, EndDegrees, CornerArcSegments, LineWidth, TEXT("CornerArc"));
		}
	}
}

void ANewEraBallStadium::BuildGoalFrame(float EndSign, FName BaseName)
{
	const FString Prefix = BaseName.ToString();
	const float GoalLineX = EndSign * Pitch.GetHalfLength();
	const float HalfGoalWidth = Pitch.GoalWidth * 0.5f;

	AddCylinder(FName(Prefix + TEXT("_PostA")), GoalFrameMaterial, FVector(GoalLineX, -HalfGoalWidth, Pitch.GoalHeight * 0.5f), FRotator::ZeroRotator, GoalPostDiameter, Pitch.GoalHeight);
	AddCylinder(FName(Prefix + TEXT("_PostB")), GoalFrameMaterial, FVector(GoalLineX, HalfGoalWidth, Pitch.GoalHeight * 0.5f), FRotator::ZeroRotator, GoalPostDiameter, Pitch.GoalHeight);

	// The crossbar cylinder's local Z axis (its height axis) is rotated to point along world Y.
	const FRotator CrossbarRotation = FRotationMatrix::MakeFromZ(FVector(0.0f, 1.0f, 0.0f)).Rotator();
	AddCylinder(FName(Prefix + TEXT("_Crossbar")), GoalFrameMaterial, FVector(GoalLineX, 0.0f, Pitch.GoalHeight), CrossbarRotation, GoalPostDiameter, Pitch.GoalWidth);
}

void ANewEraBallStadium::BuildGoalFrames()
{
	BuildGoalFrame(1.0f, TEXT("GoalEast"));
	BuildGoalFrame(-1.0f, TEXT("GoalWest"));
}

void ANewEraBallStadium::BuildStandRun(const FVector& CentreLocation, const FVector& InwardDirection, float RunLength, FName BaseName)
{
	const FVector Inward = InwardDirection.GetSafeNormal2D();
	const FVector Outward = -Inward;
	// MakeFromX keeps a horizontal input's local Z aligned with world up, so local X = Inward (depth),
	// local Y = the perpendicular run axis, local Z = height: exactly what FullSizeCm below expects.
	const FRotator TierRotation = FRotationMatrix::MakeFromX(Inward).Rotator();
	const FString Prefix = BaseName.ToString();

	for (int32 Tier = 0; Tier < StandTierCount; ++Tier)
	{
		const float FrontDistance = StandFrontGap + Tier * StandTierDepth;
		const float CentreDistance = FrontDistance + StandTierDepth * 0.5f;
		const float CentreHeight = Tier * StandTierHeight + StandTierHeight * 0.5f;
		const FVector TierCentre = CentreLocation + Outward * CentreDistance + FVector(0.0f, 0.0f, CentreHeight);

		AddBox(FName(FString::Printf(TEXT("%s_Tier%d"), *Prefix, Tier)), CubeMesh, StandMaterial, TierCentre, TierRotation, FVector(StandTierDepth, RunLength, StandTierHeight));
	}

	if (StandFasciaHeight > 0.0f)
	{
		const FVector FasciaCentre = CentreLocation + Outward * StandFrontGap + FVector(0.0f, 0.0f, StandFasciaHeight * 0.5f);
		AddBox(FName(Prefix + TEXT("_Fascia")), CubeMesh, StandFasciaMaterial, FasciaCentre, TierRotation, FVector(6.0f, RunLength, StandFasciaHeight));
	}
}

void ANewEraBallStadium::BuildStands()
{
	const float HalfLength = Pitch.GetHalfLength();
	const float HalfWidth = Pitch.GetHalfWidth();

	// Stands run slightly short of the full touchline/goal-line length, leaving an open gap at each corner.
	const float SideRunLength = FMath::Max(100.0f, 2.0f * (HalfLength - StandCornerGap));
	const float EndRunLength = FMath::Max(100.0f, 2.0f * (HalfWidth - StandCornerGap));

	BuildStandRun(FVector(0.0f, -HalfWidth, 0.0f), FVector(0.0f, 1.0f, 0.0f), SideRunLength, TEXT("StandSouth"));
	BuildStandRun(FVector(0.0f, HalfWidth, 0.0f), FVector(0.0f, -1.0f, 0.0f), SideRunLength, TEXT("StandNorth"));
	BuildStandRun(FVector(-HalfLength, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), EndRunLength, TEXT("StandWest"));
	BuildStandRun(FVector(HalfLength, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), EndRunLength, TEXT("StandEast"));
}

void ANewEraBallStadium::BuildCornerLightRig(float SignX, float SignY, FName BaseName)
{
	const float HalfLength = Pitch.GetHalfLength();
	const float HalfWidth = Pitch.GetHalfWidth();
	const FString Prefix = BaseName.ToString();

	const FVector Corner(SignX * HalfLength, SignY * HalfWidth, 0.0f);
	const FVector OutwardDiagonal = FVector(SignX, SignY, 0.0f).GetSafeNormal();
	const float StandDepthTotal = StandFrontGap + StandTierCount * StandTierDepth;
	const FVector MastBase = Corner + OutwardDiagonal * (StandDepthTotal + LightRigOutset);

	AddCylinder(FName(Prefix + TEXT("_Mast")), LightRigMaterial, MastBase + FVector(0.0f, 0.0f, LightMastHeight * 0.5f), FRotator::ZeroRotator, LightMastDiameter, LightMastHeight);

	// The rig head's long axis (local X) is turned to face the pitch centre.
	const FVector ToCentre = (-MastBase).GetSafeNormal2D();
	const FRotator HeadRotation = FRotationMatrix::MakeFromX(ToCentre).Rotator();
	const FVector HeadCentre = MastBase + FVector(0.0f, 0.0f, LightMastHeight + LightRigHeadSize.Z * 0.5f);
	AddBox(FName(Prefix + TEXT("_Head")), CubeMesh, LightRigMaterial, HeadCentre, HeadRotation, LightRigHeadSize);

	const FName UniqueLightName = MakeUniqueObjectName(this, USpotLightComponent::StaticClass(), BaseName);
	USpotLightComponent* Spot = NewObject<USpotLightComponent>(this, UniqueLightName, RF_Transient);
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetupAttachment(StadiumRoot);
	Spot->SetRelativeLocation(HeadCentre);

	FRotator AimRotation = HeadRotation;
	AimRotation.Pitch -= SpotLightDownwardTiltDegrees;
	Spot->SetRelativeRotation(AimRotation);

	Spot->Intensity = SpotLightIntensity;
	Spot->SetLightColor(SpotLightColor);
	Spot->OuterConeAngle = SpotLightOuterConeAngle;
	Spot->InnerConeAngle = SpotLightOuterConeAngle * 0.5f;
	Spot->SetAttenuationRadius(SpotLightAttenuationRadius);
	Spot->SetCastShadows(false); // Visual fill from four rigs; shadowing is left to the main directional light.
	Spot->RegisterComponent();

	GeneratedLightComponents.Add(Spot);
}

void ANewEraBallStadium::BuildCornerLightRigs()
{
	if (!bBuildLightingRigs)
	{
		return;
	}

	BuildCornerLightRig(-1.0f, -1.0f, TEXT("CornerRigSW"));
	BuildCornerLightRig(-1.0f, 1.0f, TEXT("CornerRigNW"));
	BuildCornerLightRig(1.0f, -1.0f, TEXT("CornerRigSE"));
	BuildCornerLightRig(1.0f, 1.0f, TEXT("CornerRigNE"));
}
