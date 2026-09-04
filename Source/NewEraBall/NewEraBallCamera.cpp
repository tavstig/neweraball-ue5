// NewEraBall - Broadcast camera implementation

#include "NewEraBallCamera.h"
#include "NewEraBall.h"
#include "NewEraBallGameMode.h"
#include "NewEraBallBall.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ANewEraBallCamera::ANewEraBallCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// Aim after every actor has moved this frame so the shot never lags a frame behind the ball.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(NearFieldOfView);
	Camera->bConstrainAspectRatio = false;
}

void ANewEraBallCamera::BeginPlay()
{
	Super::BeginPlay();

	if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		GameMode->RegisterBroadcastCamera(this);
	}

	if (bAutoPlaceFromPitch)
	{
		PlaceFromPitch();
	}

	SnapToTarget();

	if (bAutoActivateForLocalPlayer)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			ActivateForController(PC, 0.0f);
		}
	}
}

void ANewEraBallCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FRotator DesiredRotation;
	float DesiredFieldOfView;
	ComputeDesired(DesiredRotation, DesiredFieldOfView);

	const FRotator NewRotation = FMath::RInterpTo(Camera->GetComponentRotation(), DesiredRotation, DeltaSeconds, RotationInterpSpeed);
	Camera->SetWorldRotation(NewRotation);

	const float NewFieldOfView = FMath::FInterpTo(Camera->FieldOfView, DesiredFieldOfView, DeltaSeconds, ZoomInterpSpeed);
	Camera->SetFieldOfView(NewFieldOfView);
}

ANewEraBallGameMode* ANewEraBallCamera::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

AActor* ANewEraBallCamera::ResolveTrackedActor() const
{
	if (TrackedActor)
	{
		return TrackedActor;
	}
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	return GameMode ? GameMode->MatchBall.Get() : nullptr;
}

void ANewEraBallCamera::ActivateForController(APlayerController* PlayerController, float BlendTime)
{
	if (!PlayerController)
	{
		return;
	}

	// Possessing a pawn would otherwise pull the view back to that pawn.
	PlayerController->bAutoManageActiveCameraTarget = false;

	if (PlayerController->GetViewTarget() != this)
	{
		PlayerController->SetViewTargetWithBlend(this, BlendTime);
	}
}

void ANewEraBallCamera::SetTrackedActor(AActor* Actor)
{
	TrackedActor = Actor;
}

void ANewEraBallCamera::PlaceFromPitch()
{
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	if (!GameMode)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("%s: no NewEraBall GameMode found, keeping the placed transform."), *GetNameSafe(this));
		return;
	}

	const float SideSign = bMountOnPositiveY ? 1.0f : -1.0f;
	const FVector Mount = GameMode->PitchCenter + FVector(
		0.0f,
		SideSign * (GameMode->Pitch.GetHalfWidth() + SidelineDistance),
		MountHeight);

	SetActorLocation(Mount);
}

FVector ANewEraBallCamera::GetAimPoint() const
{
	const AActor* Target = ResolveTrackedActor();
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();

	FVector Aim;
	if (Target)
	{
		Aim = Target->GetActorLocation() + Target->GetVelocity() * LookAheadSeconds;
	}
	else if (GameMode)
	{
		Aim = GameMode->PitchCenter;
	}
	else
	{
		Aim = GetActorLocation() + GetActorForwardVector() * 1000.0f;
	}

	if (bClampAimToPitch && GameMode)
	{
		const FVector Clamped = GameMode->ClampToPitch(Aim, -AimClampMargin);
		Aim.X = Clamped.X;
		Aim.Y = Clamped.Y;
	}

	Aim.Z += AimHeightOffset;
	return Aim;
}

void ANewEraBallCamera::ComputeDesired(FRotator& OutRotation, float& OutFieldOfView) const
{
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector Aim = GetAimPoint();
	const FVector ToAim = Aim - CameraLocation;

	OutRotation = ToAim.IsNearlyZero() ? Camera->GetComponentRotation() : ToAim.Rotation();

	const float Distance = ToAim.Size();
	const float Range = FMath::Max(FarDistance - NearDistance, 1.0f);
	const float Alpha = FMath::Clamp((Distance - NearDistance) / Range, 0.0f, 1.0f);
	OutFieldOfView = FMath::Lerp(NearFieldOfView, FarFieldOfView, Alpha);
}

void ANewEraBallCamera::SnapToTarget()
{
	FRotator DesiredRotation;
	float DesiredFieldOfView;
	ComputeDesired(DesiredRotation, DesiredFieldOfView);
	Camera->SetWorldRotation(DesiredRotation);
	Camera->SetFieldOfView(DesiredFieldOfView);
}
