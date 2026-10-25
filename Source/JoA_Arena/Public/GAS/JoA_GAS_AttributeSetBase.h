// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// --- GAS ---
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "JoA_GAS_AttributeSetBase.generated.h"

#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER( \
		ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

/**
 * Shared clamp and push-model dirty mark for figure
 * attribute sets. Meta attributes stay local.
 * Replicated attributes mark themselves dirty,
 * because push model does not send a clean property.
 */
UCLASS(Abstract)
class JOA_ARENA_API UJoA_GAS_AttributeSetBase
	: public UAttributeSet
{
	GENERATED_BODY()

public:
	/* Health, Stamina, and Mana. 0 is empty. The full
	 * value is the Max attribute on the same set. */
	static constexpr float MinResource = 0.f;

	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute,
		float& NewValue) override;

	virtual void PreAttributeBaseChange(
		const FGameplayAttribute& Attribute,
		float& NewValue) const override;

	virtual void PostAttributeChange(
		const FGameplayAttribute& Attribute,
		float OldValue,
		float NewValue) override;

	virtual void PostAttributeBaseChange(
		const FGameplayAttribute& Attribute,
		float OldValue,
		float NewValue) const override;

protected:
	virtual void ClampAttribute(
		const FGameplayAttribute& Attribute,
		float& NewValue) const;

	virtual bool IsReplicatedAttribute(
		const FGameplayAttribute& Attribute) const;

	void MarkReplicatedAttribute(
		const FGameplayAttribute& Attribute);
};
