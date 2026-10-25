// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "GameplayEffectExecutionCalculation.h"

#include "JoA_GAS_VitalExecution.generated.h"

/**
 * Writes Damage, Healing, spends, and gains from
 * SetByCaller magnitudes. The attribute sets commit
 * those meta values onto the real stats.
 */
UCLASS()
class JOA_ARENA_API UJoA_GAS_VitalExecution
	: public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UJoA_GAS_VitalExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters&
			ExecutionParams,
		FGameplayEffectCustomExecutionOutput&
			OutExecutionOutput
	) const override;
};
