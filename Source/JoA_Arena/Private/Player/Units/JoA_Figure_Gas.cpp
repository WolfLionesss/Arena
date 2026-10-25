// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Units/JoA_Figure.h"

// --- GAS ---
#include "GAS/Abilities/JoA_GAS_BuildObstacleAbility.h"
#include "GAS/Abilities/JoA_GAS_FallAbility.h"
#include "GAS/Abilities/JoA_GAS_FlagAbility.h"
#include "GAS/Abilities/JoA_GAS_JumpAbility.h"
#include "GAS/Abilities/JoA_GAS_MoveAbility.h"
#include "GAS/Abilities/JoA_GAS_RestAbility.h"
#include "GAS/Abilities/JoA_GAS_SkipTurnAbility.h"
#include "GAS/Abilities/JoA_GAS_TurnAbility.h"
#include "GAS/Effects/JoA_GAS_VitalEffect.h"
#include "GAS/JoA_GAS_AbilitySystemComponent.h"
#include "GAS/JoA_GAS_CombatSet.h"
#include "GAS/JoA_GAS_GameplayTags.h"
#include "GAS/JoA_GAS_HealthSet.h"
#include "GAS/JoA_GAS_InitializationData.h"
#include "GAS/JoA_GAS_ManaSet.h"
#include "GAS/JoA_GAS_StaminaSet.h"

namespace
{
	void AddBaseValue(
		FJoA_GAS_InitializationData& Data,
		const FGameplayAttribute& Attribute,
		float Value)
	{
		FJoA_GAS_AttributeBaseValue BaseValue;
		BaseValue.Attribute = Attribute;
		BaseValue.Value = Value;
		Data.AttributeBaseValues.Add(BaseValue);
	}
}

static FJoA_GAS_InitializationData
	BuildFigureInitializationData(
		const AJoA_Figure& Figure);

void AJoA_Figure::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	/* Death is decided on the server. The client reads
	 * bIsDead. Bound once, before the first base value
	 * is written. */
	if (HasAuthority() && !HealthChangedHandle.IsValid())
	{
		HealthChangedHandle = AbilitySystemComponent->
			GetGameplayAttributeValueChangeDelegate(
				UJoA_GAS_HealthSet::GetHealthAttribute())
			.AddUObject(this, &AJoA_Figure::OnHealthChanged);
	}

	const FJoA_GAS_InitializationData Data =
		BuildFigureInitializationData(*this);
	AbilitySystemComponent->InitializeAbilitySystemData(
		Data,
		this,
		this);
}

static FJoA_GAS_InitializationData
	BuildFigureInitializationData(
		const AJoA_Figure& Figure)
{
	FJoA_GAS_InitializationData Data;
	Data.AttributeSets.Add(
		UJoA_GAS_HealthSet::StaticClass());
	Data.AttributeSets.Add(
		UJoA_GAS_StaminaSet::StaticClass());
	Data.AttributeSets.Add(
		UJoA_GAS_ManaSet::StaticClass());
	Data.AttributeSets.Add(
		UJoA_GAS_CombatSet::StaticClass());

	const FUnitStats Defaults =
		FUnitStats::GetDefaults(Figure.UnitType);

	/* The maximum goes first. The current value is
	 * clamped to it when it is written. */
	AddBaseValue(
		Data,
		UJoA_GAS_HealthSet::GetMaxHealthAttribute(),
		Defaults.MaxHealth);
	AddBaseValue(
		Data,
		UJoA_GAS_StaminaSet::GetMaxStaminaAttribute(),
		Defaults.MaxStamina);
	AddBaseValue(
		Data,
		UJoA_GAS_ManaSet::GetMaxManaAttribute(),
		Defaults.MaxMana);
	AddBaseValue(
		Data,
		UJoA_GAS_HealthSet::GetHealthAttribute(),
		Defaults.Health);
	AddBaseValue(
		Data,
		UJoA_GAS_StaminaSet::GetStaminaAttribute(),
		Defaults.Stamina);
	AddBaseValue(
		Data,
		UJoA_GAS_ManaSet::GetManaAttribute(),
		Defaults.Mana);
	AddBaseValue(
		Data,
		UJoA_GAS_CombatSet::GetAttackAttribute(),
		Defaults.Attack);
	AddBaseValue(
		Data,
		UJoA_GAS_CombatSet::GetDefenseAttribute(),
		Defaults.Defense);
	AddBaseValue(
		Data,
		UJoA_GAS_CombatSet::GetHealAttribute(),
		Defaults.Heal);

	/* One class serves all four steps and both turns.
	 * The InputID of the spec holds the direction. */
	for (int32 Step = 0; Step < 4; ++Step)
	{
		FAbilityGrant Grant;
		Grant.Ability = UJoA_GAS_MoveAbility::StaticClass();
		Grant.InputID = Step;
		Data.AbilityGrants.Add(Grant);
	}

	for (int32 Turn = 0; Turn < 2; ++Turn)
	{
		FAbilityGrant Grant;
		Grant.Ability = UJoA_GAS_TurnAbility::StaticClass();
		Grant.InputID = Turn;
		Data.AbilityGrants.Add(Grant);
	}

	FAbilityGrant SkipGrant;
	SkipGrant.Ability =
		UJoA_GAS_SkipTurnAbility::StaticClass();
	SkipGrant.InputID = 0;
	Data.AbilityGrants.Add(SkipGrant);

	FAbilityGrant RestGrant;
	RestGrant.Ability =
		UJoA_GAS_RestAbility::StaticClass();
	RestGrant.InputID = 0;
	Data.AbilityGrants.Add(RestGrant);

	for (int32 Command = 0; Command < 2; ++Command)
	{
		FAbilityGrant FlagGrant;
		FlagGrant.Ability =
			UJoA_GAS_FlagAbility::StaticClass();
		FlagGrant.InputID = Command;
		Data.AbilityGrants.Add(FlagGrant);
	}

	if (Defaults.bCanBuildObstacles)
	{
		FAbilityGrant BuildGrant;
		BuildGrant.Ability =
			UJoA_GAS_BuildObstacleAbility::StaticClass();
		BuildGrant.InputID = 0;
		Data.AbilityGrants.Add(BuildGrant);
	}

	if (Defaults.bCanJump)
	{
		FAbilityGrant JumpGrant;
		JumpGrant.Ability =
			UJoA_GAS_JumpAbility::StaticClass();
		JumpGrant.InputID = 0;
		Data.AbilityGrants.Add(JumpGrant);

		FAbilityGrant FallGrant;
		FallGrant.Ability =
			UJoA_GAS_FallAbility::StaticClass();
		FallGrant.InputID = 0;
		Data.AbilityGrants.Add(FallGrant);
	}
	return Data;
}

UJoA_GAS_HealthSet* AJoA_Figure::GetHealthSet() const
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	return const_cast<UJoA_GAS_HealthSet*>(
		AbilitySystemComponent->GetSet<UJoA_GAS_HealthSet>());
}

UJoA_GAS_StaminaSet* AJoA_Figure::GetStaminaSet() const
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	return const_cast<UJoA_GAS_StaminaSet*>(
		AbilitySystemComponent->
			GetSet<UJoA_GAS_StaminaSet>());
}

UJoA_GAS_ManaSet* AJoA_Figure::GetManaSet() const
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	return const_cast<UJoA_GAS_ManaSet*>(
		AbilitySystemComponent->GetSet<UJoA_GAS_ManaSet>());
}

UJoA_GAS_CombatSet* AJoA_Figure::GetCombatSet() const
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	return const_cast<UJoA_GAS_CombatSet*>(
		AbilitySystemComponent->GetSet<UJoA_GAS_CombatSet>());
}

void AJoA_Figure::ApplyVitalChange(
	float Damage,
	float Healing,
	float StaminaSpend,
	float ManaSpend,
	float StaminaGain,
	float ManaGain)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	if (StaminaSpend > 0.f)
	{
		StaminaSpend += GetHeldFlagSpendPenalty();
	}

	if (Damage <= 0.f
		&& Healing <= 0.f
		&& StaminaSpend <= 0.f
		&& ManaSpend <= 0.f
		&& StaminaGain <= 0.f
		&& ManaGain <= 0.f)
	{
		return;
	}

	FGameplayEffectContextHandle Context =
		AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec =
		AbilitySystemComponent->MakeOutgoingSpec(
			UJoA_GAS_VitalEffect::StaticClass(),
			1.f,
			Context);
	if (!Spec.IsValid() || !Spec.Data.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_Damage,
		Damage);
	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_Healing,
		Healing);
	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_StaminaSpend,
		StaminaSpend);
	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_ManaSpend,
		ManaSpend);
	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_StaminaGain,
		StaminaGain);
	Spec.Data->SetSetByCallerMagnitude(
		TAG_JoA_GAS_Data_ManaGain,
		ManaGain);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*Spec.Data.Get());
}
