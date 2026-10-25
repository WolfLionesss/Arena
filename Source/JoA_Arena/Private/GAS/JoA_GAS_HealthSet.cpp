// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_HealthSet.h"

// --- GAS ---
#include "GameplayEffectExtension.h"

// --- Net ---
#include "Net/UnrealNetwork.h"

namespace
{
	// Full health before the figure writes its own value.
	constexpr float DefaultMaxHealth = 10.f;
}

UJoA_GAS_HealthSet::UJoA_GAS_HealthSet()
	: Damage(0.f)
	, Healing(0.f)
	, Health(DefaultMaxHealth)
	, MaxHealth(DefaultMaxHealth)
{
}

void UJoA_GAS_HealthSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_HealthSet,
		Health,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UJoA_GAS_HealthSet,
		MaxHealth,
		Params);
}

void UJoA_GAS_HealthSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float DamageDone = GetDamage();
		SetDamage(0.f);
		if (DamageDone > 0.f)
		{
			SetHealth(FMath::Clamp(
				GetHealth() - DamageDone,
				MinResource,
				GetMaxHealth()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float HealingDone = GetHealing();
		SetHealing(0.f);
		if (HealingDone > 0.f)
		{
			SetHealth(FMath::Clamp(
				GetHealth() + HealingDone,
				MinResource,
				GetMaxHealth()));
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute()
		|| Data.EvaluatedData.Attribute
			== GetMaxHealthAttribute())
	{
		// A lower maximum also pulls Health down.
		SetHealth(FMath::Clamp(
			GetHealth(),
			MinResource,
			GetMaxHealth()));
	}
}

void UJoA_GAS_HealthSet::ClampAttribute(
	const FGameplayAttribute& Attribute,
	float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(
			NewValue,
			MinResource,
			GetMaxHealth());
		return;
	}

	if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetDamageAttribute()
		|| Attribute == GetHealingAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

bool UJoA_GAS_HealthSet::IsReplicatedAttribute(
	const FGameplayAttribute& Attribute) const
{
	return Attribute == GetHealthAttribute()
		|| Attribute == GetMaxHealthAttribute();
}

void UJoA_GAS_HealthSet::OnRep_Health(
	const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_HealthSet,
		Health,
		OldHealth);
}

void UJoA_GAS_HealthSet::OnRep_MaxHealth(
	const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		UJoA_GAS_HealthSet,
		MaxHealth,
		OldMaxHealth);
}
