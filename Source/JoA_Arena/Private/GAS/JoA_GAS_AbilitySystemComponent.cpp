// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS/JoA_GAS_AbilitySystemComponent.h"

// --- GAS ---
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

UJoA_GAS_AbilitySystemComponent::
	UJoA_GAS_AbilitySystemComponent()
{
	/* Mixed replicates full effect data to the owner
	 * and a minimal set to the other player. */
	ReplicationMode =
		EGameplayEffectReplicationMode::Mixed;
	SetIsReplicatedByDefault(true);
}

const UAttributeSet*
	UJoA_GAS_AbilitySystemComponent::GetOrCreateAttributeSet(
		const TSubclassOf<UAttributeSet>& AttributeSetClass)
{
	return GetOrCreateAttributeSubobject(AttributeSetClass);
}

bool UJoA_GAS_AbilitySystemComponent::
	TryActivateAbilityByInputID(
		TSubclassOf<UGameplayAbility> AbilityClass,
		int32 InputID)
{
	if (!AbilityClass)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec
		: GetActivatableAbilities())
	{
		if (Spec.Ability
			&& Spec.Ability->GetClass() == AbilityClass
			&& Spec.InputID == InputID)
		{
			return TryActivateAbility(Spec.Handle);
		}
	}

	return false;
}

void UJoA_GAS_AbilitySystemComponent::
	InitializeAbilitySystemData(
		const FJoA_GAS_InitializationData&
			InitializationData,
		AActor* InOwningActor,
		AActor* InAvatarActor)
{
	if (bAbilitySystemDataInitialized)
	{
		return;
	}

	bAbilitySystemDataInitialized = true;
	InitAbilityActorInfo(InOwningActor, InAvatarActor);

	if (!InitializationData.GameplayTags.IsEmpty())
	{
		AddLooseGameplayTags(
			InitializationData.GameplayTags);
	}

	/* Sets, base values, abilities, and effects
	 * replicate from the server. The client stops
	 * after actor info and loose tags. */
	if (!InOwningActor || !InOwningActor->HasAuthority())
	{
		return;
	}

	for (const TSubclassOf<UAttributeSet>& SetClass
		: InitializationData.AttributeSets)
	{
		if (SetClass)
		{
			GetOrCreateAttributeSet(SetClass);
		}
	}

	for (const FJoA_GAS_AttributeBaseValue& BaseValue
		: InitializationData.AttributeBaseValues)
	{
		if (!BaseValue.Attribute.IsValid())
		{
			continue;
		}

		if (!HasAttributeSetForAttribute(BaseValue.Attribute))
		{
			continue;
		}

		SetNumericAttributeBase(
			BaseValue.Attribute,
			BaseValue.Value);
	}

	for (const FAbilityGrant& Grant
		: InitializationData.AbilityGrants)
	{
		if (!Grant.Ability)
		{
			continue;
		}

		const FGameplayAbilitySpec AbilitySpec(
			Grant.Ability,
			1,
			Grant.InputID,
			this);
		GiveAbility(AbilitySpec);
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass
		: InitializationData.GameplayEffects)
	{
		if (!EffectClass)
		{
			continue;
		}

		FGameplayEffectContextHandle Context =
			MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec =
			MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (!Spec.IsValid() || !Spec.Data.IsValid())
		{
			continue;
		}

		ApplyGameplayEffectSpecToTarget(
			*Spec.Data.Get(),
			this);
	}
}
