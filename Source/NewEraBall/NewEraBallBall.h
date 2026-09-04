// NewEraBall - Match ball
// Physics-simulated ball with contact detection. Every touch is attributed to a player and
// reported to the GameMode (possession, restarts) and to the player (2-touch rule). The ball
// also detects goals and out-of-play geometrically from the GameMode's pitch dimensions, so
// no trigger volumes are required. Physics only, no randomness.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NewEraBallTypes.h"
#include "NewEraBallBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ANewEraBallBall;
class ANewEraBallPlayer;
class ANewEraBallGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBallTouched, ANewEraBallBall*, Ball, ANewEraBallPlayer*, Player, EBallTouchType, TouchType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBallKicked, ANewEraBallBall*, Ball, ANewEraBallPlayer*, Player, FVector, Velocity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBallControlChanged, ANewEraBallBall*, Ball, ANewEraBallPlayer*, NewController);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallFrozenChanged, bool, bFrozen);

UCLASS()
class NEWERABALL_API ANewEraBallBall : public AActor
{
	GENERATED_BODY()

public:
	ANewEraBallBall();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** Physics body and collision. Root component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<USphereComponent> Collision;

	/** Visual only. Assign the mesh in the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// ------------------------------------------------------------------
	// Physical tuning
	// ------------------------------------------------------------------

	/** Ball radius. 11 cm is a size 5 football. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Physics", meta = (ClampMin = "1", Units = "cm"))
	float BallRadius = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Physics", meta = (ClampMin = "0.01", Units = "kg"))
	float MassKg = 0.43f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Physics", meta = (ClampMin = "0"))
	float LinearDamping = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Physics", meta = (ClampMin = "0"))
	float AngularDamping = 0.8f;

	/** Hard cap on ball speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Physics", meta = (ClampMin = "100", Units = "cm/s"))
	float MaxSpeed = 4000.0f;

	// ------------------------------------------------------------------
	// Touch detection
	// ------------------------------------------------------------------

	/** Repeated physical contacts by the same player inside this window count as one touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Touches", meta = (ClampMin = "0", Units = "s"))
	float ContactDebounceSeconds = 0.25f;

	/** Detect goals and out-of-play from the GameMode pitch geometry. Disable to use trigger volumes instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Touches")
	bool bDetectBoundariesGeometrically = true;

	/** Register with the GameMode on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Touches")
	bool bAutoRegisterWithGameMode = true;

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	ETeamSide LastTouchTeam = ETeamSide::None;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	FVector LastTouchLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	EBallTouchType LastTouchType = EBallTouchType::Contact;

	/** World time of the last registered touch. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	float LastTouchTime = -1.0f;

	/** True while the ball is dead and held in place. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	bool bFrozen = false;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	ANewEraBallPlayer* GetLastTouchPlayer() const { return LastTouchPlayer.Get(); }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	ETeamSide GetLastTouchTeam() const { return LastTouchTeam; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	FVector GetLastTouchLocation() const { return LastTouchLocation; }

	/** Player currently keeping the ball under close control, if any. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	ANewEraBallPlayer* GetControllingPlayer() const { return ControllingPlayer.Get(); }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	bool IsFrozen() const { return bFrozen; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	float GetBallRadius() const { return BallRadius; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	FVector GetBallVelocity() const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|State")
	float GetBallSpeed() const { return GetBallVelocity().Size(); }

	// ------------------------------------------------------------------
	// Control
	// ------------------------------------------------------------------

	/** Place the ball resting on the ground at a location, stop it, and hold it dead. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	void PlaceAt(FVector GroundLocation);

	/** Stop the ball where it is and hold it dead. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	void FreezeInPlace();

	/** Let the ball simulate again. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	void Release();

	/**
	 * Strike the ball. Sets its velocity outright, drops any close control and registers a touch of
	 * the given type for the kicker. Ignored if the rules do not allow the kicker to play the ball.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	bool Kick(ANewEraBallPlayer* Kicker, FVector Velocity, EBallTouchType KickType);

	/**
	 * Report a non-physics touch (control gained, dribble touch, dribble move). Returns false if the
	 * touch was refused by the rules or filtered as a duplicate contact.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	bool NotifyTouch(ANewEraBallPlayer* Player, EBallTouchType TouchType);

	/** Claim or clear close control. Used by UDribblingComponent. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	void SetControllingPlayer(ANewEraBallPlayer* Player);

	/** Set the ball's horizontal velocity while under close control, keeping its vertical velocity. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Control")
	void SetControlVelocity(FVector HorizontalVelocity);

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallTouched OnBallTouched;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallKicked OnBallKicked;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallControlChanged OnControlChanged;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallFrozenChanged OnFrozenChanged;

protected:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Shared touch bookkeeping. Returns false when the touch is refused or filtered. */
	bool RegisterTouch(ANewEraBallPlayer* Player, EBallTouchType TouchType, const FVector& Location);

	/** Geometric goal / out-of-play test against the GameMode pitch. */
	void CheckBoundaries();

	void ClampSpeed();

	ANewEraBallGameMode* GetNewEraBallGameMode() const;

	TWeakObjectPtr<ANewEraBallPlayer> LastTouchPlayer;
	TWeakObjectPtr<ANewEraBallPlayer> ControllingPlayer;

	/** Set once a boundary event has been reported; cleared when the ball is placed or released. */
	bool bBoundaryReported = false;
};
