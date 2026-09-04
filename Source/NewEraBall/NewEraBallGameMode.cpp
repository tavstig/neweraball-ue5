// NewEraBall - Match GameMode implementation

#include "NewEraBallGameMode.h"
#include "NewEraBall.h"
#include "NewEraBallPlayer.h"
#include "NewEraBallBall.h"
#include "NewEraBallCamera.h"
#include "TouchCounterComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ANewEraBallGameMode::ANewEraBallGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Human players are handed a team player by AssignHumanControl rather than a default pawn.
	bStartPlayersAsSpectators = true;
	DefaultPawnClass = nullptr;
}

void ANewEraBallGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnTeams)
	{
		SpawnTeams();
	}

	if (bAutoSpawnBall)
	{
		SpawnMatchBall();
	}

	if (bAutoStartMatch)
	{
		StartMatch();
	}
}

void ANewEraBallGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(PostGoalTimerHandle);
		Timers.ClearTimer(HalfTimeTimerHandle);
		Timers.ClearTimer(ShootoutAttemptTimerHandle);
		Timers.ClearTimer(ShootoutInterAttemptTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ANewEraBallGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The clock only runs while the ball is live.
	if (MatchPhase == EMatchPhase::InPlay)
	{
		HalfClockSeconds += DeltaSeconds;
		if (HalfClockSeconds >= HalfDurationSeconds)
		{
			HalfClockSeconds = HalfDurationSeconds;
			EndHalf();
		}
	}

	if (bShootoutAttemptLive)
	{
		if (const UWorld* World = GetWorld())
		{
			ShootoutAttemptTimeRemaining = FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(ShootoutAttemptTimerHandle));
		}
	}
}

// ----------------------------------------------------------------------
// Team size & setup
// ----------------------------------------------------------------------

bool ANewEraBallGameMode::SetTeamSize(ETeamSizeFormat NewFormat)
{
	if (MatchPhase != EMatchPhase::PreMatch)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("SetTeamSize refused: team size can only change before the match starts (phase %d)."), static_cast<int32>(MatchPhase));
		return false;
	}

	if (!FTeamConfig::IsSupportedFormat(NewFormat))
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("SetTeamSize refused: unsupported format %d."), static_cast<int32>(NewFormat));
		return false;
	}

	TeamConfig.Format = NewFormat;
	UE_LOG(LogNewEraBall, Log, TEXT("Team size set to %d per side."), GetPlayersPerTeam());

	// Teams that were already spawned are rebuilt so the player count matches the new format.
	if (SpawnedPlayers.Num() > 0)
	{
		SpawnTeams();
	}

	return true;
}

TSubclassOf<ANewEraBallPlayer> ANewEraBallGameMode::ResolvePlayerClass(ETeamSide Team, bool bGoalkeeper) const
{
	const FTeamIdentity& Identity = TeamConfig.GetIdentity(Team);

	if (bGoalkeeper && Identity.GoalkeeperClass)
	{
		return Identity.GoalkeeperClass;
	}
	if (Identity.OutfieldPlayerClass)
	{
		return Identity.OutfieldPlayerClass;
	}
	return DefaultPlayerClass;
}

ANewEraBallPlayer* ANewEraBallGameMode::SpawnPlayer(ETeamSide Team, int32 SlotIndex, bool bGoalkeeper)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const TSubclassOf<ANewEraBallPlayer> PlayerClass = ResolvePlayerClass(Team, bGoalkeeper);
	if (!PlayerClass)
	{
		UE_LOG(LogNewEraBall, Error, TEXT("SpawnPlayer: no player class configured for %s slot %d. Set DefaultPlayerClass or the team's player classes."),
			Team == ETeamSide::Home ? TEXT("Home") : TEXT("Away"), SlotIndex);
		return nullptr;
	}

	const FTransform SpawnTransform = GetFormationTransform(Team, SlotIndex);

	ANewEraBallPlayer* Player = World->SpawnActorDeferred<ANewEraBallPlayer>(
		PlayerClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!Player)
	{
		return nullptr;
	}

	// Configure identity before BeginPlay so the player can self-register consistently.
	Player->SetTeam(Team);
	Player->SetIsGoalkeeper(bGoalkeeper);
	Player->SetFormationSlot(SlotIndex);
	Player->SetSquadNumber(SlotIndex + 1);

	UGameplayStatics::FinishSpawningActor(Player, SpawnTransform);

	SpawnedPlayers.Add(Player);
	RegisterPlayer(Player, Team);
	return Player;
}

void ANewEraBallGameMode::SpawnTeams()
{
	DespawnTeams();

	const int32 PlayersPerTeam = GetPlayersPerTeam();
	const ETeamSide Sides[] = { ETeamSide::Home, ETeamSide::Away };

	for (ETeamSide Side : Sides)
	{
		// Slot 0 is always the goalkeeper; the remaining slots are outfield players.
		for (int32 Slot = 0; Slot < PlayersPerTeam; ++Slot)
		{
			SpawnPlayer(Side, Slot, Slot == 0);
		}
	}

	UE_LOG(LogNewEraBall, Log, TEXT("Spawned teams: %d per side, %d total."), PlayersPerTeam, SpawnedPlayers.Num());
	OnTeamsSpawned.Broadcast(PlayersPerTeam, SpawnedPlayers.Num());

	if (bAutoPossessFirstHomePlayer)
	{
		const TArray<ANewEraBallPlayer*> HomeOutfield = GetOutfieldPlayers(ETeamSide::Home);
		if (HomeOutfield.Num() > 0)
		{
			AssignHumanControl(HomeOutfield[0], 0);
		}
	}
}

void ANewEraBallGameMode::DespawnTeams()
{
	for (int32 Index = SpawnedPlayers.Num() - 1; Index >= 0; --Index)
	{
		ANewEraBallPlayer* Player = SpawnedPlayers[Index];
		if (Player)
		{
			UnregisterPlayer(Player);
			Player->Destroy();
		}
	}
	SpawnedPlayers.Reset();
}

void ANewEraBallGameMode::RegisterPlayer(ANewEraBallPlayer* Player, ETeamSide Team)
{
	if (!Player || Team == ETeamSide::None)
	{
		return;
	}

	HomePlayers.Remove(Player);
	AwayPlayers.Remove(Player);

	if (Team == ETeamSide::Home)
	{
		HomePlayers.Add(Player);
	}
	else
	{
		AwayPlayers.Add(Player);
	}

	if (Player->GetTeam() != Team)
	{
		Player->SetTeam(Team);
	}
}

void ANewEraBallGameMode::UnregisterPlayer(ANewEraBallPlayer* Player)
{
	if (!Player)
	{
		return;
	}
	HomePlayers.Remove(Player);
	AwayPlayers.Remove(Player);
	SpawnedPlayers.Remove(Player);
}

void ANewEraBallGameMode::RegisterBall(ANewEraBallBall* Ball)
{
	if (!Ball)
	{
		return;
	}

	if (MatchBall && MatchBall != Ball)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("RegisterBall: a match ball is already registered (%s). Ignoring %s."), *GetNameSafe(MatchBall), *GetNameSafe(Ball));
		return;
	}

	MatchBall = Ball;
}

void ANewEraBallGameMode::RegisterBroadcastCamera(ANewEraBallCamera* Camera)
{
	if (Camera)
	{
		BroadcastCamera = Camera;
	}
}

ANewEraBallBall* ANewEraBallGameMode::SpawnMatchBall()
{
	if (MatchBall)
	{
		return MatchBall;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Prefer a ball already placed in the level.
	for (TActorIterator<ANewEraBallBall> It(World); It; ++It)
	{
		RegisterBall(*It);
		if (MatchBall)
		{
			return MatchBall;
		}
	}

	if (!BallClass)
	{
		UE_LOG(LogNewEraBall, Error, TEXT("SpawnMatchBall: no BallClass configured and no ball found in the level."));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	const FVector SpawnLocation = PitchCenter + FVector(0.0f, 0.0f, 50.0f);
	ANewEraBallBall* Ball = World->SpawnActor<ANewEraBallBall>(BallClass, SpawnLocation, FRotator::ZeroRotator, Params);
	RegisterBall(Ball);
	return MatchBall;
}

TArray<ANewEraBallPlayer*> ANewEraBallGameMode::GetTeamPlayers(ETeamSide Team) const
{
	TArray<ANewEraBallPlayer*> Result;
	const TArray<TObjectPtr<ANewEraBallPlayer>>& Source = (Team == ETeamSide::Away) ? AwayPlayers : HomePlayers;
	if (Team == ETeamSide::None)
	{
		return Result;
	}

	Result.Reserve(Source.Num());
	for (const TObjectPtr<ANewEraBallPlayer>& Player : Source)
	{
		if (Player)
		{
			Result.Add(Player);
		}
	}
	return Result;
}

TArray<ANewEraBallPlayer*> ANewEraBallGameMode::GetAllPlayers() const
{
	TArray<ANewEraBallPlayer*> Result = GetTeamPlayers(ETeamSide::Home);
	Result.Append(GetTeamPlayers(ETeamSide::Away));
	return Result;
}

ANewEraBallPlayer* ANewEraBallGameMode::GetGoalkeeper(ETeamSide Team) const
{
	for (ANewEraBallPlayer* Player : GetTeamPlayers(Team))
	{
		if (Player->IsGoalkeeper())
		{
			return Player;
		}
	}
	return nullptr;
}

TArray<ANewEraBallPlayer*> ANewEraBallGameMode::GetOutfieldPlayers(ETeamSide Team) const
{
	TArray<ANewEraBallPlayer*> Result;
	for (ANewEraBallPlayer* Player : GetTeamPlayers(Team))
	{
		if (!Player->IsGoalkeeper())
		{
			Result.Add(Player);
		}
	}
	return Result;
}

void ANewEraBallGameMode::AssignHumanControl(ANewEraBallPlayer* Player, int32 LocalPlayerIndex)
{
	if (!Player)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, LocalPlayerIndex);
	if (!PC)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("AssignHumanControl: no player controller at index %d."), LocalPlayerIndex);
		return;
	}

	if (PC->GetPawn() != Player)
	{
		PC->Possess(Player);
	}

	if (BroadcastCamera)
	{
		BroadcastCamera->ActivateForController(PC);
	}
}

FTransform ANewEraBallGameMode::GetFormationTransform_Implementation(ETeamSide Team, int32 SlotIndex) const
{
	const float AttackSign = GetAttackDirectionSign(Team);
	const float OwnEndSign = -AttackSign;
	const float HalfLength = Pitch.GetHalfLength();
	const float OwnGoalLineX = PitchCenter.X + OwnEndSign * HalfLength;
	const float GroundZ = PitchCenter.Z + SpawnHeightOffset;

	// Players face the goal they attack.
	const FRotator Facing = FRotationMatrix::MakeFromX(FVector(AttackSign, 0.0f, 0.0f)).Rotator();

	// Slot 0: goalkeeper, a short distance off the goal line.
	if (SlotIndex <= 0)
	{
		const FVector Location(OwnGoalLineX + AttackSign * 150.0f, PitchCenter.Y, GroundZ);
		return FTransform(Facing, Location);
	}

	// Outfield players are split into rows of at most three, deepest rows first.
	const int32 OutfieldCount = FMath::Max(1, GetOutfieldPlayersPerTeam());
	const int32 OutfieldIndex = FMath::Clamp(SlotIndex - 1, 0, OutfieldCount - 1);

	const int32 RowCount = FMath::Max(1, FMath::DivideAndRoundUp(OutfieldCount, 3));
	const int32 BaseRowSize = OutfieldCount / RowCount;
	const int32 ExtraPlayers = OutfieldCount % RowCount;

	int32 RowIndex = 0;
	int32 IndexInRow = OutfieldIndex;
	int32 RowSize = BaseRowSize + (ExtraPlayers > 0 ? 1 : 0);
	while (IndexInRow >= RowSize)
	{
		IndexInRow -= RowSize;
		++RowIndex;
		RowSize = BaseRowSize + (RowIndex < ExtraPlayers ? 1 : 0);
	}

	// Rows sit between 25% and 85% of the way from the own goal line to the midfield line.
	const float RowFraction = (RowCount > 1) ? (0.25f + 0.6f * (static_cast<float>(RowIndex) / static_cast<float>(RowCount - 1))) : 0.55f;
	const float X = OwnGoalLineX + AttackSign * HalfLength * RowFraction;

	const float Spacing = Pitch.Width / static_cast<float>(RowSize + 1);
	const float Y = PitchCenter.Y + (static_cast<float>(IndexInRow) - static_cast<float>(RowSize - 1) * 0.5f) * Spacing;

	return FTransform(Facing, FVector(X, Y, GroundZ));
}

void ANewEraBallGameMode::ResetPositionsForKickoff_Implementation(ETeamSide KickoffTeam)
{
	for (ANewEraBallPlayer* Player : GetAllPlayers())
	{
		Player->ResetForRestart(GetFormationTransform(Player->GetTeam(), Player->GetFormationSlot()));
	}

	// The kickoff taker stands just behind the centre spot on their own side.
	const TArray<ANewEraBallPlayer*> Takers = GetOutfieldPlayers(KickoffTeam);
	if (Takers.Num() > 0)
	{
		const FVector AttackDir = GetAttackDirection(KickoffTeam);
		const FVector Location = PitchCenter - AttackDir * 150.0f + FVector(0.0f, 0.0f, SpawnHeightOffset);
		Takers[0]->ResetForRestart(FTransform(AttackDir.Rotation(), Location));
	}

	PlaceBall(PitchCenter);
}

// ----------------------------------------------------------------------
// Match flow
// ----------------------------------------------------------------------

void ANewEraBallGameMode::SetMatchPhase(EMatchPhase NewPhase)
{
	if (MatchPhase == NewPhase)
	{
		return;
	}

	const EMatchPhase OldPhase = MatchPhase;
	MatchPhase = NewPhase;
	UE_LOG(LogNewEraBall, Log, TEXT("Match phase %d -> %d"), static_cast<int32>(OldPhase), static_cast<int32>(NewPhase));
	OnMatchPhaseChanged.Broadcast(NewPhase, OldPhase);
}

void ANewEraBallGameMode::StartMatch()
{
	if (MatchPhase != EMatchPhase::PreMatch)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("StartMatch ignored: match already started."));
		return;
	}

	HomePoints = 0;
	AwayPoints = 0;
	CurrentHalf = 1;
	HalfClockSeconds = 0.0f;
	bHomeAttacksPositiveX = true;

	ShootoutRound = 0;
	HomeShootoutGoals = 0;
	AwayShootoutGoals = 0;
	HomeShootoutAttempts = 0;
	AwayShootoutAttempts = 0;
	HomeShootoutAttackerCursor = 0;
	AwayShootoutAttackerCursor = 0;
	bShootoutAttemptLive = false;

	if (!MatchBall)
	{
		SpawnMatchBall();
	}

	OnScoreChanged.Broadcast(HomePoints, AwayPoints);

	HalfKickoffTeam = (FirstKickoffTeam == ETeamSide::None) ? ETeamSide::Home : FirstKickoffTeam;
	Kickoff(HalfKickoffTeam);
}

void ANewEraBallGameMode::Kickoff(ETeamSide KickoffTeam)
{
	if (MatchPhase == EMatchPhase::Finished)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PostGoalTimerHandle);
	}

	if (KickoffTeam == ETeamSide::None)
	{
		KickoffTeam = ETeamSide::Home;
	}

	PenaltyTeam = ETeamSide::None;
	PenaltyTaker = nullptr;
	bPenaltyKickTaken = false;

	ResetAllTouchCounters();
	ResetPositionsForKickoff(KickoffTeam);

	RestartTeam = KickoffTeam;
	RestartLocation = PitchCenter;
	SetPossession(KickoffTeam);
	SetMatchPhase(EMatchPhase::Restart);

	OnKickoff.Broadcast(KickoffTeam, CurrentHalf);
}

void ANewEraBallGameMode::EndHalf()
{
	if (MatchPhase != EMatchPhase::InPlay && MatchPhase != EMatchPhase::Restart && MatchPhase != EMatchPhase::GoalStoppage)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PostGoalTimerHandle);
	}

	if (MatchBall)
	{
		MatchBall->FreezeInPlace();
	}

	if (CurrentHalf >= NumberOfHalves)
	{
		EndRegulation();
		return;
	}

	SetMatchPhase(EMatchPhase::HalfTime);

	if (HalfTimeDurationSeconds <= 0.0f)
	{
		StartNextHalf();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HalfTimeTimerHandle, this, &ANewEraBallGameMode::OnHalfTimeElapsed, HalfTimeDurationSeconds, false);
	}
}

void ANewEraBallGameMode::OnHalfTimeElapsed()
{
	StartNextHalf();
}

void ANewEraBallGameMode::StartNextHalf()
{
	if (MatchPhase != EMatchPhase::HalfTime)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HalfTimeTimerHandle);
	}

	++CurrentHalf;
	HalfClockSeconds = 0.0f;

	if (bSwapEndsAtHalfTime)
	{
		bHomeAttacksPositiveX = !bHomeAttacksPositiveX;
	}

	HalfKickoffTeam = GetOpposingTeam(HalfKickoffTeam);
	Kickoff(HalfKickoffTeam);
}

void ANewEraBallGameMode::EndRegulation()
{
	const ETeamSide Leader = GetLeadingTeam();
	if (Leader != ETeamSide::None)
	{
		FinishMatch(Leader);
	}
	else
	{
		// No draws: a level score is settled by the 1v1 shootout.
		StartShootout();
	}
}

void ANewEraBallGameMode::FinishMatch(ETeamSide Winner)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(PostGoalTimerHandle);
		Timers.ClearTimer(HalfTimeTimerHandle);
		Timers.ClearTimer(ShootoutAttemptTimerHandle);
		Timers.ClearTimer(ShootoutInterAttemptTimerHandle);
	}

	bShootoutAttemptLive = false;
	ShootoutAttemptTimeRemaining = 0.0f;

	if (MatchBall)
	{
		MatchBall->FreezeInPlace();
	}

	SetMatchPhase(EMatchPhase::Finished);
	UE_LOG(LogNewEraBall, Log, TEXT("Match finished. Winner: %d. Points %d - %d. Shootout %d - %d."),
		static_cast<int32>(Winner), HomePoints, AwayPoints, HomeShootoutGoals, AwayShootoutGoals);
	OnMatchFinished.Broadcast(Winner, HomePoints, AwayPoints);
}

float ANewEraBallGameMode::GetMatchClockSeconds() const
{
	const int32 CompletedHalves = FMath::Max(0, CurrentHalf - 1);
	return CompletedHalves * HalfDurationSeconds + HalfClockSeconds;
}

bool ANewEraBallGameMode::CanPlayerPlayBall(const ANewEraBallPlayer* Player) const
{
	if (!Player)
	{
		return false;
	}

	const ETeamSide Team = Player->GetTeam();

	switch (MatchPhase)
	{
	case EMatchPhase::InPlay:
		return true;
	case EMatchPhase::Restart:
		return Team == RestartTeam;
	case EMatchPhase::PenaltyKick:
		return bPenaltyKickTaken || Team == PenaltyTeam;
	case EMatchPhase::Shootout:
		return bShootoutAttemptLive && (Player == ShootoutAttacker || Player == ShootoutGoalkeeper);
	default:
		return false;
	}
}

// ----------------------------------------------------------------------
// Scoring
// ----------------------------------------------------------------------

int32 ANewEraBallGameMode::GetPointsForScoreType(EScoreType ScoreType) const
{
	switch (ScoreType)
	{
	case EScoreType::StandardGoal:	return StandardGoalPoints;
	case EScoreType::LongRangeGoal:	return LongRangeGoalPoints;
	case EScoreType::Penalty:		return PenaltyPoints;
	default:						return 0;
	}
}

int32 ANewEraBallGameMode::GetPoints(ETeamSide Team) const
{
	switch (Team)
	{
	case ETeamSide::Home:	return HomePoints;
	case ETeamSide::Away:	return AwayPoints;
	default:				return 0;
	}
}

ETeamSide ANewEraBallGameMode::GetLeadingTeam() const
{
	if (HomePoints > AwayPoints)
	{
		return ETeamSide::Home;
	}
	if (AwayPoints > HomePoints)
	{
		return ETeamSide::Away;
	}
	return ETeamSide::None;
}

void ANewEraBallGameMode::AwardPoints(ETeamSide Team, EScoreType ScoreType)
{
	if (Team == ETeamSide::None)
	{
		return;
	}

	const int32 Points = GetPointsForScoreType(ScoreType);
	if (Team == ETeamSide::Home)
	{
		HomePoints += Points;
	}
	else
	{
		AwayPoints += Points;
	}

	UE_LOG(LogNewEraBall, Log, TEXT("%s +%d (%d). Score %d - %d."),
		Team == ETeamSide::Home ? TEXT("Home") : TEXT("Away"), Points, static_cast<int32>(ScoreType), HomePoints, AwayPoints);

	OnPointsAwarded.Broadcast(Team, ScoreType, Points);
	OnScoreChanged.Broadcast(HomePoints, AwayPoints);
}

void ANewEraBallGameMode::RegisterGoal(ETeamSide ScoringTeam, FVector ShotOrigin, bool bOwnGoal)
{
	if (ScoringTeam == ETeamSide::None || MatchPhase == EMatchPhase::Finished)
	{
		return;
	}

	const ETeamSide GoalOwner = GetOpposingTeam(ScoringTeam);

	EScoreType ScoreType;
	if (bOwnGoal && bOwnGoalAlwaysStandard)
	{
		ScoreType = EScoreType::StandardGoal;
	}
	else
	{
		ScoreType = IsInsideGoalArea(GoalOwner, ShotOrigin) ? EScoreType::StandardGoal : EScoreType::LongRangeGoal;
	}

	AwardPoints(ScoringTeam, ScoreType);

	// Dead ball, then the conceding team kicks off.
	if (MatchBall)
	{
		MatchBall->FreezeInPlace();
	}

	PendingKickoffTeam = GoalOwner;
	SetMatchPhase(EMatchPhase::GoalStoppage);

	if (PostGoalDelaySeconds <= 0.0f)
	{
		OnPostGoalDelayElapsed();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PostGoalTimerHandle, this, &ANewEraBallGameMode::OnPostGoalDelayElapsed, PostGoalDelaySeconds, false);
	}
}

void ANewEraBallGameMode::OnPostGoalDelayElapsed()
{
	if (MatchPhase != EMatchPhase::GoalStoppage)
	{
		return;
	}

	// A goal that lands on the final whistle ends the half instead of restarting play.
	if (HalfClockSeconds >= HalfDurationSeconds)
	{
		EndHalf();
		return;
	}

	Kickoff(PendingKickoffTeam);
}

void ANewEraBallGameMode::NotifyBallEnteredGoal(ETeamSide GoalOwnerTeam)
{
	if (GoalOwnerTeam == ETeamSide::None)
	{
		return;
	}

	const ETeamSide ScoringTeam = GetOpposingTeam(GoalOwnerTeam);

	switch (MatchPhase)
	{
	case EMatchPhase::InPlay:
	{
		const ETeamSide LastTouchTeam = MatchBall ? MatchBall->GetLastTouchTeam() : ETeamSide::None;
		const FVector ShotOrigin = MatchBall ? MatchBall->GetLastTouchLocation() : GetGoalCenter(GoalOwnerTeam);
		const bool bOwnGoal = (LastTouchTeam == GoalOwnerTeam);
		RegisterGoal(ScoringTeam, ShotOrigin, bOwnGoal);
		break;
	}
	case EMatchPhase::PenaltyKick:
		ResolvePenaltyKick(ScoringTeam == PenaltyTeam);
		break;
	case EMatchPhase::Shootout:
		if (bShootoutAttemptLive)
		{
			ResolveShootoutAttempt(ScoringTeam == ShootoutAttackingTeam);
		}
		break;
	default:
		// Dead ball phases: nothing to score.
		break;
	}
}

// ----------------------------------------------------------------------
// Rules
// ----------------------------------------------------------------------

void ANewEraBallGameMode::ResetAllTouchCounters()
{
	for (ANewEraBallPlayer* Player : GetAllPlayers())
	{
		if (UTouchCounterComponent* Counter = Player->GetTouchCounter())
		{
			Counter->ResetTouches();
		}
	}
}

void ANewEraBallGameMode::PlaceBall(const FVector& Location)
{
	if (MatchBall)
	{
		MatchBall->PlaceAt(Location);
	}
}

void ANewEraBallGameMode::ReleaseBall()
{
	if (MatchBall)
	{
		MatchBall->Release();
	}
}

void ANewEraBallGameMode::SetPossession(ETeamSide Team)
{
	if (PossessionTeam == Team)
	{
		return;
	}
	PossessionTeam = Team;
	OnPossessionChanged.Broadcast(Team);
}

ETouchViolationResult ANewEraBallGameMode::HandleTouchViolation(ANewEraBallPlayer* Offender, FVector BallLocation)
{
	if (!Offender || MatchPhase != EMatchPhase::InPlay)
	{
		return ETouchViolationResult::None;
	}

	const ETeamSide OffendingTeam = Offender->GetTeam();
	const ETeamSide BenefitingTeam = GetOpposingTeam(OffendingTeam);
	if (BenefitingTeam == ETeamSide::None)
	{
		return ETouchViolationResult::None;
	}

	ETouchViolationResult Result;
	if (IsInsideGoalArea(OffendingTeam, BallLocation))
	{
		// Violation inside the offender's own goal area: penalty to the opposing team.
		Result = ETouchViolationResult::Penalty;
		AwardPenaltyKick(BenefitingTeam, Offender);
	}
	else
	{
		// Violation in open play behind midfield: automatic turnover where it happened.
		Result = ETouchViolationResult::Turnover;
		AwardTurnover(BenefitingTeam, ETurnoverReason::TouchViolation, ClampToPitch(BallLocation, 50.0f));
	}

	OnTouchViolationAdjudicated.Broadcast(Offender, Result);
	return Result;
}

void ANewEraBallGameMode::AwardTurnover(ETeamSide NewPossessionTeam, ETurnoverReason Reason, FVector InRestartLocation)
{
	if (NewPossessionTeam == ETeamSide::None || MatchPhase == EMatchPhase::Finished || MatchPhase == EMatchPhase::PreMatch)
	{
		return;
	}

	ResetAllTouchCounters();

	RestartTeam = NewPossessionTeam;
	RestartLocation = InRestartLocation;
	PenaltyTeam = ETeamSide::None;
	PenaltyTaker = nullptr;
	bPenaltyKickTaken = false;

	PlaceBall(RestartLocation);
	SetPossession(NewPossessionTeam);
	SetMatchPhase(EMatchPhase::Restart);

	OnTurnover.Broadcast(NewPossessionTeam, Reason, RestartLocation);
}

void ANewEraBallGameMode::AwardPenaltyKick(ETeamSide TakingTeam, ANewEraBallPlayer* Offender)
{
	if (TakingTeam == ETeamSide::None || MatchPhase == EMatchPhase::Finished || MatchPhase == EMatchPhase::PreMatch)
	{
		return;
	}

	ResetAllTouchCounters();

	const ETeamSide GoalOwner = GetOpposingTeam(TakingTeam);
	const FVector Spot = GetPenaltySpot(GoalOwner);

	PenaltyTeam = TakingTeam;
	bPenaltyKickTaken = false;
	RestartTeam = TakingTeam;
	RestartLocation = Spot;

	// The first outfield player of the taking team takes it unless Blueprint reassigns the taker.
	const TArray<ANewEraBallPlayer*> Takers = GetOutfieldPlayers(TakingTeam);
	PenaltyTaker = Takers.Num() > 0 ? Takers[0] : nullptr;

	ResetPositionsForPenalty(TakingTeam, PenaltyTaker);
	PlaceBall(Spot);
	SetPossession(TakingTeam);
	SetMatchPhase(EMatchPhase::PenaltyKick);

	OnPenaltyAwarded.Broadcast(TakingTeam, Offender);
}

void ANewEraBallGameMode::ResetPositionsForPenalty_Implementation(ETeamSide TakingTeam, ANewEraBallPlayer* Taker)
{
	const ETeamSide GoalOwner = GetOpposingTeam(TakingTeam);
	const FVector AttackDir = GetAttackDirection(TakingTeam);
	const FVector Spot = GetPenaltySpot(GoalOwner);
	const FVector HeightOffset(0.0f, 0.0f, SpawnHeightOffset);

	for (ANewEraBallPlayer* Player : GetAllPlayers())
	{
		FTransform Target = GetFormationTransform(Player->GetTeam(), Player->GetFormationSlot());

		// Nobody except the taker and the goalkeeper stands inside the goal area.
		if (IsInsideGoalArea(GoalOwner, Target.GetLocation()))
		{
			const FVector Pushed = Spot - AttackDir * (Pitch.GoalAreaDepth + 200.0f);
			Target.SetLocation(FVector(Pushed.X, Target.GetLocation().Y, Target.GetLocation().Z));
		}

		Player->ResetForRestart(Target);
	}

	if (Taker)
	{
		Taker->ResetForRestart(FTransform(AttackDir.Rotation(), Spot - AttackDir * 200.0f + HeightOffset));
	}

	if (ANewEraBallPlayer* Keeper = GetGoalkeeper(GoalOwner))
	{
		const FVector KeeperLocation = GetGoalCenter(GoalOwner) - AttackDir * 50.0f + HeightOffset;
		Keeper->ResetForRestart(FTransform((-AttackDir).Rotation(), KeeperLocation));
	}
}

void ANewEraBallGameMode::ResolvePenaltyKick(bool bScored)
{
	if (MatchPhase != EMatchPhase::PenaltyKick)
	{
		return;
	}

	const ETeamSide TakingTeam = PenaltyTeam;
	const ETeamSide DefendingTeam = GetOpposingTeam(TakingTeam);

	PenaltyTeam = ETeamSide::None;
	PenaltyTaker = nullptr;
	bPenaltyKickTaken = false;

	if (bScored)
	{
		AwardPoints(TakingTeam, EScoreType::Penalty);

		if (MatchBall)
		{
			MatchBall->FreezeInPlace();
		}

		PendingKickoffTeam = DefendingTeam;
		SetMatchPhase(EMatchPhase::GoalStoppage);

		if (PostGoalDelaySeconds <= 0.0f)
		{
			OnPostGoalDelayElapsed();
		}
		else if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(PostGoalTimerHandle, this, &ANewEraBallGameMode::OnPostGoalDelayElapsed, PostGoalDelaySeconds, false);
		}
	}
	else
	{
		// Missed or saved: the defending team restarts from the edge of their goal area.
		const FVector GoalKickSpot = GetGoalCenter(DefendingTeam) + GetAttackDirection(DefendingTeam) * Pitch.GoalAreaDepth;
		AwardTurnover(DefendingTeam, ETurnoverReason::PenaltyMissed, GoalKickSpot);
	}
}

void ANewEraBallGameMode::NotifyBallTouched(ANewEraBallPlayer* Player, EBallTouchType TouchType, FVector TouchLocation)
{
	if (!Player)
	{
		return;
	}

	const ETeamSide Team = Player->GetTeam();

	switch (MatchPhase)
	{
	case EMatchPhase::Restart:
		if (Team == RestartTeam)
		{
			ReleaseBall();
			SetPossession(Team);
			SetMatchPhase(EMatchPhase::InPlay);
		}
		break;

	case EMatchPhase::InPlay:
		SetPossession(Team);
		break;

	case EMatchPhase::PenaltyKick:
		if (!bPenaltyKickTaken)
		{
			// Only the taking team may play the ball until it is struck. Defenders are held off by
			// CanPlayerPlayBall until then; after the strike a defender touch counts as a save.
			if (Team == PenaltyTeam)
			{
				ReleaseBall();
				if (TouchType == EBallTouchType::Shot || TouchType == EBallTouchType::Pass)
				{
					bPenaltyKickTaken = true;
				}
			}
		}
		else if (bDefenderTouchEndsPenalty && Team != PenaltyTeam)
		{
			ResolvePenaltyKick(false);
		}
		break;

	case EMatchPhase::Shootout:
		if (bShootoutAttemptLive)
		{
			if (Player == ShootoutAttacker)
			{
				ReleaseBall();
			}
			else if (Player == ShootoutGoalkeeper && bGoalkeeperTouchEndsShootoutAttempt)
			{
				ResolveShootoutAttempt(false);
			}
		}
		break;

	default:
		break;
	}
}

void ANewEraBallGameMode::NotifyBallOutOfPlay(FVector ExitLocation, ETeamSide LastTouchTeam)
{
	switch (MatchPhase)
	{
	case EMatchPhase::InPlay:
	{
		ETeamSide NewTeam = GetOpposingTeam(LastTouchTeam);
		if (NewTeam == ETeamSide::None)
		{
			NewTeam = ETeamSide::Home;
		}
		AwardTurnover(NewTeam, ETurnoverReason::OutOfPlay, ClampToPitch(ExitLocation, 50.0f));
		break;
	}
	case EMatchPhase::PenaltyKick:
		ResolvePenaltyKick(false);
		break;
	case EMatchPhase::Shootout:
		if (bShootoutAttemptLive)
		{
			ResolveShootoutAttempt(false);
		}
		break;
	case EMatchPhase::Restart:
		// A dead ball nudged off the pitch goes back to the restart spot.
		PlaceBall(RestartLocation);
		break;
	default:
		break;
	}
}

// ----------------------------------------------------------------------
// Geometry
// ----------------------------------------------------------------------

float ANewEraBallGameMode::GetAttackDirectionSign(ETeamSide Team) const
{
	switch (Team)
	{
	case ETeamSide::Home:	return bHomeAttacksPositiveX ? 1.0f : -1.0f;
	case ETeamSide::Away:	return bHomeAttacksPositiveX ? -1.0f : 1.0f;
	default:				return 0.0f;
	}
}

FVector ANewEraBallGameMode::GetAttackDirection(ETeamSide Team) const
{
	return FVector(GetAttackDirectionSign(Team), 0.0f, 0.0f);
}

bool ANewEraBallGameMode::IsInOwnHalf(ETeamSide Team, FVector Location) const
{
	const float Sign = GetAttackDirectionSign(Team);
	if (Sign == 0.0f)
	{
		return false;
	}
	// Strictly behind the midfield line on the side the team defends.
	return (Location.X - PitchCenter.X) * Sign < 0.0f;
}

bool ANewEraBallGameMode::IsInsideGoalArea(ETeamSide GoalOwner, FVector Location) const
{
	const float OwnEndSign = -GetAttackDirectionSign(GoalOwner);
	if (OwnEndSign == 0.0f)
	{
		return false;
	}

	const float GoalLineX = PitchCenter.X + OwnEndSign * Pitch.GetHalfLength();
	const float InwardDistance = (GoalLineX - Location.X) * OwnEndSign;
	const float LateralDistance = FMath::Abs(Location.Y - PitchCenter.Y);

	// A small negative inward distance keeps the goal mouth itself inside the area.
	return InwardDistance >= -100.0f
		&& InwardDistance <= Pitch.GoalAreaDepth
		&& LateralDistance <= Pitch.GoalAreaWidth * 0.5f;
}

bool ANewEraBallGameMode::IsInsidePitch(FVector Location, float Tolerance) const
{
	return FMath::Abs(Location.X - PitchCenter.X) <= Pitch.GetHalfLength() + Tolerance
		&& FMath::Abs(Location.Y - PitchCenter.Y) <= Pitch.GetHalfWidth() + Tolerance;
}

FVector ANewEraBallGameMode::GetGoalCenter(ETeamSide GoalOwner) const
{
	const float OwnEndSign = -GetAttackDirectionSign(GoalOwner);
	return FVector(PitchCenter.X + OwnEndSign * Pitch.GetHalfLength(), PitchCenter.Y, PitchCenter.Z);
}

FVector ANewEraBallGameMode::GetPenaltySpot(ETeamSide GoalOwner) const
{
	const float OwnEndSign = -GetAttackDirectionSign(GoalOwner);
	return FVector(PitchCenter.X + OwnEndSign * (Pitch.GetHalfLength() - Pitch.PenaltySpotDistance), PitchCenter.Y, PitchCenter.Z);
}

FVector ANewEraBallGameMode::ClampToPitch(FVector Location, float Inset) const
{
	const float HalfLength = FMath::Max(0.0f, Pitch.GetHalfLength() - Inset);
	const float HalfWidth = FMath::Max(0.0f, Pitch.GetHalfWidth() - Inset);
	return FVector(
		FMath::Clamp(Location.X, PitchCenter.X - HalfLength, PitchCenter.X + HalfLength),
		FMath::Clamp(Location.Y, PitchCenter.Y - HalfWidth, PitchCenter.Y + HalfWidth),
		PitchCenter.Z);
}

// ----------------------------------------------------------------------
// Shootout
// ----------------------------------------------------------------------

void ANewEraBallGameMode::StartShootout()
{
	if (MatchPhase == EMatchPhase::Finished)
	{
		return;
	}

	ShootoutRound = 0;
	HomeShootoutGoals = 0;
	AwayShootoutGoals = 0;
	HomeShootoutAttempts = 0;
	AwayShootoutAttempts = 0;
	HomeShootoutAttackerCursor = 0;
	AwayShootoutAttackerCursor = 0;
	bShootoutAttemptLive = false;

	SetMatchPhase(EMatchPhase::Shootout);
	BeginShootoutAttempt();
}

ETeamSide ANewEraBallGameMode::GetNextShootoutTeam() const
{
	const ETeamSide First = (ShootoutFirstTeam == ETeamSide::None) ? ETeamSide::Home : ShootoutFirstTeam;
	const ETeamSide Second = GetOpposingTeam(First);

	const int32 FirstAttempts = (First == ETeamSide::Home) ? HomeShootoutAttempts : AwayShootoutAttempts;
	const int32 SecondAttempts = (Second == ETeamSide::Home) ? HomeShootoutAttempts : AwayShootoutAttempts;

	return (FirstAttempts <= SecondAttempts) ? First : Second;
}

ANewEraBallPlayer* ANewEraBallGameMode::GetNextShootoutAttacker(ETeamSide Team)
{
	TArray<ANewEraBallPlayer*> Candidates = GetOutfieldPlayers(Team);
	if (Candidates.Num() == 0)
	{
		// Fall back to anyone on the team, goalkeeper included.
		Candidates = GetTeamPlayers(Team);
	}
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	int32& Cursor = (Team == ETeamSide::Home) ? HomeShootoutAttackerCursor : AwayShootoutAttackerCursor;
	ANewEraBallPlayer* Attacker = Candidates[Cursor % Candidates.Num()];
	++Cursor;
	return Attacker;
}

bool ANewEraBallGameMode::IsShootoutDecided() const
{
	return HomeShootoutAttempts == AwayShootoutAttempts && HomeShootoutGoals != AwayShootoutGoals;
}

FVector ANewEraBallGameMode::GetShootoutStartLocation(ETeamSide AttackingTeam) const
{
	// The attacker starts on the centre spot and runs at the goalkeeper.
	return PitchCenter + FVector(0.0f, 0.0f, SpawnHeightOffset);
}

void ANewEraBallGameMode::ResetPositionsForShootout_Implementation(ANewEraBallPlayer* Attacker, ANewEraBallPlayer* Goalkeeper)
{
	const float BenchOffset = Pitch.GetHalfWidth() + 300.0f;

	// Everyone not involved waits on their team's touchline.
	for (ANewEraBallPlayer* Player : GetAllPlayers())
	{
		if (Player == Attacker || Player == Goalkeeper)
		{
			continue;
		}

		const float SideSign = (Player->GetTeam() == ETeamSide::Home) ? -1.0f : 1.0f;
		const float Slot = static_cast<float>(Player->GetFormationSlot());
		const FVector Location(
			PitchCenter.X - Pitch.GetHalfLength() * 0.5f + Slot * 200.0f,
			PitchCenter.Y + SideSign * BenchOffset,
			PitchCenter.Z + SpawnHeightOffset);
		const FRotator Facing = FRotationMatrix::MakeFromX(FVector(0.0f, -SideSign, 0.0f)).Rotator();
		Player->ResetForRestart(FTransform(Facing, Location));
	}

	if (Attacker)
	{
		const FVector AttackDir = GetAttackDirection(Attacker->GetTeam());
		Attacker->ResetForRestart(FTransform(AttackDir.Rotation(), GetShootoutStartLocation(Attacker->GetTeam())));
	}

	if (Goalkeeper)
	{
		const ETeamSide KeeperTeam = Goalkeeper->GetTeam();
		const FVector KeeperForward = GetAttackDirection(KeeperTeam);
		const FVector Location = GetGoalCenter(KeeperTeam) + KeeperForward * 50.0f + FVector(0.0f, 0.0f, SpawnHeightOffset);
		Goalkeeper->ResetForRestart(FTransform(KeeperForward.Rotation(), Location));
	}
}

void ANewEraBallGameMode::BeginShootoutAttempt()
{
	if (MatchPhase != EMatchPhase::Shootout || bShootoutAttemptLive)
	{
		return;
	}

	if (IsShootoutDecided())
	{
		FinishMatch(HomeShootoutGoals > AwayShootoutGoals ? ETeamSide::Home : ETeamSide::Away);
		return;
	}

	const ETeamSide Team = GetNextShootoutTeam();
	if (HomeShootoutAttempts == AwayShootoutAttempts)
	{
		++ShootoutRound;
	}

	ANewEraBallPlayer* Attacker = GetNextShootoutAttacker(Team);
	if (!Attacker)
	{
		UE_LOG(LogNewEraBall, Error, TEXT("BeginShootoutAttempt: no players available for team %d. Finishing match undecided."), static_cast<int32>(Team));
		FinishMatch(ETeamSide::None);
		return;
	}

	ANewEraBallPlayer* Keeper = GetGoalkeeper(GetOpposingTeam(Team));
	if (!Keeper)
	{
		UE_LOG(LogNewEraBall, Warning, TEXT("BeginShootoutAttempt: opposing team has no goalkeeper. Attempt runs against an empty net."));
	}

	ShootoutAttackingTeam = Team;
	ShootoutAttacker = Attacker;
	ShootoutGoalkeeper = Keeper;
	RestartTeam = Team;

	ResetAllTouchCounters();
	ResetPositionsForShootout(Attacker, Keeper);

	// Ball just ahead of the attacker so the first touch is already past midfield.
	const FVector BallSpot = PitchCenter + GetAttackDirection(Team) * 100.0f;
	RestartLocation = BallSpot;
	PlaceBall(BallSpot);
	SetPossession(Team);

	bShootoutAttemptLive = true;
	ShootoutAttemptTimeRemaining = ShootoutAttemptTimeLimit;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ShootoutAttemptTimerHandle, this, &ANewEraBallGameMode::OnShootoutAttemptTimeout, ShootoutAttemptTimeLimit, false);
	}

	UE_LOG(LogNewEraBall, Log, TEXT("Shootout round %d: %s attempt by %s."), ShootoutRound,
		Team == ETeamSide::Home ? TEXT("Home") : TEXT("Away"), *GetNameSafe(Attacker));

	OnShootoutAttemptStarted.Broadcast(Team, Attacker, Keeper);
}

void ANewEraBallGameMode::OnShootoutAttemptTimeout()
{
	ResolveShootoutAttempt(false);
}

void ANewEraBallGameMode::ResolveShootoutAttempt(bool bScored)
{
	if (!bShootoutAttemptLive)
	{
		return;
	}

	bShootoutAttemptLive = false;
	ShootoutAttemptTimeRemaining = 0.0f;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShootoutAttemptTimerHandle);
	}

	const ETeamSide Team = ShootoutAttackingTeam;
	if (Team == ETeamSide::Home)
	{
		++HomeShootoutAttempts;
		if (bScored) { ++HomeShootoutGoals; }
	}
	else
	{
		++AwayShootoutAttempts;
		if (bScored) { ++AwayShootoutGoals; }
	}

	if (MatchBall)
	{
		MatchBall->FreezeInPlace();
	}

	UE_LOG(LogNewEraBall, Log, TEXT("Shootout attempt %s. Shootout score %d - %d."), bScored ? TEXT("scored") : TEXT("missed"), HomeShootoutGoals, AwayShootoutGoals);
	OnShootoutAttemptResolved.Broadcast(Team, bScored, HomeShootoutGoals, AwayShootoutGoals);

	ShootoutAttacker = nullptr;
	ShootoutGoalkeeper = nullptr;
	ShootoutAttackingTeam = ETeamSide::None;

	if (IsShootoutDecided())
	{
		FinishMatch(HomeShootoutGoals > AwayShootoutGoals ? ETeamSide::Home : ETeamSide::Away);
		return;
	}

	if (ShootoutInterAttemptDelay <= 0.0f)
	{
		BeginShootoutAttempt();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ShootoutInterAttemptTimerHandle, this, &ANewEraBallGameMode::OnShootoutInterAttemptDelayElapsed, ShootoutInterAttemptDelay, false);
	}
}

void ANewEraBallGameMode::OnShootoutInterAttemptDelayElapsed()
{
	BeginShootoutAttempt();
}
