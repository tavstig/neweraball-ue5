// NewEraBall - Broadcast camera
// A fixed TV-style main camera mounted high on one touchline at the halfway line. It never moves;
// it pans, tilts and zooms to follow the ball with look-ahead and smoothing, the way a broadcast
// operator would. Takes over the local player's view so gameplay is seen from the stands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NewEraBallCamera.generated.h"

class UCameraComponent;
class USceneComponent;
class APlayerController;
class ANewEraBallGameMode;

UCLASS()
class NEWERABALL_API ANewEraBallCamera : public AActor
{
	GENERATED_BODY()

public:
	ANewEraBallCamera();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<UCameraComponent> Camera;

	// ------------------------------------------------------------------
	// Placement
	// ------------------------------------------------------------------

	/**
	 * Position the camera from the GameMode's pitch geometry on BeginPlay instead of where it was
	 * placed in the level: on the -Y touchline at the halfway line, SidelineDistance out and
	 * MountHeight up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Placement")
	bool bAutoPlaceFromPitch = true;

	/** Distance back from the touchline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Placement", meta = (ClampMin = "0", Units = "cm"))
	float SidelineDistance = 2600.0f;

	/** Height above the pitch surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Placement", meta = (ClampMin = "0", Units = "cm"))
	float MountHeight = 1500.0f;

	/** Mount on the +Y touchline instead of -Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Placement")
	bool bMountOnPositiveY = false;

	/** Take over local player 0's view on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Placement")
	bool bAutoActivateForLocalPlayer = true;

	// ------------------------------------------------------------------
	// Tracking
	// ------------------------------------------------------------------

	/** Actor to follow. Defaults to the match ball when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking")
	TObjectPtr<AActor> TrackedActor;

	/** Seconds of the target's velocity added ahead of it so the shot leads the play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking", meta = (ClampMin = "0", Units = "s"))
	float LookAheadSeconds = 0.35f;

	/** Aim this far above the target so the ball sits below centre frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking", meta = (Units = "cm"))
	float AimHeightOffset = 120.0f;

	/** Keep the aim point inside the pitch rectangle (with this margin) so the camera never chases a dead ball off the stage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking")
	bool bClampAimToPitch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking", meta = (Units = "cm"))
	float AimClampMargin = 600.0f;

	/** How quickly the camera pans and tilts toward the aim point. Higher is snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Tracking", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 4.5f;

	/** Field of view when the target is at or nearer than NearDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Zoom", meta = (ClampMin = "5", ClampMax = "170"))
	float NearFieldOfView = 55.0f;

	/** Field of view when the target is at or beyond FarDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Zoom", meta = (ClampMin = "5", ClampMax = "170"))
	float FarFieldOfView = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Zoom", meta = (ClampMin = "0", Units = "cm"))
	float NearDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Zoom", meta = (ClampMin = "0", Units = "cm"))
	float FarDistance = 6500.0f;

	/** How quickly the zoom follows its target value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Zoom", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 3.0f;

	// ------------------------------------------------------------------
	// API
	// ------------------------------------------------------------------

	/** Make this camera the view for a controller and stop it from auto-switching back to its pawn. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Camera")
	void ActivateForController(APlayerController* PlayerController, float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Camera")
	void SetTrackedActor(AActor* Actor);

	/** Recompute the mount position from the GameMode pitch. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Camera")
	void PlaceFromPitch();

	/** Point straight at the target with no smoothing. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Camera")
	void SnapToTarget();

	/** World point the camera is currently aiming for (after look-ahead and clamping). */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Camera")
	FVector GetAimPoint() const;

protected:
	AActor* ResolveTrackedActor() const;
	ANewEraBallGameMode* GetNewEraBallGameMode() const;

	/** Rotation and field of view the camera would have if it were pointed straight at the aim point. */
	void ComputeDesired(FRotator& OutRotation, float& OutFieldOfView) const;
};
