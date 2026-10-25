// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/Calculations/JoA_GAS_PromotionExecution.h"

// --- GAME ---
#include "Player/Units/JoA_UnitTypes.h"

// --- GAS ---
#include "AbilitySystemComponent.h"
#include "GAS/JoA_GAS_CombatSet.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

namespace
{
	float ReadPromotedAttribute(
		const UAbilitySystemComponent* ASC,
		const FGameplayAttribute& Attribute)
	{
		return ASC
			? ASC->GetNumericAttribute(Attribute)
			: 0.f;
	}

	void AddPromotionAdditive(
		FGameplayEffectCustomExecutionOutput& Output,
		const FGameplayAttribute& Attribute,
		float Magnitude)
	{
		if (Magnitude == 0.f)
		{
			return;
		}

		Output.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				Attribute,
				EGameplayModOp::Additive,
				Magnitude));
	}

	void AddPromotionOverride(
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

UJoA_GAS_PromotionExecution::UJoA_GAS_PromotionExecution()
{
}

void UJoA_GAS_PromotionExecution::
	Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters&
			ExecutionParams,
		FGameplayEffectCustomExecutionOutput&
			OutExecutionOutput) const
{
	const UAbilitySystemComponent* TargetASC =
		ExecutionParams.GetTargetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	const float Bonus = PromotionVitalBonus;
	AddPromotionAdditive(
		OutExecutionOutput,
		UJoA_GAS_HealthSet::GetMaxHealthAttribute(),
		Bonus);

	const float MaxStamina = ReadPromotedAttribute(
		TargetASC,
		UJoA_GAS_StaminaSet::GetMaxStaminaAttribute());
	const float MaxMana = ReadPromotedAttribute(
		TargetASC,
		UJoA_GAS_ManaSet::GetMaxManaAttribute());
	if (MaxStamina > 0.f)
	{
		AddPromotionAdditive(
			OutExecutionOutput,
			UJoA_GAS_StaminaSet::GetMaxStaminaAttribute(),
			Bonus);
	}
	else if (MaxMana > 0.f)
	{
		AddPromotionAdditive(
			OutExecutionOutput,
			UJoA_GAS_ManaSet::GetMaxManaAttribute(),
			Bonus);
	}

	const float Mul = PromotionCombatMultiplier;
	AddPromotionOverride(
		OutExecutionOutput,
		UJoA_GAS_CombatSet::GetAttackAttribute(),
		ReadPromotedAttribute(
			TargetASC,
			UJoA_GAS_CombatSet::GetAttackAttribute())
			* Mul);
	AddPromotionOverride(
		OutExecutionOutput,
		UJoA_GAS_CombatSet::GetDefenseAttribute(),
		ReadPromotedAttribute(
			TargetASC,
			UJoA_GAS_CombatSet::GetDefenseAttribute())
			* Mul);
	AddPromotionOverride(
		OutExecutionOutput,
		UJoA_GAS_CombatSet::GetHealAttribute(),
		ReadPromotedAttribute(
			TargetASC,
			UJoA_GAS_CombatSet::GetHealAttribute())
			* Mul);
}
