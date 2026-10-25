// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JoA_UnitTypes.generated.h"

/**
 * What the combat text over a figure says.
 */
UENUM()
enum class ECombatTextKind : uint8
{
	Miss,
	Hit,
	Heal
};

/**
 * One body clip. Names stay short. Asset files
 * keep the KayKit import names.
 */
UENUM()
enum class EFigureAnim : uint8
{
	Idle,
	Run,
	Attack,
	Heal,
	Shoot,
	Jump,
	Fall,
	Hit,
	Dodge,
	Death,
	Sit,
	Lie,
	Wave,
	Clap,
	Cheer,
	Cry,
	Interact,
	PickUp
};

/**
 * Server-locked result that waits for the clip.
 */
UENUM()
enum class EDeferredResolve : uint8
{
	None,
	Attack,
	Heal,
	Destroy,
	Build,
	Jump,
	Fall,
	Pickup,
	Drop
};

/**
 * Rest pose of a figure on the board edge.
 */
UENUM()
enum class EBenchPhase : uint8
{
	None,
	Wave,
	Idle,
	Sit,
	Lie
};

/**
 * One figure class. The server copies these numbers
 * into the figure ASC when the owner is assigned.
 */
UENUM(BlueprintType)
enum class EUnitType : uint8
{
	/* HeavyKnight: Attack 2, Defense 2, one square
	 * forward. Ability: a hit lowers an obstacle by 1. */
	HeavyKnight UMETA(DisplayName = "HeavyKnight"),

	/* LightInfantry: Attack 1, Defense 1, two squares
	 * forward. A height-1 obstacle square is a valid
	 * target. Ability: raise the one square in front
	 * by 1, up to height 2. */
	LightInfantry UMETA(DisplayName = "LightInfantry"),

	/* Archer: Attack 2, Defense 1, the whole file or
	 * rank ahead. Height 1 shortens the shot to the one
	 * clear field behind that obstacle. Height 2 blocks
	 * the shot. */
	Archer UMETA(DisplayName = "Archer"),

	/* Medic: Heal 2, Defense 0, Mana 10, heal range 2.
	 * Height-1 obstacle rules match LightInfantry. */
	Medic UMETA(DisplayName = "Medic")
};

/**
 * How a facing line treats obstacles.
 */
UENUM(BlueprintType)
enum class EReachPolicy : uint8
{
	/* HeavyKnight only. The range includes empty
	 * squares. The first obstacle, height 1 or 2,
	 * blocks the line and is not a target. */
	StopAtObstacle UMETA(DisplayName = "StopAtObstacle"),

	/* LightInfantry and Medic. Height 1 is included
	 * and is the final target. Height 2 is excluded
	 * and also ends the line. */
	IncludeHeightOne UMETA(DisplayName = "IncludeHeightOne"),

	/* Archer only. Height 1 and 2 are targets.
	 * Height 1 also allows the next square.
	 * Height 2 blocks everything behind it. */
	Archer UMETA(DisplayName = "Archer")
};

/**
 * Facing on the 8x8 board. Range is measured only
 * along this line, including the diagonals.
 */
UENUM(BlueprintType)
enum class EFacing : uint8
{
	PositiveX UMETA(DisplayName = "+X"),
	NegativeX UMETA(DisplayName = "-X"),
	PositiveY UMETA(DisplayName = "+Y"),
	NegativeY UMETA(DisplayName = "-Y"),
	PositiveXPositiveY UMETA(DisplayName = "+X+Y"),
	PositiveXNegativeY UMETA(DisplayName = "+X-Y"),
	NegativeXPositiveY UMETA(DisplayName = "-X+Y"),
	NegativeXNegativeY UMETA(DisplayName = "-X-Y")
};

/**
 * One step of a figure, relative to its Facing.
 * The value is stored as the InputID of the granted
 * move ability, so keep the order.
 */
UENUM(BlueprintType)
enum class EStepDirection : uint8
{
	Forward UMETA(DisplayName = "Forward"),
	Back UMETA(DisplayName = "Back"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

/**
 * Turn of 45 degrees on the 8-way Facing ring.
 * The value is stored as the InputID of the granted
 * turn ability, so keep the order.
 */
UENUM(BlueprintType)
enum class ETurnDirection : uint8
{
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

/* Reaching the enemy home file without a flag
 * grows the figure once. Combat doubles. Health
 * and the used resource rise by this bonus. */
inline constexpr float PromotionVitalBonus = 5.f;
inline constexpr float PromotionCombatMultiplier = 2.f;
inline constexpr float PromotionMeshMultiplier = 1.5f;

/**
 * Authored starting stats for a figure class.
 * Range is a placement rule, not a GAS attribute.
 * Archer range follows the board, not a fixed integer.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FUnitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	EUnitType UnitType = EUnitType::HeavyKnight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Health = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Stamina = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Mana = 0.f;

	// Full values. The UI shows the current over these.
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float MaxHealth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float MaxStamina = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float MaxMana = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Attack = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Defense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	float Heal = 0.f;

	/* Squares in front. Archer ignores this and uses
	 * EReachPolicy::Archer on the rest of the line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit",
		meta = (ClampMin = "0"))
	int32 Range = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	EReachPolicy ReachPolicy =
		EReachPolicy::StopAtObstacle;

	/* HeavyKnight only. A hit lowers the first obstacle
	 * on the facing line by 1, whatever Attack is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bCanDestroyObstacles = false;

	/* LightInfantry only. The square in front rises
	 * by one height, up to 2. A flag in hand blocks
	 * it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bCanBuildObstacles = false;

	/* Every class but HeavyKnight. Climb one
	 * height, or step down with Fall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bCanJump = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bCanAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bCanHeal = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Unit")
	bool bUsesStamina = true;

	static FUnitStats GetDefaults(EUnitType Type);
};
