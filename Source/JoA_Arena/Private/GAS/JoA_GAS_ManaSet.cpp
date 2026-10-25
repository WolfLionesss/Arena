// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_ManaSet.h"

// --- GAS ---
#include "GameplayEffectExtension.h"

// --- Net ---
#include "Net/UnrealNetwork.h"

UJoA_GAS_ManaSet::UJoA_GAS_ManaSet()
	: ManaSpend(0.f)
	, ManaGain(0.f)
	, Mana(0.f)
	, MaxMana(0.f)
{
}

void UJoA_GAS_ManaSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_ManaSet,
		Mana,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_ManaSet,
		MaxMana,
		Params);
}

void UJoA_GAS_ManaSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute
		== GetManaSpendAttribute())
	{
		const float Spend = GetManaSpend();
		SetManaSpend(0.f);
		if (Spend > 0.f)
		{
			SetMana(FMath::Clamp(
				GetMana() - Spend,
				MinResource,
				GetMaxMana()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute
		== GetManaGainAttribute())
	{
		const float Gain = GetManaGain();
		SetManaGain(0.f);
		if (Gain > 0.f)
		{
			SetMana(FMath::Clamp(
				GetMana() + Gain,
				MinResource,
				GetMaxMana()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetManaAttribute()
		|| Data.EvaluatedData.Attribute
			== GetMaxManaAttribute())
	{
		// A lower maximum also pulls Mana down.
		SetMana(FMath::Clamp(
			GetMana(),
			MinResource,
			GetMaxMana()));
	}
}

void UJoA_GAS_ManaSet::ClampAttribute(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(
			NewValue,
			MinResource,
			GetMaxMana());
		return;
	}

	if (Attribute == GetMaxManaAttribute()
		|| Attribute == GetManaSpendAttribute()
		|| Attribute == GetManaGainAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool UJoA_GAS_ManaSet::IsReplicatedAttribute(
	const FGameplayAttribute& Attribute) const
{
	return Attribute == GetManaAttribute()
		|| Attribute == GetMaxManaAttribute();
}

void UJoA_GAS_ManaSet::OnRep_Mana(
	const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_ManaSet,
		Mana,
		OldMana);
}

void UJoA_GAS_ManaSet::OnRep_MaxMana(
	const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_ManaSet,
		MaxMana,
		OldMaxMana);
}
