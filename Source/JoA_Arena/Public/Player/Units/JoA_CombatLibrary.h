// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- Engine ---
#include "Kismet/BlueprintFunctionLibrary.h"

// --- GAME ---
#include "Player/Units/JoA_UnitTypes.h"

#include "JoA_CombatLibrary.generated.h"

/**
 * Server result of one figure attacking another.
 * Clients observe the attribute changes, not this roll.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FAttackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bInRange = false;

	// 0 to 100.
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float HitChance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float HealthDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float AttackerStaminaCost = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float DefenderStaminaCost = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 PreviousComboAttacks = 0;
};

/**
 * Server result of one Medic heal.
 * Mana is spent only when the heal is applied.
 */
USTRUCT(BlueprintType)
struct JOA_ARENA_API FHealResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bInRange = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bInsufficientMana = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float HealthRestored = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float ManaSpent = 0.f;
};

/**
 * Reach, hit chance, and stamina helpers.
 */
UCLASS()
class JOA_ARENA_API UJoA_CombatLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/* Hit chance after PreviousComboAttacks on this
	 * figure.
	 * Attack > Defense: ((Attack - Defense) / Attack)
	 * times 100, plus 25 per earlier attempt.
	 * 2 vs 1 is 50%. 2 vs 0 is 100%.
	 * Attack == Defense: 0, then +25 per attempt.
	 * Attack < Defense: 25, then +25. The fourth
	 * attempt is 100%. Clamped to 0-100. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static float CalculateHitChance(
		float Attack,
		float Defense,
		int32 PreviousComboAttacks);

	/* Stamina spent by the two figures in the attack.
	 * Costs apply to the attempt, before the hit roll. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static void GetStaminaCosts(
		float Attack,
		float Defense,
		float& OutAttackerCost,
		float& OutDefenderCost);

	/* Squares this figure can target, not its own.
	 * IncludeHeightOne: height 1 is included and ends
	 * the line. Height 2 is excluded and ends it.
	 * On field 3 with height 1 on field 2, field 2 is
	 * reachable and field 1 is not.
	 * Archer: a figure on height 1 or 2 is a
	 * target. Height 1 also allows the square
	 * immediately behind it. Height 2 blocks
	 * everything behind it. */
	/* The board is the one grid cached on the GameMode. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Combat",
		meta = (WorldContext = "WorldContext"))
	static TArray<FIntPoint> CollectCellsInFront(
		const UObject* WorldContext,
		FIntPoint Origin,
		EFacing Facing,
		int32 MaxSteps,
		EReachPolicy ReachPolicy);

	/* True when CollectCellsInFront would list Cell.
	 * The walk stops on that square. */
	static bool IsCellInFront(
		const UObject* WorldContext,
		FIntPoint Origin,
		FIntPoint Cell,
		EFacing Facing,
		int32 MaxSteps,
		EReachPolicy ReachPolicy);

	/* First obstacle on the facing line within MaxSteps.
	 * HeavyKnight lowers that cell by 1. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Combat",
		meta = (WorldContext = "WorldContext"))
	static bool FindFirstObstacleInFront(
		const UObject* WorldContext,
		FIntPoint Origin,
		EFacing Facing,
		int32 MaxSteps,
		FIntPoint& OutCell);
};
