// NewEraBall - Touch counter component implementation

#include "TouchCounterComponent.h"
#include "NewEraBall.h"
#include "NewEraBallPlayer.h"
#include "NewEraBallGameMode.h"
#include "Engine/World.h"

UTouchCounterComponent::UTouchCounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ANewEraBallPlayer* UTouchCounterComponent::GetOwningPlayer() const
{
	return Cast<ANewEraBallPlayer>(GetOwner());
}

ANewEraBallGameMode* UTouchCounterComponent::GetNewEraBallGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<ANewEraBallGameMode>() : nullptr;
}

int32 UTouchCounterComponent::GetMaxTouches() const
{
	if (MaxTouchesOverride > 0)
	{
		return MaxTouchesOverride;
	}
	if (const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		return FMath::Max(1, GameMode->MaxTouchesBehindMidfield);
	}
	return 2;
}

bool UTouchCounterComponent::IsLocationRestricted(FVector BallLocation) const
{
	const ANewEraBallPlayer* Player = GetOwningPlayer();
	const ANewEraBallGameMode* GameMode = GetNewEraBallGameMode();
	if (!Player || !GameMode)
	{
		return false;
	}
	return GameMode->IsInOwnHalf(Player->GetTeam(), BallLocation);
}

ETouchViolationResult UTouchCounterComponent::RegisterTouch(FVector BallLocation, EBallTouchType TouchType)
{
	ANewEraBallPlayer* Player = GetOwningPlayer();
	if (!Player)
	{
		return ETouchViolationResult::None;
	}

	LastTouchLocation = BallLocation;
	LastTouchType = TouchType;

	const bool bRestricted = IsLocationRestricted(BallLocation);
	bLastTouchRestricted = bRestricted;

	if (!bRestricted)
	{
		// Unlimited touches past midfield. Crossing the line gives the player a clean slate.
		if (bResetWhenBallCrossesMidfield && TouchCount != 0)
		{
			TouchCount = 0;
			OnTouchesReset.Broadcast(Player);
		}
		OnTouchRegistered.Broadcast(Player, TouchCount, GetMaxTouches(), false);
		return ETouchViolationResult::None;
	}

	++TouchCount;
	const int32 MaxTouches = GetMaxTouches();
	OnTouchRegistered.Broadcast(Player, TouchCount, MaxTouches, true);

	if (TouchCount <= MaxTouches)
	{
		return ETouchViolationResult::None;
	}

	// Over the limit: the GameMode decides between turnover and penalty and applies it.
	ETouchViolationResult Result = ETouchViolationResult::None;
	if (ANewEraBallGameMode* GameMode = GetNewEraBallGameMode())
	{
		Result = GameMode->HandleTouchViolation(Player, BallLocation);
	}

	if (Result != ETouchViolationResult::None)
	{
		UE_LOG(LogNewEraBall, Log, TEXT("%s exceeded %d touches behind midfield: %s."), *GetNameSafe(Player), MaxTouches,
			Result == ETouchViolationResult::Penalty ? TEXT("penalty") : TEXT("turnover"));
		OnTouchViolation.Broadcast(Player, Result);
	}

	return Result;
}

void UTouchCounterComponent::ResetTouches()
{
	if (TouchCount == 0 && !bLastTouchRestricted)
	{
		return;
	}

	TouchCount = 0;
	bLastTouchRestricted = false;

	if (ANewEraBallPlayer* Player = GetOwningPlayer())
	{
		OnTouchesReset.Broadcast(Player);
	}
}
