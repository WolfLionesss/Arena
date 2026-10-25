// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GameplayEffectExecutionCalculation.h"

#include "JoA_GAS_PromotionExecution.generated.h"

/**
 * Instant promotion write. Raises MaxHealth and
 * the used resource cap, then doubles Attack,
 * Defense, and Heal. Current Health and the
 * resource fill on the figure after this runs,
 * so Health is not clamped to the old maximum.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_PromotionExecution
	: public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UJoA_GAS_PromotionExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters&
			ExecutionParams,
		FGameplayEffectCustomExecutionOutput&
			OutExecutionOutput
	) const override;
};
