// NewEraBall - Dribbling component
// The mechanical centrepiece. Keeps the ball under close control at the owning player's feet,
// executes the dribble move set (direction cuts, speed changes, feints, body fakes), detects when
// a defender has been beaten, and hands out the resulting momentum burst and recovery delay.
// Everything here is driven by input and geometry. There is no randomness.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NewEraBallTypes.h"
#include "DribblingComponent.generated.h"

class ANewEraBallPlayer;
class ANewEraBallBall;
class ANewEraBallGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBallControlEvent, ANewEraBallPlayer*, Player, ANewEraBallBall*, Ball);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDribbleMoveStarted, ANewEraBallPlayer*, Player, EDribbleMove, Move, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDribbleMoveEnded, ANewEraBallPlayer*, Player, EDribbleMove, Move);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenderBeaten, ANewEraBallPlayer*, Attacker, ANewEraBallPlayer*, Defender);

/** Per-defender bookkeeping used by beat detection. */
struct FDefenderTrack
{
	/** World time the defender was last close and in front of the ball carrier. */
	float LastTimeInFront = -1000.0f;

	/** World time before which this defender cannot be counted as beaten again. */
	float BeatCooldownUntil = -1000.0f;
};

UCLASS(ClassGroup = (NewEraBall), meta = (BlueprintSpawnableComponent))
class NEWERABALL_API UDribblingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDribblingComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	// ------------------------------------------------------------------
	// Close control tuning
	// ------------------------------------------------------------------

	/** Horizontal distance from the player within which a loose ball is brought under control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "10", Units = "cm"))
	float ControlRadius = 115.0f;

	/** Horizontal distance beyond which control is lost. Must exceed ControlRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "10", Units = "cm"))
	float LoseControlRadius = 210.0f;

	/** Where the ball is kept while dribbling, measured ahead of the player along the heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "10", Units = "cm"))
	float ControlDistance = 72.0f;

	/** Ball is kept this far ahead while standing still (tighter than when running). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "10", Units = "cm"))
	float IdleControlDistance = 55.0f;

	/** How aggressively the ball is steered to its target point (per second). Higher is tighter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0.1"))
	float ControlStiffness = 14.0f;

	/** Cap on the corrective speed applied to the ball while under control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxControlSpeed = 1600.0f;

	/** Ball centre may be at most this far above the player's feet to be controllable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0", Units = "cm"))
	float MaxControlHeight = 60.0f;

	/** A ball travelling faster than this cannot be trapped; it must be slowed by contact first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxBallSpeedToGainControl = 2600.0f;

	/** While dribbling on the move, a touch is registered this often for the 2-touch rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0.05", Units = "s"))
	float DribbleTouchInterval = 0.5f;

	/** Player must be moving faster than this for dribble touches to be counted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0", Units = "cm/s"))
	float MinSpeedForDribbleTouch = 60.0f;

	/** A player closer to the ball than its current controller by this margin wins it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Close Control", meta = (ClampMin = "0", Units = "cm"))
	float StealDistanceMargin = 15.0f;

	// ------------------------------------------------------------------
	// Dribble moves
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0.05", Units = "s"))
	float DirectionCutDuration = 0.32f;

	/** Speed the ball is knocked at when a direction cut starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0", Units = "cm/s"))
	float DirectionCutKnockSpeed = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0.05", Units = "s"))
	float SpeedChangeDuration = 0.45f;

	/** Ball is pushed this many times further ahead during an explosive speed change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "1"))
	float SpeedChangePushMultiplier = 2.2f;

	/** Player speed multiplier during a speed change burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0.5"))
	float SpeedChangeBurstMultiplier = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0.05", Units = "s"))
	float FeintDuration = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0.05", Units = "s"))
	float BodyFakeDuration = 0.34f;

	/** Ball is knocked away from the faked direction at this speed when a body fake ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0", Units = "cm/s"))
	float BodyFakeExitKnockSpeed = 520.0f;

	/** Minimum gap between two moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Moves", meta = (ClampMin = "0", Units = "s"))
	float MoveCooldown = 0.2f;

	// ------------------------------------------------------------------
	// Beating defenders, momentum and recovery
	// ------------------------------------------------------------------

	/** A defender within this distance and in front of the carrier is a candidate to be beaten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Beat Detection", meta = (ClampMin = "0", Units = "cm"))
	float BeatDetectionRadius = 260.0f;

	/** The defender must have been in front within this many seconds of ending up behind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Beat Detection", meta = (ClampMin = "0.1", Units = "s"))
	float BeatWindowSeconds = 1.0f;

	/** The same defender cannot be counted as beaten again until this has elapsed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Beat Detection", meta = (ClampMin = "0", Units = "s"))
	float BeatCooldownSeconds = 2.0f;

	/** Cosine threshold for "in front of" the carrier's heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Beat Detection", meta = (ClampMin = "-1", ClampMax = "1"))
	float InFrontDotThreshold = 0.3f;

	/** Cosine threshold for "behind" the carrier's heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Beat Detection", meta = (ClampMin = "-1", ClampMax = "1"))
	float BehindDotThreshold = -0.2f;

	/** Speed multiplier granted to the attacker after beating a defender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Momentum", meta = (ClampMin = "1"))
	float MomentumBurstMultiplier = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Momentum", meta = (ClampMin = "0", Units = "s"))
	float MomentumBurstDuration = 1.2f;

	/** How long the beaten defender is slowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Momentum", meta = (ClampMin = "0", Units = "s"))
	float RecoveryDelayDuration = 0.7f;

	/** Speed multiplier applied to the beaten defender while recovering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Momentum", meta = (ClampMin = "0", ClampMax = "1"))
	float RecoverySpeedMultiplier = 0.45f;

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	EDribbleMove ActiveMove = EDribbleMove::None;

	/** World-space direction of the active move (flattened, normalised). */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	FVector ActiveMoveDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	float ActiveMoveTimeRemaining = 0.0f;

	/** Seconds of uninterrupted close control. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	float ControlDuration = 0.0f;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	bool IsControllingBall() const { return bControlling && ControlledBall.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	ANewEraBallBall* GetControlledBall() const { return bControlling ? ControlledBall.Get() : nullptr; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	EDribbleMove GetActiveMove() const { return ActiveMove; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	bool IsPerformingMove() const { return ActiveMove != EDribbleMove::None; }

	/** True when a move could start right now (has the ball, not on cooldown, not recovering). */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	bool CanPerformMove() const;

	/**
	 * Start a dribble move toward a world-space horizontal direction. Returns false if the move
	 * could not start. Direction falls back to the current heading when zero.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Dribbling")
	bool PerformMove(EDribbleMove Move, FVector2D Direction);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Dribbling")
	void CancelActiveMove();

	/** Drop close control and refuse to regain it for SuppressSeconds (used after kicks). */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Dribbling")
	void ReleaseControl(float SuppressSeconds);

	/** Fed each frame by the owning player: world-space movement input, flattened, at most unit length. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Dribbling")
	void SetMovementInput(FVector WorldInput) { MovementInput = FVector(WorldInput.X, WorldInput.Y, 0.0f); }

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Dribbling")
	void SetSprinting(bool bInSprinting) { bSprinting = bInSprinting; }

	/** Heading the ball is kept on: movement input when present, otherwise the player's facing. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	FVector GetHeading() const;

	/** Distance ahead of the player the ball is currently being held. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Dribbling")
	float GetCurrentControlDistance() const;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallControlEvent OnControlGained;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnBallControlEvent OnControlLost;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnDribbleMoveStarted OnMoveStarted;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnDribbleMoveEnded OnMoveEnded;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnDefenderBeaten OnDefenderBeaten;

protected:
	ANewEraBallPlayer* GetOwningPlayer() const;
	ANewEraBallGameMode* GetNewEraBallGameMode() const;
	ANewEraBallBall* FindMatchBall() const;

	/** Horizontal distance from the owner's feet to the ball centre. */
	float GetHorizontalDistanceToBall(const ANewEraBallBall* Ball) const;

	/** Whether a loose ball can be taken right now. */
	bool CanGainControl(const ANewEraBallBall* Ball) const;

	void GainControl(ANewEraBallBall* Ball);
	void LoseControl();

	/** Steer the controlled ball toward its target point. */
	void UpdateCloseControl(float DeltaTime);

	/** Advance the active move and finish it when its time is up. */
	void UpdateActiveMove(float DeltaTime);

	/** Look for defenders who were in front and are now behind. */
	void UpdateBeatDetection();

	/** Move duration lookup. */
	float GetMoveDuration(EDribbleMove Move) const;

	/** Knock the ball in a direction at a speed, keeping control. */
	void KnockBall(const FVector& Direction, float Speed);

	TWeakObjectPtr<ANewEraBallBall> ControlledBall;
	bool bControlling = false;

	/** World time before which control cannot be regained. */
	float ControlSuppressedUntil = -1.0f;

	/** Accumulator for dribble touch registration. */
	float DribbleTouchTimer = 0.0f;

	float MoveCooldownRemaining = 0.0f;

	/** Ball location when the active move began. Feints and body fakes hold the ball here. */
	FVector MoveAnchor = FVector::ZeroVector;

	/** True when the active speed change is an acceleration push rather than a check. */
	bool bSpeedChangeIsPush = true;

	FVector MovementInput = FVector::ZeroVector;
	bool bSprinting = false;

	/** Beat detection bookkeeping per opponent. */
	TMap<TWeakObjectPtr<ANewEraBallPlayer>, FDefenderTrack> DefenderTracks;
};
