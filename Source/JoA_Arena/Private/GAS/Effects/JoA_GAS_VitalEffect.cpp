// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Effects/JoA_GAS_VitalEffect.h"

// --- GAS ---
#include "GAS/Calculations/JoA_GAS_VitalExecution.h"

UJoA_GAS_VitalEffect::UJoA_GAS_VitalEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass =
		UJoA_GAS_VitalExecution::StaticClass();
	Executions.Add(Execution);
}
