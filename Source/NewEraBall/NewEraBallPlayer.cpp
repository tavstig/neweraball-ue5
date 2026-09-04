// NewEraBall - Player character implementation

#include "NewEraBallPlayer.h"
#include "NewEraBall.h"
#include "NewEraBallBall.h"
#include "NewEraBallGameMode.h"
#include "TouchCounterComponent.h"
#include "DribblingComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

ANewEraBallPlayer::ANewEraBallPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule sized for an adult footballer.
	GetCapsuleComponent()->InitCapsuleSize(38.0f, 92.0f);

	// The character turns to face where it moves; the camera never drives the body.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Tight, immediate response: high acceleration, hard braking, fast turning.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
	Movement->MaxWalkSpeed = BaseRunSpeed;
	Movement->MaxAcceleration = 4500.0f;
	Movement->BrakingDecelerationWalking = 4500.0f;
	Movement->GroundFriction = 12.0f;
	Movement->BrakingFrictionFactor = 2.0f;
	Movement->bUseSeparateBrakingFriction = true;
	Movement->BrakingFriction = 8.0f;
	Movement->JumpZVelocity = 420.0f;
	Movement->AirControl = 0.35f;
	Movement->bCanWalkOffLedges = true;

	TouchCounter = CreateDefaultSubobject<UTouchCounterComponent>(TEXT("TouchCounter"));
	Dribbling = CreateDefaultSubobject<UDribblingComponent>(TEXT("Dribbling"));
}

void ANewEraBallPlayer::BeginPlay()
{
	Super::BeginPlay();

	UpdateMovementSpeed();

	if (Team != ETeamSide::None)
	{
		if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
		{
			GameMode->RegisterPlayer(this, Team);
		}
	}
}

void ANewEraBallPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		GameMode->UnregisterPlayer(this);
	}

	RemoveMappingContextFromController();
	Super::EndPlay(EndPlayReason);
}

void ANewEraBallPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Momentum burst and recovery delay countdowns.
	if (MomentumTimeRemaining > 0.0f)
	{
		MomentumTimeRemaining -= DeltaSeconds;
		if (MomentumTimeRemaining <= 0.0f)
		{
			MomentumTimeRemaining = 0.0f;
			MomentumMultiplier = 1.0f;
			OnMomentumBurstEnded.Broadcast(this);
		}
	}

	if (RecoveryTimeRemaining > 0.0f)
	{
		RecoveryTimeRemaining -= DeltaSeconds;
		if (RecoveryTimeRemaining <= 0.0f)
		{
			RecoveryTimeRemaining = 0.0f;
			RecoveryMultiplier = 1.0f;
			OnRecoveryEnded.Broadcast(this);
		}
	}

	// Shot charge builds while the input is held.
	if (bIsChargingShot)
	{
		ShotChargeElapsed += DeltaSeconds;
		ShotCharge = FMath::Clamp(ShotChargeElapsed / FMath::Max(ShotChargeSeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	}

	// Apply this frame's movement input.
	CurrentMoveInput = PendingMoveInput;
	if (CurrentMoveInput.SizeSquared() > 1.0f)
	{
		CurrentMoveInput.Normalize();
	}
	PendingMoveInput = FVector::ZeroVector;

	if (!CurrentMoveInput.IsNearlyZero())
	{
		LastMoveDirection = CurrentMoveInput.GetSafeNormal();
		AddMovementInput(CurrentMoveInput);
	}

	if (Dribbling)
	{
		Dribbling->SetMovementInput(CurrentMoveInput);
		Dribbling->SetSprinting(bIsSprinting);
	}

	UpdateMovementSpeed();
}

// ----------------------------------------------------------------------
// Identity
// ----------------------------------------------------------------------

void ANewEraBallPlayer::SetTeam(ETeamSide NewTeam)
{
	if (Team == NewTeam)
	{
		return;
	}

	Team = NewTeam;

	if (HasActorBegunPlay())
	{
		if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
		{
			GameMode->RegisterPlayer(this, Team);
		}
	}
}

bool ANewEraBallPlayer::IsOpponentOf(const ANewEraBallPlayer* Other) const
{
	return Other && Other != this && Other->Team != ETeamSide::None && Team != ETeamSide::None && Other->Team != Team;
}

ANewEraBallGameMode* ANewEraBallPlayer::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

// ----------------------------------------------------------------------
// Momentum & recovery
// ----------------------------------------------------------------------

void ANewEraBallPlayer::ApplyMomentumBurst(float SpeedMultiplier, float Duration)
{
	if (Duration <= 0.0f || SpeedMultiplier <= 0.0f)
	{
		return;
	}

	MomentumMultiplier = SpeedMultiplier;
	MomentumTimeRemaining = Duration;
	UpdateMovementSpeed();
	OnMomentumBurstStarted.Broadcast(this, Duration);
}

void ANewEraBallPlayer::ApplyRecoveryDelay(float Duration, float SpeedMultiplier)
{
	if (Duration <= 0.0f)
	{
		return;
	}

	// A beaten player loses any momentum they had.
	ClearMomentumBurst();

	RecoveryMultiplier = FMath::Clamp(SpeedMultiplier, 0.0f, 1.0f);
	RecoveryTimeRemaining = Duration;
	UpdateMovementSpeed();
	OnRecoveryStarted.Broadcast(this, Duration);
}

void ANewEraBallPlayer::ClearMomentumBurst()
{
	const bool bWasActive = MomentumTimeRemaining > 0.0f;
	MomentumTimeRemaining = 0.0f;
	MomentumMultiplier = 1.0f;
	UpdateMovementSpeed();
	if (bWasActive)
	{
		OnMomentumBurstEnded.Broadcast(this);
	}
}

void ANewEraBallPlayer::ClearRecoveryDelay()
{
	const bool bWasActive = RecoveryTimeRemaining > 0.0f;
	RecoveryTimeRemaining = 0.0f;
	RecoveryMultiplier = 1.0f;
	UpdateMovementSpeed();
	if (bWasActive)
	{
		OnRecoveryEnded.Broadcast(this);
	}
}

float ANewEraBallPlayer::GetEffectiveMaxSpeed() const
{
	float Speed = bIsSprinting ? SprintSpeed : BaseRunSpeed;

	if (HasBallControl())
	{
		Speed *= SpeedMultiplierWithBall;
	}
	if (MomentumTimeRemaining > 0.0f)
	{
		Speed *= MomentumMultiplier;
	}
	if (RecoveryTimeRemaining > 0.0f)
	{
		Speed *= RecoveryMultiplier;
	}
	return Speed;
}

void ANewEraBallPlayer::UpdateMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = GetEffectiveMaxSpeed();
	}
}

// ----------------------------------------------------------------------
// Ball interaction
// ----------------------------------------------------------------------

ANewEraBallBall* ANewEraBallPlayer::GetControlledBall() const
{
	return Dribbling ? Dribbling->GetControlledBall() : nullptr;
}

bool ANewEraBallPlayer::HasBallControl() const
{
	return Dribbling && Dribbling->IsControllingBall();
}

ANewEraBallBall* ANewEraBallPlayer::GetBallInReach() const
{
	if (ANewEraBallBall* Controlled = GetControlledBall())
	{
		return Controlled;
	}

	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	ANewEraBallBall* Ball = GameMode ? GameMode->MatchBall.Get() : nullptr;
	if (!Ball)
	{
		return nullptr;
	}

	const FVector ToBall = Ball->GetActorLocation() - GetActorLocation();
	const float HorizontalDistance = FVector(ToBall.X, ToBall.Y, 0.0f).Size();
	const float VerticalDistance = FMath::Abs(ToBall.Z);
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	if (HorizontalDistance <= KickReach && VerticalDistance <= CapsuleHalfHeight + Ball->GetBallRadius())
	{
		return Ball;
	}
	return nullptr;
}

FVector ANewEraBallPlayer::GetAimDirection() const
{
	if (!CurrentMoveInput.IsNearlyZero())
	{
		return CurrentMoveInput.GetSafeNormal2D();
	}
	return GetActorForwardVector().GetSafeNormal2D();
}

bool ANewEraBallPlayer::KickBall(FVector Direction, float Speed, float Lift, EBallTouchType KickType)
{
	ANewEraBallBall* Ball = GetBallInReach();
	if (!Ball)
	{
		return false;
	}

	if (const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		if (!GameMode->CanPlayerPlayBall(this))
		{
			return false;
		}
	}

	FVector Flat = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (Flat.IsNearlyZero())
	{
		Flat = GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Velocity = (Flat + FVector(0.0f, 0.0f, FMath::Clamp(Lift, 0.0f, 1.0f))).GetSafeNormal() * FMath::Max(Speed, 0.0f);

	// Let go of close control first so the kick is not immediately damped back to the feet.
	if (Dribbling)
	{
		Dribbling->ReleaseControl(PostKickControlSuppression);
	}

	Ball->Kick(this, Velocity, KickType);

	OnKicked.Broadcast(this, KickType, Speed);
	return true;
}

bool ANewEraBallPlayer::Pass()
{
	return KickBall(GetAimDirection(), PassSpeed, PassLift, EBallTouchType::Pass);
}

bool ANewEraBallPlayer::Shoot(float Charge)
{
	const float Alpha = FMath::Clamp(Charge, 0.0f, 1.0f);
	const float Speed = FMath::Lerp(MinShotSpeed, MaxShotSpeed, Alpha);
	const float Lift = FMath::Lerp(MinShotLift, MaxShotLift, Alpha);
	return KickBall(GetAimDirection(), Speed, Lift, EBallTouchType::Shot);
}

void ANewEraBallPlayer::NotifyBallTouch(ANewEraBallBall* Ball, EBallTouchType TouchType, FVector TouchLocation)
{
	if (TouchCounter)
	{
		TouchCounter->RegisterTouch(TouchLocation, TouchType);
	}

	OnBallTouched.Broadcast(this, Ball, TouchType);
}

void ANewEraBallPlayer::ResetForRestart(const FTransform& Transform)
{
	if (Dribbling)
	{
		Dribbling->ReleaseControl(0.0f);
		Dribbling->CancelActiveMove();
	}

	ClearMomentumBurst();
	ClearRecoveryDelay();

	bIsSprinting = false;
	bIsChargingShot = false;
	ShotCharge = 0.0f;
	ShotChargeElapsed = 0.0f;
	PendingMoveInput = FVector::ZeroVector;
	CurrentMoveInput = FVector::ZeroVector;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	LastMoveDirection = Transform.GetRotation().GetForwardVector().GetSafeNormal2D();

	if (AController* OwningController = GetController())
	{
		OwningController->SetControlRotation(Transform.Rotator());
	}

	UpdateMovementSpeed();
}

// ----------------------------------------------------------------------
// Input
// ----------------------------------------------------------------------

void ANewEraBallPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogNewEraBall, Error, TEXT("'%s' expected an Enhanced Input component. Check DefaultInputComponentClass in DefaultInput.ini."), *GetNameSafe(this));
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANewEraBallPlayer::MoveInput);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANewEraBallPlayer::MoveInputCompleted);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANewEraBallPlayer::LookInput);
	}
	if (SprintAction)
	{
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::SprintStarted);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ANewEraBallPlayer::SprintCompleted);
	}
	if (PassAction)
	{
		EnhancedInput->BindAction(PassAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::PassInput);
	}
	if (ShootAction)
	{
		EnhancedInput->BindAction(ShootAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::ShootStarted);
		EnhancedInput->BindAction(ShootAction, ETriggerEvent::Completed, this, &ANewEraBallPlayer::ShootCompleted);
	}
	if (DirectionCutAction)
	{
		EnhancedInput->BindAction(DirectionCutAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::DirectionCutInput);
	}
	if (SpeedChangeAction)
	{
		EnhancedInput->BindAction(SpeedChangeAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::SpeedChangeInput);
	}
	if (FeintAction)
	{
		EnhancedInput->BindAction(FeintAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::FeintInput);
	}
	if (BodyFakeAction)
	{
		EnhancedInput->BindAction(BodyFakeAction, ETriggerEvent::Started, this, &ANewEraBallPlayer::BodyFakeInput);
	}
}

void ANewEraBallPlayer::NotifyControllerChanged()
{
	// Drop the mapping context from whichever controller we just left.
	if (APlayerController* OldPC = Cast<APlayerController>(PreviousController.Get()))
	{
		if (DefaultMappingContext)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OldPC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
			}
		}
	}

	Super::NotifyControllerChanged();

	AddMappingContextToController();
}

void ANewEraBallPlayer::AddMappingContextToController()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (!Subsystem->HasMappingContext(DefaultMappingContext))
			{
				Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
			}
		}
	}
}

void ANewEraBallPlayer::RemoveMappingContextFromController()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
}

float ANewEraBallPlayer::GetMovementFrameYaw() const
{
	if (bCameraRelativeMovement)
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				return PC->PlayerCameraManager->GetCameraRotation().Yaw;
			}
			return PC->GetControlRotation().Yaw;
		}
	}
	// AI and world-relative movement use the world axes.
	return 0.0f;
}

void ANewEraBallPlayer::DoMove(float Right, float Forward)
{
	const FRotator Frame(0.0f, GetMovementFrameYaw(), 0.0f);
	const FVector ForwardDir = FRotationMatrix(Frame).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(Frame).GetUnitAxis(EAxis::Y);

	PendingMoveInput += ForwardDir * Forward + RightDir * Right;
}

void ANewEraBallPlayer::DoMoveWorld(FVector WorldDirection, float Scale)
{
	PendingMoveInput += FVector(WorldDirection.X, WorldDirection.Y, 0.0f).GetSafeNormal() * FMath::Clamp(Scale, 0.0f, 1.0f);
}

void ANewEraBallPlayer::DoLook(float Yaw, float Pitch)
{
	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void ANewEraBallPlayer::DoSprint(bool bEnable)
{
	bIsSprinting = bEnable;
	UpdateMovementSpeed();
}

void ANewEraBallPlayer::DoPass()
{
	// Passing cancels any shot being charged.
	bIsChargingShot = false;
	ShotCharge = 0.0f;
	ShotChargeElapsed = 0.0f;
	Pass();
}

void ANewEraBallPlayer::DoShootStart()
{
	bIsChargingShot = true;
	ShotCharge = 0.0f;
	ShotChargeElapsed = 0.0f;
}

void ANewEraBallPlayer::DoShootRelease()
{
	if (!bIsChargingShot)
	{
		return;
	}

	const float Charge = ShotCharge;
	bIsChargingShot = false;
	ShotCharge = 0.0f;
	ShotChargeElapsed = 0.0f;
	Shoot(Charge);
}

void ANewEraBallPlayer::DoDribbleMove(EDribbleMove Move)
{
	if (!Dribbling || IsInRecovery())
	{
		return;
	}

	const FVector Direction = CurrentMoveInput.IsNearlyZero() ? LastMoveDirection : CurrentMoveInput.GetSafeNormal();
	Dribbling->PerformMove(Move, FVector2D(Direction.X, Direction.Y));
}

void ANewEraBallPlayer::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoMove(Axis.X, Axis.Y);
}

void ANewEraBallPlayer::MoveInputCompleted(const FInputActionValue& Value)
{
	PendingMoveInput = FVector::ZeroVector;
}

void ANewEraBallPlayer::LookInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoLook(Axis.X, Axis.Y);
}

void ANewEraBallPlayer::SprintStarted(const FInputActionValue& Value)
{
	DoSprint(true);
}

void ANewEraBallPlayer::SprintCompleted(const FInputActionValue& Value)
{
	DoSprint(false);
}

void ANewEraBallPlayer::PassInput(const FInputActionValue& Value)
{
	DoPass();
}

void ANewEraBallPlayer::ShootStarted(const FInputActionValue& Value)
{
	DoShootStart();
}

void ANewEraBallPlayer::ShootCompleted(const FInputActionValue& Value)
{
	DoShootRelease();
}

void ANewEraBallPlayer::DirectionCutInput(const FInputActionValue& Value)
{
	DoDribbleMove(EDribbleMove::DirectionCut);
}

void ANewEraBallPlayer::SpeedChangeInput(const FInputActionValue& Value)
{
	DoDribbleMove(EDribbleMove::SpeedChange);
}

void ANewEraBallPlayer::FeintInput(const FInputActionValue& Value)
{
	DoDribbleMove(EDribbleMove::Feint);
}

void ANewEraBallPlayer::BodyFakeInput(const FInputActionValue& Value)
{
	DoDribbleMove(EDribbleMove::BodyFake);
}
