// NewEraBall - Shared gameplay types
// Enums, pitch geometry and the FTeamConfig structure used across all NewEraBall systems.
// One Unreal unit = one centimetre everywhere in this project.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NewEraBallTypes.generated.h"

class ANewEraBallPlayer;

/** Which side of the match a player, goal or event belongs to. */
UENUM(BlueprintType)
enum class ETeamSide : uint8
{
	None	UMETA(DisplayName = "None"),
	Home	UMETA(DisplayName = "Home"),
	Away	UMETA(DisplayName = "Away")
};

/**
 * Supported match formats. The numeric value of each entry is the number of players per team
 * (including the goalkeeper), so the player count scales directly from the selected format.
 */
UENUM(BlueprintType)
enum class ETeamSizeFormat : uint8
{
	None		= 0 UMETA(DisplayName = "None"),
	FiveVFive	= 5 UMETA(DisplayName = "5v5"),
	SixVSix		= 6 UMETA(DisplayName = "6v6"),
	SevenVSeven	= 7 UMETA(DisplayName = "7v7"),
	EightVEight	= 8 UMETA(DisplayName = "8v8"),
	NineVNine	= 9 UMETA(DisplayName = "9v9")
};

/** High level state of the match, driven by ANewEraBallGameMode. */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	/** Teams configured but the match has not started. Team size may still be changed. */
	PreMatch		UMETA(DisplayName = "Pre-Match"),
	/** Ball is dead and waiting for the restart team to play it (kickoff, turnover restart). */
	Restart			UMETA(DisplayName = "Restart"),
	/** Ball is live. */
	InPlay			UMETA(DisplayName = "In Play"),
	/** Short stoppage after a goal before the kickoff. */
	GoalStoppage	UMETA(DisplayName = "Goal Stoppage"),
	/** A penalty kick is being taken. */
	PenaltyKick		UMETA(DisplayName = "Penalty Kick"),
	/** Break between halves. */
	HalfTime		UMETA(DisplayName = "Half Time"),
	/** MLS 1996-1999 style 1v1 shootout to resolve a tied match. */
	Shootout		UMETA(DisplayName = "Shootout"),
	/** Match over. */
	Finished		UMETA(DisplayName = "Finished")
};

/** The kind of score being awarded. Point values live in the GameMode and are configurable. */
UENUM(BlueprintType)
enum class EScoreType : uint8
{
	/** Goal scored from inside the goal area. Default: 2 points. */
	StandardGoal	UMETA(DisplayName = "Standard Goal (inside goal area)"),
	/** Goal scored from outside the goal area. Default: 3 points. */
	LongRangeGoal	UMETA(DisplayName = "Long Range Goal (outside goal area)"),
	/** Converted penalty kick. Default: 1 point. */
	Penalty			UMETA(DisplayName = "Penalty")
};

/** Outcome of registering a touch against the 2-touch rule. */
UENUM(BlueprintType)
enum class ETouchViolationResult : uint8
{
	/** Touch was legal. */
	None		UMETA(DisplayName = "None"),
	/** Too many touches in open play behind the midfield line: automatic turnover. */
	Turnover	UMETA(DisplayName = "Turnover"),
	/** Too many touches inside the player's own goal area: penalty to the opposing team. */
	Penalty		UMETA(DisplayName = "Penalty")
};

/** Why possession changed hands. */
UENUM(BlueprintType)
enum class ETurnoverReason : uint8
{
	TouchViolation	UMETA(DisplayName = "Touch Violation"),
	OutOfPlay		UMETA(DisplayName = "Out Of Play"),
	Interception	UMETA(DisplayName = "Interception"),
	PenaltyMissed	UMETA(DisplayName = "Penalty Missed"),
	Manual			UMETA(DisplayName = "Manual")
};

/** Dribble moves available to every player. Purely input driven, no randomness. */
UENUM(BlueprintType)
enum class EDribbleMove : uint8
{
	None			UMETA(DisplayName = "None"),
	/** Sharp change of direction with the ball knocked to the new heading. */
	DirectionCut	UMETA(DisplayName = "Direction Cut"),
	/** Sudden acceleration or deceleration with the ball pushed further or pulled closer. */
	SpeedChange		UMETA(DisplayName = "Speed Change"),
	/** Fake touch: the body sells a direction, the ball stays put. */
	Feint			UMETA(DisplayName = "Feint"),
	/** Upper body fake without a touch, followed by a lateral shift. */
	BodyFake		UMETA(DisplayName = "Body Fake")
};

/** What kind of ball contact is being reported. Used by touch counting and for animation hooks. */
UENUM(BlueprintType)
enum class EBallTouchType : uint8
{
	/** Physical contact detected by the ball (deflection, block, uncontrolled bounce). */
	Contact			UMETA(DisplayName = "Contact"),
	/** The player brought the ball under close control. */
	ControlGained	UMETA(DisplayName = "Control Gained"),
	/** A touch taken while keeping the ball under close control. */
	Dribble			UMETA(DisplayName = "Dribble"),
	/** A touch produced by a dribble move. */
	DribbleMove		UMETA(DisplayName = "Dribble Move"),
	Pass			UMETA(DisplayName = "Pass"),
	Shot			UMETA(DisplayName = "Shot")
};

/**
 * Pitch geometry in centimetres. The pitch is centred on the GameMode's PitchCenter with its
 * length along the world X axis and its width along the world Y axis. The midfield line is X = 0
 * in pitch space. Goals sit on the +X and -X ends.
 */
USTRUCT(BlueprintType)
struct NEWERABALL_API FPitchDimensions
{
	GENERATED_BODY()

	/** Goal line to goal line. 59 metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "1000", Units = "cm"))
	float Length = 5900.0f;

	/** Touchline to touchline. 41 metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "1000", Units = "cm"))
	float Width = 4100.0f;

	/** How far the goal area extends from the goal line into the pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "100", Units = "cm"))
	float GoalAreaDepth = 1100.0f;

	/** Full width of the goal area, centred on the goal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "100", Units = "cm"))
	float GoalAreaWidth = 2400.0f;

	/** Distance from the goal line to the penalty spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "100", Units = "cm"))
	float PenaltySpotDistance = 900.0f;

	/** Inside width of the goal mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "100", Units = "cm"))
	float GoalWidth = 500.0f;

	/** Inside height of the goal mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "100", Units = "cm"))
	float GoalHeight = 200.0f;

	float GetHalfLength() const { return Length * 0.5f; }
	float GetHalfWidth() const { return Width * 0.5f; }
};

/** Identity and spawn classes for one of the two teams. */
USTRUCT(BlueprintType)
struct NEWERABALL_API FTeamIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FLinearColor PrimaryColor = FLinearColor::White;

	/** Class spawned for outfield players of this team. Falls back to the GameMode default when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	TSubclassOf<ANewEraBallPlayer> OutfieldPlayerClass;

	/** Class spawned for this team's goalkeeper. Falls back to OutfieldPlayerClass when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	TSubclassOf<ANewEraBallPlayer> GoalkeeperClass;
};

/**
 * Match-wide team configuration. Selected before the match starts. The team size format drives
 * how many players each side spawns; nothing about team size is hardcoded elsewhere.
 */
USTRUCT(BlueprintType)
struct NEWERABALL_API FTeamConfig
{
	GENERATED_BODY()

	/** Players per team including the goalkeeper. 7v7 by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Config")
	ETeamSizeFormat Format = ETeamSizeFormat::SevenVSeven;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Config")
	FTeamIdentity Home;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Config")
	FTeamIdentity Away;

	/** Total players on one team, goalkeeper included. */
	int32 GetPlayersPerTeam() const { return PlayersForFormat(Format); }

	/** Outfield players on one team (everyone except the goalkeeper). */
	int32 GetOutfieldPlayersPerTeam() const { return FMath::Max(0, GetPlayersPerTeam() - 1); }

	/** Players on the pitch across both teams. */
	int32 GetTotalPlayers() const { return GetPlayersPerTeam() * 2; }

	const FTeamIdentity& GetIdentity(ETeamSide Side) const { return Side == ETeamSide::Away ? Away : Home; }

	static int32 PlayersForFormat(ETeamSizeFormat InFormat) { return static_cast<int32>(InFormat); }

	static bool IsSupportedFormat(ETeamSizeFormat InFormat)
	{
		const int32 Count = PlayersForFormat(InFormat);
		return Count >= PlayersForFormat(ETeamSizeFormat::FiveVFive) && Count <= PlayersForFormat(ETeamSizeFormat::NineVNine);
	}
};

/** Blueprint helpers for the shared types (struct member functions are not visible to Blueprints). */
UCLASS()
class NEWERABALL_API UNewEraBallTypeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Players per team, goalkeeper included, for the given format. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team Config")
	static int32 GetPlayersPerTeamForFormat(ETeamSizeFormat Format) { return FTeamConfig::PlayersForFormat(Format); }

	/** Players per team, goalkeeper included, for the given config. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team Config")
	static int32 GetPlayersPerTeam(const FTeamConfig& Config) { return Config.GetPlayersPerTeam(); }

	/** Outfield players per team for the given config. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team Config")
	static int32 GetOutfieldPlayersPerTeam(const FTeamConfig& Config) { return Config.GetOutfieldPlayersPerTeam(); }

	/** Total number of players on the pitch for the given config. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team Config")
	static int32 GetTotalPlayers(const FTeamConfig& Config) { return Config.GetTotalPlayers(); }

	/** Identity block for one side of the given config. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Team Config")
	static FTeamIdentity GetTeamIdentity(const FTeamConfig& Config, ETeamSide Side) { return Config.GetIdentity(Side); }

	/** Home becomes Away and Away becomes Home. None stays None. */
	UFUNCTION(BlueprintPure, Category = "NewEraBall|Teams")
	static ETeamSide GetOpposingTeam(ETeamSide Side)
	{
		switch (Side)
		{
		case ETeamSide::Home: return ETeamSide::Away;
		case ETeamSide::Away: return ETeamSide::Home;
		default: return ETeamSide::None;
		}
	}
};
