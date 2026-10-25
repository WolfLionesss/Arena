// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_StaminaSet.h"

// --- GAS ---
#include "GameplayEffectExtension.h"

// --- Net ---
#include "Net/UnrealNetwork.h"

namespace
{
	// Full stamina before the figure writes its own value.
	constexpr float DefaultMaxStamina = 10.f;
}

UJoA_GAS_StaminaSet::UJoA_GAS_StaminaSet()
	: StaminaSpend(0.f)
	, StaminaGain(0.f)
	, Stamina(DefaultMaxStamina)
	, MaxStamina(DefaultMaxStamina)
{
}

void UJoA_GAS_StaminaSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_StaminaSet,
		Stamina,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_StaminaSet,
		MaxStamina,
		Params);
}

void UJoA_GAS_StaminaSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute
		== GetStaminaSpendAttribute())
	{
		const float Spend = GetStaminaSpend();
		SetStaminaSpend(0.f);
		if (Spend > 0.f)
		{
			SetStamina(FMath::Clamp(
				GetStamina() - Spend,
				MinResource,
				GetMaxStamina()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute
		== GetStaminaGainAttribute())
	{
		const float Gain = GetStaminaGain();
		SetStaminaGain(0.f);
		if (Gain > 0.f)
		{
			SetStamina(FMath::Clamp(
				GetStamina() + Gain,
				MinResource,
				GetMaxStamina()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetStaminaAttribute()
		|| Data.EvaluatedData.Attribute
			== GetMaxStaminaAttribute())
	{
		// A lower maximum also pulls Stamina down.
		SetStamina(FMath::Clamp(
			GetStamina(),
			MinResource,
			GetMaxStamina()));
	}
}

void UJoA_GAS_StaminaSet::ClampAttribute(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(
			NewValue,
			MinResource,
			GetMaxStamina());
		return;
	}

	if (Attribute == GetMaxStaminaAttribute()
		|| Attribute == GetStaminaSpendAttribute()
		|| Attribute == GetStaminaGainAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool UJoA_GAS_StaminaSet::IsReplicatedAttribute(
	const FGameplayAttribute& Attribute) const
{
	return Attribute == GetStaminaAttribute()
		|| Attribute == GetMaxStaminaAttribute();
}

void UJoA_GAS_StaminaSet::OnRep_Stamina(
	const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_StaminaSet,
		Stamina,
		OldStamina);
}

void UJoA_GAS_StaminaSet::OnRep_MaxStamina(
	const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_StaminaSet,
		MaxStamina,
		OldMaxStamina);
}
