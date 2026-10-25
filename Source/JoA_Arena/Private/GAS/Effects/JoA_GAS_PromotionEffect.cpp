// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Effects/JoA_GAS_PromotionEffect.h"

// --- GAS ---
#include "GAS/Calculations/JoA_GAS_PromotionExecution.h"

UJoA_GAS_PromotionEffect::UJoA_GAS_PromotionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass =
		UJoA_GAS_PromotionExecution::StaticClass();
	Executions.Add(Execution);
}
