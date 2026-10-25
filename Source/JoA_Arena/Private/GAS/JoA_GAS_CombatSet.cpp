// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_CombatSet.h"

// --- GAS ---
#include "GameplayEffectExtension.h"

// --- Net ---
#include "Net/UnrealNetwork.h"

UJoA_GAS_CombatSet::UJoA_GAS_CombatSet()
	: Attack(0.f)
	, Defense(0.f)
	, Heal(0.f)
{
}

void UJoA_GAS_CombatSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_CombatSet,
		Attack,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_CombatSet,
		Defense,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_CombatSet,
		Heal,
		Params);
}

void UJoA_GAS_CombatSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetAttackAttribute())
	{
		SetAttack(FMath::Max(GetAttack(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute
		== GetDefenseAttribute())
	{
		SetDefense(FMath::Max(GetDefense(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealAttribute())
	{
		SetHeal(FMath::Max(GetHeal(), 0.f));
	}
}

void UJoA_GAS_CombatSet::ClampAttribute(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	if (Attribute == GetAttackAttribute()
		|| Attribute == GetDefenseAttribute()
		|| Attribute == GetHealAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool UJoA_GAS_CombatSet::IsReplicatedAttribute(
	const FGameplayAttribute& Attribute) const
{
	return Attribute == GetAttackAttribute()
		|| Attribute == GetDefenseAttribute()
		|| Attribute == GetHealAttribute();
}

void UJoA_GAS_CombatSet::OnRep_Attack(
	const FGameplayAttributeData& OldAttack)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_CombatSet,
		Attack,
		OldAttack);
}

void UJoA_GAS_CombatSet::OnRep_Defense(
	const FGameplayAttributeData& OldDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_CombatSet,
		Defense,
		OldDefense);
}

void UJoA_GAS_CombatSet::OnRep_Heal(
	const FGameplayAttributeData& OldHeal)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_CombatSet,
		Heal,
		OldHeal);
}
