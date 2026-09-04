// NewEraBall - Match ball implementation

#include "NewEraBallBall.h"
#include "NewEraBall.h"
#include "NewEraBallPlayer.h"
#include "NewEraBallGameMode.h"
#include "TouchCounterComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ANewEraBallBall::ANewEraBallBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(BallRadius);
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetSimulatePhysics(true);
	Collision->SetEnableGravity(true);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetUseCCD(true);
	Collision->SetLinearDamping(LinearDamping);
	Collision->SetAngularDamping(AngularDamping);
	Collision->SetCanEverAffectNavigation(false);
	Collision->BodyInstance.bLockRotation = false;
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
}

void ANewEraBallBall::BeginPlay()
{
	Super::BeginPlay();

	// Apply the editable tuning to the physics body.
	Collision->SetSphereRadius(BallRadius);
	Collision->SetMassOverrideInKg(NAME_None, MassKg, true);
	Collision->SetLinearDamping(LinearDamping);
	Collision->SetAngularDamping(AngularDamping);
	Collision->OnComponentHit.AddDynamic(this, &ANewEraBallBall::HandleHit);

	if (bAutoRegisterWithGameMode)
	{
		if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
		{
			GameMode->RegisterBall(this);
		}
	}
}

void ANewEraBallBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Collision)
	{
		Collision->OnComponentHit.RemoveDynamic(this, &ANewEraBallBall::HandleHit);
	}
	Super::EndPlay(EndPlayReason);
}

void ANewEraBallBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFrozen)
	{
		return;
	}

	ClampSpeed();

	if (bDetectBoundariesGeometrically && !bBoundaryReported)
	{
		CheckBoundaries();
	}
}

ANewEraBallGameMode* ANewEraBallBall::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

FVector ANewEraBallBall::GetBallVelocity() const
{
	return Collision && Collision->IsSimulatingPhysics() ? Collision->GetPhysicsLinearVelocity() : FVector::ZeroVector;
}

// ----------------------------------------------------------------------
// Control
// ----------------------------------------------------------------------

void ANewEraBallBall::PlaceAt(FVector GroundLocation)
{
	FreezeInPlace();

	const FVector Location = GroundLocation + FVector(0.0f, 0.0f, BallRadius + 1.0f);
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator::ZeroRotator, ETeleportType::TeleportPhysics);
	bBoundaryReported = false;
}

void ANewEraBallBall::FreezeInPlace()
{
	SetControllingPlayer(nullptr);

	if (Collision->IsSimulatingPhysics())
	{
		Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Collision->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Collision->SetSimulatePhysics(false);
	}

	if (!bFrozen)
	{
		bFrozen = true;
		OnFrozenChanged.Broadcast(true);
	}
}

void ANewEraBallBall::Release()
{
	if (!Collision->IsSimulatingPhysics())
	{
		Collision->SetSimulatePhysics(true);
		Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Collision->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}

	bBoundaryReported = false;

	if (bFrozen)
	{
		bFrozen = false;
		OnFrozenChanged.Broadcast(false);
	}
}

bool ANewEraBallBall::Kick(ANewEraBallPlayer* Kicker, FVector Velocity, EBallTouchType KickType)
{
	if (!Kicker)
	{
		return false;
	}

	// A kick during a restart is what brings the ball back into play, so register before releasing.
	if (!RegisterTouch(Kicker, KickType, GetActorLocation()))
	{
		return false;
	}

	SetControllingPlayer(nullptr);
	Release();

	Collision->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(MaxSpeed));
	Collision->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);

	OnBallKicked.Broadcast(this, Kicker, Velocity);
	return true;
}

bool ANewEraBallBall::NotifyTouch(ANewEraBallPlayer* Player, EBallTouchType TouchType)
{
	return RegisterTouch(Player, TouchType, GetActorLocation());
}

void ANewEraBallBall::SetControllingPlayer(ANewEraBallPlayer* Player)
{
	if (ControllingPlayer.Get() == Player)
	{
		return;
	}
	ControllingPlayer = Player;
	OnControlChanged.Broadcast(this, Player);
}

void ANewEraBallBall::SetControlVelocity(FVector HorizontalVelocity)
{
	if (bFrozen || !Collision->IsSimulatingPhysics())
	{
		return;
	}

	const FVector Current = Collision->GetPhysicsLinearVelocity();
	FVector NewVelocity(HorizontalVelocity.X, HorizontalVelocity.Y, Current.Z);
	NewVelocity = NewVelocity.GetClampedToMaxSize(MaxSpeed);
	Collision->SetPhysicsLinearVelocity(NewVelocity);
}

// ----------------------------------------------------------------------
// Touch detection
// ----------------------------------------------------------------------

void ANewEraBallBall::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ANewEraBallPlayer* Player = Cast<ANewEraBallPlayer>(OtherActor);
	if (!Player)
	{
		return;
	}

	// A player who already has the ball under close control is steering it, not touching it again.
	if (ControllingPlayer.Get() == Player)
	{
		return;
	}

	RegisterTouch(Player, EBallTouchType::Contact, Hit.ImpactPoint);
}

bool ANewEraBallBall::RegisterTouch(ANewEraBallPlayer* Player, EBallTouchType TouchType, const FVector& Location)
{
	if (!Player)
	{
		return false;
	}

	ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	if (GameMode && !GameMode->CanPlayerPlayBall(Player))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// Filter repeated physical contacts from the same player.
	if (TouchType == EBallTouchType::Contact
		&& LastTouchPlayer.Get() == Player
		&& LastTouchTime >= 0.0f
		&& (Now - LastTouchTime) < ContactDebounceSeconds)
	{
		return false;
	}

	// Possession changing hands resets the previous player's touch count.
	ANewEraBallPlayer* Previous = LastTouchPlayer.Get();
	if (Previous && Previous != Player)
	{
		if (UTouchCounterComponent* PreviousCounter = Previous->GetTouchCounter())
		{
			PreviousCounter->ResetTouches();
		}
	}

	LastTouchPlayer = Player;
	LastTouchTeam = Player->GetTeam();
	LastTouchLocation = Location;
	LastTouchType = TouchType;
	LastTouchTime = Now;

	// GameMode first: a restart touch brings the ball live before the touch is counted.
	if (GameMode)
	{
		GameMode->NotifyBallTouched(Player, TouchType, Location);
	}

	Player->NotifyBallTouch(this, TouchType, Location);
	OnBallTouched.Broadcast(this, Player, TouchType);
	return true;
}

void ANewEraBallBall::CheckBoundaries()
{
	ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	if (!GameMode)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector Local = Location - GameMode->PitchCenter;
	const float HalfLength = GameMode->Pitch.GetHalfLength();
	const float HalfWidth = GameMode->Pitch.GetHalfWidth();

	// The whole ball must cross the line, as in football.
	const bool bPastEndLine = FMath::Abs(Local.X) > HalfLength + BallRadius;
	const bool bPastTouchline = FMath::Abs(Local.Y) > HalfWidth + BallRadius;

	if (bPastEndLine)
	{
		const bool bInGoalMouth = FMath::Abs(Local.Y) <= GameMode->Pitch.GoalWidth * 0.5f
			&& Local.Z <= GameMode->Pitch.GoalHeight;

		if (bInGoalMouth)
		{
			// The goal at +X belongs to whichever team defends that end.
			const ETeamSide GoalOwner = (Local.X > 0.0f)
				? (GameMode->GetAttackDirectionSign(ETeamSide::Home) > 0.0f ? ETeamSide::Away : ETeamSide::Home)
				: (GameMode->GetAttackDirectionSign(ETeamSide::Home) > 0.0f ? ETeamSide::Home : ETeamSide::Away);

			bBoundaryReported = true;
			GameMode->NotifyBallEnteredGoal(GoalOwner);
			return;
		}
	}

	if (bPastEndLine || bPastTouchline)
	{
		bBoundaryReported = true;
		GameMode->NotifyBallOutOfPlay(Location, LastTouchTeam);
	}
}

void ANewEraBallBall::ClampSpeed()
{
	if (!Collision->IsSimulatingPhysics())
	{
		return;
	}

	const FVector Velocity = Collision->GetPhysicsLinearVelocity();
	if (Velocity.SizeSquared() > MaxSpeed * MaxSpeed)
	{
		Collision->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(MaxSpeed));
	}
}
