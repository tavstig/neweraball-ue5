// NewEraBall - Stadium and pitch environment
// Procedurally builds a compact, Kings League / Baller League style stadium shell: a grass pitch
// with full line markings, two goal frames, four tiered stands close to the pitch, and an LED-style
// lighting rig on each corner. Everything is built from engine-native StaticMeshComponents (the
// built-in BasicShapes cube, plane and cylinder), so no ProceduralMesh plugin or imported meshes
// are required. Geometry rebuilds whenever a property changes, so it previews live in the editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NewEraBallTypes.h"
#include "NewEraBallStadium.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class USpotLightComponent;
class ANewEraBallGameMode;

UCLASS()
class NEWERABALL_API ANewEraBallStadium : public AActor
{
	GENERATED_BODY()

public:
	ANewEraBallStadium();

	//~ Begin AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<USceneComponent> StadiumRoot;

	// ------------------------------------------------------------------
	// Pitch geometry
	// ------------------------------------------------------------------

	/** Pitch dimensions in centimetres. Matches ANewEraBallGameMode's FPitchDimensions by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Pitch")
	FPitchDimensions Pitch;

	/** Copy Pitch from the in-world NewEraBall GameMode, if one exists, then rebuild. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NewEraBall|Pitch")
	void SyncPitchFromGameMode();

	// ------------------------------------------------------------------
	// Materials (Blueprint-ready slots; assign real Content assets to replace the placeholders)
	// ------------------------------------------------------------------

	/** Pitch grass surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** White pitch line markings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> LineMaterial;

	/** Goal posts and crossbar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> GoalFrameMaterial;

	/** Stand tiers (seating deck). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> StandMaterial;

	/** Stand front fascia board (accent band facing the pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> StandFasciaMaterial;

	/** Corner light rig mast and head housing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Materials")
	TObjectPtr<UMaterialInterface> LightRigMaterial;

	// ------------------------------------------------------------------
	// Line markings
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "1", Units = "cm"))
	float LineWidth = 12.0f;

	/** How far above the pitch surface the line geometry sits, to avoid z-fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "0", Units = "cm"))
	float LineHeightOffset = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "0", Units = "cm"))
	float LineThickness = 2.0f;

	/** Centre circle radius. 915 cm is the regulation 9.15 m radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "10", Units = "cm"))
	float CentreCircleRadius = 915.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "8", ClampMax = "128"))
	int32 CentreCircleSegments = 32;

	/** Corner arc radius. 100 cm is the regulation 1 m radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "10", Units = "cm"))
	float CornerArcRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "2", ClampMax = "32"))
	int32 CornerArcSegments = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lines", meta = (ClampMin = "1", Units = "cm"))
	float SpotMarkingRadius = 15.0f;

	// ------------------------------------------------------------------
	// Goal frames
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Goals", meta = (ClampMin = "1", Units = "cm"))
	float GoalPostDiameter = 12.0f;

	// ------------------------------------------------------------------
	// Stands
	// ------------------------------------------------------------------

	/** Tiers stacked per stand. Kept low for a compact, close-to-the-pitch look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "1", ClampMax = "20"))
	int32 StandTierCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "10", Units = "cm"))
	float StandTierHeight = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "10", Units = "cm"))
	float StandTierDepth = 140.0f;

	/** Gap between the pitch boundary and the front face of the first tier. Keep small for intimacy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "0", Units = "cm"))
	float StandFrontGap = 250.0f;

	/** Open gap left at each of the four corners between the side stand and the end stand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "0", Units = "cm"))
	float StandCornerGap = 400.0f;

	/** Height of the accent fascia board running along the front of the lowest tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Stands", meta = (ClampMin = "0", Units = "cm"))
	float StandFasciaHeight = 60.0f;

	// ------------------------------------------------------------------
	// Corner lighting rigs
	// ------------------------------------------------------------------

	/** Build the four corner light rigs (mast, head housing and a functional spotlight each). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs")
	bool bBuildLightingRigs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "10", Units = "cm"))
	float LightMastHeight = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "1", Units = "cm"))
	float LightMastDiameter = 30.0f;

	/** Full size (length along the mast-to-pitch axis, width, height) of the LED rig head housing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs")
	FVector LightRigHeadSize = FVector(220.0f, 90.0f, 45.0f);

	/** How far beyond the stand corner the mast is planted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "0", Units = "cm"))
	float LightRigOutset = 350.0f;

	/** Downward tilt of each corner spotlight from horizontal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float SpotLightDownwardTiltDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "1", ClampMax = "80", Units = "deg"))
	float SpotLightOuterConeAngle = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "0"))
	float SpotLightIntensity = 150000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs")
	FLinearColor SpotLightColor = FLinearColor(1.0f, 0.95f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Lighting Rigs", meta = (ClampMin = "10", Units = "cm"))
	float SpotLightAttenuationRadius = 9000.0f;

	// ------------------------------------------------------------------
	// Build
	// ------------------------------------------------------------------

	/** Destroy and regenerate every piece of stadium geometry from the current properties. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NewEraBall|Build")
	void RebuildStadium();

protected:
	void ClearGeneratedComponents();

	void BuildPitchSurface();
	void BuildLineMarkings();
	void BuildGoalFrames();
	void BuildStands();
	void BuildCornerLightRigs();

	/** One tiered stand along a straight run, facing InwardDirection, centred on CentreLocation. */
	void BuildStandRun(const FVector& CentreLocation, const FVector& InwardDirection, float RunLength, FName BaseName);

	/** One goal frame at the given end (+1 for the +X goal, -1 for the -X goal). */
	void BuildGoalFrame(float EndSign, FName BaseName);

	/** One corner light rig at a pitch corner (SignX, SignY each +1/-1). */
	void BuildCornerLightRig(float SignX, float SignY, FName BaseName);

	/** A straight thin box of pitch-line marking between two points, flush with the pitch. */
	void BuildLineSegment(const FVector& From, const FVector& To, float Width, FName BaseName);

	/** A circular arc of pitch-line marking approximated by straight chords. */
	void BuildArc(const FVector& Center, float Radius, float StartDegrees, float EndDegrees, int32 Segments, float Width, FName BaseName);

	/** Create, attach and register a StaticMeshComponent using CubeMesh, scaled to a full-size box in cm. */
	UStaticMeshComponent* AddBox(FName BaseName, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, const FVector& FullSizeCm);

	/** Create, attach and register a StaticMeshComponent using CylinderMesh (axis along local Z). */
	UStaticMeshComponent* AddCylinder(FName BaseName, UMaterialInterface* Material, const FVector& Location, const FRotator& Rotation, float DiameterCm, float HeightCm);

	ANewEraBallGameMode* GetNewEraBallGameMode() const;

	/** Engine-native BasicShapes meshes, found once in the constructor. Each is a 100x100(x100) uu unit shape. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	/** Every component created by RebuildStadium, so the next rebuild can clear them first. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GeneratedMeshComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USpotLightComponent>> GeneratedLightComponents;
};
