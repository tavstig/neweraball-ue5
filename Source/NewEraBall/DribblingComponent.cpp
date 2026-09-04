// NewEraBall - Dribbling component implementation

#include "DribblingComponent.h"
#include "NewEraBall.h"
#include "NewEraBallPlayer.h"
#include "NewEraBallBall.h"
#include "NewEraBallGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	/** Sideways component of Direction relative to Forward. Falls back to the right-hand side. */
	FVector ComputeLateral(const FVector& Direction, const FVector& Forward)
	{
		const FVector Flat = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
		const FVector FlatForward = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
		FVector Lateral = Flat - FlatForward * FVector::DotProduct(Flat, FlatForward);
		if (Lateral.SizeSquared() < 0.04f)
		{
			// Right vector in Unreal's left-handed frame.
			Lateral = FVector::CrossProduct(FVector::UpVector, FlatForward);
		}
		return Lateral.GetSafeNormal();
	}
}

UDribblingComponent::UDribblingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDribblingComponent::BeginPlay()
{
	Super::BeginPlay();
	LoseControlRadius = FMath::Max(LoseControlRadius, ControlRadius + 10.0f);
}

ANewEraBallPlayer* UDribblingComponent::GetOwningPlayer() const
{
	return Cast<ANewEraBallPlayer>(GetOwner());
}

ANewEraBallGameMode* UDribblingComponent::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

ANewEraBallBall* UDribblingComponent::FindMatchBall() const
{
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	return GameMode ? GameMode->MatchBall.Get() : nullptr;
}

void UDribblingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ANewEraBallPlayer* Owner = GetOwningPlayer();
	if (!Owner)
	{
		return;
	}

	if (MoveCooldownRemaining > 0.0f)
	{
		MoveCooldownRemaining -= DeltaTime;
	}

	UpdateActiveMove(DeltaTime);

	ANewEraBallBall* Ball = FindMatchBall();
	if (!Ball)
	{
		if (bControlling)
		{
			LoseControl();
		}
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	if (!bControlling)
	{
		if (Now >= ControlSuppressedUntil && CanGainControl(Ball))
		{
			GainControl(Ball);
		}
		return;
	}

	// Conditions under which close control ends.
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	const bool bRulesForbid = GameMode && !GameMode->CanPlayerPlayBall(Owner);
	const bool bStolen = Ball->GetControllingPlayer() != Owner;
	const bool bTooFar = GetHorizontalDistanceToBall(Ball) > LoseControlRadius;

	if (ControlledBall.Get() != Ball || Ball->IsFrozen() || bStolen || bTooFar || bRulesForbid)
	{
		LoseControl();
		return;
	}

	ControlDuration += DeltaTime;
	UpdateCloseControl(DeltaTime);

	if (bControlling)
	{
		UpdateBeatDetection();
	}
}

// ----------------------------------------------------------------------
// Control acquisition
// ----------------------------------------------------------------------

float UDribblingComponent::GetHorizontalDistanceToBall(const ANewEraBallBall* Ball) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Ball)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist2D(Owner->GetActorLocation(), Ball->GetActorLocation());
}

bool UDribblingComponent::CanGainControl(const ANewEraBallBall* Ball) const
{
	const ANewEraBallPlayer* Owner = GetOwningPlayer();
	if (!Owner || !Ball)
	{
		return false;
	}

	// A beaten defender cannot win the ball while recovering.
	if (Owner->IsInRecovery())
	{
		return false;
	}

	const float Distance = GetHorizontalDistanceToBall(Ball);
	if (Distance > ControlRadius)
	{
		return false;
	}

	const float FeetZ = Owner->GetActorLocation().Z - Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Ball->GetActorLocation().Z - FeetZ > MaxControlHeight + Ball->GetBallRadius())
	{
		return false;
	}

	if (Ball->GetBallSpeed() > MaxBallSpeedToGainControl)
	{
		return false;
	}

	if (const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		if (!GameMode->CanPlayerPlayBall(Owner))
		{
			return false;
		}
	}

	// Taking the ball off another player requires being clearly closer to it.
	if (const ANewEraBallPlayer* Holder = Ball->GetControllingPlayer())
	{
		if (Holder != Owner)
		{
			const float HolderDistance = FVector::Dist2D(Holder->GetActorLocation(), Ball->GetActorLocation());
			if (Distance + StealDistanceMargin >= HolderDistance)
			{
				return false;
			}
		}
	}

	return true;
}

void UDribblingComponent::GainControl(ANewEraBallBall* Ball)
{
	ANewEraBallPlayer* Owner = GetOwningPlayer();
	if (!Owner || !Ball)
	{
		return;
	}

	// The touch may be refused by the rules (wrong team at a restart, dead ball).
	if (!Ball->NotifyTouch(Owner, EBallTouchType::ControlGained))
	{
		return;
	}

	// A violation on this very touch can have killed the ball already.
	if (Ball->IsFrozen())
	{
		return;
	}

	ControlledBall = Ball;
	bControlling = true;
	ControlDuration = 0.0f;
	DribbleTouchTimer = 0.0f;
	DefenderTracks.Reset();

	Ball->SetControllingPlayer(Owner);
	OnControlGained.Broadcast(Owner, Ball);
}

void UDribblingComponent::LoseControl()
{
	ANewEraBallPlayer* Owner = GetOwningPlayer();
	ANewEraBallBall* Ball = ControlledBall.Get();

	bControlling = false;
	ControlledBall = nullptr;
	ControlDuration = 0.0f;
	DribbleTouchTimer = 0.0f;

	if (Ball && Owner && Ball->GetControllingPlayer() == Owner)
	{
		Ball->SetControllingPlayer(nullptr);
	}

	if (ActiveMove != EDribbleMove::None)
	{
		CancelActiveMove();
	}

	OnControlLost.Broadcast(Owner, Ball);
}

void UDribblingComponent::ReleaseControl(float SuppressSeconds)
{
	if (const UWorld* World = GetWorld())
	{
		ControlSuppressedUntil = World->GetTimeSeconds() + FMath::Max(0.0f, SuppressSeconds);
	}

	if (bControlling)
	{
		LoseControl();
	}
}

// ----------------------------------------------------------------------
// Close control
// ----------------------------------------------------------------------

FVector UDribblingComponent::GetHeading() const
{
	if (ActiveMove == EDribbleMove::DirectionCut)
	{
		return ActiveMoveDirection;
	}

	if (!MovementInput.IsNearlyZero())
	{
		return MovementInput.GetSafeNormal();
	}

	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
}

float UDribblingComponent::GetCurrentControlDistance() const
{
	const ANewEraBallPlayer* Owner = GetOwningPlayer();
	if (!Owner)
	{
		return ControlDistance;
	}

	if (ActiveMove == EDribbleMove::SpeedChange)
	{
		// Explosive push when accelerating, tight check when slowing down.
		return bSpeedChangeIsPush ? ControlDistance * SpeedChangePushMultiplier : IdleControlDistance * 0.8f;
	}

	const float Speed = Owner->GetVelocity().Size2D();
	const float SpeedFraction = FMath::Clamp(Speed / FMath::Max(Owner->BaseRunSpeed, 1.0f), 0.0f, 1.0f);
	const float Distance = FMath::Lerp(IdleControlDistance, ControlDistance, SpeedFraction);
	return bSprinting ? Distance * 1.25f : Distance;
}

void UDribblingComponent::UpdateCloseControl(float DeltaTime)
{
	ANewEraBallPlayer* Owner = GetOwningPlayer();
	ANewEraBallBall* Ball = ControlledBall.Get();
	if (!Owner || !Ball)
	{
		return;
	}

	// Dribbling on the move counts as touches for the 2-touch rule.
	const float OwnerSpeed = Owner->GetVelocity().Size2D();
	if (OwnerSpeed >= MinSpeedForDribbleTouch)
	{
		DribbleTouchTimer += DeltaTime;
		if (DribbleTouchTimer >= DribbleTouchInterval)
		{
			DribbleTouchTimer = 0.0f;
			Ball->NotifyTouch(Owner, EBallTouchType::Dribble);
		}
	}

	// The touch above can have triggered a turnover and killed the ball.
	if (Ball->IsFrozen() || Ball->GetControllingPlayer() != Owner)
	{
		LoseControl();
		return;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector BallLocation = Ball->GetActorLocation();
	const FVector OwnerVelocity2D(Owner->GetVelocity().X, Owner->GetVelocity().Y, 0.0f);

	FVector Desired;
	if (ActiveMove == EDribbleMove::Feint || ActiveMove == EDribbleMove::BodyFake)
	{
		// The ball stays where it was when the fake began; only the body moves.
		const FVector Delta = FVector(MoveAnchor.X - BallLocation.X, MoveAnchor.Y - BallLocation.Y, 0.0f);
		Desired = Delta * ControlStiffness;
	}
	else
	{
		const FVector Target = FVector(OwnerLocation.X, OwnerLocation.Y, 0.0f) + GetHeading() * GetCurrentControlDistance();
		const FVector Delta = FVector(Target.X - BallLocation.X, Target.Y - BallLocation.Y, 0.0f);
		Desired = OwnerVelocity2D + Delta * ControlStiffness;
	}

	Ball->SetControlVelocity(Desired.GetClampedToMaxSize(MaxControlSpeed));
}

void UDribblingComponent::KnockBall(const FVector& Direction, float Speed)
{
	ANewEraBallBall* Ball = ControlledBall.Get();
	if (!Ball)
	{
		return;
	}
	const FVector Flat = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	Ball->SetControlVelocity(Flat * FMath::Max(Speed, 0.0f));
}

// ----------------------------------------------------------------------
// Dribble moves
// ----------------------------------------------------------------------

float UDribblingComponent::GetMoveDuration(EDribbleMove Move) const
{
	switch (Move)
	{
	case EDribbleMove::DirectionCut:	return DirectionCutDuration;
	case EDribbleMove::SpeedChange:		return SpeedChangeDuration;
	case EDribbleMove::Feint:			return FeintDuration;
	case EDribbleMove::BodyFake:		return BodyFakeDuration;
	default:							return 0.0f;
	}
}

bool UDribblingComponent::CanPerformMove() const
{
	const ANewEraBallPlayer* Owner = GetOwningPlayer();
	return Owner
		&& IsControllingBall()
		&& ActiveMove == EDribbleMove::None
		&& MoveCooldownRemaining <= 0.0f
		&& !Owner->IsInRecovery();
}

bool UDribblingComponent::PerformMove(EDribbleMove Move, FVector2D Direction)
{
	ANewEraBallPlayer* Owner = GetOwningPlayer();
	ANewEraBallBall* Ball = ControlledBall.Get();
	if (Move == EDribbleMove::None || !Owner || !Ball || !CanPerformMove())
	{
		return false;
	}

	FVector Dir = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = GetHeading();
	}
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();

	ActiveMove = Move;
	ActiveMoveTimeRemaining = GetMoveDuration(Move);
	MoveAnchor = Ball->GetActorLocation();

	switch (Move)
	{
	case EDribbleMove::DirectionCut:
		// Knock the ball onto the new line immediately; steering follows it for the move duration.
		ActiveMoveDirection = Dir;
		KnockBall(Dir, DirectionCutKnockSpeed);
		Ball->NotifyTouch(Owner, EBallTouchType::DribbleMove);
		break;

	case EDribbleMove::SpeedChange:
		// Not sprinting: explosive push. Sprinting: sudden check that pulls the ball tight.
		ActiveMoveDirection = Dir;
		bSpeedChangeIsPush = !bSprinting;
		if (bSpeedChangeIsPush && !Owner->HasMomentumBurst())
		{
			Owner->ApplyMomentumBurst(SpeedChangeBurstMultiplier, SpeedChangeDuration);
		}
		Ball->NotifyTouch(Owner, EBallTouchType::DribbleMove);
		break;

	case EDribbleMove::Feint:
		// Body sells a sideways direction; the ball is untouched.
		ActiveMoveDirection = ComputeLateral(Dir, Forward);
		break;

	case EDribbleMove::BodyFake:
		// Upper body fake one way; the ball is shifted the other way when the move ends.
		ActiveMoveDirection = ComputeLateral(Dir, Forward);
		break;

	default:
		ActiveMove = EDribbleMove::None;
		return false;
	}

	// A move touch can itself be a violation; the ball is then dead and the move is void.
	if (!bControlling || Ball->IsFrozen())
	{
		ActiveMove = EDribbleMove::None;
		ActiveMoveTimeRemaining = 0.0f;
		return false;
	}

	OnMoveStarted.Broadcast(Owner, Move, ActiveMoveDirection);
	return true;
}

void UDribblingComponent::UpdateActiveMove(float DeltaTime)
{
	if (ActiveMove == EDribbleMove::None)
	{
		return;
	}

	ActiveMoveTimeRemaining -= DeltaTime;
	if (ActiveMoveTimeRemaining > 0.0f)
	{
		return;
	}

	ANewEraBallPlayer* Owner = GetOwningPlayer();
	ANewEraBallBall* Ball = ControlledBall.Get();
	const EDribbleMove FinishedMove = ActiveMove;

	if (FinishedMove == EDribbleMove::BodyFake && bControlling && Owner && Ball)
	{
		// Exit the fake by shifting the ball away from the faked side and slightly forward.
		const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
		const FVector ExitDirection = (Forward * 0.6f - ActiveMoveDirection).GetSafeNormal();
		KnockBall(ExitDirection, BodyFakeExitKnockSpeed);
		Ball->NotifyTouch(Owner, EBallTouchType::DribbleMove);
	}

	ActiveMove = EDribbleMove::None;
	ActiveMoveTimeRemaining = 0.0f;
	MoveCooldownRemaining = MoveCooldown;

	OnMoveEnded.Broadcast(Owner, FinishedMove);
}

void UDribblingComponent::CancelActiveMove()
{
	if (ActiveMove == EDribbleMove::None)
	{
		return;
	}

	const EDribbleMove Cancelled = ActiveMove;
	ActiveMove = EDribbleMove::None;
	ActiveMoveTimeRemaining = 0.0f;
	MoveCooldownRemaining = MoveCooldown;

	OnMoveEnded.Broadcast(GetOwningPlayer(), Cancelled);
}

// ----------------------------------------------------------------------
// Beat detection
// ----------------------------------------------------------------------

void UDribblingComponent::UpdateBeatDetection()
{
	ANewEraBallPlayer* Owner = GetOwningPlayer();
	ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	const UWorld* World = GetWorld();
	if (!Owner || !GameMode || !World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FVector OwnerLocation = Owner->GetActorLocation();

	FVector Heading = FVector(Owner->GetVelocity().X, Owner->GetVelocity().Y, 0.0f);
	if (Heading.SizeSquared() < 100.0f * 100.0f)
	{
		Heading = Owner->GetActorForwardVector();
	}
	Heading = Heading.GetSafeNormal2D();
	if (Heading.IsNearlyZero())
	{
		return;
	}

	const float TrackingRadius = BeatDetectionRadius * 1.5f;

	for (ANewEraBallPlayer* Defender : GameMode->GetTeamPlayers(UNewEraBallTypeLibrary::GetOpposingTeam(Owner->GetTeam())))
	{
		if (!Defender || Defender == Owner)
		{
			continue;
		}

		const FVector ToDefender = FVector(Defender->GetActorLocation().X - OwnerLocation.X, Defender->GetActorLocation().Y - OwnerLocation.Y, 0.0f);
		const float Distance = ToDefender.Size();
		if (Distance > TrackingRadius || Distance < UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ToDefender / Distance, Heading);
		FDefenderTrack& Track = DefenderTracks.FindOrAdd(Defender);

		if (Distance <= BeatDetectionRadius && Dot >= InFrontDotThreshold)
		{
			Track.LastTimeInFront = Now;
			continue;
		}

		const bool bNowBehind = Dot <= BehindDotThreshold;
		const bool bWasInFrontRecently = (Now - Track.LastTimeInFront) <= BeatWindowSeconds;
		const bool bOffCooldown = Now >= Track.BeatCooldownUntil;

		if (bNowBehind && bWasInFrontRecently && bOffCooldown)
		{
			Track.BeatCooldownUntil = Now + BeatCooldownSeconds;
			Track.LastTimeInFront = -1000.0f;

			Owner->ApplyMomentumBurst(MomentumBurstMultiplier, MomentumBurstDuration);
			Defender->ApplyRecoveryDelay(RecoveryDelayDuration, RecoverySpeedMultiplier);

			UE_LOG(LogNewEraBall, Verbose, TEXT("%s beat %s."), *GetNameSafe(Owner), *GetNameSafe(Defender));
			OnDefenderBeaten.Broadcast(Owner, Defender);
		}
	}

	// Drop entries for players that no longer exist.
	for (auto It = DefenderTracks.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
