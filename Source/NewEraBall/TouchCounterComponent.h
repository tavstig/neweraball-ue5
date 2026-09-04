// NewEraBall - Touch counter component
// Enforces the 2-touch rule for the owning player. Behind the midfield line every player,
// goalkeeper included, may touch the ball at most MaxTouches times. Exceeding the limit in open
// play is an automatic turnover; exceeding it inside the player's own goal area is a penalty to
// the opposing team. The GameMode adjudicates; this component counts and reports.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NewEraBallTypes.h"
#include "TouchCounterComponent.generated.h"

class ANewEraBallPlayer;
class ANewEraBallGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnTouchRegistered, ANewEraBallPlayer*, Player, int32, TouchCount, int32, MaxTouches, bool, bRestricted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTouchViolation, ANewEraBallPlayer*, Player, ETouchViolationResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTouchesReset, ANewEraBallPlayer*, Player);

UCLASS(ClassGroup = (NewEraBall), meta = (BlueprintSpawnableComponent))
class NEWERABALL_API UTouchCounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTouchCounterComponent();

	/** Overrides the GameMode's MaxTouchesBehindMidfield when greater than zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Touch Rule", meta = (ClampMin = "0"))
	int32 MaxTouchesOverride = 0;

	/** Touches taken in the opposing half are unrestricted and clear the count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Touch Rule")
	bool bResetWhenBallCrossesMidfield = true;

	/** Touches taken behind the midfield line since the last reset. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Touch Rule")
	int32 TouchCount = 0;

	/** Whether the last registered touch was subject to the restriction. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Touch Rule")
	bool bLastTouchRestricted = false;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Touch Rule")
	FVector LastTouchLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|Touch Rule")
	EBallTouchType LastTouchType = EBallTouchType::Contact;

	/**
	 * Register a touch at a ball location. Counts it when the ball is behind the owning player's
	 * midfield line and asks the GameMode to adjudicate when the count exceeds the limit.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Touch Rule")
	ETouchViolationResult RegisterTouch(FVector BallLocation, EBallTouchType TouchType);

	/** Clear the count. Called when possession changes, play restarts, or the ball crosses midfield. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Touch Rule")
	void ResetTouches();

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Touch Rule")
	int32 GetTouchCount() const { return TouchCount; }

	/** Effective limit: the override when set, otherwise the GameMode rule. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Touch Rule")
	int32 GetMaxTouches() const;

	/** Touches left before the next one becomes a violation. Only meaningful behind midfield. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Touch Rule")
	int32 GetRemainingTouches() const { return FMath::Max(0, GetMaxTouches() - TouchCount); }

	/** True when a touch at this location would be counted against the limit. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Touch Rule")
	bool IsLocationRestricted(FVector BallLocation) const;

	/** True when the next restricted touch would be a violation. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Touch Rule")
	bool IsAtLimit() const { return TouchCount >= GetMaxTouches(); }

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTouchRegistered OnTouchRegistered;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTouchViolation OnTouchViolation;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTouchesReset OnTouchesReset;

protected:
	ANewEraBallPlayer* GetOwningPlayer() const;
	ANewEraBallGameMode* GetNewEraBallGameMode() const;
};
