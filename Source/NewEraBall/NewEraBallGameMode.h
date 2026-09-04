// NewEraBall - Match GameMode
// Owns the match: team sizes, spawning, phases, clock, scoring in points, 2-touch rule
// adjudication, penalties, turnovers and the 1v1 shootout used to resolve tied matches.
// Every rule is deterministic and skill based. No random numbers are used anywhere.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NewEraBallTypes.h"
#include "NewEraBallGameMode.generated.h"

class ANewEraBallPlayer;
class ANewEraBallBall;
class ANewEraBallCamera;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, int32, HomePoints, int32, AwayPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPointsAwarded, ETeamSide, Team, EScoreType, ScoreType, int32, Points);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchPhaseChanged, EMatchPhase, NewPhase, EMatchPhase, OldPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTurnover, ETeamSide, NewPossessionTeam, ETurnoverReason, Reason, FVector, RestartLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPenaltyAwarded, ETeamSide, TakingTeam, ANewEraBallPlayer*, Offender);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTouchViolationAdjudicated, ANewEraBallPlayer*, Offender, ETouchViolationResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPossessionChanged, ETeamSide, PossessionTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShootoutAttemptStarted, ETeamSide, AttackingTeam, ANewEraBallPlayer*, Attacker, ANewEraBallPlayer*, Goalkeeper);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnShootoutAttemptResolved, ETeamSide, AttackingTeam, bool, bScored, int32, HomeShootoutGoals, int32, AwayShootoutGoals);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMatchFinished, ETeamSide, Winner, int32, HomePoints, int32, AwayPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKickoff, ETeamSide, KickoffTeam, int32, Half);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamsSpawned, int32, PlayersPerTeam, int32, TotalPlayers);

/**
 * Match controller for NewEraBall.
 *
 * Coordinate convention: the pitch is centred on PitchCenter. Length runs along world X, width
 * along world Y. Home attacks the +X goal in the first half and Away the -X goal; ends swap at
 * half time when bSwapEndsAtHalfTime is set. Use the geometry helpers rather than assuming axes.
 */
UCLASS()
class NEWERABALL_API ANewEraBallGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANewEraBallGameMode();

	//~ Begin AActor / AGameModeBase interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor / AGameModeBase interface

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Team size format and team identities. Format may only change while in PreMatch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	FTeamConfig TeamConfig;

	/** Default class used when a team identity does not specify its own player classes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	TSubclassOf<ANewEraBallPlayer> DefaultPlayerClass;

	/** Ball class spawned by SpawnMatchBall when no ball has been registered from the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	TSubclassOf<ANewEraBallBall> BallClass;

	/** Spawn both teams automatically in BeginPlay using TeamConfig. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	bool bAutoSpawnTeams = true;

	/** Spawn a ball automatically in BeginPlay if the level did not register one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	bool bAutoSpawnBall = true;

	/** After spawning, give local player 0 control of the first Home outfield player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams")
	bool bAutoPossessFirstHomePlayer = true;

	/** Height above the pitch surface at which players are spawned and repositioned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Teams", meta = (Units = "cm"))
	float SpawnHeightOffset = 100.0f;

	/** Start the match automatically once BeginPlay has finished setting up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NewEraBall|Match")
	bool bAutoStartMatch = false;

	/** Pitch geometry in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Pitch")
	FPitchDimensions Pitch;

	/** World location of the centre spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Pitch")
	FVector PitchCenter = FVector::ZeroVector;

	/** Number of halves in regulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match", meta = (ClampMin = "1", ClampMax = "4"))
	int32 NumberOfHalves = 2;

	/** Length of one half in seconds of live play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match", meta = (ClampMin = "10", Units = "s"))
	float HalfDurationSeconds = 1200.0f;

	/** Length of the half time break. The next half starts automatically when it elapses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match", meta = (ClampMin = "0", Units = "s"))
	float HalfTimeDurationSeconds = 10.0f;

	/** Delay between a goal and the following kickoff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match", meta = (ClampMin = "0", Units = "s"))
	float PostGoalDelaySeconds = 4.0f;

	/** Swap attacking ends between halves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match")
	bool bSwapEndsAtHalfTime = true;

	/** Team that kicks off the first half. Away kicks off the next half. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Match")
	ETeamSide FirstKickoffTeam = ETeamSide::Home;

	/** Maximum touches allowed behind the midfield line, for every player including the goalkeeper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules", meta = (ClampMin = "1"))
	int32 MaxTouchesBehindMidfield = 2;

	/** Points for a goal scored from inside the goal area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Scoring", meta = (ClampMin = "0"))
	int32 StandardGoalPoints = 2;

	/** Points for a goal scored from outside the goal area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Scoring", meta = (ClampMin = "0"))
	int32 LongRangeGoalPoints = 3;

	/** Points for a converted penalty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Scoring", meta = (ClampMin = "0"))
	int32 PenaltyPoints = 1;

	/** Own goals always score as a standard goal regardless of where the ball was last touched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Scoring")
	bool bOwnGoalAlwaysStandard = true;

	/** Seconds the shootout attacker has to score once released from midfield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Shootout", meta = (ClampMin = "1", Units = "s"))
	float ShootoutAttemptTimeLimit = 5.0f;

	/** Delay between shootout attempts so players can be repositioned and the result shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Shootout", meta = (ClampMin = "0", Units = "s"))
	float ShootoutInterAttemptDelay = 3.0f;

	/** Team that takes the first attempt of every shootout round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Shootout")
	ETeamSide ShootoutFirstTeam = ETeamSide::Home;

	/** A goalkeeper touch ends the live shootout attempt as a save. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules|Shootout")
	bool bGoalkeeperTouchEndsShootoutAttempt = true;

	/** A defending-team touch after the penalty is struck resolves it as saved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NewEraBall|Rules")
	bool bDefenderTouchEndsPenalty = true;

	// ------------------------------------------------------------------
	// Runtime state
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	EMatchPhase MatchPhase = EMatchPhase::PreMatch;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	int32 HomePoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	int32 AwayPoints = 0;

	/** 1-based index of the half being played. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	int32 CurrentHalf = 0;

	/** Seconds of live play elapsed in the current half. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	float HalfClockSeconds = 0.0f;

	/** Team currently credited with possession. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	ETeamSide PossessionTeam = ETeamSide::None;

	/** Team that must play the ball while MatchPhase is Restart. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	ETeamSide RestartTeam = ETeamSide::None;

	/** Where the current restart takes place. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	FVector RestartLocation = FVector::ZeroVector;

	/** Team taking the penalty while MatchPhase is PenaltyKick. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	ETeamSide PenaltyTeam = ETeamSide::None;

	/** Player designated to take the current penalty. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	TObjectPtr<ANewEraBallPlayer> PenaltyTaker;

	/** True once the penalty ball has been struck. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	bool bPenaltyKickTaken = false;

	/** True while Home attacks the +X goal. Flips at half time when ends are swapped. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	bool bHomeAttacksPositiveX = true;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	int32 ShootoutRound = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	int32 HomeShootoutGoals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	int32 AwayShootoutGoals = 0;

	/** Team attacking in the shootout attempt currently in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	ETeamSide ShootoutAttackingTeam = ETeamSide::None;

	/** Attacker for the current shootout attempt. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	TObjectPtr<ANewEraBallPlayer> ShootoutAttacker;

	/** Goalkeeper defending the current shootout attempt. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	TObjectPtr<ANewEraBallPlayer> ShootoutGoalkeeper;

	/** Seconds remaining in the current shootout attempt. Zero when no attempt is live. */
	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State|Shootout")
	float ShootoutAttemptTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	TArray<TObjectPtr<ANewEraBallPlayer>> HomePlayers;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	TArray<TObjectPtr<ANewEraBallPlayer>> AwayPlayers;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	TObjectPtr<ANewEraBallBall> MatchBall;

	UPROPERTY(BlueprintReadOnly, Category = "NewEraBall|State")
	TObjectPtr<ANewEraBallCamera> BroadcastCamera;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPointsAwarded OnPointsAwarded;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTurnover OnTurnover;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPenaltyAwarded OnPenaltyAwarded;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTouchViolationAdjudicated OnTouchViolationAdjudicated;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnPossessionChanged OnPossessionChanged;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnKickoff OnKickoff;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnTeamsSpawned OnTeamsSpawned;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events|Shootout")
	FOnShootoutAttemptStarted OnShootoutAttemptStarted;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events|Shootout")
	FOnShootoutAttemptResolved OnShootoutAttemptResolved;

	UPROPERTY(BlueprintAssignable, Category = "NewEraBall|Events")
	FOnMatchFinished OnMatchFinished;

	// ------------------------------------------------------------------
	// Team size & setup
	// ------------------------------------------------------------------

	/** Change the match format. Only allowed in PreMatch. Returns false if the change was refused. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	bool SetTeamSize(ETeamSizeFormat NewFormat);

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	ETeamSizeFormat GetTeamSize() const { return TeamConfig.Format; }

	/** Players per team including the goalkeeper for the current format. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	int32 GetPlayersPerTeam() const { return TeamConfig.GetPlayersPerTeam(); }

	/** Outfield players per team for the current format. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	int32 GetOutfieldPlayersPerTeam() const { return TeamConfig.GetOutfieldPlayersPerTeam(); }

	/** Spawn both teams according to TeamConfig. Destroys any players previously spawned by this GameMode. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void SpawnTeams();

	/** Remove every player spawned by this GameMode. Level-placed players are left alone. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void DespawnTeams();

	/** Register a player that already exists in the level (or was spawned elsewhere). */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void RegisterPlayer(ANewEraBallPlayer* Player, ETeamSide Team);

	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void UnregisterPlayer(ANewEraBallPlayer* Player);

	/** Register the match ball. Called by the ball itself on BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void RegisterBall(ANewEraBallBall* Ball);

	/** Register the broadcast camera. Called by the camera on BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void RegisterBroadcastCamera(ANewEraBallCamera* Camera);

	/** Spawn a ball at the centre spot if none is registered. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	ANewEraBallBall* SpawnMatchBall();

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	TArray<ANewEraBallPlayer*> GetTeamPlayers(ETeamSide Team) const;

	/** Every registered player on both teams. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	TArray<ANewEraBallPlayer*> GetAllPlayers() const;

	/** Give a local player controller control of a team player and switch it to the broadcast camera. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Teams")
	void AssignHumanControl(ANewEraBallPlayer* Player, int32 LocalPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	ANewEraBallPlayer* GetGoalkeeper(ETeamSide Team) const;

	/** Outfield players of a team in registration order. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	TArray<ANewEraBallPlayer*> GetOutfieldPlayers(ETeamSide Team) const;

	/**
	 * Formation transform for a player slot. Slot 0 is the goalkeeper. Outfield slots are laid out
	 * in lines across the team's own half. Override in Blueprint for custom formations.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "NewEraBall|Teams")
	FTransform GetFormationTransform(ETeamSide Team, int32 SlotIndex) const;

	/** Move every registered player to their formation slot and put the ball on the centre spot. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NewEraBall|Teams")
	void ResetPositionsForKickoff(ETeamSide KickoffTeam);

	// ------------------------------------------------------------------
	// Match flow
	// ------------------------------------------------------------------

	/** Begin the match from PreMatch: resets scores and clock, then kicks off the first half. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void StartMatch();

	/** Set up a kickoff for the given team. The ball is dead until that team plays it. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void Kickoff(ETeamSide KickoffTeam);

	/** Ends the current half. Moves to HalfTime, or to EndRegulation after the final half. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void EndHalf();

	/** Start the next half after a HalfTime break. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void StartNextHalf();

	/** Called when regulation ends. Finishes the match, or starts a shootout if points are level. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void EndRegulation();

	/** Finish the match immediately with the given winner. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Match")
	void FinishMatch(ETeamSide Winner);

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Match")
	bool IsBallLive() const { return MatchPhase == EMatchPhase::InPlay; }

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Match")
	float GetHalfTimeRemaining() const { return FMath::Max(0.0f, HalfDurationSeconds - HalfClockSeconds); }

	/** Total seconds of live play elapsed across all halves so far. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Match")
	float GetMatchClockSeconds() const;

	/**
	 * True if a player of this team is allowed to play the ball right now. During a Restart only the
	 * restart team may play it; during a penalty only the taking team; during a shootout only the two
	 * participants.
	 */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Match")
	bool CanPlayerPlayBall(const ANewEraBallPlayer* Player) const;

	// ------------------------------------------------------------------
	// Scoring
	// ------------------------------------------------------------------

	/**
	 * Report that the ball crossed the goal line of the given goal. Called by the goal trigger.
	 * Determines the scoring team and point value from the ball's last touch, or resolves the
	 * current penalty / shootout attempt when one is in progress.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Scoring")
	void NotifyBallEnteredGoal(ETeamSide GoalOwnerTeam);

	/** Award a goal to a team. ShotOrigin decides between standard and long range points. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Scoring")
	void RegisterGoal(ETeamSide ScoringTeam, FVector ShotOrigin, bool bOwnGoal = false);

	/** Add the points for a score type to a team's total and broadcast events. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Scoring")
	void AwardPoints(ETeamSide Team, EScoreType ScoreType);

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Scoring")
	int32 GetPointsForScoreType(EScoreType ScoreType) const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Scoring")
	int32 GetPoints(ETeamSide Team) const;

	/** Team with more points, or None when level. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Scoring")
	ETeamSide GetLeadingTeam() const;

	// ------------------------------------------------------------------
	// Rules: touches, turnovers, penalties, possession
	// ------------------------------------------------------------------

	/**
	 * Adjudicate a 2-touch violation reported by a UTouchCounterComponent. Inside the offender's own
	 * goal area this awards a penalty to the opposing team; anywhere else behind midfield it is an
	 * automatic turnover. Returns what was applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	ETouchViolationResult HandleTouchViolation(ANewEraBallPlayer* Offender, FVector BallLocation);

	/** Give possession to a team with a dead-ball restart at the given location. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void AwardTurnover(ETeamSide NewPossessionTeam, ETurnoverReason Reason, FVector InRestartLocation);

	/** Award a penalty kick to a team. The ball is placed on the spot in front of the opposing goal. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void AwardPenaltyKick(ETeamSide TakingTeam, ANewEraBallPlayer* Offender);

	/** Position the taker behind the spot, the goalkeeper on their line, and everyone else outside the area. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NewEraBall|Rules")
	void ResetPositionsForPenalty(ETeamSide TakingTeam, ANewEraBallPlayer* Taker);

	/** Resolve the current penalty kick. A converted penalty is worth PenaltyPoints. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void ResolvePenaltyKick(bool bScored);

	/** Credit possession to a team. Broadcasts OnPossessionChanged when it changes. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void SetPossession(ETeamSide Team);

	/** Called by the ball whenever it is touched. Handles restart activation and possession. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void NotifyBallTouched(ANewEraBallPlayer* Player, EBallTouchType TouchType, FVector TouchLocation);

	/** Called by the ball when it leaves the pitch. Awards a restart to the team that did not touch it last. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Rules")
	void NotifyBallOutOfPlay(FVector ExitLocation, ETeamSide LastTouchTeam);

	// ------------------------------------------------------------------
	// Geometry
	// ------------------------------------------------------------------

	/** +1 when the team attacks the +X goal, -1 when it attacks the -X goal. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	float GetAttackDirectionSign(ETeamSide Team) const;

	/** Unit vector pointing toward the goal the team is attacking. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	FVector GetAttackDirection(ETeamSide Team) const;

	/** True when the location is on the team's own side of the midfield line ("behind midfield"). */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	bool IsInOwnHalf(ETeamSide Team, FVector Location) const;

	/** True when the location is inside the goal area in front of the goal that GoalOwner defends. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	bool IsInsideGoalArea(ETeamSide GoalOwner, FVector Location) const;

	/** True when the location is inside the team's own goal area. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	bool IsInsideOwnGoalArea(ETeamSide Team, FVector Location) const { return IsInsideGoalArea(Team, Location); }

	/** True when the location is within the pitch boundary (with the given tolerance). */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	bool IsInsidePitch(FVector Location, float Tolerance = 0.0f) const;

	/** Centre of the goal mouth the team defends. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	FVector GetGoalCenter(ETeamSide GoalOwner) const;

	/** Penalty spot in front of the goal that GoalOwner defends. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	FVector GetPenaltySpot(ETeamSide GoalOwner) const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	FVector GetCentreSpot() const { return PitchCenter; }

	/** Clamp a location into the pitch rectangle. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Pitch")
	FVector ClampToPitch(FVector Location, float Inset = 0.0f) const;

	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	static ETeamSide GetOpposingTeam(ETeamSide Team) { return UNewEraBallTypeLibrary::GetOpposingTeam(Team); }

	// ------------------------------------------------------------------
	// Shootout (MLS 1996-1999 style 1v1)
	// ------------------------------------------------------------------

	/** Begin the shootout. Called automatically by EndRegulation when points are level. */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Shootout")
	void StartShootout();

	/**
	 * Begin the next attempt: one outfield player starts on the centre spot with the ball and runs at
	 * the opposing goalkeeper. Everyone else is moved off the pitch line by ResetPositionsForShootout.
	 */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Shootout")
	void BeginShootoutAttempt();

	/** Resolve the live attempt. Called by NotifyBallEnteredGoal, the timeout, or Blueprint (save / miss). */
	UFUNCTION(BlueprintCallable, Category = "NewEraBall|Shootout")
	void ResolveShootoutAttempt(bool bScored);

	/** True when both teams have taken equal attempts and one has scored more. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Shootout")
	bool IsShootoutDecided() const;

	/** Position the attacker on the centre spot, the goalkeeper on their line and everyone else clear. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NewEraBall|Shootout")
	void ResetPositionsForShootout(ANewEraBallPlayer* Attacker, ANewEraBallPlayer* Goalkeeper);

	/** Attacker start location for a shootout attempt by the given team. Midfield by rule. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Shootout")
	FVector GetShootoutStartLocation(ETeamSide AttackingTeam) const;

protected:
	/** Change phase and broadcast. */
	void SetMatchPhase(EMatchPhase NewPhase);

	/** Spawn one player for a team into a formation slot. */
	ANewEraBallPlayer* SpawnPlayer(ETeamSide Team, int32 SlotIndex, bool bGoalkeeper);

	/** Pick the class to spawn for a slot, honouring team overrides then DefaultPlayerClass. */
	TSubclassOf<ANewEraBallPlayer> ResolvePlayerClass(ETeamSide Team, bool bGoalkeeper) const;

	/** Reset every registered player's touch counter. */
	void ResetAllTouchCounters();

	/** Place the ball at a location, stop it, and hold it dead. */
	void PlaceBall(const FVector& Location);

	/** Release the ball into live play. */
	void ReleaseBall();

	/** Timer callbacks. */
	void OnPostGoalDelayElapsed();
	void OnHalfTimeElapsed();
	void OnShootoutAttemptTimeout();
	void OnShootoutInterAttemptDelayElapsed();

	/** Team whose turn it is in the shootout, based on round and attempts taken. */
	ETeamSide GetNextShootoutTeam() const;

	/** Next outfield player in rotation for a team's shootout attempt. */
	ANewEraBallPlayer* GetNextShootoutAttacker(ETeamSide Team);

	/** Players spawned by this GameMode, tracked so DespawnTeams leaves level-placed actors alone. */
	UPROPERTY()
	TArray<TObjectPtr<ANewEraBallPlayer>> SpawnedPlayers;

	/** Team that kicked off the current half. */
	ETeamSide HalfKickoffTeam = ETeamSide::None;

	/** Kickoff team pending after a goal stoppage. */
	ETeamSide PendingKickoffTeam = ETeamSide::None;

	/** Attempts taken by each team in the current shootout round. */
	int32 HomeShootoutAttempts = 0;
	int32 AwayShootoutAttempts = 0;

	/** Rotation cursors for shootout attackers. */
	int32 HomeShootoutAttackerCursor = 0;
	int32 AwayShootoutAttackerCursor = 0;

	/** Set while a shootout attempt is live and undecided. */
	bool bShootoutAttemptLive = false;

	FTimerHandle PostGoalTimerHandle;
	FTimerHandle HalfTimeTimerHandle;
	FTimerHandle ShootoutAttemptTimerHandle;
	FTimerHandle ShootoutInterAttemptTimerHandle;
};
