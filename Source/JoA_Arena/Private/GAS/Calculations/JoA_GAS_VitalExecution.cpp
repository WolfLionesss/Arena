// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Calculations/JoA_GAS_VitalExecution.h"

// --- GAS ---
#include "GAS/JoA_GAS_GameplayTags.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

namespace
{
	float CallerMagnitude(
		const FGameplayEffectSpec& Spec,
		const FGameplayTag& DataTag)
	{
		return FMath::Max(
			0.f,
			Spec.GetSetByCallerMagnitude(
				DataTag,
				false,
				0.f));
	}

	void AddOverride(
		FGameplayEffectCustomExecutionOutput& Output,
		const FGameplayAttribute& Attribute,
		float Magnitude)
	{
		if (Magnitude <= 0.f)
		{
			return;
		}

		Output.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				Attribute,
				EGameplayModOp::Override,
				Magnitude));
	}
}

UJoA_GAS_VitalExecution::UJoA_GAS_VitalExecution()
{
}

void UJoA_GAS_VitalExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters&
		ExecutionParams,
	FGameplayEffectCustomExecutionOutput&
		OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec =
		ExecutionParams.GetOwningSpec();

	/* Hit chance and costs were already resolved.
	 * This only forwards those numbers onto metas. */
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_HealthSet::GetDamageAttribute(),
		CallerMagnitude(Spec, TAG_JoA_GAS_Data_Damage));
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_HealthSet::GetHealingAttribute(),
		CallerMagnitude(Spec, TAG_JoA_GAS_Data_Healing));
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_StaminaSet::GetStaminaSpendAttribute(),
		CallerMagnitude(
			Spec,
			TAG_JoA_GAS_Data_StaminaSpend));
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_ManaSet::GetManaSpendAttribute(),
		CallerMagnitude(Spec, TAG_JoA_GAS_Data_ManaSpend));
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_StaminaSet::GetStaminaGainAttribute(),
		CallerMagnitude(
			Spec,
			TAG_JoA_GAS_Data_StaminaGain));
	AddOverride(
		OutExecutionOutput,
		UJoA_GAS_ManaSet::GetManaGainAttribute(),
		CallerMagnitude(
			Spec,
			TAG_JoA_GAS_Data_ManaGain));
}
