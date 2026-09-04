// NewEraBall - Player character
// Base class for every player on the pitch: outfield and goalkeeper, human and AI controlled.
// Owns the touch counter and dribbling components, applies momentum bursts and recovery delays
// to movement, and turns input into kicks. Input response is tuned to be tight and immediate.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NewEraBallTypes.h"
#include "NewEraBallPlayer.generated.h"

class UInputAction;
class UInputMappingContext;
class UTouchCounterComponent;
class UDribblingComponent;
class ANewEraBallBall;
class ANewEraBallGameMode;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerBallTouch, ANewEraBallPlayer*, Player, ANewEraBallBall*, Ball, EBallTouchType, TouchType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerKick, ANewEraBallPlayer*, Player, EBallTouchType, KickType, float, Power);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMomentumBurst, ANewEraBallPlayer*, Player, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRecoveryDelay, ANewEraBallPlayer*, Player, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStateCleared, ANewEraBallPlayer*, Player);

UCLASS()
class NEWERABALL_API ANewEraBallPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	ANewEraBallPlayer();

	//~ Begin AActor / APawn interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	//~ End AActor / APawn interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** Enforces the 2-touch rule for this player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<UTouchCounterComponent> TouchCounter;

	/** Close control, dribble moves, beat detection, momentum and defender recovery. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NewEraBall|Components")
	TObjectPtr<UDribblingComponent> Dribbling;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Components")
	UTouchCounterComponent* GetTouchCounter() const { return TouchCounter; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Components")
	UDribblingComponent* GetDribblingComponent() const { return Dribbling; }

	// ------------------------------------------------------------------
	// Identity
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Team")
	ETeamSide Team = ETeamSide::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Team")
	bool bIsGoalkeeper = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Team")
	int32 SquadNumber = 0;

	/** Formation slot assigned by the GameMode. Slot 0 is the goalkeeper. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Team")
	int32 FormationSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Team")
	FText PlayerName;

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Team")
	void SetTeam(ETeamSide NewTeam);

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team")
	ETeamSide GetTeam() const { return Team; }

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Team")
	void SetIsGoalkeeper(bool bInGoalkeeper) { bIsGoalkeeper = bInGoalkeeper; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team")
	bool IsGoalkeeper() const { return bIsGoalkeeper; }

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Team")
	void SetSquadNumber(int32 InNumber) { SquadNumber = InNumber; }

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Team")
	void SetFormationSlot(int32 InSlot) { FormationSlot = InSlot; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team")
	int32 GetFormationSlot() const { return FormationSlot; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team")
	bool IsOpponentOf(const ANewEraBallPlayer* Other) const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team")
	ANewEraBallGameMode* GetNewEraBallGameMode() const;

	// ------------------------------------------------------------------
	// Movement tuning
	// ------------------------------------------------------------------

	/** Top speed when not sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float BaseRunSpeed = 620.0f;

	/** Top speed while sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Movement", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 860.0f;

	/** Speed multiplier applied while the ball is under close control. 1.0 keeps full speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Movement", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float SpeedMultiplierWithBall = 0.95f;

	/** Movement input is taken relative to the camera view when true, or to the world axes when false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Movement")
	bool bCameraRelativeMovement = true;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Movement")
	bool bIsSprinting = false;

	/** Last non-zero world-space movement input direction (flattened, normalised). */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Movement")
	FVector LastMoveDirection = FVector::ForwardVector;

	/** Current world-space movement input (flattened). Zero when idle. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Movement")
	FVector CurrentMoveInput = FVector::ZeroVector;

	// ------------------------------------------------------------------
	// Momentum burst & recovery delay
	// ------------------------------------------------------------------

	/**
	 * Grant a temporary speed boost, typically after beating a defender. Multiplier scales top speed,
	 * Duration is in seconds. A fresh burst replaces any active one.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Momentum")
	void ApplyMomentumBurst(float SpeedMultiplier, float Duration);

	/**
	 * Slow this player for a short time, typically after being beaten. SpeedMultiplier scales top speed
	 * down while it lasts and dribble moves are unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Momentum")
	void ApplyRecoveryDelay(float Duration, float SpeedMultiplier);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Momentum")
	void ClearMomentumBurst();

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Momentum")
	void ClearRecoveryDelay();

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Momentum")
	bool HasMomentumBurst() const { return MomentumTimeRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Momentum")
	bool IsInRecovery() const { return RecoveryTimeRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Momentum")
	float GetMomentumTimeRemaining() const { return MomentumTimeRemaining; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Momentum")
	float GetRecoveryTimeRemaining() const { return RecoveryTimeRemaining; }

	/** Top speed after sprint, ball control, momentum and recovery are applied. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Momentum")
	float GetEffectiveMaxSpeed() const;

	// ------------------------------------------------------------------
	// Ball interaction
	// ------------------------------------------------------------------

	/** Horizontal reach from the player's centre within which the ball can be kicked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "10", Units = "cm"))
	float KickReach = 140.0f;

	/** Ball speed for a pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", Units = "cm/s"))
	float PassSpeed = 1400.0f;

	/** Upward component of a pass as a fraction of its direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", ClampMax = "1"))
	float PassLift = 0.03f;

	/** Ball speed for an uncharged shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", Units = "cm/s"))
	float MinShotSpeed = 1600.0f;

	/** Ball speed for a fully charged shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxShotSpeed = 2800.0f;

	/** Seconds of holding the shoot input needed to reach MaxShotSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0.05", Units = "s"))
	float ShotChargeSeconds = 0.8f;

	/** Upward component of a shot at zero charge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", ClampMax = "1"))
	float MinShotLift = 0.02f;

	/** Upward component of a shot at full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxShotLift = 0.18f;

	/** How long close control is suppressed after a kick so the ball can leave the feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Kicking", meta = (ClampMin = "0", Units = "s"))
	float PostKickControlSuppression = 0.35f;

	/** True while the shoot input is held and power is building. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Kicking")
	bool bIsChargingShot = false;

	/** 0..1 charge of the shot being held. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Kicking")
	float ShotCharge = 0.0f;

	/** Ball currently under this player's close control, if any. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Ball")
	ANewEraBallBall* GetControlledBall() const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Ball")
	bool HasBallControl() const;

	/** The match ball if it is within KickReach, otherwise null. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Ball")
	ANewEraBallBall* GetBallInReach() const;

	/** Direction a kick would travel right now: movement input if any, otherwise the facing direction. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Ball")
	FVector GetAimDirection() const;

	/**
	 * Kick the ball in reach. Direction is flattened; Lift adds an upward component. Returns false when
	 * no ball is in reach or the rules do not allow this player to play the ball.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Ball")
	bool KickBall(FVector Direction, float Speed, float Lift, EBallTouchType KickType);

	/** Pass along the aim direction at PassSpeed. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Ball")
	bool Pass();

	/** Shoot along the aim direction with the given 0..1 charge. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Ball")
	bool Shoot(float Charge);

	/** Called by the ball whenever this player touches it. Feeds the touch counter and broadcasts. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Ball")
	void NotifyBallTouch(ANewEraBallBall* Ball, EBallTouchType TouchType, FVector TouchLocation);

	/** Teleport to a transform, stop moving, drop the ball and clear momentum / recovery state. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Ball")
	void ResetForRestart(const FTransform& Transform);

	// ------------------------------------------------------------------
	// Input (Enhanced Input, engine built-in)
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	int32 MappingContextPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputAction> PassAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input")
	TObjectPtr<UInputAction> ShootAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input|Dribbling")
	TObjectPtr<UInputAction> DirectionCutAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input|Dribbling")
	TObjectPtr<UInputAction> SpeedChangeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input|Dribbling")
	TObjectPtr<UInputAction> FeintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Input|Dribbling")
	TObjectPtr<UInputAction> BodyFakeAction;

	/** Movement from controls, UI or AI. Right and Forward are relative to the movement frame. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoMove(float Right, float Forward);

	/** Move toward a world-space direction. Used by AI. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoMoveWorld(FVector WorldDirection, float Scale = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoSprint(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoPass();

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoShootStart();

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoShootRelease();

	/** Trigger a dribble move using the current movement input as its direction. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Input")
	virtual void DoDribbleMove(EDribbleMove Move);

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPlayerBallTouch OnBallTouched;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPlayerKick OnKicked;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnMomentumBurst OnMomentumBurstStarted;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPlayerStateCleared OnMomentumBurstEnded;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnRecoveryDelay OnRecoveryStarted;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPlayerStateCleared OnRecoveryEnded;

protected:
	/** Enhanced Input handlers. */
	void MoveInput(const FInputActionValue& Value);
	void MoveInputCompleted(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);
	void SprintStarted(const FInputActionValue& Value);
	void SprintCompleted(const FInputActionValue& Value);
	void PassInput(const FInputActionValue& Value);
	void ShootStarted(const FInputActionValue& Value);
	void ShootCompleted(const FInputActionValue& Value);
	void DirectionCutInput(const FInputActionValue& Value);
	void SpeedChangeInput(const FInputActionValue& Value);
	void FeintInput(const FInputActionValue& Value);
	void BodyFakeInput(const FInputActionValue& Value);

	/** Yaw of the frame movement input is expressed in. */
	float GetMovementFrameYaw() const;

	/** Push the effective top speed into the movement component. */
	void UpdateMovementSpeed();

	/** Register or unregister the mapping context on the owning local player. */
	void AddMappingContextToController();
	void RemoveMappingContextFromController();

	/** Timer state for momentum and recovery. */
	float MomentumTimeRemaining = 0.0f;
	float MomentumMultiplier = 1.0f;
	float RecoveryTimeRemaining = 0.0f;
	float RecoveryMultiplier = 1.0f;

	/** Seconds the shoot input has been held. */
	float ShotChargeElapsed = 0.0f;

	/** Movement input received this frame, consumed in Tick. */
	FVector PendingMoveInput = FVector::ZeroVector;
};
